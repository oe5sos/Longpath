// src/core/CwDecoder.h
// no-port-check: this file is an AetherSDR port (registered in
// AETHERSDR-PORTS.md and aethersdr-reconciliation.md Bucket A, see the
// header block further down); it cites no Thetis source.
// Native CW (Morse) decoder for Longpath around ggmorse (Georgi Gerganov,
// MIT, vendored unmodified at third_party/ggmorse). No external program
// needed. Runs the decoding on its own worker thread; feed it raw
// interleaved-stereo float32 PCM from the CW audio tap
// (AudioEngine::setCwTap, 48 kHz, the native WDSP RX output rate) via
// feedAudio(); it emits decoded text and the live pitch/speed estimate
// back on the caller's thread via queued signals.
//
// =================================================================
// src/core/CwDecoder.h  (Longpath)
// =================================================================
//
// Source attribution (AetherSDR -- GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       -- per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a structural and behavioral derivative of AetherSDR's
//   CW decoder wrapper (src/core/CwDecoder.{h,cpp} [@e944ec49], the
//   state after "Fix CW decoder parameter races" 6e58e633): the worker
//   thread with the one coherent pending-parameter snapshot, the ring of
//   mono samples the worker drains one ggmorse frame at a time, the
//   pitch/speed lock semantics (a lock pins the last estimate; a locked
//   value is operator state and survives stop()), the queued-at-delivery
//   statistics emission. Translated for Longpath's audio-tap convention
//   (float32 interleaved stereo at 48 kHz in, mono float32 to ggmorse --
//   AetherSDR converts 24 kHz stereo to int16) and Longpath's logging.
//   There is no Thetis equivalent -- Thetis has no native decoder of any
//   digital mode -- so AetherSDR is the sole source here, the same class
//   of citation as src/core/RttyDecoder.h.
//   AetherSDR is licensed under the GNU General Public License v3.
//   Longpath is also GPLv3. Attribution follows GPLv3 SS5 requirements.
//
//   The decoder itself is ggmorse (MIT), vendored unmodified from
//   upstream at commit 8fb433d6 -- see third_party/ggmorse/README-LONGPATH.md
//   and docs/attribution/GGMORSE-PROVENANCE.md. AetherSDR's local ggmorse
//   patches (Nordic letters, a wider speed search) are NOT carried over;
//   the speed range therefore stays ggmorse's own 5-55 WPM.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude. Replaces nothing:
//                 Longpath had no CW decoder after the earlier port
//                 from another reference client was withdrawn.
// =================================================================

#pragma once

#include <QObject>
#include <QString>

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

class QThread;

namespace Longpath {

class CwDecoder : public QObject {
    Q_OBJECT

public:
    explicit CwDecoder(QObject* parent = nullptr);
    ~CwDecoder() override;

    // Lifecycle and parameter setters run on this QObject's owning thread.
    // feedAudio() may run on any thread (the applet's pump, a test).
    void start();
    void stop();
    bool isRunning() const { return m_running.load(std::memory_order_acquire); }

    // The rate the caller feeds. 48 kHz is Longpath's RX audio rate; a
    // test may feed something else. Takes effect on the next start().
    void setSampleRate(int hz);
    int  sampleRate() const { return m_sampleRateHz; }

    float estimatedPitch() const { return m_pitch.load(std::memory_order_relaxed); }
    float estimatedSpeed() const { return m_speed.load(std::memory_order_relaxed); }

    // Lock pitch/speed to the current detected values (prevents wandering).
    void lockPitch(bool lock);
    void lockSpeed(bool lock);
    bool isPitchLocked() const { return m_pitchLocked.load(std::memory_order_relaxed); }
    bool isSpeedLocked() const { return m_speedLocked.load(std::memory_order_relaxed); }

    // The band ggmorse searches for the tone (its own default 200-1200 Hz).
    void setPitchRange(int minHz, int maxHz);

    // Interleaved stereo float32 frames, downmixed to mono here.
    void feedAudio(const float* interleavedStereo, int frames);

    // For tests: how many mono samples wait for the worker.
    int queuedSamplesForTest() const;

signals:
    // Decoded text (one or more characters), with ggmorse's cost function
    // (lower = more confident).
    void textDecoded(const QString& text, float cost);
    void statsUpdated(float pitchHz, float speedWpm);

private:
    void decodeLoop();

    std::unique_ptr<QThread> m_workerThread;
    std::atomic<bool> m_running{false};
    int m_sampleRateHz{48000};

    // One coherent pending configuration. Only setters and the decoder
    // worker take this mutex; feedAudio() never does. GGMorse itself is
    // worker-local.
    struct DecodeParameters {
        float pitchHz{-1.0f};        // <= 0: automatic
        float speedWpm{-1.0f};       // <= 0: automatic
        // GGMorse::getDefaultParametersDecode()'s own band. An unconfigured
        // decoder must land on ggmorse's defaults, not a narrower guess.
        float pitchRangeMin{200.0f};
        float pitchRangeMax{1200.0f};
    };
    std::mutex m_parametersMutex;
    DecodeParameters m_pendingParameters;
    bool m_parametersDirty{true};

    // Mono float32 samples waiting for the worker (4 s at 48 kHz).
    mutable std::mutex m_bufMutex;
    std::vector<float> m_ring;
    static constexpr int kRingCapacitySeconds = 4;

    std::atomic<float> m_pitch{0.0f};
    std::atomic<float> m_speed{0.0f};
    std::atomic<bool>  m_pitchLocked{false};
    std::atomic<bool>  m_speedLocked{false};
};

} // namespace Longpath
