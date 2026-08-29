#pragma once

// =================================================================
// src/core/audio/WavRecorderController.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Was den Audio-Abgriff mit WavRecorder verbindet — der einspurige
// Gegenpart zu QsoRecorderController (siehe dort fuer die ausfuehrliche
// Begruendung des Zwischenspeicher+Zeitgeber-Baus, AudioTapRing.h).
//
//   EMPFANG   AudioEngine::setWavRecordTap → Zwischenspeicher → hier
//             abgeholt und an WavRecorder gereicht
//
// ── Warum eine eigene Klasse statt QsoRecorderController wiederzuverwenden ──
//
// QsoRecorderController kennt zwei Abgriffe (Empfang UND Mikrofon) und
// eine Ausrichtungsregel zwischen ihnen. Eine "off the air"-Aufnahme
// hat nur eine Quelle — die Ausrichtungsfrage existiert nicht, und ein
// zweiter, hier ungenutzter Zwischenspeicher waere nur totes Gewicht.
//
// Design doc: docs/architecture/phase3m-recording-design.md §7.1.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-25 — Original fuer NereusSDR/Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QObject>
#include <QTimer>

#include "core/audio/AudioTapRing.h"
#include "core/audio/WavRecorder.h"

namespace Longpath {

class AudioEngine;

class WavRecorderController : public QObject
{
    Q_OBJECT

public:
    // Zwei Sekunden Vorrat. Der Zeitgeber holt zehnmal so oft ab — wie
    // QsoRecorderController, aus demselben Grund (siehe dort).
    static constexpr int kRingSeconds = 2;
    static constexpr int kDrainMs     = 200;

    explicit WavRecorderController(QObject* parent = nullptr);
    ~WavRecorderController() override;

    // Darf nullptr sein — ohne Funkgeraet laesst sich alles ausser dem
    // Ton pruefen.
    void attach(AudioEngine* audio);

    // Welche Scheibe aufgenommen wird. Vor start() setzen.
    void setSliceId(int sliceId) { m_sliceId = sliceId; }
    int  sliceId() const { return m_sliceId; }

    void setSampleRate(int hz);
    int  sampleRate() const { return m_recorder.sampleRate(); }

    bool isRecording() const { return m_recorder.isRecording(); }

    // Oeffnet die Datei sofort (WavRecorder ist streamend) und macht
    // danach den Abgriff auf. Gibt false zurueck, wenn die Datei nicht
    // angelegt werden konnte.
    bool start(const QString& path, const WavRecordingInfo& info,
              QString* error = nullptr);
    void stop();

    // Nur fuer Tests und fuer den Notfall: holt jetzt ab, statt auf den
    // Zeitgeber zu warten.
    void drainNow();

    WavRecorder&       recorder()       { return m_recorder; }
    const WavRecorder& recorder() const { return m_recorder; }

    // Der Zwischenspeicher, damit ein Test von aussen hineinschreiben
    // kann — genau das, was der Audio-Faden tut.
    AudioTapRing& ring() { return m_ring; }

    // Groesser null heisst: in der Aufnahme fehlt etwas.
    long long droppedSamples() const { return m_ring.dropped(); }

    // Der Spitzenwert des letzten Abholvorgangs, 0..1. Begruendung wie
    // bei QsoRecorderController: hier gerechnet, weil er sich genau
    // dann bewegt, wenn wirklich etwas in der Aufnahme landet.
    float lastPeak() const { return m_peak; }

signals:
    void recordingChanged(bool on);
    void secondsChanged(double seconds);
    // Einmal je Aufnahme, sobald der erste Wert verlorengegangen ist.
    void samplesLost();

private:
    void drain();

    WavRecorder  m_recorder;
    AudioTapRing m_ring;
    QTimer       m_drainTimer;

    AudioEngine* m_audio{nullptr};

    int  m_sliceId{0};
    bool m_lossReported{false};

    float m_peak{0.0f};

    std::vector<float> m_scratch;   // Hauptfaden, einmal angefordert
};

} // namespace Longpath
