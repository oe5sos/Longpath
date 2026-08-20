// =================================================================
// tests/tst_antenna_sweep.cpp  (NereusSDR)
// =================================================================
//
// Three modules, one job: turn a measured sweep into "add 22 cm to
// each leg" without getting the sign, the units or the physics wrong.
//
// The tests that matter here are not the arithmetic ones. They are:
//
//   * a Touchstone file with no frequency unit is REFUSED, not read as
//     gigahertz. The standard's default is GHZ, so guessing puts a
//     40 m dipole in the microwave region and the curve still looks
//     plausible.
//   * the anti-resonance is not offered as something to trim towards.
//     A wide sweep crosses zero at the half-wave AND at the full wave,
//     and the second one has a feed resistance in the thousands.
//   * resonance and the SWR minimum are reported separately, because
//     conflating them is the mistake the whole module exists to stop.
//   * shortening is halved and lengthening is not, because wire does
//     not grow back.
//
// Every expected number below was computed in Python before the C++
// was written — see the run recorded in the commit message.
//
// no-port-check: NereusSDR-original.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/antenna/AntennaSweep.h"
#include "core/antenna/AntennaTrim.h"
#include "core/antenna/Touchstone.h"

#include <QtTest/QtTest>

#include <cmath>
#include <functional>

using namespace Longpath;
using Kind = AntennaTrim::Kind;

namespace {

std::complex<double> gammaOf(double r, double x, double z0 = 50.0)
{
    const std::complex<double> z(r, x);
    return (z - z0) / (z + z0);
}

// A sweep built from an impedance model, so the right answer is known
// by construction rather than by running the code under test.
Sweep synth(double startHz, double stopHz, int points,
            const std::function<std::complex<double>(double)>& zAt)
{
    Sweep s;
    s.referenceOhms = 50.0;
    for (int i = 0; i < points; ++i) {
        const double f = startHz
            + (stopHz - startHz) * double(i) / double(points - 1);
        const std::complex<double> z = zAt(f);
        SweepPoint p;
        p.freqHz = f;
        p.gamma  = gammaOf(z.real(), z.imag());
        s.points.append(p);
    }
    return s;
}

// The antenna in the worked example: resonant at 7.183 MHz, 55 Ω there,
// reactance sloping 250 Ω per MHz.
std::complex<double> dipoleModel(double fHz)
{
    const double dMHz = (fHz - 7.183e6) / 1e6;
    return {std::max(6.0, 55.0 + 60.0 * dMHz), 250.0 * dMHz};
}

} // namespace

class TstAntennaSweep : public QObject {
    Q_OBJECT
private slots:

    // ── Touchstone ───────────────────────────────────────────────────

    void the_three_number_formats_agree()
    {
        // The same point written three ways must parse to the same
        // reflection coefficient. Reading MA as RI produces a curve
        // that looks fine and is wrong everywhere.
        const auto ri = Touchstone::parseS1p(QStringLiteral(
            "# MHZ S RI R 50\n7.05 0.3 0.4\n7.06 0.3 0.4\n"));
        // |0.3+0.4j| = 0.5, angle = 53.13010235°
        const auto ma = Touchstone::parseS1p(QStringLiteral(
            "# MHZ S MA R 50\n7.05 0.5 53.13010235\n7.06 0.5 53.13010235\n"));
        // 20·log10(0.5) = -6.020599913 dB
        const auto db = Touchstone::parseS1p(QStringLiteral(
            "# MHZ S DB R 50\n7.05 -6.020599913 53.13010235\n"
            "7.06 -6.020599913 53.13010235\n"));

        QVERIFY2(!ri.isEmpty(), qPrintable(ri.note));
        QVERIFY2(!ma.isEmpty(), qPrintable(ma.note));
        QVERIFY2(!db.isEmpty(), qPrintable(db.note));

        for (const Sweep* s : {&ma, &db}) {
            QVERIFY2(std::abs(s->points.first().gamma
                              - ri.points.first().gamma) < 1e-6,
                     "the number formats do not agree");
        }
    }

