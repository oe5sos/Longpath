#pragma once
// =================================================================
// src/models/RecordingModel.h  (Longpath)
// =================================================================
//
// Die Bindeschicht fuer Phase 3M (Aufnahme). Der Plan hat sie
// vorgesehen -- „a small RecordingModel (Q_PROPERTY surface:
// recording/playing state, current file, elapsed time) for whatever UI
// eventually binds to it. Kept deliberately thin -- no layout decisions
// live here." (docs/architecture/phase3m-recording-plan.md
// §Deliverables) -- und sie wurde nie geschrieben.
//
// WARUM SIE FEHLTE UND WARUM DAS ZAEHLTE. Am 2026-10-21 gemessen:
// WavRecorder, IqRecorder, RecordingScheduler und
// PlaybackRadioConnection sind gebaut und von sechs Prueflaeufen
// gedeckt -- aber keine davon wurde irgendwo in src/ konstruiert. Jede
// Fundstelle ausserhalb der eigenen Dateien war ein Kommentar. Die
// Flaeche fehlte nach §0.4 mit Absicht („Technik Nereus, Design ich"),
// die Bindeschicht aber nicht.
//
// WAS HIER NICHT HINEINGEHOERT: Knopfplaetze, Farben, Dialoge. Diese
// Klasse weiss nichts davon, wie sie bedient wird. Sie weiss, wie man
// die Angaben fuer eine Aufnahme aus dem Radio holt, wohin die Datei
// kommt, und welcher Zustand nach draussen zu melden ist.
//
// WAS SIE (NOCH) NICHT KANN: die Wiedergabe. PlaybackRadioConnection
// ist eine RadioConnection-Unterklasse, und sie zu erreichen heisst,
// `RadioConnection::create()` einen Zweig zu geben oder einen zweiten
// Bauweg ohne Geraetesuche (Plan §6). Das ist eine Entscheidung am
// Verbindungsweg -- wie soll eine Datei in der Geraeteliste
// auftauchen? -- und keine, die eine Bindeschicht treffen darf. Die
// beiden Merkmale stehen hier darum als Eigenschaft mit festem Wert,
// damit eine Flaeche sie schon abfragen kann, ohne dass die Antwort
// erfunden ist.
//
// Modification history (Longpath):
//   2026-10-21 — Original fuer Longpath von Martin Fischer,
//                geschrieben mit Claude Opus 5 (Anthropic).
// =================================================================

#include <QObject>
#include <QString>

#include "core/audio/IqRecorderController.h"
#include "core/audio/RecordingScheduler.h"
#include "core/audio/WavRecorderController.h"

namespace Longpath {

class RadioModel;

class RecordingModel : public QObject
{
    Q_OBJECT

    // ── Ton ──────────────────────────────────────────────────────────
    Q_PROPERTY(bool    audioRecording   READ audioRecording   NOTIFY audioRecordingChanged)
    Q_PROPERTY(QString audioFile        READ audioFile        NOTIFY audioFileChanged)
    Q_PROPERTY(double  audioSeconds     READ audioSeconds     NOTIFY audioSecondsChanged)
    Q_PROPERTY(bool    audioSamplesLost READ audioSamplesLost NOTIFY audioSamplesLostChanged)

    // ── I/Q ──────────────────────────────────────────────────────────
    Q_PROPERTY(bool    iqRecording      READ iqRecording      NOTIFY iqRecordingChanged)
    Q_PROPERTY(QString iqFile           READ iqFile           NOTIFY iqFileChanged)
    Q_PROPERTY(double  iqSeconds        READ iqSeconds        NOTIFY iqSecondsChanged)
    Q_PROPERTY(bool    iqSamplesLost    READ iqSamplesLost    NOTIFY iqSamplesLostChanged)

    // ── Zeitplan ─────────────────────────────────────────────────────
    Q_PROPERTY(bool scheduleRunning     READ scheduleRunning  NOTIFY scheduleRunningChanged)
    Q_PROPERTY(int  scheduleCount       READ scheduleCount    NOTIFY scheduleCountChanged)

    // ── Wiedergabe: siehe Kopf, noch kein Weg dorthin ────────────────
    Q_PROPERTY(bool    playing          READ playing          NOTIFY playingChanged)
    Q_PROPERTY(QString playbackFile     READ playbackFile     NOTIFY playbackFileChanged)
    Q_PROPERTY(bool    playbackAvailable READ playbackAvailable CONSTANT)

