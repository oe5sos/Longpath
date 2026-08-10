// =================================================================
// src/core/antenna/AntennaSweep.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See AntennaSweep.h for why resonance and the
// SWR minimum are reported separately.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/antenna/AntennaSweep.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR::AntennaSweep {
namespace {

// An |Γ| this close to 1 is an open or a short, and 1/(1-|Γ|) runs away.
// 999 is the number SWR meters print for "off the scale", and it is
// more honest than infinity because it can be drawn.
constexpr double kMaxSwr = 999.0;

// Anti-resonances sit where R is enormous. Nothing this high is a feed
// point, and the threshold keeps a noisy crossing near a genuine series
// resonance from being misfiled as an anti-resonance.
constexpr double kAntiResonanceOhms = 400.0;

double lerp(double a, double b, double t) { return a + (b - a) * t; }

} // namespace

double swr(const std::complex<double>& gamma)
{
    const double g = std::abs(gamma);
    if (!(g < 1.0)) { return kMaxSwr; }   // also catches NaN
    const double s = (1.0 + g) / (1.0 - g);
    return std::min(s, kMaxSwr);
}

double returnLossDb(const std::complex<double>& gamma)
{
    const double g = std::abs(gamma);
    if (g <= 1e-9) { return -180.0; }   // a perfect match, near enough
    return 20.0 * std::log10(g);
}

std::complex<double> impedance(const std::complex<double>& gamma,
                               double referenceOhms)
{
    const std::complex<double> one(1.0, 0.0);
    const std::complex<double> den = one - gamma;
    // Γ = 1 is an open circuit: infinite impedance. Report something
    // large and finite rather than a NaN that poisons every plot it
    // touches.
    if (std::abs(den) < 1e-12) { return {1e9, 0.0}; }
    return referenceOhms * (one + gamma) / den;
}

QVector<Crossing> resonances(const Sweep& s)
{
    QVector<Crossing> out;
    if (s.points.size() < 2) { return out; }

    auto reactance = [&s](int i) {
        return impedance(s.points.at(i).gamma, s.referenceOhms).imag();
    };

    for (int i = 1; i < s.points.size(); ++i) {
        const double x0 = reactance(i - 1);
        const double x1 = reactance(i);

        // ── Half-open intervals, and why ─────────────────────────────
        //
        // A sample can land exactly on zero — it does whenever the
        // sweep grid happens to include the resonant frequency, which
        // on a round-numbered sweep is often. Testing "the two samples
        // differ in sign or one of them is zero" then finds the SAME
        // crossing twice: once as the interval ending on the zero and
        // once as the interval starting from it. Two markers, one
        // resonance, and a caller counting crossings gets it wrong.
        //
        // Treating each interval as [x0, x1) fixes it: a zero belongs
        // to the interval it ENDS, never to the one it begins. x0 == 0
        // therefore matches neither test below and is skipped, which is
        // also what makes a run of zeros report nothing.
        const bool rises = x0 < 0.0 && x1 >= 0.0;
        const bool falls = x0 > 0.0 && x1 <= 0.0;
        if (!rises && !falls) { continue; }

        const double f0 = s.points.at(i - 1).freqHz;
        const double f1 = s.points.at(i).freqHz;
        // Where the straight line between the two samples hits zero.
        const double t = (x1 == x0) ? 0.0 : (-x0 / (x1 - x0));
        const double fx = lerp(f0, f1, std::clamp(t, 0.0, 1.0));

        const std::complex<double> z0 =
            impedance(s.points.at(i - 1).gamma, s.referenceOhms);
        const std::complex<double> z1 =
            impedance(s.points.at(i).gamma, s.referenceOhms);

        Crossing c;
        c.found  = true;
        c.freqHz = fx;
        c.resistanceOhms = lerp(z0.real(), z1.real(), std::clamp(t, 0.0, 1.0));
        c.rising = rises;
        // The SWR at the crossing, from the interpolated impedance
        // rather than from the nearer sample — the point of
        // interpolating at all is not to round back to the grid.
        const std::complex<double> zc(c.resistanceOhms, 0.0);
        const std::complex<double> g =
            (zc - s.referenceOhms) / (zc + s.referenceOhms);
        c.swrThere = swr(g);
        out.append(c);
    }
    return out;
}

Crossing nearestResonance(const Sweep& s, double nearHz)
{
    const QVector<Crossing> all = resonances(s);
    Crossing best;
    double bestDistance = 0.0;

    for (const Crossing& c : all) {
        // Series resonance only. A falling crossing with a huge R is
        // the anti-resonance — trimming towards it makes the antenna
        // worse in a way that looks like progress on an SWR meter.
        if (!c.rising && c.resistanceOhms > kAntiResonanceOhms) { continue; }

        const double d = std::abs(c.freqHz - nearHz);
        if (!best.found || d < bestDistance) {
            best = c;
            bestDistance = d;
        }
    }
    return best;
}

Minimum bestMatch(const Sweep& s)
{
    Minimum m;
    for (const SweepPoint& p : s.points) {
        const double v = swr(p.gamma);
        if (!m.found || v < m.swr) {
            m.found  = true;
            m.swr    = v;
            m.freqHz = p.freqHz;
        }
    }
    return m;
}

double swrAt(const Sweep& s, double freqHz)
{
    if (s.points.size() < 2) { return 0.0; }
    if (freqHz < s.startHz() || freqHz > s.stopHz()) { return 0.0; }

    for (int i = 1; i < s.points.size(); ++i) {
        const double f0 = s.points.at(i - 1).freqHz;
        const double f1 = s.points.at(i).freqHz;
        if (freqHz < f0 || freqHz > f1) { continue; }
        const double t = (f1 == f0) ? 0.0 : (freqHz - f0) / (f1 - f0);
        return lerp(swr(s.points.at(i - 1).gamma),
                    swr(s.points.at(i).gamma), t);
    }
    return 0.0;
}

