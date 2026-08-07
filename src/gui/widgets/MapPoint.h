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

namespace NereusSDR {

struct MapPoint {
    double  lat{0.0};
    double  lon{0.0};   // positive east, matching the Maidenhead helpers
    QString label;
};

} // namespace NereusSDR
