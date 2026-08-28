// =================================================================
// src/core/audio/IqRecorderController.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Begruendung steht im Header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-25 — Original fuer NereusSDR/Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "core/audio/IqRecorderController.h"

#include "models/RadioModel.h"

#include <algorithm>
#include <cmath>

namespace Longpath {

IqRecorderController::IqRecorderController(QObject* parent)
    : QObject(parent)
{
    m_drainTimer.setInterval(kDrainMs);
    connect(&m_drainTimer, &QTimer::timeout, this,
            &IqRecorderController::drain);
    setSampleRate(m_recorder.sampleRate());
}

IqRecorderController::~IqRecorderController()
{
    QObject::disconnect(m_tap);
}

void IqRecorderController::attach(RadioModel* radio)
{
    QObject::disconnect(m_tap);
    m_radio = radio;
    if (!m_radio) { return; }

    // DirectConnection: derselbe Faden, der rawIqDataForStream aus
    // forkIqToTaps() heraus sendet (der Verbindungsfaden), soll auch
    // hier direkt in den Ring schreiben — kein Umweg ueber den
    // Hauptfaden, der bei einem beschaeftigten UI sonst die Aufnahme
    // verzoegern wuerde. Der Ring selbst ist dafuer gebaut (siehe
    // AudioTapRing.h): kein Schloss, keine Speicheranforderung.
    m_tap = connect(m_radio, &RadioModel::rawIqDataForStream, this,
                    &IqRecorderController::onRawIqDataForStream,
                    Qt::DirectConnection);
}

void IqRecorderController::onRawIqDataForStream(
    int streamIndex, const QVector<float>& samples)
{
    if (streamIndex != m_streamIndex) { return; }
    if (!m_recorder.isRecording()) { return; }
    // samples ist bereits I,Q,I,Q… verschachtelt — dieselbe Form, die
    // WavRecorderController fuer Audio abgreift, nur eine andere
    // Quelle. write() zaehlt in Werten, nicht Rahmen.
    m_ring.write(samples.constData(), samples.size());
}

void IqRecorderController::setSampleRate(int hz)
{
    if (hz <= 0) { return; }
    m_recorder.setSampleRate(hz);

    // I und Q zusammen, daher der Faktor zwei — wie bei Stereo-Audio.
    m_ring.resize(hz * kRingSeconds * 2);
    m_scratch.resize(static_cast<size_t>(hz) * kRingSeconds * 2);
}

bool IqRecorderController::start(const QString& path,
                                 const IqRecordingInfo& info,
                                 QString* error)
{
    if (m_recorder.isRecording()) { return false; }

    m_ring.reset();
    m_lossReported = false;

    if (!m_recorder.start(path, info, error)) {
        return false;
    }

    m_drainTimer.start();
    emit recordingChanged(true);
    return true;
}

void IqRecorderController::stop()
{
    if (!m_recorder.isRecording()) { return; }

    m_drainTimer.stop();
    drain();   // was noch im Ring liegt, gehoert zur Aufnahme

    m_recorder.stop();
    emit recordingChanged(false);
}

void IqRecorderController::drainNow() { drain(); }

void IqRecorderController::drain()
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
