#pragma once

// =================================================================
// src/core/KmlExport.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// The logbook as a KML document, for Google Earth. The native map
// answers questions inside NereusSDR; this answers "show me my log on
// the best imagery there is", which is Google's, and Google Earth is
// the only way to it — the imagery cannot be embedded here.
//
// Shape of the document, chosen so Google Earth's own controls do the
// filtering the operator asked for:
//
//   - One folder per band, so the sidebar's checkboxes ARE the band
//     filter. Folder names carry the contact count.
//   - One placemark per contact, with a <TimeStamp>, so Google Earth's
//     time slider IS the time filter.
//   - Each placemark is a MultiGeometry of the point and the great-
//     circle line from home (tessellated, so Google Earth bends it
//     over the globe), and the description carries the OM's data —
//     name, QTH, locator, band, mode, reports, distance, bearing.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "models/LogEntry.h"

#include <QString>
#include <QVector>

#include <functional>

namespace Longpath {

namespace KmlExport {

struct Options {
    // Own locator. With it, every contact gets a great-circle line from
    // home; without it, pins only.
    QString myGrid;

    // Last resort for a contact with no locator: the centre of its DXCC
    // entity. Same contract as QsoMapWindow::PositionFallback, injected
    // for the same reason — cty.dat lives with the radio model.
    std::function<bool(const QString& call, double& lat, double& lon)>
        fallback;
};

struct Result {
    QString kml;      // the document; empty on placed == 0
    int placed{0};    // contacts that made it in
    int skipped{0};   // contacts with no locator and no fallback answer
};

// Build the document. Never touches the filesystem.
Result toKml(const QVector<LogEntry>& entries, const Options& opt);

// Build and write. False (with *error set) on I/O failure or when
// nothing could be placed — an empty KML that opens as a blank Earth
// would read as "the export is broken", not as "the log has no
// locators".
bool writeKml(const QString& path, const QVector<LogEntry>& entries,
              const Options& opt, QString* error = nullptr,
              Result* result = nullptr);

} // namespace KmlExport

} // namespace Longpath
