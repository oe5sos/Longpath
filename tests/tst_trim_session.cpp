// =================================================================
// tests/tst_trim_session.cpp  (NereusSDR)
// =================================================================
//
// AntennaTrim assumes f ∝ 1/L, which is true of an antenna in free
// space and of almost nothing on a summit. This works out the real
// exponent from two measurements either side of a known change.
//
// It matters more than it sounds. In the worked case the antenna
// responded half as much as the textbook rule predicted, and the
// corrected recommendation is +66 cm where the textbook still says
// +33. A factor of two, and the operator would have walked back down
// for nothing.
//
// So the tests are in two halves: that it learns the right number, and
// that it REFUSES to learn from measurements that cannot support one.
// The refusals are the important half — a wrong exponent fed forward is
// worse than no exponent at all.
//
// Numbers computed in Python before the C++ was written.
//
// no-port-check: NereusSDR-original.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/antenna/TrimSession.h"

#include <QtTest/QtTest>

#include <cmath>

using namespace Longpath;
using Kind = AntennaTrim::Kind;

class TstTrimSession : public QObject {
    Q_OBJECT
private slots:

    // ── Learning ─────────────────────────────────────────────────────

    void an_ideal_antenna_gives_an_exponent_of_one()
    {
        // f ∝ 1/L exactly. Three different steps, all of which must
        // come back as 1 — including a shortening.
        for (double newLen : {20.20, 20.44, 19.50}) {
            TrimSession s;
            s.record(20.00, 7.183e6);
            s.record(newLen, 7.183e6 * 20.00 / newLen);

            const auto l = s.learned();
            QVERIFY2(l.valid, qPrintable(l.note));
            QVERIFY2(std::abs(l.exponent - 1.0) < 1e-6,
                     qPrintable(QStringLiteral("length %1 gave k = %2")
                                    .arg(newLen).arg(l.exponent)));
            // What happened and what was predicted must agree here.
            QVERIFY(std::abs(l.movedHz - l.predictedHz) < 1.0);
            QVERIFY(l.note.contains(QStringLiteral("behaving")));
        }
    }

    // ── The one this exists for ──────────────────────────────────────
    //
    // Python: 22 cm on a 20 m dipole should move 78.2 kHz. This antenna
    // moves 39.1. k comes out at 0.4986, and the next step doubles.
    void an_antenna_that_responds_less_gets_a_bigger_next_step()
    {
        const double f1 = 7.183e6;
        const double L1 = 20.00, L2 = 20.22;
        const double predicted = f1 * L1 / L2;
        const double actual = f1 - (f1 - predicted) * 0.5;

        TrimSession s;
        s.record(L1, f1);
        s.record(L2, actual);

        const auto l = s.learned();
        QVERIFY2(l.valid, qPrintable(l.note));
        QVERIFY2(std::abs(l.exponent - 0.4986) < 0.001,
                 qPrintable(QStringLiteral("k = %1, expected 0.4986")
                                .arg(l.exponent)));
        QVERIFY2(l.note.contains(QStringLiteral("less than")),
                 qPrintable(l.note));

        // Python: textbook +16.4 cm per leg, learned +33.1 cm per leg.
        const auto textbook =
            AntennaTrim::compute(Kind::Dipole, actual, 7.030e6, L2);
        const auto learnt = s.recommend(Kind::Dipole, 7.030e6, L2);

        QVERIFY(std::abs(textbook.perElementM - 0.164) < 0.002);
        QVERIFY2(std::abs(learnt.perElementM - 0.331) < 0.003,
                 qPrintable(QStringLiteral("learned step %1 m, expected "
                                           "0.331").arg(learnt.perElementM)));
        QVERIFY2(learnt.perElementM > textbook.perElementM * 1.8,
                 "the corrected step should be about twice the textbook "
                 "one");
    }

