// tests/tst_swr_curve_panels.cpp  (NereusSDR)
//
// NereusSDR-original. No Thetis port.
//
// ── The layout, checked with arithmetic ──────────────────────────────
//
// A range sweep across HF was unreadable four times running, and each
// time I improved the DRAWING — closed the gaps, smoothed the line,
// tripled the points — when the fault was the AXIS. On a linear
// 1.8-to-30 MHz scale, 40 m is 200 kHz: seven thousandths of the
// width, about ten pixels. Eleven samples fit in ten pixels. A shape
// does not.
//
// It took four rounds because the only check available was "does it
// look right", the operator had to run the sweep to produce the
// picture, and I never see the picture at all.
//
// This is that check turned into arithmetic. The question "does 40 m
// get a ninth of the width or a hundred-and-fortieth" has a number for
// an answer, and a number can be asserted.

#include <QtTest>

#include "gui/widgets/SwrCurveWidget.h"
#include "core/antenna/Touchstone.h"

#include <cmath>
#include <complex>

using namespace Longpath;

namespace {

void appendBand(Sweep& s, double loHz, double hiHz, int n = 11)
{
    for (int i = 0; i < n; ++i) {
        SweepPoint p;
        p.freqHz = loHz + (hiHz - loHz) * i / (n - 1);
        p.gamma  = std::complex<double>(0.1, 0.0);
        s.points.append(p);
    }
}

/// The nine HF bands a 1.8–30 MHz range sweep measures in Region 1.
Sweep hfRangeSweep()
{
    Sweep s;
    s.source = QStringLiteral("range");
    s.magnitudeOnly = true;
    appendBand(s,  1.810e6,  2.000e6);
    appendBand(s,  3.500e6,  3.800e6);
    appendBand(s,  7.000e6,  7.200e6);
    appendBand(s, 10.100e6, 10.150e6);
    appendBand(s, 14.000e6, 14.350e6);
    appendBand(s, 18.068e6, 18.168e6);
    appendBand(s, 21.000e6, 21.450e6);
    appendBand(s, 24.890e6, 24.990e6);
    appendBand(s, 28.000e6, 29.700e6);
    return s;
}

} // namespace

class TestSwrCurvePanels : public QObject
{
    Q_OBJECT

private slots:
    void aContinuousSweepKeepsTheLinearAxis()
    {
        SwrCurveWidget w;
        Sweep s;
        s.magnitudeOnly = true;
        appendBand(s, 14.0e6, 14.35e6, 51);
        w.setSweep(s);
        QVERIFY2(w.viewPanels().isEmpty(),
                 "a single-band sweep was split into panels — it has no "
                 "holes and must keep the plain frequency axis");
    }

    void everyMeasuredBandGetsItsOwnPanel()
    {
        SwrCurveWidget w;
        w.setSweep(hfRangeSweep());
        QCOMPARE(w.viewPanels().size(), 9);
    }

    // ── The one that would have saved four rounds ────────────────────
    void aNarrowBandGetsAFairShareOfTheWidth()
    {
        SwrCurveWidget w;
        w.setSweep(hfRangeSweep());
        const auto panels = w.viewPanels();
        QCOMPARE(panels.size(), 9);

        // On the old linear axis 40 m occupied 200 kHz of a 28.2 MHz
        // span: 0.7 % of the width. Worked out, not recalled:
        const double linearShare = 0.2 / 28.2;
        QVERIFY(linearShare < 0.008);

        for (const auto& p : panels) {
            const double share = p.f1 - p.f0;
            QVERIFY2(share > 0.09,
                     qPrintable(QStringLiteral(
                         "panel %1–%2 MHz got %3 %% of the width; nine "
                         "equal panels are about 10 %% each and the old "
                         "linear axis gave 40 m 0.7 %%")
                             .arg(p.loHz / 1e6, 0, 'f', 3)
                             .arg(p.hiHz / 1e6, 0, 'f', 3)
                             .arg(share * 100.0, 0, 'f', 1)));
            // ...and no panel may hog it either.
            QVERIFY(share < 0.13);
        }
    }

    void panelsAreOrderedAndDoNotOverlap()
    {
        SwrCurveWidget w;
        w.setSweep(hfRangeSweep());
        const auto panels = w.viewPanels();
        QVERIFY(panels.size() > 1);

        for (int i = 0; i < panels.size(); ++i) {
            const auto& p = panels.at(i);
            QVERIFY(p.f1 > p.f0);
            QVERIFY(p.hiHz > p.loHz);
            QVERIFY(p.f0 >= -1e-9);
            QVERIFY(p.f1 <= 1.0 + 1e-9);
            if (i > 0) {
                const auto& prev = panels.at(i - 1);
                QVERIFY2(p.f0 > prev.f1,
                         "panels overlap — a frequency would land in two "
                         "places at once");
                QVERIFY2(p.loHz > prev.hiHz,
                         "panel frequency ranges overlap");
            }
        }
    }

