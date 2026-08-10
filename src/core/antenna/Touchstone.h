#pragma once

// =================================================================
// src/core/antenna/Touchstone.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Reading a one-port sweep out of a Touchstone file.
//
// ── Why this before any NanoVNA driver ───────────────────────────────
//
// Every vector analyser ever sold writes .s1p, including the NanoVNA
// (to its own SD card). So a file reader works with hardware nobody
// here owns, can be tested without plugging anything in, and produces
// exactly the same data structure a serial driver would.
//
// The driver is the easy half and the interesting half is the
// arithmetic. Building the arithmetic first means it can be wrong in
// a test rather than wrong on a summit.
//
// ── The format, and the parts that bite ──────────────────────────────
//
// A comment starts at '!' and runs to the end of the line — ANYWHERE in
// the line, not only at the start. The option line begins with '#':
//
//     # MHZ S RI R 50
//
// and gives the frequency unit, the parameter, the number format and
// the reference impedance. Every field is optional and the defaults are
// the ones nobody expects:
//
//     GHZ S MA R 50
//
// GHz, not Hz. A file with a bare '#' and rows reading "7.05 …" is
// 7.05 GHz by the standard, and reading it as MHz would put a 40 m
// dipole in the microwave region without complaining. So the option
// line is parsed properly rather than guessed at, and a file with no
// option line at all is refused instead of assumed.
//
// Three number formats, and they are not interchangeable:
//
//     RI  real, imaginary
//     MA  linear magnitude, angle in DEGREES
//     DB  magnitude in dB (20·log10), angle in degrees
//
// Reading MA as RI gives a plausible-looking curve that is wrong
// everywhere, which is the worst kind of wrong.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QString>
#include <QVector>

#include <complex>

namespace NereusSDR {

// One measured point: a frequency and the reflection coefficient there.
struct SweepPoint {
    double               freqHz{0.0};
    std::complex<double> gamma;   // S11
};

// A one-port sweep, in ascending frequency order.
struct Sweep {
    QVector<SweepPoint> points;
    double  referenceOhms{50.0};
    QString source;        // file name, or how else it arrived
    QString note;          // why it is empty, when it is

    bool   isEmpty() const { return points.isEmpty(); }
    double startHz() const { return points.isEmpty() ? 0.0
                                                     : points.first().freqHz; }
    double stopHz()  const { return points.isEmpty() ? 0.0
                                                     : points.last().freqHz; }
};

namespace Touchstone {

// Parse .s1p text. On failure the sweep is empty and `note` says what
// was wrong in words an operator can act on — a file that silently
// yields nothing is indistinguishable from an antenna that measures
// nothing.
Sweep parseS1p(const QString& text, const QString& sourceName = {});

// Read from disk. Same contract.
Sweep readS1p(const QString& path);

} // namespace Touchstone
} // namespace NereusSDR
