// no-port-check: NereusSDR/Longpath-original regression test.

// =================================================================
// tests/tst_radio_model_band_save_uses_slice_frequency.cpp  (NereusSDR)
// =================================================================
//
// Regression test for a real bug found live, 2026-08-26 (OE5SOS): after
// switching to 40m the mode selector showed FM. Root cause, confirmed
// against the operator's own settings file (a leftover Band40m/ModeFM/
// Filter* residue): RadioModel::saveSliceState() keyed the per-band save
// on the class-wide RadioModel::m_lastBand instead of computing the band
// from the slice actually being saved.
//
// m_lastBand is shared across every slice's frequencyChanged handler
// (wireSliceSignals connects it "for every slice", RadioModel.cpp), and
// scheduleSettingsSave()'s 500ms coalesce always targets m_activeSlice
// when it finally fires. So: slice A crosses a band boundary (updating
// the SHARED m_lastBand), and slice B -- the active slice, on a
// completely different band, unrelated to A's crossing -- gets its
// current mode saved under A's new band instead of its own. This test
// reproduces that exact race with two slices and asserts the fixed
// behaviour: the save always uses bandFromFrequency(slice->frequency())
// for whichever slice is actually being saved, never the shared field.
//
// Fix: RadioModel.cpp, RadioModel::saveSliceState().
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-26 — Original for NereusSDR/Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QtTest/QtTest>

#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/Band.h"
#include "core/AppSettings.h"
#include "core/WdspTypes.h"
#include "core/P1RadioConnection.h"
#include "core/HpsdrModel.h"
#include "core/ConnectionState.h"

using namespace Longpath;

namespace {

// setState is protected on RadioConnection -- same helper subclass
// tst_p1_alex_lpf_word_source.cpp uses to bring a test connection up
// Connected the way a real session would.
class ConnectedP1RadioConnection final : public P1RadioConnection {
public:
    ConnectedP1RadioConnection() { setState(ConnectionState::Connected); }
};

} // namespace

class TestRadioModelBandSaveUsesSliceFrequency : public QObject
{
    Q_OBJECT

private slots:

    void staleSharedLastBandDoesNotCorruptTheWrongSlicesModeSlot()
    {
        AppSettings::instance().clear();

        RadioModel model;
        model.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);

        auto* conn = new ConnectedP1RadioConnection();
        conn->setBoardForTest(HPSDRHW::HermesII);
        model.injectConnectionForTest(conn);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);  // 20m
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);   // 40m

        // No manual wireSliceSignalsForTest() needed: addSlice() already
        // calls wireSliceSignals(slice) unconditionally for every slice it
        // creates (RadioModel.cpp, addSlice(), "Wire this slice's DSP
        // controls to its OWN WDSP channel" -- fixed 2026-07-26, bench-
        // caught as "AGC does not seem to be wired up" on B/C/D). Calling
        // it again here would just double-connect both slices' signals.
        model.setActiveSlice(b);

        // Slice B (active, 40m) gets a distinctive mode that nothing
        // else in this scenario would produce, so seeing it land in the
        // wrong band's slot is unambiguous.
        model.slices().at(b)->setDspMode(DSPMode::CWL);

        // The race: slice A -- NOT active, NOT the one about to be
        // saved -- crosses a band boundary AFTER slice B's mode was
        // set. This updates the SHARED m_lastBand to A's new band,
        // while slice B (still sitting on 40m) is the one whose state
        // is actually coalesced for save.
        model.slices().at(a)->setFrequency(3700000.0);  // 20m -> 80m

        // Force the 500ms coalesce to run now instead of waiting.
        model.flushPendingSettingsSave();

        // Slice B's mode must be recoverable from 40m's own slot...
        // SliceModel::saveToSettings/restoreFromSettings scope every key by
        // sliceIndex() as well as band (bandPrefix(m_sliceIndex, band)) --
        // a plain default-constructed SliceModel reads slice index 0 (slice
        // A's own slot), not slice B's. Must match slice B's real index or
        // this reads/writes the wrong slice entirely, independent of the
        // band bug under test.
        SliceModel restored40m;
        restored40m.setSliceIndex(b);
        restored40m.restoreFromSettings(Band::Band40m);
        QCOMPARE(restored40m.dspMode(), DSPMode::CWL);

        // ...and must NOT have leaked into 80m's slot -- the band the
        // stale shared m_lastBand pointed at when the save fired. Before
        // the fix, this is exactly where it landed instead.
        SliceModel restored80m;
        restored80m.setSliceIndex(b);
        restored80m.restoreFromSettings(Band::Band80m);
        QVERIFY2(restored80m.dspMode() != DSPMode::CWL,
                 "slice B's mode leaked into slice A's band via the "
                 "shared m_lastBand field -- the exact 2026-08-26 bug");

        model.injectConnectionForTest(nullptr);
        delete conn;
    }

    // Simpler companion case: even with only ONE slice in play, the
    // saved band must track that slice's actual frequency, not
    // whatever m_lastBand happens to hold -- pins the fix's basic
    // correctness independent of the multi-slice race above.
    void singleSliceSaveUsesItsOwnFrequencyDerivedBand()
    {
        AppSettings::instance().clear();

        RadioModel model;
        model.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);

        auto* conn = new ConnectedP1RadioConnection();
        conn->setBoardForTest(HPSDRHW::HermesII);
        model.injectConnectionForTest(conn);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);  // 20m
        model.setActiveSlice(a);  // addSlice() already wired this slice.

        model.slices().at(a)->setFrequency(7150000.0);   // 20m -> 40m
        model.slices().at(a)->setDspMode(DSPMode::CWU);

        model.flushPendingSettingsSave();

        // Slice A keeps its default index 0, so a default-constructed
        // SliceModel already reads the right slot here.
        SliceModel restored;
        restored.restoreFromSettings(Band::Band40m);
        QCOMPARE(restored.dspMode(), DSPMode::CWU);

        model.injectConnectionForTest(nullptr);
        delete conn;
    }
};

QTEST_MAIN(TestRadioModelBandSaveUsesSliceFrequency)
#include "tst_radio_model_band_save_uses_slice_frequency.moc"
