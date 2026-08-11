// =================================================================
// tests/tst_feedline.cpp  (NereusSDR)
// =================================================================
//
// A length of coax between the analyser and the antenna does two
// things, and only one of them is obvious.
//
// The obvious one: loss flatters the SWR. Fifteen ohms at the feed
// point is 3.33; through forty metres of RG-58 on 40 m the meter reads
// 2.20, and through the same length of RG-174 on 10 m it reads 1.27.
//
// The other one is why this file exists. The line rotates the
// impedance, so the frequency where the reactance crosses zero is no
// longer where the antenna is resonant — and at some lengths the only
// crossing left in the band is a FALLING one at an entirely plausible
// resistance. The first version of nearestResonance() accepted that and
// returned it as the answer. The test named
// a_feedline_artefact_is_not_mistaken_for_a_resonance pins the fix.
//
// Everything expected here was computed in Python first; the numbers in
// the comments are from that run.
//
// no-port-check: NereusSDR-original.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/antenna/AntennaSweep.h"
#include "core/antenna/Feedline.h"

#include <QtTest/QtTest>

#include <cmath>
#include <functional>

using namespace NereusSDR;

namespace {

std::complex<double> gammaOf(double r, double x, double z0 = 50.0)
{
    const std::complex<double> z(r, x);
    return (z - z0) / (z + z0);
}

Sweep synth(double startHz, double stopHz, int points,
            const std::function<std::complex<double>(double)>& zAt)
{
    Sweep s;
    s.referenceOhms = 50.0;
    for (int i = 0; i < points; ++i) {
        const double f = startHz
            + (stopHz - startHz) * double(i) / double(points - 1);
        const std::complex<double> z = zAt(f);
        s.points.append({f, gammaOf(z.real(), z.imag())});
    }
    return s;
}

// The dipole used throughout: resonant at 7.183 MHz, 55 Ω there.
std::complex<double> dipole(double fHz)
{
    const double d = (fHz - 7.183e6) / 1e6;
    return {std::max(6.0, 55.0 + 60.0 * d), 250.0 * d};
}

Feedline::Cable rg58()
{
    Feedline::Cable c;
    c.name = QStringLiteral("RG-58");
    c.velocityFactor = 0.66;
    c.lossDb100m = 4.6;
    c.refHz = 10e6;
    return c;
}

Feedline::Cable lossless(double vf = 0.66)
{
    Feedline::Cable c;
    c.name = QStringLiteral("ideal");
    c.velocityFactor = vf;
    c.lossDb100m = 0.0;
    c.refHz = 10e6;
    return c;
}

} // namespace

class TstFeedline : public QObject {
    Q_OBJECT
private slots:

    // ── The round trip ───────────────────────────────────────────────

    void putting_a_line_in_and_taking_it_out_returns_the_original()
    {
        const Sweep truth = synth(7.0e6, 7.4e6, 81, dipole);
        const Sweep seen  = Feedline::embed(truth, 5.0, rg58());
        const auto back   = Feedline::deEmbed(seen, 5.0, rg58());

        QCOMPARE(back.sweep.points.size(), truth.points.size());
        for (int i = 0; i < truth.points.size(); ++i) {
            QVERIFY2(std::abs(back.sweep.points.at(i).gamma
                              - truth.points.at(i).gamma) < 1e-9,
                     qPrintable(QStringLiteral("point %1 did not come "
                                               "back").arg(i)));
        }
        QVERIFY(!back.lossTooHigh);
    }

    void a_zero_length_changes_nothing()
    {
        const Sweep truth = synth(7.0e6, 7.4e6, 41, dipole);
        const auto out = Feedline::deEmbed(truth, 0.0, rg58());
        for (int i = 0; i < truth.points.size(); ++i) {
            QCOMPARE(out.sweep.points.at(i).gamma,
                     truth.points.at(i).gamma);
        }
    }

    // ── Loss ─────────────────────────────────────────────────────────

    // A lossless line does not change the SWR at all. People expect it
    // to, and a tool that quietly "corrected" for a lossless line would
    // be inventing a difference.
    void a_lossless_line_leaves_the_swr_alone()
    {
        const Sweep truth = synth(7.0e6, 7.4e6, 41, [](double) {
            return std::complex<double>(15.0, 0.0);   // SWR 3.33
        });
        for (double len : {2.0, 5.0, 10.0, 40.0}) {
            const Sweep seen = Feedline::embed(truth, len, lossless());
            const double s = AntennaSweep::swr(seen.points.at(10).gamma);
            QVERIFY2(std::abs(s - 3.3333) < 0.001,
                     qPrintable(QStringLiteral("%1 m of lossless line "
                                               "changed SWR to %2")
                                    .arg(len).arg(s)));
        }
    }

