// =================================================================
// src/core/strip/EqLoudness.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See EqLoudness.h for why louder always sounds
// better and what follows from that.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/EqLoudness.h"

#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace NereusSDR::EqLoudness {

namespace {

// Where the average is taken. Below 80 Hz nothing survives an SSB
// transmitter and the ear discounts what is there; above 6 kHz there is
// no transmit channel wide enough to carry it.
constexpr double kLoHz    = 80.0;
constexpr double kHiHz    = 6000.0;
constexpr int    kSamples = 120;   // log-spaced probes

struct Pt { double hz; double db; };

double interpLog(const std::vector<Pt>& pts, double hz)
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

double aWeightingDb(double hz)
{
    // IEC 61672 class 1. The +2.0 dB offset is the normalisation that
    // makes the curve read exactly 0 dB at 1 kHz; without it every
    // figure here would be off by two decibels in the same direction,
    // which is the sort of error that looks like a preference.
    if (hz <= 0.0) { return -200.0; }
    const double f2 = hz * hz;
    const double num = 12194.0 * 12194.0 * f2 * f2;
    const double den = (f2 + 20.6 * 20.6)
                     * std::sqrt((f2 + 107.7 * 107.7) * (f2 + 737.9 * 737.9))
                     * (f2 + 12194.0 * 12194.0);
    if (den <= 0.0) { return -200.0; }
    return 20.0 * std::log10(num / den) + 2.0;
}

double kWeightingDb(double hz)
{
    // The two analog sections BS.1770's coefficients come from. Written
    // out rather than pulled from ClientEq so this file does not depend
    // on the equaliser's filter maths being right in order to say how
    // loud something is — the two must be able to disagree, and a test
    // must be able to catch it.
    if (hz <= 0.0) { return -200.0; }
    const double w   = 2.0 * M_PI * hz;
    const double wsq = w * w;

    // High shelf: f0 1681.97 Hz, +3.99984 dB, Q 0.7071752.
    double shelf = 0.0;
    {
        const double f0 = 1681.9744509555319;
        const double q  = 0.7071752369554193;
        const double A  = std::pow(10.0, 3.999843853973347 / 40.0);
        const double w0 = 2.0 * M_PI * f0;
        const double w0sq = w0 * w0;
        const double dN = w0sq - A * wsq;
        const double dD = A * w0sq - wsq;
        const double cross  = w0 * w / q;
        const double crossA = A * cross * cross;
        const double den = dD * dD + crossA;
        if (den > 0.0) {
            shelf = 10.0 * std::log10(A * A * (dN * dN + crossA) / den);
        }
    }

    // High-pass: f0 38.135 Hz, Q 0.500327.
    double high = 0.0;
    {
        const double f0 = 38.13547087602444;
        const double q  = 0.5003270373238773;
        const double w0 = 2.0 * M_PI * f0;
        const double w0sq = w0 * w0;
        const double diff  = w0sq - wsq;
        const double cross = w0 * w / q;
        const double den = diff * diff + cross * cross;
        if (den > 0.0) { high = 10.0 * std::log10(wsq * wsq / den); }
    }

    return shelf + high;
}

double speechWeightDb(double hz)
{
    // The shape of a long-term average speech spectrum: a broad peak in
    // the low hundreds, falling away steadily above it. Relative to its
    // own peak, so only the shape matters.
    static const std::vector<Pt> kLtas = {
        {50, -22}, {80, -12}, {125, -4}, {200, 0},  {315, 0},
        {500, -3}, {800, -7}, {1250, -11}, {2000, -15}, {3150, -19},
        {5000, -25}, {8000, -32},
    };
    return interpLog(kLtas, hz);
}

double addedLoudnessDb(const ClientEq& eq)
{
    const int bands = eq.activeBandCount();
    if (bands <= 0) { return 0.0; }

    const double rate = eq.sampleRate();
    const ClientEq::FilterFamily fam = eq.filterFamily();

    // Weighted average of the equaliser's power gain. Power domain, not
    // decibels: a 12 dB notch one sixth of an octave wide and a 1 dB
    // lift across the whole voice are very different amounts of
    // loudness, and averaging their decibel figures would call them
    // comparable.
    double sumW = 0.0;
    double sumWG = 0.0;

    for (int i = 0; i < kSamples; ++i) {
        const double hz = kLoHz * std::pow(kHiHz / kLoHz,
                                           double(i) / (kSamples - 1));
        if (hz >= rate * 0.5) { break; }

        double gainDb = 0.0;
        for (int b = 0; b < bands; ++b) {
            gainDb += double(ClientEq::bandMagnitudeDb(
                eq.band(b), float(hz), rate, fam));
        }

        // Weight is speech energy as a loudness meter counts it, as a
        // power. K-weighting, not A — see the header for why the first
        // attempt was wrong in a way that sounded like an opinion.
        const double wDb = speechWeightDb(hz) + kWeightingDb(hz);
        const double w   = std::pow(10.0, wDb / 10.0);

        sumW  += w;
        sumWG += w * std::pow(10.0, gainDb / 10.0);
    }

    if (sumW <= 0.0) { return 0.0; }
    const double meanPowerGain = sumWG / sumW;
    if (meanPowerGain <= 0.0) { return 0.0; }
    return 10.0 * std::log10(meanPowerGain);
}

double makeupDb(const ClientEq& eq)
{
    return std::clamp(-addedLoudnessDb(eq), -kMaxMakeupDb, kMaxMakeupDb);
}

void apply(ClientEq& eq)
{
    eq.setMasterGain(float(std::pow(10.0, makeupDb(eq) / 20.0)));
}

} // namespace NereusSDR::EqLoudness
