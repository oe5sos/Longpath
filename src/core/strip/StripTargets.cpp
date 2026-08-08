// =================================================================
// src/core/strip/StripTargets.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See StripTargets.h for why the target is a
// named choice rather than a constant.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/StripTargets.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace NereusSDR::StripTargets {

namespace {

struct Point { double hz; double db; };

// Interpolated on a log frequency axis, because that is how the ear
// and every filter in the chain treat frequency. Linear interpolation
// between these on a linear axis would put the knee of each curve in
// the wrong place by an octave.
double interp(const std::vector<Point>& pts, double hz)
{
    if (pts.empty()) { return 0.0; }
    if (hz <= pts.front().hz) { return pts.front().db; }
    if (hz >= pts.back().hz)  { return pts.back().db; }
    for (size_t i = 1; i < pts.size(); ++i) {
        if (hz <= pts[i].hz) {
            const double t = (std::log10(hz) - std::log10(pts[i - 1].hz))
                           / (std::log10(pts[i].hz) - std::log10(pts[i - 1].hz));
            return pts[i - 1].db + t * (pts[i].db - pts[i - 1].db);
        }
    }
    return pts.back().db;
}

} // namespace

QVector<Profile> profiles()
{
    return {
        {QStringLiteral("SSB 2.7 kHz"),
         QStringLiteral("The everyday setting, and the one most receivers "
                        "are set to. Balanced: enough low end to sound "
                        "like you, enough presence to be understood."),
         2700.0},
        {QStringLiteral("SSB 3.0 kHz"),
         QStringLiteral("A little more room at the top. Slightly more "
                        "natural on a strong signal; on a crowded band "
                        "the extra 300 Hz is spent on your neighbours."),
         3000.0},
        {QStringLiteral("SSB 3.3 kHz"),
         QStringLiteral("Wide. Worth it only when the band is quiet and "
                        "the other operator has the filter open to match "
                        "— otherwise most of it is thrown away at their "
                        "receiver."),
         3300.0},
        {QStringLiteral("Contest"),
         QStringLiteral("Everything into 300-2500 Hz, low end cut hard, "
                        "presence lifted. Costs naturalness and buys the "
                        "one thing that matters in a pile-up: being "
                        "picked out on the first call."),
         2500.0},
        {QStringLiteral("DX / Voodoo"),
         QStringLiteral("Harder still. A narrow, forward sound built to "
                        "cut through noise at the far end. Tiring over a "
                        "long contact and unmistakable over a short one."),
         2400.0},
    };
}

double targetDb(const QString& profile, double hz)
{
    // Every curve is 0 dB at 1 kHz, so they can be compared with each
    // other and offset onto a measurement without further arithmetic.
    static const std::vector<Point> kSsb27 = {
        {20, -26}, {60, -20}, {100, -12}, {200, -5}, {400, -1},
        {1000, 0}, {1800, 2}, {2400, 2}, {2700, 0}, {3200, -12},
        {6000, -26}, {16000, -34},
    };
    static const std::vector<Point> kSsb30 = {
        {20, -26}, {60, -20}, {100, -12}, {200, -5}, {400, -1},
        {1000, 0}, {1800, 2}, {2600, 2}, {3000, 0}, {3600, -12},
        {6000, -24}, {16000, -32},
    };
    static const std::vector<Point> kSsb33 = {
        {20, -24}, {60, -18}, {100, -10}, {200, -4}, {400, -1},
        {1000, 0}, {1800, 2}, {2800, 2}, {3300, 0}, {4000, -12},
        {7000, -22}, {16000, -30},
    };
    // Contest: the low end goes, because below 300 Hz nothing survives
    // a narrow receive filter and every decibel of it was taken from
    // the compressor's headroom. Presence lifted hard.
    static const std::vector<Point> kContest = {
        {20, -34}, {100, -26}, {200, -14}, {300, -6}, {600, -2},
        {1000, 0}, {1600, 4}, {2200, 5}, {2500, 2}, {3000, -16},
        {6000, -30}, {16000, -38},
    };
    // Harder again, and narrower. The peak sits where the ear is most
    // sensitive and where a noisy channel is least so.
    static const std::vector<Point> kDx = {
        {20, -36}, {100, -30}, {250, -16}, {400, -8}, {700, -3},
        {1000, 0}, {1600, 5}, {2100, 7}, {2400, 3}, {2900, -18},
        {6000, -32}, {16000, -40},
    };

    if (profile == QLatin1String("SSB 3.0 kHz")) { return interp(kSsb30, hz); }
    if (profile == QLatin1String("SSB 3.3 kHz")) { return interp(kSsb33, hz); }
    if (profile == QLatin1String("Contest"))     { return interp(kContest, hz); }
    if (profile == QLatin1String("DX / Voodoo")) { return interp(kDx, hz); }
    // Unknown falls back to the everyday curve rather than to zero: a
    // flat line would look like a deliberate target and would tell the
    // operator to undo every useful thing they had done.
    return interp(kSsb27, hz);
}

} // namespace NereusSDR::StripTargets
