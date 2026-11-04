// tests/tst_kein_blauer_grund.cpp  (Longpath)
//
// Betreiber am 2026-11-04, zum wievielten Mal: "zum hundertsten mal der
// hintergrund blau". Und am 2026-11-02: "ich will diesen blauen
// hintergrund ueberall weg, er kommt immer wieder und es ist aergerlich,
// dass ich das immer wieder sagen muss."
//
// Die Quelle, gefunden am 2026-11-04: `#0f0f1a` -- der Grund der alten,
// blaustichigen Palette -- stand roh in zwoelf Stylesheets (Hauptfenster,
// Einstellungsdialog, Verbindungsmaske, Dialogseiten, Fortschritt,
// Park-Info, kPageStyle). `Style::themed()` schreibt diese Literale nur
// um, wenn ein Thema AKTIV ist; bei "Theme: aus" (sein Stand) bleiben
// sie, wie sie sind. Darum kam das Blau mit jedem Thema-Aus zurueck.
//
// Dieser Pruefstand liest den Quellbaum und verbietet das Literal in
// jedem String und jeder QColor -- nicht nur dort, wo es gerade jemand
// sah. Ein Kommentar darf es nennen (die Geschichte bleibt lesbar), eine
// Fläche nicht mehr tragen.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-04 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>

class TestKeinBlauerGrund : public QObject
{
    Q_OBJECT

private slots:
    void dasAlteBlauStehtInKeinemStringUndKeinerFarbe()
    {
        const QString wurzel = QStringLiteral(LONGPATH_SRC_DIR "/src");
        QVERIFY2(QDir(wurzel).exists(), qPrintable(wurzel));

        // Das Literal in einem String ("...#0f0f1a...") oder als QColor.
        static const QRegularExpression imString(
            QStringLiteral("\"[^\"\\n]*#0f0f1a[^\"\\n]*\""),
            QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression alsFarbe(
            QStringLiteral("QColor\\(\\s*0x0f\\s*,\\s*0x0f\\s*,\\s*0x1a\\s*\\)"),
            QRegularExpression::CaseInsensitiveOption);

        QStringList funde;
        QDirIterator it(wurzel, {QStringLiteral("*.cpp"), QStringLiteral("*.h")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString pfad = it.next();
            QFile f(pfad);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
            int zeile = 0;
            while (!f.atEnd()) {
                const QString l = QString::fromUtf8(f.readLine());
                ++zeile;
                const QString ohneKommentar = l.left(l.indexOf(QStringLiteral("//")) < 0
                                                         ? l.size() : l.indexOf(QStringLiteral("//")));
                if (imString.match(ohneKommentar).hasMatch()
                    || alsFarbe.match(ohneKommentar).hasMatch()) {
                    funde << QStringLiteral("%1:%2: %3")
                                 .arg(QDir(wurzel).relativeFilePath(pfad))
                                 .arg(zeile)
                                 .arg(l.trimmed());
                }
            }
        }
        QVERIFY2(funde.isEmpty(),
                 qPrintable(QStringLiteral("%1 Stelle(n) tragen noch das alte Blau #0f0f1a:\n  %2")
                                .arg(funde.size())
                                .arg(funde.join(QStringLiteral("\n  ")))));
    }
};

QTEST_MAIN(TestKeinBlauerGrund)
#include "tst_kein_blauer_grund.moc"
