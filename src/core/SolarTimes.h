#pragma once

// =================================================================
// src/core/SolarTimes.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Thetis has no solar model; the FreeDV station
// code carries locator maths but nothing about the sun.
//
// Sunrise, sunset and current sun elevation for a point on the Earth.
//
// This is operating information, not decoration. On the low bands the
// path opens and closes with the terminator, and the useful question is
// not "is it dark there" but "how far is that station from its own
// sunrise" — which is what the elevation angle answers.
//
// The implementation is the standard sunrise equation (mean solar noon,
// equation of centre, ecliptic longitude, declination, hour angle). It
// agrees with the NOAA solar calculator to within about two minutes,
// which is well inside the width of the grey line — a more expensive
// model would buy nothing here. The tests pin it against NOAA figures
// rather than against remembered almanac times.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include <QDateTime>

namespace Longpath {

struct SolarInfo {
    // Both invalid inside the polar day and the polar night — there is
    // no event to report, and a fabricated time would be worse than a
    // blank. Check the two flags below before reading them.
    QDateTime riseUtc;
    QDateTime setUtc;

    bool alwaysUp{false};      // midnight sun
    bool alwaysDown{false};    // polar night

    // Sun elevation above the horizon right now, in degrees. Negative
    // after sunset. This is the number that tells you how deep into
    // darkness the far end is.
    double elevationDeg{0.0};

    // Within the grey line: the sun is close enough to the horizon that
    // the D layer is thinning or rebuilding. Six degrees either side is
    // civil twilight, which is the band operators actually work.
    bool greyline{false};

    bool daylight() const { return elevationDeg > -0.833; }
};

// Solar circumstances at a position, for the instant given.
//
// Longitude is positive EAST, matching the Maidenhead helpers and the
// corrected cty.dat coordinates. (cty.dat itself stores positive west;
// CtyDatParser already flips it.)
SolarInfo solarInfo(const QDateTime& utc, double lat, double lonEast);

} // namespace Longpath