    // The one that misleads. Python, at 7.2 MHz through RG-58:
    //   antenna 3.333 → 5 m 3.12 → 10 m 2.94 → 20 m 2.64 → 40 m 2.20
    // Thin cable and a higher band is worse: RG-174 on 10 m reaches
    // 1.27, which reads as a perfectly good antenna.
    void loss_makes_a_bad_antenna_look_good()
    {
        const Sweep truth = synth(7.0e6, 7.4e6, 41, [](double) {
            return std::complex<double>(15.0, 0.0);
        });
        const double atAntenna =
            AntennaSweep::swr(truth.points.at(10).gamma);
        QVERIFY(std::abs(atAntenna - 3.3333) < 0.001);

        double previous = atAntenna;
        for (double len : {5.0, 10.0, 20.0, 40.0}) {
            const Sweep seen = Feedline::embed(truth, len, rg58());
            const double s = AntennaSweep::swr(seen.points.at(10).gamma);
            QVERIFY2(s < previous,
                     qPrintable(QStringLiteral("%1 m did not flatter the "
                                               "reading further").arg(len)));
            previous = s;
        }
        QVERIFY2(previous < 2.4,
                 qPrintable(QStringLiteral("forty metres of RG-58 should "
                                           "bring 3.33 down to about 2.2, "
                                           "got %1").arg(previous)));

        // Thinner cable on a higher band is the case that actually
        // deceives: the same antenna reads as good.
        Feedline::Cable rg174;
        rg174.name = QStringLiteral("RG-174");
        rg174.velocityFactor = 0.66;
        rg174.lossDb100m = 9.8;
        rg174.refHz = 10e6;
        const Sweep ten = synth(28.0e6, 29.0e6, 21, [](double) {
            return std::complex<double>(15.0, 0.0);
        });
        const Sweep far = Feedline::embed(ten, 40.0, rg174);
        const double flattered = AntennaSweep::swr(far.points.at(10).gamma);
        QVERIFY2(flattered < 1.5,
                 qPrintable(QStringLiteral("expected about 1.27, got %1")
                                .arg(flattered)));
    }

    void removing_more_loss_than_exists_is_refused_not_believed()
    {
        // A near-total reflection through a cable that is claimed to be
        // long and lossy: de-embedding would push |Γ| past 1, which is
        // the antenna returning more than it was given.
        const Sweep truth = synth(7.0e6, 7.4e6, 21, [](double) {
            return std::complex<double>(2.0, 0.0);   // almost a short
        });
        Feedline::Cable awful = rg58();
        awful.lossDb100m = 60.0;
        const auto out = Feedline::deEmbed(truth, 60.0, awful);
        QVERIFY2(out.lossTooHigh, "an impossible result was accepted");
        QVERIFY(!out.note.isEmpty());
        for (const SweepPoint& p : out.sweep.points) {
            QVERIFY(std::abs(p.gamma) < 1.0);
        }
    }

    // ── Attenuation model ────────────────────────────────────────────

    void attenuation_scales_as_the_square_root_of_frequency()
    {
        const double a10 = Feedline::alphaNpPerM(4.6, 10e6, 10e6);
        const double a40 = Feedline::alphaNpPerM(4.6, 10e6, 40e6);
        QVERIFY2(std::abs(a40 / a10 - 2.0) < 1e-9,
                 "four times the frequency should be twice the loss");
        // 4.6 dB/100 m = 0.046 dB/m = 0.005296 Np/m.
        QVERIFY(std::abs(a10 - 0.0052959) < 1e-6);
    }

    void a_cable_with_no_loss_figure_attenuates_nothing()
    {
        QCOMPARE(Feedline::alphaNpPerM(0.0, 10e6, 7e6), 0.0);
        QCOMPARE(Feedline::alphaNpPerM(4.6, 0.0, 7e6), 0.0);
    }

    void the_catalogue_starts_with_no_cable_at_all()
    {
        const auto& c = Feedline::catalogue();
        QVERIFY(c.size() > 3);
        QCOMPARE(c.first().lossDb100m, 0.0);
        for (const auto& e : c) {
            QVERIFY(!e.name.isEmpty());
            QVERIFY(e.velocityFactor > 0.0 && e.velocityFactor <= 1.0);
            QVERIFY(e.lossDb100m >= 0.0);
            QVERIFY(e.refHz > 0.0);
        }
    }

