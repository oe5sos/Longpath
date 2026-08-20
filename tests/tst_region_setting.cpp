// tests/tst_region_setting.cpp  (NereusSDR)
//
// NereusSDR-original. No Thetis port — this covers a wiring defect that
// is ours.
//
// ── What went wrong ──────────────────────────────────────────────────
//
// A sweep on 80 m transmitted from 3.500 to 4.000 MHz for a station in
// Austria, where the band ends at 3.800.
//
// Setup → General → Region wrote the display string ("Europe") to the
// settings key "Region". The two things that decide whether the
// transmitter may key — the sweep planner and the MOX band-plan check —
// read an integer from the key "BandPlanRegion". Nothing ever wrote
// that key. Two readers, no writer, so both always took the default,
// and the default was UnitedStates.
//
// The combo box saved and reloaded perfectly. It just had nothing on
// the other end of it, and the failure was invisible because the
// antenna chart drew its band edges from a third setting that DID work.

#include <QtTest>

#include "core/safety/RegionSetting.h"
#include "core/safety/BandPlanGuard.h"
#include "core/AppSettings.h"
#include "core/SwrSweepController.h"

using namespace Longpath;
using namespace Longpath::safety;

class TestRegionSetting : public QObject
{
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── The mapping ──────────────────────────────────────────────────

    void everyRegionHasADisplayNameAndSurvivesTheRoundTrip()
    {
        // The table is indexed by enum value. If those ever drift the
        // parse silently returns a DIFFERENT country, which is the
        // worst possible failure for this particular setting.
        for (int i = 0; i < kRegionCount; ++i) {
            const auto r = static_cast<Region>(i);
            const QString name = regionDisplayName(r);
            QVERIFY2(!name.isEmpty(),
                     qPrintable(QStringLiteral("region %1 has no name")
                                    .arg(i)));
            bool ok = false;
            QCOMPARE(regionFromDisplayName(name, &ok), r);
            QVERIFY(ok);
        }
    }

    void theStringsAreTheOnesTheSetupPageWrites()
    {
        // Spot-checks against GeneralOptionsPage's combo items. If
        // someone tidies one list, this fails rather than the band plan
        // quietly reverting to the default.
        bool ok = false;
        QCOMPARE(regionFromDisplayName(QStringLiteral("Europe"), &ok),
                 Region::Europe);
        QVERIFY(ok);
        QCOMPARE(regionFromDisplayName(QStringLiteral("United States"), &ok),
                 Region::UnitedStates);
        QVERIFY(ok);
        QCOMPARE(regionFromDisplayName(QStringLiteral("Region1"), &ok),
                 Region::Region1);
        QVERIFY(ok);
    }

    void anUnknownStringIsReportedAsUnknownNotGuessed()
    {
        bool ok = true;
        regionFromDisplayName(QStringLiteral("Atlantis"), &ok);
        QVERIFY2(!ok, "an unrecognised region was accepted");
    }

    // ── Reading the setting ──────────────────────────────────────────

    void noSettingMeansUnconfigured_notUnitedStates()
    {
        const RegionChoice c = configuredRegion();
        QVERIFY2(!c.configured,
                 "an unset region was reported as a choice — this is the "
                 "defect: the old code turned 'nobody said' into "
                 "'United States'");
    }

    void theKeyReadIsTheKeyTheInterfaceWrites()
    {
        // Exactly what GeneralOptionsPage does on currentTextChanged.
        AppSettings::instance().setValue(QStringLiteral("Region"),
                                         QStringLiteral("Europe"));
        const RegionChoice c = configuredRegion();
        QVERIFY2(c.configured, "the region the operator chose did not "
                               "reach the band-plan guard");
        QCOMPARE(c.region, Region::Europe);
    }

    void theOldKeyIsNoLongerConsulted()
    {
        // Writing only the dead key must NOT configure anything, or the
        // two-keys-one-meaning confusion is still alive.
        AppSettings::instance().setValue(
            QStringLiteral("BandPlanRegion"),
            QString::number(static_cast<int>(Region::UnitedStates)));
        QVERIFY(!configuredRegion().configured);
    }

    // ── The narrowest fallback ───────────────────────────────────────

    void unconfiguredAllowsOnlyWhatEveryRegionAllows()
    {
        BandPlanGuard guard;

        // 3.650 MHz is inside 80 m under every plan.
        QVERIFY(isValidTxFreqEverywhere(guard, 3650000, DSPMode::LSB, false));

        // 3.900 MHz is US 80 m and outside Region 1. With no region
        // set it must be refused — that is the whole point.
        QVERIFY2(!isValidTxFreqEverywhere(guard, 3900000, DSPMode::LSB,
                                          false),
                 "3.900 MHz was allowed with no region configured");

        // 7.250 MHz: US 40 m, outside Region 1's 7.200 edge.
        QVERIFY(!isValidTxFreqEverywhere(guard, 7250000, DSPMode::LSB, false));
    }

    // ── The sweep, which is the thing that actually transmitted ──────

    void an80mSweepInEuropeStopsAt3_800()
    {
        BandPlanGuard guard;
        SwrSweepPlan plan = SwrSweepPlan::forBand(Band::Band80m);
        plan.points = 51;
        // The seed is the Region-2 table — this is what made the bug
        // possible, so pin it.
        QCOMPARE(plan.startHz, quint64(3500000));
        QCOMPARE(plan.stopHz,  quint64(4000000));

        QVERIFY(plan.clipToGuard(guard, Region::Europe, DSPMode::LSB));
        QCOMPARE(plan.startHz, quint64(3500000));
        QVERIFY2(plan.stopHz <= quint64(3800000),
                 qPrintable(QStringLiteral("sweep would have transmitted "
                                           "up to %1 Hz").arg(plan.stopHz)));
    }

    void an80mSweepWithNoRegionStopsAt3_800Too()
    {
        // std::nullopt — nobody said where we are. It must not fall
        // back to the widest plan.
        BandPlanGuard guard;
        SwrSweepPlan plan = SwrSweepPlan::forBand(Band::Band80m);
        plan.points = 51;
        QVERIFY(plan.clipToGuard(guard, std::nullopt, DSPMode::LSB));
        QVERIFY2(plan.stopHz <= quint64(3800000),
                 qPrintable(QStringLiteral("with no region set the sweep "
                                           "would have reached %1 Hz")
                                .arg(plan.stopHz)));
    }

    void the40mSweepIsClippedTheSameWay()
    {
        BandPlanGuard guard;
        SwrSweepPlan plan = SwrSweepPlan::forBand(Band::Band40m);
        plan.points = 31;
        QVERIFY(plan.clipToGuard(guard, std::nullopt, DSPMode::LSB));
        QVERIFY2(plan.stopHz <= quint64(7200000),
                 qPrintable(QStringLiteral("40 m reached %1 Hz")
                                .arg(plan.stopHz)));
    }
};

QTEST_GUILESS_MAIN(TestRegionSetting)
#include "tst_region_setting.moc"
