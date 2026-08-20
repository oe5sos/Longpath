// tests/tst_theme_filter.cpp  (NereusSDR)
//
// NereusSDR-original. No Thetis port.
//
// ── Der Download-Fall, wörtlich ──────────────────────────────────────
//
// OE5SOS, 2026-08-15:
//
//   „Es werden immer Änderungen von Nereus kommen, die ich dann
//    downloade und die sich dann automatisch meiner Farben und meinem
//    Design anpassen sollen."
//
// Das ist der Test dafür. Ein Widget, das von diesem Theme nie gehört
// hat, mit Nereus-Farben im Stylesheet, wird in die Anwendung gehängt —
// und muss hinterher die Farben des Betreibers tragen, ohne dass
// irgendjemand es angefasst hat.
//
// Wenn dieser Test durchläuft, ist „Technik Nereus, Design ich" keine
// Absicht mehr, sondern eine Eigenschaft des Programms.

#include <QtTest>

#include "gui/styles/Theme.h"
#include "gui/styles/ThemeQss.h"
#include "gui/StyleConstants.h"

#include <QApplication>
#include <QLabel>
#include <QTemporaryDir>
#include <QWidget>

using namespace Longpath;

class TestThemeFilter : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    Style::ThemeFilter* m_filter{nullptr};

    void loadTheme(const QString& colours)
    {
        const QString path = m_dir.filePath(QStringLiteral("t.json"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write(QStringLiteral("{ \"name\": \"Test\", \"colors\": { %1 } }")
                    .arg(colours).toUtf8());
        f.close();
        QString err;
        QVERIFY2(Style::Theme::instance().loadFile(path, &err), qPrintable(err));
    }

private slots:
    void initTestCase()
    {
        m_filter = new Style::ThemeFilter(qApp);
        qApp->installEventFilter(m_filter);
    }

    void cleanupTestCase()
    {
        if (m_filter) { qApp->removeEventFilter(m_filter); }
        Style::Theme::instance().clear();
    }

    void cleanup() { Style::Theme::instance().clear(); }

    // ── Der eine, um den es geht ─────────────────────────────────────
    void aWidgetThatNeverHeardOfTheThemeGetsItAnyway()
    {
        loadTheme(QStringLiteral("\"border\": \"#123456\", "
                                 "\"text\": \"#abcdef\""));

        // So sieht ein Widget aus, das mit dem nächsten Download kommt:
        // Nereus-Farben, ausgeschrieben, kein Wissen über ein Theme.
        QWidget w;
        w.setStyleSheet(QStringLiteral(
            "QWidget { color: #c8d8e8; border: 1px solid #205070; }"));

        w.resize(120, 60);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        const QString after = w.styleSheet();
        QVERIFY2(after.contains(QStringLiteral("#123456")),
                 qPrintable(QStringLiteral(
                     "der Rahmen folgt dem Theme nicht:\n%1").arg(after)));
        QVERIFY2(after.contains(QStringLiteral("#abcdef")),
                 qPrintable(QStringLiteral(
                     "die Textfarbe folgt dem Theme nicht:\n%1").arg(after)));
        QVERIFY2(!after.contains(QStringLiteral("#205070")),
                 "die Nereus-Farbe steht noch drin");
    }

    void aChildWidgetIsCaughtToo()
    {
        // Panels bauen ihre Knöpfe selbst. Der Filter hängt an der
        // Anwendung, nicht am Fenster, also muss auch ein Kind, das
        // sein Stylesheet im eigenen Konstruktor setzt, erwischt werden.
        loadTheme(QStringLiteral("\"button\": \"#654321\""));

        QWidget parent;
        auto* child = new QLabel(QStringLiteral("x"), &parent);
        child->setStyleSheet(QStringLiteral("QLabel { background: #1a2a3a; }"));

        parent.resize(120, 60);
        parent.show();
        QVERIFY(QTest::qWaitForWindowExposed(&parent));

        QVERIFY2(child->styleSheet().contains(QStringLiteral("#654321")),
                 qPrintable(child->styleSheet()));
    }

    void aLaterChangeIsCaughtAsWell()
    {
        // Zustandswechsel setzen Stylesheets nach dem Polish neu. Ohne
        // StyleChange im Filter fiele genau das durch — ein Knopf, der
        // beim Einschalten in die alte Palette zurückspringt.
        loadTheme(QStringLiteral("\"accent\": \"#0f0f0f\""));

        QWidget w;
        w.resize(120, 60);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        w.setStyleSheet(QStringLiteral("QWidget { color: #00b4d8; }"));
        QVERIFY2(w.styleSheet().contains(QStringLiteral("#0f0f0f")),
                 qPrintable(w.styleSheet()));
    }

    // ── Ohne Theme-Datei gilt trotzdem die Nereus-Palette ────────────
    //
    // Dieser Test behauptete zuerst „kein Theme, keine Änderung". Das
    // stimmte genau so lange, wie die Tabelle die Identität war, und
    // wurde am 2026-08-15 falsch, als die Palette entblaut wurde.
    //
    // Die eingebaute Tabelle ist nämlich selbst eine Abbildung: ein
    // Widget schreibt den alten Wert #c8d8e8 aus, und kTextPrimary steht
    // heute auf #c4c4c9. Würde der Filter das in Ruhe lassen, bliebe
    // jedes Widget mit ausgeschriebenem Literal in der alten Palette
    // stehen — und das sind 2000 Stellen.
    //
    // Was ohne Theme gilt, ist also nicht „nichts", sondern „Nereus".
    void withoutAThemeTheNereusPaletteStillApplies()
    {
        Style::Theme::instance().clear();

        QWidget w;
        w.setStyleSheet(QStringLiteral(
            "QWidget { color: #c8d8e8; border: 1px solid #205070; }"));
        w.resize(120, 60);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        const QString after = w.styleSheet();
        QVERIFY2(after.contains(QString::fromLatin1(Style::kTextPrimary)),
                 qPrintable(QStringLiteral("die alte Textfarbe folgt der "
                                           "Palette nicht:\n%1").arg(after)));
        QVERIFY2(after.contains(QString::fromLatin1(Style::kBorder)),
                 qPrintable(QStringLiteral("der alte Rahmen folgt der "
                                           "Palette nicht:\n%1").arg(after)));
    }

    void aColourTheTableDoesNotKnowIsNeverTouched()
    {
        // DAS ist die Zusicherung, die der Test vorher geben wollte:
        // der Filter rät nicht. Was in keiner Zeile steht, bleibt
        // stehen — mit und ohne Theme.
        loadTheme(QStringLiteral("\"border\": \"#123456\""));
        const QString qss =
            QStringLiteral("QWidget { color: #778899; background: #abcdef; }");

        QWidget w;
        w.setStyleSheet(qss);
        w.resize(120, 60);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        QCOMPARE(w.styleSheet(), qss);
    }

    void aWidgetWithoutAStyleSheetIsLeftAlone()
    {
        loadTheme(QStringLiteral("\"border\": \"#123456\""));
        QWidget w;
        w.resize(120, 60);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QVERIFY(w.styleSheet().isEmpty());
    }

    void itTerminates()
    {
        // setStyleSheet löst StyleChange aus, was den Filter wieder
        // aufruft. Dass das aufhört, hängt daran, dass themed()
        // idempotent ist — der zweite Durchlauf ändert nichts mehr und
        // der Filter bricht ab. Ohne das hinge der Test hier fest.
        loadTheme(QStringLiteral("\"border\": \"#123456\", "
                                 "\"text\": \"#abcdef\", "
                                 "\"accent\": \"#0f0f0f\""));
        QWidget w;
        w.setStyleSheet(QStringLiteral(
            "QWidget { color: #c8d8e8; border: 1px solid #205070; }"
            "QWidget:hover { color: #00b4d8; }"));
        w.resize(120, 60);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        const QString a = w.styleSheet();
        w.setStyleSheet(a);              // noch einmal, von Hand
        QCOMPARE(w.styleSheet(), a);
    }

    void theFilterCanSayThatItDidSomething()
    {
        // Ein eingehängter Filter, der nie zuschlägt, sieht aus wie
        // einer, der wirkt. Der Zähler ist der Unterschied.
        loadTheme(QStringLiteral("\"border\": \"#123456\""));
        const quint64 before = Style::ThemeFilter::appliedCount();

        QWidget w;
        w.setStyleSheet(QStringLiteral("QWidget { border: 1px solid #205070; }"));
        w.resize(120, 60);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        QVERIFY(Style::ThemeFilter::appliedCount() > before);
    }
};

QTEST_MAIN(TestThemeFilter)
#include "tst_theme_filter.moc"
