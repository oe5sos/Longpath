// =================================================================
// tests/tst_waterfall_time_markers.cpp  (Longpath)
// =================================================================
//
// Longpath-original test. WaterfallTimeMarkers against a fixed clock:
//   * a boundary lies between a row and the older row beneath it; the
//     marker sits on the newer row and carries the boundary's time
//   * seconds intervals label HH:mm:ss, minutes HH:mm
//   * the label follows the zone (13:15 UTC reads 15:15 in Vienna); the
//     rows do not, since every zone offset is a whole quarter hour
//   * rows without a timestamp never mark and never break a run
//   * labels closer than the gap stay silent, their lines remain
//   * off / too few rows give nothing
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QDateTime>
#include <QTimeZone>

#include "gui/spectrum/WaterfallTimeMarkers.h"
#include "gui/SpectrumWidget.h"

#include <QImage>
#include <QPainter>

using namespace Longpath;

namespace {

qint64 utcMs(int y, int mo, int d, int h, int mi, int s, int ms = 0)
{
    return QDateTime(QDate(y, mo, d), QTime(h, mi, s, ms), QTimeZone::UTC).toMSecsSinceEpoch();
}

// Rows newest first: row 0 is `newest`, each following row `stepMs` older.
QVector<qint64> rows(qint64 newest, int count, qint64 stepMs)
{
    QVector<qint64> v(count);
    for (int i = 0; i < count; ++i) { v[i] = newest - i * stepMs; }
    return v;
}

} // namespace

class TestWaterfallTimeMarkers : public QObject {
    Q_OBJECT

private slots:
    void boundaryMarksTheNewerRowWithTheBoundaryTime()
    {
        const QTimeZone utc(QTimeZone::UTC);
        // 100 rows, 1 s apart, newest at 12:00:41.500: boundaries at
        // 12:00:30 (row 11.5 -> the row at 12:00:30.5 is row 11) and
        // 12:00:00 (row 41), 11:59:30 (row 71).
        const QVector<qint64> ts = rows(utcMs(2026, 9, 21, 12, 0, 41, 500), 100, 1000);
        const auto marks = WaterfallTimeMarkers::compute(ts, 30, utc, 0);
        QCOMPARE(marks.size(), 3);
        QCOMPARE(marks.at(0).row, 11);
        QCOMPARE(marks.at(0).label, QStringLiteral("12:00:30"));
        QCOMPARE(marks.at(1).row, 41);
        QCOMPARE(marks.at(1).label, QStringLiteral("12:00:00"));
        QCOMPARE(marks.at(2).row, 71);
        QCOMPARE(marks.at(2).label, QStringLiteral("11:59:30"));
        for (const auto& m : marks) { QVERIFY(m.labelShown); }
    }

    void minutesLabelWithoutSeconds()
    {
        const QTimeZone utc(QTimeZone::UTC);
        const QVector<qint64> ts = rows(utcMs(2026, 9, 21, 12, 3, 10), 400, 1000);   // 6 min 40 s
        // 11:56:31 ... 12:03:10 holds seven minute boundaries, 11:57 ... 12:03.
        const auto marks = WaterfallTimeMarkers::compute(ts, 60, utc, 0);
        QCOMPARE(marks.size(), 7);
        QCOMPARE(marks.first().label, QStringLiteral("12:03"));
        QCOMPARE(marks.first().row, 10);       // 12:03:00 is 10 s below the top
        QCOMPARE(marks.last().label, QStringLiteral("11:57"));
        // 5 min: only 12:00 and 11:55 (if within the window: 12:03:10 - 400 s = 11:56:30 -> no)
        const auto five = WaterfallTimeMarkers::compute(ts, 300, utc, 0);
        QCOMPARE(five.size(), 1);
        QCOMPARE(five.first().label, QStringLiteral("12:00"));
        QCOMPARE(five.first().row, 190);
    }

    void theLabelFollowsTheZone()
    {
        // Every zone offset is a whole number of quarter hours, so up to the
        // 15-minute interval the boundaries fall on the same rows in every
        // zone -- what differs is the label. Vienna in summer = UTC+2:
        // 13:15 UTC reads 15:15 there.
        const QTimeZone vienna("Europe/Vienna");
        QVERIFY(vienna.isValid());
        const QVector<qint64> ts = rows(utcMs(2026, 9, 21, 13, 15, 10), 21, 1000);
        const auto local = WaterfallTimeMarkers::compute(ts, 900, vienna, 0);
        QCOMPARE(local.size(), 1);
        QCOMPARE(local.first().row, 10);
        QCOMPARE(local.first().label, QStringLiteral("15:15"));
        const auto utc = WaterfallTimeMarkers::compute(ts, 900, QTimeZone(QTimeZone::UTC), 0);
        QCOMPARE(utc.size(), 1);
        QCOMPARE(utc.first().row, 10);
        QCOMPARE(utc.first().label, QStringLiteral("13:15"));
    }

