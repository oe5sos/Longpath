// =================================================================
// src/core/SolarTimes.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — see SolarTimes.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "SolarTimes.h"

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

constexpr double kDeg = M_PI / 180.0;

// Julian date 2440587.5 is the Unix epoch, and one day is 86400 s.
constexpr double kUnixEpochJd = 2440587.5;

double julianDate(const QDateTime& utc)
{
    return kUnixEpochJd
         + utc.toUTC().toMSecsSinceEpoch() / 86400000.0;
}

QDateTime fromJulian(double jd)
{
    const double ms = (jd - kUnixEpochJd) * 86400000.0;
    // Qt::UTC (not QTimeZone::UTC, which is Qt 6.7+) so this keeps
    // compiling on the ubuntu-24.04-arm release runner's system Qt 6.4.2.
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(std::llround(ms)),
                                          Qt::UTC);
}

double normDeg(double d)
{
    d = std::fmod(d, 360.0);
    return d < 0.0 ? d + 360.0 : d;
}

} // namespace

SolarInfo solarInfo(const QDateTime& utc, double lat, double lonEast)
{
    SolarInfo info;
    if (!utc.isValid()) { return info; }

    const double jdNow = julianDate(utc);
    const double days  = jdNow - 2451545.0;      // days since J2000.0

    // The classical formulation counts longitude positive WEST, so the
    // sign flips here once and nowhere else.
    const double lw = -lonEast;

    // Mean solar noon at this longitude, as a day number since J2000.
    const double n     = std::round(days - 0.0009 - lw / 360.0);
    const double jStar = 0.0009 + lw / 360.0 + n;

    // Solar mean anomaly, then the equation of centre — the Earth's
    // orbit is an ellipse, so the sun runs ahead of the mean by up to
    // about two degrees.
    const double m = normDeg(357.5291 + 0.98560028 * jStar);
    const double c = 1.9148 * std::sin(m * kDeg)
                   + 0.0200 * std::sin(2 * m * kDeg)
                   + 0.0003 * std::sin(3 * m * kDeg);

    const double lambda = normDeg(m + c + 180.0 + 102.9372);

    // Solar transit: the instant the sun crosses the local meridian.
    const double jTransit = 2451545.0 + jStar
                          + 0.0053 * std::sin(m * kDeg)
                          - 0.0069 * std::sin(2 * lambda * kDeg);

    // Declination: how far north or south the sun is standing today.
    const double sinDec = std::sin(lambda * kDeg) * std::sin(23.4397 * kDeg);
    const double dec    = std::asin(sinDec);

    const double phi = lat * kDeg;

    // Hour angle of sunrise. -0.833 deg rather than 0 accounts for the
    // sun's radius plus average refraction — the disc is already fully
    // up when its centre is still below the geometric horizon.
    const double cosOmega =
        (std::sin(-0.833 * kDeg) - std::sin(phi) * sinDec)
        / (std::cos(phi) * std::cos(dec));

    if (cosOmega < -1.0) {
        info.alwaysUp = true;          // sun never sets today
    } else if (cosOmega > 1.0) {
        info.alwaysDown = true;        // sun never rises today
    } else {
        const double omega = std::acos(cosOmega) / kDeg;   // degrees
        info.riseUtc = fromJulian(jTransit - omega / 360.0);
        info.setUtc  = fromJulian(jTransit + omega / 360.0);
    }

    // Elevation now. One day is a full turn of the hour angle, so the
    // offset from transit converts straight to degrees.
    const double h = (jdNow - jTransit) * 360.0 * kDeg;
    const double sinElev = std::sin(phi) * sinDec
                         + std::cos(phi) * std::cos(dec) * std::cos(h);
    info.elevationDeg = std::asin(std::clamp(sinElev, -1.0, 1.0)) / kDeg;

    // Civil twilight either side of the horizon. Deliberately symmetric:
    // the band matters on the way in and on the way out.
    info.greyline = std::abs(info.elevationDeg) <= 6.0;

    return info;
}

} // namespace Longpath
