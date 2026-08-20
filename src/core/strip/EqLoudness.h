#pragma once

// =================================================================
// src/core/strip/EqLoudness.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Louder always sounds better, and that is the problem.
//
// Every judgement an operator makes about an equaliser is a comparison:
// with and without, this curve or that one. If one side of the
// comparison is louder, it wins — reliably, in every listening test
// ever run, and regardless of whether it is actually better. A boost of
// three decibels somewhere useful and a boost of three decibels
// somewhere useless sound equally like an improvement, because both
// made the signal louder.
//
// So the equaliser measures how much it is adding overall and takes the
// same amount off at the output. Switching it in and out then changes
// the SHAPE and nothing else, which is the only condition under which
// the operator's ear is being asked a question it can answer.
//
// It also keeps the compressor's job roughly constant while a curve is
// being shaped. Not exactly — a compressor responds to peaks and to the
// whole spectrum, not to a loudness-weighted average — so this is a
// help, not a guarantee. The real defence of the low end against the
// compressor is the high-pass, which is a separate control with its own
// reasons.
//
// ── What is being averaged, and why not the measured voice ───────────
//
// The average is weighted twice: by a long-term average speech
// spectrum, because that is where a voice actually has energy, and by
// the K-weighting of ITU-R BS.1770, because that is the standard answer
// to "how loud is this" and the one every loudness meter in the world
// uses.
//
// A-weighting was tried first and is wrong for this. It was designed
// for very quiet sounds and discounts 100 Hz by nineteen decibels, so a
// six-decibel bass lift came out as a third of a decibel of added
// loudness — audibly false. K-weighting discounts the same point by
// about one decibel, and the same lift reads as 1.2 dB, which is
// roughly what it sounds like. The A-weighting curve is kept because it
// is worth having and is tested, but nothing here uses it.
//
// Deliberately NOT the operator's measured spectrum, even though one is
// available. A makeup that tracked the live measurement would move
// while they spoke — that is a compressor, not a trim, and it would
// make the A/B comparison it exists to enable impossible for the
// opposite reason.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/ClientEq.h"

namespace Longpath::EqLoudness {

// A-weighting at one frequency, in dB, per IEC 61672. 0 dB at 1 kHz by
// definition. Not used by the makeup — see the note above — but kept
// and tested, because "how loud is this in a quiet room" is a question
// that comes up and getting it right twice is cheaper than getting it
// wrong once.
double aWeightingDb(double hz);

// K-weighting at one frequency, in dB, per ITU-R BS.1770.
//
// Evaluated from the analog prototype the standard's digital
// coefficients are derived from — a high shelf at 1682 Hz, +4 dB,
// Q 0.7072, and a high-pass at 38.1 Hz, Q 0.5003 — rather than from the
// published 48 kHz biquads, so that it is correct at whatever rate the
// strip happens to be running. Verified against those biquads: within
// 0.05 dB below 1 kHz and 0.42 dB at 2 kHz, the difference being the
// bilinear frequency warping the digital form carries and the analog
// one does not.
//
// Not normalised to 0 dB at 1 kHz — BS.1770 does not normalise it
// either, and only the shape matters to a weighted average.
double kWeightingDb(double hz);

// The long-term average spectrum of speech, in dB relative to its own
// peak. An approximation of the published shape, interpolated on a log
// axis; it does not need to be anyone's particular voice, it needs to
// say that a voice has far more energy at 400 Hz than at 4 kHz.
double speechWeightDb(double hz);

// How much louder the equaliser makes a voice, in dB.
//
// Positive means the curve adds level. Averaged in the power domain
// across the speech range, because loudness follows power and averaging
// decibels would let a deep narrow notch cancel a broad gentle lift.
double addedLoudnessDb(const ClientEq& eq);

// What to take off at the output: the negative of the above, clamped.
//
// Clamped because ClientEq's master gain runs from -inf to +12 dB and
// because a curve wanting more than twelve decibels of correction is a
// curve with something wrong with it — silently applying a huge trim
// would hide that rather than let the operator see it.
double makeupDb(const ClientEq& eq);

inline constexpr double kMaxMakeupDb = 12.0;

// Compute and apply. The one call the window makes.
void apply(ClientEq& eq);

} // namespace Longpath::EqLoudness
