// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - RotatorItem smoothing tests
//
// 2026-08-10 (KG4VCF, AI tooling: Anthropic Claude Code).
// Pins the contract for the RotatorItem compass smoothing after the
// three 2026-08-10 fixes:
//   - azimuthConvergesAndSnaps: repeated setValue() ticks converge on
//     the reported heading and land EXACTLY on it (the upstream snap
//     branch was dead code — |d| < 0.2*|d| is never true — so the
//     needle previously only approached the target asymptotically).
//   - azimuthTakesShortestPathAcrossNorth: 350° -> 10° travels 20°
//     through north (wrap handling), not 340° the long way round.
//   - elevationFollowsSetElevation: setElevation() now advances the
//     smoothed elevation (previously nothing wrote m_smoothedEle, so
//     the ELE needle was stuck at 0°) and converges exactly.
//   - elevationZenithIsNotWrappedToZero: a 90° (zenith) reading
//     stays 90° (the paint path previously used fmod(…, 90), which
//     mapped exactly 90° to 0°); values beyond the mechanical range
//     are clamped into [0, 90].

#include <QtTest>

#include "gui/meters/RotatorItem.h"

using namespace NereusSDR;

namespace {
// Drive enough poll ticks for the geometric approach (20% of the
// remaining error per tick) plus the 0.5° snap to settle.
constexpr int kTicks = 64;
}

class TestRotatorItem : public QObject {
    Q_OBJECT
private slots:
    void azimuthConvergesAndSnaps();
    void azimuthTakesShortestPathAcrossNorth();
    void elevationFollowsSetElevation();
    void elevationZenithIsNotWrappedToZero();
};

void TestRotatorItem::azimuthConvergesAndSnaps()
{
    RotatorItem item;
    for (int i = 0; i < kTicks; ++i) {
        item.setValue(123.0);
    }
    // Exact equality is the point: the snap must land the needle on the
    // reported heading, not merely near it.
    QCOMPARE(item.smoothedAzimuth(), 123.0f);
}

void TestRotatorItem::azimuthTakesShortestPathAcrossNorth()
{
    RotatorItem item;
    for (int i = 0; i < kTicks; ++i) {
        item.setValue(350.0);
    }
    QCOMPARE(item.smoothedAzimuth(), 350.0f);

    // One tick towards 10°: shortest path is +20° through north, so the
    // smoothed value must move up past 350°, not down towards 10° the
    // long way (which would pass 180°).
    item.setValue(10.0);
    const float afterOneTick = item.smoothedAzimuth();
    QVERIFY2(afterOneTick > 350.0f || afterOneTick < 10.0f,
             qPrintable(QString("expected wrap through north, got %1")
                            .arg(static_cast<double>(afterOneTick))));

    for (int i = 0; i < kTicks; ++i) {
        item.setValue(10.0);
    }
    QCOMPARE(item.smoothedAzimuth(), 10.0f);
}

void TestRotatorItem::elevationFollowsSetElevation()
{
    RotatorItem item;
    QCOMPARE(item.smoothedElevation(), 0.0f);

    for (int i = 0; i < kTicks; ++i) {
        item.setElevation(45.0f);
    }
    QCOMPARE(item.elevation(), 45.0f);
    QCOMPARE(item.smoothedElevation(), 45.0f);
}

void TestRotatorItem::elevationZenithIsNotWrappedToZero()
{
    RotatorItem item;
    for (int i = 0; i < kTicks; ++i) {
        item.setElevation(90.0f);
    }
    // Regression: fmod(90, 90) == 0 previously zeroed the zenith reading.
    QCOMPARE(item.smoothedElevation(), 90.0f);

    // Out-of-range readings (rotator overshoot / bad telemetry) clamp
    // into the mechanical 0-90° range instead of wrapping.
    for (int i = 0; i < kTicks; ++i) {
        item.setElevation(95.0f);
    }
    QCOMPARE(item.smoothedElevation(), 90.0f);
}

QTEST_GUILESS_MAIN(TestRotatorItem)
#include "tst_rotator_item.moc"
