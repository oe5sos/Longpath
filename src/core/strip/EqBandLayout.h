#pragma once

// =================================================================
// src/core/strip/EqBandLayout.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Where the equaliser handles sit, and how many there are.
//
// ── The problem this fixes ───────────────────────────────────────────
//
// The three starting points each set seven bands, and four of those are
// not shaping controls: a high-pass and three mains notches that ship
// switched off. What the operator can actually grab and move is a low
// shelf, ONE peak and a high shelf.
//
// Three handles over the whole voice. Enough to tilt a voice, not
// enough to correct a resonance without dragging its neighbours with
// it — which is the difference between fixing a problem and hiding it.
//
// ── Fourteen, and why the new ones are inert ─────────────────────────
//
// Seven more peaks, spread through the voice range at roughly half-
// octave spacing. They arrive at 0 dB and DISABLED.
//
// That is the property that makes this safe: the three presets are
// tuned, and adding handles must not change how any of them sounds. A
// band at unity gain with `enabled` false is not a filter that happens
// to cancel — it is a handle waiting to be grabbed, and the editor
// switches it on the first time it is touched.
//
// ── Why it is in core and not beside the panel ───────────────────────
//
// Two callers need it: StripSettings, for the presets, and the reset
// button in the ported equaliser panel. The panel is in gui/ and
// StripSettings is in core/, so the table has to live down here or be
// written twice — and written twice is how the reset button and the
// presets start disagreeing about how many handles exist.
//
// ClientEq allows 24. Fourteen leaves ten free on purpose: double-
// clicking the curve adds a band, and an operator who has used every
// slot before touching anything has been given a worse tool.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-11 — Created in C++20 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/ClientEq.h"

namespace NereusSDR::EqBandLayout {

// 0        high-pass
// 1 - 3    mains notches, off by default
// 4        low shelf
// 5        the one peak the presets always had
// 6        high shelf
// 7 - 13   the shaping peaks added 2026-08-11, flat and off
inline constexpr int kBandCount = 14;

// Where the presets' own six end and the spare handles begin.
inline constexpr int kFirstSpare = 7;

static_assert(kBandCount <= ClientEq::kMaxBands,
              "the layout must fit inside kMaxBands with room left for "
              "bands the operator adds by double-clicking the curve");

// The frequency of spare peak `n`, counting from kFirstSpare.
//
// Half-octave-ish through the voice: the ratios run 1.5, 1.44, 1.46,
// 1.47, 1.79, 1.52, which reads as an even row on a log axis and puts
// the closest spacing where a 2.7 kHz filter actually passes signal.
inline float spareFrequencyHz(int n)
{
    static constexpr float kHz[kBandCount - kFirstSpare] = {
        300.0f, 450.0f, 650.0f, 950.0f, 1400.0f, 2500.0f, 3800.0f,
    };
    if (n < 0 || n >= int(sizeof(kHz) / sizeof(kHz[0]))) { return 1000.0f; }
    return kHz[n];
}

// A spare handle: a flat, disabled peak. Inert until touched.
inline ClientEq::BandParams spare(int n)
{
    ClientEq::BandParams p;
    p.type    = ClientEq::FilterType::Peak;
    p.freqHz  = spareFrequencyHz(n);
    p.q       = 0.707f;   // Butterworth, the neutral default
    p.gainDb  = 0.0f;
    p.enabled = false;
    return p;
}

// The whole layout, for callers with nothing to preserve — the reset
// button. Bands 0-6 follow ClientEq's own defaults so a reset still
// lands somewhere AetherSDR would recognise; 7-13 are the spares.
inline ClientEq::BandParams band(int idx)
{
    if (idx >= kFirstSpare && idx < kBandCount) {
        return spare(idx - kFirstSpare);
    }
    return ClientEq::defaultBand(idx);
}

// Bring an existing equaliser up to the full layout WITHOUT touching a
// single band the operator has already shaped.
//
// ── The function the comments promised and nobody wrote ──────────────
//
// StripWindow.h has described a "seedEqLayout" since the extra handles
// were added — "APPENDS them to an existing chain at 0 dB rather than
// re-seeding, so an operator who had already shaped a curve gets four
// more handles and not a reset". No such function was ever written. The
// only code path that raised the band count was applying one of the
// three presets, so anybody who restored a saved chain and never
// touched the preset box got the old seven handles and a comment
// claiming otherwise.
//
// Reported from the bench as "the several points on the graph are
// missing", which is exactly what it looks like from the outside.
//
// Appends only: existing slots are left exactly as they are, and the
// count only ever goes up. A chain that somehow has MORE bands than
// this layout — the canvas can add them — keeps them.
inline void ensureSeeded(ClientEq& eq)
{
    const int have = eq.activeBandCount();
    if (have >= kBandCount) { return; }
    for (int i = have; i < kBandCount; ++i) {
        eq.setBand(i, band(i));
    }
    eq.setActiveBandCount(kBandCount);
}

} // namespace NereusSDR::EqBandLayout
