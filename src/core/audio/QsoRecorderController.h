#pragma once

// =================================================================
// src/core/audio/QsoRecorderController.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Was die beiden Abgriffe mit der Aufnahme verbindet.
//
// QsoRecorder weiss nicht, woher der Ton kommt — absichtlich, so laesst
// er sich ohne Funkgeraet pruefen. Diese Klasse ist die Stelle, an der
// er an die Anwendung angeschlossen wird, und sie ist die einzige, die
// beide Faeden kennt:
//
//   EMPFANG   AudioEngine::setQsoTap → Zwischenspeicher → hier abgeholt
//   MIKROFON  TxWorkerThread::preStripAudioReady (DirectConnection)
//             → Zwischenspeicher → hier abgeholt
//
// ── Warum zwei Zwischenspeicher und ein Zeitgeber ───────────────────
//
// Beide Abgriffe laufen auf ihrem eigenen Faden, und beide duerfen dort
// keinen Speicher anfordern. Der Zeitgeber holt im Hauptfaden ab und
// reicht weiter an QsoRecorder, der dort anhaengen darf. Begruendung
// im Langen: AudioTapRing.h.
//
// ── Warum das Mikrofon VOR der Sprachbearbeitung abgegriffen wird ───
//
// Derselbe Punkt, den der Sprachspeicher nimmt. Eine Aufnahme des QSOs
// soll die Stimme zeigen, die gesprochen wurde, nicht die Stimme durch
// den heutigen Kompressor. Wer hoeren will, was auf dem Band ankam,
// nimmt den Empfaenger — dafuer ist die linke Spur da.
//
// ── Was hier NICHT passiert ─────────────────────────────────────────
//
// Getastet wird nichts. Beide Abgriffe lesen mit.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QObject>
#include <QTimer>

#include <memory>

#include "core/audio/AudioTapRing.h"
#include "core/audio/QsoRecorder.h"

namespace Longpath {

class AudioEngine;
class TxWorkerThread;

class QsoRecorderController : public QObject
{
    Q_OBJECT

public:
    // Zwei Sekunden Vorrat je Spur. Der Zeitgeber holt zehnmal so oft
    // ab; wer bei zwei Sekunden Rueckstand noch mitschreibt, hat ein
    // groesseres Problem als eine Luecke in der Aufnahme.
    static constexpr int kRingSeconds = 2;
    static constexpr int kDrainMs     = 200;

    explicit QsoRecorderController(QObject* parent = nullptr);
    ~QsoRecorderController() override;

    // Woran es haengt. Beide duerfen nullptr sein — ohne Funkgeraet
    // laesst sich alles ausser dem Ton pruefen.
    void attach(AudioEngine* audio, TxWorkerThread* tx);

    // Welche Scheibe aufgenommen wird. Vor start() setzen.
    void setSliceId(int sliceId) { m_sliceId = sliceId; }
    int  sliceId() const { return m_sliceId; }

    void setSampleRate(int hz);
    int  sampleRate() const { return m_recorder.sampleRate(); }

    bool isRecording() const { return m_recorder.isRecording(); }
    void start(const QsoRecordingInfo& info);
    void stop();

    // Nur fuer Tests und fuer den Notfall: holt jetzt ab, statt auf den
    // Zeitgeber zu warten.
    void drainNow();

    QsoRecorder&       recorder()       { return m_recorder; }
    const QsoRecorder& recorder() const { return m_recorder; }

    // Die Zwischenspeicher, damit ein Test von aussen hineinschreiben
    // kann — genau das, was der Audio-Faden tut.
    AudioTapRing& rxRing() { return m_rxRing; }
    AudioTapRing& txRing() { return m_txRing; }

    // Ist unterwegs etwas verlorengegangen? Groesser null heisst: in der
    // Aufnahme fehlt etwas, und das muss man sehen koennen.
    long long droppedSamples() const
    { return m_rxRing.dropped() + m_txRing.dropped(); }

    // Der Spitzenwert des letzten Abholvorgangs, je Spur, 0..1.
    //
    // Hier gerechnet und nicht in der Anzeige: die Werte liegen beim
    // Abholen ohnehin in der Hand, und ein Pegel, der aus einer ANDEREN
    // Quelle kommt als die Aufnahme, kann zappeln, waehrend die Datei
    // still bleibt. Dieser bewegt sich genau dann, wenn wirklich etwas
    // in der Aufnahme landet.
    float lastRxPeak() const { return m_rxPeak; }
    float lastTxPeak() const { return m_txPeak; }

signals:
    void recordingChanged(bool on);
    void secondsChanged(double seconds);
    // Einmal je Aufnahme, sobald der erste Wert verlorengegangen ist.
    void samplesLost();

private:
    void drain();

    QsoRecorder  m_recorder;
    AudioTapRing m_rxRing;
    AudioTapRing m_txRing;
    QTimer       m_drainTimer;

    AudioEngine*    m_audio{nullptr};
    TxWorkerThread* m_tx{nullptr};
    QMetaObject::Connection m_micTap;

    int  m_sliceId{0};
    bool m_lossReported{false};

    float m_rxPeak{0.0f};
    float m_txPeak{0.0f};

    std::vector<float> m_scratch;   // Hauptfaden, einmal angefordert
};

} // namespace Longpath
