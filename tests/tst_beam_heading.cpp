// =================================================================
// tests/tst_beam_heading.cpp  (NereusSDR)
// =================================================================
//
// The end stop is the part that is easy to get wrong and expensive to
// get wrong. A rotor with a north stop asked to go from 350° to 10° has
// to travel 340° the other way; software that assumes it can wrap sends
// a large beam the long way round on a windy day while the operator
// watches.
//
// So the tests are mostly about the stop, and the numbers in them were
// computed independently before the C++ was written.
//
// no-port-check: NereusSDR-original.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/BeamHeading.h"

#include <QtTest/QtTest>

#include <cmath>

using namespace NereusSDR;
using Stop = BeamHeading::Stop;

class TstBeamHeading : public QObject {
    Q_OBJECT
private slots:

    void wrap_brings_any_angle_into_range()
    {
        QCOMPARE(BeamHeading::wrap360(0.0),    0.0);
        QCOMPARE(BeamHeading::wrap360(360.0),  0.0);
        QCOMPARE(BeamHeading::wrap360(370.0), 10.0);
        QCOMPARE(BeamHeading::wrap360(-10.0), 350.0);
        QCOMPARE(BeamHeading::wrap360(-370.0), 350.0);
    }

    void long_path_is_the_other_side_of_the_world()
    {
        QCOMPARE(BeamHeading::longPath(0.0),   180.0);
        QCOMPARE(BeamHeading::longPath(90.0),  270.0);
        QCOMPARE(BeamHeading::longPath(270.0),  90.0);
        // And it is its own inverse, which is the property that makes it
        // safe to offer as a toggle rather than a one-way conversion.
        for (double d : {0.0, 37.0, 180.0, 359.0}) {
            QCOMPARE(BeamHeading::longPath(BeamHeading::longPath(d)),
                     BeamHeading::wrap360(d));
        }
    }

    // ── Free rotation: always the shorter way ────────────────────────

    void a_continuous_rotator_takes_the_short_way()
    {
        struct C { double from, to, travel; };
        for (const C& c : {C{350, 10,  20}, C{10, 350, -20},
                           C{0, 180, 180},  C{180, 0, -180},
                           C{90, 90, 0},    C{0, 181, -179}}) {
            const auto m = BeamHeading::plan(c.from, c.to, Stop::None);
            QVERIFY(m.reachable);
            QVERIFY2(std::abs(m.travelDeg - c.travel) < 0.001,
                     qPrintable(QStringLiteral("%1 → %2 gave %3, expected %4")
                                    .arg(c.from).arg(c.to)
                                    .arg(m.travelDeg).arg(c.travel)));
            // Never more than half a turn when it is free to choose.
            QVERIFY(std::abs(m.travelDeg) <= 180.001);
        }
    }

    // ── The test this file exists for ────────────────────────────────
    //
    // 350° to 10° is twenty degrees on a continuous rotator and three
    // hundred and forty on one that cannot pass north. Getting this
    // wrong is not a rounding error, it is the antenna going the wrong
    // way for a minute and a half.
    void a_north_stop_cannot_be_crossed()
    {
        const auto free  = BeamHeading::plan(350, 10, Stop::None);
        const auto stopped = BeamHeading::plan(350, 10, Stop::North);
        QVERIFY(std::abs(free.travelDeg - 20.0) < 0.001);
        QVERIFY2(std::abs(stopped.travelDeg + 340.0) < 0.001,
                 qPrintable(QStringLiteral("north-stop 350→10 gave %1, "
                                           "expected -340")
                                .arg(stopped.travelDeg)));
        QVERIFY(!stopped.note.isEmpty());   // and it says why
    }

    void a_south_stop_blocks_the_south_and_not_the_north()
    {
        // Crossing 180 is the long way...
        const auto across = BeamHeading::plan(170, 190, Stop::South);
        QVERIFY(std::abs(across.travelDeg + 340.0) < 0.001);
        // ...while crossing 0 is unaffected, because the stop is
        // elsewhere. A single "has a stop" flag would get this wrong.
        const auto north = BeamHeading::plan(350, 10, Stop::South);
        QVERIFY2(std::abs(north.travelDeg - 20.0) < 0.001,
                 qPrintable(QStringLiteral("south-stop 350→10 gave %1, "
                                           "expected 20")
                                .arg(north.travelDeg)));
    }

    void a_stop_never_makes_a_heading_unreachable()
    {
        // A stop forbids a route, not a bearing. Every heading is still
        // reachable, sometimes the long way — reporting otherwise would
        // stop the operator pointing at a station they can hear.
        for (int from = 0; from < 360; from += 30) {
            for (int to = 0; to < 360; to += 30) {
                for (Stop s : {Stop::None, Stop::North, Stop::South}) {
                    const auto m = BeamHeading::plan(from, to, s);
                    QVERIFY(m.reachable);
                    QVERIFY(std::abs(m.travelDeg) <= 360.001);
                }
            }
        }
    }

    void the_move_always_lands_on_the_heading_asked_for()
    {
        for (int from = 0; from < 360; from += 45) {
            for (int to = 0; to < 360; to += 45) {
                for (Stop s : {Stop::None, Stop::North, Stop::South}) {
                    const auto m = BeamHeading::plan(from, to, s);
                    const double landed =
                        BeamHeading::wrap360(from + m.travelDeg);
                    QVERIFY2(std::abs(landed - m.targetDeg) < 0.001
                                 || std::abs(landed - m.targetDeg) > 359.999,
                             qPrintable(QStringLiteral("%1 + %2 = %3, "
                                                       "wanted %4")
                                            .arg(from).arg(m.travelDeg)
                                            .arg(landed).arg(m.targetDeg)));
                }
            }
        }
    }

    // ── What the operator is told ────────────────────────────────────

    void advice_says_nothing_useless_when_already_pointing_there()
    {
        const auto m = BeamHeading::plan(90, 90, Stop::None);
        QVERIFY(BeamHeading::advice(m).contains(
            QStringLiteral("Already pointing")));
    }

    void advice_warns_about_a_long_move()
    {
        const auto m = BeamHeading::plan(350, 10, Stop::North);
        const QString s = BeamHeading::advice(m);
        QVERIFY(s.contains(QStringLiteral("340")));
        QVERIFY2(s.contains(QStringLiteral("end stop")),
                 qPrintable(s));
    }

    void advice_names_the_direction()
    {
        QVERIFY(BeamHeading::advice(BeamHeading::plan(0, 90, Stop::None))
                    .contains(QStringLiteral("clockwise")));
        QVERIFY(BeamHeading::advice(BeamHeading::plan(90, 0, Stop::None))
                    .contains(QStringLiteral("anticlockwise")));
    }
};

QTEST_APPLESS_MAIN(TstBeamHeading)
#include "tst_beam_heading.moc"
