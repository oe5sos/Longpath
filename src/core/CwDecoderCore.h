#pragma once
// =================================================================
// src/core/CwDecoderCore.h  (Longpath)
// =================================================================
//
// Source attribution (Zeus station engine -- GPL-2.0-or-later):
//
//   Copyright (C) 2026 Douglas J. Cerrato (KB2UKA), Christian Suarez
//       (N9WAR), and contributors -- per
//       https://github.com/Zeus-SDR/station-engine, LICENSE and
//       ATTRIBUTIONS.md at the repository root.
//
//   This file is a port of the receive-side CW decoder in
//   Station.Engine.Hosting/CwDecoder/ at @8970f2d (Release v2.0.26):
//   GoertzelDetector.cs, AdaptiveThreshold.cs, MorseTimingEstimator.cs,
//   MorseFsm.cs and CwDecoderCore.cs. The upstream authors describe it as
//   "a first-principles implementation rather than a port of another
//   receiver's CW DSP"; there is no Thetis or AetherSDR equivalent, so
//   Zeus is the sole source. Every class below cites the file and lines
//   it was taken from. The Zeus station engine is GPL-2.0-or-later;
//   Longpath is GPLv3, so the "or later" option is exercised. "Zeus" and
//   "ZeusSDR" are trademarks of the maintainers; this is nominative use.
//   Record: docs/attribution/ZEUS-PROVENANCE.md.
//
// What it does, in the order the audio goes through it:
//
//   1. GoertzelDetector -- eleven Goertzel bins 25 Hz apart around the
//      receive pitch, one 256-sample block (5.3 ms at 48 kHz) at a time.
//      The bin with the most smoothed power becomes the tracked tone,
//      but only after eight consecutive wins, so a click or a neighbour
//      cannot retune the decoder on one block.
//   2. AdaptiveThreshold -- a noise-floor tracker with separate key-on
//      (x12) and key-off (x5) ratios, a warm-up, and a rule that a keyed
//      block never pulls the floor towards the signal. Gives tone/no
//      tone per block and an SNR in dB.
//   3. MorseTimingEstimator -- a rolling estimate of the sender's dit
//      length from the lower cluster of element durations; WPM = 1200 /
//      dit ms. Dah, letter-gap and word-gap thresholds follow from it.
//   4. MorseFsm -- turns the tone/no-tone sequence into elements, the
//      elements into a pattern, the pattern into a character via the
//      International Morse table (plus <SK>; AR/BT/KN come out as + = (
//      because those share the wire pattern). Each character carries a
//      confidence from how well its elements fit the timing.
//
// Everything here is plain C++ without Qt so the tests can drive it with
// synthetic audio and the applet's QObject wrapper (CwDecoder) stays thin.
// No allocation after construction; process() is safe to call from the
// applet's pump timer at any block size.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-17 -- Ported for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude, from the Zeus station
//                 engine @8970f2d. Straight C# -> C++ port: same
//                 constants, same arithmetic, same state machine. C#
//                 records became structs, Span became pointer+count,
//                 the Action<> callback became a std::function.
//                 Two documented deviations, both in the timing
//                 estimator and both found by the synthetic-Morse tests:
//                 letter/word gap thresholds at 2.0/5.0 dits instead of
//                 3.0/5.5 (soft keying lengthens tones and shortens gaps,
//                 so 3.0 never fired), and the displayed speed from the
//                 mean of the tone and element-gap clusters instead of
//                 tones alone (which read ~12 % slow). Marked
//                 "Longpath deviation" / "Longpath addition" at the site.
// =================================================================

#include <array>
#include <functional>
#include <string>

namespace Longpath {

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/GoertzelDetector.cs:9-139 [@8970f2d]
class CwGoertzelDetector {
public:
    // 256 samples at 48 kHz is 5.33 ms: short enough to resolve 50 WPM key
    // edges while retaining ample processing gain for a narrow CW tone.
    static constexpr int kBlockSize = 256;

    CwGoertzelDetector(int sampleRateHz, double centerFrequencyHz);

    // The currently stable acquired tone center.
    double trackedFrequencyHz() const { return m_frequenciesHz[static_cast<size_t>(m_lockedIndex)]; }

