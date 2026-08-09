// =================================================================
// src/gui/applets/eq/ClientEqFftAnalyzer.h  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR (https://github.com/aethersdr/AetherSDR),
// GPLv3, primary author Jeremy [KK7GWY]:
//   src/gui/ClientEqFftAnalyzer.h at 31b29583
//
// AetherSDR carries no per-file licence headers, so per
// docs/attribution/HOW-TO-PORT.md rule 6 the citation is at project
// level: there is no verbatim block to copy. Both projects are GPLv3,
// so the code carries forward under the same licence per GPLv3 §5.
//
// Ported at the bench's request to reproduce AetherSDR's equaliser
// exactly — its display and its behaviour — rather than to approximate
// it. The DSP it drives, core/strip/ClientEq, was already a verbatim
// port of the same upstream, so the pair are back together.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Ported to NereusSDR by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork). Namespace AetherSDR →
//                 NereusSDR; include paths rebased onto
//                 core/strip/ and gui/applets/eq/. Behaviour unchanged.
// =================================================================

#pragma once

#include <array>
#include <vector>

namespace NereusSDR {

// Small, self-contained FFT analyzer used by the Client EQ editor to
// render a live spectrum behind the response curve. Fixed 2048-point
// radix-2 Cooley-Tukey — bin resolution fs/N = 11.7 Hz at 24 kHz, so
// the first (non-DC) bin lands below the 20 Hz display floor and the
// analyzer does not produce a visible "cutoff" artifact at the
// leftmost visible frequency. Runs in ~200 µs on the UI thread, cheap
// enough for a 25 Hz timer.
//
// Usage:
//   ClientEqFftAnalyzer fft;
//   fft.update(samples, ClientEqFftAnalyzer::kFftSize);  // from audio tap
//   for (auto db : fft.magnitudesDb()) ...
//
// Magnitude bins are exponentially smoothed per-bin with asymmetric
// attack (fast) and decay (slow) — the classic "analyzer follow" feel.
class ClientEqFftAnalyzer {
public:
    static constexpr int kFftSize = 2048;
    static constexpr int kBinCount = kFftSize / 2 + 1;  // 0 Hz .. Nyquist

    ClientEqFftAnalyzer();

    // Feed the most-recent kFftSize samples. The window (Hann) and the
    // FFT run inline; smoothed magnitudes are updated afterwards.
    void update(const float* samples, int count) noexcept;

    // Reset smoothing state — e.g. when the editor hides so the next
    // opening doesn't show frozen bars from the last session.
    void reset() noexcept;

    // Magnitudes in dB, length kBinCount. Floor is kFloorDb to keep the
    // log curve from collapsing to -infinity at silent bins.
    const std::vector<float>& magnitudesDb() const { return m_smoothedDb; }

    // Frequency of bin index i for a given sample rate.
    static float binFreq(int bin, double sampleRate) {
        return static_cast<float>(bin * sampleRate / kFftSize);
    }

    static constexpr float kFloorDb = -100.0f;

private:
    void buildWindow();

    std::array<float, kFftSize>   m_window;    // precomputed Hann
    std::vector<float>            m_smoothedDb; // size kBinCount
    bool                          m_primed{false};
};

} // namespace NereusSDR
