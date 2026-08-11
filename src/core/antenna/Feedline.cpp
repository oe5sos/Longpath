// =================================================================
// src/core/antenna/Feedline.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See Feedline.h for what a length of coax does
// to a measurement, and for the estimation trick that did not work.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/antenna/Feedline.h"

#include <cmath>
#include <complex>

namespace NereusSDR::Feedline {
namespace {

constexpr double kC = 299'792'458.0;
// Nepers per decibel. 20/ln(10).
constexpr double kDbPerNeper = 8.685889638065035;

// Applying the line, in whichever direction. `sign` is -1 travelling
// from the antenna towards the analyser (what the analyser sees) and
// +1 travelling back (what the antenna is doing).
Sweep applyLine(const Sweep& in, double lengthM, const Cable& cable,
                double sign, bool* clamped)
{
    Sweep out = in;
    if (clamped) { *clamped = false; }
    if (lengthM <= 0.0 || in.isEmpty()) { return out; }

    const double vf = (cable.velocityFactor > 0.05
                       && cable.velocityFactor <= 1.0)
                          ? cable.velocityFactor : 0.66;

    for (SweepPoint& p : out.points) {
        if (p.freqHz <= 0.0) { continue; }
        const double alpha = alphaNpPerM(cable.lossDb100m, cable.refHz,
                                         p.freqHz);
        const double beta  = 2.0 * M_PI * p.freqHz / (kC * vf);
        // Γ travels the line twice — out and back — hence the 2.
        const std::complex<double> exponent =
            sign * 2.0 * lengthM * std::complex<double>(alpha, beta);
        // `out` is already a copy of `in`, so this reads the measured
        // value and writes the corrected one in one step. An earlier
        // version indexed back into `in` through pointer arithmetic on
        // the loop variable, which worked and was a trap waiting for
        // whoever changed the container type.
        p.gamma *= std::exp(exponent);

        // Removing loss makes |Γ| larger. Past 1 it is not a reflection
        // any more, it is a claim that the antenna returned more than
        // it was given — which means the loss figure is wrong.
        const double mag = std::abs(p.gamma);
        if (mag >= 1.0) {
            if (clamped) { *clamped = true; }
            p.gamma *= (0.9999 / mag);
        }
    }
    return out;
}

} // namespace

const QVector<Cable>& catalogue()
{
    // Nominal figures at 10 MHz. Real cable differs by make, by age and
    // by how much water has found its way in; these are a starting
    // point the operator is expected to edit.
    static const QVector<Cable> c = {
        {QStringLiteral("None — calibrated at the antenna"), 1.00, 0.0, 10e6},
        {QStringLiteral("RG-58"),   0.66,  4.6, 10e6},
        {QStringLiteral("RG-58 foam"), 0.79, 3.9, 10e6},
        {QStringLiteral("RG-8X"),   0.82,  3.0, 10e6},
        {QStringLiteral("RG-174"),  0.66,  9.8, 10e6},
        {QStringLiteral("RG-316"),  0.70, 10.0, 10e6},
        {QStringLiteral("RG-213"),  0.66,  2.0, 10e6},
        {QStringLiteral("LMR-400"), 0.85,  1.3, 10e6},
        {QStringLiteral("Custom"),  0.66,  4.5, 10e6},
    };
    return c;
}

double alphaNpPerM(double lossDb100m, double refHz, double f)
{
    if (lossDb100m <= 0.0 || refHz <= 0.0 || f <= 0.0) { return 0.0; }
    // Conductor loss dominates below VHF and goes as sqrt(f). The
    // dielectric term, linear in f, is left out — it matters at
    // microwave and not on an HF antenna.
    const double db100 = lossDb100m * std::sqrt(f / refHz);
    return (db100 / 100.0) / kDbPerNeper;
}

Result deEmbed(const Sweep& measured, double lengthM, const Cable& cable)
{
    Result r;
    bool clamped = false;
    r.sweep = applyLine(measured, lengthM, cable, +1.0, &clamped);
    r.lossTooHigh = clamped;
    if (clamped) {
        r.note = QStringLiteral(
            "Removing %1 m of %2 pushed the reflection above 1, which "
            "cannot happen. The cable loss figure is too high, or the "
            "cable is shorter than entered.")
                .arg(lengthM, 0, 'f', 1).arg(cable.name);
        r.sweep.note = r.note;
    }
    return r;
}

Sweep embed(const Sweep& atAntenna, double lengthM, const Cable& cable)
{
    return applyLine(atAntenna, lengthM, cable, -1.0, nullptr);
}

} // namespace NereusSDR::Feedline
