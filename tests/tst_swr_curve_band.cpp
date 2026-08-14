// tests/tst_swr_curve_band.cpp  (NereusSDR)
//
// NereusSDR-original. No Thetis port.
//
// ── The window that stayed on the previous band ──────────────────────
//
// Bench, 2026-08-14. A 40 m sweep ran; the window defaulted its target
// to 7.100 MHz. A 20 m sweep followed. The curve drew 14.000–14.350
// correctly, and everything describing it was still 40 m: the three
// tiles read "not swept" against 7.000 / 7.100 / 7.200, and the line
// under the axis said "40 m · mid 7.100 MHz · 200 kHz wide" underneath
// twenty metres of measured data.
//
// recompute() let a target choose the band, and never asked whether the
// sweep reached that target. Worse, it could not recover on its own:
// AntennaWindow re-defaults the target to the middle of whichever band
// it is shown, so 40 m wrote 7.100 straight back and the next sweep
// found the same stale target waiting for it.
//
// The sweep decides which bands are in play. A target may only choose
// among them.

#include <QtTest>

#include "gui/widgets/SwrCurveWidget.h"
#include "core/antenna/Touchstone.h"

#include <QImage>

#include <cmath>
#include <complex>

using namespace NereusSDR;

namespace {

/// A flat sweep across [loHz, hiHz] at a harmless SWR. The band logic
/// does not care what the curve does, only where it is.
Sweep flatSweep(double loHz, double hiHz, const QString& source)
{
    Sweep s;
    s.source = source;
    s.magnitudeOnly = true;
    constexpr int kN = 21;
    for (int i = 0; i < kN; ++i) {
        SweepPoint p;
        p.freqHz = loHz + (hiHz - loHz) * i / (kN - 1);
        p.gamma  = std::complex<double>(0.1, 0.0);   // SWR ≈ 1.22
        s.points.append(p);
    }
    return s;
}

/// Render the sweep and count pixels differing from the background in
/// the column at atHz. The absolute number is meaningless — grid lines
/// cross every column — so callers must only ever compare two of these
/// against each other.
int inkAt(const Sweep& s, double atHz, double sweepLoHz, double sweepHiHz)
{
    SwrCurveWidget w;
    w.setRegion(AmateurBands::Region::One);
    w.resize(800, 420);
    w.setSweep(s);

    QImage img(w.size(), QImage::Format_ARGB32);
    w.render(&img);

    // Mirror of recompute()'s 12 % margin and paintEvent's plot rect.
    // If either changes this lands in the wrong column and the two
    // counts converge, which the caller's assertion catches.
    const double pad = (sweepHiHz - sweepLoHz) * 0.12;
    const double lo  = std::max(0.0, sweepLoHz - pad);
    const double hi  = sweepHiHz + pad;
    const int x = int(44.0 + (atHz - lo) / (hi - lo) * ((800.0 - 12.0) - 44.0));
    if (x <= 44 || x >= 788) { return -1; }

    const QRgb bg = img.pixel(x, 40);
    int ink = 0;
    for (int y = 40; y < img.height() - 60; ++y) {
        if (img.pixel(x, y) != bg) { ++ink; }
    }
    return ink;
}

} // namespace

class TestSwrCurveBand : public QObject
{
    Q_OBJECT

private slots:
    void bandFollowsTheSweep()
    {
        SwrCurveWidget w;
        w.setRegion(AmateurBands::Region::One);
        w.setSweep(flatSweep(14.0e6, 14.35e6, QStringLiteral("20m")));
        QCOMPARE(w.shownBand().name, QStringLiteral("20 m"));
    }

    void aTargetInsideTheSweepStillChoosesTheBand()
    {
        // The behaviour the guard must not break: on a wide end-fed
        // sweep the target is how the operator says which band he means.
        SwrCurveWidget w;
        w.setRegion(AmateurBands::Region::One);
        w.setSweep(flatSweep(3.0e6, 30.0e6, QStringLiteral("endfed")));
        w.setTargetHz(7.1e6);
        QCOMPARE(w.shownBand().name, QStringLiteral("40 m"));
    }