    void frequencies_are_scaled_by_the_declared_unit()
    {
        const auto mhz = Touchstone::parseS1p(QStringLiteral(
            "# MHZ S RI R 50\n7.05 0 0\n7.06 0 0\n"));
        QVERIFY(std::abs(mhz.points.first().freqHz - 7.05e6) < 1.0);

        const auto hz = Touchstone::parseS1p(QStringLiteral(
            "# Hz S RI R 50\n7050000 0 0\n7060000 0 0\n"));
        QVERIFY(std::abs(hz.points.first().freqHz - 7.05e6) < 1.0);

        const auto khz = Touchstone::parseS1p(QStringLiteral(
            "# KHZ S RI R 50\n7050 0 0\n7060 0 0\n"));
        QVERIFY(std::abs(khz.points.first().freqHz - 7.05e6) < 1.0);
    }

    // ── The one that would be silently catastrophic ──────────────────
    //
    // Touchstone's default unit is GHZ. A file whose '#' line omits the
    // unit would read "7.05" as 7.05 GHz — wrong by a thousand, and the
    // curve still plots. Refusing is the only safe answer.
    void a_header_with_no_unit_is_refused_rather_than_assumed()
    {
        const auto s = Touchstone::parseS1p(QStringLiteral(
            "# S RI R 50\n7.05 0.1 0.1\n7.06 0.1 0.1\n"));
        QVERIFY2(s.isEmpty(),
                 "a file with no frequency unit was read anyway");
        QVERIFY(s.note.contains(QStringLiteral("unit")));
    }

    void a_header_with_no_format_is_refused()
    {
        const auto s = Touchstone::parseS1p(QStringLiteral(
            "# MHZ S R 50\n7.05 0.1 0.1\n"));
        QVERIFY(s.isEmpty());
        QVERIFY(s.note.contains(QStringLiteral("format")));
    }

    void a_file_of_other_parameters_says_so()
    {
        const auto s = Touchstone::parseS1p(QStringLiteral(
            "# MHZ Z RI R 50\n7.05 40 -3\n"));
        QVERIFY(s.isEmpty());
        QVERIFY(s.note.contains(QStringLiteral("Z-parameters")));
    }

    void a_comment_may_start_anywhere_in_a_line()
    {
        // Several instruments append "! point 3" to every data row.
        // Treating '!' as start-of-line-only loses the whole sweep.
        const auto s = Touchstone::parseS1p(QStringLiteral(
            "! saved by something\n"
            "# MHZ S RI R 50   ! fifty ohms\n"
            "7.05 0.3 0.4 ! point 1\n"
            "7.06 0.3 0.4 ! point 2\n"));
        QCOMPARE(s.points.size(), 2);
        QVERIFY(std::abs(s.points.first().gamma
                         - std::complex<double>(0.3, 0.4)) < 1e-9);
    }

    void the_reference_impedance_is_read()
    {
        const auto s = Touchstone::parseS1p(QStringLiteral(
            "# MHZ S RI R 75\n7.05 0 0\n7.06 0 0\n"));
        QCOMPARE(s.referenceOhms, 75.0);
    }

    void a_descending_sweep_is_put_in_order()
    {
        // Everything downstream interpolates between neighbours and
        // would produce nonsense on a reversed sweep.
        const auto s = Touchstone::parseS1p(QStringLiteral(
            "# MHZ S RI R 50\n7.20 0 0\n7.10 0 0\n7.00 0 0\n"));
        QCOMPARE(s.points.size(), 3);
        QVERIFY(s.points.at(0).freqHz < s.points.at(1).freqHz);
        QVERIFY(s.points.at(1).freqHz < s.points.at(2).freqHz);
    }

    void data_without_a_header_is_refused()
    {
        const auto s = Touchstone::parseS1p(
            QStringLiteral("7.05 0.1 0.1\n7.06 0.1 0.1\n"));
        QVERIFY(s.isEmpty());
        QVERIFY(!s.note.isEmpty());
    }

    // ── Point arithmetic, against hand-computed values ───────────────

