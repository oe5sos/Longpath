#pragma once

// =================================================================
// src/core/antenna/AntennaTrim.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// How many centimetres, and which way.
//
// ── The question this answers ────────────────────────────────────────
//
// On a summit, "the antenna is resonant at 7.183" is not an answer, it
// is a homework problem. The answer is "add 22 cm to each leg", and
// getting the SIGN of it wrong costs a walk back down.
//
// ── The arithmetic, and where it stops being true ────────────────────
//
// Resonant length scales inversely with resonant frequency:
//
//     L_new = L_old × f_measured / f_target
//
// Measured high means the wire is too short: the factor is greater than
// one and the wire grows. Measured low means too long.
//
// This is a first-order model and it is good to a few tenths of a
// percent over corrections of a few percent, which covers nearly every
// real trim. It degrades for large changes because the length/diameter
// ratio, the end effect and the height in wavelengths all shift as the
// antenna changes. Past ten percent the number here is a direction, not
// a measurement, and `Trim::caution` says so rather than printing four
// significant figures at somebody standing on a rock.
//
// ── Two things that are not in the formula ───────────────────────────
//
// Height above ground moves the resonance, sometimes by more than the
// trim being attempted. An antenna adjusted at waist height in a garden
// is a different antenna at ten metres on a summit. The tool cannot
// know this; the advice says it.
//
// An end-fed half-wave on a 49:1 does not move all its bands by the
// same percentage — the transformer and the counterpoise have their own
// frequency behaviour on top of the wire's. So the trim is computed for
// one band at a time and never extrapolated to the others.
//
// ── Cut half ─────────────────────────────────────────────────────────
//
// Every recommendation to SHORTEN is halved before it is shown.
// The first measurement usually carries a systematic offset — a cable
// not de-embedded, a different height, a counterpoise lying differently
// — and wire cannot be un-cut. Two passes cost five minutes. One pass
// in the wrong direction costs the activation.
//
// Lengthening is not halved: adding wire is reversible.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QString>

namespace NereusSDR::AntennaTrim {

// How a change in total wire length is shared out, and what the
// operator physically does about it.
enum class Kind {
    // Two legs, fed in the middle. Half the change per side, and doing
    // it to one side only skews the pattern and unbalances the feed.
    Dipole = 0,
    // One wire, one end. All the change at the far end.
    EndFedHalfWave,
    // The radiator sets the resonance. The radials set the feed
    // resistance — a different problem with a different fix.
    VerticalRadiator,
    // Circumference. One place to adjust.
    Loop,
};

QString kindName(Kind k);

// Where the change goes, in plain words: "per leg", "at the far end".
QString kindWhere(Kind k);

struct Trim {
    bool   valid{false};

    double measuredHz{0.0};
    double targetHz{0.0};

    // Positive: the wire must get longer. Negative: shorter.
    double percentChange{0.0};

    // Total change over the whole antenna, metres. Zero when the
    // current length was not given — the percentage still holds.
    double totalChangeM{0.0};
    // What to change at each adjustment point: half of the total for a
    // dipole, all of it otherwise.
    double perElementM{0.0};
    // What to actually do first. Same as perElementM when lengthening;
    // half of it when shortening, because wire does not grow back.
    double firstStepM{0.0};
    bool   halved{false};

    bool   lengthen{false};
    // Set when the correction is large enough that the linear model is
    // no longer trustworthy.
    QString caution;
};

// Compute the change. `currentTotalM` may be 0 when the length is
// unknown — the percentage is still returned and the metre figures are
// left at zero rather than invented.
//
// ── The exponent ─────────────────────────────────────────────────────
//
// The textbook rule is f ∝ 1/L, which is `exponent` = 1 and the default.
// It assumes an antenna in free space, and a real one is not: ground,
// nearby wire and the mast all change how much the resonance moves for
// a given cut.
//
// TrimSession measures the real exponent from two sweeps either side of
// a known change, and it is routinely nowhere near 1 — in the worked
// case it came out at 0.5, which doubles the wire needed. Passing it
// here is what turns a textbook answer into an answer about THIS
// antenna in THIS spot.
//
// Exactly 1 short-circuits to a plain multiply rather than pow(), so
// the common case stays bit-for-bit what it always was.
Trim compute(Kind kind, double measuredHz, double targetHz,
             double currentTotalM, double exponent = 1.0);

// One sentence for the operator: the action, the amount, and where.
QString instruction(const Trim& t, Kind kind);

// A rough half-wave length for a frequency, metres, for insulated wire.
// Offered only as a starting point when the operator does not know what
// they have — labelled as an estimate wherever it is shown, because the
// velocity factor of the wire they actually own is not known here.
double halfWaveEstimateM(double freqHz, double velocityFactor = 0.95);

} // namespace NereusSDR::AntennaTrim