    void an_antenna_that_responds_more_gets_a_smaller_next_step()
    {
        // Python: -40 cm on a 20 m wire, moving further than predicted,
        // gives k = 1.3944.
        const double f1 = 6.900e6;
        const double L1 = 20.00, L2 = 19.60;
        const double predicted = f1 * L1 / L2;
        const double actual = f1 + (predicted - f1) * 1.4;

        TrimSession s;
        s.record(L1, f1);
        s.record(L2, actual);

        const auto l = s.learned();
        QVERIFY2(l.valid, qPrintable(l.note));
        QVERIFY2(std::abs(l.exponent - 1.3944) < 0.002,
                 qPrintable(QStringLiteral("k = %1").arg(l.exponent)));
        QVERIFY(l.note.contains(QStringLiteral("more than")));
    }

    // ── The refusals, which matter more ──────────────────────────────

    void one_measurement_teaches_nothing()
    {
        TrimSession s;
        s.record(20.0, 7.183e6);
        const auto l = s.learned();
        QVERIFY(!l.valid);
        QCOMPARE(l.exponent, 1.0);      // falls back, does not invent
        QVERIFY(!l.note.isEmpty());
    }

    void a_length_that_barely_changed_teaches_nothing()
    {
        TrimSession s;
        s.record(20.00, 7.183e6);
        s.record(20.02, 7.176e6);       // 2 cm, under half a percent
        const auto l = s.learned();
        QVERIFY2(!l.valid, "2 cm on 20 m is inside the noise");
        QVERIFY(l.note.contains(QStringLiteral("cm")));
    }

    void a_resonance_that_barely_moved_teaches_nothing()
    {
        TrimSession s;
        s.record(20.00, 7.183e6);
        s.record(20.30, 7.182e6);       // 30 cm and only 1 kHz
        const auto l = s.learned();
        QVERIFY2(!l.valid, "30 cm should move far more than 1 kHz");
        // And it should say that something is wrong rather than shrug.
        QVERIFY(l.note.contains(QStringLiteral("something else")));
    }

    // Lengthening a wire lowers its resonance. If it rose, the length is
    // not what changed, and an exponent from that is negative nonsense.
    void a_resonance_moving_the_wrong_way_is_refused_and_explained()
    {
        TrimSession s;
        s.record(20.00, 7.183e6);
        s.record(20.22, 7.250e6);       // longer AND higher
        const auto l = s.learned();
        QVERIFY2(!l.valid, "a negative exponent was accepted");
        QCOMPARE(l.exponent, 1.0);
        QVERIFY2(l.note.contains(QStringLiteral("backwards")),
                 qPrintable(l.note));
        QVERIFY(l.note.contains(QStringLiteral("counterpoise")));
    }

    void without_a_length_there_is_nothing_to_compare()
    {
        TrimSession s;
        s.record(0.0,   7.183e6);       // length unknown
        s.record(20.22, 7.140e6);
        const auto l = s.learned();
        QVERIFY(!l.valid);
        QVERIFY(l.note.contains(QStringLiteral("length")));
    }

    void an_invalid_learning_still_recommends_by_the_textbook()
    {
        TrimSession s;
        s.record(20.00, 7.183e6);
        s.record(20.22, 7.250e6);       // backwards, so nothing learned
        QVERIFY(!s.learned().valid);

        const auto viaSession = s.recommend(Kind::Dipole, 7.030e6, 20.22);
        const auto textbook =
            AntennaTrim::compute(Kind::Dipole, 7.250e6, 7.030e6, 20.22);
        QVERIFY(viaSession.valid);
        QVERIFY2(std::abs(viaSession.perElementM
                          - textbook.perElementM) < 1e-12,
                 "a refused exponent must leave the textbook answer "
                 "untouched");
    }

    // ── Housekeeping ─────────────────────────────────────────────────

    void a_measurement_with_no_resonance_is_not_recorded()
    {
        TrimSession s;
        s.record(20.0, 0.0);
        s.record(20.0, -1.0);
        QCOMPARE(s.count(), 0);
    }