    void swr_and_impedance_round_trip()
    {
        struct C { double r, x, swr; };
        // From the independent Python run.
        for (const C& c : {C{35, 0, 1.4286}, C{55, -38.2, 2.0525},
                           C{25, 60, 5.1872}, C{200, -100, 5.0521},
                           C{50, 0, 1.0}}) {
            const auto g = gammaOf(c.r, c.x);
            QVERIFY2(std::abs(AntennaSweep::swr(g) - c.swr) < 0.001,
                     qPrintable(QStringLiteral("R=%1 X=%2 gave SWR %3, "
                                               "expected %4")
                                    .arg(c.r).arg(c.x)
                                    .arg(AntennaSweep::swr(g)).arg(c.swr)));
            const auto z = AntennaSweep::impedance(g, 50.0);
            QVERIFY(std::abs(z.real() - c.r) < 1e-6);
            QVERIFY(std::abs(z.imag() - c.x) < 1e-6);
        }
    }

    void an_open_or_a_short_does_not_produce_infinity()
    {
        // |Γ| = 1 divides by zero. A NaN here poisons every plot and
        // every comparison downstream.
        QVERIFY(std::isfinite(AntennaSweep::swr({1.0, 0.0})));
        QVERIFY(std::isfinite(AntennaSweep::swr({-1.0, 0.0})));
        QVERIFY(std::isfinite(AntennaSweep::swr({1.5, 0.0})));  // impossible
        QVERIFY(AntennaSweep::swr({1.0, 0.0}) > 100.0);
        QVERIFY(std::isfinite(AntennaSweep::impedance({1.0, 0.0}, 50.0).real()));
    }

    // ── Finding the resonance ────────────────────────────────────────

    void the_crossing_is_interpolated_not_rounded_to_a_sample()
    {
        // Samples 10 kHz apart, true crossing at 7.183 — three tenths
        // of the way between two of them. Rounding to the nearest
        // sample is 2 kHz out, which on 40 m is about 6 cm of wire.
        const Sweep s = synth(7.00e6, 7.40e6, 41, dipoleModel);
        const auto r = AntennaSweep::nearestResonance(s, 7.05e6);
        QVERIFY2(r.found, "no resonance found in a sweep that has one");
        QVERIFY2(std::abs(r.freqHz - 7.183e6) < 500.0,
                 qPrintable(QStringLiteral("resonance at %1, expected "
                                           "7183000").arg(r.freqHz)));
        QVERIFY2(std::abs(r.resistanceOhms - 55.0) < 1.0,
                 qPrintable(QStringLiteral("R was %1, expected 55")
                                .arg(r.resistanceOhms)));
        QVERIFY(r.rising);
    }

    // ── The test this file exists for ────────────────────────────────
    //
    // A sweep wide enough to see two crossings sees the half-wave
    // (rising, low R — the one to trim to) and the full-wave
    // anti-resonance (falling, R in the thousands). Offering the second
    // one sends somebody trimming a 40 m dipole towards its 20 m
    // anti-resonance, where nothing they do will help.
    void the_anti_resonance_is_not_offered_as_a_thing_to_trim_to()
    {
        const Sweep s = synth(6.0e6, 15.0e6, 181, [](double fHz) {
            const double f = fHz / 1e6;
            if (f < 10.5) {
                // Half-wave: rises through zero at 7.0, R around 60.
                return std::complex<double>(60.0, 200.0 * (f - 7.0));
            }
            // Full wave: falls through zero at 14.0, R in the thousands.
            return std::complex<double>(3000.0, -900.0 * (f - 14.0));
        });

        const auto all = AntennaSweep::resonances(s);
        QVERIFY2(all.size() >= 2,
                 "the synthetic sweep should contain both crossings");

        // Asked for the one nearest 13.9 MHz — which IS the
        // anti-resonance — the answer must still be the half-wave.
        const auto r = AntennaSweep::nearestResonance(s, 13.9e6);
        QVERIFY(r.found);
        QVERIFY2(std::abs(r.freqHz - 7.0e6) < 100e3,
                 qPrintable(QStringLiteral(
                     "returned %1 MHz — that is the anti-resonance")
                         .arg(r.freqHz / 1e6)));
        QVERIFY(r.rising);
    }

