// =================================================================
// src/core/antenna/AntennaTrim.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See AntennaTrim.h for where the model stops
// being true and why shortening is halved.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/antenna/AntennaTrim.h"

#include <cmath>

namespace NereusSDR::AntennaTrim {
namespace {

// Beyond this the linear scaling is a direction rather than a figure.
constexpr double kCautionPercent = 10.0;

// Speed of light. Written out because 3e8 is wrong in the third digit
// and a 20 m wire deserves better than a 10 cm error from a constant.
constexpr double kC = 299'792'458.0;

} // namespace

QString kindName(Kind k)
{
    switch (k) {
    case Kind::Dipole:           return QStringLiteral("Dipole");
    case Kind::EndFedHalfWave:   return QStringLiteral("End-fed half-wave");
    case Kind::VerticalRadiator: return QStringLiteral("Vertical radiator");
    case Kind::Loop:             return QStringLiteral("Loop");
    }
    return QStringLiteral("Antenna");
}

QString kindWhere(Kind k)
{
    switch (k) {
    case Kind::Dipole:
        return QStringLiteral("on each leg");
    case Kind::EndFedHalfWave:
        return QStringLiteral("at the far end");
    case Kind::VerticalRadiator:
        return QStringLiteral("on the radiator");
    case Kind::Loop:
        return QStringLiteral("on the loop");
    }
    return QString{};
}

double halfWaveEstimateM(double freqHz, double velocityFactor)
{
    if (freqHz <= 0.0) { return 0.0; }
    const double vf = (velocityFactor > 0.0 && velocityFactor <= 1.0)
                          ? velocityFactor : 0.95;
    return (kC / freqHz) * 0.5 * vf;
}

Trim compute(Kind kind, double measuredHz, double targetHz,
             double currentTotalM, double exponent)
{
    Trim t;
    if (measuredHz <= 0.0 || targetHz <= 0.0) { return t; }

    t.valid      = true;
    t.measuredHz = measuredHz;
    t.targetHz   = targetHz;

    // f ∝ L^-k, so the length ratio is the frequency ratio to the power
    // 1/k. The default k of 1 is the textbook rule; anything else came
    // from watching this antenna actually move.
    const double k = (exponent > 0.05 && exponent < 20.0) ? exponent : 1.0;
    const double ratio = measuredHz / targetHz;
    // Exactly 1 takes the plain path so the ordinary case is unchanged
    // to the last bit — pow(x, 1.0) is very nearly x, and "very nearly"
    // is how a test that used to pass starts failing by 1e-16.
    const double factor = (k == 1.0) ? ratio : std::pow(ratio, 1.0 / k);
    t.percentChange = (factor - 1.0) * 100.0;
    t.lengthen      = factor > 1.0;

    if (std::abs(t.percentChange) > kCautionPercent) {
        t.caution = QStringLiteral(
            "That is a %1 %2 change. The simple length-scaling rule is "
            "only reliable for a few percent, so treat this as a "
            "direction and re-measure after the first step.")
                .arg(std::abs(t.percentChange), 0, 'f', 0)
                .arg(QStringLiteral("percent"));
    }

    if (currentTotalM > 0.0) {
        t.totalChangeM = currentTotalM * (factor - 1.0);
        // Everywhere except a centre-fed dipole, the whole change
        // happens at one place.
        const double share = (kind == Kind::Dipole) ? 2.0 : 1.0;
        t.perElementM = t.totalChangeM / share;

        t.firstStepM = t.perElementM;
        if (!t.lengthen) {
            // Cutting is one-way. Take half now, measure, take the rest
            // if the antenna agrees.
            t.firstStepM = t.perElementM * 0.5;
            t.halved = true;
        }
    }
    return t;
}

QString instruction(const Trim& t, Kind kind)
{
    if (!t.valid) { return QStringLiteral("No measurement yet."); }

    // Within a tenth of a percent there is nothing to do, and telling
    // somebody to move 2 mm of wire is telling them to introduce an
    // error.
    if (std::abs(t.percentChange) < 0.1) {
        return QStringLiteral("Already resonant where you want it — "
                              "leave it alone.");
    }

    const QString verb = t.lengthen ? QStringLiteral("Lengthen")
                                    : QStringLiteral("Shorten");

    if (t.totalChangeM == 0.0) {
        // No length given, so only the proportion is knowable. Still
        // worth saying: the operator can measure their own wire.
        return QStringLiteral("%1 the antenna by %2 %. Enter the current "
                              "length to get it in centimetres.")
                   .arg(verb).arg(std::abs(t.percentChange), 0, 'f', 2);
    }

    const double cm = std::abs(t.firstStepM) * 100.0;
    QString s = QStringLiteral("%1 by %2 cm %3.")
                    .arg(verb)
                    .arg(cm, 0, 'f', 1)
                    .arg(kindWhere(kind));

    if (t.halved) {
        s += QStringLiteral(" That is half of the %1 cm the maths asks "
                            "for — cut it in two passes, because wire "
                            "does not grow back.")
                 .arg(std::abs(t.perElementM) * 100.0, 0, 'f', 1);
    }
    if (!t.caution.isEmpty()) {
        s += QLatin1Char(' ') + t.caution;
    }
    return s;
}

} // namespace NereusSDR::AntennaTrim
