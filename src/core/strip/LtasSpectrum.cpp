// =================================================================
// src/core/strip/LtasSpectrum.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Siehe die Kopfdatei.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20, AI-assisted via Anthropic Claude
//                 (Claude Code), operator Martin Fischer.
// =================================================================

#include "core/strip/LtasSpectrum.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace Longpath::Ltas {

namespace {

constexpr int    kFft      = 2048;   // 42,7 ms bei 48 kHz
constexpr int    kHop      = 1024;   // halbe Ueberlappung
constexpr double kLowHz    = 50.0;
constexpr double kHighHz   = 8000.0;
constexpr int    kPerOctave = 6;

// Radix-2 in place, wie in VoiceAnalyzer.cpp.
void fft(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    if (n < 2) { return; }
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

const std::vector<double>& centresHz()
{
    static const std::vector<double> grid = [] {
        std::vector<double> g;
        const double step = std::pow(2.0, 1.0 / double(kPerOctave));
        for (double f = kLowHz; f <= kHighHz * 1.0001; f *= step) {
            g.push_back(f);
        }
        return g;
    }();
    return grid;
}

std::vector<double> analyse(const float* samples, int frames, int sampleRate)
{
    const std::vector<double>& grid = centresHz();
    if (samples == nullptr || frames < kFft || sampleRate <= 0) { return {}; }

    static const std::vector<double> hann = [] {
        std::vector<double> w(kFft);
        for (int i = 0; i < kFft; ++i) {
            w[size_t(i)] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (kFft - 1)));
        }
        return w;
    }();

    std::vector<double> power(static_cast<size_t>(kFft / 2 + 1), 0.0);
    int runs = 0;
    // static_cast, nicht size_t(kFft): letzteres liest der
    // Uebersetzer als Parameterdeklaration und haelt die Zeile
    // fuer eine Funktionsdeklaration (most vexing parse).
    std::vector<std::complex<double>> buf(static_cast<size_t>(kFft));

    for (int start = 0; start + kFft <= frames; start += kHop) {
        // Stille Faenster weglassen. Eine Sprechpause ist Raumklang,
        // und Raumklang gemittelt mit Sprache ergibt ein Spektrum,
        // das keine von beiden ist.
        double energy = 0.0;
        for (int i = 0; i < kFft; ++i) {
            const double v = samples[start + i];
            energy += v * v;
        }
        if (energy / kFft < 1e-8) { continue; }

        for (int i = 0; i < kFft; ++i) {
            buf[size_t(i)] = std::complex<double>(
                samples[start + i] * hann[size_t(i)], 0.0);
        }
        fft(buf);
        for (int k = 0; k <= kFft / 2; ++k) {
            power[size_t(k)] += std::norm(buf[size_t(k)]);
        }
        ++runs;
    }
    if (runs == 0) { return {}; }

    const double binHz = double(sampleRate) / kFft;
    const double edge = std::pow(2.0, 0.5 / double(kPerOctave));

    std::vector<double> out(grid.size(), -120.0);
    for (size_t b = 0; b < grid.size(); ++b) {
        const double lo = grid[b] / edge;
        const double hi = grid[b] * edge;
        const int kLo = std::max(1, int(std::floor(lo / binHz)));
        const int kHi = std::min(kFft / 2, int(std::ceil(hi / binHz)));
        if (kHi < kLo) { continue; }
        double sum = 0.0;
        for (int k = kLo; k <= kHi; ++k) { sum += power[size_t(k)]; }
        // Je Fach, nicht je Band: ein breites Band oben haette sonst
        // allein wegen seiner Breite mehr Pegel.
        const double perBin = sum / double(kHi - kLo + 1) / double(runs);
        out[b] = 10.0 * std::log10(std::max(perBin, 1e-20));
    }

    // Auf das Sprachband beziehen. Absolute Pegel haengen an der
    // Mikrofonverstaerkung; verglichen wird die Form.
    double ref = 0.0;
    int refCount = 0;
    for (size_t b = 0; b < grid.size(); ++b) {
        if (grid[b] >= 300.0 && grid[b] <= 3000.0) {
            ref += out[b];
            ++refCount;
        }
    }
    if (refCount > 0) {
        ref /= refCount;
        for (double& v : out) { v -= ref; }
    }
    return out;
}

} // namespace Longpath::Ltas
