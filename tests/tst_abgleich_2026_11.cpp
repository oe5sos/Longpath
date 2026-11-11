// =================================================================
// tests/tst_abgleich_2026_11.cpp  (Longpath)
// =================================================================
//
// Prueflaeufe zu den Behebungen aus dem Upstream-Abgleich vom 2026-11-10
// (NereusSDR 2026.11.0 / 2026.11.1, J.J. Boyd KG4VCF). Jeder Fall ist am
// eigenen Baum nachgewiesen und gegen die alte Fassung rot gefahren.
//
//   1. TUNE-Ende stellt PWR nur unter FIXED zurueck (Thetis
//      console.cs:30110-30115) und nie auf einem anderen Band als dem,
//      auf dem TUNE begann -- sonst bekam das neue Band den Antrieb des
//      alten (Upstream 3314e6599).
//   2. Zweiton-Ende: erst „nicht mehr Zweiton", dann PWR zurueck, wie
//      Thetis setup.cs:11194-11201 -- sonst rechnete die Rueckgabe noch im
//      Zweiton und das Band behielt dessen Leistung.
//   3. TCI „CWL/CWU becomes CW" gilt fuer jede Modusmeldung (d29d19248).
//   4. Anvelina: Stroeme nur auf DDCs, die das Geraet meldet (a15d7a80a).
//
// Martin Fischer (OE5SOS), KI-gestuetzt mit Anthropic Claude Code.
//
// no-port-check: Pruefstand; die Thetis-Stellen oben sind das erwartete
// Verhalten, die Behebungen stehen in RadioModel und TwoToneController.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include <array>

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/DdcAssignment.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/TciProtocol.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/codec/CodecContext.h"
#include "core/codec/P2CodecOrionMkII.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace Longpath;

namespace {

class Attrappe : public RadioConnection {
    Q_OBJECT
public:
    explicit Attrappe(QObject* parent = nullptr) : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }
    void init() override {}
    void connectToRadio(const Longpath::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

void pump()
{
    for (int i = 0; i < 6; ++i) {
        QCoreApplication::processEvents();
    }
}

std::array<SliceConfig, 5> lebendigeStroeme(int anzahl)
{
    std::array<SliceConfig, 5> slices{};
    for (int i = 0; i < anzahl && i < 5; ++i) {
        slices[i].live = true;
        slices[i].frequencyHz = 7000000 + i * 3000000;
        slices[i].sampleRateHz = 192000;
        slices[i].antennaIndex = 1;
    }
    slices[0].txBound = true;
    return slices;
}

} // namespace

class TstAbgleich2026_11 : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── 1. TUNE: Bandwechsel behaelt den Wert des neuen Bandes ────────
    void tuneEndeNachBandwechselBehaeltNeuesBand()
    {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/false);
        auto* verbindung = new Attrappe();
        model.injectConnectionForTest(verbindung);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.setTuneOffSettleMsForTest(0);
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setFrequency(3700000.0);
        slice->setDspMode(DSPMode::LSB);

        TransmitModel& tx = model.transmitModel();
        QCOMPARE(tx.tuneDrivePowerSource(), DrivePowerSource::DriveSlider);
        tx.setPower(50);

        model.setTune(true);
        pump();

        // Bandwechsel waehrend TUNE: die Bedienung (TxApplet::setCurrentBand)
        // setzt PWR auf den Wert des neuen Bandes.
        slice->setFrequency(7100000.0);
        tx.setPower(30);

        model.setTune(false);
        pump();
        QTest::qWait(20);
        pump();

        QVERIFY2(tx.power() == 30,
                 qPrintable(QStringLiteral("Nach TUNE-Ende steht PWR auf %1 statt 30 -- "
                                           "der Wert des alten Bandes wurde auf das neue gelegt")
                                .arg(tx.power())));

