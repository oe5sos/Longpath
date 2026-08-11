#pragma once

// =================================================================
// src/core/antenna/AmateurBands.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Where the bands begin and end, so a sweep can be drawn against
// something real.
//
// ── Why this is not the table already in DxccColorProvider ───────────
//
// That one exists to guess a mode from a spot frequency, and it is
// written to Region 2 edges: 40 m running to 7.300, 80 m to 4.000,
// 160 m from 1.800.
//
// For an operator in Region 1 those numbers are wrong in the direction
// that matters. Drawing a 40 m band edge at 7.300 puts a hundred
// kilohertz of "your antenna is fine here" over spectrum they may not
// transmit in. An antenna tool has to know which set it is using and
// say so.
//
// ── This is a band PLAN, not a licence ───────────────────────────────
//
// The IARU plan is what the three regions agreed among themselves.
// National allocations differ — a licence class may be narrower, and a
// country may have more or less than the plan says. So the region is a
// setting rather than a constant, and anything drawn from this is
// labelled with the region it came from. Nobody should read a green
// bar here as permission.
//
// Region 1 edges checked against the IARU R1 HF band plan, August 2026.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QString>
#include <QVector>

namespace NereusSDR::AmateurBands {

enum class Region {
    One   = 1,   // Europe, Africa, Middle East, northern Asia — OE is here
    Two   = 2,   // the Americas
    Three = 3,   // Asia-Pacific
};

struct Band {
    double  lowHz{0.0};
    double  highHz{0.0};
    QString name;        // "40 m"

    bool   isValid()  const { return highHz > lowHz; }
    double centreHz() const { return (lowHz + highHz) * 0.5; }
    double widthHz()  const { return highHz - lowHz; }
    bool   contains(double hz) const { return hz >= lowHz && hz <= highHz; }
};

// Every band in a region, in ascending frequency order.
const QVector<Band>& forRegion(Region r);

// The band a frequency falls in, or an invalid Band when it falls
// between bands. Deliberately not "the nearest band" — a frequency in
// the gap is in no band at all, and rounding it into one would draw an
// edge that is not there.
Band containing(double hz, Region r = Region::One);

// The band a sweep is mostly about: the one it overlaps by the largest
// span. A NanoVNA sweep usually straddles a band with margin either
// side, so "which band is this" needs overlap, not containment.
// Invalid when the sweep touches no band.
Band bestOverlap(double startHz, double stopHz, Region r = Region::One);

// Every band a sweep touches, in frequency order.
//
// An end-fed half-wave is swept across the whole of HF at once, and the
// question its owner has is not "how is 40 m" but "where are all the
// resonances and do they land in bands". bestOverlap() answers the
// first and cannot answer the second.
QVector<Band> allOverlapping(double startHz, double stopHz,
                             Region r = Region::One);

QString regionName(Region r);

} // namespace NereusSDR::AmateurBands
