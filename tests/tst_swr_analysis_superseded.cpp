// tests/tst_swr_analysis_superseded.cpp  (NereusSDR)
//
// NereusSDR-original. No Thetis port.
//
// ── Two halves of one page, both current, about different bands ──────
//
// 2026-08-15. During a running sweep the head of the antenna window
// named the band being measured while the curve, the tiles, the usable
// span and the band table below it all still described the previous
// one — and nothing on screen said so. Seventeen seconds at 51 points,
// half a minute at 99.
//
// The cause was structural rather than a missing call. The analysis had
// exactly one input, AntennaWindow::setSweep(), reached only from
// SwrSweepPanel::analysisReady, which is emitted when a sweep FINISHES.
// There was no state for "measured, real, and no longer about this
// band", so the window could not have shown one.
//
// What is tested here is that third state: it arrives on a start for a
// different band, it does NOT arrive for the same band, a run that
// measures nothing leaves it in place for good, and a real measurement
// is the only thing that clears it. Plus the rule that says which ink
// steps back — because the two colours that must not are a safety
// matter, not a style one.

#include <QtTest>

#include "gui/AntennaWindow.h"
#include "gui/widgets/SwrCurveWidget.h"
#include "gui/widgets/SwrSweepPanel.h"
#include "core/antenna/Touchstone.h"

#include <complex>

using namespace NereusSDR;

namespace {

/// A flat sweep across [loHz, hiHz]. Eleven points, because that is what
/// a band sweep from the radio actually delivers per band.
Sweep flatSweep(const QString& name, double loHz, double hiHz,
                double gammaMag = 0.1)
{
    Sweep s;
    s.source = name;
    s.magnitudeOnly = true;
    constexpr int kN = 11;
    for (int i = 0; i < kN; ++i) {
        SweepPoint p;
        p.freqHz = loHz + (hiHz - loHz) * i / (kN - 1);
        p.gamma  = std::complex<double>(gammaMag, 0.0);
        s.points.append(p);
    }
    return s;
}

constexpr double k20mLo = 14.000e6, k20mHi = 14.350e6;
constexpr double k80mLo =  3.500e6, k80mHi =  3.800e6;

} // namespace

class TestSwrAnalysisSuperseded : public QObject
{
    Q_OBJECT

private slots:

    // ── The overlap rule, on its own ─────────────────────────────────

    void theSameBandTwiceIsTheSameBand()
    {
        QVERIFY(AntennaWindow::sameBandSpan(k20mLo, k20mHi,
                                            k20mLo, k20mHi));
        // A second run of the same band with a slightly different span —
        // more points, a guard that clipped one end — is still 20 m.
        QVERIFY(AntennaWindow::sameBandSpan(k20mLo, k20mHi,
                                            14.050e6, 14.300e6));
    }

    void anotherBandIsNot()
    {
        QVERIFY(!AntennaWindow::sameBandSpan(k20mLo, k20mHi,
                                             k80mLo, k80mHi));
        QVERIFY(!AntennaWindow::sameBandSpan(k80mLo, k80mHi,
                                             k20mLo, k20mHi));
    }

    void anEmptyRangeIsNeverTheSameBand()
    {
        // Guards the "nothing measured yet" path: a zero-width analysis
        // must not be judged to match whatever starts next.
        QVERIFY(!AntennaWindow::sameBandSpan(0.0, 0.0, k20mLo, k20mHi));
        QVERIFY(!AntennaWindow::sameBandSpan(k20mLo, k20mHi, 0.0, 0.0));
    }

    void touchingAtTheEdgeIsNotOverlapping()
    {
        // 20 m ends where the next range begins. Zero overlap, and the
        // test exists because `>= 0` instead of `> 0` would silently
        // call these the same band.
        QVERIFY(!AntennaWindow::sameBandSpan(k20mLo, k20mHi,
                                             k20mHi, k20mHi + 350e3));
    }

    // ── Which ink may step back ──────────────────────────────────────

    void theWarningColoursNeverFade()
    {
        // The hard boundary, as an assertion rather than a promise:
        // docs/design/HAUSSTIL.md §Die Grenze, die kein Design
        // überschreibt — "Das SWR-Rot bleibt kräftig."
        QVERIFY2(!SwrCurveWidget::fadesWhenSuperseded("danger"),
                 "the over-limit SWR red was allowed to fade");
        QVERIFY2(!SwrCurveWidget::fadesWhenSuperseded("measured-border"),
                 "the SWR limit rule was allowed to fade");
    }

    void everythingElseFades()
    {
        for (const char* role : {"accent", "text", "text-secondary",
                                 "text-scale", "border-subtle", "ok",
                                 "measured"}) {
            QVERIFY2(SwrCurveWidget::fadesWhenSuperseded(role),
                     QByteArray("role did not step back: ")
                         .append(role).constData());
        }
    }

    // ── The state, through the real wiring ───────────────────────────

