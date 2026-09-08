// src/core/RttyDecoder.h
// Native RTTY (Baudot/ITA2) decoder for Longpath -- no external program
// (fldigi, WSJT-X) needed, unlike FT8/PSK31 which stay routed through an
// external decoder via VAC + CAT (see docs/architecture/2026-09-06-rtty-
// decoder-scoping.md for why RTTY is the one digital mode with a real,
// from-scratch decoder to port and the others are not).
//
// Runs the actual demodulation (mark/space bandpass filters, envelope
// detection, Schmitt-trigger bit slicing, start-stop frame decode) on its
// own worker thread. Feed it raw interleaved-stereo float32 PCM from the
// RTTY audio tap (AudioEngine::setRttyTap, 48 kHz, the native WDSP RX
// output rate) via feedAudio(); it emits decoded characters and live
// mark/space/lock stats back on the caller's thread via queued signals.
//
// =================================================================
// src/core/RttyDecoder.h  (Longpath)
// =================================================================
//
// Source attribution (AetherSDR -- GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       -- per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a structural and behavioral derivative of AetherSDR's
//   RTTY decoder (src/core/RttyDecoder.{h,cpp}) -- the mark/space biquad
//   bandpass design, envelope-based Schmitt-trigger bit slicing,
//   proportional clock recovery, and Baudot/ITA2 table decode are carried
//   over close to verbatim (translated for Longpath's own worker-thread
//   and audio-tap conventions, and re-derived for Longpath's native 48 kHz
//   RX audio rate rather than AetherSDR's 24 kHz). There is no Thetis
//   equivalent -- Thetis has no native digital-mode decoder of any kind,
//   relying entirely on VAC + CAT to an external program -- so AetherSDR
//   is the sole source here, same class of citation as
//   src/core/DevAutomationServer.h.
//   AetherSDR is licensed under the GNU General Public License v3.
//   Longpath is also GPLv3. Attribution follows GPLv3 SS5 requirements.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-06 -- Created for Longpath, by OE5SOS with AI-assisted
//                 transformation via Anthropic Claude Code. Default mark/
//                 shift frequencies deliberately NOT copied from
//                 AetherSDR's own defaults (2125/170 Hz): Longpath already
//                 has Thetis-sourced rttyMarkHz/rttyShiftHz on SliceModel
//                 (2295/170 Hz, From Thetis setup.designer.cs:40635-40665
//                 [v2.10.3.13]), which the operator already tunes via the
//                 existing RttyMarkShiftContainer -- this decoder reads
//                 those live instead of owning a second, divergent copy.
//                 Baud rate and reverse-polarity have no Thetis/SliceModel
//                 equivalent and remain decoder-local settings.
// =================================================================

#pragma once

#include <QObject>
#include <QByteArray>
#include <QMutex>
#include <QThread>

#include <atomic>

namespace Longpath {

// Client-side RTTY (Baudot/ITA2) decoder using mark/space bandpass filters.
// Runs decoding on a worker thread. Feed it 48 kHz interleaved-stereo
// float32 PCM (the native WDSP RX rate; see RttyDecoder.cpp's rationale
// for staying at the native rate instead of resampling to AetherSDR's
// 24 kHz -- the biquad design already takes sample rate as a parameter,
// so there is nothing to gain from resampling).
class RttyDecoder : public QObject {
    Q_OBJECT

public:
    explicit RttyDecoder(QObject* parent = nullptr);
    ~RttyDecoder() override;

    void start();
    void stop();
    bool isRunning() const { return m_running; }

    int   markFreqHz()      const { return m_markFreqHz; }
    int   shiftHz()         const { return m_shiftHz; }
    float baudRate()        const { return m_baudRate; }
    bool  reversePolarity() const { return m_reverse; }

public slots:
    void feedAudio(const float* interleavedStereo, int frames);

    // Driven from the bound slice's rttyMarkHz/rttyShiftHz (Thetis-sourced,
    // see header comment) -- not independent decoder settings.
    void setMarkFreqHz(int hz);
    void setShiftHz(int hz);

    // No SliceModel/Thetis equivalent; decoder-local.
    void setBaudRate(float baud);
    void setReversePolarity(bool rev);

signals:
    // confidence: 0.5 = weak, 1.0 = perfect (mark/space ratio during character)
    void textDecoded(const QString& text, float confidence);

    // markLevel/spaceLevel are fractional envelope strengths (0-1, sum ~ 1).
    // snrDb > 3 typically means locked.
    void statsUpdated(float markLevel, float spaceLevel, float snrDb, bool locked);

private:
    void decodeLoop();
    void recalcFilterCoeffs();

    struct BiquadState  { double x1{}, x2{}, y1{}, y2{}; };
    struct BiquadCoeffs { double b0{}, b1{}, b2{}, a1{}, a2{}; };

    static BiquadCoeffs designBandpass(double centerHz, double bwHz, double sampleRate);
    static double processBiquad(BiquadCoeffs& c, BiquadState& s, double x);

    static char baudotToAscii(int code, bool figs);

    // From Thetis setup.designer.cs:40635-40665 [v2.10.3.13] via
    // SliceModel::m_rttyMarkHz/m_rttyShiftHz -- see this file's own header
    // comment. Not a decoder-original constant, kept here only as the
    // pre-bind default before setMarkFreqHz()/setShiftHz() are called.
    static constexpr int kDefaultMarkHz  = 2295;
    static constexpr int kDefaultShiftHz = 170;

    // From Thetis radio.cs / CAT ZZ commands: no standard baud rate is
    // Thetis-sourced (Thetis has no decoder to derive one from), so this
    // keeps AetherSDR's own default -- 45.45 baud is the universal ham HF
    // RTTY rate (ITU-R 60wpm teleprinter standard), not an AetherSDR
    // invention.
    static constexpr double kDefaultBaudRate = 45.45;

    static constexpr double kSampleRate  = 48000.0;
    static constexpr int    kBaudotLtrs  = 0x1F;
    static constexpr int    kBaudotFigs  = 0x1B;
    static constexpr int    kBaudotNull  = 0x00;
    // 4 s mono at the native rate, matching AetherSDR's own margin.
    static constexpr int    kRingCapacity = static_cast<int>(kSampleRate) * sizeof(float) * 4;

    QThread*   m_workerThread{nullptr};
    QMutex     m_bufMutex;
    QByteArray m_ringBuf;

    std::atomic<bool>  m_running{false};
    std::atomic<bool>  m_paramsChanged{false};

    std::atomic<int>   m_markFreqHz{kDefaultMarkHz};
    std::atomic<int>   m_shiftHz{kDefaultShiftHz};
    std::atomic<float> m_baudRate{static_cast<float>(kDefaultBaudRate)};
    std::atomic<bool>  m_reverse{false};

    // Filter state (worker thread only -- no atomic needed)
    BiquadCoeffs m_markCoeffs;
    BiquadCoeffs m_spaceCoeffs;
    BiquadState  m_markState;
    BiquadState  m_spaceState;
    double       m_markEnv{};
    double       m_spaceEnv{};
};

} // namespace Longpath