std::complex<double> impedanceAt(const Sweep& s, double freqHz)
{
    if (s.points.size() < 2) { return {}; }
    if (freqHz < s.startHz() || freqHz > s.stopHz()) { return {}; }

    for (int i = 1; i < s.points.size(); ++i) {
        const double f0 = s.points.at(i - 1).freqHz;
        const double f1 = s.points.at(i).freqHz;
        if (freqHz < f0 || freqHz > f1) { continue; }
        const double t = (f1 == f0) ? 0.0 : (freqHz - f0) / (f1 - f0);
        const std::complex<double> z0 =
            impedance(s.points.at(i - 1).gamma, s.referenceOhms);
        const std::complex<double> z1 =
            impedance(s.points.at(i).gamma, s.referenceOhms);
        // Interpolating the impedance rather than the reflection
        // coefficient: R and X are what the operator reads, and a
        // straight line between two impedances is what they expect the
        // number between two samples to be.
        return {lerp(z0.real(), z1.real(), t), lerp(z0.imag(), z1.imag(), t)};
    }
    return {};
}

Span usableSpan(const Sweep& s, double limit, double aroundHz)
{
    Span span;
    if (s.points.size() < 2) { return span; }

    // Which sample to grow outwards from. Without a frequency to centre
    // on, the widest run in the sweep is the useful default; with one,
    // the run containing it — a multiband antenna has several and only
    // one of them is the band you are on.
    int seed = -1;
    if (aroundHz > 0.0) {
        double bestD = 0.0;
        for (int i = 0; i < s.points.size(); ++i) {
            const double d = std::abs(s.points.at(i).freqHz - aroundHz);
            if (seed < 0 || d < bestD) { seed = i; bestD = d; }
        }
        if (swr(s.points.at(seed).gamma) > limit) { return span; }
    } else {
        const Minimum m = bestMatch(s);
        if (!m.found || m.swr > limit) { return span; }
        for (int i = 0; i < s.points.size(); ++i) {
            if (s.points.at(i).freqHz == m.freqHz) { seed = i; break; }
        }
        if (seed < 0) { return span; }
    }

    int lo = seed;
    while (lo > 0 && swr(s.points.at(lo - 1).gamma) <= limit) { --lo; }
    int hi = seed;
    while (hi + 1 < s.points.size()
           && swr(s.points.at(hi + 1).gamma) <= limit) { ++hi; }

    span.found  = true;
    span.lowHz  = s.points.at(lo).freqHz;
    span.highHz = s.points.at(hi).freqHz;

    // Interpolate the edges. Stopping at the last sample under the
    // limit reports a narrower span than the antenna has, and on a
    // 101-point sweep that is a few kilohertz at each end.
    if (lo > 0) {
        const double a = swr(s.points.at(lo - 1).gamma);
        const double b = swr(s.points.at(lo).gamma);
        if (a > limit && b < a) {
            const double t = (a - limit) / (a - b);
            span.lowHz = lerp(s.points.at(lo - 1).freqHz,
                              s.points.at(lo).freqHz, std::clamp(t, 0.0, 1.0));
        }
    }
    if (hi + 1 < s.points.size()) {
        const double a = swr(s.points.at(hi).gamma);
        const double b = swr(s.points.at(hi + 1).gamma);
        if (b > limit && b > a) {
            const double t = (limit - a) / (b - a);
            span.highHz = lerp(s.points.at(hi).freqHz,
                               s.points.at(hi + 1).freqHz,
                               std::clamp(t, 0.0, 1.0));
        }
    }
    return span;
}

QString describe(const Sweep& s, double targetHz)
{
    if (s.isEmpty()) {
        return s.note.isEmpty() ? QStringLiteral("No sweep loaded.") : s.note;
    }

    const Crossing r = nearestResonance(s, targetHz);
    const Minimum  m = bestMatch(s);

    auto mhz = [](double hz) {
        return QStringLiteral("%1 MHz").arg(hz / 1e6, 0, 'f', 3);
    };

    if (!r.found) {
        // Worth saying rather than reporting the SWR dip as if it were
        // the answer: no crossing means the antenna is not resonant
        // anywhere in the swept range, and a wider sweep is the next
        // step.
        return QStringLiteral(
            "The reactance never reaches zero in this sweep, so the "
            "antenna is not resonant between %1 and %2. Sweep wider to "
            "find it.").arg(mhz(s.startHz()), mhz(s.stopHz()));
    }

    QString out = QStringLiteral("Resonant at %1 — %2 Ω there, SWR %3.")
                      .arg(mhz(r.freqHz))
                      .arg(r.resistanceOhms, 0, 'f', 0)
                      .arg(r.swrThere, 0, 'f', 2);

    // Only worth mentioning when they are far enough apart to matter.
    // A few kilohertz is measurement noise, not a lesson.
    if (m.found && std::abs(m.freqHz - r.freqHz) > 20e3) {
        out += QStringLiteral(
            " The lowest SWR is elsewhere, at %1 (%2) — that is where "
            "the feed resistance passes 50 Ω, not where the antenna is "
            "resonant. Trim to the resonance, not to the dip.")
                   .arg(mhz(m.freqHz)).arg(m.swr, 0, 'f', 2);
    }
    return out;
}

} // namespace NereusSDR::AntennaSweep