    void aSweepForAnotherBandSupersedesTheAnalysis()
    {
        AntennaWindow w;
        w.setSweep(flatSweep(QStringLiteral("20m · 09:14 (Funkgerät)"),
                             k20mLo, k20mHi));

        auto* curve = w.findChild<SwrCurveWidget*>();
        QVERIFY(curve);
        QVERIFY2(!curve->isSuperseded(),
                 "a fresh measurement arrived already superseded");

        emit w.sweepPanel()->sweepStartedFor(QStringLiteral("80m"),
                                             k80mLo, k80mHi);

        QVERIFY2(curve->isSuperseded(),
                 "an 80 m sweep started and the 20 m analysis went on "
                 "presenting itself as current");
    }

    void theCurveIsStillThereAndCarriesItsOwnBand()
    {
        AntennaWindow w;
        w.setSweep(flatSweep(QStringLiteral("20m · 09:14 (Funkgerät)"),
                             k20mLo, k20mHi));
        auto* curve = w.findChild<SwrCurveWidget*>();
        QVERIFY(curve);
        const int pointsBefore = curve->sweep().points.size();

        emit w.sweepPanel()->sweepStartedFor(QStringLiteral("80m"),
                                             k80mLo, k80mHi);

        // Not cleared. The measurement is real and worth comparing
        // against; only its claim to be current is withdrawn.
        QCOMPARE(curve->sweep().points.size(), pointsBefore);
        QCOMPARE(curve->targetDim(), SwrCurveWidget::kSupersededDim);

        // And it names the band the numbers belong to — 20 m — not the
        // one being measured. Naming the latter would be the confusion
        // this exists to end.
        QVERIFY2(curve->supersededBand().contains(QStringLiteral("20")),
                 qPrintable(QStringLiteral("capsule named '%1'")
                                .arg(curve->supersededBand())));
    }

    void aSweepForTheSameBandLeavesItAlone()
    {
        AntennaWindow w;
        w.setSweep(flatSweep(QStringLiteral("20m · 09:14 (Funkgerät)"),
                             k20mLo, k20mHi));
        auto* curve = w.findChild<SwrCurveWidget*>();
        QVERIFY(curve);

        emit w.sweepPanel()->sweepStartedFor(QStringLiteral("20m"),
                                             k20mLo, k20mHi);

        // Operator decision, 2026-08-15: re-measuring the band you are
        // already looking at is not a reason to step the picture back
        // one second before it is replaced anyway.
        QVERIFY2(!curve->isSuperseded(),
                 "a second 20 m sweep dimmed the 20 m analysis");
    }

    void aRunThatMeasuredNothingLeavesItSupersededForGood()
    {
        AntennaWindow w;
        w.setSweep(flatSweep(QStringLiteral("20m · 09:14 (Funkgerät)"),
                             k20mLo, k20mHi));
        auto* curve = w.findChild<SwrCurveWidget*>();
        QVERIFY(curve);

        // Starts on 80 m, then the coupler reads nothing: no
        // analysisReady is emitted for a run with no valid points, so
        // nothing arrives to clear the mark.
        emit w.sweepPanel()->sweepStartedFor(QStringLiteral("80m"),
                                             k80mLo, k80mHi);
        QVERIFY(curve->isSuperseded());

        // Time passes, the operator reads the failure message at the
        // top. The curve must not have quietly re-asserted itself.
        QVERIFY2(curve->isSuperseded(),
                 "a failed run let the old band look current again");
    }

    void arealMeasurementIsTheWayBack()
    {
        AntennaWindow w;
        w.setSweep(flatSweep(QStringLiteral("20m · 09:14 (Funkgerät)"),
                             k20mLo, k20mHi));
        auto* curve = w.findChild<SwrCurveWidget*>();
        QVERIFY(curve);

        emit w.sweepPanel()->sweepStartedFor(QStringLiteral("80m"),
                                             k80mLo, k80mHi);
        QVERIFY(curve->isSuperseded());

        w.setSweep(flatSweep(QStringLiteral("80m · 09:15 (Funkgerät)"),
                             k80mLo, k80mHi));

        QVERIFY2(!curve->isSuperseded(),
                 "the 80 m measurement arrived and the window stayed "
                 "dimmed");
        QCOMPARE(curve->targetDim(), 1.0);
    }

    void nothingOnScreenMeansNothingToSupersede()
    {
        AntennaWindow w;
        auto* curve = w.findChild<SwrCurveWidget*>();
        QVERIFY(curve);

        emit w.sweepPanel()->sweepStartedFor(QStringLiteral("80m"),
                                             k80mLo, k80mHi);

        // An empty window has no claim to withdraw, and putting a
        // capsule on an empty frame would be furniture.
        QVERIFY(!curve->isSuperseded());
    }
};

QTEST_MAIN(TestSwrAnalysisSuperseded)
#include "tst_swr_analysis_superseded.moc"