    void aTargetOutsideTheSweepDoesNotChooseTheBand()
    {
        // The bug, exactly as it happened.
        SwrCurveWidget w;
        w.setRegion(AmateurBands::Region::One);
        w.setSweep(flatSweep(7.0e6, 7.2e6, QStringLiteral("40m")));
        w.setTargetHz(7.1e6);
        QCOMPARE(w.shownBand().name, QStringLiteral("40 m"));

        // Now 20 m arrives with the 40 m target still standing.
        w.setSweep(flatSweep(14.0e6, 14.35e6, QStringLiteral("20m")));
        QVERIFY2(w.shownBand().name == QStringLiteral("20 m"),
                 qPrintable(QStringLiteral(
                     "a 20 m sweep was labelled '%1' because the target "
                     "from the previous band was still set")
                         .arg(w.shownBand().name)));
    }

    void theBandIsWithinTheSweepWhicheverWayItIsDriven()
    {
        // Whatever the caller does with targets, the band it shows must
        // be one the measurement actually covers. That is the invariant
        // the window was relying on and the widget was not providing.
        SwrCurveWidget w;
        w.setRegion(AmateurBands::Region::One);
        const double lo = 14.0e6;
        const double hi = 14.35e6;
        for (double target : {0.0, 3.6e6, 7.1e6, 14.2e6, 28.5e6}) {
            w.setSweep(flatSweep(lo, hi, QStringLiteral("20m")));
            w.setTargetHz(target);
            const auto b = w.shownBand();
            if (!b.isValid()) { continue; }
            QVERIFY2(b.highHz >= lo && b.lowHz <= hi,
                     qPrintable(QStringLiteral(
                         "target %1 MHz produced band %2, which the sweep "
                         "does not touch")
                             .arg(target / 1e6).arg(b.name)));
        }
    }

    // ── The gap must stay a gap ──────────────────────────────────────
    //
    // A range sweep has holes: the spectrum between the bands is not
    // ours, so no point exists there. Drawn as one polyline it becomes
    // a straight stroke from 7.200 to 14.000 MHz — full confidence
    // across almost seven megahertz nobody measured.
    //
    // These check the decision. gapThresholdHz is the whole of it, and
    // it is arithmetic, so it can be checked exactly.
    //
    // A note on how they came to exist. The file failed; I assumed the
    // render-and-count-pixels tests below were the cause, replaced them
    // with these, and it failed again. The pixel tests had been passing
    // the whole time — the failure was somewhere else entirely, and one
    // look at the output would have said so. Both kinds are here now:
    // these because they are exact, those because they are the only
    // ones that would notice if the drawing stopped happening at all.

    void aBandGapIsFarAboveTheGapThreshold()
    {
        Sweep s = flatSweep(7.0e6, 7.2e6, QStringLiteral("40m"));
        const Sweep upper = flatSweep(14.0e6, 14.35e6, QStringLiteral("20m"));
        s.points.append(upper.points);

        const double threshold = SwrCurveWidget::gapThresholdHz(s);
        QVERIFY(threshold > 0.0);
        QVERIFY(std::isfinite(threshold));

        // Worked out rather than guessed: 41 steps in all — twenty of
        // 10 kHz from the 40 m half, twenty of 17.5 kHz from the 20 m
        // half, and one of 6.8 MHz across the hole. The median is the
        // 17.5, so the threshold is 70 kHz. The hole is ninety-seven
        // times that, which is not a close call.
        //
        // (I first wrote "40 kHz" here from the 40 m spacing alone, and
        // the arithmetic said 70. The assertion below is deliberately
        // loose so it tests the order of magnitude, not my mental
        // arithmetic.)
        QVERIFY2(threshold < 1.0e6,
                 qPrintable(QStringLiteral("threshold came out at %1 kHz")
                                .arg(threshold / 1e3)));

        int breaks = 0;
        for (int i = 1; i < s.points.size(); ++i) {
            const double step = s.points.at(i).freqHz
                                - s.points.at(i - 1).freqHz;
            if (step > threshold) { ++breaks; }
        }
        QCOMPARE(breaks, 1);
    }