    // ── Allgemein ────────────────────────────────────────────────────
    /// true, sobald attach() ein Radio gesehen hat. Ohne das kann
    /// nichts aufgenommen werden, und eine Flaeche soll ihre Knoepfe
    /// danach ausgrauen koennen, statt einen Fehler zu zeigen.
    Q_PROPERTY(bool    ready            READ ready            NOTIFY readyChanged)
    Q_PROPERTY(QString lastError        READ lastError        NOTIFY lastErrorChanged)

public:
    explicit RecordingModel(QObject* parent = nullptr);
    ~RecordingModel() override;

    /// Haengt die Aufnahme an ein Radio. Der Tonabgriff braucht dessen
    /// AudioEngine, der I/Q-Abgriff das Radio selbst. Ein zweiter Ruf
    /// mit einem anderen Radio loest den alten Abgriff; nullptr loest
    /// ihn ganz.
    void attach(RadioModel* radio);

    // ── Handlungen, die eine Flaeche ruft ────────────────────────────
    //
    // Ein leerer Pfad heisst „waehle selbst einen" -- siehe
    // suggestedPath(). Rueckgabe false heisst: nichts laeuft, und
    // lastError() sagt warum.
    Q_INVOKABLE bool startAudio(const QString& path = QString());
    Q_INVOKABLE void stopAudio();
    Q_INVOKABLE bool startIq(const QString& path = QString());
    Q_INVOKABLE void stopIq();

    /// Der Ordner, in dem Aufnahmen landen. Derselbe Ort, den die
    /// QSO-Aufnahme benutzt (AppSettings::dataDir() + "/recordings"),
    /// damit nicht zwei Aufnahmearten an zwei Stellen ablegen. Wird
    /// angelegt, wenn er fehlt.
    Q_INVOKABLE QString recordingFolder() const;

    /// Ein Vorschlag, kein Zwang: <Ordner>/<UTC>_<Band>_<Mode>.<suffix>,
    /// z. B. „20261021-134500Z_20m_LSB.wav". UTC, weil eine Aufnahme
    /// ueber Mitternacht sonst zweimal denselben Namen traegt.
    Q_INVOKABLE QString suggestedPath(const QString& suffix) const;

    // ── Eigenschaften ────────────────────────────────────────────────
    bool    audioRecording() const { return m_wav.isRecording(); }
    QString audioFile() const { return m_audioFile; }
    double  audioSeconds() const { return m_audioSeconds; }
    bool    audioSamplesLost() const { return m_audioLost; }

    bool    iqRecording() const { return m_iq.isRecording(); }
    QString iqFile() const { return m_iqFile; }
    double  iqSeconds() const { return m_iqSeconds; }
    bool    iqSamplesLost() const { return m_iqLost; }

    bool scheduleRunning() const { return m_schedule.isRunning(); }
    int  scheduleCount() const
    {
        return static_cast<int>(m_schedule.entries().size());
    }

    bool    playing() const { return false; }
    QString playbackFile() const { return QString(); }
    bool    playbackAvailable() const { return false; }

    bool    ready() const { return m_radio != nullptr; }
    QString lastError() const { return m_lastError; }

    // ── Zugang fuer Flaeche und Prueflauf ────────────────────────────
    WavRecorderController& audio() { return m_wav; }
    IqRecorderController&  iq()    { return m_iq; }
    RecordingScheduler&    schedule() { return m_schedule; }

signals:
    void audioRecordingChanged(bool on);
    void audioFileChanged(const QString& path);
    void audioSecondsChanged(double seconds);
    void audioSamplesLostChanged(bool lost);

    void iqRecordingChanged(bool on);
    void iqFileChanged(const QString& path);
    void iqSecondsChanged(double seconds);
    void iqSamplesLostChanged(bool lost);

    void scheduleRunningChanged(bool running);
    void scheduleCountChanged(int count);

    void playingChanged(bool on);
    void playbackFileChanged(const QString& path);

    void readyChanged(bool ready);
    void lastErrorChanged(const QString& message);

private:
    void wireControllers();
    void setLastError(const QString& message);
    /// Frequenz, Betriebsart und Band der aktiven Scheibe. Bewusst
    /// OHNE die Abtastrate: die haengt an der Verbindung, die Scheibe
    /// nicht. Zusammengelegt hiess es, dass ein Dateiname ohne
    /// Verbindung Band und Betriebsart verliert -- der Vorschlag soll
    /// aber auch dann etwas taugen.
    bool sliceInfo(QString* freq, QString* mode, QString* band) const;

    RadioModel* m_radio{nullptr};

    WavRecorderController m_wav;
    IqRecorderController  m_iq;
    RecordingScheduler    m_schedule;

    QString m_audioFile;
    double  m_audioSeconds{0.0};
    bool    m_audioLost{false};

    QString m_iqFile;
    double  m_iqSeconds{0.0};
    bool    m_iqLost{false};

    QString m_lastError;
};

}  // namespace Longpath
