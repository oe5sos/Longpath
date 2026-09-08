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
//   2026-09-02 — Speicherplatz-Wache statt Dauer-Deckel, von Martin
//                 Fischer, KI-gestuetzt ueber Anthropic Claude
//                 (Cowork). Begruendung im Header.
// =================================================================

#include "core/audio/QsoRecorderController.h"

#include "core/AudioEngine.h"
#include "core/TxWorkerThread.h"

#include <QFileInfo>
#include <QStorageInfo>

#include <algorithm>
#include <cmath>

namespace Longpath {

QsoRecorderController::QsoRecorderController(QObject* parent)
    : QObject(parent)
{
    m_drainTimer.setInterval(kDrainMs);
    connect(&m_drainTimer, &QTimer::timeout, this,
            &QsoRecorderController::drain);
    m_spaceTimer.setInterval(kSpaceCheckMs);
    connect(&m_spaceTimer, &QTimer::timeout, this,
            &QsoRecorderController::checkDiskSpace);
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

bool QsoRecorderController::start(const QString& wavPath,
                                  const QsoRecordingInfo& info,
                                  QString* error)
{
    if (m_recorder.isRecording()) { return false; }

    m_rxRing.reset();
    m_txRing.reset();
    m_lossReported = false;
    m_recorder.clear();
    if (!m_recorder.start(wavPath, info, error)) { return false; }

    // Reihenfolge: erst aufnahmebereit, dann den Abgriff aufmachen.
    // Andersherum liefe der Audio-Faden in einen Zwischenspeicher, den
    // gleich darauf reset() leert — die ersten Zehntelsekunden waeren
    // still, und niemand wuesste warum.
    if (m_audio) { m_audio->setQsoTap(&m_rxRing, m_sliceId); }

    m_drainTimer.start();
    m_spaceTimer.start();
    emit recordingChanged(true);
    return true;
}

void QsoRecorderController::stop()
{
    if (!m_recorder.isRecording()) { return; }

    // Erst das Tor zu, dann das letzte Mal abholen: was noch im
    // Zwischenspeicher liegt, gehoert zur Aufnahme.
    if (m_audio) { m_audio->setQsoTap(nullptr, -1); }
    m_drainTimer.stop();
    m_spaceTimer.stop();
    drain();

    m_recorder.stop();
    emit recordingChanged(false);
}

void QsoRecorderController::drainNow() { drain(); }

// From Thetis OkToRecord (clsAudioRecordPlayback.cs:630-641 [@852bf0e]):
// free space as a percentage of the drive's total size, not an absolute
// byte count — a duration-based "need N MB for N minutes" estimate
// stopped making sense once the recording length is unbounded (see
// QsoRecorder.h). allowUnknown-equivalent: an unreadable drive doesn't
// block recording, same as Thetis's OkToRecord(..., allowUnknown) call
// from the running timer.
bool QsoRecorderController::hasEnoughDiskSpace(const QString& forPath)
{
    const QStorageInfo disk(QFileInfo(forPath).absolutePath());
    if (!disk.isValid() || disk.bytesTotal() <= 0) { return true; }
    const qint64 percentFree = (disk.bytesAvailable() * 100) / disk.bytesTotal();
    return percentFree >= kFreeSpacePercentFloor;
}

void QsoRecorderController::checkDiskSpace()
{
    if (!m_recorder.isRecording()) { return; }
    if (hasEnoughDiskSpace(m_recorder.path())) { return; }

    // Thetis stops itself the same way from onRecordSpaceTimer
    // (clsAudioRecordPlayback.cs:662-692 [@852bf0e]): what's already on
    // disk stays a valid recording, it just ends earlier than planned.
    stop();
    emit diskSpaceLow();
}

void QsoRecorderController::drain()
{
    if (m_scratch.empty()) { return; }

    // EMPFANG ZUERST. Der Empfang ist die Uhr: QsoRecorder fuellt die
    // Sprechspur bis zum aktuellen Empfangsstand mit Stille auf, sobald
    // Mikrofonton kommt. Holte man das Mikrofon zuerst ab, laege die
    // eigene Stimme um einen Zeitgeber-Takt zu frueh.
    float rxPeak = 0.0f;
    float txPeak = 0.0f;

    while (true) {
        const int n = m_rxRing.read(m_scratch.data(),
                                    static_cast<int>(m_scratch.size()));
        if (n <= 0) { break; }
        for (int i = 0; i < n; ++i) {
            rxPeak = std::max(rxPeak, std::abs(m_scratch[i]));
        }
        m_recorder.feedRx(m_scratch.data(), n / 2);
    }

    while (true) {
        const int n = m_txRing.read(m_scratch.data(),
                                    static_cast<int>(m_scratch.size()));
        if (n <= 0) { break; }
        for (int i = 0; i < n; ++i) {
            txPeak = std::max(txPeak, std::abs(m_scratch[i]));
        }
        m_recorder.feedTx(m_scratch.data(), n);
    }

    m_rxPeak = rxPeak;
    m_txPeak = txPeak;

    if (!m_lossReported && droppedSamples() > 0) {
        m_lossReported = true;
        emit samplesLost();
    }

    emit secondsChanged(m_recorder.recordedSeconds());
}

} // namespace Longpath
