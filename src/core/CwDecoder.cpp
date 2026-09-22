// src/core/CwDecoder.cpp
// no-port-check: this file is an AetherSDR port (registered in
// AETHERSDR-PORTS.md and aethersdr-reconciliation.md Bucket A, see the
// header block in CwDecoder.h); it cites no Thetis source.
//
// =================================================================
// src/core/CwDecoder.cpp  (Longpath)
// =================================================================
//
// Source attribution (AetherSDR -- GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       -- per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   Structural and behavioral derivative of AetherSDR's
//   src/core/CwDecoder.cpp [@e944ec49]; see CwDecoder.h for the full
//   attribution and what was translated.
//   AetherSDR is licensed under the GNU General Public License v3.
//   Longpath is also GPLv3. Attribution follows GPLv3 SS5 requirements.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "core/CwDecoder.h"

#include "core/LogCategories.h"
#include "ggmorse/ggmorse.h"

#include <QThread>

#include <algorithm>
#include <cstring>

namespace Longpath {

CwDecoder::CwDecoder(QObject* parent)
    : QObject(parent)
{
}

CwDecoder::~CwDecoder()
{
    stop();
}

void CwDecoder::setSampleRate(int hz)
{
    if (hz > 0) {
        m_sampleRateHz = hz;
    }
}

// From AetherSDR src/core/CwDecoder.cpp [@e944ec49] CwDecoder::start
void CwDecoder::start()
{
    if (m_running.load(std::memory_order_acquire)) { return; }

    {
        std::lock_guard<std::mutex> lock(m_parametersMutex);
        m_parametersDirty = true;
    }
    {
        std::lock_guard<std::mutex> lock(m_bufMutex);
        m_ring.clear();
    }
    m_running.store(true, std::memory_order_release);

    // Run the decode loop on a worker thread (CwDecoder stays on its own).
    QThread* worker = QThread::create([this]() { decodeLoop(); });
    worker->setObjectName(QStringLiteral("CwDecoder"));
    m_workerThread.reset(worker);
    worker->start();

    qCDebug(lcDsp) << "CwDecoder: started at" << m_sampleRateHz << "Hz";
}

// From AetherSDR src/core/CwDecoder.cpp [@e944ec49] CwDecoder::stop
void CwDecoder::stop()
{
    if (!m_running.load(std::memory_order_acquire)) { return; }
    m_running.store(false, std::memory_order_release);

    if (m_workerThread) {
        // A JOIN, never a timeout. The callback checks m_running and each
        // decode call is frame-bounded, so this returns promptly -- and the
        // m_pitch/m_speed writes below run OUTSIDE m_parametersMutex, which
        // the worker holds when writing the same members. They are safe
        // only because the worker is provably dead once wait() returns.
        m_workerThread->wait();
        m_workerThread.reset();
    }

    // The estimates died with the ggmorse instance -- clear them so a later
    // reading cannot show a pitch from a previous run. Locked values are
    // operator-set state, not estimates: keep them.
    if (!m_pitchLocked.load()) { m_pitch.store(0.0f); }
    if (!m_speedLocked.load()) { m_speed.store(0.0f); }
    if (!m_pitchLocked.load() || !m_speedLocked.load()) {
        // Post the clearing emission through the event queue: the worker's
        // cross-thread statsUpdated deliveries are queued, so a reading it
        // posted just before m_running flipped would otherwise arrive AFTER
        // a direct emit and re-show the dead estimate.
        QMetaObject::invokeMethod(this, [this] {
            emit statsUpdated(m_pitch.load(), m_speed.load());
        }, Qt::QueuedConnection);
    }

    qCDebug(lcDsp) << "CwDecoder: stopped";
}

// From AetherSDR src/core/CwDecoder.cpp [@e944ec49] lockPitch / lockSpeed
void CwDecoder::lockPitch(bool lock)
{
    std::lock_guard<std::mutex> guard(m_parametersMutex);
    m_pitchLocked.store(lock);
    m_pendingParameters.pitchHz = lock ? m_pitch.load() : -1.0f;
    m_parametersDirty = true;
}

void CwDecoder::lockSpeed(bool lock)
{
    std::lock_guard<std::mutex> guard(m_parametersMutex);
    m_speedLocked.store(lock);
    m_pendingParameters.speedWpm = lock ? m_speed.load() : -1.0f;
    m_parametersDirty = true;
}

void CwDecoder::setPitchRange(int minHz, int maxHz)
{
    if (minHz <= 0 || maxHz <= minHz) { return; }
    std::lock_guard<std::mutex> lock(m_parametersMutex);
    m_pendingParameters.pitchRangeMin = static_cast<float>(minHz);
    m_pendingParameters.pitchRangeMax = static_cast<float>(maxHz);
    m_parametersDirty = true;
}

// From AetherSDR src/core/CwDecoder.cpp [@e944ec49] feedAudio -- there a
// QByteArray of 24 kHz stereo float32 downmixed to int16; here Longpath's
// tap-ring convention (interleaved float32 frames) downmixed to mono
// float32, which ggmorse takes directly (GGMORSE_SAMPLE_FORMAT_F32).
void CwDecoder::feedAudio(const float* interleavedStereo, int frames)
{
    if (!m_running.load(std::memory_order_acquire) || !interleavedStereo || frames <= 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_bufMutex);
    const size_t capacity = static_cast<size_t>(m_sampleRateHz) * kRingCapacitySeconds;
    m_ring.reserve(std::min(capacity, m_ring.size() + static_cast<size_t>(frames)));
    for (int i = 0; i < frames; ++i) {
        m_ring.push_back((interleavedStereo[2 * i] + interleavedStereo[2 * i + 1]) * 0.5f);
    }
    // Trim to capacity (drop the oldest): a stalled worker must not grow
    // the ring without bound, and stale audio is worthless anyway.
    if (m_ring.size() > capacity) {
        m_ring.erase(m_ring.begin(),
                     m_ring.begin() + static_cast<std::ptrdiff_t>(m_ring.size() - capacity));
    }
}

int CwDecoder::queuedSamplesForTest() const
{
    std::lock_guard<std::mutex> lock(m_bufMutex);
    return static_cast<int>(m_ring.size());
}

// From AetherSDR src/core/CwDecoder.cpp [@e944ec49] decodeLoop
void CwDecoder::decodeLoop()
{
    GGMorse::Parameters params;
    params.sampleRateInp = static_cast<float>(m_sampleRateHz);
    params.sampleRateOut = static_cast<float>(m_sampleRateHz);
    params.samplesPerFrame = GGMorse::kDefaultSamplesPerFrame;
    params.sampleFormatInp = GGMORSE_SAMPLE_FORMAT_F32;
    params.sampleFormatOut = GGMORSE_SAMPLE_FORMAT_F32;

    GGMorse ggmorse(params);

    // ggmorse asks for samplesPerFrame * resampleFactor samples per callback:
    // 128 * (48000 / 4000) = 1536 samples = 32 ms at 48 kHz.
    const int resampleFactor =
        static_cast<int>(ggmorse.getSampleRateInp() / GGMorse::kBaseSampleRate);
    const int samplesPerCall = ggmorse.getSamplesPerFrame() * std::max(1, resampleFactor);
    qint64 framesFed = 0;

    while (m_running.load(std::memory_order_acquire)) {
        // Wait until at least one frame of audio waits.
        bool enough = false;
        {
            std::lock_guard<std::mutex> lock(m_bufMutex);
            enough = static_cast<int>(m_ring.size()) >= samplesPerCall;
        }
        if (!enough) {
            QThread::msleep(20);
            continue;
        }

        DecodeParameters pending;
        bool applyParameters = false;
        {
            std::lock_guard<std::mutex> lock(m_parametersMutex);
            if (m_parametersDirty) {
                pending = m_pendingParameters;
                m_parametersDirty = false;
                applyParameters = true;
            }
        }
        if (applyParameters) {
            GGMorse::ParametersDecode dp = GGMorse::getDefaultParametersDecode();
            dp.frequency_hz = pending.pitchHz;
            dp.speed_wpm = pending.speedWpm;
            dp.frequencyRangeMin_hz = pending.pitchRangeMin;
            dp.frequencyRangeMax_hz = pending.pitchRangeMax;
            ggmorse.setParametersDecode(dp);
        }

        int framesThisCall = 0;
        ggmorse.decode([this, &framesThisCall](void* data, uint32_t nMaxBytes) -> uint32_t {
            // Return after one frame so continuously arriving audio cannot
            // postpone pending parameter changes or stop indefinitely.
            if (!m_running.load(std::memory_order_acquire) || framesThisCall > 0) {
                return 0;
            }
            std::lock_guard<std::mutex> lock(m_bufMutex);
            const size_t wanted = nMaxBytes / sizeof(float);
            // ggmorse requires exactly nMaxBytes -- a partial return makes
            // it abort the frame (and complain on stderr).
            if (m_ring.size() < wanted) { return 0; }
            std::memcpy(data, m_ring.data(), wanted * sizeof(float));
            m_ring.erase(m_ring.begin(), m_ring.begin() + static_cast<std::ptrdiff_t>(wanted));
            ++framesThisCall;
            return static_cast<uint32_t>(wanted * sizeof(float));
        });
        framesFed += framesThisCall;

        const GGMorse::Statistics& stats = ggmorse.getStatistics();

        // Accept every decode; the cost travels with the text so the
        // display can grade it.
        GGMorse::TxRx rxData;
        if (ggmorse.takeRxData(rxData) > 0 && stats.costFunction < 1.0f) {
            const QString text = QString::fromLatin1(
                reinterpret_cast<const char*>(rxData.data()),
                static_cast<int>(rxData.size()));
            emit textDecoded(text, stats.costFunction);
        }

        if (stats.estimatedPitch_Hz > 0) {
            {
                std::lock_guard<std::mutex> lock(m_parametersMutex);
                // A just-completed old frame must not overwrite a newer lock
                // request. Locked setpoints live in the pending snapshot;
                // nonpositive locks still mean automatic detection.
                if (!m_pitchLocked.load() || m_pendingParameters.pitchHz <= 0.0f) {
                    m_pitch.store(stats.estimatedPitch_Hz);
                }
                if (!m_speedLocked.load() || m_pendingParameters.speedWpm <= 0.0f) {
                    m_speed.store(stats.estimatedSpeed_wpm);
                }
            }
            // Read the members at DELIVERY, not here: a value copied now is
            // stale by the time the queued emission lands if a setter ran
            // in between.
            QMetaObject::invokeMethod(this, [this] {
                emit statsUpdated(m_pitch.load(), m_speed.load());
            }, Qt::QueuedConnection);
        }
    }

    qCDebug(lcDsp) << "CwDecoder: decode loop exiting, frames fed:" << framesFed;
}

} // namespace Longpath
