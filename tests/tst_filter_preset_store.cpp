// tst_filter_preset_store.cpp
//
// no-port-check: Longpath-original test file. All Thetis filter-preset
// source cites are in SliceModel.cpp (console.cs:5118-5515 [@852bf0e]).
// =================================================================
// tests/tst_filter_preset_store.cpp  (Longpath)
// =================================================================
//
// Stage C2 (Task TDD) — verifies FilterPresetStore:
//   1. defaultsMatchSliceModel:     defaultPreset(mode,slot) matches
//      SliceModel::presetsForMode(mode)[slot] for representative modes/slots.
//   2. setPresetPersists:           mutate a slot, round-trip through
//      a fresh store, verify the override survived.
//   3. resetPresetRestoresDefault:  mutate then resetPreset, verify default.
//   4. setPresetsForModeReorderPersists: swap two slots, verify order survives.
//   5. presetsChangedSignalFires:   QSignalSpy confirms every mutator emits.
//   6. defaultsAreThetisInitFilterPresets: every F1..F10 of LSB/USB/DIGL/
//      DIGU/CWL/CWU/AM/SAM/DSB, edges and name, against Thetis.
//   7. digitalDefaultIsThetisF5:    defaultFilterForMode(DIGU/DIGL) = F5.
//
// Isolation: uses AppSettings::clear() in initTestCase/init/cleanup so that
// filter overrides written here cannot bleed into other tests.
// =================================================================
// Modification history (Longpath):
//   2026-05-02 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted authoring via Anthropic
//                 Claude Code (Stage C2 filter preset editor).
//   2026-09-28 -- Thetis names instead of "F<n>", full-table and
//                 DIGU/DIGL-default cases (Martin Fischer, OE5SOS,
//                 AI-assisted via Anthropic Claude).
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "models/FilterPresetStore.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TstFilterPresetStore : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void init()         { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // ── 1. Defaults match SliceModel ────────────────────────────────────────
    // Verifies that FilterPresetStore::defaultPreset(mode, slot) returns the
    // same (low, high) values as SliceModel::presetsForMode(mode)[slot].
    // Tests a representative subset: USB/LSB/CWU slots 0,4,9 and AM slot 0.
    void defaultsMatchSliceModel()
    {
        const struct { DSPMode mode; int slot; } cases[] = {
            {DSPMode::USB, 0}, {DSPMode::USB, 4}, {DSPMode::USB, 9},
            {DSPMode::LSB, 0}, {DSPMode::LSB, 4}, {DSPMode::LSB, 9},
            {DSPMode::CWU, 0}, {DSPMode::CWU, 4}, {DSPMode::CWU, 9},
            {DSPMode::AM,  0}, {DSPMode::AM,  4}, {DSPMode::AM,  9},
            {DSPMode::FM,  0}, {DSPMode::FM,  1}, {DSPMode::FM,  2},
        };

        for (const auto& c : cases) {
            const auto pairs = SliceModel::presetsForMode(c.mode);
            if (c.slot >= pairs.size()) { continue; }  // FM only has 3 slots
            const FilterPreset def = FilterPresetStore::defaultPreset(c.mode, c.slot);
            QCOMPARE(def.low,  pairs[c.slot].first);
            QCOMPARE(def.high, pairs[c.slot].second);
            // Name is the Thetis button name of the slot, not "F<slot+1>".
            QCOMPARE(def.name, SliceModel::presetNamesForMode(c.mode).at(c.slot));
        }
    }

    // ── 2. setPreset persists across store re-creation ──────────────────────
    // After writing a preset and clearing the in-memory store (simulated by
    // constructing a fresh FilterPresetStore), the value survives because the
    // underlying AppSettings flush persisted it.
    void setPresetPersists()
    {
        {
            FilterPresetStore store;
            FilterPreset p;
            p.name = QStringLiteral("DX-2.4k");
            p.low  = 100;
            p.high = 2500;
            store.setPreset(DSPMode::USB, 4, p);
        }

        // Construct a fresh store — reads from AppSettings.
        FilterPresetStore store2;
        const auto presets = store2.presetsForMode(DSPMode::USB);
        QVERIFY(presets.size() > 4);
        QCOMPARE(presets[4].name, QStringLiteral("DX-2.4k"));
        QCOMPARE(presets[4].low,  100);
        QCOMPARE(presets[4].high, 2500);
    }

    // ── 3. resetPreset restores default ─────────────────────────────────────
    void resetPresetRestoresDefault()
    {
        FilterPresetStore store;

        // Mutate slot 2 of LSB.
        FilterPreset custom;
        custom.name = QStringLiteral("Narrow");
        custom.low  = -800;
        custom.high = -200;
        store.setPreset(DSPMode::LSB, 2, custom);

        // Verify the override is there.
        auto presets = store.presetsForMode(DSPMode::LSB);
        QCOMPARE(presets[2].name, QStringLiteral("Narrow"));

        // Reset to default.
        store.resetPreset(DSPMode::LSB, 2);

        // Verify default restored.
        presets = store.presetsForMode(DSPMode::LSB);
        const FilterPreset def = FilterPresetStore::defaultPreset(DSPMode::LSB, 2);
        QCOMPARE(presets[2].low,  def.low);
        QCOMPARE(presets[2].high, def.high);
        QCOMPARE(presets[2].name, def.name);
    }

    // ── 4. setPresetsForMode reorder persists ───────────────────────────────
    void setPresetsForModeReorderPersists()
    {
        FilterPresetStore store;

        // Get USB defaults.
        QList<FilterPreset> presets = store.presetsForMode(DSPMode::USB);
        QVERIFY(presets.size() >= 2);

        // Swap slots 0 and 1.
        const FilterPreset slot0 = presets[0];
        const FilterPreset slot1 = presets[1];
        presets.swapItemsAt(0, 1);
        store.setPresetsForMode(DSPMode::USB, presets);

        // Verify in memory.
        const auto after = store.presetsForMode(DSPMode::USB);
        QCOMPARE(after[0].low,  slot1.low);
        QCOMPARE(after[0].high, slot1.high);
        QCOMPARE(after[1].low,  slot0.low);
        QCOMPARE(after[1].high, slot0.high);

        // Verify persisted: construct a fresh store and read back.
        FilterPresetStore store2;
        const auto persisted = store2.presetsForMode(DSPMode::USB);
        QCOMPARE(persisted[0].low,  slot1.low);
        QCOMPARE(persisted[0].high, slot1.high);
        QCOMPARE(persisted[1].low,  slot0.low);
        QCOMPARE(persisted[1].high, slot0.high);
    }

    // ── 5. presetsChanged signal fires on each mutator ──────────────────────
    void presetsChangedSignalFires()
    {
        FilterPresetStore store;
        QSignalSpy spy(&store, &FilterPresetStore::presetsChanged);
        QVERIFY(spy.isValid());

        // setPreset → 1 signal
        FilterPreset p;
        p.name = QStringLiteral("Test");
        p.low  = 100;
        p.high = 2900;
        store.setPreset(DSPMode::USB, 0, p);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last().first().value<DSPMode>(), DSPMode::USB);

        spy.clear();

        // resetPreset → 1 signal
        store.resetPreset(DSPMode::USB, 0);
        QCOMPARE(spy.count(), 1);

        spy.clear();

        // setPresetsForMode → 1 signal
        auto presets = store.presetsForMode(DSPMode::LSB);
        store.setPresetsForMode(DSPMode::LSB, presets);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last().first().value<DSPMode>(), DSPMode::LSB);

        spy.clear();

        // resetMode → 1 signal
        store.resetMode(DSPMode::AM);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last().first().value<DSPMode>(), DSPMode::AM);

        spy.clear();

        // resetAll → 12 signals (one per DSPMode)
        store.resetAll();
        // DSPMode has 12 values (LSB..DRM = 0..11)
        QCOMPARE(spy.count(), 12);
    }

    // ── 6. The whole table is Thetis's ──────────────────────────────────────
    // Thetis console.cs:5118-5515 [@852bf0e], InitFilterPresets — identical
    // at v2.10.3.13 (501e3f5) and in mi0bot (0cef1c9). CW pitch 600 (the
    // default), DIGU offset 1500, DIGL offset 2210. Until 2026-09-28 the
    // table held values no Thetis release ever carried and "F1".."F10" as
    // names; this case pins every slot so that cannot come back quietly.
    void defaultsAreThetisInitFilterPresets()
    {
        struct Slot { int low; int high; const char* name; };
        const int p = 600, du = 1500, dl = 2210;
        const struct { DSPMode mode; QList<Slot> expected; } table[] = {
            {DSPMode::LSB, {{-5100,-100,"5.0k"},{-4500,-100,"4.4k"},{-3900,-100,"3.8k"},
                            {-3400,-100,"3.3k"},{-3000,-100,"2.9k"},{-2800,-100,"2.7k"},
                            {-2500,-100,"2.4k"},{-2200,-100,"2.1k"},{-1900,-100,"1.8k"},
                            {-1100,-100,"1.0k"}}},
            {DSPMode::USB, {{100,5100,"5.0k"},{100,4500,"4.4k"},{100,3900,"3.8k"},
                            {100,3400,"3.3k"},{100,3000,"2.9k"},{100,2800,"2.7k"},
                            {100,2500,"2.4k"},{100,2200,"2.1k"},{100,1900,"1.8k"},
                            {100,1100,"1.0k"}}},
            {DSPMode::DIGL, {{-dl-1500,-dl+1500,"3.0k"},{-dl-1250,-dl+1250,"2.5k"},
                             {-dl-1000,-dl+1000,"2.0k"},{-dl-750,-dl+750,"1.5k"},
                             {-dl-500,-dl+500,"1.0k"},{-dl-400,-dl+400,"800"},
                             {-dl-300,-dl+300,"600"},{-dl-150,-dl+150,"300"},
                             {-dl-75,-dl+75,"150"},{-dl-38,-dl+38,"75"}}},
            {DSPMode::DIGU, {{du-1500,du+1500,"3.0k"},{du-1250,du+1250,"2.5k"},
                             {du-1000,du+1000,"2.0k"},{du-750,du+750,"1.5k"},
                             {du-500,du+500,"1.0k"},{du-400,du+400,"800"},
                             {du-300,du+300,"600"},{du-150,du+150,"300"},
                             {du-75,du+75,"150"},{du-38,du+38,"75"}}},
            {DSPMode::CWL, {{-p-500,-p+500,"1.0k"},{-p-400,-p+400,"800"},{-p-300,-p+300,"600"},
                            {-p-250,-p+250,"500"},{-p-200,-p+200,"400"},{-p-125,-p+125,"250"},
                            {-p-75,-p+75,"150"},{-p-50,-p+50,"100"},{-p-25,-p+25,"50"},
                            {-p-13,-p+13,"25"}}},
            {DSPMode::CWU, {{p-500,p+500,"1.0k"},{p-400,p+400,"800"},{p-300,p+300,"600"},
                            {p-250,p+250,"500"},{p-200,p+200,"400"},{p-125,p+125,"250"},
                            {p-75,p+75,"150"},{p-50,p+50,"100"},{p-25,p+25,"50"},
                            {p-13,p+13,"25"}}},
            {DSPMode::AM,  {{-10000,10000,"20k"},{-9000,9000,"18k"},{-8000,8000,"16k"},
                            {-6000,6000,"12k"},{-5000,5000,"10k"},{-4500,4500,"9.0k"},
                            {-4000,4000,"8.0k"},{-3500,3500,"7.0k"},{-3000,3000,"6.0k"},
                            {-2500,2500,"5.0k"}}},
            {DSPMode::SAM, {{-10000,10000,"20k"},{-9000,9000,"18k"},{-8000,8000,"16k"},
                            {-6000,6000,"12k"},{-5000,5000,"10k"},{-4500,4500,"9.0k"},
                            {-4000,4000,"8.0k"},{-3500,3500,"7.0k"},{-3000,3000,"6.0k"},
                            {-2500,2500,"5.0k"}}},
            {DSPMode::DSB, {{-8000,8000,"16k"},{-6000,6000,"12k"},{-5000,5000,"10k"},
                            {-4000,4000,"8.0k"},{-3300,3300,"6.6k"},{-2600,2600,"5.2k"},
                            {-2000,2000,"4.0k"},{-1550,1550,"3.1k"},{-1450,1450,"2.9k"},
                            {-1200,1200,"2.4k"}}},
        };

        for (const auto& t : table) {
            const auto edges = SliceModel::presetsForMode(t.mode);
            const QStringList names = SliceModel::presetNamesForMode(t.mode);
            QCOMPARE(edges.size(), 10);
            QCOMPARE(names.size(), 10);
            for (int i = 0; i < 10; ++i) {
                const Slot& want = t.expected.at(i);
                const QString where = QStringLiteral("%1 F%2")
                    .arg(SliceModel::modeName(t.mode)).arg(i + 1);
                QVERIFY2(edges.at(i).first  == want.low,  qPrintable(where));
                QVERIFY2(edges.at(i).second == want.high, qPrintable(where));
                QVERIFY2(names.at(i) == QLatin1String(want.name), qPrintable(where));
                const FilterPreset def = FilterPresetStore::defaultPreset(t.mode, i);
                QVERIFY2(def.name == QLatin1String(want.name), qPrintable(where));
            }
        }
    }

    // ── 7. DIGU/DIGL first-touch default is Thetis F5 ───────────────────────
    // InitFilterPresets sets LastFilter = F5 for every mode; F5 of DIGU is
    // digu_click_tune_offset ± 500, "1.0k" (console.cs:5271 [@852bf0e]).
    // Until 2026-09-28 this was ± 600 "1.2k", quoted as Thetis verbatim —
    // no Thetis release ever had it.
    void digitalDefaultIsThetisF5()
    {
        for (DSPMode m : {DSPMode::LSB, DSPMode::USB, DSPMode::DIGL, DSPMode::DIGU,
                          DSPMode::CWL, DSPMode::CWU, DSPMode::AM, DSPMode::SAM,
                          DSPMode::DSB}) {
            const auto f5 = SliceModel::presetsForMode(m).at(4);
            QVERIFY2(SliceModel::defaultFilterForMode(m) == f5,
                     qPrintable(SliceModel::modeName(m)));
        }
        QCOMPARE(SliceModel::defaultFilterForMode(DSPMode::DIGU), std::make_pair(1000, 2000));
        QCOMPARE(SliceModel::defaultFilterForMode(DSPMode::DIGL), std::make_pair(-2710, -1710));
    }
};

QTEST_MAIN(TstFilterPresetStore)
#include "tst_filter_preset_store.moc"
