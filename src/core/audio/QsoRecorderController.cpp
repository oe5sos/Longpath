// =================================================================
// src/core/audio/QsoRecorderController.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Begruendung steht im Header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "core/audio/QsoRecorderController.h"

#include "core/AudioEngine.h"
#include "core/TxWorkerThread.h"

namespace NereusSDR {

QsoRecorderController::QsoRecorderController(QObject* parent)
    : QObject(parent)
{
    m_drainTimer.setInterval(kDrainMs);
    connect(&m_drainTimer, &QTimer::timeout, this,
            &QsoRecorderController::drain);
    setSampleRate(m_recorder.sampleRate());
}

QsoRecorderController::~QsoRecorderController()
{
    // Erst den Abgriff loesen, dann sterben. Andersherum schriebe der
    // Audio-Faden noch in einen Zwischenspeicher, den es nicht mehr
    // gibt — der klassische Weg, wie ein Aufnahmewerkzeug beim Beenden
    // die ganze Anwendung mitnimmt.
    if (m_audio) { m_audio->setQsoTap(nullptr, -1); }
    QObject::disconnect(m_micTap);
}

void QsoRecorderController::attach(AudioEngine* audio, TxWorkerThread* tx)
{
    if (m_audio && m_audio != audio) { m_audio->setQsoTap(nullptr, -1); }
    QObject::disconnect(m_micTap);

    m_audio = audio;
    m_tx    = tx;

    if (m_tx) {
        // DirectConnection: der Zeiger zeigt auf einen Zwischenspeicher
        // des Sendefadens, der beim naechsten Block ueberschrieben wird.
        // Eine eingereihte Verbindung wuerde den ZEIGER kopieren, nicht
        // den Ton — derselbe Vertrag wie bei antiVoxBlockReady.
        m_micTap = connect(m_tx, &TxWorkerThread::preStripAudioReady, this,
                           [this](const float* samples, int frames) {
            if (m_recorder.isRecording()) {
                m_txRing.write(samples, frames);
            }
        }, Qt::DirectConnection);
    }
}

void QsoRecorderController::setSampleRate(int hz)
{
    if (hz <= 0) { return; }
    m_recorder.setSampleRate(hz);

    // Empfang ist zweikanalig, das Mikrofon einkanalig — daher der
    // Faktor zwei nur auf der linken Spur.
    m_rxRing.resize(hz * kRingSeconds * 2);
    m_txRing.resize(hz * kRingSeconds);
    m_scratch.resize(static_cast<size_t>(hz) * kRingSeconds * 2);
}

void QsoRecorderController::start(const QsoRecordingInfo& info)
{
    if (m_recorder.isRecording()) { return; }

    m_rxRing.reset();
    m_txRing.reset();
    m_lossReported = false;
    m_recorder.clear();
    m_recorder.start(info);

    // Reihenfolge: erst aufnahmebereit, dann den Abgriff aufmachen.
    // Andersherum liefe der Audio-Faden in einen Zwischenspeicher, den
    // gleich darauf reset() leert — die ersten Zehntelsekunden waeren
    // still, und niemand wuesste warum.
    if (m_audio) { m_audio->setQsoTap(&m_rxRing, m_sliceId); }

    m_drainTimer.start();
    emit recordingChanged(true);
}

void QsoRecorderController::stop()
{
    if (!m_recorder.isRecording()) { return; }

    // Erst das Tor zu, dann das letzte Mal abholen: was noch im
    // Zwischenspeicher liegt, gehoert zur Aufnahme.
    if (m_audio) { m_audio->setQsoTap(nullptr, -1); }
    m_drainTimer.stop();
    drain();

    m_recorder.stop();
    emit recordingChanged(false);
}

void QsoRecorderController::drainNow() { drain(); }

void QsoRecorderController::drain()
{
    if (m_scratch.empty()) { return; }

    // EMPFANG ZUERST. Der Empfang ist die Uhr: QsoRecorder fuellt die
    // Sprechspur bis zum aktuellen Empfangsstand mit Stille auf, sobald
    // Mikrofonton kommt. Holte man das Mikrofon zuerst ab, laege die
    // eigene Stimme um einen Zeitgeber-Takt zu frueh.
    while (true) {
        const int n = m_rxRing.read(m_scratch.data(),
                                    static_cast<int>(m_scratch.size()));
        if (n <= 0) { break; }
        m_recorder.feedRx(m_scratch.data(), n / 2);
    }

    while (true) {
        const int n = m_txRing.read(m_scratch.data(),
                                    static_cast<int>(m_scratch.size()));
        if (n <= 0) { break; }
        m_recorder.feedTx(m_scratch.data(), n);
    }

    if (!m_lossReported && droppedSamples() > 0) {
        m_lossReported = true;
        emit samplesLost();
    }

    emit secondsChanged(m_recorder.recordedSeconds());
}

} // namespace NereusSDR
