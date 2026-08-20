#pragma once

// =================================================================
// src/core/strip/TxSpectrumAnalysis.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// What actually goes out, as opposed to what goes in.
//
// Everything else in the channel strip measures the microphone. The
// signal that leaves the radio has been through WDSP's equaliser,
// leveler, CFC and the ALC since then, and nobody has ever looked at
// it. That is the largest blind spot in this whole tool: the operator
// spends an hour shaping the input to a chain whose output is not
// displayed anywhere, and the first person to see the result is the
// station at the other end.
//
// ── Where the samples come from, and why that is safe ────────────────
//
// TxChannel::sip1OutputReady — the post-SSB-modulator I channel, which
// already exists and already feeds the off-air monitor. Crucially, the
// off-air monitor path lifts the siphon gate WITHOUT going on air: the
// radio write stays gated on m_running alone. So this measures a real
// modulated signal while the operator listens to themselves, and
// nothing here can key the transmitter. Nothing here even knows how.
//
// ── Why the I channel alone is the right spectrum ────────────────────
//
// This looks wrong and is not. For a single-sideband signal the
// modulator output s(t) = I + jQ is analytic, so I = Re{s} = (s + s*)/2
// and the spectrum of I at a positive frequency f is exactly S(f)/2.
// The magnitude shape is therefore the true single-sideband spectrum,
// six decibels down and otherwise unaltered. What is lost is only the
// distinction between upper and lower sideband, which the operator
// already knows and which no bandwidth figure depends on.
//
// ── Occupied bandwidth ───────────────────────────────────────────────
//
// Measured at -26 dB relative to the peak, which is the figure emission
// masks are written against, and again at -60 dB, which is where
// splatter lives. Both edges are found by scanning INWARD from the ends
// of the range rather than outward from the peak: a spectrum has dips
// in it, and an outward walk stops at the first one and reports a
// bandwidth far narrower than the truth.
//
// This is the one number in the audio tool that is about somebody else.
// Everything else is taste; this is whether the neighbours can use the
// band.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QString>

#include <vector>

namespace Longpath::TxSpectrumAnalysis {

// The long-term average spectrum of a block of modulator output, in dB
// per FFT bin. Power-domain average over overlapping Hann windows, for
// the same reason as everywhere else in the strip: averaging decibels
// gives a silent window the same weight as a loud one.
//
// Empty if there is not enough signal to average.
std::vector<double> ltasDb(const std::vector<float>& mono, int fftSize);

// Windows quieter than this are the gaps between words, not the signal.
inline constexpr double kSilenceDbFs = -60.0;

struct Occupancy {
    bool    valid{false};
    QString note;          // why not, when invalid

    double  peakHz{0.0};   // where the most energy is
    double  peakDb{0.0};   // its level, for scaling the picture

    // -26 dBc: the emission-mask figure.
    double  lowHz26{0.0};
    double  highHz26{0.0};
    double  bandwidth26Hz{0.0};

    // -60 dBc: where splatter lives. Wider, and the one that annoys
    // people two kilohertz away.
    double  lowHz60{0.0};
    double  highHz60{0.0};
    double  bandwidth60Hz{0.0};
};

// Find the occupied bandwidth of an LTAS.
//
// `binHz` is the frequency step per bin. Only the range `loHz`..`hiHz`
// is considered: DC and the very bottom of the spectrum carry offset
// and rumble that are not part of the emission, and looking for a peak
// there finds one.
Occupancy occupiedBandwidth(const std::vector<double>& magDb, double binHz,
                            double loHz = 80.0, double hiHz = 6000.0);

// A sentence an operator can act on, or an empty string when there is
// nothing worth saying. Deliberately not a verdict on taste — it speaks
// only when the number affects somebody else.
QString advice(const Occupancy& occ, double filterHighHz);

} // namespace Longpath::TxSpectrumAnalysis
