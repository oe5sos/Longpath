// no-port-check: Longpath-original test for the anti-VOX state replay
// (2026-10-12).
//
// MoxController is created once with the RadioModel; TxWorkerThread is
// rebuilt on every connect. The controller's idempotency guards --
// m_antiVoxRunInitialized for the run flag, a NaN sentinel for the
// detector tau -- mean the re-push after a reconnect emits nothing when
// the value has not changed. The REPLACEMENT worker then never learns
// the anti-VOX state, and the cancellation feed stays off until the
// operator happens to toggle the setting.
//
// Upstream NereusSDR fixed the same thing after 2026.10.0 ("Replay
// saved anti-VOX state to replacement transmit workers", #334).
//
// forgetAntiVoxEmissions() is the counterpart to primeWdspState(),
// which deliberately leaves these two alone.

#include <QtTest/QtTest>
#include <QtTest/QSignalSpy>

#include "core/MoxController.h"

using namespace Longpath;

class TestAntiVoxReplay : public QObject {
    Q_OBJECT

private slots:
    void the_run_flag_is_swallowed_on_a_second_push()
    {
        // This is the behaviour the guard is there for, and it is right
        // as long as the receiving worker is the same one.
        MoxController mox;
        QSignalSpy runs(&mox, &MoxController::antiVoxRunRequested);

        mox.setAntiVoxRun(true);
        QCOMPARE(runs.count(), 1);
        mox.setAntiVoxRun(true);
        QCOMPARE(runs.count(), 1);   // swallowed, as intended
    }

    void after_forgetting_the_same_value_is_sent_again()
    {
        // ... and this is what a replacement worker needs.
        MoxController mox;
        QSignalSpy runs(&mox, &MoxController::antiVoxRunRequested);

        mox.setAntiVoxRun(true);
        QCOMPARE(runs.count(), 1);

        mox.forgetAntiVoxEmissions();
        mox.setAntiVoxRun(true);
        QCOMPARE(runs.count(), 2);
        QCOMPARE(runs.last().at(0).toBool(), true);
    }

    void the_detector_tau_replays_too()
    {
        MoxController mox;
        QSignalSpy taus(&mox, &MoxController::antiVoxDetectorTauRequested);

        mox.setAntiVoxTau(20);
        QCOMPARE(taus.count(), 1);
        mox.setAntiVoxTau(20);
        QCOMPARE(taus.count(), 1);   // swallowed

        mox.forgetAntiVoxEmissions();
        mox.setAntiVoxTau(20);
        QCOMPARE(taus.count(), 2);
        QCOMPARE(taus.last().at(0).toDouble(), 0.020);   // ms → s
    }

    void forgetting_does_not_change_the_values_themselves()
    {
        // Only the memory of having sent them is dropped. A worker that
        // asks for the current state must still get the operator's
        // setting, not a default.
        MoxController mox;
        mox.setAntiVoxRun(true);
        mox.setAntiVoxTau(123);

        mox.forgetAntiVoxEmissions();

        QSignalSpy runs(&mox, &MoxController::antiVoxRunRequested);
        QSignalSpy taus(&mox, &MoxController::antiVoxDetectorTauRequested);
        mox.setAntiVoxRun(true);
        mox.setAntiVoxTau(123);
        QCOMPARE(runs.last().at(0).toBool(), true);
        QCOMPARE(taus.last().at(0).toDouble(), 0.123);
    }

    void a_changed_value_still_goes_through_without_forgetting()
    {
        // The counter-check: forgetting must not be the only way to get a
        // value out, or an ordinary operator change would be lost.
        MoxController mox;
        QSignalSpy runs(&mox, &MoxController::antiVoxRunRequested);

        mox.setAntiVoxRun(true);
        mox.setAntiVoxRun(false);
        QCOMPARE(runs.count(), 2);
        QCOMPARE(runs.last().at(0).toBool(), false);
    }
};

QTEST_MAIN(TestAntiVoxReplay)
#include "tst_anti_vox_replay.moc"
