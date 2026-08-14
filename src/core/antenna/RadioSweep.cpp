// =================================================================
// src/core/antenna/RadioSweep.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See RadioSweep.h for what survives the trip from
// a directional coupler to a Sweep, and what cannot.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-14 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/antenna/RadioSweep.h"

#include "models/Band.h"

namespace NereusSDR::RadioSweep {

double reflectionMagnitude(double swr)
{
    if (swr <= 1.0) { return 0.0; }
    const double g = (swr - 1.0) / (swr + 1.0);
    // SWR is capped at 99 upstream, which is |Γ| = 0.98. The clamp is
    // for anything that arrives from elsewhere: |Γ| > 1 is a reflection
    // larger than the incident wave, and downstream arithmetic divides
    // by (1 − |Γ|).
    return g > 0.999 ? 0.999 : g;
}

Sweep fromResult(const SwrSweepResult& result)
{
    Sweep out;
    out.referenceOhms = 50.0;
    out.magnitudeOnly = true;
    out.source = QStringLiteral("%1 · %2 (Funkgerät)")
                     .arg(bandLabel(result.band),
                          result.startedAt.toString(QStringLiteral("HH:mm")));

    out.points.reserve(result.points.size());
    for (const SwrSweepPoint& p : result.points) {
        // swr <= 0 marks a point the bridge could not measure. Plotting
        // it would put it at SWR 1.00 — a discarded measurement
        // becoming a perfect one, which is exactly the fiction that
        // wasted a day. Drop it; the curve simply has a gap, and a gap
        // is honest.
        if (p.swr <= 0.0) { continue; }
        SweepPoint sp;
        sp.freqHz = static_cast<double>(p.freqHz);
        // Real and negative by convention: the magnitude is what was
        // measured, and putting it on the negative real axis is the
        // least misleading placeholder — it corresponds to a purely
        // resistive load BELOW 50 Ω, and magnitudeOnly stops anything
        // reading meaning into that choice.
        sp.gamma = {-reflectionMagnitude(p.swr), 0.0};
        out.points.append(sp);
    }

    if (out.points.isEmpty()) {
        out.note = QStringLiteral(
            "Der Sweep hat keinen einzigen gültigen Messpunkt geliefert.");
    } else if (out.points.size() < result.points.size()) {
        out.note = QStringLiteral(
            "%1 von %2 Punkten konnten nicht gemessen werden und fehlen "
            "in der Kurve.")
                .arg(result.points.size() - out.points.size())
                .arg(result.points.size());
    }
    return out;
}

} // namespace NereusSDR::RadioSweep
