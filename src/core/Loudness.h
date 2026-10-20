#pragma once

// =================================================================
// src/core/Loudness.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Loudness the way the rest of the audio world measures it.
//
// ── Why this exists, when the analyser already had a level ───────────
//
// It had several, and that was the problem. "Speech level in dBFS",
// "crest factor", "how much the level wanders" — three numbers, none
// of them comparable with anything outside this program, and two of
// them measuring overlapping things. The house rule about two numbers
// for one role applies to measurements as much as to colours.
//
// So the wandering measure is gone and this is in its place:
//
//   LUFS   integrated loudness, ITU-R BS.1770 K-weighting with the
//          two-stage gate. One number for "how loud is this voice",
//          weighted the way hearing actually works rather than by
//          raw sample energy.
//
//   LRA    loudness range, EBU Tech 3342. How much the loudness
//          moves across the take, in LU. This is the standard answer
//          to "does this operator's level wander", and unlike a
//          home-made percentile spread it means the same thing in
//          every other tool that reports it.
//
// The point is not standards for their own sake. It is that these two
// numbers make a BEFORE and AFTER comparable: render the same take
// through a profile, measure again, and the difference in LUFS is
// what the profile actually bought in average level while the
// difference in LRA is what it cost in life. A crest factor cannot
// tell you that, and an ear cannot tell you it reliably twice.
//
// ── Where the filter comes from ──────────────────────────────────────
//
// K-weighting is two biquads: a high-frequency shelf and the RLB
// high-pass. BS.1770 tabulates their coefficients for 48 kHz only,
// and recomputing them for other rates is part of the standard's own
// method — so they are derived here from the continuous-time design
// parameters by bilinear transform rather than hard-coded, and the
// analyser then works at whatever rate the take was recorded at.
//
// The design values below are reproduced from secondary sources
// (the published 48 kHz table, and implementations that derive it);
// the ITU document itself is paywalled. They are checked in the test
// against the published 48 kHz coefficients, which is the strongest
// check available without the document: if the derivation is wrong,
// it cannot land on the table by accident.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

namespace Longpath::Loudness {

// ── The K-weighting design, as continuous-time parameters ────────────
//
// Stage 1, the shelf: a high-frequency boost standing in for the
// acoustic effect of a head in a sound field.
inline constexpr double kShelfFreqHz = 1681.974450955533;
inline constexpr double kShelfGainDb = 3.999843853973347;
inline constexpr double kShelfQ      = 0.7071752369554196;

// Stage 2, RLB: a high-pass that discards the low end hearing does
// not weight as loudness.
inline constexpr double kRlbFreqHz = 38.13547087602444;
inline constexpr double kRlbQ      = 0.5003270373238773;

// The gates, from BS.1770 and Tech 3342 respectively. Named rather
// than written inline because every one of them is a number somebody
// will want to check against the document.
inline constexpr double kAbsoluteGateLufs   = -70.0;
inline constexpr double kRelativeGateLu     = -10.0;   // BS.1770, for LUFS
inline constexpr double kRangeRelativeGateLu = -20.0;  // Tech 3342, for LRA
inline constexpr double kBlockSeconds       = 0.400;   // loudness block
inline constexpr double kBlockOverlap       = 0.75;
inline constexpr double kRangeBlockSeconds  = 3.000;   // Tech 3342 block
inline constexpr double kRangeLowPercentile  = 0.10;
inline constexpr double kRangeHighPercentile = 0.95;

struct Result {
    bool   valid{false};

    // Integrated loudness in LUFS. Silence, or a take with nothing
    // above the absolute gate, leaves this at the gate rather than
    // at an arbitrary floor.
    double integratedLufs{kAbsoluteGateLufs};

    // Loudness range in LU. Zero when there were too few gated
    // three-second blocks to form a range — which is the honest
    // answer for a short take, not a claim that the voice is steady.
    double rangeLu{0.0};
    bool   rangeValid{false};

    // Highest absolute sample, in dBFS.
    //
    // Sample peak, NOT true peak: true peak needs the signal
    // oversampled at least four times to catch inter-sample
    // overshoot, and this is a microphone take being judged for
    // shaping, not a master being checked against a delivery spec.
    // Named for what it is so nobody reads it as the other thing.
    double samplePeakDbFs{-120.0};
};

// Mono, any sample rate. Measures the whole buffer; the caller
// decides what to hand over (the analyser hands over the speech, the
// A/B hands over the same take rendered two ways).
Result measure(const float* samples, int frames, int sampleRate);

// ── Exposed for the test ─────────────────────────────────────────────
//
// The five normalised biquad coefficients of one K-weighting stage at
// a given rate, b0 b1 b2 a1 a2 with a0 divided out. Exposed so the
// derivation can be checked against BS.1770's published 48 kHz table
// instead of being taken on trust.
struct Biquad { double b0, b1, b2, a1, a2; };
Biquad shelfAt(double sampleRate);
Biquad rlbAt(double sampleRate);

} // namespace Longpath::Loudness
