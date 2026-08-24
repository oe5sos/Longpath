// SPDX-License-Identifier: GPL-3.0-or-later
//
// Schritt "Steuerung": Frequenz UND Betriebsart laufen jetzt in beide
// Richtungen zwischen Longpath und ExpertSDR2 -- nicht nur die
// Panadapter-Mitte (Schritt "Bild", siehe tst_sunsdr_spectrum_wiring.cpp).
//
// Diese Pruefung geht durch den tatsaechlichen Weg (connectSunSdr) und
// deckt vier Stellen ab:
//
//   1. Eingehend: "vfo:0,0,hz" (VFO-Kanal A von Empfaenger 0) uebernimmt
//      die Scheibenfrequenz, "modulation:0,mode" die Betriebsart.
//   2. Derselbe Bench-Fund wie bei "Bild" (siehe MainWindow_SunSdr.cpp,
//      "ExpertSDR2 hat ZWEI Empfaenger-Plaetze"): eine Zeile fuer
//      Empfaenger 1 oder VFO-Kanal B darf die Scheibe NICHT beeinflussen.
//   3. Ausgehend: eine Bedieneraenderung an der Scheibe (setFrequency/
//      setDspMode) sendet "vfo:0,0,hz;"/"modulation:0,mode;" tatsaechlich
//      an ExpertSDR2 -- und eine EINGEHENDE Aenderung darf NICHT
//      postwendend als Ausgangsbefehl zurueckgehen (Echo-Schutz,
//      m_sunSdrApplyingRemoteState).
//   4. Sicherheitsschranke: eine Scheibe mit echter DDC-Bindung
//      (streamIndex() >= 0 -- ein ECHTES Funkgeraet) wird in KEINER
//      Richtung von SunSDR gesteuert, weder eingehend noch ausgehend.
//      Siehe wireSunSdrOutboundControl()/applyRemoteSunSdrFrequency()
//      fuer die Begruendung (CLAUDE.local.md: "Wo Zurueckhaltung und
//      Sicherheit sich widersprechen, gewinnt die Sicherheit").

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocketServer>
#include <QWebSocket>

#include "core/TciClient.h"
#include "core/WdspTypes.h"
#include "gui/MainWindow.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {
constexpr int kWaitMs = 8000;

const QString kReadyLine = QStringLiteral(
    "device:Testgeraet;iq_samplerate:48000;audio_samplerate:48000;"
    "vfo:0,0,14164070;vfo:0,1,14163000;vfo:1,0,1905000;"
    "modulation:0,usb;modulation:1,lsb;"
    "start;ready;");
} // namespace

