// =================================================================
// src/core/SolarTimes.cpp  (Longpath)
// =================================================================
//
// Longpath-original — see SolarTimes.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-11-10 — subsolarPoint(); see SolarTimes.h. AI-assisted via
//                 Anthropic Claude (Claude Code), operator Martin Fischer.
// =================================================================

#include "SolarTimes.h"
#include "core/ZeitUtc.h"

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
    // UTC ueber Longpath::Zeit (core/ZeitUtc.h). Dort steht, warum:
    // `Qt::TimeSpec` ist seit Qt 6.7 veraltet, `QTimeZone::UTC` gibt
    // es erst ab 6.7 -- und von Hand uebersetzt wird auch auf Ubuntu
    // 24.04 mit Qt 6.4.2. Die Fallunterscheidung steht an EINER Stelle.
    return Longpath::Zeit::ausMillisekunden(static_cast<qint64>(std::llround(ms)));
}

double normDeg(double d)
{
    d = std::fmod(d, 360.0);
    return d < 0.0 ? d + 360.0 : d;
}

// Mean anomaly and ecliptic longitude for a day number since J2000.0.
// One place for both solarInfo() and subsolarPoint(), so the globe and
// the grey-line verdict cannot drift apart.
void ecliptic(double j, double& m, double& lambda)
{
    m = normDeg(357.5291 + 0.98560028 * j);
    // Equation of centre — the Earth's orbit is an ellipse, so the sun
    // runs ahead of the mean by up to about two degrees.
    const double c = 1.9148 * std::sin(m * kDeg)
                   + 0.0200 * std::sin(2 * m * kDeg)
                   + 0.0003 * std::sin(3 * m * kDeg);
    lambda = normDeg(m + c + 180.0 + 102.9372);
}

// The transit correction in days: how far the sun's crossing of a
// meridian runs from mean noon. Times 1440 it is the equation of time
// (with the opposite sign).
double transitTerm(double m, double lambda)
{
    return 0.0053 * std::sin(m * kDeg) - 0.0069 * std::sin(2 * lambda * kDeg);
}

double sinDeclination(double lambda)
{
    return std::sin(lambda * kDeg) * std::sin(23.4397 * kDeg);
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

    // Solar mean anomaly and ecliptic longitude (see ecliptic()).
    double m = 0.0, lambda = 0.0;
    ecliptic(jStar, m, lambda);

    // Solar transit: the instant the sun crosses the local meridian.
    const double jTransit = 2451545.0 + jStar + transitTerm(m, lambda);

    // Declination: how far north or south the sun is standing today.
    const double sinDec = sinDeclination(lambda);
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

void subsolarPoint(const QDateTime& utc, double& lat, double& lonEast)
{
    lat = 0.0;
    lonEast = 0.0;
    if (!utc.isValid()) { return; }

    const double days = julianDate(utc) - 2451545.0;
    double m = 0.0, lambda = 0.0;
    ecliptic(days, m, lambda);
    lat = std::asin(sinDeclination(lambda)) / kDeg;

    // solarInfo() puts the transit at longitude lw (positive west) at
    //   2451545 + 0.0009 + lw/360 + n + transitTerm.
    // Solved for the lw whose transit is now: only the fraction of a
    // day matters, so it is taken before scaling to degrees.
    double frac = days - 0.0009 - transitTerm(m, lambda);
    frac -= std::floor(frac);
    const double lw = frac * 360.0;
    lonEast = lw > 180.0 ? 360.0 - lw : -lw;      // [-180, 180)
}

} // namespace Longpath
