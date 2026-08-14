// =================================================================
// tests/tst_coupler_zero.cpp  (NereusSDR)
// =================================================================
//
// The zero of a directional coupler, measured instead of tabled.
//
// This is the term that made an 80 m sweep draw SWR 1.00 along the
// bottom of the band where the operator's VNA said 2.5: a tabled offset
// of 28 counts subtracted from a board that idles at 0, deleting every
// reverse reading small enough to matter. The same table is wrong the
// other way on other boards, where it invents forward power during
// receive.
//
// So these tests are mostly about the two ways a measured zero can be
// wrong in turn: biased upward by the residue after a carrier drops,
// and pinned downward by a single dropout. Both were real risks in the
// first draft; the settle guard and the percentile are the answers.
//
// no-port-check: NereusSDR-original.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-14 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/CouplerZero.h"

#include <QtTest/QtTest>

using namespace NereusSDR;

namespace {

void feedRx(CouplerZero& z, quint16 fwd, quint16 rev, int n)
{
    for (int i = 0; i < n; ++i) { z.observe(fwd, rev, /*tx=*/false); }
}

} // namespace

class TstCouplerZero : public QObject {
    Q_OBJECT
private slots:

    // ── Before it knows anything ─────────────────────────────────────

    void it_says_it_does_not_know_and_hands_back_the_fallback()
    {
        // A zero guessed from three samples is worse than one from a
        // table. Until it has grounds, the caller keeps what it had.
        CouplerZero z;
        QVERIFY(!z.known());
        QCOMPARE(z.forwardZero(32), quint16(32));
        QCOMPARE(z.reverseZero(28), quint16(28));

        feedRx(z, 0, 0, CouplerZero::kMinSamples - 1);
        QVERIFY2(!z.known(), "answered before it had enough samples");
        QCOMPARE(z.forwardZero(32), quint16(32));
    }

    void it_knows_once_it_has_enough()
    {
        CouplerZero z;
        feedRx(z, 0, 0, CouplerZero::kMinSamples);
        QVERIFY(z.known());
    }

    // ── The measurement this exists for ──────────────────────────────

    void a_board_that_idles_at_zero_measures_zero()
    {
        // OE5SOS's Anvelina, 2026-08-14: idle reads VOR 0 · RÜCK 0
        // while the table wants 32 and 28 subtracted.
        CouplerZero z;
        feedRx(z, 0, 0, 200);
        QCOMPARE(z.forwardZero(32), quint16(0));
        QCOMPARE(z.reverseZero(28), quint16(0));
    }

    void a_board_with_a_real_pedestal_measures_the_pedestal()
    {
        // The other direction, and just as important: a tabled 3 on a
        // board that idles at 30 reports power while receiving.
        CouplerZero z;
        feedRx(z, 30, 24, 200);
        QCOMPARE(z.forwardZero(3), quint16(30));
        QCOMPARE(z.reverseZero(3), quint16(24));
    }

    void the_two_channels_are_measured_separately()
    {
        CouplerZero z;
        feedRx(z, 40, 12, 200);
        QCOMPARE(z.forwardZero(0), quint16(40));
        QCOMPARE(z.reverseZero(0), quint16(12));
    }

    // ── Bias upward: the residue after a carrier ─────────────────────
    //
    // The detector falls slowly. Samples taken just after the carrier
    // drops are decaying transmit power, not a zero, and a mean would
    // fold them straight in.
    void residue_after_transmitting_is_not_mistaken_for_the_zero()
    {
        CouplerZero z;
        feedRx(z, 5, 2, 200);
        const quint16 before = z.forwardZero(0);
        QCOMPARE(before, quint16(5));

        // Key, then release and let the reading decay through hundreds
        // of counts before settling back.
        for (int i = 0; i < 40; ++i) { z.observe(900, 120, /*tx=*/true); }
        for (int i = 0; i < CouplerZero::kSettleSamples; ++i) {
            z.observe(quint16(600 - i * 20), quint16(90 - i * 3), false);
        }
        QVERIFY2(z.forwardZero(0) <= 5,
                 qPrintable(QStringLiteral("the decay tail moved the zero "
                                           "to %1").arg(z.forwardZero(0))));

        feedRx(z, 5, 2, 60);
        QCOMPARE(z.forwardZero(0), quint16(5));
    }

    void samples_taken_while_transmitting_are_never_learned()
    {
        CouplerZero z;
        for (int i = 0; i < 500; ++i) { z.observe(900, 120, /*tx=*/true); }
        QVERIFY2(!z.known(),
                 "it learned a zero from a transmitting radio");
    }

    // ── Bias downward: one dropout ───────────────────────────────────
    //
    // An outright minimum would be pinned by a single zero sample for
    // the life of the window. A low percentile has a sample of slack.
    void one_dropout_does_not_pin_the_zero()
    {
        CouplerZero z;
        feedRx(z, 40, 12, 200);
        z.observe(0, 0, false);              // one bad sample
        feedRx(z, 40, 12, 20);
        QCOMPARE(z.forwardZero(0), quint16(40));
    }

    // But a genuine change must still get through, or the whole point
    // of measuring rather than tabling is lost.
    void a_board_that_drifts_is_followed()
    {
        CouplerZero z;
        feedRx(z, 40, 12, 300);
        QCOMPARE(z.forwardZero(0), quint16(40));

        // Warms up; the old value must age out of the window.
        feedRx(z, 55, 20, CouplerZero::kWindow + 10);
        QCOMPARE(z.forwardZero(0), quint16(55));
    }

    void the_zero_sits_at_the_quiet_end_of_a_noisy_window()
    {
        // Real telemetry jitters. The zero is the floor, not the
        // average — an average would sit above every quiet moment and
        // clip small readings, which is the original fault.
        CouplerZero z;
        for (int i = 0; i < 240; ++i) {
            z.observe(quint16(20 + (i % 9)), quint16(8 + (i % 5)), false);
        }
        const quint16 f = z.forwardZero(0);
        QVERIFY2(f >= 20 && f <= 22,
                 qPrintable(QStringLiteral("zero came out at %1, expected "
                                           "the low end of 20..28").arg(f)));
    }

    // ── Starting again ───────────────────────────────────────────────

    void a_reset_forgets_the_previous_radio()
    {
        CouplerZero z;
        feedRx(z, 40, 12, 200);
        QVERIFY(z.known());
        z.reset();
        QVERIFY2(!z.known(), "a zero survived a reconnection");
        QCOMPARE(z.forwardZero(7), quint16(7));
    }

    // A fresh object has not just stopped transmitting, so it must not
    // spend its first half second refusing to learn.
    void a_fresh_object_starts_learning_immediately()
    {
        CouplerZero z;
        feedRx(z, 11, 4, CouplerZero::kMinSamples);
        QVERIFY(z.known());
        QCOMPARE(z.forwardZero(0), quint16(11));
    }
};

QTEST_APPLESS_MAIN(TstCouplerZero)
#include "tst_coupler_zero.moc"
