// tests/tst_theme_file.cpp  (NereusSDR)
//
// NereusSDR-original. No Thetis port.
//
// ── Die Datei, die den Download überlebt ─────────────────────────────
//
// OE5SOS, 2026-08-15: „Technik Nereus, Design ich."
//
// Damit das mehr ist als eine Absicht, muss die Palette aus einer Datei
// kommen, die kein Upstream-Commit anfasst. Diese Tests prüfen die
// Datei-Seite davon; tst_theme_filter prüft, dass sie auch bei Widgets
// ankommt, die noch gar nicht geschrieben sind.
//
// Der wichtigste Test hier ist nicht, dass Laden funktioniert. Es ist
// der mit der kaputten Datei: ein Tippfehler in der Mitte darf nicht
// die halbe Palette umstellen und die andere Hälfte stehen lassen —
// dann sucht man den Fehler im Programm statt in der Datei.

#include <QtTest>

#include "gui/styles/Theme.h"
#include "gui/styles/ThemeQss.h"
#include "gui/StyleConstants.h"

#include <QTemporaryDir>

using namespace NereusSDR;

class TestThemeFile : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString write(const QString& name, const QString& body)
    {
        const QString path = m_dir.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { return {}; }
        f.write(body.toUtf8());
        f.close();
        return path;
    }

