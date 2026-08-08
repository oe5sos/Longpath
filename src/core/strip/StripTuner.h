#pragma once

// =================================================================
// src/core/strip/StripTuner.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Turn one voice measurement into a whole chain setting.
//
// The voice check measures; the strip changes the sound. Until now they
// were two windows that did not speak, and the analyser's advice went
// into WDSP's equaliser — a second equaliser, not the one the operator
// can see a curve for. Two equalisers set from two places is how an
// operator ends up with a sound nobody, including them, can explain.
//
// So this maps the measurement onto the strip, and every setting it
// chooses comes from a number that was measured rather than from a
// taste:
//
//   high-pass       from how much energy sits below the speech band
//   mains notches   from the hum frequency that was actually found,
//                   and only when the hum is loud enough to matter
//   tone            from the ten-band suggestion, folded onto the
//                   three tone controls the panel exposes
//   gate threshold  from the noise floor measured in the pauses,
//                   placed between the floor and the speech
//   de-esser        from the measured sibilance, and left off when
//                   there is none
//   compressor      from the crest factor — how much louder the peaks
//                   are than the average is exactly the question a
//                   compressor answers
//
// What it will not do:
//
//   Boost. The EQ suggestion only ever cuts (see VoiceAnalyzer), and
//   nothing here adds gain to a band. A chain that reaches a target by
//   boosting amplifies whatever noise was in that band.
//
//   Switch the strip on. Same rule as the presets: the operator decides
//   when their transmit audio changes.
//
//   Act on a measurement that failed. A clipped or too-short recording
//   produces no changes and says so.
//
// Every change is reported in a sentence. An automatic setup that
// cannot explain itself is one the operator has to either trust
// completely or discard completely, and neither is what they want.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/VoiceAnalyzer.h"

#include <QString>
#include <QStringList>

namespace NereusSDR {

class StripChain;

namespace StripTuner {

// Applied to the chain; returns one line per change, worst problem
// first, or a single line explaining why nothing was done.
struct Result {
    bool        changed{false};
    QStringList notes;
};

Result applyAnalysis(const VoiceAnalysis& a, StripChain& chain);

// ── The individual decisions, exposed so they can be tested ──────────
//
// Each is a pure function of the measurement. Testing them directly is
// worth more than testing applyAnalysis, because these are where a
// wrong sign or a swapped bound hides.

// Where to put the high-pass, in Hz, given the measured band levels.
// Never below 60 — there is nothing under it that an SSB transmitter
// will send — and never above 200, which starts removing the voice.
double highPassHz(const VoiceAnalysis& a);

// The gate threshold, in dBFS: above the noise floor, below the
// speech, and refusing to place itself when the two are too close to
// tell apart. Returns the speech level itself as a sentinel when the
// separation is under kMinGateSeparationDb, which the caller reads as
// "do not enable the gate".
double gateThresholdDbFs(const VoiceAnalysis& a);
inline constexpr double kMinGateSeparationDb = 12.0;

// Compressor ratio from the crest factor. A voice with 20 dB of peaks
// over its average needs more than one with 8 dB, and that is the
// entire justification for the number.
double compressorRatio(const VoiceAnalysis& a);

// Is the hum worth notching? Below this separation the notches are a
// repair; above it they cost phase for nothing.
inline constexpr double kHumWorthNotchingDb = 25.0;
bool humWorthNotching(const VoiceAnalysis& a);

} // namespace StripTuner
} // namespace NereusSDR
