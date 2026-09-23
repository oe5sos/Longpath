// no-port-check: Longpath-original unit-test file.
// =================================================================
// tests/tst_radio_model_mic_inputs_wire.cpp  (Longpath)
// =================================================================
//
// Werkbank Runde 4 (2026-09-22): dieselbe Luecke wie Issue #182, nur
// fuenf Schalter weiter. Die Verbindung kann die Bits seit 3M-1b
// (setMicBoost / setLineIn / setMicTipRing / setMicBias / setMicXlr,
// jeweils mit Quelle und Polaritaet dokumentiert), die Setup-Seite
// schreibt sie ins TransmitModel — dazwischen war nichts. Die Signale
// hatten als einzige Empfaenger die Oberflaeche selbst.
//
// Am Geraet hiess das: Setup > Audio > TX Input schaltete ins Leere.
// Der Mikrofonvorverstaerker, die Umschaltung Mic/Line, die Belegung
// Tip/Ring und die Mikrofonspeisung (ohne die ein Elektretmikrofon
// stumm bleibt) standen immer auf der Vorgabe der Verbindung.
// An der HPSDR-Werkbank gegen -hermes nachgewiesen: vor dem Fix kam
// keines der fuenf Bits am Simulator an.
//
// Dieser Prüfstand braucht kein Funkgeraet: er haengt eine
// Attrappen-Verbindung ein, ruft die Verdrahtung ueber ihre Testnaht
// und schaut, was ankommt — beim Umschalten und beim Verbinden.
// =================================================================

#include <QtTest/QtTest>
#include <QObject>
#include <QCoreApplication>
#include <QScopeGuard>

#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace Longpath;

// Attrappe: schreibt jeden Aufruf mit. Muster wie in
// tst_radio_model_mic_ptt_wire.cpp.
class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    QList<bool> micBoostLog;
    QList<bool> lineInLog;
    QList<bool> tipRingLog;
    QList<bool> micBiasLog;
    QList<bool> micXlrLog;

    explicit MockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
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
    void setMicBoost(bool on) override   { micBoostLog.append(on); }
    void setLineIn(bool on) override     { lineInLog.append(on); }
    void setMicTipRing(bool on) override { tipRingLog.append(on); }
    void setMicBias(bool on) override    { micBiasLog.append(on); }
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool on) override     { micXlrLog.append(on); }
};

static void pump()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
}

class TestRadioModelMicInputsWire : public QObject {
    Q_OBJECT
private slots:

    // Jeder der fuenf Schalter erreicht die Verbindung, wenn er umgelegt wird.
    void everySwitchReachesTheConnection()
    {
        RadioModel model;
        auto* conn = new MockConnection();
        model.injectConnectionForTest(conn);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        model.wireMicInputsForTest();
        pump();
        conn->micBoostLog.clear();
        conn->lineInLog.clear();
        conn->tipRingLog.clear();
        conn->micBiasLog.clear();
        conn->micXlrLog.clear();

        TransmitModel& tx = model.transmitModel();
        tx.setMicBoost(!tx.micBoost());
        tx.setLineIn(!tx.lineIn());
        tx.setMicTipRing(!tx.micTipRing());
        tx.setMicBias(!tx.micBias());
        tx.setMicXlr(!tx.micXlr());
        pump();

        QCOMPARE(conn->micBoostLog.size(), 1);
        QCOMPARE(conn->lineInLog.size(),   1);
        QCOMPARE(conn->tipRingLog.size(),  1);
        QCOMPARE(conn->micBiasLog.size(),  1);
        QCOMPARE(conn->micXlrLog.size(),   1);
        QCOMPARE(conn->micBoostLog.first(), tx.micBoost());
        QCOMPARE(conn->lineInLog.first(),   tx.lineIn());
        QCOMPARE(conn->tipRingLog.first(),  tx.micTipRing());
        QCOMPARE(conn->micBiasLog.first(),  tx.micBias());
        QCOMPARE(conn->micXlrLog.first(),   tx.micXlr());
    }

    // Und der gespeicherte Stand wird beim Verbinden einmal vorgeladen —
    // sonst gilt er erst nach dem naechsten Klick.
    void connectPrimesThePersistedState()
    {
        RadioModel model;
        TransmitModel& tx = model.transmitModel();
        // Ein Stand, der sich von den Vorgaben der Verbindung unterscheidet
        // (Boost aus, Line aus, Tip = Mikrofon, Bias aus).
        tx.setMicBoost(true);
        tx.setLineIn(true);
        tx.setMicTipRing(false);
        tx.setMicBias(true);

        auto* conn = new MockConnection();
        model.injectConnectionForTest(conn);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        model.wireMicInputsForTest();
        pump();

        QVERIFY2(!conn->micBoostLog.isEmpty(), "Beim Verbinden kam kein Mic-Boost-Wert an");
        QVERIFY2(!conn->lineInLog.isEmpty(),   "Beim Verbinden kam kein Line-In-Wert an");
        QVERIFY2(!conn->tipRingLog.isEmpty(),  "Beim Verbinden kam kein Tip/Ring-Wert an");
        QVERIFY2(!conn->micBiasLog.isEmpty(),  "Beim Verbinden kam kein Mic-Bias-Wert an");
        QCOMPARE(conn->micBoostLog.last(), true);
        QCOMPARE(conn->lineInLog.last(),   true);
        QCOMPARE(conn->tipRingLog.last(),  false);
        QCOMPARE(conn->micBiasLog.last(),  true);
    }
};

QTEST_MAIN(TestRadioModelMicInputsWire)
#include "tst_radio_model_mic_inputs_wire.moc"
