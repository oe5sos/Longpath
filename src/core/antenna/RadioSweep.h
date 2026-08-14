#pragma once

// =================================================================
// src/core/antenna/RadioSweep.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Turn a sweep the radio measured into the same Sweep the file half of
// the antenna window analyses.
//
// ── Why ──────────────────────────────────────────────────────────────
//
// Asked for in one sentence, after a long day: "der sweep sollte nach
// beendigung genauso wie das beispiel aussehen."
//
// He is right, and the window had grown two halves that did not know
// about each other. Load a .s1p and you get the band shaded and named,
// SWR at the band start, middle and end, the usable span in kilohertz,
// the best match marked — and, from the phase, the resonance and how
// many centimetres of wire to add. Run the radio's own sweep and you
// got a bare line on a second chart with none of it.
//
// The measurement is the same shape. Only the source differs, and only
// in one respect: a directional coupler measures how much comes back,
// not when it comes back.
//
// ── What survives the trip and what does not ─────────────────────────
//
// From SWR alone, |Γ| = (SWR−1)/(SWR+1) exactly. Everything the window
// derives from magnitude therefore carries over untouched:
//
//     the SWR curve            the band edges and centre
//     SWR at any frequency     the usable span at a limit
//     the best match           the comparison against a previous sweep
//
// The angle of Γ is not measured and cannot be recovered. So NOT:
//
//     the resonance (X = 0)    the feed resistance in ohms
//     centimetres of wire      the learned length exponent
//
// The honest move is to carry the gap in the data rather than paper
// over it: Sweep::magnitudeOnly is set, and every reading that needs
// phase declines to answer. Filling the angle with a plausible zero
// would make the window print a resonance at every point and an
// impedance nobody measured — the same shape of lie as the flat SWR
// 1.00 curve this feature produced earlier today.
//
// What the operator gets from a radio sweep is therefore: where in the
// band the match is best, how wide the usable part is, and how it
// compares with last week. For a SOTA antenna that is most of the job.
// For the last piece — resonant HERE, add THIS much wire — a phase
// measurement is needed, which is what the .s1p half is for.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-14 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/antenna/Touchstone.h"
#include "core/SwrSweepController.h"

namespace NereusSDR::RadioSweep {

/// |Γ| from SWR. Exact, not an approximation: SWR = (1+|Γ|)/(1−|Γ|)
/// inverts to |Γ| = (SWR−1)/(SWR+1).
///
/// Returns 0 for any SWR at or below 1 — including the invalid 0 that
/// marks an unmeasured point, which the caller drops anyway.
double reflectionMagnitude(double swr);

/// Convert a finished radio sweep. Invalid points (swr <= 0) are
/// dropped rather than plotted as SWR 1.00 — a discarded measurement
/// must not become a perfect one, which is precisely how a dead
/// reverse channel drew a flat, beautiful, entirely fictional curve.
///
/// The result carries magnitudeOnly = true and a `source` naming the
/// band and the time, so it sits in the trace list beside file sweeps
/// and can be told apart from them.
Sweep fromResult(const SwrSweepResult& result);

} // namespace NereusSDR::RadioSweep
