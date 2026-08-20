// =================================================================
// tests/tst_amateur_bands.cpp  (NereusSDR)
// =================================================================
//
// A band edge drawn in the wrong place is worse than no band edge at
// all: it paints "your antenna is fine here" over spectrum the operator
// may not transmit in.
//
// The project already had a band table, written to Region 2 edges, for
// guessing a mode from a spot frequency. Reusing it for an antenna tool
// would put the 40 m edge at 7.300 for an operator in Region 1. These
// tests are mostly about the three places the regions differ.
//
// no-port-check: NereusSDR-original.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/antenna/AmateurBands.h"

#include <QtTest/QtTest>

using namespace Longpath::AmateurBands;

class TstAmateurBands : public QObject {
    Q_OBJECT
private slots:

    // ── The differences that bite ────────────────────────────────────

    void region_one_stops_40m_at_7200()
    {
        const Band b = containing(7.150e6, Region::One);
        QCOMPARE(b.name, QStringLiteral("40 m"));
        QCOMPARE(b.highHz, 7.200e6);
        QVERIFY2(!containing(7.250e6, Region::One).isValid(),
                 "7.250 is inside 40 m in Region 2, not in Region 1");
        QVERIFY(containing(7.250e6, Region::Two).isValid());
    }

    void region_one_stops_80m_at_3800()
    {
        QCOMPARE(containing(3.700e6, Region::One).name,
                 QStringLiteral("80 m"));
        QVERIFY(!containing(3.900e6, Region::One).isValid());
        QVERIFY(containing(3.900e6, Region::Two).isValid());
    }

    void region_one_starts_160m_at_1810()
    {
        QVERIFY2(!containing(1.805e6, Region::One).isValid(),
                 "Region 1 has no allocation below 1.810");
        QVERIFY(containing(1.805e6, Region::Two).isValid());
    }

    // ── The centre, which is what the curve marks ────────────────────

    void the_band_centre_is_the_midpoint()
    {
        const Band b = containing(7.100e6, Region::One);
        QVERIFY(b.isValid());
        QCOMPARE(b.centreHz(), 7.100e6);
        QCOMPARE(b.widthHz(),  200e3);

        const Band twenty = containing(14.200e6, Region::One);
        QCOMPARE(twenty.centreHz(), 14.175e6);
    }

    // ── Gaps are gaps ────────────────────────────────────────────────

    void a_frequency_between_bands_is_in_no_band()
    {
        // Rounding a frequency in a gap into the nearest band would
        // draw an edge that is not there.
        QVERIFY(!containing(8.000e6, Region::One).isValid());
        QVERIFY(!containing(0.5e6,  Region::One).isValid());
        QVERIFY(!containing(1000e6, Region::One).isValid());
    }

    void the_edges_themselves_count_as_inside()
    {
        QVERIFY(containing(7.000e6, Region::One).isValid());
        QVERIFY(containing(7.200e6, Region::One).isValid());
    }

    // ── Which band a sweep is about ──────────────────────────────────

    void a_sweep_is_matched_by_how_much_it_overlaps()
    {
        // A NanoVNA sweep straddles the band with margin, so
        // containment is the wrong test and overlap is the right one.
        const Band b = bestOverlap(6.90e6, 7.40e6, Region::One);
        QCOMPARE(b.name, QStringLiteral("40 m"));
    }

    void a_sweep_covering_two_bands_picks_the_larger_overlap()
    {
        // 9.5 to 14.1 covers all 25 kHz of 30 m and 100 kHz of 20 m.
        const Band b = bestOverlap(10.125e6, 14.100e6, Region::One);
        QCOMPARE(b.name, QStringLiteral("20 m"));
    }

    void a_sweep_touching_no_band_returns_nothing()
    {
        QVERIFY(!bestOverlap(8.0e6, 9.0e6, Region::One).isValid());
    }

    // Touching a band at exactly one point is not being about it.
    void stopping_exactly_on_an_edge_is_not_an_overlap()
    {
        QVERIFY(!bestOverlap(6.5e6, 7.000e6, Region::One).isValid());
        QVERIFY(bestOverlap(6.5e6, 7.001e6, Region::One).isValid());
    }

