#pragma once

// =================================================================
// src/core/CouplerZero.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Where a directional coupler's ADC sits when nothing is being
// transmitted — measured on the radio in front of you, rather than
// looked up in a table.
//
// ── The problem this replaces ────────────────────────────────────────
//
// Forward and reverse power are scaled from raw ADC counts:
//
//     volts = (raw − adc_cal_offset) / 4095 × refvoltage
//     watts = volts² / bridge_volt
//
// Two of those three constants are properties of the coupler and
// belong in a per-board table: refvoltage and bridge_volt. The third,
// adc_cal_offset, is not. It is the count the ADC reads with no drive
// — a zero — and it varies with the individual board, its temperature
// and its age. Thetis carries a table of them and NereusSDR inherited
// it: 3, 6, 16, 18, 28, 32 counts depending on the model.
//
// A tabled zero is wrong in both directions and neither is harmless.
//
// TOO HIGH and it deletes signal. Measured on an Anvelina Pro 3 on
// 2026-08-14: the board idles at 0 counts, and the table says to
// subtract 28 from the reverse channel. Every reverse reading of 28
// counts or less therefore became exactly zero watts, which became
// exactly SWR 1.00 — a flat floor along the bottom of an 80 m sweep
// where the operator's VNA said 2.5. A whole day went into suspecting
// the coupler, the protocol and the board profile before the arithmetic
// was worked through.
//
// TOO LOW and it invents signal. A board that idles at 30 counts with a
// tabled offset of 3 reports forward power while receiving, which is
// how "residue" readings refill a power meter that was just zeroed.
//
// ── Measuring it instead ─────────────────────────────────────────────
//
// The zero is observable: it is what the ADC reads while the
// transmitter is off, which is most of the time. So watch it.
//
// A LOW PERCENTILE of a window of receive-time samples, not the mean.
// After the carrier drops, the detector takes a moment to fall and the
// board reports decaying residue; a mean folds that residue into the
// zero and biases it upward, which is the failure mode being fixed. The
// floor of a window is what the ADC reads when nothing at all is
// happening, and that is the definition of the zero.
//
// A window rather than an all-time minimum, so a board that warms up —
// or one whose zero was measured once during a cold start — is not
// stuck with a figure from an hour ago.
//
// Nothing is learned while transmitting, and nothing for a short guard
// after transmit ends, because that is exactly when the residue is
// largest.
//
// ── What it does NOT do ──────────────────────────────────────────────
//
// It does not touch bridge_volt or refvoltage. Those are real coupling
// constants that cannot be measured without a reference of known power,
// and the per-board table is the right home for them. This class fixes
// the zero and only the zero — the term that has no business being in a
// table at all.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-14 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QtGlobal>

#include <array>

namespace NereusSDR {

class CouplerZero {
public:
    /// Receive-time samples kept. At the ~50 Hz the telemetry arrives,
    /// 256 is about five seconds — long enough to contain a quiet
    /// moment, short enough to follow a board that drifts.
    static constexpr int kWindow = 256;

    /// How many samples before an answer is offered. Below this the
    /// caller keeps its tabled offset: a zero guessed from three
    /// samples is worse than a zero from a table.
    static constexpr int kMinSamples = 32;

    /// Samples ignored after the carrier drops. The detector falls
    /// slowly and the board reports decaying residue; folding that into
    /// the zero is the bias this class exists to remove. At ~50 Hz, 25
    /// samples is half a second.
    static constexpr int kSettleSamples = 25;

    /// Fraction of the window taken as the zero. Not the outright
    /// minimum — one dropout would pin it forever — and not the median,
    /// which sits above the floor whenever the band is busy. A low
    /// percentile is the floor with one sample of slack.
    static constexpr int kPercentile = 10;

    /// One telemetry sample. `transmitting` must be the wire-level
    /// truth, not an intention: a sample taken while the PA is still
    /// producing is not a zero.
    void observe(quint16 fwdRaw, quint16 revRaw, bool transmitting);

    /// True once enough receive-time samples have been seen.
    bool known() const noexcept { return m_count >= kMinSamples; }

    /// The measured zero, or the caller's fallback while unknown.
    quint16 forwardZero(quint16 fallback) const;
    quint16 reverseZero(quint16 fallback) const;

    /// Forget everything. For a reconnection or a board change — a zero
    /// measured on one radio says nothing about another.
    void reset() noexcept;

    int sampleCount() const noexcept { return m_count; }

private:
    quint16 percentileOf(const std::array<quint16, kWindow>& ring) const;

    std::array<quint16, kWindow> m_fwd{};
    std::array<quint16, kWindow> m_rev{};
    int m_head{0};
    int m_count{0};
    int m_sinceTx{kSettleSamples};   // start ready, not mid-settle
};

} // namespace NereusSDR