    void thePanelsCoverTheWholeWidthBetweenThem()
    {
        // The gutters are the only thing not covered, and they must be
        // small enough that the picture is curves rather than gaps.
        SwrCurveWidget w;
        w.setSweep(hfRangeSweep());
        const auto panels = w.viewPanels();

        double covered = 0.0;
        for (const auto& p : panels) { covered += p.f1 - p.f0; }
        QVERIFY2(covered > 0.85,
                 qPrintable(QStringLiteral("panels use only %1 %% of the "
                                           "width").arg(covered * 100.0,
                                                        0, 'f', 1)));
        QVERIFY(covered <= 1.0 + 1e-9);
    }

    void eachPanelHoldsTheBandItWasMeasuredOn()
    {
        // A panel's frequency range must actually contain the band it
        // is drawn for, with the small margin the widget adds.
        SwrCurveWidget w;
        w.setRegion(AmateurBands::Region::One);
        w.setSweep(hfRangeSweep());

        const QList<QPair<double, double>> bands = {
            { 1.810e6,  2.000e6}, { 3.500e6,  3.800e6},
            { 7.000e6,  7.200e6}, {10.100e6, 10.150e6},
            {14.000e6, 14.350e6}, {18.068e6, 18.168e6},
            {21.000e6, 21.450e6}, {24.890e6, 24.990e6},
            {28.000e6, 29.700e6},
        };
        const auto panels = w.viewPanels();
        QCOMPARE(panels.size(), bands.size());
        for (int i = 0; i < panels.size(); ++i) {
            QVERIFY2(panels.at(i).loHz <= bands.at(i).first + 1.0
                     && panels.at(i).hiHz >= bands.at(i).second - 1.0,
                     qPrintable(QStringLiteral(
                         "panel %1 spans %2–%3 MHz but was measured over "
                         "%4–%5")
                             .arg(i)
                             .arg(panels.at(i).loHz / 1e6, 0, 'f', 3)
                             .arg(panels.at(i).hiHz / 1e6, 0, 'f', 3)
                             .arg(bands.at(i).first / 1e6, 0, 'f', 3)
                             .arg(bands.at(i).second / 1e6, 0, 'f', 3)));
        }
    }

    // ── The scale must not print a frequency nobody measured ─────────
    //
    // The panel is the measured run plus 6 % of air at each end, so the
    // curve does not touch the break lines. The labels were taken from
    // the PANEL, which put "1.799 – 2.011" under 160 m: one end below
    // the band, the other above it, neither swept, and both illegal to
    // transmit on. A scale that reads out of band beside a transmitting
    // instrument is worse than no scale.
    void thePanelScaleReadsWhatWasMeasured()
    {
        SwrCurveWidget w;
        w.setRegion(AmateurBands::Region::One);
        w.setSweep(hfRangeSweep());

        const QList<QPair<double, double>> measured = {
            { 1.810e6,  2.000e6}, { 3.500e6,  3.800e6},
            { 7.000e6,  7.200e6}, {10.100e6, 10.150e6},
            {14.000e6, 14.350e6}, {18.068e6, 18.168e6},
            {21.000e6, 21.450e6}, {24.890e6, 24.990e6},
            {28.000e6, 29.700e6},
        };
        const auto panels = w.viewPanels();
        QCOMPARE(panels.size(), measured.size());

        for (int i = 0; i < panels.size(); ++i) {
            const auto& p = panels.at(i);
            QVERIFY2(std::abs(p.dataLoHz - measured.at(i).first) < 1.0,
                     qPrintable(QStringLiteral(
                         "panel %1 would label its left end %2 MHz; the "
                         "first point measured there was %3 MHz")
                             .arg(i)
                             .arg(p.dataLoHz / 1e6, 0, 'f', 4)
                             .arg(measured.at(i).first / 1e6, 0, 'f', 4)));
            QVERIFY2(std::abs(p.dataHiHz - measured.at(i).second) < 1.0,
                     qPrintable(QStringLiteral(
                         "panel %1 would label its right end %2 MHz; the "
                         "last point measured there was %3 MHz")
                             .arg(i)
                             .arg(p.dataHiHz / 1e6, 0, 'f', 4)
                             .arg(measured.at(i).second / 1e6, 0, 'f', 4)));
            // ...and the air is real, so the drawn panel is wider.
            QVERIFY(p.loHz < p.dataLoHz);
            QVERIFY(p.hiHz > p.dataHiHz);
        }
    }

    void aTwoBandSweepSplitsInTwo()
    {
        // The smallest segmented case, and the one the antenna-window
        // tests use.
        SwrCurveWidget w;
        Sweep s;
        s.magnitudeOnly = true;
        appendBand(s,  7.0e6,  7.2e6);
        appendBand(s, 28.0e6, 29.7e6);
        w.setSweep(s);

        const auto panels = w.viewPanels();
        QCOMPARE(panels.size(), 2);
        // Two panels, so each gets nearly half — on the linear axis
        // 40 m would have had 200 kHz of a 22.7 MHz span, under 1 %.
        for (const auto& p : panels) {
            QVERIFY(p.f1 - p.f0 > 0.45);
        }
    }
};

QTEST_MAIN(TestSwrCurvePanels)
#include "tst_swr_curve_panels.moc"
