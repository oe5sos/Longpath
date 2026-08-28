#pragma once

// =================================================================
// src/core/audio/IqRecorderController.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Was den I/Q-Abgriff mit IqRecorder verbindet — der Gegenpart zu
// WavRecorderController, aber an einer anderen Stelle angeschlossen.
//
// ── Warum kein neuer Abgriff in AudioEngine/RadioModel noetig war ────
//
// WavRecorder braucht einen NEUEN Abgriffspunkt, weil demodulierter
// Ton nirgendwo sonst nach aussen dringt — er entsteht im Audio-Faden
// und wird dort sofort mit Mixer und Lautstaerke vermischt.
//
// Rohes I/Q dagegen wird schon oeffentlich verteilt: RadioModel
// zweigt es fuer den Wasserfall/FFT-Pfad ab und sendet es als
// rawIqDataForStream(streamIndex, samples) — siehe
// RadioModel::forkIqToTaps(), design doc
// docs/architecture/phase3m-recording-design.md §6. Diese Klasse
// haengt sich einfach an DASSELBE Signal, mit DirectConnection, genau
// wie es der FFT-Pfad selbst tut — kein einziges bestehendes File
// musste dafuer angefasst werden.
//
// ── Warum trotzdem ein Zwischenspeicher (wie bei QSO/WAV) ────────────
//
// rawIqDataForStream feuert auf dem Verbindungsfaden (DirectConnection
// aus forkIqToTaps), nicht dem Hauptfaden. Datei schreiben darf dort
// genausowenig passieren wie im Audio-Rueckruf — derselbe Grund wie
// bei AudioTapRing.h: unvorhersehbare Wartezeit auf einem Faden, der
// pausenlos Netzwerkpakete verarbeiten muss. Also wieder: Faden
// schreibt in den Ring, Zeitgeber im Hauptfaden holt ab.
//
// Design doc: docs/architecture/phase3m-recording-design.md §7.2.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-25 — Original fuer NereusSDR/Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QObject>
#include <QTimer>

#include "core/audio/AudioTapRing.h"
#include "core/audio/IqRecorder.h"

namespace Longpath {

class RadioModel;

class IqRecorderController : public QObject
{
    Q_OBJECT

public:
    // Zwei Sekunden Vorrat. I/Q-Raten liegen oft ueber Audio-Raten,
    // also grosszuegiger bemessen als bei WavRecorderController.
    static constexpr int kRingSeconds = 2;
    static constexpr int kDrainMs     = 200;

    explicit IqRecorderController(QObject* parent = nullptr);
    ~IqRecorderController() override;

    // Darf nullptr sein — ohne Funkgeraet laesst sich alles ausser dem
    // Datenstrom pruefen.
    void attach(RadioModel* radio);

    // Welcher Stream (logischer Empfaenger-Index) aufgenommen wird.
    // Vor start() setzen.
    void setStreamIndex(int streamIndex) { m_streamIndex = streamIndex; }
    int  streamIndex() const { return m_streamIndex; }

    void setSampleRate(int hz);
    int  sampleRate() const { return m_recorder.sampleRate(); }

    bool isRecording() const { return m_recorder.isRecording(); }

    bool start(const QString& path, const IqRecordingInfo& info,
              QString* error = nullptr);
    void stop();

    // Nur fuer Tests und fuer den Notfall: holt jetzt ab, statt auf den
    // Zeitgeber zu warten.
    void drainNow();

    IqRecorder&       recorder()       { return m_recorder; }
    const IqRecorder& recorder() const { return m_recorder; }

    // Der Zwischenspeicher, damit ein Test von aussen hineinschreiben
    // kann — genau das, was rawIqDataForStream tut.
    AudioTapRing& ring() { return m_ring; }

    long long droppedSamples() const { return m_ring.dropped(); }

    float lastPeak() const { return m_peak; }

signals:
    void recordingChanged(bool on);
    void secondsChanged(double seconds);
    void samplesLost();

private:
    void drain();
    void onRawIqDataForStream(int streamIndex, const QVector<float>& samples);

    IqRecorder   m_recorder;
    AudioTapRing m_ring;
    QTimer       m_drainTimer;

    RadioModel* m_radio{nullptr};
    QMetaObject::Connection m_tap;

    int  m_streamIndex{0};
    bool m_lossReported{false};

    float m_peak{0.0f};

    std::vector<float> m_scratch;   // Hauptfaden, einmal angefordert
};

} // namespace Longpath
