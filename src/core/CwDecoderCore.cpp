// =================================================================
// src/core/CwDecoderCore.cpp  (Longpath)
// =================================================================
//
// Source attribution (Zeus station engine -- GPL-2.0-or-later):
//   Copyright (C) 2026 Douglas J. Cerrato (KB2UKA), Christian Suarez
//   (N9WAR), and contributors. Port of Station.Engine.Hosting/CwDecoder/
//   at @8970f2d; see CwDecoderCore.h for the full statement and
//   docs/attribution/ZEUS-PROVENANCE.md for the record.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-17 -- Ported for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude. Arithmetic and
//                 constants unchanged from upstream.
// =================================================================

#include "core/CwDecoderCore.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace Longpath {

namespace {
constexpr double kPi = 3.14159265358979323846;

template <typename T>
T clampTo(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
} // namespace

// ── CwGoertzelDetector ───────────────────────────────────────────────────

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/GoertzelDetector.cs:34-41 [@8970f2d]
CwGoertzelDetector::CwGoertzelDetector(int sampleRateHz, double centerFrequencyHz)
    : m_sampleRateHz(sampleRateHz > 0 ? sampleRateHz : 48000)
{
    for (int i = 0; i < kBlockSize; ++i) {
        m_window[static_cast<size_t>(i)] =
            0.5 - (0.5 * std::cos(2.0 * kPi * i / (kBlockSize - 1)));
    }
    retune(centerFrequencyHz);
}

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/GoertzelDetector.cs:46-67 [@8970f2d]
void CwGoertzelDetector::retune(double centerFrequencyHz)
{
    // Upstream throws on an out-of-range pitch; a decoder in a radio
    // panel must not, so the pitch is clamped into the audible band
    // instead and the applet keeps its own 100..2000 Hz limits.
    const double highestFrequency = (m_sampleRateHz / 2.0) - 1.0;
    centerFrequencyHz = clampTo(centerFrequencyHz, 1.0, highestFrequency);

    m_configuredCenterFrequencyHz = centerFrequencyHz;
    for (int i = 0; i < kBins; ++i) {
        const int step = i - kSearchHalfWidthSteps;
        const double frequency =
            clampTo(centerFrequencyHz + (step * kSearchStepHz), 1.0, highestFrequency);
        m_frequenciesHz[static_cast<size_t>(i)] = frequency;
        m_coefficients[static_cast<size_t>(i)] =
            2.0 * std::cos(2.0 * kPi * frequency / m_sampleRateHz);
    }

    m_instantPowers.fill(0.0);
    m_smoothedPowers.fill(0.0);
    m_lockedIndex = kSearchHalfWidthSteps;
    m_pendingIndex = -1;
    m_pendingBlocks = 0;
    m_havePower = false;
}

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/GoertzelDetector.cs:72-96 [@8970f2d]
double CwGoertzelDetector::detectPower(const float* samples)
{
    int bestIndex = m_lockedIndex;
    double bestPower = -1.0;   // every power is >= 1e-20, so this always loses
    for (int candidate = 0; candidate < kBins; ++candidate) {
        const auto c = static_cast<size_t>(candidate);
        const double power = detectPowerAt(samples, m_coefficients[c]);
        m_instantPowers[c] = power;
        m_smoothedPowers[c] = m_havePower
            ? m_smoothedPowers[c] + (kPowerSmoothing * (power - m_smoothedPowers[c]))
            : power;
        if (m_smoothedPowers[c] > bestPower) {
            bestPower = m_smoothedPowers[c];
            bestIndex = candidate;
        }
    }

    m_havePower = true;
    trackStableCandidate(bestIndex);
    return m_instantPowers[static_cast<size_t>(m_lockedIndex)];
}

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/GoertzelDetector.cs:98-110 [@8970f2d]
double CwGoertzelDetector::detectPowerAt(const float* samples, double coefficient) const
{
    double s1 = 0.0;
    double s2 = 0.0;
    for (int i = 0; i < kBlockSize; ++i) {
        const double s0 = samples[i] * m_window[static_cast<size_t>(i)]
                        + (coefficient * s1) - s2;
        s2 = s1;
        s1 = s0;
    }
    const double power = s1 * s1 + s2 * s2 - coefficient * s1 * s2;
    return std::max(power / (double(kBlockSize) * double(kBlockSize)), 1e-20);
}

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/GoertzelDetector.cs:112-138 [@8970f2d]
void CwGoertzelDetector::trackStableCandidate(int bestIndex)
{
    if (bestIndex == m_lockedIndex) {
        m_pendingIndex = -1;
        m_pendingBlocks = 0;
        return;
    }

    if (bestIndex == m_pendingIndex) {
        ++m_pendingBlocks;
    } else {
        m_pendingIndex = bestIndex;
        m_pendingBlocks = 1;
    }

    if (m_pendingBlocks < kLockConfirmationBlocks) { return; }
    m_lockedIndex = m_pendingIndex;
    m_pendingIndex = -1;
    m_pendingBlocks = 0;
}

// ── CwAdaptiveThreshold ──────────────────────────────────────────────────

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/AdaptiveThreshold.cs:30-101 [@8970f2d]
bool CwAdaptiveThreshold::update(double tonePower)
{
    if (tonePower <= kDigitalZeroPower) {
        if (m_digitalZeroBlocks < kDigitalZeroReacquireBlocks) { ++m_digitalZeroBlocks; }
        if (m_blocks == 0) { m_noiseFloor = kMinimumNoiseFloor; }
        else               { m_noiseFloor = std::max(m_noiseFloor, kMinimumNoiseFloor); }
        m_tonePresent = false;
        return false;
    }

    tonePower = std::max(tonePower, kMinimumNoiseFloor);
    m_lastPower = tonePower;

    // Reacquire the real receiver floor instead of treating the first
    // post-squelch noise block as a key-down edge. A cold decoder does not
    // rebase, so leading silence cannot hide its first keyed element.
    const bool reacquire = m_hasNonZeroInput
        && m_digitalZeroBlocks >= kDigitalZeroReacquireBlocks;
    m_digitalZeroBlocks = 0;
    m_hasNonZeroInput = true;
    if (reacquire) {
        m_noiseFloor = tonePower;
        m_blocks = 1;
        return false;
    }

    if (m_blocks++ == 0) {
        m_noiseFloor = std::max(tonePower, kMinimumNoiseFloor);
        return false;
    }

    if (m_blocks <= kWarmupBlocks) {
        m_noiseFloor = std::max(kMinimumNoiseFloor,
                                m_noiseFloor + 0.25 * (tonePower - m_noiseFloor));
        return false;
    }

    const double threshold = m_noiseFloor * (m_tonePresent ? kOffRatio : kOnRatio);
    if (m_tonePresent) {
        if (tonePower < threshold) { m_tonePresent = false; }
    } else if (tonePower > threshold) {
        m_tonePresent = true;
    }

    if (m_tonePresent) {
        const double ratio = m_lastPower / std::max(m_noiseFloor, kMinimumNoiseFloor);
        m_snrDb = clampTo(10.0 * std::log10(std::max(ratio, 1e-12)), -30.0, 60.0);
    }

    // A keyed block must not pull the noise estimate toward the signal.
    // During key-up, the EMA follows AGC/QSB changes in roughly 120 ms.
    if (!m_tonePresent) {
        m_noiseFloor = std::max(kMinimumNoiseFloor,
                                m_noiseFloor + kNoiseAlpha * (tonePower - m_noiseFloor));
    }

    return m_tonePresent;
}

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/AdaptiveThreshold.cs:103-111 [@8970f2d]
void CwAdaptiveThreshold::reset()
{
    m_noiseFloor = 0.0;
    m_lastPower = 0.0;
    m_blocks = 0;
    m_snrDb = 0.0;
    m_digitalZeroBlocks = 0;
    m_hasNonZeroInput = false;
    m_tonePresent = false;
}

// ── CwMorseTimingEstimator ───────────────────────────────────────────────

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/MorseTimingEstimator.cs:19-22 [@8970f2d]
CwMorseTimingEstimator::CwMorseTimingEstimator(double initialWpm)
{
    reset(initialWpm);
}

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/MorseTimingEstimator.cs:31-50 [@8970f2d]
void CwMorseTimingEstimator::observeElement(double durationMs)
{
    if (!std::isfinite(durationMs) || durationMs <= 0.0) { return; }
    durationMs = clampTo(durationMs, kMinDitMs * 0.35, kMaxDitMs * 4.0);
    m_durations[static_cast<size_t>(m_cursor)] = durationMs;
    m_cursor = (m_cursor + 1) % kWindowSize;
    if (m_count < kWindowSize) { ++m_count; }

    // Fast lower-cluster tracking gives a cold decoder useful timing in
    // the first character; the periodic clustering below rejects dahs.
    if (durationMs < dahThresholdMs()) {
        m_ditMs = clampTo(m_ditMs + 0.28 * (durationMs - m_ditMs), kMinDitMs, kMaxDitMs);
    }

    if (++m_sinceEstimate >= 6) {
        m_sinceEstimate = 0;
        reestimate();
    }
}

// Longpath addition -- see the header.
void CwMorseTimingEstimator::observeElementGap(double durationMs)
{
    if (!std::isfinite(durationMs) || durationMs <= 0.0) { return; }
    durationMs = clampTo(durationMs, kMinDitMs * 0.35, kMaxDitMs * 4.0);
    if (m_gapDitMs <= 0.0) {
        m_gapDitMs = durationMs;
        return;
    }
    // Only gaps in the dit cluster: a letter gap that slipped through
    // (e.g. during a speed change) must not drag the estimate up.
    if (durationMs < 1.5 * m_gapDitMs) {
        m_gapDitMs = clampTo(m_gapDitMs + 0.28 * (durationMs - m_gapDitMs),
                             kMinDitMs * 0.5, kMaxDitMs);
    }
}

// Longpath addition -- see the header.
double CwMorseTimingEstimator::effectiveDitMs() const
{
    if (m_gapDitMs <= 0.0) { return m_ditMs; }
    return clampTo(0.5 * (m_ditMs + m_gapDitMs), kMinDitMs, kMaxDitMs);
}

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/MorseTimingEstimator.cs:52-78 [@8970f2d]
void CwMorseTimingEstimator::reestimate()
{
    if (m_count < 3) { return; }
    std::copy_n(m_durations.begin(), m_count, m_scratch.begin());
    std::sort(m_scratch.begin(), m_scratch.begin() + m_count);

    // The lower timing cluster is dits. Split at the largest adjacent
    // ratio; with only dits present, use the lower two-thirds median.
    int split = std::max(1, (m_count * 2) / 3);
    double bestRatio = 1.0;
    for (int i = 1; i < m_count; ++i) {
        const double ratio = m_scratch[static_cast<size_t>(i)]
                           / std::max(m_scratch[static_cast<size_t>(i - 1)], 1e-9);
        if (ratio > bestRatio) {
            bestRatio = ratio;
            split = i;
        }
    }
    if (bestRatio < 1.45) { split = std::max(1, (m_count * 2) / 3); }

    const double median = m_scratch[static_cast<size_t>((split - 1) / 2)];
    m_ditMs = clampTo((m_ditMs * 0.35) + (median * 0.65), kMinDitMs, kMaxDitMs);
}

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/MorseTimingEstimator.cs:80-87 [@8970f2d]
void CwMorseTimingEstimator::reset(double initialWpm)
{
    m_durations.fill(0.0);
    m_count = 0;
    m_cursor = 0;
    m_sinceEstimate = 0;
    m_ditMs = clampTo(1200.0 / initialWpm, kMinDitMs, kMaxDitMs);
    m_gapDitMs = 0.0;
}

// ── CwMorseFsm ───────────────────────────────────────────────────────────

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/MorseFsm.cs:13-31 [@8970f2d]
// AR, BT, and KN share their wire patterns with +, =, and (. Those
// conventional glyphs preserve round-trippable punctuation while also
// rendering the corresponding prosigns. SK is unambiguous and is named.
std::string CwMorseFsm::decodePattern(const std::string& pattern)
{
    static const std::unordered_map<std::string, std::string> kTable = {
        {".-", "A"}, {"-...", "B"}, {"-.-.", "C"}, {"-..", "D"}, {".", "E"},
        {"..-.", "F"}, {"--.", "G"}, {"....", "H"}, {"..", "I"}, {".---", "J"},
        {"-.-", "K"}, {".-..", "L"}, {"--", "M"}, {"-.", "N"}, {"---", "O"},
        {".--.", "P"}, {"--.-", "Q"}, {".-.", "R"}, {"...", "S"}, {"-", "T"},
        {"..-", "U"}, {"...-", "V"}, {".--", "W"}, {"-..-", "X"}, {"-.--", "Y"}, {"--..", "Z"},
        {"-----", "0"}, {".----", "1"}, {"..---", "2"}, {"...--", "3"}, {"....-", "4"},
        {".....", "5"}, {"-....", "6"}, {"--...", "7"}, {"---..", "8"}, {"----.", "9"},
        {".-.-.-", "."}, {"--..--", ","}, {"..--..", "?"}, {"-..-.", "/"}, {".--.-.", "@"},
        {"-...-", "="}, {".-.-.", "+"}, {"..--.-", "_"}, {".----.", "'"}, {"-.--.-", ")"},
        {"-.--.", "("}, {"-.-.--", "!"}, {"---...", ":"}, {"-.-.-.", ";"}, {".-..-.", "\""},
        {"...-.-", "<SK>"},
    };
    const auto it = kTable.find(pattern);
    return it == kTable.end() ? std::string(kUnknownPlaceholder) : it->second;
}

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/MorseFsm.cs:50-83 [@8970f2d]
bool CwMorseFsm::process(bool tone, double blockDurationMs, CwDecodedSymbol& symbol)
{
    symbol = CwDecodedSymbol{};
    m_quantumMs = blockDurationMs;
    if (!m_initialized) {
        m_initialized = true;
        m_tone = tone;
        m_durationMs = blockDurationMs;
        return false;
    }

    if (tone == m_tone) {
        m_durationMs += blockDurationMs;
        return !tone && tryEmitGap(symbol);
    }

    if (m_tone) {
        // A block detector reports the edge after one complete analysis
        // quantum. Remove that fixed latency before clustering; otherwise
        // dits grow while the complementary key-up gaps shrink.
        appendElement(std::max(blockDurationMs, m_durationMs - blockDurationMs));
        m_tone = false;
        m_durationMs = blockDurationMs;
        return false;
    }

    const bool emitted = tryEmitGap(symbol);
    // Longpath addition: the gap that just ended was inside a character
    // if no letter was emitted for it -- that is the element-gap cluster
    // the speed estimate needs (same +1 quantum as the tone edge).
    if (!m_letterEmitted && m_patternLength > 0) {
        m_timing.observeElementGap(m_durationMs + blockDurationMs);
    }
    m_tone = true;
    m_durationMs = blockDurationMs;
    m_letterEmitted = false;
    m_wordEmitted = false;
    return emitted;
}

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/MorseFsm.cs:85-96 [@8970f2d]
void CwMorseFsm::appendElement(double durationMs)
{
    const bool dah = m_timing.isDah(durationMs);
    const double target = dah ? 3.0 * m_timing.ditMs() : m_timing.ditMs();
    const double error = std::fabs(durationMs - target) / std::max(target, 1e-9);
    m_fitSum += clampTo(1.0 - error, 0.0, 1.0);
    ++m_fitCount;
    if (m_patternLength < static_cast<int>(m_pattern.size())) {
        m_pattern[static_cast<size_t>(m_patternLength++)] = dah ? '-' : '.';
    }
    m_timing.observeElement(durationMs);
}

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/MorseFsm.cs:98-130 [@8970f2d]
bool CwMorseFsm::tryEmitGap(CwDecodedSymbol& symbol)
{
    symbol = CwDecodedSymbol{};
    if (!m_letterEmitted
        && m_patternLength > 0
        && m_durationMs + m_quantumMs >= m_timing.letterGapThresholdMs()) {
        const std::string pattern(m_pattern.data(), static_cast<size_t>(m_patternLength));
        const std::string text = decodePattern(pattern);
        double fit = m_fitCount == 0 ? 0.0 : m_fitSum / m_fitCount;
        if (text == kUnknownPlaceholder) { fit *= 0.25; }
        symbol.text = text;
        symbol.confidence = static_cast<float>(clampTo(fit, 0.0, 1.0));
        m_patternLength = 0;
        m_fitSum = 0.0;
        m_fitCount = 0;
        m_letterEmitted = true;
        return true;
    }

    if (m_letterEmitted
        && !m_wordEmitted
        && m_durationMs + m_quantumMs >= m_timing.wordGapThresholdMs()) {
        m_wordEmitted = true;
        symbol.text = " ";
        symbol.confidence = 1.0f;
        return true;
    }

    return false;
}

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/MorseFsm.cs:132-143 [@8970f2d]
void CwMorseFsm::reset()
{
    m_patternLength = 0;
    m_initialized = false;
    m_tone = false;
    m_durationMs = 0.0;
    m_fitSum = 0.0;
    m_fitCount = 0;
    m_letterEmitted = false;
    m_wordEmitted = false;
    m_quantumMs = 0.0;
}

// ── CwDecoderCore ────────────────────────────────────────────────────────

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/CwDecoderCore.cs:17-22 [@8970f2d]
CwDecoderCore::CwDecoderCore(int sampleRateHz, double centerFrequencyHz)
    : m_sampleRateHz(sampleRateHz > 0 ? sampleRateHz : 48000)
    , m_detector(m_sampleRateHz, centerFrequencyHz)
    , m_fsm(m_timing)
{
}

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/CwDecoderCore.cs:30-52 [@8970f2d]
void CwDecoderCore::process(const float* samples, int count, const SymbolCallback& onDecoded)
{
    int offset = 0;
    while (offset < count) {
        const int room = static_cast<int>(m_block.size()) - m_blockFill;
        const int copy = std::min(room, count - offset);
        std::memcpy(m_block.data() + m_blockFill, samples + offset,
                    static_cast<size_t>(copy) * sizeof(float));
        m_blockFill += copy;
        offset += copy;
        if (m_blockFill != static_cast<int>(m_block.size())) { continue; }

        const double power = m_detector.detectPower(m_block.data());
        const bool tone = m_threshold.update(power);
        CwDecodedSymbol symbol;
        const double blockMs = 1000.0 * double(m_block.size()) / double(m_sampleRateHz);
        if (m_fsm.process(tone, blockMs, symbol)) {
            const double signalFit = clampTo((m_threshold.snrDb() - 2.0) / 10.0, 0.2, 1.0);
            symbol.confidence = static_cast<float>(symbol.confidence * signalFit);
            if (onDecoded) { onDecoded(symbol); }
        }
        m_blockFill = 0;
    }
}

// From Zeus station-engine Station.Engine.Hosting/CwDecoder/CwDecoderCore.cs:54-61 [@8970f2d]
void CwDecoderCore::reset()
{
    m_blockFill = 0;
    m_detector.reset();
    m_threshold.reset();
    m_timing.reset();
    m_fsm.reset();
}

} // namespace Longpath