    void anOrdinarySweepHasNoBreakAtAll()
    {
        // The one that matters more. A threshold that fires too easily
        // turns every curve into a dotted line, and that failure would
        // look like a rendering quirk rather than a bug.
        const Sweep s = flatSweep(14.0e6, 14.35e6, QStringLiteral("20m"));
        const double threshold = SwrCurveWidget::gapThresholdHz(s);
        for (int i = 1; i < s.points.size(); ++i) {
            const double step = s.points.at(i).freqHz
                                - s.points.at(i - 1).freqHz;
            QVERIFY2(step <= threshold,
                     qPrintable(QStringLiteral(
                         "an even sweep was split at %1 MHz")
                             .arg(s.points.at(i).freqHz / 1e6, 0, 'f', 4)));
        }
    }

    void unevenSpacingAloneDoesNotSplitTheCurve()
    {
        // A file from a VNA need not be evenly spaced. Mild variation
        // must not read as a band gap.
        Sweep s;
        s.source = QStringLiteral("vna");
        double f = 14.0e6;
        for (int i = 0; i < 30; ++i) {
            SweepPoint p;
            p.freqHz = f;
            p.gamma  = std::complex<double>(0.1, 0.0);
            s.points.append(p);
            f += (i % 3 == 0) ? 12000.0 : 9000.0;   // 9–12 kHz jitter
        }
        const double threshold = SwrCurveWidget::gapThresholdHz(s);
        for (int i = 1; i < s.points.size(); ++i) {
            QVERIFY(s.points.at(i).freqHz - s.points.at(i - 1).freqHz
                    <= threshold);
        }
    }

    void tooFewPointsToJudgeMeansNoBreak()
    {
        Sweep s;
        for (double hz : {14.0e6, 14.2e6}) {
            SweepPoint p;
            p.freqHz = hz;
            p.gamma  = std::complex<double>(0.1, 0.0);
            s.points.append(p);
        }
        QVERIFY(!std::isfinite(SwrCurveWidget::gapThresholdHz(s))
                || SwrCurveWidget::gapThresholdHz(s) > 1e12);
    }

    // ── And it must actually reach the paint ─────────────────────────
    //
    // The four above check the decision. None of them would notice if
    // drawBrokenCurve stopped being called at all.
    //
    // The first version of this rendered once and compared the ink in
    // one column against a number I had guessed. That number happened
    // to be right — the test passed, and I removed it anyway on a wrong
    // hypothesis about why the file was failing. Restored, but without
    // the guess: render the same widget twice, once with the hole and
    // once with it filled in, and require strictly less ink in the
    // column when the hole is there. The comparison calibrates itself,
    // so the grid lines that cross that column cancel out.
    void theGapIsActuallyMissingFromThePaint()
    {
        Sweep gapped = flatSweep(7.0e6, 7.2e6, QStringLiteral("40m"));
        gapped.points.append(
            flatSweep(14.0e6, 14.35e6, QStringLiteral("20m")).points);

        // Same span, same ends, no hole — the control.
        Sweep filled = flatSweep(7.0e6, 14.35e6, QStringLiteral("filled"));

        const int inkGapped = inkAt(gapped, 10.6e6, 7.0e6, 14.35e6);
        const int inkFilled = inkAt(filled, 10.6e6, 7.0e6, 14.35e6);

        QVERIFY2(inkFilled > inkGapped,
                 qPrintable(QStringLiteral(
                     "the control drew no more ink at 10.6 MHz than the "
                     "gapped sweep did (%1 vs %2) — this test cannot "
                     "tell the two apart and proves nothing")
                         .arg(inkFilled).arg(inkGapped)));
    }

    void aForcedBandStillWins()
    {
        // A range the operator typed is his own instruction and outranks
        // both the sweep and the target.
        SwrCurveWidget w;
        w.setRegion(AmateurBands::Region::One);
        w.setSweep(flatSweep(14.0e6, 14.35e6, QStringLiteral("20m")));
        AmateurBands::Band typed;
        typed.lowHz  = 14.1e6;
        typed.highHz = 14.2e6;
        typed.name   = QStringLiteral("your range");
        w.setBand(typed);
        QCOMPARE(w.shownBand().name, QStringLiteral("your range"));
    }
};

QTEST_MAIN(TestSwrCurveBand)
#include "tst_swr_curve_band.moc"
