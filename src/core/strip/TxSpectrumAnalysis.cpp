// =================================================================
// src/core/strip/TxSpectrumAnalysis.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See TxSpectrumAnalysis.h for why the I channel
// alone is the right spectrum and why the edges are found by scanning
// inward.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/TxSpectrumAnalysis.h"

#include <algorithm>
#include <cmath>
#include <complex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace NereusSDR::TxSpectrumAnalysis {

namespace {

void fftInPlace(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) { j ^= bit; }
        j ^= bit;
        if (i < j) { std::swap(a[i], a[j]); }
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * M_PI / double(len);
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k) {
                const std::complex<double> u = a[i + k];
                const std::complex<double> v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

} // namespace

std::vector<double> ltasDb(const std::vector<float>& mono, int fftSize)
{
    std::vector<double> out;
    if (fftSize < 64 || (fftSize & (fftSize - 1)) != 0) { return out; }
    if (mono.size() < size_t(fftSize)) { return out; }

    const int bins = fftSize / 2;
    std::vector<double> power(static_cast<size_t>(bins), 0.0);
    int windows = 0;

    const double silenceRms = std::pow(10.0, kSilenceDbFs / 20.0);
    std::vector<std::complex<double>> a(static_cast<size_t>(fftSize));

    for (size_t start = 0; start + size_t(fftSize) <= mono.size();
         start += size_t(fftSize) / 2) {
        double rms = 0.0;
        for (int i = 0; i < fftSize; ++i) {
            const double s = mono[start + size_t(i)];
            rms += s * s;
        }
        rms = std::sqrt(rms / fftSize);
        if (rms < silenceRms) { continue; }

        for (int i = 0; i < fftSize; ++i) {
            const double w =
                0.5 * (1.0 - std::cos(2.0 * M_PI * i / (fftSize - 1)));
            a[static_cast<size_t>(i)] =
                std::complex<double>(mono[start + size_t(i)] * w, 0.0);
        }
        fftInPlace(a);
        for (int i = 0; i < bins; ++i) {
            const double m =
                std::abs(a[static_cast<size_t>(i)]) / (fftSize / 4.0);
            power[static_cast<size_t>(i)] += m * m;
        }
        ++windows;
    }

    if (windows == 0) { return out; }

    out.assign(static_cast<size_t>(bins), -160.0);
    for (int i = 0; i < bins; ++i) {
        const double mean = power[static_cast<size_t>(i)] / windows;
        out[static_cast<size_t>(i)] =
            mean > 1e-20 ? 10.0 * std::log10(mean) : -160.0;
    }
    return out;
}

Occupancy occupiedBandwidth(const std::vector<double>& magDb, double binHz,
                            double loHz, double hiHz)
{
    Occupancy occ;
    if (magDb.empty() || binHz <= 0.0) {
        occ.note = QStringLiteral("No spectrum to measure yet.");
        return occ;
    }

    const int n = int(magDb.size());
    const int lo = std::max(1, int(loHz / binHz));
    const int hi = std::min(n - 1, int(hiHz / binHz));
    if (hi <= lo) {
        occ.note = QStringLiteral("Measurement range is empty.");
        return occ;
    }

    int peakBin = lo;
    for (int i = lo; i <= hi; ++i) {
        if (magDb[static_cast<size_t>(i)]
            > magDb[static_cast<size_t>(peakBin)]) {
            peakBin = i;
        }
    }
    occ.peakDb = magDb[static_cast<size_t>(peakBin)];
    occ.peakHz = peakBin * binHz;

    if (occ.peakDb <= -140.0) {
        occ.note = QStringLiteral("Nothing is being transmitted — speak, or "
                                  "switch the off-air monitor on.");
        return occ;
    }

    // Scanning INWARD from each end, not outward from the peak.
    //
    // A real spectrum has dips in it. An outward walk stops at the first
    // one and reports a bandwidth far narrower than the truth — which is
    // the flattering direction, and therefore the dangerous one: it
    // would tell an operator who is splattering that they are clean.
    auto edges = [&](double downDb, double& lowHz, double& highHz) {
        const double thr = occ.peakDb - downDb;
        int a = lo;
        while (a <= hi && magDb[static_cast<size_t>(a)] < thr) { ++a; }
        int b = hi;
        while (b >= lo && magDb[static_cast<size_t>(b)] < thr) { --b; }
        if (a > b) { a = b = peakBin; }
        lowHz  = a * binHz;
        highHz = b * binHz;
    };

    edges(26.0, occ.lowHz26, occ.highHz26);
    edges(60.0, occ.lowHz60, occ.highHz60);
    occ.bandwidth26Hz = occ.highHz26 - occ.lowHz26;
    occ.bandwidth60Hz = occ.highHz60 - occ.lowHz60;
    occ.valid = true;
    return occ;
}

QString advice(const Occupancy& occ, double filterHighHz)
{
    if (!occ.valid) { return {}; }

    // Speaks only when the number affects somebody else. Everything
    // else in this window is taste, and an opinion delivered in the
    // same voice as a measurement devalues the measurement.
    if (occ.bandwidth26Hz > filterHighHz * 1.25 && filterHighHz > 0.0) {
        return QStringLiteral(
            "Your signal is %1 Hz wide at −26 dB, against a transmit "
            "filter set to %2 Hz. Energy is getting out past the filter, "
            "which means something ahead of it is generating it — most "
            "often the compressor or the exciter driven hard enough to "
            "make harmonics the filter never saw. Back one of them off "
            "and watch this number rather than the meter.")
            .arg(occ.bandwidth26Hz, 0, 'f', 0)
            .arg(filterHighHz, 0, 'f', 0);
    }
    if (occ.bandwidth60Hz > 6000.0) {
        return QStringLiteral(
            "There is still measurable energy %1 Hz wide at −60 dB. That "
            "is not loud, but it is two kilohertz either side of you and "
            "it is the part your neighbours hear rather than you.")
            .arg(occ.bandwidth60Hz, 0, 'f', 0);
    }
    return {};
}

} // namespace NereusSDR::TxSpectrumAnalysis
