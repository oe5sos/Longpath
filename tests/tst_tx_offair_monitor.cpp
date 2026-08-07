// The one rule the off-air monitor must never break: it may feed the
// monitor, and it may not put anything on the air.
//
// This is the transmit path. A mistake here does not produce a wrong
// pixel or a lost log entry — it radiates, on somebody else's
// frequency, without the operator knowing. So the two gates are pure
// functions in the header and this file checks the truth table rather
// than the wiring: a test that needed a radio, WDSP and an audio device
// would not run, and a guarantee nobody checks is not a guarantee.
// no-port-check: NereusSDR-original.

#include <QtTest/QtTest>

#include "core/TxChannel.h"

using namespace NereusSDR;

class TstTxOffAirMonitor : public QObject {
    Q_OBJECT
private slots:
    void nothing_on_the_air_unless_transmitting();
    void the_monitor_runs_off_air_only_when_asked();
    void transmitting_still_does_both();
    void vox_listening_stays_silent_and_off_air();
    void the_gates_are_compile_time_constants();
};

void TstTxOffAirMonitor::nothing_on_the_air_unless_transmitting()
{
    // Every combination of the two listening modes, with MOX off. Not
    // one of them may write to the radio. If a future gate is added and
    // this file is not updated, the new combination is absent from the
    // loop and the omission is visible in the diff.
    for (bool vox : {false, true}) {
        for (bool mon : {false, true}) {
            QVERIFY2(!TxChannel::writesToRadio(false, vox, mon),
                     qPrintable(QStringLiteral("vox=%1 monitor=%2 went on air")
                                    .arg(vox).arg(mon)));
        }
    }
}

void TstTxOffAirMonitor::the_monitor_runs_off_air_only_when_asked()
{
    // Off air, the monitor follows its own flag and nothing else.
    QVERIFY(!TxChannel::feedsMonitor(false, false, false));
    QVERIFY(TxChannel::feedsMonitor(false, false, true));

    // And enabling it does not enable the radio write as a side effect —
    // the failure this whole design exists to prevent.
    QVERIFY(!TxChannel::writesToRadio(false, false, true));
}

void TstTxOffAirMonitor::transmitting_still_does_both()
{
    // The off-air flag must not take anything away. An operator who
    // leaves the monitor on and then keys must still be heard on the
    // band and still hear themselves.
    for (bool vox : {false, true}) {
        for (bool mon : {false, true}) {
            QVERIFY(TxChannel::writesToRadio(true, vox, mon));
            QVERIFY(TxChannel::feedsMonitor(true, vox, mon));
        }
    }
}

void TstTxOffAirMonitor::vox_listening_stays_silent_and_off_air()
{
    // VOX listening pumps the chain so the detector can see the mic. It
    // was never meant to be audible and must not become audible by
    // accident: someone with VOX armed would otherwise suddenly hear
    // themselves, conclude they were transmitting, and stop trusting
    // the monitor.
    QVERIFY(!TxChannel::feedsMonitor(false, true, false));
    QVERIFY(!TxChannel::writesToRadio(false, true, false));
}

void TstTxOffAirMonitor::the_gates_are_compile_time_constants()
{
    // constexpr, so the compiler can check the important case before
    // the program ever runs. If someone makes writesToRadio depend on
    // the monitor flag, this stops being compilable.
    static_assert(!TxChannel::writesToRadio(false, false, true),
                  "the off-air monitor must never write to the radio");
    static_assert(!TxChannel::writesToRadio(false, true, true),
                  "no listening mode may write to the radio");
    static_assert(TxChannel::writesToRadio(true, false, false),
                  "transmitting must still transmit");
    static_assert(TxChannel::feedsMonitor(false, false, true),
                  "the off-air monitor must feed the monitor");
    QVERIFY(true);
}

QTEST_APPLESS_MAIN(TstTxOffAirMonitor)
#include "tst_tx_offair_monitor.moc"
