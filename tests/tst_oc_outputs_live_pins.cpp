// no-port-check: Phase 3P-H Task 5b — OC Outputs live pin-state LED row.
//
// Verifies OcOutputsHfTab::onLiveStateChanged() computes the displayed OC
// byte as OcMatrix::maskFor(currentBand, isTx):
//   - no pins set → byte 0, no LEDs lit.
//   - RX matrix pin 0 set for 20m → byte 0x01, LED 0 lit.
//   - RX matrix pin 3 set for 40m → byte 0x08, LED 3 lit (band change wiring).
//   - TX matrix pin 1 set for 20m → byte 0x02, LED 1 lit only while MOX
//     is on (RX matrix for same band is different).
//
// Bug fix 2026-09-07: this drove the band via addPanadapter() +
// PanadapterModel::setCenterFrequency(), which is exactly the dead path
// the fix removed (RadioModel::addPanadapter() has no caller anywhere in
// the shipped app). Rewired to addSlice() + SliceModel::setFrequency(),
// matching what OcOutputsHfTab actually listens to now.
#include <QtTest/QtTest>
#include <QApplication>

#include "core/OcMatrix.h"
#include "gui/setup/hardware/OcOutputsHfTab.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace Longpath;

class TestOcOutputsLivePins : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int   argc = 0;
            static char* argv = nullptr;
            new QApplication(argc, &argv);
        }
    }

    // Empty matrix + first-slice 20m + MOX=off → byte 0, no LEDs lit.
    void empty_matrix_yields_zero_byte()
    {
        RadioModel model;
        model.sliceById(model.addSlice())->setFrequency(14.200e6);
        OcOutputsHfTab tab(&model, &model.ocMatrixMutable());
        QCOMPARE(int(tab.currentOcByteForTest()), 0);
        for (int pin = 0; pin < 7; ++pin) {
            QVERIFY(!tab.livePinLitForTest(pin));
        }
    }

    // RX pin 0 set for 20m → byte 0x01 while MOX is off.
    void rx_pin_for_current_band_lights_led()
    {
        RadioModel model;
        model.sliceById(model.addSlice())->setFrequency(14.200e6);
        model.ocMatrixMutable().setPin(Band::Band20m, /*pin=*/0,
                                        /*tx=*/false, /*enabled=*/true);
        OcOutputsHfTab tab(&model, &model.ocMatrixMutable());
        QCOMPARE(int(tab.currentOcByteForTest()), 0x01);
        QVERIFY(tab.livePinLitForTest(0));
        QVERIFY(!tab.livePinLitForTest(1));
    }

    // A band change from 20m to 40m shifts the LED to the 40m RX mask.
    void band_change_switches_mask()
    {
        RadioModel model;
        SliceModel* slice = model.sliceById(model.addSlice());
        slice->setFrequency(14.200e6);

        OcMatrix& m = model.ocMatrixMutable();
        m.setPin(Band::Band20m, /*pin=*/0, /*tx=*/false, true);
        m.setPin(Band::Band40m, /*pin=*/3, /*tx=*/false, true);
        OcOutputsHfTab tab(&model, &m);
        QCOMPARE(int(tab.currentOcByteForTest()), 0x01);  // 20m pin 0

        // Setting to 40m RX freq crosses into Band40m.
        slice->setFrequency(7.150e6);
        QCOMPARE(int(tab.currentOcByteForTest()), 0x08);  // 40m pin 3
    }

    // MOX flips from RX matrix to TX matrix for the current band.
    void mox_toggles_rx_tx_matrix()
    {
        RadioModel model;
        model.sliceById(model.addSlice())->setFrequency(14.200e6);

        OcMatrix& m = model.ocMatrixMutable();
        m.setPin(Band::Band20m, /*pin=*/0, /*tx=*/false, true);  // RX
        m.setPin(Band::Band20m, /*pin=*/1, /*tx=*/true,  true);  // TX
        OcOutputsHfTab tab(&model, &m);
        QCOMPARE(int(tab.currentOcByteForTest()), 0x01);  // RX matrix

        model.transmitModel().setMox(true);
        QCOMPARE(int(tab.currentOcByteForTest()), 0x02);  // TX matrix

        model.transmitModel().setMox(false);
        QCOMPARE(int(tab.currentOcByteForTest()), 0x01);  // back to RX
    }

    // setCurrentOcByte direct test — repaints 7 LEDs.
    void set_current_oc_byte_lights_matching_leds()
    {
        RadioModel model;
        OcOutputsHfTab tab(&model, &model.ocMatrixMutable());
        tab.setCurrentOcByte(0x55);  // pins 0, 2, 4, 6
        QVERIFY(tab.livePinLitForTest(0));
        QVERIFY(!tab.livePinLitForTest(1));
        QVERIFY(tab.livePinLitForTest(2));
        QVERIFY(tab.livePinLitForTest(4));
        QVERIFY(tab.livePinLitForTest(6));
    }
};

QTEST_MAIN(TestOcOutputsLivePins)
#include "tst_oc_outputs_live_pins.moc"