        model.injectConnectionForTest(nullptr);
        delete verbindung;
    }

    // Unter FIXED bleibt das Thetis-Verhalten: auf demselben Band kommt
    // der gemerkte Wert zurueck.
    void tuneEndeUnterFixedStelltZurueck()
    {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/false);
        auto* verbindung = new Attrappe();
        model.injectConnectionForTest(verbindung);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.setTuneOffSettleMsForTest(0);
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        slice->setFrequency(3700000.0);
        slice->setDspMode(DSPMode::LSB);

        TransmitModel& tx = model.transmitModel();
        tx.setTuneDrivePowerSource(DrivePowerSource::Fixed);
        tx.setPower(50);
        model.setTune(true);
        pump();
        tx.setPower(20);  // waehrend TUNE verstellt
        model.setTune(false);
        pump();
        QTest::qWait(20);
        pump();
        QCOMPARE(tx.power(), 50);

        model.injectConnectionForTest(nullptr);
        delete verbindung;
    }

    // ── 2. Zweiton: erst nicht mehr Zweiton, dann PWR zurueck ─────────
    void zweitonEndeRechnetPwrImNormalbetrieb()
    {
        TransmitModel tx;
        tx.setTwoToneDrivePowerSource(DrivePowerSource::Fixed);
        tx.setTwoTonePower(10);
        tx.setPower(50);

        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        SliceModel slice;

        // Ohne WDSP kehren die Generator-Setter frueh zurueck; der Kanal
        // ist nur da, weil der Regler ohne ihn nicht startet.
        TxChannel kanal(1);

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&kanal);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);

        ctrl.setActive(true);
        pump();
        QCOMPARE(tx.power(), 10);
        // Ohne PaProfileManager setzt der Regler den Spiegel nicht selbst;
        // so steht er im Betrieb (TwoToneController::continueActivation).
        tx.setTwoToneActive(true);

        QList<bool> zweitonBeimZuruecksetzen;
        connect(&tx, &TransmitModel::powerChanged, this, [&](int) {
            zweitonBeimZuruecksetzen.append(tx.isTwoToneActive());
        });

        ctrl.setActive(false);
        pump();
        QTest::qWait(20);
        pump();

        QCOMPARE(tx.power(), 50);
        QVERIFY(!zweitonBeimZuruecksetzen.isEmpty());
        QVERIFY2(!zweitonBeimZuruecksetzen.last(),
                 "PWR kam zurueck, waehrend noch Zweiton galt -- die Rueckgabe "
                 "rechnet dann nicht im Normalbetrieb, und das Band behaelt die "
                 "Zweiton-Leistung (Thetis: erst TwoTone = false, dann PWR)");
        QVERIFY(!tx.isTwoToneActive());
    }

    // ── 3. TCI: CWL/CWU als CW in jeder Modusmeldung ──────────────────
    void tciMeldetCwWennGewuenscht()
    {
        AppSettings::instance().setValue(QStringLiteral("TciCwluBecomesCw"),
                                         QStringLiteral("True"));
        QCOMPARE(TciProtocol::tciModeForWire(QStringLiteral("CWU")), QStringLiteral("CW"));
        QCOMPARE(TciProtocol::tciModeForWire(QStringLiteral("CWL")), QStringLiteral("CW"));
        QCOMPARE(TciProtocol::tciModeForWire(QStringLiteral("USB")), QStringLiteral("USB"));

        AppSettings::instance().setValue(QStringLiteral("TciCwluBecomesCw"),
                                         QStringLiteral("False"));
        QCOMPARE(TciProtocol::tciModeForWire(QStringLiteral("CWU")), QStringLiteral("CWU"));
    }

    // ── 4. Anvelina: nur DDCs, die das Geraet meldet ──────────────────
    void ddcGrenzeNurWoSieGilt()
    {
        const BoardCapabilities& orionMk2 = BoardCapsTable::forBoard(HPSDRHW::OrionMKII);
        QCOMPARE(BoardCapsTable::ddcIndexLimit(orionMk2, true, 5), 5);
        QCOMPARE(BoardCapsTable::ddcIndexLimit(orionMk2, true, 8), 8);
        QCOMPARE(BoardCapsTable::ddcIndexLimit(orionMk2, true, 0), 0);   // keine Meldung
        QCOMPARE(BoardCapsTable::ddcIndexLimit(orionMk2, false, 5), 0);  // Protocol 1
        QCOMPARE(BoardCapsTable::ddcIndexLimit(orionMk2, true, 2), 3);   // DDC2 bleibt
        const BoardCapabilities& saturn = BoardCapsTable::forBoard(HPSDRHW::Saturn);
        QCOMPARE(BoardCapsTable::ddcIndexLimit(saturn, true, 4), 0);
    }

    void anvelinaGemeldetFuenfNimmtDdc2bis4()
    {
        P2CodecOrionMkII codec;
        CodecContext ctx{};
        ctx.model = HPSDRModel::ANVELINAPRO3;
        ctx.ddcIndexLimit = 5;
        const auto a = codec.applyDdcAssignment(ctx, lebendigeStroeme(3));
        QCOMPARE(a.streamDdc[0], 2);
        QCOMPARE(a.streamDdc[1], 3);
        QVERIFY2(a.streamDdc[2] == 4,
                 qPrintable(QStringLiteral("Dritter Strom auf DDC%1 -- das Geraet hat nur DDC0 bis DDC4")
                                .arg(a.streamDdc[2])));
        QCOMPARE(a.ddcEnable & (1 << 5), 0);
        QCOMPARE(a.ddcEnable & (1 << 6), 0);
    }

    void anvelinaOhneMeldungBehaeltDieReihe()
    {
        P2CodecOrionMkII codec;
        CodecContext ctx{};
        ctx.model = HPSDRModel::ANVELINAPRO3;
        const auto a = codec.applyDdcAssignment(ctx, lebendigeStroeme(5));
        QCOMPARE(a.streamDdc[0], 2);
        QCOMPARE(a.streamDdc[1], 3);
        QCOMPARE(a.streamDdc[2], 5);
        QCOMPARE(a.streamDdc[3], 6);
        QCOMPARE(a.streamDdc[4], 4);
    }
};

QTEST_MAIN(TstAbgleich2026_11)
#include "tst_abgleich_2026_11.moc"
