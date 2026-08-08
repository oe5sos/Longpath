// =================================================================
// tests/tst_eq_loudness.cpp  (NereusSDR)
// =================================================================
//
// Loudness matching is the difference between an A/B comparison and a
// loudness comparison. If the maths is wrong the operator still hears
// something and still forms an opinion — they just form it about the
// wrong thing, and nothing on screen will tell them.
//
// So two kinds of test here. The weighting curves are checked against
// the published tables in IEC 61672 and ITU-R BS.1770, because a
// transcription error in a constant produces a plausible curve that is
// simply somebody else's. And the average is checked for the properties
// it must have whatever the weighting is: a flat equaliser adds
// nothing, a constant gain reports itself, and a boost where a voice
// has energy counts for more than the same boost where it has none.
//
// no-port-check: NereusSDR-original.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/EqLoudness.h"

#include <QtTest/QtTest>

#include <cmath>

using namespace NereusSDR;

namespace {

// An equaliser with one band, ready to be asked a question.
void setOne(ClientEq& eq, ClientEq::FilterType t, float hz, float gainDb,
            float q)
{
    ClientEq::BandParams p;
    p.type = t; p.freqHz = hz; p.gainDb = gainDb; p.q = q;
    p.enabled = true; p.slopeDbPerOct = 12;
    eq.setBand(0, p);
    eq.setActiveBandCount(1);
}

} // namespace

class TstEqLoudness : public QObject {
    Q_OBJECT
private slots:

    // ── The weighting curves, against the standards ──────────────────

    void a_weighting_matches_the_published_table()
    {
        // IEC 61672 class 1, to one decimal as published.
        const struct { double hz; double db; } kRef[] = {
            {10, -70.4},   {20, -50.4},   {50, -30.3},   {100, -19.1},
            {200, -10.9},  {500, -3.2},   {1000, 0.0},   {2000, 1.2},
            {4000, 1.0},   {8000, -1.1},  {10000, -2.5}, {20000, -9.3},
        };
        for (const auto& r : kRef) {
            const double got = EqLoudness::aWeightingDb(r.hz);
            QVERIFY2(std::abs(got - r.db) < 0.1,
                     qPrintable(QStringLiteral("%1 Hz: %2 dB, table says %3")
                                    .arg(r.hz).arg(got).arg(r.db)));
        }
        QVERIFY(std::abs(EqLoudness::aWeightingDb(1000.0)) < 0.01);
    }

    // The analog prototype must agree with the digital filter the
    // standard actually publishes. Checked at 48 kHz, where those
    // coefficients are defined; the two diverge slightly toward Nyquist
    // because the digital form carries the bilinear frequency warping
    // and the analog one does not, which is expected and is why the
    // tolerance widens above 1 kHz rather than being dropped.
    void k_weighting_matches_the_published_biquads()
    {
        const double b1[3] = {1.53512485958697, -2.69169618940638,
                              1.19839281085285};
        const double a1[3] = {1.0, -1.69065929318241, 0.73248077421585};
        const double b2[3] = {1.0, -2.0, 1.0};
        const double a2[3] = {1.0, -1.99004745483398, 0.99007225036621};

        auto digital = [&](double hz) {
            const double w = 2.0 * M_PI * hz / 48000.0;
            auto sec = [&](const double b[3], const double a[3]) {
                const double nr = b[0] + b[1] * std::cos(w)
                                       + b[2] * std::cos(2 * w);
                const double ni = -(b[1] * std::sin(w)
                                  + b[2] * std::sin(2 * w));
                const double dr = a[0] + a[1] * std::cos(w)
                                       + a[2] * std::cos(2 * w);
                const double di = -(a[1] * std::sin(w)
                                  + a[2] * std::sin(2 * w));
                return std::sqrt((nr * nr + ni * ni) / (dr * dr + di * di));
            };
            return 20.0 * std::log10(sec(b1, a1) * sec(b2, a2));
        };

        for (double hz : {20.0, 50.0, 100.0, 200.0, 400.0, 1000.0}) {
            const double d = digital(hz);
            const double a = EqLoudness::kWeightingDb(hz);
            QVERIFY2(std::abs(a - d) < 0.3,
                     qPrintable(QStringLiteral("%1 Hz: analog %2, digital %3")
                                    .arg(hz).arg(a).arg(d)));
        }
        for (double hz : {2000.0, 4000.0, 8000.0}) {
            const double d = digital(hz);
            const double a = EqLoudness::kWeightingDb(hz);
            QVERIFY2(std::abs(a - d) < 0.6,
                     qPrintable(QStringLiteral("%1 Hz: analog %2, digital %3")
                                    .arg(hz).arg(a).arg(d)));
        }
    }

    // The reason A-weighting is not used here, stated as a test so the
    // decision cannot be quietly reversed. A-weighting was designed for
    // very quiet sounds; using it made a six-decibel bass lift read as a
    // third of a decibel, which is audibly false.
    void k_weighting_discounts_bass_far_less_than_a_weighting()
    {
        const double aAt100 = EqLoudness::aWeightingDb(100.0)
                            - EqLoudness::aWeightingDb(1000.0);
        const double kAt100 = EqLoudness::kWeightingDb(100.0)
                            - EqLoudness::kWeightingDb(1000.0);
        QVERIFY(aAt100 < -15.0);          // about -19
        QVERIFY(kAt100 > -4.0);           // about -1.8
    }

    void speech_weight_peaks_in_the_low_hundreds()
    {
        const double at300 = EqLoudness::speechWeightDb(300.0);
        QVERIFY(at300 > EqLoudness::speechWeightDb(60.0) + 10.0);
        QVERIFY(at300 > EqLoudness::speechWeightDb(4000.0) + 15.0);
    }

