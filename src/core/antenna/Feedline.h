#pragma once

// =================================================================
// src/core/antenna/Feedline.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Taking the coax back out of the measurement.
//
// ── Why this matters more than it looks ──────────────────────────────
//
// A vector analyser measures at its own port. Whatever sits between
// that port and the antenna is part of what it measured, and a length
// of coax does two quite different things to the answer:
//
//   LOSS makes the antenna look better than it is. A lossless line
//   changes the SWR not at all — this surprises people — but a lossy
//   one absorbs the reflected wave twice, once each way. Fifteen ohms
//   at the feed point is an SWR of 3.33; through forty metres of RG-58
//   on 40 m the meter reads 2.20, and through forty of RG-174 on 10 m
//   it reads 1.27. The antenna has not improved.
//
//   PHASE rotates the impedance, and this is the one that ruins a
//   trim. The reactance an analyser reports through a line is not the
//   antenna's reactance, so the frequency where it crosses zero is not
//   where the antenna is resonant. Two metres of RG-58 moved a 7.183
//   resonance to 7.247 in the model this was written against — 64 kHz,
//   which is about 20 cm of wire cut off the wrong end.
//
//   At three metres the crossing vanished from the band altogether. At
//   five, the only crossing left was a FALLING one — an artefact of the
//   line that looks exactly like a resonance to anything not checking
//   the direction.
//
// ── An idea that did not survive being checked ───────────────────────
//
// The obvious trick is to estimate the cable length from the sweep
// itself: a line rotates the phase of Γ linearly with frequency, so the
// slope should give the length. It does not work here. Over the few
// hundred kilohertz of a band sweep, the ANTENNA's own phase rotation
// near resonance dominates completely — in the check that killed the
// idea, a half-metre cable was estimated at minus 125 metres.
//
// The slope does track CHANGES in length correctly, so the method is
// not wrong, merely useless without a way to separate the antenna from
// the line. The operator knows how long their cable is. Asking them is
// the honest interface, so that is what this offers.
//
// ── The loss model is approximate and says so ────────────────────────
//
// Matched loss scales roughly as the square root of frequency, which is
// the conductor loss dominating below VHF. Dielectric loss (linear in
// f) is ignored. The catalogue figures are nominal: real cable varies
// by manufacturer, by age, and by how much water has got into it. They
// are a starting point to be edited, not a specification.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/antenna/Touchstone.h"

#include <QString>
#include <QVector>

namespace Longpath::Feedline {

// One coax type. `lossDb100m` is matched loss per 100 m at `refHz`.
struct Cable {
    QString name;
    double  velocityFactor{0.66};
    double  lossDb100m{4.5};
    double  refHz{10e6};
};

// A short list of what people actually carry. Nominal figures — see
// the header note. Index 0 is "no cable / calibrated at the antenna",
// which is the correct answer for anyone who calibrated at the far end
// and the one that should cost nothing to choose.
const QVector<Cable>& catalogue();

// Attenuation in nepers per metre at `f`, from a dB/100 m figure at a
// reference frequency, scaled as sqrt(f).
double alphaNpPerM(double lossDb100m, double refHz, double f);

struct Result {
    Sweep   sweep;
    // Set when removing the stated loss pushed |Γ| above 1 — which is
    // physically impossible and means the loss figure is too large, or
    // the cable is shorter than entered. The sweep is still returned,
    // clamped, because a plausible-looking curve with a warning beats
    // an empty window.
    bool    lossTooHigh{false};
    QString note;
};

// Move the reference plane from the analyser to the far end of a length
// of line: what the antenna is actually doing.
//
// `lengthM` of zero returns the sweep untouched, which is the case for
// anyone who calibrated at the antenna end.
Result deEmbed(const Sweep& measured, double lengthM, const Cable& cable);

// The inverse — put a line back in. Only used by the tests, where being
// able to synthesise "what the analyser would have seen" is what makes
// the round trip checkable.
Sweep embed(const Sweep& atAntenna, double lengthM, const Cable& cable);

} // namespace Longpath::Feedline