    void a_reversed_range_is_accepted()
    {
        QCOMPARE(bestOverlap(7.40e6, 6.90e6, Region::One).name,
                 QStringLiteral("40 m"));
    }

    // ── Every band a wide sweep touches ──────────────────────────────
    //
    // The case an end-fed owner actually has: sweep the whole of HF and
    // ask where all the resonances are. bestOverlap() answers "10 m",
    // which is true and useless.
    void a_wide_sweep_reports_every_band_it_touches()
    {
        const auto all = allOverlapping(3.0e6, 30.0e6, Region::One);
        QStringList names;
        for (const Band& b : all) { names << b.name; }

        // 80, 60, 40, 30, 20, 17, 15, 12, 10 — but NOT 160, which ends
        // at 2.000, below the sweep.
        QVERIFY2(names.contains(QStringLiteral("80 m")), qPrintable(names.join(QStringLiteral(", "))));
        QVERIFY(names.contains(QStringLiteral("40 m")));
        QVERIFY(names.contains(QStringLiteral("20 m")));
        QVERIFY(names.contains(QStringLiteral("10 m")));
        QVERIFY2(!names.contains(QStringLiteral("160 m")),
                 "160 m ends at 2.000 and is not in a 3-to-30 sweep");
        QVERIFY2(!names.contains(QStringLiteral("6 m")),
                 "6 m starts at 50 and is not in a 3-to-30 sweep");
        QCOMPARE(all.size(), 9);
    }

    void the_bands_come_back_in_frequency_order()
    {
        const auto all = allOverlapping(3.0e6, 30.0e6, Region::One);
        for (int i = 1; i < all.size(); ++i) {
            QVERIFY(all.at(i).lowHz > all.at(i - 1).lowHz);
        }
    }

    void a_narrow_sweep_reports_one_band_or_none()
    {
        QCOMPARE(allOverlapping(7.0e6, 7.3e6, Region::One).size(), 1);
        QCOMPARE(allOverlapping(8.0e6, 9.0e6, Region::One).size(), 0);
    }

    void touching_an_edge_is_not_touching_the_band_here_either()
    {
        // Same strictness as bestOverlap, so the two cannot disagree
        // about whether a sweep is "about" a band.
        QCOMPARE(allOverlapping(6.5e6, 7.000e6, Region::One).size(), 0);
        QCOMPARE(allOverlapping(6.5e6, 7.001e6, Region::One).size(), 1);
    }

    void a_reversed_wide_range_is_accepted_too()
    {
        QCOMPARE(allOverlapping(30.0e6, 3.0e6, Region::One).size(),
                 allOverlapping(3.0e6, 30.0e6, Region::One).size());
    }

    // ── The table itself ─────────────────────────────────────────────

    void every_band_is_ordered_and_does_not_overlap_its_neighbour()
    {
        for (Region r : {Region::One, Region::Two, Region::Three}) {
            const auto& bands = forRegion(r);
            QVERIFY(!bands.isEmpty());
            for (int i = 0; i < bands.size(); ++i) {
                QVERIFY2(bands.at(i).highHz > bands.at(i).lowHz,
                         qPrintable(bands.at(i).name));
                QVERIFY(!bands.at(i).name.isEmpty());
                if (i > 0) {
                    QVERIFY2(bands.at(i).lowHz > bands.at(i - 1).highHz,
                             qPrintable(QStringLiteral("%1 overlaps %2")
                                            .arg(bands.at(i).name,
                                                 bands.at(i - 1).name)));
                }
            }
        }
    }

    void the_region_is_named_so_a_drawing_can_say_where_it_came_from()
    {
        QVERIFY(regionName(Region::One).contains(QStringLiteral("1")));
        QVERIFY(regionName(Region::Two).contains(QStringLiteral("2")));
        QVERIFY(regionName(Region::One).contains(QStringLiteral("IARU")));
    }
};

QTEST_APPLESS_MAIN(TstAmateurBands)
#include "tst_amateur_bands.moc"
