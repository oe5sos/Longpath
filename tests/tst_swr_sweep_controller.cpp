// =================================================================
// tests/tst_swr_sweep_controller.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original feature test (Thetis has no SWR
// sweep analyzer).
//
// The controller's physics enter through ingestTelemetry and leave
// through the injected txFrequencyFn — so a synthetic dipole (SWR
// computed from a known resonance curve, converted back to fwd/rev
// watts) drives the whole state machine without a radio, and the
// sweep must find the resonance that was planted. The state-machine
// contracts (restore-on-every-exit, refusal gates, aborts) are pinned
// alongside.
//
// MoxController is exercised for real (its Rx/Tx state machine runs
// on timers); the BandPlanGuard check is not installed, so setTune
// succeeds without a radio — exactly the seam TwoToneController's
// tests use.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-13 — Created by Ralph Martin Fischer (OE5SOS),
//                 AI-assisted implementation via Anthropic Claude
//                 (Cowork).
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/SwrSweepController.h"
#include "core/MoxController.h"

#include <cmath>

using namespace NereusSDR;

namespace {

// A synthetic dipole resonant at fRes: SWR rises linearly with
// |f - fRes|, 1.1 at resonance, ~3 at ±150 kHz. Inverted into fwd/rev
// watts the way the bridge would report them at 10 W drive.
struct FakeDipole {
    double fResHz{14150000.0};
    double swrAt(double fHz) const
    {
        return 1.1 + std::fabs(fHz - fResHz) / 150000.0 * 1.9;
    }
    void wattsAt(double fHz, double& fwdW, double& revW) const
    {
        const double swr = swrAt(fHz);
        const double gamma = (swr - 1.0) / (swr + 1.0);
        fwdW = 10.0;
        revW = fwdW * gamma * gamma;
    }
};

struct Harness {
    MoxController      mox;
    SwrSweepController ctl;
    quint64            currentFreq{0};
    int                restoreCalls{0};

    Harness()
    {
        ctl.setMoxController(&mox);
        ctl.setTxFrequencyFn([this](quint64 hz) { currentFreq = hz; });
        ctl.setTxFreqRestoreFn([this]() { ++restoreCalls; });
        ctl.setReadyFn([]() { return true; });
        // Compress every wait so a full sweep runs in tens of ms.
        ctl.setTimingsForTest(/*tuneSettle*/ 1, /*telemetryTimeout*/ 200);
    }

    SwrSweepPlan tinyPlan(int points = 21)
    {
        SwrSweepPlan plan;
        plan.band    = Band::Band20m;
        plan.startHz = 14000000;
        plan.stopHz  = 14350000;
        plan.points  = points;
        plan.settleMs = 1;
        plan.dwellMs  = 5;
        return plan;
    }

    // Pump telemetry for the fake antenna while the event loop spins.
    void pumpUntilFinished(const FakeDipole& dipole, int timeoutMs = 5000)
    {
        QSignalSpy fin(&ctl, &SwrSweepController::sweepFinished);
        QElapsedTimer t;
        t.start();
        while (fin.isEmpty() && t.elapsed() < timeoutMs) {
            double fwd = 0.0;
            double rev = 0.0;
            dipole.wattsAt(static_cast<double>(currentFreq), fwd, rev);
            ctl.ingestTelemetry(fwd, rev);
            QTest::qWait(2);
        }
    }
};

} // namespace