    // ── The bug this file was written for ────────────────────────────
    //
    // Five metres of RG-58 in front of the dipole leaves exactly one
    // reactance crossing inside 7.0-7.4: a FALLING one at 7.163 MHz
    // with a resistance of 45 Ω. Plausible in every respect and wholly
    // an artefact of the line.
    //
    // The first nearestResonance() only rejected falling crossings
    // above 400 Ω, so it returned this one — 20 kHz below the truth, in
    // the wrong direction, and with no hint that anything was amiss.
    void a_feedline_artefact_is_not_mistaken_for_a_resonance()
    {
        const Sweep truth = synth(7.0e6, 7.4e6, 401, dipole);
        const auto real = AntennaSweep::nearestResonance(truth, 7.2e6);
        QVERIFY(real.found);
        QVERIFY(std::abs(real.freqHz - 7.183e6) < 2000.0);

        const Sweep seen = Feedline::embed(truth, 5.0, rg58());
        const auto fooled = AntennaSweep::nearestResonance(seen, 7.2e6);

        QVERIFY2(!fooled.found,
                 "a falling crossing produced by five metres of coax was "
                 "returned as the antenna's resonance");
        // There IS a crossing — just not a series one. The difference is
        // what the operator needs to be told.
        QVERIFY2(AntennaSweep::anyCrossing(seen),
                 "the artefact crossing should still be visible");

        const QString said = AntennaSweep::describe(seen, 7.2e6);
        QVERIFY2(said.contains(QStringLiteral("coax")), qPrintable(said));
    }

    void de_embedding_puts_the_resonance_back_where_it_belongs()
    {
        const Sweep truth = synth(7.0e6, 7.4e6, 401, dipole);

        for (double len : {1.0, 2.0, 5.0, 10.0}) {
            const Sweep seen = Feedline::embed(truth, len, rg58());
            const auto fixed = Feedline::deEmbed(seen, len, rg58());
            const auto r = AntennaSweep::nearestResonance(fixed.sweep,
                                                          7.2e6);
            QVERIFY2(r.found,
                     qPrintable(QStringLiteral("no resonance after "
                                               "removing %1 m").arg(len)));
            QVERIFY2(std::abs(r.freqHz - 7.183e6) < 2000.0,
                     qPrintable(QStringLiteral("%1 m: resonance came back "
                                               "at %2, wanted 7183000")
                                    .arg(len).arg(r.freqHz)));
        }
    }

    // Two metres is short enough that a crossing survives — and it is
    // in the wrong place, which is the quiet version of the same fault.
    // Python: 7.183 becomes 7.247, sixty-four kilohertz, about 20 cm of
    // wire.
    void a_short_cable_moves_the_resonance_without_hiding_it()
    {
        const Sweep truth = synth(7.0e6, 7.4e6, 401, dipole);
        const Sweep seen  = Feedline::embed(truth, 2.0, rg58());

        const auto wrong = AntennaSweep::nearestResonance(seen, 7.2e6);
        QVERIFY2(wrong.found, "two metres should still leave a crossing");
        QVERIFY2(wrong.freqHz - 7.183e6 > 40e3,
                 qPrintable(QStringLiteral("expected the apparent "
                                           "resonance to move well above "
                                           "7.183, got %1")
                                .arg(wrong.freqHz)));

        const auto fixed = Feedline::deEmbed(seen, 2.0, rg58());
        const auto right = AntennaSweep::nearestResonance(fixed.sweep,
                                                          7.2e6);
        QVERIFY(right.found);
        QVERIFY(std::abs(right.freqHz - 7.183e6) < 2000.0);
    }

    // The velocity factor decides how much line a metre of cable is.
    // Getting it wrong is getting the rotation wrong.
    void the_velocity_factor_changes_the_rotation()
    {
        const Sweep truth = synth(7.0e6, 7.4e6, 201, dipole);
        const Sweep slow = Feedline::embed(truth, 3.0, lossless(0.66));
        const Sweep fast = Feedline::embed(truth, 3.0, lossless(0.85));
        bool differs = false;
        for (int i = 0; i < slow.points.size(); ++i) {
            if (std::abs(slow.points.at(i).gamma
                         - fast.points.at(i).gamma) > 1e-3) {
                differs = true;
                break;
            }
        }
        QVERIFY2(differs, "the velocity factor had no effect");
    }
};

QTEST_APPLESS_MAIN(TstFeedline)
#include "tst_feedline.moc"
