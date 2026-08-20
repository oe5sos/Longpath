// =================================================================
// tests/tst_radio_sweep.cpp  (NereusSDR)
// =================================================================
//
// Turning a radio's SWR sweep into the Sweep the analysis half of the
// antenna window understands. Asked for after a long day, in one
// sentence: "der sweep sollte nach beendigung genauso wie das beispiel
// aussehen."
//
// The arithmetic is one line, so these tests are almost entirely about
// the boundary — what a coupler measures and what it does not. A
// magnitude with no phase must not become an impedance, and an
// unmeasured point must not become a perfect one. Both of those were
// live faults in this feature within the last hour: a flat SWR 1.00
// curve that looked like an ideal antenna, and 51 discarded points
// reported as 51 measurements.
//
// no-port-check: NereusSDR-original.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-14 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/antenna/RadioSweep.h"
#include "core/antenna/AntennaSweep.h"

#include <QtTest/QtTest>

#include <cmath>

using namespace Longpath;

namespace {

SwrSweepResult makeResult(const QVector<QPair<quint64, double>>& pts)
{
    SwrSweepResult r;
    r.band      = Band::Band20m;
    r.startedAt = QDateTime::currentDateTime();
    r.completed = true;
    for (const auto& p : pts) {
        SwrSweepPoint sp;
        sp.freqHz = p.first;
        sp.swr    = p.second;
        sp.fwdW   = 5.0;
        r.points.append(sp);
    }
    return r;
}

} // namespace

class TstRadioSweep : public QObject {
    Q_OBJECT
private slots:

    // ── The one line of arithmetic ───────────────────────────────────

    void the_magnitude_round_trips_through_the_swr_formula()
    {
        // |Γ| = (SWR−1)/(SWR+1), and SWR = (1+|Γ|)/(1−|Γ|) must give it
        // back. Anything else and the curve drawn is not the curve
        // measured.
        for (double swr : {1.0, 1.1, 1.5, 2.0, 3.0, 5.0, 10.0}) {
            const double g = RadioSweep::reflectionMagnitude(swr);
            const double back = (1.0 + g) / (1.0 - g);
            QVERIFY2(std::abs(back - swr) < 1e-9,
                     qPrintable(QStringLiteral("SWR %1 → |Γ| %2 → %3")
                                    .arg(swr).arg(g).arg(back)));
        }
    }

    void a_perfect_match_reflects_nothing()
    {
        QCOMPARE(RadioSweep::reflectionMagnitude(1.0), 0.0);
        // And nothing below 1 exists; an SWR of 0 is the marker for an
        // unmeasured point and must not become a reflection of −1.
        QCOMPARE(RadioSweep::reflectionMagnitude(0.0), 0.0);
        QCOMPARE(RadioSweep::reflectionMagnitude(-3.0), 0.0);
    }

    void the_magnitude_never_reaches_one()
    {
        // |Γ| = 1 divides by zero downstream. 99 is the cap upstream.
        QVERIFY(RadioSweep::reflectionMagnitude(99.0) < 1.0);
        QVERIFY(RadioSweep::reflectionMagnitude(1e9) < 1.0);
    }

    // ── What must not survive the trip ───────────────────────────────

    void an_unmeasured_point_is_dropped_and_not_drawn_as_perfect()
    {
        // swr <= 0 marks a point the bridge could not read. Carried
        // through, it lands at SWR 1.00 — a discarded measurement
        // becoming an ideal one. That is exactly the fiction the flat
        // curve was made of.
        const auto r = makeResult({{14000000, 2.0},
                                   {14100000, 0.0},    // not measured
                                   {14200000, 1.4}});
        const Sweep s = RadioSweep::fromResult(r);

        QCOMPARE(s.points.size(), 2);
        for (const auto& p : s.points) {
            QVERIFY2(AntennaSweep::swr(p.gamma) > 1.001,
                     "a dropped point reappeared as a perfect match");
        }
        QVERIFY2(s.note.contains(QStringLiteral("1 von 3")),
                 qPrintable(s.note));
    }

    void a_sweep_that_measured_nothing_says_so_and_stays_empty()
    {
        const auto r = makeResult({{14000000, 0.0}, {14100000, 0.0}});
        const Sweep s = RadioSweep::fromResult(r);
        QVERIFY(s.isEmpty());
        QVERIFY(!s.note.isEmpty());
    }

