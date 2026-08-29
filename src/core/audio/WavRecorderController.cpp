// =================================================================
// src/core/audio/WavRecorderController.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Begruendung steht im Header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-25 — Original fuer NereusSDR/Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "core/audio/WavRecorderController.h"

#include "core/AudioEngine.h"

#include <algorithm>
#include <cmath>

namespace Longpath {

WavRecorderController::WavRecorderController(QObject* parent)
    : QObject(parent)
{
    m_drainTimer.setInterval(kDrainMs);
    connect(&m_drainTimer, &QTimer::timeout, this,
            &WavRecorderController::drain);
    setSampleRate(m_recorder.sampleRate());
}

WavRecorderController::~WavRecorderController()
{
    // Erst den Abgriff loesen, dann sterben — wie QsoRecorderController.
    // Andersherum schriebe der Audio-Faden noch in einen
    // Zwischenspeicher, den es nicht mehr gibt.
    if (m_audio) { m_audio->setWavRecordTap(nullptr, -1); }
}

void WavRecorderController::attach(AudioEngine* audio)
{
    if (m_audio && m_audio != audio) { m_audio->setWavRecordTap(nullptr, -1); }
    m_audio = audio;
}

void WavRecorderController::setSampleRate(int hz)
{
    if (hz <= 0) { return; }
    m_recorder.setSampleRate(hz);

    // Zweikanalig, daher der Faktor zwei.
    m_ring.resize(hz * kRingSeconds * 2);
    m_scratch.resize(static_cast<size_t>(hz) * kRingSeconds * 2);
}

bool WavRecorderController::start(const QString& path,
                                  const WavRecordingInfo& info,
                                  QString* error)
{
    if (m_recorder.isRecording()) { return false; }

    m_ring.reset();
    m_lossReported = false;

    if (!m_recorder.start(path, info, error)) {
        return false;
    }

    // Reihenfolge wie QsoRecorderController: erst aufnahmebereit, dann
    // den Abgriff aufmachen. Andersherum liefe der Audio-Faden in
    // einen Zwischenspeicher, den gleich darauf reset() leert.
    if (m_audio) { m_audio->setWavRecordTap(&m_ring, m_sliceId); }

    m_drainTimer.start();
    emit recordingChanged(true);
    return true;
}

void WavRecorderController::stop()
{
    if (!m_recorder.isRecording()) { return; }

    // Erst das Tor zu, dann das letzte Mal abholen: was noch im
    // Zwischenspeicher liegt, gehoert zur Aufnahme.
    if (m_audio) { m_audio->setWavRecordTap(nullptr, -1); }
    m_drainTimer.stop();
    drain();

    m_recorder.stop();
    emit recordingChanged(false);
}

void WavRecorderController::drainNow() { drain(); }

void WavRecorderController::drain()
{
    if (m_scratch.empty()) { return; }

    float peak = 0.0f;

    while (true) {
        const int n = m_ring.read(m_scratch.data(),
                                  static_cast<int>(m_scratch.size()));
        if (n <= 0) { break; }
        for (int i = 0; i < n; ++i) {
            peak = std::max(peak, std::abs(m_scratch[i]));
        }
        m_recorder.feed(m_scratch.data(), n / 2);
    }

    m_peak = peak;

    if (!m_lossReported && droppedSamples() > 0) {
        m_lossReported = true;
        emit samplesLost();
    }

    emit secondsChanged(m_recorder.recordedSeconds());
}

} // namespace Longpath
