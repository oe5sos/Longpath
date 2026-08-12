// =================================================================
// tests/tst_connected_pushes_ddc_assignment.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Pins the close of the 2026-08-11
// remote-bench MOX-rate investigation.
//
// The restore path moves the stream allocator to the persisted
// per-band rate, but invokeCodecDdcAssignment's wire push is gated on
// isConnected() — every pre-Connected run skipped it and nothing
// re-ran it afterwards, so the radio idled on the connection's
// constructor-default 48 kHz until the first MOX toggle "corrected"
// the rate mid-TX (quadrupling the DDC stream on a marginal remote
// link, measured 3-9% loss both directions).
//
// The fix adds requestDdcAssignment() to the Connected transition,
// beside the Alex-antenna / per-ADC-BPF / TX-LPF pushes that exist
// for exactly the same "a fresh connection must be told" reason.
//
// This test drives the transition WITHOUT a connection object (every
// wire push in the Connected branch is null-guarded); what it pins is
// that the transition re-runs the assignment publish at all,
// observable through the publish's PS-orchestration rate sync: seed a
// stale rate into ReceiverManager, fire Connected, and the next
// MOX-time PsDdcConfig must carry the allocator's live rate again.
// Remove the requestDdcAssignment() call and this fails with 192000.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-11 — Created by Ralph Martin Fischer (OE5SOS),
//                 AI-assisted implementation via Anthropic Claude
//                 (Cowork).
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "core/ReceiverManager.h"
#include "core/codec/CodecContext.h"
#include "core/codec/P2CodecOrionMkII.h"

using namespace NereusSDR;

class TstConnectedPushesDdcAssignment : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<PsDdcConfig>("NereusSDR::PsDdcConfig");
        qRegisterMetaType<PsDdcConfig>("PsDdcConfig");
    }

    void connected_transition_republishes_the_assignment()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        model.configureStreamPool(/*userDdcCount*/ 4, /*maxSlices*/ 4,
                                  /*defaultRateHz*/ 48000);

        P2CodecOrionMkII codec;
        model.receiverManager()->setP2Codec(&codec);
        model.receiverManager()->setHpsdrModel(HPSDRModel::ANAN_G2);

        const int id = model.addSlice();
        QVERIFY(model.sliceById(id) != nullptr);

        // Simulate the drift the bench measured: a stale connect-time
        // seed sitting in the PS-orchestration store while the
        // allocator runs 48 kHz.
        model.receiverManager()->setRx1Rate(192000);

        // The fix under test: the Connected transition must re-run
        // requestDdcAssignment(), whose publish re-syncs the PS store
        // from the allocator.
        model.onConnectionStateChangedForTest(ConnectionState::Connected);

        QSignalSpy spy(model.receiverManager(),
                       &ReceiverManager::ddcConfigChanged);
        model.receiverManager()->setMox(true);

        QVERIFY(spy.count() >= 1);
        const auto config = spy.last().first().value<PsDdcConfig>();
        // G2-class MOX / !PS / !diversity → DDC2 at rx1_rate
        // (console.cs:8246-8255 [v2.10.3.13]). The regression carried
        // the stale 192000 here.
        QCOMPARE(int(config.rate[2]), 48000);
    }
};

QTEST_GUILESS_MAIN(TstConnectedPushesDdcAssignment)
#include "tst_connected_pushes_ddc_assignment.moc"