class TstSwrSweepController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<SwrSweepPlan>("NereusSDR::SwrSweepPlan");
        qRegisterMetaType<SwrSweepResult>("NereusSDR::SwrSweepResult");
        qRegisterMetaType<SwrSweepPlan>("SwrSweepPlan");
        qRegisterMetaType<SwrSweepResult>("SwrSweepResult");
    }

    // ── The pure math ────────────────────────────────────────────────
    void swr_math_matches_the_bridge_identities()
    {
        // Perfect match: no reflection.
        QVERIFY(std::fabs(SwrSweepController::swrFromWatts(10.0, 0.0) - 1.0)
                < 1e-9);
        // SWR 2.0 → gamma 1/3 → rev = fwd/9.
        QVERIFY(std::fabs(SwrSweepController::swrFromWatts(9.0, 1.0) - 2.0)
                < 1e-6);
        // Total reflection caps at 99.
        QCOMPARE(SwrSweepController::swrFromWatts(10.0, 10.0), 99.0);
        QCOMPARE(SwrSweepController::swrFromWatts(10.0, 20.0), 99.0);
        // Below the bridge floor: invalid, not a number.
        QCOMPARE(SwrSweepController::swrFromWatts(0.2, 0.0), 0.0);
    }

    // ── The planner ──────────────────────────────────────────────────
    void plan_seeds_from_the_band_table_and_refuses_pseudo_bands()
    {
        const SwrSweepPlan p20 = SwrSweepPlan::forBand(Band::Band20m);
        QVERIFY(p20.isValid());
        QCOMPARE(p20.startHz, quint64(14000000));
        QCOMPARE(p20.stopHz,  quint64(14350000));

        QVERIFY(!SwrSweepPlan::forBand(Band::GEN).isValid());
        QVERIFY(!SwrSweepPlan::forBand(Band::WWV).isValid());
        QVERIFY(!SwrSweepPlan::forBand(Band::XVTR).isValid());
        // 60 m: channelized, excluded in v1 (design doc §Deferred).
        QVERIFY(!SwrSweepPlan::forBand(Band::Band60m).isValid());
    }

    void plan_clips_to_the_regional_band_edges()
    {
        // Region 1 ends 40 m at 7.200 MHz; the Region-2 seed reaches
        // 7.300. The clip must trim the tail, not fail the plan.
        safety::BandPlanGuard guard;
        SwrSweepPlan plan = SwrSweepPlan::forBand(Band::Band40m);
        plan.points = 31;
        QVERIFY(plan.clipToGuard(guard, safety::Region::Region1,
                                 DSPMode::LSB));
        QCOMPARE(plan.startHz, quint64(7000000));
        QVERIFY(plan.stopHz <= quint64(7200000));
        QVERIFY(plan.points >= SwrSweepPlan::kMinPoints);
    }

    // ── The sweep finds the planted resonance ────────────────────────
    void sweep_finds_the_synthetic_resonance()
    {
        Harness h;
        FakeDipole dipole;   // resonant at 14.150 MHz

        QSignalSpy fin(&h.ctl, &SwrSweepController::sweepFinished);
        QVERIFY(h.ctl.startSweep(h.tinyPlan()));
        h.pumpUntilFinished(dipole);
        QCOMPARE(fin.count(), 1);

        const auto result =
            fin.first().first().value<SwrSweepResult>();
        QVERIFY(result.completed);
        QCOMPARE(result.points.size(), 21);

        // The resonance must land on the grid point nearest 14.150 MHz
        // (grid step is 17.5 kHz on 21 points across 350 kHz).
        const quint64 res = result.resonanceHz();
        QVERIFY2(std::llabs(static_cast<long long>(res) - 14150000LL)
                     <= 17500,
                 qPrintable(QStringLiteral("resonance found at %1")
                                .arg(res)));
        QVERIFY(result.minSwr() < 1.3);

        // Restore ran exactly once, TUNE is released.
        QTRY_COMPARE(h.mox.state(), MoxState::Rx);
        QCOMPARE(h.restoreCalls, 1);
    }

    // ── Refusals and aborts ──────────────────────────────────────────
    void busy_and_invalid_plans_are_refused()
    {
        Harness h;
        FakeDipole dipole;
        QVERIFY(h.ctl.startSweep(h.tinyPlan()));
        // Second start while running: refused.
        QVERIFY(!h.ctl.startSweep(h.tinyPlan()));
        h.pumpUntilFinished(dipole);

        // Invalid plan: refused outright.
        SwrSweepPlan bad;
        QVERIFY(!h.ctl.startSweep(bad));
    }

    void operator_abort_restores_and_reports()
    {
        Harness h;
        FakeDipole dipole;
        QSignalSpy fin(&h.ctl, &SwrSweepController::sweepFinished);
        QSignalSpy pts(&h.ctl, &SwrSweepController::pointReady);
        QVERIFY(h.ctl.startSweep(h.tinyPlan(101)));

        // Let a few points land, then pull the plug.
        QElapsedTimer t;
        t.start();
        while (pts.count() < 5 && t.elapsed() < 3000) {
            double fwd = 0.0;
            double rev = 0.0;
            dipole.wattsAt(static_cast<double>(h.currentFreq), fwd, rev);
            h.ctl.ingestTelemetry(fwd, rev);
            QTest::qWait(2);
        }
        QVERIFY(pts.count() >= 5);
        h.ctl.abortSweep(QStringLiteral("test abort"));

        QTRY_COMPARE(fin.count(), 1);
        const auto result =
            fin.first().first().value<SwrSweepResult>();
        QVERIFY(!result.completed);
        QCOMPARE(result.abortReason, QStringLiteral("test abort"));
        QTRY_COMPARE(h.mox.state(), MoxState::Rx);
        QCOMPARE(h.restoreCalls, 1);
    }

    void telemetry_silence_aborts()
    {
        Harness h;
        QSignalSpy fin(&h.ctl, &SwrSweepController::sweepFinished);
        QVERIFY(h.ctl.startSweep(h.tinyPlan()));
        // Feed NOTHING: the 200 ms test watchdog must abort.
        QTRY_COMPARE_WITH_TIMEOUT(fin.count(), 1, 3000);
        const auto result =
            fin.first().first().value<SwrSweepResult>();
        QVERIFY(!result.completed);
        QVERIFY(result.abortReason.contains(QStringLiteral("telemetry"),
                                            Qt::CaseInsensitive)
                || result.abortReason.contains(QStringLiteral("Telemetry"),
                                               Qt::CaseInsensitive)
                || !result.abortReason.isEmpty());
        QTRY_COMPARE(h.mox.state(), MoxState::Rx);
        QCOMPARE(h.restoreCalls, 1);
    }

    void open_feedline_aborts_after_three_points()
    {
        Harness h;
        QSignalSpy fin(&h.ctl, &SwrSweepController::sweepFinished);
        QVERIFY(h.ctl.startSweep(h.tinyPlan()));
        // Total reflection everywhere: rev == fwd.
        QElapsedTimer t;
        t.start();
        while (fin.isEmpty() && t.elapsed() < 3000) {
            h.ctl.ingestTelemetry(10.0, 10.0);
            QTest::qWait(2);
        }
        QCOMPARE(fin.count(), 1);
        const auto result =
            fin.first().first().value<SwrSweepResult>();
        QVERIFY(!result.completed);
        QVERIFY(result.points.size() >= SwrSweepController::kAbortSwrRun);
        QVERIFY(result.points.size() < 10);   // aborted early, not at the end
        QTRY_COMPARE(h.mox.state(), MoxState::Rx);
    }

    // ── The bench, 2026-08-14 ────────────────────────────────────────
    //
    // Tune Pwr sat at 1 W. The sweep keyed all fifty-one points, the
    // bridge never rose above its floor, every point was discarded, and
    // the verdict — "no valid measurements, forward power too low?" —
    // arrived seventeen seconds later with a question mark. Seventeen
    // seconds of transmitting to learn nothing, and then a guess.
    //
    // The mirror of the open-feedline guard: stop when the bridge is
    // telling us nothing, and say it in watts.
    void a_bridge_that_reads_nothing_stops_the_sweep_early()
    {
        Harness h;
        QSignalSpy fin(&h.ctl, &SwrSweepController::sweepFinished);
        QVERIFY(h.ctl.startSweep(h.tinyPlan(/*points*/ 21)));

        // Under the floor at every point — 1 W of tune power into an
        // ANAN's coupler, near enough.
        QElapsedTimer t;
        t.start();
        while (fin.isEmpty() && t.elapsed() < 3000) {
            h.ctl.ingestTelemetry(0.2, 0.02);
            QTest::qWait(2);
        }
        QCOMPARE(fin.count(), 1);
        const auto result = fin.first().first().value<SwrSweepResult>();

        QVERIFY(!result.completed);
        QCOMPARE(result.validPoints(), 0);
        QVERIFY2(result.points.size() >= SwrSweepController::kAbortDeadRun,
                 "gave up before it had grounds to");
        QVERIFY2(result.points.size() < 21,
                 "transmitted the whole plan before noticing it was "
                 "measuring nothing");

        // The reason has to carry the number. "Too low" is not
        // actionable; "0.20 W measured, 0.5 W needed" is.
        QVERIFY2(result.abortReason.contains(QStringLiteral("0.20")),
                 qPrintable(result.abortReason));
        QVERIFY(result.maxFwdW > 0.0 && result.maxFwdW < 0.5);
        QTRY_COMPARE(h.mox.state(), MoxState::Rx);
    }

    // ── The bench, second run ────────────────────────────────────────
    //
    // Tune power raised to 5 W and the bridge still reported 0.01 W.
    // That is not a small reading, it is no reading, and "raise the
    // tune power" would send the operator winding the power up against
    // a fault somewhere else. The two cases get different sentences.
    void a_silent_bridge_is_not_reported_as_merely_weak()
    {
        Harness h;
        QSignalSpy fin(&h.ctl, &SwrSweepController::sweepFinished);
        QVERIFY(h.ctl.startSweep(h.tinyPlan(/*points*/ 21)));

        QElapsedTimer t;
        t.start();
        while (fin.isEmpty() && t.elapsed() < 3000) {
            h.ctl.ingestTelemetry(0.01, 0.0);   // what the ANAN reported
            QTest::qWait(2);
        }
        QCOMPARE(fin.count(), 1);
        const auto result = fin.first().first().value<SwrSweepResult>();

        QVERIFY(!result.completed);
        QVERIFY(result.maxFwdW < SwrSweepController::kSilentBridgeW);
        // Must NOT tell the operator to turn the power up.
        QVERIFY2(!result.abortReason.contains(QStringLiteral("höher"),
                                              Qt::CaseInsensitive),
                 qPrintable(result.abortReason));
        // Must point at the thing to check by hand instead.
        QVERIFY2(result.abortReason.contains(QStringLiteral("TUNE")),
                 qPrintable(result.abortReason));
    }

    void a_weak_bridge_still_says_raise_the_power()
    {
        // The other side of the same fork: 0.3 W IS a reading, just
        // under the floor, and more power is genuinely the remedy.
        Harness h;
        QSignalSpy fin(&h.ctl, &SwrSweepController::sweepFinished);
        QVERIFY(h.ctl.startSweep(h.tinyPlan(/*points*/ 21)));

        QElapsedTimer t;
        t.start();
        while (fin.isEmpty() && t.elapsed() < 3000) {
            h.ctl.ingestTelemetry(0.30, 0.02);
            QTest::qWait(2);
        }
        QCOMPARE(fin.count(), 1);
        const auto result = fin.first().first().value<SwrSweepResult>();
        QVERIFY(result.maxFwdW > SwrSweepController::kSilentBridgeW);
        QVERIFY2(result.abortReason.contains(QStringLiteral("Tune-Leistung")),
                 qPrintable(result.abortReason));
    }

    // The fork only works while the silent threshold sits below the
    // measurement floor; if they ever cross, one branch becomes
    // unreachable and the operator gets the wrong advice forever.
    void the_silent_threshold_sits_below_the_measurement_floor()
    {
        QVERIFY(SwrSweepController::kSilentBridgeW
                < SwrSweepController::kMinFwdW);
        QVERIFY(SwrSweepController::kSilentBridgeW > 0.0);
    }

    // ── QRP: the reason the baseline exists ──────────────────────────
    //
    // "ich werde mit dem anan 10e sota funken, tunen muss also mit
    //  1 watt auch gehen, ausgangsleistung qrp maximal 10 watt."
    //
    // A ten-watt radio at one watt puts perhaps a fifth of a watt into
    // the old fixed floor's face and every point was discarded. The
    // coupler on such a radio is scaled for such a radio: the reading
    // is SMALL, not ABSENT, and small well clear of the noise is a
    // perfectly good measurement. Judging against the radio's own idle
    // count is what tells the two apart.
    void a_qrp_sweep_measures_even_though_the_watts_are_under_the_old_floor()
    {
        Harness h;
        FakeDipole dipole;
        QSignalSpy fin(&h.ctl, &SwrSweepController::sweepFinished);
        QVERIFY(h.ctl.startSweep(h.tinyPlan()));

        // Idle ADC sits at 40 counts; keying at ~1 W lifts it to 300.
        // Watts stay at 0.2 — well under kMinFwdW, which used to throw
        // the whole sweep away.
        constexpr quint16 kIdleRaw = 40;
        constexpr quint16 kKeyedRaw = 300;
        QElapsedTimer t;
        t.start();
        while (fin.isEmpty() && t.elapsed() < 5000) {
            double fwd = 0.0;
            double rev = 0.0;
            dipole.wattsAt(static_cast<double>(h.currentFreq), fwd, rev);
            // Scale the fake antenna down to QRP levels; the RATIO,
            // which is all SWR depends on, is untouched.
            const double qrpScale = 0.2 / 10.0;
            const bool keyed = (h.mox.state() != MoxState::Rx);
            h.ctl.ingestTelemetry(fwd * qrpScale, rev * qrpScale,
                                  keyed ? kKeyedRaw : kIdleRaw);
            QTest::qWait(2);
        }

        QCOMPARE(fin.count(), 1);
        const auto result = fin.first().first().value<SwrSweepResult>();
        QVERIFY2(result.completed,
                 qPrintable(QStringLiteral("QRP sweep aborted: %1")
                                .arg(result.abortReason)));
        QVERIFY2(result.validPoints() > 0,
                 "a QRP sweep well clear of the noise measured nothing");
        QVERIFY2(result.maxFwdW < SwrSweepController::kMinFwdW,
                 "the test is not exercising the case it claims to — "
                 "these watts are above the old floor");
        QVERIFY2(result.baselineRaw >= kIdleRaw - 2
                     && result.baselineRaw <= kIdleRaw + 2,
                 qPrintable(QStringLiteral("baseline came out at %1, "
                                           "expected about %2")
                                .arg(result.baselineRaw).arg(kIdleRaw)));
    }

    // The mirror: a count that never leaves the idle reading is not a
    // measurement however many watts the scaling claims for it.
    void an_adc_that_never_leaves_its_idle_reading_is_refused()
    {
        Harness h;
        QSignalSpy fin(&h.ctl, &SwrSweepController::sweepFinished);
        QVERIFY(h.ctl.startSweep(h.tinyPlan()));

        // 41 counts throughout, keyed or not — OE5SOS's Anvelina.
        QElapsedTimer t;
        t.start();
        while (fin.isEmpty() && t.elapsed() < 3000) {
            h.ctl.ingestTelemetry(5.0, 0.5, 41);
            QTest::qWait(2);
        }
        QCOMPARE(fin.count(), 1);
        const auto result = fin.first().first().value<SwrSweepResult>();
        QVERIFY2(!result.completed, "a dead ADC produced a finished sweep");
        QCOMPARE(result.validPoints(), 0);
        // Even though the watts handed over were generous.
        QVERIFY2(result.abortReason.contains(QStringLiteral("ADC")),
                 qPrintable(result.abortReason));
    }

    void the_baseline_is_taken_before_anything_is_keyed()
    {
        // A baseline sampled with the carrier up is not a baseline. The
        // controller must still be in Rx while it collects.
        Harness h;
        QVERIFY(h.ctl.startSweep(h.tinyPlan()));
        QCOMPARE(h.mox.state(), MoxState::Rx);
        QVERIFY(h.ctl.isSweeping());
        h.ctl.abortSweep(QStringLiteral("test"));
        QTRY_COMPARE(h.mox.state(), MoxState::Rx);
    }

    // points.size() counts attempts, validPoints() counts measurements.
    // The panel used the first to decide whether to keep a trace, so a
    // sweep that measured nothing still produced a named entry with a
    // tick beside it and an empty chart.
    void a_good_sweep_reports_every_point_as_valid()
    {
        Harness h;
        FakeDipole dipole;
        QSignalSpy fin(&h.ctl, &SwrSweepController::sweepFinished);
        QVERIFY(h.ctl.startSweep(h.tinyPlan()));
        h.pumpUntilFinished(dipole);

        QCOMPARE(fin.count(), 1);
        const auto result = fin.first().first().value<SwrSweepResult>();
        QVERIFY(result.completed);
        QCOMPARE(result.validPoints(), int(result.points.size()));
        QVERIFY2(result.maxFwdW >= SwrSweepController::kMinFwdW,
                 "a sweep that measured cleanly reported no forward "
                 "power");
    }

    // The floor is a fact about the bridge; the tune-power minimum is
    // that fact expressed where it can be checked before keying. If one
    // ever drops below the other the guard stops guarding.
    void the_tune_power_minimum_leaves_room_above_the_bridge_floor()
    {
        QVERIFY2(double(SwrSweepController::kMinUsefulTuneW)
                     > SwrSweepController::kMinFwdW * 2.0,
                 "the minimum tune power must leave headroom over the "
                 "bridge floor, or the pre-flight check passes sweeps "
                 "that cannot measure");
    }
};

QTEST_GUILESS_MAIN(TstSwrSweepController)
#include "tst_swr_sweep_controller.moc"
