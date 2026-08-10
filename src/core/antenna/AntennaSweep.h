#pragma once

// =================================================================
// src/core/antenna/AntennaSweep.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// What a measured sweep says about the antenna.
//
// ── Resonance is not the SWR minimum ─────────────────────────────────
//
// This is the whole reason the module exists, and the reason a vector
// analyser is worth carrying up a hill when an SWR meter weighs less.
//
//   resonant  means  X = 0        — a fact about the LENGTH of the wire
//   matched   means  Z ≈ 50 Ω     — a fact about the FEED POINT
//
// They are different conditions and they generally happen at different
// frequencies. On an antenna whose feed resistance rises across the
// band, the SWR minimum sits where R passes through 50, which can be
// tens of kilohertz away from where the reactance vanishes.
//
// An operator who trims to the SWR dip is trimming to the wrong number,
// and the error does not announce itself: the SWR looks excellent and
// the antenna is the wrong length. So both are reported, separately and
// by name.
//
// ── Which zero crossing ──────────────────────────────────────────────
//
// A wide sweep crosses zero several times. They are not equivalent:
//
//   rising   (capacitive → inductive)  series resonance — a half-wave,
//                                      low R, the one you trim to
//   falling  (inductive → capacitive)  anti-resonance — a full wave,
//                                      R in the thousands, useless as a
//                                      feed point
//
// Returning "the nearest crossing" without that distinction would send
// somebody trimming a 40 m dipole towards its 20 m anti-resonance. So
// the direction is recorded and the caller asks for what it wants.
//
// ── Interpolation ────────────────────────────────────────────────────
//
// A NanoVNA sweeping a band with 101 points has samples about 5 kHz
// apart. Reporting the nearest SAMPLE to the crossing rounds the answer
// to that grid, which on 40 m is about 1.5 cm of wire — small, but it
// is free to do better, so the crossing is interpolated linearly
// between the two samples that straddle it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/antenna/Touchstone.h"

#include <QString>
#include <QVector>

namespace NereusSDR::AntennaSweep {

// ── Point arithmetic ────────────────────────────────────────────────

// Standing wave ratio from a reflection coefficient. Clamped: a |Γ| of
// 1 or more is an open, a short, or a measurement that went wrong, and
// the formula divides by zero there.
double swr(const std::complex<double>& gamma);

// Return loss, negative dB. -20 dB is a tenth of the power coming back.
double returnLossDb(const std::complex<double>& gamma);

// Impedance at the reference plane.
std::complex<double> impedance(const std::complex<double>& gamma,
                               double referenceOhms);

// ── Features of a whole sweep ───────────────────────────────────────

struct Crossing {
    bool   found{false};
    double freqHz{0.0};
    double resistanceOhms{0.0};   // R where X vanishes
    double swrThere{0.0};
    // True when the reactance goes from capacitive to inductive — the
    // series resonance. False is the anti-resonance, where R is huge
    // and which is not a thing to trim towards.
    bool   rising{false};
};

// Every zero crossing of the reactance, in frequency order.
QVector<Crossing> resonances(const Sweep& s);

// The series resonance nearest `nearHz`. Pass 0 for the first one in
// the sweep. Anti-resonances are skipped — see the header note.
Crossing nearestResonance(const Sweep& s, double nearHz = 0.0);

struct Minimum {
    bool   found{false};
    double freqHz{0.0};
    double swr{0.0};
};

// Where the SWR is lowest. Named apart from the resonance because
// conflating the two is the mistake this module exists to prevent.
Minimum bestMatch(const Sweep& s);

struct Span {
    bool   found{false};
    double lowHz{0.0};
    double highHz{0.0};
    double widthHz() const { return highHz - lowHz; }
};

// The contiguous run of frequencies around `aroundHz` where the SWR
// stays at or below `limit`. Around a point rather than across the
// whole sweep: a multiband antenna has several such runs and the useful
// answer is the one containing the frequency you care about.
Span usableSpan(const Sweep& s, double limit = 2.0, double aroundHz = 0.0);

// Linear interpolation of SWR at an arbitrary frequency inside the
// sweep. Zero when outside it — asking about a frequency that was never
// measured should not produce a confident number.
double swrAt(const Sweep& s, double freqHz);
std::complex<double> impedanceAt(const Sweep& s, double freqHz);

// A sentence naming what was found and, where they differ, saying so.
QString describe(const Sweep& s, double targetHz);

} // namespace NereusSDR::AntennaSweep
