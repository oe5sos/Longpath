// =================================================================
// src/core/BeamHeading.cpp  (Longpath)
// =================================================================
//
// Longpath-original. See BeamHeading.h for why the end stop is the
// part that matters.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-09 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/BeamHeading.h"

#include <algorithm>

#include <cmath>

namespace Longpath::BeamHeading {

double wrap360(double deg)
{
    double d = std::fmod(deg, 360.0);
    if (d < 0.0) { d += 360.0; }
    return d;
}

double longPath(double shortPathDeg)
{
    return wrap360(shortPathDeg + 180.0);
}

GreatCircle greatCircle(double lat1, double lon1, double lat2, double lon2)
{
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kEarthKm = 6371.0;
    const double p1 = lat1 * kPi / 180.0;
    const double p2 = lat2 * kPi / 180.0;
    const double dl = (lon2 - lon1) * kPi / 180.0;
    const double a = std::sin((p2 - p1) / 2) * std::sin((p2 - p1) / 2)
                   + std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
    GreatCircle g;
    g.km = 2.0 * kEarthKm * std::asin(std::min(1.0, std::sqrt(a)));
    const double y = std::sin(dl) * std::cos(p2);
    const double x = std::cos(p1) * std::sin(p2) - std::sin(p1) * std::cos(p2) * std::cos(dl);
    g.bearingDeg = wrap360(std::atan2(y, x) * 180.0 / kPi);
    return g;
}

Move plan(double fromDeg, double toDeg, Stop stop)
{
    Move m;
    m.targetDeg = wrap360(toDeg);
    const double from = wrap360(fromDeg);

    if (stop == Stop::None) {
        // Free to rotate: take whichever direction is shorter. The
        // signed difference wrapped into -180..180 IS that direction.
        double d = m.targetDeg - from;
        if (d > 180.0)  { d -= 360.0; }
        if (d < -180.0) { d += 360.0; }
        m.travelDeg = d;
        m.reachable = true;
        return m;
    }

    // With a stop, the rotor lives on one continuous span and cannot
    // cross the boundary. Rotate both angles so the stop sits at 0, and
    // then the only route is the plain difference — no wrapping, because
    // wrapping is exactly what the stop forbids.
    const double stopAt = (stop == Stop::North) ? 0.0 : 180.0;
    const double a = wrap360(from      - stopAt);
    const double b = wrap360(m.targetDeg - stopAt);

    m.travelDeg = b - a;
    m.reachable = true;

    // A stop does not usually forbid a heading, only a route — a rotor
    // with a north stop still reaches 350° and 10°, just never directly
    // between them. So nothing here is unreachable; it is only
    // sometimes a very long way round, and the travel says which.
    if (std::abs(m.travelDeg) > 270.0) {
        m.note = QStringLiteral(
            "The rotor cannot turn through its end stop, so this is the "
            "long way round.");
    }
    return m;
}

QString advice(const Move& m)
{
    if (!m.reachable) { return m.note; }

    const double t = std::abs(m.travelDeg);
    if (t < 2.0) {
        return QStringLiteral("Already pointing there.");
    }

    QString s = QStringLiteral("%1° %2")
                    .arg(t, 0, 'f', 0)
                    .arg(m.travelDeg >= 0.0 ? QStringLiteral("clockwise")
                                            : QStringLiteral("anticlockwise"));
    if (!m.note.isEmpty()) { s += QStringLiteral(" — ") + m.note; }
    else if (t > 180.0) {
        s += QStringLiteral(" — a long move; give it time before you "
                            "call.");
    }
    return s;
}

} // namespace Longpath::BeamHeading