    void a_sweep_with_no_crossing_says_so_instead_of_guessing()
    {
        // Capacitive everywhere: the antenna is far too short and the
        // resonance is outside the sweep.
        const Sweep s = synth(7.0e6, 7.2e6, 21, [](double) {
            return std::complex<double>(20.0, -300.0);
        });
        QVERIFY(!AntennaSweep::nearestResonance(s).found);
        QVERIFY(AntennaSweep::describe(s, 7.03e6)
                    .contains(QStringLiteral("not resonant")));
    }

    // Resonance and best match are different questions with different
    // answers, and the module must never quietly return one for the
    // other.
    void resonance_and_the_swr_minimum_are_reported_separately()
    {
        // R climbs through 50 well below where X reaches zero.
        const Sweep s = synth(6.9e6, 7.3e6, 81, [](double fHz) {
            const double d = (fHz - 7.02e6) / 1e6;
            return std::complex<double>(std::max(8.0, 35.0 + 250.0 * d),
                                        150.0 * d);
        });
        const auto r = AntennaSweep::nearestResonance(s, 7.0e6);
        const auto m = AntennaSweep::bestMatch(s);
        QVERIFY(r.found);
        QVERIFY(m.found);
        QVERIFY2(std::abs(r.freqHz - 7.02e6) < 5e3,
                 qPrintable(QStringLiteral("resonance %1").arg(r.freqHz)));
        QVERIFY2(m.freqHz > r.freqHz + 20e3,
                 "the SWR minimum should be well above the resonance in "
                 "this model");

        const QString d = AntennaSweep::describe(s, 7.03e6);
        QVERIFY2(d.contains(QStringLiteral("lowest SWR")),
                 qPrintable(d));
    }

    void the_usable_span_grows_around_the_frequency_asked_about()
    {
        const Sweep s = synth(7.0e6, 7.4e6, 81, dipoleModel);
        const auto span = AntennaSweep::usableSpan(s, 2.0, 7.183e6);
        QVERIFY(span.found);
        QVERIFY(span.lowHz < 7.183e6);
        QVERIFY(span.highHz > 7.183e6);
        QVERIFY(AntennaSweep::swrAt(s, span.lowHz)  < 2.05);
        QVERIFY(AntennaSweep::swrAt(s, span.highHz) < 2.05);
    }

    void asking_outside_the_sweep_returns_nothing_rather_than_a_guess()
    {
        const Sweep s = synth(7.0e6, 7.2e6, 21, dipoleModel);
        QCOMPARE(AntennaSweep::swrAt(s, 3.5e6), 0.0);
        QCOMPARE(AntennaSweep::swrAt(s, 14.0e6), 0.0);
    }

    // ── Trimming ─────────────────────────────────────────────────────

    void measured_high_means_too_short()
    {
        // The worked example. Python: +2.1764 %, +43.53 cm total,
        // +21.76 cm per leg.
        const auto t = AntennaTrim::compute(Kind::Dipole, 7.183e6,
                                            7.030e6, 20.0);
        QVERIFY(t.valid);
        QVERIFY2(t.lengthen, "measured above target must mean lengthen");
        QVERIFY(std::abs(t.percentChange - 2.1764) < 0.001);
        QVERIFY(std::abs(t.totalChangeM - 0.4353) < 0.0005);
        QVERIFY(std::abs(t.perElementM - 0.21764) < 0.0005);
    }

    void measured_low_means_too_long()
    {
        const auto t = AntennaTrim::compute(Kind::Dipole, 7.030e6,
                                            7.183e6, 20.0);
        QVERIFY(t.valid);
        QVERIFY2(!t.lengthen, "measured below target must mean shorten");
        QVERIFY(std::abs(t.percentChange + 2.1300) < 0.001);
        QVERIFY(t.perElementM < 0.0);
    }

