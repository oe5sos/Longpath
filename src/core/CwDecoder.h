#pragma once
// =================================================================
// src/core/CwDecoder.h  (Longpath)
// =================================================================
//
// Longpath-original. The Qt face of the CW decoder: takes the applet's
// stereo tap audio, feeds the ported Zeus pipeline in CwDecoderCore, and
// turns its callbacks into signals. The DSP itself is in CwDecoderCore
// (see its header for the attribution); nothing here is ported.
//
// Why no worker thread, unlike RttyDecoder: eleven Goertzel bins over a
// 256-sample block are a few thousand multiply-adds every 5.3 ms. The
// applet's 50 ms pump hands over ~2400 frames at a time, which is nine
// blocks -- microseconds on the GUI thread. A thread, a mutex and a
// second ring would be more code than the decoder.
//
// Stats are emitted at most ten times a second (the Zeus broadcast
// cadence), text as it decodes.
// no-port-check: Longpath-original wrapper; the ported logic lives in
// CwDecoderCore.{h,cpp} and is cited there.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-17 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "core/CwDecoderCore.h"

#include <QElapsedTimer>
#include <QObject>
#include <QString>

#include <vector>

namespace Longpath {

class CwDecoder : public QObject {
    Q_OBJECT
public:
    explicit CwDecoder(QObject* parent = nullptr);
    ~CwDecoder() override = default;

    void start();
    void stop();
    bool isRunning() const { return m_running; }

    int    pitchHz() const { return m_pitchHz; }
    double wpm() const { return m_core.wpm(); }
    double snrDb() const { return m_core.snrDb(); }
    bool   tonePresent() const { return m_core.tonePresent(); }
    double trackedToneHz() const { return m_core.trackedToneHz(); }

    static constexpr int kSampleRateHz = 48000;   // the RX tap's native rate
    static constexpr int kMinPitchHz = 100;       // Thetis udCWPitch range
    static constexpr int kMaxPitchHz = 2000;

public slots:
    // Interleaved stereo float32 at 48 kHz, as AudioEngine's taps deliver
    // it. The left channel is decoded; the tap is a mono signal on both.
    void feedAudio(const float* interleavedStereo, int frames);
    // Receive pitch the decoder searches around (+-125 Hz). Clamped to
    // 100..2000 Hz; retunes and re-acquires immediately.
    void setPitchHz(int hz);
    // Forget everything decoded so far (timing, noise floor, tone lock).
    void reset();

signals:
    void textDecoded(const QString& text, float confidence);
    void statsUpdated(float wpm, float snrDb, bool tonePresent, float trackedHz);

private:
    void emitStatsIfDue();

    CwDecoderCore      m_core;
    std::vector<float> m_mono;
    QElapsedTimer      m_statsClock;
    int                m_pitchHz{600};
    bool               m_running{false};
    bool               m_statsEverSent{false};
};

} // namespace Longpath