private slots:
    void init()    { Style::Theme::instance().clear(); }
    void cleanup() { Style::Theme::instance().clear(); }

    void withoutAFileNereusDrawsItsOwnPalette()
    {
        QVERIFY(!Style::Theme::instance().isActive());
        const QString qss = QStringLiteral("QLabel { color: %1; }")
                                .arg(QString::fromLatin1(Style::kTextPrimary));
        // Der Nereus-Wert steht schon da; es gibt nichts zu bewegen.
        QCOMPARE(Style::themed(qss), qss);
    }

    void aRoleInTheFileMovesTheColour()
    {
        const QString p = write(QStringLiteral("t.json"), QStringLiteral(R"({
            "name": "Test",
            "colors": { "border": "#123456" }
        })"));
        QVERIFY(!p.isEmpty());
        QString err;
        QVERIFY2(Style::Theme::instance().loadFile(p, &err), qPrintable(err));
        QCOMPARE(Style::Theme::instance().name(), QStringLiteral("Test"));

        // Der Quelltext schreibt weiter den alten Nereus-Rahmen hin.
        const QString out =
            Style::themed(QStringLiteral("QFrame { border: 1px solid #205070; }"));
        QVERIFY2(out.contains(QStringLiteral("#123456")), qPrintable(out));
        QVERIFY(!out.contains(QStringLiteral("#205070")));
    }

    void roleAlsoReachesPaintCode()
    {
        // Malcode hat kein Stylesheet. Style::role() ist der Weg dorthin.
        const QString p = write(QStringLiteral("r.json"), QStringLiteral(R"({
            "colors": { "measured": "#abcdef" }
        })"));
        QVERIFY(Style::Theme::instance().loadFile(p));
        QCOMPARE(Style::role("measured", Style::kAmberText),
                 QStringLiteral("#abcdef"));
        // Eine Rolle, zu der die Datei schweigt, bleibt bei Nereus.
        QCOMPARE(Style::role("accent", Style::kAccent),
                 QString::fromLatin1(Style::kAccent));
    }

    void aHexKeyReachesAColourWithNoRole()
    {
        // 162 Farben im Programm haben keine Rolle. Bis die benannt
        // sind, ist der Hex-Schlüssel der einzige Zugang.
        const QString p = write(QStringLiteral("h.json"), QStringLiteral(R"({
            "colors": { "#adff2f": "#6fa384" }
        })"));
        QVERIFY(Style::Theme::instance().loadFile(p));
        const QString out =
            Style::themed(QStringLiteral("QLabel { color: #adff2f; }"));
        QVERIFY2(out.contains(QStringLiteral("#6fa384")), qPrintable(out));
    }

    void aRoleBeatsAHexKeyForTheSameColour()
    {
        // Beide zeigen auf denselben Nereus-Wert. Die Rolle gewinnt,
        // weil sie das ausdrückt, was gemeint war.
        const QString p = write(QStringLiteral("b.json"), QStringLiteral(R"({
            "colors": { "border": "#111111", "#205070": "#222222" }
        })"));
        QVERIFY(Style::Theme::instance().loadFile(p));
        const QString out =
            Style::themed(QStringLiteral("QFrame { border: 1px solid #205070; }"));
        QVERIFY2(out.contains(QStringLiteral("#111111")), qPrintable(out));
        QVERIFY(!out.contains(QStringLiteral("#222222")));
    }

    // ── Der wichtigste ───────────────────────────────────────────────
    void abrokenFileChangesNothingAtAll()
    {
        const QString good = write(QStringLiteral("g.json"), QStringLiteral(R"({
            "name": "Gut", "colors": { "border": "#123456" }
        })"));
        QVERIFY(Style::Theme::instance().loadFile(good));

        // Halb gültig: die erste Farbe geht, die zweite ist Unsinn.
        const QString bad = write(QStringLiteral("b2.json"), QStringLiteral(R"({
            "name": "Kaputt",
            "colors": { "accent": "#654321", "border": "dunkelblau" }
        })"));
        QString err;
        QVERIFY2(!Style::Theme::instance().loadFile(bad, &err),
                 "eine Datei mit einem unlesbaren Farbwert wurde übernommen");
        QVERIFY2(!err.isEmpty(), "abgelehnt, ohne zu sagen warum");
        QVERIFY2(err.contains(QStringLiteral("dunkelblau")),
                 qPrintable(QStringLiteral("die Meldung nennt den Fehler "
                                           "nicht:\n%1").arg(err)));

        // Und das alte Theme steht unverändert.
        QCOMPARE(Style::Theme::instance().name(), QStringLiteral("Gut"));
        const QString out =
            Style::themed(QStringLiteral("QFrame { border: 1px solid #205070; }"));
        QVERIFY2(out.contains(QStringLiteral("#123456")),
                 "die kaputte Datei hat das gute Theme beschädigt");
        QVERIFY2(!out.contains(QStringLiteral("#654321")),
                 "die erste Hälfte der kaputten Datei wurde übernommen");
    }

    void aNoteInsideTheColoursIsSkippedNotRejected()
    {
        // JSON kennt keine Kommentare. Wer seine Palette gliedert,
        // schreibt eine Zwischenüberschrift hinein — und ohne diese
        // Ausnahme wäre die ganze Datei durchgefallen, mit einer
        // Meldung über einen Farbwert, der gar keiner sein wollte.
        const QString p = write(QStringLiteral("n.json"), QStringLiteral(R"({
            "colors": {
                "_ab_hier_ohne_rolle": "Farben, die noch keinen Namen haben",
                "border": "#123456"
            }
        })"));
        QString err;
        QVERIFY2(Style::Theme::instance().loadFile(p, &err), qPrintable(err));
        const QString out =
            Style::themed(QStringLiteral("QFrame { border: 1px solid #205070; }"));
        QVERIFY(out.contains(QStringLiteral("#123456")));
    }

    void unparsableJsonSaysWhere()
    {
        const QString p = write(QStringLiteral("x.json"),
                                QStringLiteral("{ \"colors\": { "));
        QString err;
        QVERIFY(!Style::Theme::instance().loadFile(p, &err));
        QVERIFY(!err.isEmpty());
    }

    void aMissingFileIsAnErrorNotACrash()
    {
        QString err;
        QVERIFY(!Style::Theme::instance().loadFile(
            m_dir.filePath(QStringLiteral("gibtsnicht.json")), &err));
        QVERIFY(!err.isEmpty());
    }

    void clearGoesBackToNereus()
    {
        const QString p = write(QStringLiteral("c.json"), QStringLiteral(R"({
            "colors": { "border": "#123456" }
        })"));
        QVERIFY(Style::Theme::instance().loadFile(p));
        Style::Theme::instance().clear();
        QVERIFY(!Style::Theme::instance().isActive());
        const QString qss = QStringLiteral("QFrame { border: 1px solid %1; }")
                                .arg(QString::fromLatin1(Style::kBorder));
        QCOMPARE(Style::themed(qss), qss);
    }

    void theSearchPathsAreNamedSoAPersonCanFindThem()
    {
        // „Lege deine Datei hier ab" ist eine bessere Antwort als
        // „irgendwo". Die Setup-Seite soll diese Liste zeigen können.
        const QStringList paths = Style::Theme::searchPaths();
        QVERIFY(!paths.isEmpty());
        for (const QString& p : paths) {
            QVERIFY(p.endsWith(QStringLiteral("/themes")));
        }
    }

    void themedStaysIdempotentWithAThemeLoaded()
    {
        // Der Filter setzt das Stylesheet neu, was ein StyleChange
        // auslöst, was den Filter wieder aufruft. Dass das terminiert,
        // hängt genau hieran.
        const QString p = write(QStringLiteral("i.json"), QStringLiteral(R"({
            "colors": { "border": "#123456", "text": "#abcdef" }
        })"));
        QVERIFY(Style::Theme::instance().loadFile(p));
        const QString in = QStringLiteral(
            "QFrame { border: 1px solid #205070; color: #c8d8e8; }");
        const QString once = Style::themed(in);
        QCOMPARE(Style::themed(once), once);
    }
};

QTEST_GUILESS_MAIN(TestThemeFile)
#include "tst_theme_file.moc"