    void retune(double centerFrequencyHz);
    // Drop the acquired offset and return to the configured pitch.
    void reset() { retune(m_configuredCenterFrequencyHz); }

    // Exactly kBlockSize samples. Returns the instantaneous power in the
    // tracked bin (linear, normalised by N^2, never below 1e-20).
    double detectPower(const float* samples);

private:
    static constexpr int    kSearchStepHz          = 25;
    static constexpr int    kSearchHalfWidthSteps  = 5;
    static constexpr int    kLockConfirmationBlocks = 8;
    static constexpr double kPowerSmoothing        = 0.22;
    static constexpr int    kBins = (kSearchHalfWidthSteps * 2) + 1;

    double detectPowerAt(const float* samples, double coefficient) const;
    void   trackStableCandidate(int bestIndex);

    int    m_sampleRateHz;
    std::array<double, kBlockSize> m_window{};
    std::array<double, kBins> m_coefficients{};
    std::array<double, kBins> m_frequenciesHz{};
    std::array<double, kBins> m_instantPowers{};
    std::array<double, kBins> m_smoothedPowers{};
    int    m_lockedIndex{kSearchHalfWidthSteps};
    int    m_pendingIndex{-1};
    int    m_pendingBlocks{0};
    bool   m_havePower{false};
    double m_configuredCenterFrequencyHz{0.0};
};

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/AdaptiveThreshold.cs:7-112 [@8970f2d]
// Noise-floor tracker with separate key-on and key-off thresholds.
class CwAdaptiveThreshold {
public:
    // Returns whether a tone is present in this block.
    bool   update(double tonePower);
    bool   tonePresent() const { return m_tonePresent; }
    double snrDb() const { return m_snrDb; }
    double noiseFloor() const { return m_noiseFloor; }
    void   reset();

private:
    static constexpr double kOnRatio    = 12.0;
    static constexpr double kOffRatio   = 5.0;
    static constexpr double kNoiseAlpha = 0.045;
    static constexpr int    kWarmupBlocks = 6;
    static constexpr double kDigitalZeroPower = 1e-20;
    // 375 detector blocks are exactly two seconds at 256 samples / 48 kHz.
    static constexpr int    kDigitalZeroReacquireBlocks = 375;
    // -120 dBFS power is below practical post-AGC receiver noise but keeps a
    // squelched digital-zero interval from collapsing the release threshold.
    static constexpr double kMinimumNoiseFloor = 1e-12;

    double m_noiseFloor{0.0};
    double m_lastPower{0.0};
    double m_snrDb{0.0};
    int    m_blocks{0};
    int    m_digitalZeroBlocks{0};
    bool   m_hasNonZeroInput{false};
    bool   m_tonePresent{false};
};

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/MorseTimingEstimator.cs:7-88 [@8970f2d]
// Rolling lower-cluster estimate of the sender's dit duration.
class CwMorseTimingEstimator {
public:
    explicit CwMorseTimingEstimator(double initialWpm = 20.0);

    double ditMs() const { return m_ditMs; }
    double dahThresholdMs() const { return 1.5 * m_ditMs; }
    // Longpath deviation from upstream (3.0 and 5.5): the dit estimate
    // comes from measured TONE lengths, and any keying envelope makes a
    // tone measure longer than nominal and the gap after it shorter --
    // with 5 ms raised-cosine edges at high SNR a 60 ms dit reads 70 ms
    // and the 180 ms letter gap reads 170 ms, so "gap >= 3.0 x dit" (210)
    // never fires and whole words collapse into one "?". The midpoints
    // between the nominal 1/3/7-dit gaps keep a margin on both sides:
    // element gaps (~0.7 dit) stay below 2.0, letter gaps (~2.4) above it,
    // word gaps (~5.9) above 5.0 and letter gaps below it. Pinned by
    // tst_cw_decoder with soft-keyed synthetic Morse at 12, 20 and 35 WPM.
    double letterGapThresholdMs() const { return 2.0 * m_ditMs; }
    double wordGapThresholdMs() const { return 5.0 * m_ditMs; }
    double elementGapThresholdMs() const { return 1.2 * m_ditMs; }
    // Longpath addition: the speed shown to the operator. The same edge
    // effect that lengthens tones shortens the gaps between elements by
    // the same amount, so the mean of the two lower clusters is the true
    // dit -- upstream reports 1200 / (tone dit) and reads ~12 % slow on a
    // soft-keyed 20 WPM signal. Until a gap has been seen this is the
    // tone-based estimate.
    double effectiveDitMs() const;
    double wpm() const { return 1200.0 / effectiveDitMs(); }
    bool   isDah(double durationMs) const { return durationMs >= dahThresholdMs(); }

