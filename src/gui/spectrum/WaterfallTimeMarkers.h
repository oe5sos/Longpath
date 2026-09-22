#pragma once
// =================================================================
// src/gui/spectrum/WaterfallTimeMarkers.h  (Longpath)
// =================================================================
//
// Longpath-original file. Clock-aligned time markers for the waterfall:
// given the timestamps of the visible rows (top row first, newest), the
// rows after a clock boundary of `intervalSec` (every 15 s, minute,
// quarter hour, ...) and the label each one carries. Pure arithmetic,
// no painting, so it is testable against a fixed clock. Idea from
// AetherSDR #5538 ("clock-aligned waterfall time markers", 2026-09-12).
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QString>
#include <QTimeZone>
#include <QVector>

namespace Longpath {

struct WaterfallTimeMarker {
    int     row = 0;        // screen row (0 = top) that sits just after the boundary
    QString label;          // "HH:mm" for minutes and up, "HH:mm:ss" below
    bool    labelShown = true;   // false when a neighbour's label is too close
};

namespace WaterfallTimeMarkers {

// `rowTimestampsMs` is newest first (row 0 at the top of the waterfall);
// 0 means "no timestamp" and never marks. A marker is placed on a row
// whose timestamp falls in a later `intervalSec` bucket of the wall clock
// in `zone` than the (older) row beneath it. Labels closer than
// `minLabelGapRows` to the label above them are kept but not shown.
QVector<WaterfallTimeMarker> compute(const QVector<qint64>& rowTimestampsMs,
                                     int intervalSec, const QTimeZone& zone,
                                     int minLabelGapRows);

// The bucket a UTC millisecond timestamp falls into on the wall clock in
// `zone` (a floor, also for negative values).
qint64 bucketOf(qint64 utcMs, int intervalSec, const QTimeZone& zone);

// The label for a bucket: the boundary's wall-clock time in `zone`.
QString labelFor(qint64 bucket, int intervalSec, const QTimeZone& zone);

} // namespace WaterfallTimeMarkers
} // namespace Longpath