    // ── The boundary this whole file exists for ──────────────────────

    void a_coupler_sweep_is_marked_as_having_no_phase()
    {
        const auto r = makeResult({{14000000, 2.0}, {14100000, 1.2}});
        const Sweep s = RadioSweep::fromResult(r);
        QVERIFY2(s.magnitudeOnly,
                 "without this flag every reading that needs phase will "
                 "answer anyway, from a storage convention");
    }

    // The placeholder puts Γ on the real axis, so the reactance is zero
    // at every sample. Unguarded, that reads as a resonance between
    // every pair of points — fifty-one of them, each with an ohm figure
    // nobody measured.
    void no_phase_means_no_resonance_and_no_impedance()
    {
        QVector<QPair<quint64, double>> pts;
        for (int i = 0; i < 21; ++i) {
            pts.append({14000000ull + quint64(i) * 17500ull,
                        1.2 + 0.05 * i});
        }
        const Sweep s = RadioSweep::fromResult(makeResult(pts));
        QCOMPARE(s.points.size(), 21);

        QVERIFY2(AntennaSweep::resonances(s).isEmpty(),
                 "a phaseless sweep reported resonances");
        QVERIFY2(!AntennaSweep::nearestResonance(s, 14.1e6).found,
                 "a phaseless sweep named a resonant frequency");
        QVERIFY2(!AntennaSweep::anyCrossing(s),
                 "a phaseless sweep claimed a reactance crossing");
        QCOMPARE(AntennaSweep::impedanceAt(s, 14.1e6),
                 std::complex<double>{});
    }

    // ── What must survive, or the feature is pointless ───────────────

    void everything_that_depends_only_on_magnitude_still_works()
    {
        // A dip at 14.175: 3.0 at the edges, 1.1 in the middle.
        QVector<QPair<quint64, double>> pts;
        for (int i = 0; i < 41; ++i) {
            const double t = (i - 20) / 20.0;          // −1 … +1
            pts.append({14000000ull + quint64(i) * 8750ull,
                        1.1 + 1.9 * t * t});
        }
        const Sweep s = RadioSweep::fromResult(makeResult(pts));

        const auto best = AntennaSweep::bestMatch(s);
        QVERIFY(best.found);
        QVERIFY2(std::abs(best.freqHz - 14175000.0) < 20000.0,
                 qPrintable(QStringLiteral("best match at %1")
                                .arg(best.freqHz)));
        QVERIFY(std::abs(best.swr - 1.1) < 0.01);

        // SWR at an arbitrary frequency, and the usable span, are the
        // two readings a coupler sweep is actually for.
        const double mid = AntennaSweep::swrAt(s, 14175000.0);
        QVERIFY2(std::abs(mid - 1.1) < 0.02,
                 qPrintable(QStringLiteral("swrAt gave %1").arg(mid)));

        const auto span = AntennaSweep::usableSpan(s, 2.0, 14175000.0);
        QVERIFY(span.found);
        QVERIFY(span.lowHz  > 14000000.0);
        QVERIFY(span.highHz < 14350000.0);
        QVERIFY(span.lowHz  < 14175000.0);
        QVERIFY(span.highHz > 14175000.0);
    }

    void the_source_names_the_band_so_it_is_not_mistaken_for_a_file()
    {
        const auto r = makeResult({{14000000, 2.0}, {14100000, 1.2}});
        const Sweep s = RadioSweep::fromResult(r);
        QVERIFY(!s.source.isEmpty());
        QVERIFY2(s.source.contains(QStringLiteral("Funkgerät")),
                 qPrintable(s.source));
    }

    void the_frequencies_arrive_unchanged_and_in_order()
    {
        const auto r = makeResult({{14000000, 2.0},
                                   {14100000, 1.5},
                                   {14200000, 1.2}});
        const Sweep s = RadioSweep::fromResult(r);
        QCOMPARE(s.points.size(), 3);
        QCOMPARE(s.points.at(0).freqHz, 14000000.0);
        QCOMPARE(s.points.at(2).freqHz, 14200000.0);
        QCOMPARE(s.startHz(), 14000000.0);
        QCOMPARE(s.stopHz(),  14200000.0);
    }
};

QTEST_APPLESS_MAIN(TstRadioSweep)
#include "tst_radio_sweep.moc"
