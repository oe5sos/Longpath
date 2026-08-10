// Verify RotorDialWidget travel maths: shortest arc, the wrap across
// north, and the end-stop rule that makes the long way the only legal
// one. Plus state transitions and click-to-aim.
// no-port-check: NereusSDR-original — Thetis has no rotator control.

#include <QApplication>
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "gui/widgets/RotorDialWidget.h"

using namespace NereusSDR;
using State = RotorDialWidget::State;

class TstRotorDial : public QObject {
    Q_OBJECT
private slots:
    void no_target_means_no_travel();
    void takes_the_shorter_arc();
    void wraps_across_north_instead_of_going_the_long_way();
    void end_stop_forces_the_long_way_round();
    void free_rotator_ignores_the_stop();
    void state_follows_target_and_tolerance();
    void turning_state_is_not_overwritten_by_movement();
    void bearings_are_normalised();
    void a_simulated_needle_says_so_and_changes_nothing_else();
};

void TstRotorDial::no_target_means_no_travel()
{
    RotorDialWidget d;
    d.setActualBearing(45);
    QCOMPARE(d.travelDegrees(), 0.0);
    QCOMPARE(d.state(), State::Idle);
    QVERIFY(!d.hasTarget());
}

void TstRotorDial::takes_the_shorter_arc()
{
    RotorDialWidget d;
    d.setActualBearing(90);
    d.setTargetBearing(140);
    QVERIFY(qAbs(d.travelDegrees() - 50.0) < 0.01);    // clockwise

    d.setTargetBearing(40);
    QVERIFY(qAbs(d.travelDegrees() + 50.0) < 0.01);    // counter-clockwise
}

void TstRotorDial::wraps_across_north_instead_of_going_the_long_way()
{
    // 350° → 010° is 20° over north, not 340° back. Getting this wrong
    // sends the antenna almost all the way round.
    RotorDialWidget d;
    d.setActualBearing(350);
    d.setTargetBearing(10);
    QVERIFY2(qAbs(d.travelDegrees() - 20.0) < 0.01,
             qPrintable(QStringLiteral("travel was %1").arg(d.travelDegrees())));

    d.setActualBearing(10);
    d.setTargetBearing(350);
    QVERIFY(qAbs(d.travelDegrees() + 20.0) < 0.01);
}

void TstRotorDial::end_stop_forces_the_long_way_round()
{
    // A rotator with a north stop cannot travel through 0°. The short
    // 20° hop from 350 to 010 crosses it, so the legal path is the
    // 340° way round — this is the difference between aiming the
    // antenna and winding it into the stop.
    RotorDialWidget d;
    d.setEndStop(0);
    d.setActualBearing(350);
    d.setTargetBearing(10);

    const double t = d.travelDegrees();
    QVERIFY2(t < -300.0,
             qPrintable(QStringLiteral("expected the long way, got %1").arg(t)));
    QVERIFY(qAbs(t + 340.0) < 1.5);
}

void TstRotorDial::free_rotator_ignores_the_stop()
{
    RotorDialWidget d;
    d.setEndStop(-1);          // free rotation
    d.setActualBearing(350);
    d.setTargetBearing(10);
    QVERIFY(qAbs(d.travelDegrees() - 20.0) < 0.01);
}

void TstRotorDial::state_follows_target_and_tolerance()
{
    RotorDialWidget d;
    d.setArrivalTolerance(3);
    d.setActualBearing(100);

    d.setTargetBearing(200);
    QCOMPARE(d.state(), State::Targeted);

    // Inside the tolerance counts as arrived.
    d.setTargetBearing(102);
    QCOMPARE(d.state(), State::OnTarget);

    d.clearTarget();
    QCOMPARE(d.state(), State::Idle);
}

void TstRotorDial::turning_state_is_not_overwritten_by_movement()
{
    // While the rotator is running, incoming heading updates must not
    // knock the dial out of Turning — the mover owns that state until
    // it reports a stop.
    RotorDialWidget d;
    d.setActualBearing(0);
    d.setTargetBearing(90);
    d.setState(State::Turning);

    d.setActualBearing(45);
    QCOMPARE(d.state(), State::Turning);
    d.setActualBearing(89);
    QCOMPARE(d.state(), State::Turning);
}

void TstRotorDial::bearings_are_normalised()
{
    RotorDialWidget d;
    d.setActualBearing(-90);
    QVERIFY(qAbs(d.actualBearing() - 270.0) < 0.01);

    d.setTargetBearing(450);
    QVERIFY(qAbs(d.targetBearing() - 90.0) < 0.01);
}

// The flag exists so a needle driven by a timer cannot be mistaken for
// a needle driven by a mast. What it must NOT do is change any of the
// arithmetic — a simulated dial that computed travel differently would
// be a second rotator model, and the one thing worse than a needle that
// lies is two of them disagreeing.
void TstRotorDial::a_simulated_needle_says_so_and_changes_nothing_else()
{
    RotorDialWidget d;
    d.setActualBearing(350);
    d.setTargetBearing(10);
    d.setEndStop(0.0);                       // north stop: the long way
    const double travelBefore = d.travelDegrees();
    const auto   stateBefore  = d.state();

    QVERIFY(!d.isSimulated());               // a real reading by default
    d.setSimulated(true);
    QVERIFY(d.isSimulated());

    QCOMPARE(d.travelDegrees(), travelBefore);
    QCOMPARE(d.state(), stateBefore);
    QCOMPARE(d.actualBearing(), 350.0);
    QCOMPARE(d.targetBearing(), 10.0);

    d.setSimulated(false);
    QVERIFY(!d.isSimulated());
    QCOMPARE(d.travelDegrees(), travelBefore);
}

QTEST_MAIN(TstRotorDial)
#include "tst_rotor_dial.moc"
