#pragma once

// =================================================================
// src/gui/widgets/MapPoint.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// One place on the Earth, for anything that draws places.
//
// It lives in its own header so the globe does not have to include the
// flat map to learn what a point is. Two views of the same data should
// share the data, not depend on each other.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include <QString>

namespace Longpath {

struct MapPoint {
    double  lat{0.0};
    double  lon{0.0};   // positive east, matching the Maidenhead helpers
    QString label;

    // Picked out in the log's table. Drawn last, in a different colour,
    // and always labelled — the point of marking rows is to find them
    // among the rest, so hiding the rest would answer a different
    // question than the one being asked.
    bool highlight{false};

    // Placed from the DXCC entity rather than from a locator the
    // station gave. Drawn as a ring instead of a filled dot: it is the
    // middle of a country, not a place anyone was, and a map that
    // renders a guess exactly like a measurement is lying quietly.
    bool approximate{false};
};

} // namespace Longpath
