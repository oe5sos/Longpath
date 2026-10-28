// tests/TestSandboxInit.cpp
//
// Auto-linked into every test binary by longpath_add_test(). Runs
// QStandardPaths::setTestModeEnabled(true) as a global static
// constructor — before main(), before any test code, and crucially
// before AppSettings::instance() can pin its file path to the real
// ~/.config/NereusSDR/NereusSDR.settings.
//
// Why this exists: AppSettings is a process-global singleton whose
// default constructor calls QStandardPaths::writableLocation(...) to
// resolve the settings file. Once resolved, the path is cached for
// the life of the process. If any test touches AppSettings::instance()
// without first enabling test mode, the singleton pins to the real
// user config directory and subsequent setValue/save calls overwrite
// the developer's actual NereusSDR.settings — wiping saved radios,
// splitter sizes, VFO state, display prefs, etc. This was observed
// during the v0.1.1 alpha when a ctest run silently replaced a
// populated 68-key settings file with a 252-key file full of
// hardware entries keyed to the P1FakeRadio test MAC
// "aa:bb:cc:11:22:33". See fix(radio) commit for the session that
// tracked it down.
//
// QStandardPaths::setTestModeEnabled(true) is safe to call multiple
// times and has no side effects other than redirecting the writable-
// location lookups to a per-user, per-app sandbox path under
// $XDG_CONFIG_HOME/qttest (Linux), ~/Library/Preferences/qttest
// (macOS), or %LOCALAPPDATA%\qttest (Windows). The sandbox is shared
// across all test binaries in a run but is completely isolated from
// the real Longpath install.
//
// This file has no public symbols. It is linked into every test
// target by longpath_add_test() in tests/CMakeLists.txt.

// ── Und das Netz, nicht nur die Dateien (2026-10-28) ────────────────
//
// Der Absatz darueber loest die halbe Aufgabe. Isoliert waren die
// DATEIEN; das Netz war es nie.
//
// `AppSettings::darfVonSelbstVerbinden()` ist der Schalter, mit dem
// alles, was von selbst nach aussen greift, fragt, ob es das in diesem
// Lauf darf -- die Cluster-Clients, der TCI-Dienst, der rigctld-Server,
// der Rotor, das Begleitgeraet. Er ist `!isSandbox()`, und
// `isSandbox()` ist genau eine Zeile:
//
//     return !qEnvironmentVariable("LONGPATH_CONFIG_DIR").trimmed().isEmpty();
//
// Diese Variable setzte bisher NIEMAND im Pruefbetrieb -- weder diese
// Datei noch `tools/run_tests.sh`. In jedem gewoehnlichen ctest-Lauf
// war `darfVonSelbstVerbinden()` also WAHR: jedes Pruefprogramm durfte
// Verbindungen aufnehmen, Ports binden und Dienste starten. Der Schutz
// griff nur bei handgestarteten Sonden, wo jemand die Variable selbst
// setzte.
//
// Das ist derselbe Vorfall wie am 2026-09-17, als sich eine Sonde mit
// dem Rufzeichen des Betreibers anmeldete und damit SEINE
// Cluster-Sitzung aus dem Netz warf -- nur eine Stufe tiefer.
//
// Gefunden hat es eine Voraussetzungspruefung in einem Pruefstand
// (`tst_begleitgeraet_regler`): ohne sie waere der Fall gruen gewesen,
// WEIL die Voraussetzung ausfiel. Er haette nichts geprueft und das
// stillschweigend getan.
//
// ── Warum ein eigener Ordner je Lauf ────────────────────────────────
//
// Es muesste nur IRGENDEIN Wert in der Variablen stehen, damit
// `isSandbox()` wahr wird. Ein echter, eigener Ordner ist aber besser,
// und zwar aus einem zweiten Grund: der qttest-Sandkasten aus
// `setTestModeEnabled` ist laut dem Absatz oben „shared across all
// test binaries in a run". Zwei Pruefprogramme, die gleichzeitig
// laufen, schreiben dort in dieselbe Einstellungsdatei -- und ctest
// faehrt sie mit `-j` gleichzeitig. Ein eigener Ordner je PROZESS
// nimmt diese Kopplung heraus.
//
// QTemporaryDir raeumt beim Beenden auf. Die Reihenfolge stimmt:
// AppSettings ist ein funktionslokales `static`, wird also erst beim
// ersten Zugriff angelegt -- nach diesem globalen Objekt hier -- und
// damit VOR ihm abgebaut. Der Ordner steht noch, wenn AppSettings
// seinen letzten Zugriff tut.
//
// ── Wer die Variable selbst fuehrt, gewinnt ─────────────────────────
//
// Steht sie schon, wird sie nicht angetastet. Zwei Pruefstaende
// pruefen den Schalter selbst (`tst_sonde_bleibt_offline`,
// `tst_sandbox_data_dir`) und schalten ihn dazu zur Laufzeit mit
// `qunsetenv` ab. Das bleibt heil, weil `isSandbox()` die Variable bei
// JEDEM Aufruf frisch liest -- sie wird nicht beim Start eingefroren.

#include <QByteArray>
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace {

struct SandboxInit {
    SandboxInit()
    {
        QStandardPaths::setTestModeEnabled(true);

        // Wer sie selbst fuehrt, gewinnt.
        if (!qEnvironmentVariableIsEmpty("LONGPATH_CONFIG_DIR")) { return; }

        // Scheitert das Anlegen, bleibt es beim alten Verhalten. Ein
        // Pruefbetrieb, der daran haengenbleibt, waere schlimmer als
        // einer, der nur die Dateien isoliert.
        if (!m_ordner.isValid()) { return; }

        qputenv("LONGPATH_CONFIG_DIR", m_ordner.path().toUtf8());
    }

    // Eigenes Muster, damit ein liegengebliebener Ordner zuzuordnen ist.
    QTemporaryDir m_ordner{QDir::tempPath()
                           + QStringLiteral("/longpath-pruefstand-XXXXXX")};
};

// Global static constructor runs before main(). The only dependency
// is QStandardPaths, a stateless Qt utility that does not require
// QCoreApplication to be constructed.
static SandboxInit g_nereusTestSandboxInit;

} // namespace
