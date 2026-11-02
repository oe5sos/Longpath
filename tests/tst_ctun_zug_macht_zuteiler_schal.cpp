// no-port-check: Longpath-original test infrastructure. No upstream port.
//
// =================================================================
// Ein CTUN-Zug macht die Mitte des Zuteilers schal.
// =================================================================
//
// MainWindow zieht die Panadapter-Mitte in CTUN so (MainWindow.cpp, im
// centerChanged-Empfaenger):
//
//     receiverManager()->forceHardwareFrequency(stream, centerHz);
//     sw->setDdcCenterFrequency(centerHz);
//     m_radioModel->reshiftSlicesOnStream(stream, centerHz);
//
// Drei Schreibvorgaenge -- und keiner davon ist der Zuteiler.
// reshiftSlicesOnStream setzt die Versaetze und den Kerbenursprung, ruehrt
// m_streamAllocator aber nicht an. Nach einem Zug steht also:
//
//     Gerät   auf der gezogenen Mitte
//     Widget  auf der gezogenen Mitte
//     Zuteiler auf der ALTEN Mitte
//
// Und bindSliceToStream rechnet beim naechsten VFO-Schritt wieder gegen
// den Zuteiler:
//
//     slice->setShiftOffsetHz(placement.shiftOffsetHz);
//     pushNotchOrigin(slice, ch,
//         m_streamAllocator.streamCentreHz(placement.streamIndex));
//
// Der Kommentar dort sagt „activateStream has already run above so the
// allocator is authoritative". Das stimmt auf jedem Weg ausser diesem
// einen: der CTUN-Zug geht am Zuteiler vorbei, mit Absicht und
// dokumentiert.
//
// Fuer den Bediener heisst das: Panadapter in CTUN ziehen, dann den VFO
// bewegen -- und der Ton sitzt um den gezogenen Betrag daneben. Das
// Fahnenschild zeigt die richtige Frequenz.
//
// NereusSDR hat denselben Fall am 2026-10-25 behoben (f183631b9, ihre
// Nummer #648, „move a CTUN DDC the allocator thinks is already there").
// Dort trat er an einer anderen Stelle zutage (requestStreamCentre kehrte
// frueh zurueck); die Ursache ist dieselbe.
//
// Modification history:
//   2026-11-02 — Original von Martin Fischer (OE5SOS), KI-gestützt
//                 über Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/RadioConnection.h"
#include "core/ReceiverManager.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

// Dasselbe Doppel wie in tst_band_change_retunes_ddc_in_ctun.cpp.
// Dateiweite Sichtbarkeit, nicht anonym: moc erzeugt fuer eine
// Q_OBJECT-Klasse mit interner Bindung kein staticMetaObject.
class ZugMockConnection : public RadioConnection {
    Q_OBJECT
public:
    QList<quint64> rxFreqCalls;

    explicit ZugMockConnection(QObject* parent = nullptr)
        : RadioConnection(parent) { setState(ConnectionState::Connected); }

