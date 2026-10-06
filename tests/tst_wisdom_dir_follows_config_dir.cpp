// no-port-check: Longpath-original. LONGPATH_CONFIG_DIR ist eine
// Longpath-Erfindung (Roadmap-Schritt 13); Thetis und deskhpsdr kennen
// weder die Variable noch eine Pruef-Instanz, es gibt nichts zu portieren.
// =================================================================
// tests/tst_wisdom_dir_follows_config_dir.cpp  (Longpath)
// =================================================================
//
// Es gibt LONGPATH_CONFIG_DIR, damit eine Pruef-Instanz den
// Konfigurationsordner des Betreibers nicht anfasst. Der Anlass steht in
// AppSettings.cpp: eine Sandbox, mit HOME= gestartet, las auf macOS
// trotzdem die echten Einstellungen, meldete sich mit seinem Rufzeichen
// am DX-Cluster an (und warf damit seine eigene Sitzung hinaus) und
// schrieb beim Beenden in seine Datei zurueck.
//
// Die FFTW-Wisdom war von dieser Trennung ausgenommen, weil sie nicht
// ueber AppSettings laeuft, sondern direkt ueber QStandardPaths. Eine
// Pruef-Instanz mit "frischem Profil" las und schrieb deshalb weiter
// ~/Library/Preferences/Longpath/Longpath/wdspWisdom00 — die Datei des
// Betreibers. Zwei Folgen, beide schlecht:
//
//   - Ein frisches Profil war nie wirklich frisch. Wer den Erstlauf
//     nachstellen wollte (die ~15 Minuten FFTW, um die es im Befund vom
//     2026-10-05 geht), musste die echte Datei des Betreibers beiseite
//     raeumen.
//   - Eine Pruef-Instanz, die die Wisdom neu baut, UEBERSCHREIBT dabei
//     die des Betreibers.
//
// Der Prueffall nagelt beide Richtungen fest: mit gesetzter Variable
// liegt die Wisdom dort, ohne sie unveraendert an der alten Stelle.
// Letzteres ist die wichtigere Haelfte — jede bestehende Installation
// muss ihre Datei weiter finden, sonst rechnet sie beim naechsten Start
// eine Viertelstunde lang neu.
// =================================================================

#include <QtTest/QtTest>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "core/WdspEngine.h"

using namespace Longpath;

class TstWisdomDirFollowsConfigDir : public QObject {
    Q_OBJECT

private slots:
    void ohneVariableBleibtEsDerAlteOrt()
    {
        qunsetenv("LONGPATH_CONFIG_DIR");
        QCOMPARE(WdspEngine::wisdomDir(),
                 QStandardPaths::writableLocation(
                     QStandardPaths::AppConfigLocation));
    }

    void mitVariableLiegtSieDort()
    {
        QTemporaryDir eigenerOrdner;
        QVERIFY(eigenerOrdner.isValid());

        qputenv("LONGPATH_CONFIG_DIR", eigenerOrdner.path().toUtf8());
        QCOMPARE(WdspEngine::wisdomDir(), eigenerOrdner.path());

        // Und dort ist die Wisdom-Datei dann auch wirklich nicht da —
        // der Punkt der ganzen Uebung: ein frisches Profil meldet
        // needsGeneration, statt die Datei des Betreibers zu finden.
        QVERIFY(WdspEngine::needsWisdomGeneration(
            eigenerOrdner.path() + QStringLiteral("/")));

        qunsetenv("LONGPATH_CONFIG_DIR");
    }

    // Leerzeichen und Leerstring zaehlen als "nicht gesetzt" — sonst
    // landet die Wisdom im Wurzelverzeichnis oder in einem Ordner namens
    // " ". Gleiche Lesart wie AppSettings::resolveConfigDir.
    void leerUndLeerzeichenZaehlenNicht()
    {
        const QString alterOrt = QStandardPaths::writableLocation(
            QStandardPaths::AppConfigLocation);

        qputenv("LONGPATH_CONFIG_DIR", QByteArray(""));
        QCOMPARE(WdspEngine::wisdomDir(), alterOrt);

        qputenv("LONGPATH_CONFIG_DIR", QByteArray("   "));
        QCOMPARE(WdspEngine::wisdomDir(), alterOrt);

        qunsetenv("LONGPATH_CONFIG_DIR");
    }
};

QTEST_MAIN(TstWisdomDirFollowsConfigDir)
#include "tst_wisdom_dir_follows_config_dir.moc"
