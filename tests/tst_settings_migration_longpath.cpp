// =================================================================
// tests/tst_settings_migration_longpath.cpp  (NereusSDR → Longpath)
// =================================================================
//
// Beim Umbenennen darf die Anordnung nicht verlorengehen.
//
// Bis zum 2026-08-20 hiess das Programm NereusSDR, und die
// Einstellungen lagen unter „<config>/NereusSDR/NereusSDR.settings".
// Nach dem Umbenennen sieht es unter „<config>/Longpath/
// Longpath.settings" nach. Ohne Uebernahme faende es dort nichts, und
// der Betreiber saehe seine gesamte Anordnung als verloren an —
// Fensterlagen, Profile, Filter, Rufzeichen, alles.
//
// DAS IST DIE GEFAEHRLICHSTE STELLE DER GANZEN UMBENENNUNG, und
// deshalb steht dieser Test vor allem anderen. Ein Umbenennen, das
// Daten verliert, ist kein Umbenennen.
//
// Drei Bedingungen, die alle drei stimmen muessen:
//   1. es wird KOPIERT, nicht verschoben — ein paralleler
//      NereusSDR-Bau soll weiterlaufen und es soll einen Rueckweg
//      geben;
//   2. es passiert NUR EINMAL — sonst uebermalte jeder Start die
//      neuen Einstellungen mit den alten;
//   3. die Hauptdatei bekommt den NEUEN Namen — sonst liegt sie da
//      und wird nie gelesen, der stillste denkbare Fehler.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-20 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include "core/AppSettings.h"

using namespace NereusSDR;

namespace {

bool writeFile(const QString& path, const QString& text)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) { return false; }
    f.write(text.toUtf8());
    f.close();
    return true;
}

} // namespace

class TestSettingsMigrationLongpath : public QObject
{
    Q_OBJECT

private:
    // NICHT ueber XDG_CONFIG_HOME umlenken — das wirkt auf macOS
    // nicht, und der erste Anlauf dieses Tests schrieb deshalb seine
    // Attrappe in ein Temp-Verzeichnis, waehrend der Aufloeser im
    // echten Einstellungsordner nachsah. Der Test war gruen an vier
    // Stellen und pruefte nichts.
    //
    // Die Tests laufen ohnehin in einer Sandkiste: TestSandboxInit.cpp
    // ruft QStandardPaths::setTestModeEnabled(true) als globalen
    // Static, und GenericConfigLocation zeigt damit in einen
    // qttest-Ordner. Genau dorthin gehoert die Attrappe.
    QString configRoot() const
    {
        return QStandardPaths::writableLocation(
            QStandardPaths::GenericConfigLocation);
    }

private slots:

    void initTestCase()
    {
        // Sauber anfangen: Reste aus einem frueheren Lauf wuerden die
        // Uebernahme davon abhalten, ueberhaupt zu laufen.
        QDir(configRoot() + QStringLiteral("/Longpath")).removeRecursively();
        QDir(configRoot() + QStringLiteral("/NereusSDR")).removeRecursively();
    }

    void theOldSettingsAreCarriedOver()
    {
        const QString oldRoot = configRoot() + QStringLiteral("/NereusSDR");
        QVERIFY(writeFile(oldRoot + QStringLiteral("/NereusSDR.settings"),
                  QStringLiteral("<NereusSDR><Callsign>OE5SOS</Callsign>"
                                 "</NereusSDR>")));

        const QString path = AppSettings::resolveSettingsPath(QString{});

        QVERIFY2(path.contains(QStringLiteral("Longpath")),
                 qPrintable(QStringLiteral("neuer Pfad erwartet, war: %1")
                                .arg(path)));
        QVERIFY2(QFile::exists(path),
                 "die alte Datei muss unter dem NEUEN Namen liegen — "
                 "sonst wird sie nie gelesen");

        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY2(QString::fromUtf8(f.readAll()).contains(
                     QStringLiteral("OE5SOS")),
                 "und sie muss den Inhalt tragen, nicht nur den Namen");
    }

    // Kopiert, nicht verschoben: ein paralleler NereusSDR-Bau soll
    // weiterlaufen, und es muss einen Rueckweg geben.
    void theOldFolderStaysWhereItIs()
    {
        QVERIFY2(QFile::exists(configRoot()
                               + QStringLiteral("/NereusSDR/NereusSDR.settings")),
                 "die alte Datei darf nicht verschwinden");
    }

    // Nur einmal. Sonst uebermalte jeder Start die neuen Einstellungen
    // mit den alten — der Fehler, den man erst nach Tagen bemerkt.
    void aSecondRunDoesNotOverwriteWhatIsNew()
    {
        const QString newPath = configRoot()
            + QStringLiteral("/Longpath/Longpath.settings");
        QVERIFY(writeFile(newPath,
                  QStringLiteral("<Longpath><Callsign>NEU</Callsign>"
                                 "</Longpath>")));

        // Noch einmal aufloesen — die Uebernahme darf nicht erneut
        // laufen.
        (void)AppSettings::resolveSettingsPath(QString{});

        QFile f(newPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(f.readAll());
        QVERIFY2(text.contains(QStringLiteral("NEU")),
                 "die neue Datei wurde von der alten uebermalt");
    }
};

QTEST_MAIN(TestSettingsMigrationLongpath)
#include "tst_settings_migration_longpath.moc"