class TstSunSdrControlWiring : public QObject
{
    Q_OBJECT

private slots:
    void verbindenUebernimmtFrequenzUndModusVonEmpfaenger0()
    {
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            QWebSocket* remote = server.nextPendingConnection();
            QVERIFY(remote);
            remote->sendTextMessage(kReadyLine);
        });

        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);

        mw->connectSunSdrForTest(
            QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));

        const int sliceId = mw->sunSdrTargetSliceForTest();
        SliceModel* slice = model->sliceById(sliceId);
        QVERIFY(slice);

        TciClient* client = mw->sunSdrClientForTest();
        QVERIFY(client);
        QTRY_COMPARE_WITH_TIMEOUT(client->state(), TciClient::State::Connected,
                                  kWaitMs);

        // vfo:0,0 (Kanal A von Empfaenger 0) -- NICHT vfo:1,0 (1905000)
        // und NICHT vfo:0,1 (Kanal B, 14163000).
        QTRY_COMPARE_WITH_TIMEOUT(slice->frequency(), 14164070.0, kWaitMs);
        QCOMPARE(slice->dspMode(), DSPMode::USB);

        mw->close();
    }

    void empfaenger1UndVfoKanalBWerdenIgnoriert()
    {
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");
        QWebSocket* remote = nullptr;
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            remote = server.nextPendingConnection();
            QVERIFY(remote);
            remote->sendTextMessage(kReadyLine);
        });

        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        mw->connectSunSdrForTest(
            QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));

        const int sliceId = mw->sunSdrTargetSliceForTest();
        SliceModel* slice = model->sliceById(sliceId);
        QVERIFY(slice);

        TciClient* client = mw->sunSdrClientForTest();
        QTRY_COMPARE_WITH_TIMEOUT(client->state(), TciClient::State::Connected,
                                  kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(slice->frequency(), 14164070.0, kWaitMs);
        QVERIFY(remote);

        // Empfaenger 1 retunt/schaltet um -- die Scheibe (an Empfaenger 0
        // gebunden) darf sich davon nicht beeindrucken lassen.
        remote->sendTextMessage(QStringLiteral(
            "vfo:1,0,3573000;modulation:1,cwl;"));
        QTest::qWait(300);
        QCOMPARE(slice->frequency(), 14164070.0);
        QCOMPARE(slice->dspMode(), DSPMode::USB);

        // VFO-Kanal B von Empfaenger 0 -- ebenfalls kein Einfluss, nur
        // Kanal A zaehlt.
        remote->sendTextMessage(QStringLiteral("vfo:0,1,7000000;"));
        QTest::qWait(300);
        QCOMPARE(slice->frequency(), 14164070.0);

        // Gegenprobe: Empfaenger 0 / Kanal A bewegt die Scheibe wirklich.
        remote->sendTextMessage(QStringLiteral("vfo:0,0,7100000;modulation:0,lsb;"));
        QTRY_COMPARE_WITH_TIMEOUT(slice->frequency(), 7100000.0, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(slice->dspMode(), DSPMode::LSB, kWaitMs);

        mw->close();
    }

    void bedienerAenderungSendetVfoUndModulationAus()
    {
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");
        QStringList received;
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            QWebSocket* remote = server.nextPendingConnection();
            QVERIFY(remote);
            connect(remote, &QWebSocket::textMessageReceived, this,
                    [&received](const QString& msg) { received << msg; });
            remote->sendTextMessage(kReadyLine);
        });

        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        mw->connectSunSdrForTest(
            QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));

        const int sliceId = mw->sunSdrTargetSliceForTest();
        SliceModel* slice = model->sliceById(sliceId);
        QVERIFY(slice);

        TciClient* client = mw->sunSdrClientForTest();
        QTRY_COMPARE_WITH_TIMEOUT(client->state(), TciClient::State::Connected,
                                  kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(slice->frequency(), 14164070.0, kWaitMs);
        received.clear();

        // Der Bediener dreht am VFO und schaltet den Modus um. CWU statt
        // CWL absichtlich: Bench-Fund 2026-08-24, ExpertSDR2 kennt am
        // Draht nur das allgemeine "cw", nicht "cwl"/"cwu" -- beide
        // muessen darum auf "cw" abbilden (siehe
        // tciModeStringForDspMode), nicht nur CWL zufaellig richtig
        // aussehen, weil es der einzige hier geprobte Wert war.
        slice->setFrequency(7100000.0);
        slice->setDspMode(DSPMode::CWU);

        QTRY_VERIFY_WITH_TIMEOUT(
            received.contains(QStringLiteral("vfo:0,0,7100000;")), kWaitMs);
        QVERIFY2(received.contains(QStringLiteral("modulation:0,cw;")),
                 "modulation-Ausgangsbefehl fehlt oder noch \"cwu\" statt "
                 "des von ExpertSDR2 verstandenen \"cw\"");

        mw->close();
    }

    void cwZurueckmeldungDrehtEineStehendeCwuWahlNichtAufCwlZurueck()
    {
        // Bench-Fund 2026-08-24: ExpertSDR2 versteht "cwl"/"cwu" nicht,
        // Longpath sendet darum fuer beide das allgemeine "cw" (siehe
        // tciModeStringForDspMode). Meldet ExpertSDR2 daraufhin -- wie
        // jede TCI-Aenderung -- eine frische "modulation:0,cw;" zurueck,
        // darf applyRemoteSunSdrModulation() das NICHT naiv auf CWL
        // abbilden: die eigene CWU-Wahl wuerde sonst bei der naechsten
        // Rueckmeldung still verschwinden, obwohl der Bediener sie nie
        // geaendert hat. Nur ein "cw" auf eine Scheibe, die noch NICHT
        // in CW ist, darf tatsaechlich (auf CWL) umschalten.
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");
        QWebSocket* remote = nullptr;
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            remote = server.nextPendingConnection();
            QVERIFY(remote);
            remote->sendTextMessage(kReadyLine);
        });

        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        mw->connectSunSdrForTest(
            QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));

        const int sliceId = mw->sunSdrTargetSliceForTest();
        SliceModel* slice = model->sliceById(sliceId);
        QVERIFY(slice);

        TciClient* client = mw->sunSdrClientForTest();
        QTRY_COMPARE_WITH_TIMEOUT(client->state(), TciClient::State::Connected,
                                  kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(slice->frequency(), 14164070.0, kWaitMs);

        // Bediener waehlt CWU an der Scheibe (nicht ueber TCI -- das
        // Ausgangsecho selbst ist hier nicht der Pruefgegenstand).
        slice->setDspMode(DSPMode::CWU);
        QCOMPARE(slice->dspMode(), DSPMode::CWU);

        // ExpertSDR2 meldet "cw" zurueck -- genau das erwartete Echo
        // eines Geraets, das die Seitenbandwahl gar nicht kennt.
        QVERIFY(remote);
        remote->sendTextMessage(QStringLiteral("modulation:0,cw;"));
        QTest::qWait(300);
        QCOMPARE(slice->dspMode(), DSPMode::CWU);

        // Gegenprobe: eine Scheibe, die NICHT schon in CW ist, wechselt
        // bei "cw" tatsaechlich (auf CWL, der Thetis-faehige Normalfall).
        remote->sendTextMessage(QStringLiteral("modulation:0,usb;"));
        QTRY_COMPARE_WITH_TIMEOUT(slice->dspMode(), DSPMode::USB, kWaitMs);
        remote->sendTextMessage(QStringLiteral("modulation:0,cw;"));
        QTRY_COMPARE_WITH_TIMEOUT(slice->dspMode(), DSPMode::CWL, kWaitMs);

        mw->close();
    }

    void eingehendeAenderungWirdNichtAlsBefehlZurueckgesendet()
    {
        // Echo-Schutz: die eingehende Zeile darf nicht postwendend als
        // Ausgangsbefehl an dasselbe Geraet zurueckgehen (siehe
        // m_sunSdrApplyingRemoteState in MainWindow_SunSdr.cpp).
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");
        QWebSocket* remote = nullptr;
        QStringList received;
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            remote = server.nextPendingConnection();
            QVERIFY(remote);
            connect(remote, &QWebSocket::textMessageReceived, this,
                    [&received](const QString& msg) { received << msg; });
            remote->sendTextMessage(kReadyLine);
        });

        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        mw->connectSunSdrForTest(
            QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));

        const int sliceId = mw->sunSdrTargetSliceForTest();
        SliceModel* slice = model->sliceById(sliceId);
        QVERIFY(slice);

        TciClient* client = mw->sunSdrClientForTest();
        QTRY_COMPARE_WITH_TIMEOUT(client->state(), TciClient::State::Connected,
                                  kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(slice->frequency(), 14164070.0, kWaitMs);
        QVERIFY(remote);
        received.clear();

        remote->sendTextMessage(QStringLiteral("vfo:0,0,3573000;modulation:0,am;"));
        QTRY_COMPARE_WITH_TIMEOUT(slice->frequency(), 3573000.0, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(slice->dspMode(), DSPMode::AM, kWaitMs);

        // Kurze Gnadenfrist fuer einen etwaigen (falschen) Echo-Befehl,
        // dann pruefen: nichts davon darf beim Testgeraet angekommen sein.
        QTest::qWait(300);
        QVERIFY2(!received.contains(QStringLiteral("vfo:0,0,3573000;")),
                 "eingehende Frequenz wurde als Ausgangsbefehl zurueckgesendet");
        QVERIFY2(!received.contains(QStringLiteral("modulation:0,am;")),
                 "eingehender Modus wurde als Ausgangsbefehl zurueckgesendet");

        mw->close();
    }

    void echteScheibeMitDdcBindungWirdInKeinerRichtungGesteuert()
    {
        // Sicherheitsschranke: eine Scheibe mit streamIndex() >= 0 gehoert
        // einem ECHTEN Funkgeraet. connectSunSdr()'s Rueckfall (aktive
        // Scheibe, sonst die erste) koennte theoretisch genau so eine
        // Scheibe treffen -- weder eingehend noch ausgehend darf sie
        // dann von SunSDR/ExpertSDR2 mitgesteuert werden.
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");
        QWebSocket* remote = nullptr;
        QStringList received;
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            remote = server.nextPendingConnection();
            QVERIFY(remote);
            connect(remote, &QWebSocket::textMessageReceived, this,
                    [&received](const QString& msg) { received << msg; });
            remote->sendTextMessage(kReadyLine);
        });

        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);
        const int existingId = model->addSlice();
        SliceModel* slice = model->sliceById(existingId);
        QVERIFY(slice);
        // Nachgestellte echte DDC-Bindung -- ohne echtes Funkgeraet ist
        // das der einzige Weg, den Zustand herzustellen, den
        // wireSunSdrOutboundControl()/applyRemoteSunSdrFrequency() prueft.
        slice->setStreamIndex(0);
        const double originalFreq = slice->frequency();

        mw->connectSunSdrForTest(
            QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));
        QCOMPARE(mw->sunSdrTargetSliceForTest(), existingId);

        TciClient* client = mw->sunSdrClientForTest();
        QTRY_COMPARE_WITH_TIMEOUT(client->state(), TciClient::State::Connected,
                                  kWaitMs);
        QTest::qWait(300);

        // Eingehend: die Selbstauskunft (vfo:0,0,14164070) darf die
        // echte Scheibe NICHT umgestimmt haben.
        QCOMPARE(slice->frequency(), originalFreq);

        // Ausgehend: eine Bedieneraenderung an der echten Scheibe darf
        // NICHT an ExpertSDR2 gesendet werden.
        QVERIFY(remote);
        received.clear();
        slice->setFrequency(7100000.0);
        QTest::qWait(300);
        QVERIFY2(received.isEmpty(),
                 "echte Scheibe hat trotz DDC-Bindung einen TCI-Befehl ausgeloest");

        mw->close();
    }
};

QTEST_MAIN(TstSunSdrControlWiring)
#include "tst_sunsdr_control_wiring.moc"