    void observeElement(double durationMs);
    // Longpath addition: a gap between two elements of one character
    // (the FSM only reports gaps below the letter threshold). Same fast
    // tracker as the tones, kept separately.
    void observeElementGap(double durationMs);
    void reset(double initialWpm = 20.0);

private:
    static constexpr int    kWindowSize = 48;
    static constexpr double kMinDitMs = 24.0;
    static constexpr double kMaxDitMs = 240.0;

    void reestimate();

    std::array<double, kWindowSize> m_durations{};
    std::array<double, kWindowSize> m_scratch{};
    int    m_count{0};
    int    m_cursor{0};
    int    m_sinceEstimate{0};
    double m_ditMs{60.0};
    // Longpath addition: lower-cluster estimate of the element gap.
    double m_gapDitMs{0.0};
};

struct CwDecodedSymbol {
    std::string text;
    float       confidence{0.0f};
};

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/MorseFsm.cs:7-144 [@8970f2d]
// Converts thresholded key timing into International Morse text.
class CwMorseFsm {
public:
    static constexpr const char* kUnknownPlaceholder = "?";

    explicit CwMorseFsm(CwMorseTimingEstimator& timing) : m_timing(timing) {}

    // Look a dot/dash pattern up in the table; "?" when unknown.
    static std::string decodePattern(const std::string& pattern);

    // One block of tone/no-tone. Returns true when a symbol (a character
    // or a word space) was emitted into `symbol`.
    bool process(bool tone, double blockDurationMs, CwDecodedSymbol& symbol);
    void reset();

private:
    void appendElement(double durationMs);
    bool tryEmitGap(CwDecodedSymbol& symbol);

    CwMorseTimingEstimator& m_timing;
    std::array<char, 8> m_pattern{};
    int    m_patternLength{0};
    bool   m_initialized{false};
    bool   m_tone{false};
    double m_durationMs{0.0};
    double m_fitSum{0.0};
    int    m_fitCount{0};
    bool   m_letterEmitted{false};
    bool   m_wordEmitted{false};
    double m_quantumMs{0.0};
};

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/CwDecoderCore.cs:7-62 [@8970f2d]
// Allocation-stable Goertzel -> threshold -> timing -> Morse pipeline.
class CwDecoderCore {
public:
    using SymbolCallback = std::function<void(const CwDecodedSymbol&)>;

    CwDecoderCore(int sampleRateHz, double centerFrequencyHz);

    int    sampleRateHz() const { return m_sampleRateHz; }
    double wpm() const { return m_timing.wpm(); }
    double snrDb() const { return m_threshold.snrDb(); }
    bool   tonePresent() const { return m_threshold.tonePresent(); }
    double trackedToneHz() const { return m_detector.trackedFrequencyHz(); }

    void retune(double centerFrequencyHz) { m_detector.retune(centerFrequencyHz); }

    // Mono float samples at sampleRateHz(), any count; symbols are handed
    // to `onDecoded` as they complete. The last partial block is kept for
    // the next call.
    void process(const float* samples, int count, const SymbolCallback& onDecoded);
    void reset();

private:
    int                    m_sampleRateHz;
    CwGoertzelDetector     m_detector;
    CwAdaptiveThreshold    m_threshold;
    CwMorseTimingEstimator m_timing;
    CwMorseFsm             m_fsm;
    std::array<float, CwGoertzelDetector::kBlockSize> m_block{};
    int                    m_blockFill{0};
};

} // namespace Longpath
