// =================================================================
// src/gui/spectrum/WaterfallTimeMarkers.cpp  (Longpath)
// =================================================================
//
// Longpath-original file. See WaterfallTimeMarkers.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "gui/spectrum/WaterfallTimeMarkers.h"

#include <QDateTime>

namespace Longpath::WaterfallTimeMarkers {

namespace {

qint64 offsetMsAt(qint64 utcMs, const QTimeZone& zone)
{
    return static_cast<qint64>(
               QDateTime::fromMSecsSinceEpoch(utcMs, zone).offsetFromUtc()) * 1000;
}

qint64 floorDiv(qint64 a, qint64 b)
{
    return a >= 0 ? a / b : (a - b + 1) / b;
}

} // namespace

qint64 bucketOf(qint64 utcMs, int intervalSec, const QTimeZone& zone)
{
    const qint64 intervalMs = static_cast<qint64>(intervalSec) * 1000;
    // Boundaries sit on the wall clock of `zone`: shift into local
    // milliseconds first, then floor. The offset is whole minutes, so a
    // 15-minute mark lands on :00/:15/:30/:45 local, and on the UTC clock
    // for UTC.
    return floorDiv(utcMs + offsetMsAt(utcMs, zone), intervalMs);
}

QString labelFor(qint64 bucket, int intervalSec, const QTimeZone& zone)
{
    const qint64 intervalMs = static_cast<qint64>(intervalSec) * 1000;
    const qint64 localMs = bucket * intervalMs;
    // Back from local milliseconds to UTC for the formatter; the offset at
    // the boundary itself, so a DST switch inside the interval formats the
    // boundary's own wall-clock time.
    qint64 utcMs = localMs - offsetMsAt(localMs, zone);
    utcMs = localMs - offsetMsAt(utcMs, zone);
    const QDateTime at = QDateTime::fromMSecsSinceEpoch(utcMs, zone);
    return at.toString(intervalSec >= 60 ? QStringLiteral("HH:mm")
                                         : QStringLiteral("HH:mm:ss"));
}

QVector<WaterfallTimeMarker> compute(const QVector<qint64>& rowTimestampsMs,
                                     int intervalSec, const QTimeZone& zone,
                                     int minLabelGapRows)
{
    QVector<WaterfallTimeMarker> out;
    const int rows = rowTimestampsMs.size();
    if (intervalSec <= 0 || rows < 2) {
        return out;
    }
    // Walk upwards from the oldest visible row: a boundary lies between a
    // row and the older row beneath it.
    qint64 below = rowTimestampsMs.at(rows - 1);
    qint64 belowBucket = below > 0 ? bucketOf(below, intervalSec, zone) : 0;
    for (int y = rows - 2; y >= 0; --y) {
        const qint64 ts = rowTimestampsMs.at(y);
        if (ts <= 0) {
            continue;
        }
        const qint64 bucket = bucketOf(ts, intervalSec, zone);
        if (below > 0 && bucket != belowBucket) {
            WaterfallTimeMarker m;
            m.row = y;
            m.label = labelFor(bucket, intervalSec, zone);
            out.prepend(m);
        }
        below = ts;
        belowBucket = bucket;
    }
    // Labels top-down: a label too close under the one above it stays
    // silent (its line is still drawn).
    int lastShownRow = -1000000;
    for (WaterfallTimeMarker& m : out) {
        m.labelShown = (m.row - lastShownRow) >= minLabelGapRows;
        if (m.labelShown) {
            lastShownRow = m.row;
        }
    }
    return out;
}

} // namespace Longpath::WaterfallTimeMarkers