    // Arithmetically a beam's driven element IS a dipole — centre-fed,
    // half at each end. The kind exists for the advice, not the maths,
    // and this pins that the maths really is identical so nobody
    // "improves" one of them alone.
    void a_beam_driven_element_computes_exactly_as_a_dipole()
    {
        const auto d = AntennaTrim::compute(Kind::Dipole, 14.310e6,
                                            14.100e6, 10.1);
        const auto y = AntennaTrim::compute(Kind::YagiDrivenElement,
                                            14.310e6, 14.100e6, 10.1);
        QCOMPARE(y.percentChange, d.percentChange);
        QCOMPARE(y.totalChangeM,  d.totalChangeM);
        QCOMPARE(y.perElementM,   d.perElementM);
        QCOMPARE(y.firstStepM,    d.firstStepM);
        QCOMPARE(y.halved,        d.halved);

        // And it must not be the end-fed's whole-change-in-one-place.
        const auto e = AntennaTrim::compute(Kind::EndFedHalfWave,
                                            14.310e6, 14.100e6, 10.1);
        QVERIFY(std::abs(y.perElementM * 2.0 - e.perElementM) < 1e-12);
    }

    void the_beam_is_told_apart_in_words_even_though_not_in_numbers()
    {
        const auto y = AntennaTrim::compute(Kind::YagiDrivenElement,
                                            14.310e6, 14.100e6, 10.1);
        const QString s = AntennaTrim::instruction(y, Kind::YagiDrivenElement);
        QVERIFY2(s.contains(QStringLiteral("driven element")),
                 qPrintable(s));
        QVERIFY(!AntennaTrim::kindName(Kind::YagiDrivenElement).isEmpty());
        QVERIFY(AntennaTrim::kindName(Kind::YagiDrivenElement)
                    != AntennaTrim::kindName(Kind::Dipole));
    }

    void every_kind_has_a_name_and_a_place_to_put_the_change()
    {
        for (Kind k : {Kind::Dipole, Kind::EndFedHalfWave,
                       Kind::YagiDrivenElement, Kind::VerticalRadiator,
                       Kind::Loop}) {
            QVERIFY2(!AntennaTrim::kindName(k).isEmpty(),
                     "a kind with no name would appear as a blank entry");
            QVERIFY2(!AntennaTrim::kindWhere(k).isEmpty(),
                     "a kind with no 'where' produces 'Lengthen by 22 cm .'");
        }
    }

    void a_dipole_splits_the_change_and_an_end_fed_does_not()
    {
        const auto d = AntennaTrim::compute(Kind::Dipole, 7.183e6,
                                            7.030e6, 20.0);
        const auto e = AntennaTrim::compute(Kind::EndFedHalfWave, 7.183e6,
                                            7.030e6, 20.0);
        QVERIFY(std::abs(d.totalChangeM - e.totalChangeM) < 1e-9);
        QVERIFY2(std::abs(d.perElementM * 2.0 - e.perElementM) < 1e-9,
                 "a dipole's per-leg change should be half the end-fed's");
    }

    // Wire does not grow back. A first measurement carries a systematic
    // offset often enough that cutting the whole way in one go is how
    // an antenna ends up too short on a summit.
    void shortening_is_halved_and_lengthening_is_not()
    {
        const auto shorter = AntennaTrim::compute(Kind::Dipole, 7.030e6,
                                                  7.183e6, 20.0);
        QVERIFY(shorter.halved);
        QVERIFY(std::abs(shorter.firstStepM
                         - shorter.perElementM * 0.5) < 1e-9);
        QVERIFY(AntennaTrim::instruction(shorter, Kind::Dipole)
                    .contains(QStringLiteral("two passes")));

        const auto longer = AntennaTrim::compute(Kind::Dipole, 7.183e6,
                                                 7.030e6, 20.0);
        QVERIFY(!longer.halved);
        QCOMPARE(longer.firstStepM, longer.perElementM);
    }