    void rowsWithoutTimestampAreSkipped()
    {
        const QTimeZone utc(QTimeZone::UTC);
        QVector<qint64> ts = rows(utcMs(2026, 9, 21, 12, 0, 5), 10, 1000);   // boundary between rows 5 and 6
        ts[5] = 0;    // the row that would carry the mark has no stamp
        ts[6] = 0;    // nor the one beneath it
        const auto marks = WaterfallTimeMarkers::compute(ts, 15, utc, 0);
        // 12:00:04 (row 1) vs 11:59:58 (row 7): the boundary is found across
        // the gap and lands on the first stamped row above it.
        QCOMPARE(marks.size(), 1);
        QCOMPARE(marks.first().row, 4);
        QCOMPARE(marks.first().label, QStringLiteral("12:00:00"));
        QVERIFY(WaterfallTimeMarkers::compute(QVector<qint64>(10, 0), 15, utc, 0).isEmpty());
    }

    void closeLabelsStaySilent()
    {
        const QTimeZone utc(QTimeZone::UTC);
        // A fast waterfall: 4 rows per second, 15 s marks are 60 rows apart;
        // with a gap of 80 rows every second label is silent.
        const QVector<qint64> ts = rows(utcMs(2026, 9, 21, 12, 1, 0, 100), 300, 250);
        const auto marks = WaterfallTimeMarkers::compute(ts, 15, utc, 80);
        QVERIFY(marks.size() >= 4);
        QVERIFY(marks.at(0).labelShown);
        QVERIFY(!marks.at(1).labelShown);
        QVERIFY(marks.at(2).labelShown);
        QVERIFY(!marks.at(3).labelShown);
    }

    // The painted result: lines and labels on a dark strip, saved as PNG
    // when LONGPATH_GRAB_DIR is set (the picture for the PR).
    void paintsLinesAndLabels()
    {
        SpectrumWidget w;
        w.setWfTimestampMode(SpectrumWidget::TimestampMode::UTC);
        w.setWfTimeMarkerSeconds(30);
        QCOMPARE(w.wfTimeMarkerSeconds(), 30);
        w.setWfTimeMarkerSeconds(7);            // not a choice -> off
        QCOMPARE(w.wfTimeMarkerSeconds(), 0);
        w.setWfTimeMarkerSeconds(30);

        QImage img(600, 200, QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(8, 8, 10));
        const QRect wf(0, 0, 600, 200);
        // 200 rows at 4 rows/s = 50 s: boundaries every 30 s -> one or two.
        const QVector<qint64> ts = rows(utcMs(2026, 9, 21, 12, 0, 41, 500), 200, 250);
        {
            QPainter p(&img);
            w.paintWaterfallTimeMarkersForTest(p, wf, ts);
        }
        // The 12:00:30 boundary sits 11.5 s = 46 rows below the top: a line
        // of the marker colour runs across at row 46.
        int lit = 0;
        for (int x = 0; x < img.width(); ++x) {
            if (img.pixelColor(x, 46) != QColor(8, 8, 10)) { ++lit; }
        }
        QVERIFY2(lit > img.width() / 2, qPrintable(QStringLiteral("only %1 px lit on the marker row").arg(lit)));
        // Rows without a marker stay dark (row 100: 12:00:16.5, no boundary).
        int litOff = 0;
        for (int x = 0; x < img.width(); ++x) {
            if (img.pixelColor(x, 100) != QColor(8, 8, 10)) { ++litOff; }
        }
        QCOMPARE(litOff, 0);
        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!grabDir.isEmpty()) {
            QVERIFY(img.save(grabDir + QStringLiteral("/longpath-grab-WaterfallTimeMarkers.png")));
        }
    }

    void offAndTooFewRowsGiveNothing()
    {
        const QTimeZone utc(QTimeZone::UTC);
        const QVector<qint64> ts = rows(utcMs(2026, 9, 21, 12, 0, 5), 10, 1000);
        QVERIFY(WaterfallTimeMarkers::compute(ts, 0, utc, 0).isEmpty());
        QVERIFY(WaterfallTimeMarkers::compute(ts.mid(0, 1), 15, utc, 0).isEmpty());
        QCOMPARE(WaterfallTimeMarkers::bucketOf(utcMs(2026, 9, 21, 12, 0, 14, 999), 15, utc),
                 WaterfallTimeMarkers::bucketOf(utcMs(2026, 9, 21, 12, 0, 0), 15, utc));
        QCOMPARE(WaterfallTimeMarkers::bucketOf(utcMs(2026, 9, 21, 12, 0, 15), 15, utc),
                 WaterfallTimeMarkers::bucketOf(utcMs(2026, 9, 21, 12, 0, 0), 15, utc) + 1);
    }
};

QTEST_MAIN(TestWaterfallTimeMarkers)
#include "tst_waterfall_time_markers.moc"