    // ── The average ──────────────────────────────────────────────────

    void a_flat_equaliser_adds_nothing()
    {
        ClientEq eq;
        eq.prepare(48000.0);
        QCOMPARE(EqLoudness::addedLoudnessDb(eq), 0.0);   // no bands at all

        setOne(eq, ClientEq::FilterType::Peak, 1000.0f, 0.0f, 1.0f);
        QVERIFY(std::abs(EqLoudness::addedLoudnessDb(eq)) < 0.001);
    }

    // The property that makes the whole thing meaningful: a gain applied
    // equally everywhere must be reported as itself. A low shelf placed
    // above the audio band is flat across it to four decimal places, so
    // it is the one way to ask this question with the filters that
    // exist.
    void a_constant_gain_reports_itself()
    {
        for (float g : {-6.0f, -3.0f, 3.0f, 6.0f}) {
            ClientEq eq;
            eq.prepare(48000.0);
            setOne(eq, ClientEq::FilterType::LowShelf, 20000.0f, g, 0.707f);
            const double got = EqLoudness::addedLoudnessDb(eq);
            QVERIFY2(std::abs(got - double(g)) < 0.1,
                     qPrintable(QStringLiteral("%1 dB everywhere read as %2")
                                    .arg(g).arg(got)));
            // And the makeup is its opposite.
            QVERIFY(std::abs(EqLoudness::makeupDb(eq) + double(g)) < 0.1);
        }
    }

    // Where the boost is put has to matter, or the weighting is doing
    // nothing and this is an average of the wrong thing.
    void where_the_boost_sits_changes_what_it_costs()
    {
        auto added = [](float hz, float q) {
            ClientEq eq;
            eq.prepare(48000.0);
            setOne(eq, ClientEq::FilterType::Peak, hz, 6.0f, q);
            return EqLoudness::addedLoudnessDb(eq);
        };
        // Speech has most of its energy in the low-to-mid hundreds.
        const double atSpeech = added(400.0f, 1.0f);
        const double atTop    = added(5000.0f, 1.0f);
        QVERIFY2(atSpeech > atTop + 1.0,
                 qPrintable(QStringLiteral("400 Hz %1 dB vs 5 kHz %2 dB")
                                .arg(atSpeech).arg(atTop)));
        QVERIFY(atSpeech > 0.0);
        QVERIFY(atTop >= 0.0);
    }

    // A wide boost is more loudness than a narrow one of the same
    // height. Obvious, and the thing a decibel-domain average would get
    // wrong — which is why the average is taken in the power domain.
    void a_wide_boost_counts_for_more_than_a_narrow_one()
    {
        auto added = [](float q) {
            ClientEq eq;
            eq.prepare(48000.0);
            setOne(eq, ClientEq::FilterType::Peak, 500.0f, 6.0f, q);
            return EqLoudness::addedLoudnessDb(eq);
        };
        QVERIFY(added(0.3f) > added(1.0f));
        QVERIFY(added(1.0f) > added(4.0f));
    }

    void a_cut_is_negative_and_a_boost_is_positive()
    {
        ClientEq eq;
        eq.prepare(48000.0);
        setOne(eq, ClientEq::FilterType::Peak, 500.0f, -6.0f, 0.7f);
        QVERIFY(EqLoudness::addedLoudnessDb(eq) < -0.5);
        QVERIFY(EqLoudness::makeupDb(eq) > 0.5);

        setOne(eq, ClientEq::FilterType::Peak, 500.0f, 6.0f, 0.7f);
        QVERIFY(EqLoudness::addedLoudnessDb(eq) > 0.5);
        QVERIFY(EqLoudness::makeupDb(eq) < -0.5);
    }

    void a_disabled_band_does_not_count()
    {
        ClientEq eq;
        eq.prepare(48000.0);
        ClientEq::BandParams p;
        p.type = ClientEq::FilterType::Peak;
        p.freqHz = 500.0f; p.gainDb = 12.0f; p.q = 0.7f;
        p.enabled = false;
        eq.setBand(0, p);
        eq.setActiveBandCount(1);
        QVERIFY(std::abs(EqLoudness::addedLoudnessDb(eq)) < 0.001);
    }

    // The clamp exists so that a curve wanting an absurd correction
    // shows up as absurd rather than being silently accommodated.
    void the_makeup_is_clamped()
    {
        ClientEq eq;
        eq.prepare(48000.0);
        setOne(eq, ClientEq::FilterType::LowShelf, 20000.0f, 24.0f, 0.707f);
        QVERIFY(EqLoudness::addedLoudnessDb(eq) > 20.0);
        QCOMPARE(EqLoudness::makeupDb(eq), -EqLoudness::kMaxMakeupDb);
    }

    // Applying it must actually move the master gain, and applying it to
    // a flat curve must leave unity rather than something near it.
    void apply_sets_the_master_gain()
    {
        ClientEq eq;
        eq.prepare(48000.0);
        setOne(eq, ClientEq::FilterType::Peak, 1000.0f, 0.0f, 1.0f);
        EqLoudness::apply(eq);
        QVERIFY(std::abs(double(eq.masterGain()) - 1.0) < 0.001);

        setOne(eq, ClientEq::FilterType::LowShelf, 20000.0f, 6.0f, 0.707f);
        EqLoudness::apply(eq);
        // Six decibels off is a factor of about 0.501.
        QVERIFY2(std::abs(double(eq.masterGain()) - 0.5012) < 0.01,
                 qPrintable(QStringLiteral("master gain %1")
                                .arg(double(eq.masterGain()))));
    }
};

QTEST_APPLESS_MAIN(TstEqLoudness)
#include "tst_eq_loudness.moc"