    void a_large_correction_is_flagged_rather_than_stated_precisely()
    {
        // 40 % is well outside where inverse-length scaling holds.
        const auto t = AntennaTrim::compute(Kind::Dipole, 14.0e6,
                                            10.0e6, 20.0);
        QVERIFY(t.valid);
        QVERIFY2(!t.caution.isEmpty(),
                 "a 40 percent change should carry a warning");

        const auto small = AntennaTrim::compute(Kind::Dipole, 7.183e6,
                                                7.030e6, 20.0);
        QVERIFY(small.caution.isEmpty());
    }

    void without_a_length_it_gives_the_percentage_and_says_so()
    {
        const auto t = AntennaTrim::compute(Kind::Dipole, 7.183e6,
                                            7.030e6, 0.0);
        QVERIFY(t.valid);
        QVERIFY(std::abs(t.percentChange - 2.1764) < 0.001);
        QCOMPARE(t.totalChangeM, 0.0);
        const QString s = AntennaTrim::instruction(t, Kind::Dipole);
        QVERIFY(s.contains(QStringLiteral("%")));
        QVERIFY(s.contains(QStringLiteral("current length")));
    }

    void already_on_frequency_means_leave_it_alone()
    {
        const auto t = AntennaTrim::compute(Kind::Dipole, 7.1e6,
                                            7.1e6, 20.0);
        QVERIFY(AntennaTrim::instruction(t, Kind::Dipole)
                    .contains(QStringLiteral("leave it alone")));
    }

    void nonsense_input_is_refused()
    {
        QVERIFY(!AntennaTrim::compute(Kind::Dipole, 0.0, 7.0e6, 20.0).valid);
        QVERIFY(!AntennaTrim::compute(Kind::Dipole, 7.0e6, 0.0, 20.0).valid);
        QVERIFY(!AntennaTrim::compute(Kind::Dipole, -1.0, 7.0e6, 20.0).valid);
    }

    void the_half_wave_estimate_matches_the_textbook()
    {
        // c / f / 2 × 0.95. Python: 20.2562 m at 7.03 MHz.
        QVERIFY(std::abs(AntennaTrim::halfWaveEstimateM(7.03e6) - 20.2562)
                < 0.001);
        QVERIFY(std::abs(AntennaTrim::halfWaveEstimateM(14.06e6) - 10.1281)
                < 0.001);
        // Free space, no insulation.
        QVERIFY(std::abs(AntennaTrim::halfWaveEstimateM(7.03e6, 1.0)
                         - 21.3224) < 0.001);
    }

    // ── The two ends joined ──────────────────────────────────────────

    void a_file_turns_into_an_instruction()
    {
        // The whole path: text in, centimetres out.
        QString text = QStringLiteral("# MHZ S RI R 50\n");
        for (int i = 0; i <= 40; ++i) {
            const double f = 7.0e6 + 10e3 * i;
            const auto g = gammaOf(dipoleModel(f).real(),
                                   dipoleModel(f).imag());
            text += QStringLiteral("%1 %2 %3\n")
                        .arg(f / 1e6, 0, 'f', 6)
                        .arg(g.real(), 0, 'f', 9)
                        .arg(g.imag(), 0, 'f', 9);
        }

        const Sweep s = Touchstone::parseS1p(text, QStringLiteral("test"));
        QVERIFY2(!s.isEmpty(), qPrintable(s.note));

        const auto r = AntennaSweep::nearestResonance(s, 7.03e6);
        QVERIFY(r.found);
        QVERIFY(std::abs(r.freqHz - 7.183e6) < 1000.0);

        const auto t = AntennaTrim::compute(Kind::Dipole, r.freqHz,
                                            7.030e6, 20.0);
        QVERIFY(t.lengthen);
        // 21.76 cm per leg, to within a millimetre of the hand figure.
        QVERIFY2(std::abs(t.perElementM - 0.2176) < 0.001,
                 qPrintable(QStringLiteral("per leg %1 m")
                                .arg(t.perElementM)));
    }
};

QTEST_APPLESS_MAIN(TstAntennaSweep)
#include "tst_antenna_sweep.moc"