    void observations_are_kept_in_order_with_a_timestamp()
    {
        TrimSession s;
        s.record(20.00, 7.183e6, QStringLiteral("first"));
        s.record(20.22, 7.140e6, QStringLiteral("second"));
        QCOMPARE(s.count(), 2);
        QCOMPARE(s.observations().first().label, QStringLiteral("first"));
        QCOMPARE(s.observations().last().label,  QStringLiteral("second"));
        QVERIFY(s.observations().first().when.isValid());
    }

    void only_the_last_two_are_used()
    {
        // An old, badly behaved pair must not poison a good recent one.
        TrimSession s;
        s.record(20.00, 7.183e6);
        s.record(20.22, 7.250e6);      // backwards
        QVERIFY(!s.learned().valid);

        s.record(20.62, 7.108e6);      // 40 cm, sensible movement
        const auto l = s.learned();
        QVERIFY2(l.valid, qPrintable(l.note));
    }

    // The window looks for the crossing NEAREST THE TARGET, so changing
    // the target can legitimately pick a different one — on an end-fed
    // swept across HF, certainly. The last observation has to keep
    // describing the measurement on screen, or recommend() answers
    // about a resonance nobody is looking at.
    void the_last_measurement_can_be_corrected_after_the_fact()
    {
        TrimSession s;
        s.record(20.00, 7.183e6);
        s.record(0.0,   7.140e6);          // length not typed yet
        QVERIFY2(!s.learned().valid, "a missing length cannot teach");

        s.updateLastLength(20.22);
        const auto l = s.learned();
        QVERIFY2(l.valid, qPrintable(l.note));

        // And the resonance, for when the target moves.
        s.updateLastResonance(7.150e6);
        QCOMPARE(s.observations().last().resonanceHz, 7.150e6);
        QCOMPARE(s.observations().last().lengthM, 20.22);
    }

    void correcting_an_empty_session_or_with_nonsense_does_nothing()
    {
        TrimSession s;
        s.updateLastLength(20.0);
        s.updateLastResonance(7.0e6);
        QCOMPARE(s.count(), 0);

        s.record(20.00, 7.183e6);
        s.updateLastResonance(0.0);
        s.updateLastResonance(-1.0);
        s.updateLastLength(-5.0);
        QCOMPARE(s.observations().last().resonanceHz, 7.183e6);
        QCOMPARE(s.observations().last().lengthM, 20.00);
    }

    void clearing_forgets_everything()
    {
        TrimSession s;
        s.record(20.00, 7.183e6);
        s.record(20.44, 7.028e6);
        QVERIFY(s.learned().valid);
        s.clear();
        QCOMPARE(s.count(), 0);
        QVERIFY(!s.learned().valid);
    }

    // ── The exponent must not disturb the ordinary path ──────────────

    void an_exponent_of_one_is_exactly_the_old_answer()
    {
        // pow(x, 1.0) is very nearly x, and "very nearly" is how a
        // passing test starts failing by 1e-16. compute() short-circuits
        // for exactly 1; this pins it.
        const auto plain =
            AntennaTrim::compute(Kind::Dipole, 7.183e6, 7.030e6, 20.0);
        const auto explicitOne =
            AntennaTrim::compute(Kind::Dipole, 7.183e6, 7.030e6, 20.0, 1.0);
        QCOMPARE(explicitOne.perElementM, plain.perElementM);
        QCOMPARE(explicitOne.percentChange, plain.percentChange);
    }

    void a_nonsense_exponent_falls_back_rather_than_exploding()
    {
        for (double bad : {0.0, -1.0, 1e6}) {
            const auto t = AntennaTrim::compute(Kind::Dipole, 7.183e6,
                                                7.030e6, 20.0, bad);
            QVERIFY(t.valid);
            QVERIFY(std::isfinite(t.perElementM));
            QVERIFY2(std::abs(t.perElementM - 0.21764) < 0.001,
                     qPrintable(QStringLiteral("exponent %1 did not fall "
                                               "back to the textbook "
                                               "answer").arg(bad)));
        }
    }
};

QTEST_APPLESS_MAIN(TstTrimSession)
#include "tst_trim_session.moc"
