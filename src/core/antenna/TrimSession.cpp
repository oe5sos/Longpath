// =================================================================
// src/core/antenna/TrimSession.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See TrimSession.h for why the textbook exponent
// is not good enough and when this refuses to replace it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/antenna/TrimSession.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

void TrimSession::record(double lengthM, double resonanceHz,
                         const QString& label)
{
    if (resonanceHz <= 0.0) { return; }
    Observation o;
    o.lengthM     = std::max(0.0, lengthM);
    o.resonanceHz = resonanceHz;
    o.when        = QDateTime::currentDateTimeUtc();
    o.label       = label;
    m_obs.append(o);
}

void TrimSession::updateLastLength(double lengthM)
{
    if (m_obs.isEmpty() || lengthM < 0.0) { return; }
    m_obs.last().lengthM = lengthM;
}

void TrimSession::updateLastResonance(double resonanceHz)
{
    if (m_obs.isEmpty() || resonanceHz <= 0.0) { return; }
    m_obs.last().resonanceHz = resonanceHz;
}

TrimSession::Learned TrimSession::learned() const
{
    Learned out;
    if (m_obs.size() < 2) {
        out.note = QStringLiteral(
            "Measure again after changing the wire and this will work "
            "out how much your antenna really moves per centimetre.");
        return out;
    }

    const Observation& a = m_obs.at(m_obs.size() - 2);
    const Observation& b = m_obs.at(m_obs.size() - 1);

    if (a.lengthM <= 0.0 || b.lengthM <= 0.0) {
        out.note = QStringLiteral(
            "The wire length was not entered for both measurements, so "
            "there is nothing to compare them against.");
        return out;
    }

    out.lengthChangeM = b.lengthM - a.lengthM;
    out.movedHz       = b.resonanceHz - a.resonanceHz;
    // What the textbook rule promised for the length actually applied.
    out.predictedHz   = a.resonanceHz * (a.lengthM / b.lengthM)
                        - a.resonanceHz;

    const double fraction = std::abs(out.lengthChangeM) / a.lengthM;
    if (fraction < kMinLengthChangeFraction) {
        out.note = QStringLiteral(
            "The length barely changed between these two, so the "
            "movement is inside the noise. A step of at least %1 cm "
            "would say something.")
                .arg(a.lengthM * kMinLengthChangeFraction * 100.0,
                     0, 'f', 0);
        return out;
    }
    if (std::abs(out.movedHz) < kMinFrequencyChangeHz) {
        out.note = QStringLiteral(
            "The resonance moved only %1 kHz for %2 cm of wire. That is "
            "close enough to nothing that something else is going on — "
            "check that the change was actually made, and on the right "
            "part.")
                .arg(std::abs(out.movedHz) / 1000.0, 0, 'f', 1)
                .arg(std::abs(out.lengthChangeM) * 100.0, 0, 'f', 1);
        return out;
    }

    // Lengthening lowers the resonance. If it rose, the length is not
    // what changed — a moved counterpoise, a different height, a
    // connector that was not tight. An exponent from that would be
    // negative and feeding it forward would be worse than not learning
    // at all.
    const bool longer = out.lengthChangeM > 0.0;
    const bool lower  = out.movedHz < 0.0;
    if (longer != lower) {
        out.note = QStringLiteral(
            "The wire got %1 but the resonance went %2, which is "
            "backwards. Something other than the length changed between "
            "these two measurements — height, the counterpoise, or what "
            "is near the antenna.")
                .arg(longer ? QStringLiteral("longer")
                            : QStringLiteral("shorter"))
                .arg(lower  ? QStringLiteral("down")
                            : QStringLiteral("up"));
        return out;
    }

    //  f ∝ L^-k   →   k = ln(f₁/f₂) / ln(L₂/L₁)
    const double k = std::log(a.resonanceHz / b.resonanceHz)
                     / std::log(b.lengthM / a.lengthM);

    if (!std::isfinite(k) || k < kMinExponent || k > kMaxExponent) {
        out.note = QStringLiteral(
            "These two measurements imply the antenna responds in a way "
            "no antenna does. Using the textbook rule instead.");
        return out;
    }

    out.valid    = true;
    out.exponent = k;

    // The sentence that is the whole point of the class.
    const double movedKHz     = std::abs(out.movedHz) / 1000.0;
    const double predictedKHz = std::abs(out.predictedHz) / 1000.0;
    if (k < 0.85) {
        out.note = QStringLiteral(
            "%1 cm moved the resonance %2 kHz where the textbook rule "
            "expected %3. Your antenna responds less than the maths "
            "assumes — the next step is scaled up to match.")
                .arg(std::abs(out.lengthChangeM) * 100.0, 0, 'f', 1)
                .arg(movedKHz, 0, 'f', 1)
                .arg(predictedKHz, 0, 'f', 1);
    } else if (k > 1.15) {
        out.note = QStringLiteral(
            "%1 cm moved the resonance %2 kHz where the textbook rule "
            "expected %3. Your antenna responds more than the maths "
            "assumes — the next step is scaled down to match.")
                .arg(std::abs(out.lengthChangeM) * 100.0, 0, 'f', 1)
                .arg(movedKHz, 0, 'f', 1)
                .arg(predictedKHz, 0, 'f', 1);
    } else {
        out.note = QStringLiteral(
            "%1 cm moved the resonance %2 kHz, which is what the "
            "textbook rule expected. The antenna is behaving.")
                .arg(std::abs(out.lengthChangeM) * 100.0, 0, 'f', 1)
                .arg(movedKHz, 0, 'f', 1);
    }
    return out;
}

AntennaTrim::Trim TrimSession::recommend(AntennaTrim::Kind kind,
                                         double targetHz,
                                         double currentLengthM) const
{
    if (m_obs.isEmpty()) { return {}; }
    const double measured = m_obs.last().resonanceHz;
    const Learned l = learned();
    return AntennaTrim::compute(kind, measured, targetHz, currentLengthM,
                                l.valid ? l.exponent : 1.0);
}

} // namespace NereusSDR