    void init() override {}
    void connectToRadio(const Longpath::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64 hz) override { rxFreqCalls.append(hz); }
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setMox(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setAlexRxBpf(AlexRxBpf) override {}
    void setWatchdogEnabled(bool e) override { m_watchdogEnabled = e; }
    void sendTxIq(const float*, int) override {}
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

namespace {

constexpr double kVfoHz      = 7191000.0;   // 40 m
constexpr double kZugHz      = 7251000.0;   // 60 kHz nach oben gezogen
constexpr double kNachherHz  = 7196000.0;   // VFO danach, 5 kHz ueber dem Start
constexpr int    kRateHz     = 192000;

} // namespace

class TestCtunZugMachtZuteilerSchal : public QObject {
    Q_OBJECT
private slots:
    void init()    { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void derZugWirdInDenZuteilerUebernommen();
    void derVersatzNachDemZugRechnetGegenDieEchteMitte();

private:
    // Baut das Modell samt Empfaenger und CTUN-Riegel auf, genau wie
    // tst_band_change_retunes_ddc_in_ctun es tut.
    static void baueAuf(RadioModel& model, ZugMockConnection* mock, int* sliceIdOut)
    {
        model.configureStreamPool(/*userDdcCount=*/1, /*maxSlices=*/1,
                                  /*defaultRateHz=*/kRateHz);
        model.injectConnectionForTest(mock);

        const int a = model.addSlice();
        SliceModel* const slice = model.slices().at(a);
        slice->setFrequency(kVfoHz);
        model.setActiveSlice(a);
        model.wireSliceSignalsForTest();

        QVERIFY(model.receiverManager() != nullptr);
        model.receiverManager()->createReceiver();
        model.receiverManager()->activateReceiver(0);
        model.receiverManager()->setDdcMapping(0, 0);
        // CTUN pinnt die DDC, wie MainWindow es beim Einschalten tut.
        model.receiverManager()->setDdcFrequencyLocked(true);

        *sliceIdOut = a;
    }

    // Der Zug, Zeile fuer Zeile wie im centerChanged-Empfaenger.
    static void ziehe(RadioModel& model, int stream, double nachHz)
    {
        model.receiverManager()->forceHardwareFrequency(
            stream, static_cast<quint64>(nachHz));
        model.reshiftSlicesOnStream(stream, nachHz);
    }
};

// ── Der Zug muss im Zuteiler ankommen ───────────────────────────────
//
// Die unmittelbare Aussage: nach dem Zug sagt der Zuteiler dieselbe
// Mitte wie das Geraet. Vor der Behebung sagte er die alte.
void TestCtunZugMachtZuteilerSchal::derZugWirdInDenZuteilerUebernommen()
{
    RadioModel model;
    auto* mock = new ZugMockConnection();
    int a = -1;
    baueAuf(model, mock, &a);
    SliceModel* const slice = model.slices().at(a);
    const int stream = slice->streamIndex();
    QVERIFY(stream >= 0);
    QCOMPARE(model.streamCentreHzForTest(stream), kVfoHz);

    ziehe(model, stream, kZugHz);

    QCOMPARE(model.streamCentreHzForTest(stream), kZugHz);

    model.injectConnectionForTest(nullptr);
    delete mock;
}

// ── Und das ist, was der Bediener hoert ─────────────────────────────
//
// Nach dem Zug einen VFO-Schritt. Der Versatz muss gegen die GEZOGENE
// Mitte rechnen, weil dort das Geraet steht:
//
//     7 196 000 - 7 251 000 = -55 000
//
// Gegen die alte Mitte waeren es +5 000 -- 60 kHz daneben, genau der
// Betrag des Zuges. Das Fahnenschild zeigte dabei 7,196 MHz, der Ton kam
// von woanders.
void TestCtunZugMachtZuteilerSchal::derVersatzNachDemZugRechnetGegenDieEchteMitte()
{
    RadioModel model;
    auto* mock = new ZugMockConnection();
    int a = -1;
    baueAuf(model, mock, &a);
    SliceModel* const slice = model.slices().at(a);
    const int stream = slice->streamIndex();
    QVERIFY(stream >= 0);

    ziehe(model, stream, kZugHz);
    // Der Zug selbst setzt den Versatz richtig -- das war nie das Problem.
    QCOMPARE(slice->shiftOffsetHz(), kVfoHz - kZugHz);

    // Jetzt der VFO-Schritt innerhalb des gepinnten Fensters.
    slice->setFrequency(kNachherHz);

    const double erwartet = kNachherHz - kZugHz;
    QVERIFY2(qFuzzyCompare(slice->shiftOffsetHz() + 1.0, erwartet + 1.0),
             qPrintable(QStringLiteral(
                 "Versatz %1 Hz statt %2 Hz -- um %3 Hz daneben, genau der "
                 "Betrag des Zuges. Der Zuteiler stand noch auf der alten "
                 "Mitte.")
                 .arg(slice->shiftOffsetHz())
                 .arg(erwartet)
                 .arg(slice->shiftOffsetHz() - erwartet)));

    // Und das Geraet bleibt, wo der Zug es hingestellt hat: ein Schritt
    // im gepinnten Fenster ist kein Abstimmbefehl.
    QCOMPARE(model.streamCentreHzForTest(stream), kZugHz);

    model.injectConnectionForTest(nullptr);
    delete mock;
}

QTEST_MAIN(TestCtunZugMachtZuteilerSchal)
#include "tst_ctun_zug_macht_zuteiler_schal.moc"
