#pragma once
// =================================================================
// src/asr/WhisperServerLauncher.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Der Whisper-Dienst aus Longpath heraus, statt aus dem Terminal.
//
// Betreiber, 2026-09-17: die Mitschrift war ausgeblendet und der Dienst
// lief nicht — "ein Startknopf fuer den Dienst aus Longpath heraus,
// damit du das Terminal nicht brauchst" / "ja bitte".
//
// Was hier steht: ein QProcess um `whisper-server`, mit Programmpfad,
// Modelldatei, Port (aus der Endpunkt-Adresse) und Sprache aus den
// Einstellungen. Ein Zustand (Aus / Startet / Laeuft / Fehler), ein
// Grund im Fehlerfall, die letzten Zeilen des Dienstes fuers Log.
//
// Was hier NICHT steht: Netz. Ob der Dienst antwortet, prueft weiter
// AsrPage::probeEndpoint bzw. das Backend selbst; dieser Starter weiss
// nur, ob der Prozess lebt. "Laeuft" heisst darum "gestartet und nicht
// wieder beendet", nicht "hat schon ein Modell geladen" — das dauert
// beim small-Modell einige Sekunden, und die erste Anfrage wartet.
//
// Der Prozess ist ein Kind von Longpath und endet mit ihm (stop() im
// Destruktor: TERM, zwei Sekunden, dann KILL). Ein Dienst, der nach dem
// Beenden weiterlaeuft, waere beim naechsten Start ein Port-Konflikt.
//
// Laeuft auf dem Port schon etwas (auf diesem Rechner steht seit dem
// 2026-09-13 ein Login-Dienst `at.longpath.whisper-server` mit
// KeepAlive), wird NICHT gestartet, sondern der Zustand "Extern"
// gemeldet: whisper-server bindet mit SO_REUSEPORT, ein zweiter Start
// scheitert also nicht, sondern legt still ein weiteres Modell
// (~600 MB) in den Speicher — beobachtet am 2026-09-17 mit drei
// Exemplaren auf 8080 nebeneinander.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-17 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace Longpath {

class WhisperServerLauncher : public QObject {
    Q_OBJECT
public:
    /// External: auf dem Port antwortet schon ein Dienst, den nicht
    /// Longpath gestartet hat — wir benutzen ihn, koennen ihn aber
    /// weder beenden noch sein Log lesen.
    enum class State { Stopped, Starting, Running, Failed, External };
    Q_ENUM(State)

    explicit WhisperServerLauncher(QObject* parent = nullptr);
    ~WhisperServerLauncher() override;

    /// Der eine Dienst des Programms. Haengt am QCoreApplication und
    /// endet mit ihm; Setup-Seite, Applet und Hauptfenster reden mit
    /// demselben Exemplar.
    static WhisperServerLauncher& instance();

    /// Programm, Modell, Adresse, Sprache — wie in den Einstellungen.
    /// Leere Felder nehmen die Vorgabe (siehe defaultBinary/defaultModel).
    struct Config {
        QString binary;     ///< z.B. /opt/homebrew/bin/whisper-server
        QString model;      ///< z.B. ~/whisper/ggml-small.bin
        QString endpoint;   ///< z.B. http://127.0.0.1:8080/inference — der Port kommt hier heraus
        QString language;   ///< z.B. "de"; leer heisst Automatik
    };

    /// Die Einstellungen als Config, mit Vorgaben ausgefuellt.
    static Config configFromSettings();
    /// Wo whisper-server ueblicherweise liegt (Homebrew), sonst leer.
    static QString defaultBinary();
    /// Das erste ggml-*.bin in ~/whisper, sonst leer.
    static QString defaultModel();
    /// Die Argumente, die start() dem Programm gibt — fuer Pruefstaende.
    static QStringList argumentsFor(const Config& cfg);
    /// Ob die Adresse auf diesen Rechner zeigt (nur dann lohnt Starten).
    static bool endpointIsLocal(const QString& endpoint);
    /// Ob auf Host:Port der Adresse schon jemand Verbindungen annimmt
    /// (kurzer TCP-Connect, oertlich in Millisekunden entschieden).
    static bool portInUse(const QString& endpoint, int timeoutMs = 300);

    void start(const Config& cfg);
    void stop();

    State   state() const { return m_state; }
    QString reason() const { return m_reason; }   ///< bei Failed: warum
    /// Die letzten Zeilen, die der Dienst geschrieben hat (max. 40).
    QStringList recentOutput() const { return m_recent; }

signals:
    void stateChanged(Longpath::WhisperServerLauncher::State state,
                      const QString& reason);

private:
    void setState(State s, const QString& reason = QString());
    void onOutput();

    QProcess*   m_proc{nullptr};
    State       m_state{State::Stopped};
    QString     m_reason;
    QStringList m_recent;
};

} // namespace Longpath
