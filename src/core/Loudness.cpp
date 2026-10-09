// =================================================================
// src/core/Loudness.cpp  (Longpath)
// =================================================================
//
// Longpath-original. See Loudness.h for why the analyser measures in
// LUFS and LRA rather than in three home-made level numbers.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/Loudness.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Longpath::Loudness {

namespace {

constexpr double kEps = 1e-30;

// ── The two stages, derived the way the standard derives them ────────
//
// NOT the RBJ cookbook. That was the first attempt and the test caught
// it: a cookbook high-shelf at the same frequency, gain and Q misses
// BS.1770's published 48 kHz table in the third decimal, and a
// cookbook high-pass produces b = (0.9975, -1.9950, 0.9975) where the
// table says exactly (1, -2, 1). Close enough to look right in a plot,
// wrong enough to give a loudness figure no other meter agrees with.
//
// The standard's own form is below. The one piece of it that cannot
// be guessed is the exponent on Vb — 0.4996667741545416, which is
// what makes the shelf's mid-band slope land where the table has it.
// With it, both stages reproduce the published coefficients to every
// digit the table prints, which is what kWeightingMatchesPublished48k
// checks.
Biquad shelf(double sampleRate, double freqHz, double gainDb, double q)
{
    const double k  = std::tan(M_PI * freqHz / sampleRate);
    const double kk = k * k;
    const double vh = std::pow(10.0, gainDb / 20.0);
    const double vb = std::pow(vh, 0.4996667741545416);
    const double a0 = 1.0 + k / q + kk;

    return Biquad{
        (vh + vb * k / q + kk) / a0,
        2.0 * (kk - vh) / a0,
        (vh - vb * k / q + kk) / a0,
        2.0 * (kk - 1.0) / a0,
        (1.0 - k / q + kk) / a0,
    };
}

// The RLB stage. Its numerator is exactly 1, -2, 1 — unnormalised,
// as the table has it, and that is not an oversight in the standard:
// the pair of zeros at DC is the whole point of the filter, and
// scaling them would move the band it hands on to the gate.
Biquad rlb(double sampleRate, double freqHz, double q)
{
    const double k   = std::tan(M_PI * freqHz / sampleRate);
    const double kk  = k * k;
    const double den = 1.0 + k / q + kk;

    return Biquad{
        1.0, -2.0, 1.0,
        2.0 * (kk - 1.0) / den,
        (1.0 - k / q + kk) / den,
    };
}

// Direct form I, double precision throughout. The RLB stage has its
// poles at 38 Hz, which at 48 kHz sits close enough to the unit
// circle that single precision loses the bottom of the answer.
void runBiquad(const Biquad& f, std::vector<double>& x)
{
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
    for (double& v : x) {
        const double in = v;
        const double out = f.b0 * in + f.b1 * x1 + f.b2 * x2
                         - f.a1 * y1 - f.a2 * y2;
        x2 = x1; x1 = in;
        y2 = y1; y1 = out;
        v = out;
    }
}

// A block's loudness in LUFS from its mean square. The -0.691 is the
// standard's own calibration offset.
double blockLufs(double meanSquare)
{
    return -0.691 + 10.0 * std::log10(std::max(meanSquare, kEps));
}

// The mean squares of every overlapping block of `blockSeconds`.
std::vector<double> blockMeanSquares(const std::vector<double>& k,
                                     int sampleRate, double blockSeconds)
{
    std::vector<double> out;
    const int len = int(blockSeconds * sampleRate);
    if (len <= 0 || int(k.size()) < len) { return out; }
    const int hop = std::max(1, int(len * (1.0 - kBlockOverlap)));

    for (int start = 0; start + len <= int(k.size()); start += hop) {
        double acc = 0.0;
        for (int i = 0; i < len; ++i) {
            acc += k[static_cast<size_t>(start + i)]
                 * k[static_cast<size_t>(start + i)];
        }
        out.push_back(acc / len);
    }
    return out;
}

} // namespace

Biquad shelfAt(double sampleRate)
{
    return shelf(sampleRate, kShelfFreqHz, kShelfGainDb, kShelfQ);
}

Biquad rlbAt(double sampleRate)
{
    return rlb(sampleRate, kRlbFreqHz, kRlbQ);
}

Result measure(const float* samples, int frames, int sampleRate)
{
    Result r;
    if (!samples || frames <= 0 || sampleRate <= 0) { return r; }

    double peak = 0.0;
    std::vector<double> k(static_cast<size_t>(frames));
    for (int i = 0; i < frames; ++i) {
        const double v = samples[i];
        k[static_cast<size_t>(i)] = v;
        peak = std::max(peak, std::abs(v));
    }
    r.samplePeakDbFs = 20.0 * std::log10(std::max(peak, kEps));

    runBiquad(shelfAt(sampleRate), k);
    runBiquad(rlbAt(sampleRate), k);

    // ── Integrated loudness: the two-stage gate ──────────────────────
    //
    // First throw away everything under the absolute gate, then take
    // the mean of what is left, then throw away everything more than
    // 10 LU under THAT and take the mean again. The second pass is
    // what stops a long pause from dragging a voice's loudness down.
    {
        const std::vector<double> ms =
            blockMeanSquares(k, sampleRate, kBlockSeconds);
        if (ms.empty()) { return r; }

        double sum = 0.0;
        int    n   = 0;
        for (double m : ms) {
            if (blockLufs(m) > kAbsoluteGateLufs) { sum += m; ++n; }
        }
        if (n == 0) {
            r.valid = true;
            return r;
        }

        const double relativeGate = blockLufs(sum / n) + kRelativeGateLu;
        double sum2 = 0.0;
        int    n2   = 0;
        for (double m : ms) {
            const double l = blockLufs(m);
            if (l > kAbsoluteGateLufs && l > relativeGate) {
                sum2 += m;
                ++n2;
            }
        }
        r.integratedLufs = n2 > 0 ? blockLufs(sum2 / n2) : blockLufs(sum / n);
    }

    // ── Loudness range, Tech 3342 ────────────────────────────────────
    //
    // Three-second blocks, the same absolute gate, a RELATIVE gate 20
    // LU down instead of 10, and then the spread between the 10th and
    // 95th percentile of what survives. The wider relative gate is
    // the whole difference between "how loud" and "how much it
    // moves": at -10 LU the quiet half of a dynamic take would be
    // gated away and the range would come out flat.
    {
        const std::vector<double> ms =
            blockMeanSquares(k, sampleRate, kRangeBlockSeconds);

        std::vector<double> gated;
        double sum = 0.0;
        int    n   = 0;
        for (double m : ms) {
            if (blockLufs(m) > kAbsoluteGateLufs) { sum += m; ++n; }
        }
        if (n > 0) {
            const double relativeGate =
                blockLufs(sum / n) + kRangeRelativeGateLu;
            for (double m : ms) {
                const double l = blockLufs(m);
                if (l > kAbsoluteGateLufs && l > relativeGate) {
                    gated.push_back(l);
                }
            }
        }

        // Fewer than four blocks is not a range, it is two numbers.
        // Saying so beats reporting a percentile spread of three
        // samples as though it described the operator's delivery.
        if (gated.size() >= 4) {
            std::sort(gated.begin(), gated.end());
            const auto at = [&](double p) {
                const size_t i = std::min(gated.size() - 1,
                    static_cast<size_t>(p * (gated.size() - 1) + 0.5));
                return gated[i];
            };
            r.rangeLu    = at(kRangeHighPercentile) - at(kRangeLowPercentile);
            r.rangeValid = true;
        }
    }

    r.valid = true;
    return r;
}

} // namespace Longpath::Loudness
