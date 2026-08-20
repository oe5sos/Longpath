// tests/tst_antenna_window_multiband.cpp  (NereusSDR)
//
// NereusSDR-original. No Thetis port.
//
// ── Four tiles describing one of nine bands ──────────────────────────
//
// A range sweep from 1.8 to 30 MHz measured nine bands, found 1.10 on
// 40 m, and headlined the window with "BAND START 28.000 · BAND END
// 29.700 · USABLE none" — 10 m, in red, because bestOverlap() picks the
// widest overlap and 10 m is 1.7 MHz wide. Everything on screen said
// the antenna was unusable. The good band was in the curve and nowhere
// in the text.
//
// A per-band table had been written for exactly this and was parented
// into the hidden holder that keeps the retired wire-length and coax
// controls. A child of a hidden parent stays hidden however often you
// call setVisible(true) on it, so refreshBandTable() had been filling
// in a table nobody could see.

#include <QtTest>

#include "gui/AntennaWindow.h"
#include "core/antenna/Touchstone.h"

#include <QLabel>
#include <QTableWidget>
#include <QWidget>

#include <complex>

using namespace Longpath;

namespace {

/// Points across [loHz, hiHz] at a fixed reflection coefficient.
void appendFlat(Sweep& s, double loHz, double hiHz, double gammaMag)
{
    constexpr int kN = 11;
    for (int i = 0; i < kN; ++i) {
        SweepPoint p;
        p.freqHz = loHz + (hiHz - loHz) * i / (kN - 1);
        p.gamma  = std::complex<double>(gammaMag, 0.0);
        s.points.append(p);
    }
}

} // namespace

class TestAntennaWindowMultiband : public QObject
{
    Q_OBJECT

private slots:
    void singleBandKeepsTheTiles()
    {
        AntennaWindow w;
        Sweep s;
        s.source = QStringLiteral("20m");
        s.magnitudeOnly = true;
        appendFlat(s, 14.0e6, 14.35e6, 0.1);
        w.setSweep(s);

        auto* table = w.findChild<QTableWidget*>();
        QVERIFY(table);
        QVERIFY2(!table->isVisible(),
                 "a one-band sweep put up a one-row table");
    }

    void severalBandsShowTheTable()
    {
        AntennaWindow w;
        w.resize(1000, 760);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        // 40 m well matched, 10 m poor — the shape that produced the
        // wrong headline.
        Sweep s;
        s.source = QStringLiteral("range");
        s.magnitudeOnly = true;
        appendFlat(s,  7.0e6,  7.2e6,  0.05);   // SWR ≈ 1.11
        appendFlat(s, 28.0e6, 29.7e6,  0.50);   // SWR = 3.00
        w.setSweep(s);

        auto* table = w.findChild<QTableWidget*>();
        QVERIFY(table);
        QVERIFY2(table->isVisible(),
                 "a two-band sweep left the per-band table hidden — the "
                 "table is parented into the retired holder again");
        QCOMPARE(table->rowCount(), 2);

        // The dip column must be filled for both, and it is the only
        // column a radio sweep can fill: no phase, so no resonance.
        QVERIFY(table->item(0, 4));
        QVERIFY(table->item(1, 4));
        QVERIFY2(!table->item(0, 4)->text().isEmpty(),
                 "the best-SWR column was left blank");
        QVERIFY2(table->isColumnHidden(5),
                 "the resonance column was shown for a magnitude-only "
                 "sweep, where it can only ever hold dashes");
    }

    void severalBandsHideTheSingleBandTiles()
    {
        AntennaWindow w;
        w.resize(1000, 760);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        Sweep s;
        s.source = QStringLiteral("range");
        s.magnitudeOnly = true;
        appendFlat(s,  7.0e6,  7.2e6,  0.05);
        appendFlat(s, 28.0e6, 29.7e6,  0.50);
        w.setSweep(s);

        // The tiles carry the caption "BAND START". If that label is
        // still on screen for a nine-band sweep, the window is again
        // describing one arbitrary band as though it were the answer.
        bool tileVisible = false;
        for (auto* l : w.findChildren<QLabel*>()) {
            // startsWith, not ==: refresh() rewrites the caption to
            // "BAND START  14.000". Comparing for equality made this
            // test pass in the negative case for the wrong reason and
            // fail in the positive one — my error, not the window's.
            if (l->text().startsWith(QStringLiteral("BAND START"))
                && l->isVisible()) {
                tileVisible = true;
                break;
            }
        }
        QVERIFY2(!tileVisible,
                 "the single-band tiles are still shown over a "
                 "multi-band sweep");
    }

    void aOneBandSweepAfterAMultiBandOneGetsItsTilesBack()
    {
        // State that only goes one way is the bug I keep writing.
        AntennaWindow w;
        w.resize(1000, 760);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        Sweep wide;
        wide.source = QStringLiteral("range");
        wide.magnitudeOnly = true;
        appendFlat(wide,  7.0e6,  7.2e6,  0.05);
        appendFlat(wide, 28.0e6, 29.7e6,  0.50);
        w.setSweep(wide);

        Sweep narrow;
        narrow.source = QStringLiteral("20m");
        narrow.magnitudeOnly = true;
        appendFlat(narrow, 14.0e6, 14.35e6, 0.1);
        w.setSweep(narrow);

        auto* table = w.findChild<QTableWidget*>();
        QVERIFY(table);
        QVERIFY(!table->isVisible());

        bool tileVisible = false;
        for (auto* l : w.findChildren<QLabel*>()) {
            // startsWith, not ==: refresh() rewrites the caption to
            // "BAND START  14.000". Comparing for equality made this
            // test pass in the negative case for the wrong reason and
            // fail in the positive one — my error, not the window's.
            if (l->text().startsWith(QStringLiteral("BAND START"))
                && l->isVisible()) {
                tileVisible = true;
                break;
            }
        }
        QVERIFY2(tileVisible,
                 "the tiles never came back after a multi-band sweep");
    }
};

QTEST_MAIN(TestAntennaWindowMultiband)
#include "tst_antenna_window_multiband.moc"
