// SPDX-License-Identifier: GPL-3.0-or-later
//
// TciClient::vfoHz()/modulation() -- die tatsaechliche VFO-Anzeige und
// Betriebsart, fuer den SunSDR-Steuerungsschritt (2026-08-24).
//
// Anlass: "vfo:R,V,hz" ist ANDERS als "dds:R,hz" (siehe
// tst_tci_client_ddc_center.cpp) -- dds ist die Mitte, um die der
// I/Q-Strom liegt, vfo ist wo der Bediener INNERHALB der Durchlassbreite
// tatsaechlich steht. Fuer die Scheibenfrequenz zaehlt vfo, nicht dds.
// "modulation:R,mode" ist klein geschrieben am Draht (gemessen gegen
// ExpertSDR2, tci_probe, 2026-08-24: "modulation:0,lsb").
//
// setVfoFrequency()/setModulation() sind das Gegenstueck: Longpath SENDET
// diese Zeilen selbst, wie startIqStream()/startAudioStream() es fuer die
// Stroeme schon tun -- gleiche Zustandsregel (nur in State::Connected).

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocketServer>
#include <QWebSocket>

#include "core/TciClient.h"

using namespace Longpath;

namespace {
constexpr int kWaitMs = 5000;
} // namespace

class TstTciClientVfoAndModulation : public QObject
{
    Q_OBJECT

private slots:
    void vfoWirdMitEmpfaengerUndKanalUebernommen()
    {
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");

        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            QWebSocket* remote = server.nextPendingConnection();
            QVERIFY(remote);
            // Wortgetreu wie am echten Geraet gemessen (tci_probe,
            // 2026-08-24): zwei Empfaenger, je zwei VFO-Kanaele.
            remote->sendTextMessage(QStringLiteral(
                "device:Testgeraet;iq_samplerate:48000;audio_samplerate:48000;"
                "vfo:0,0,14164070;vfo:0,1,14163000;"
                "vfo:1,0,1905000;"
                "ready;"));
        });

        TciClient client;
        QSignalSpy describedSpy(&client, &TciClient::deviceDescribed);
        QSignalSpy vfoSpy(&client, &TciClient::vfoChanged);
        client.connectToEndpoint(QStringLiteral("127.0.0.1"), server.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(describedSpy.count() >= 1, kWaitMs);

        QCOMPARE(vfoSpy.count(), 3);
        QCOMPARE(client.vfoHz(0, 0), qint64(14164070));
        QCOMPARE(client.vfoHz(0, 1), qint64(14163000));
        QCOMPARE(client.vfoHz(1, 0), qint64(1905000));
        // Nie gemeldeter Kanal: unbekannt, nicht 0 als Zufallstreffer.
        QCOMPARE(client.vfoHz(1, 1), qint64(0));
    }

    void modulationWirdKleinGeschriebenUebernommen()
    {
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");

        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            QWebSocket* remote = server.nextPendingConnection();
            QVERIFY(remote);
            // Grossschreibung am Draht ist nicht garantiert -- die Sonde
            // hat "modulation:0,lsb" klein gemessen, hier zusaetzlich
            // gemischt geschrieben gegengeprueft.
            remote->sendTextMessage(QStringLiteral(
                "device:Testgeraet;iq_samplerate:48000;audio_samplerate:48000;"
                "modulation:0,LSB;modulation:1,Usb;"
                "ready;"));
        });

        TciClient client;
        QSignalSpy describedSpy(&client, &TciClient::deviceDescribed);
        QSignalSpy modSpy(&client, &TciClient::modulationChanged);
        client.connectToEndpoint(QStringLiteral("127.0.0.1"), server.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(describedSpy.count() >= 1, kWaitMs);

        QCOMPARE(modSpy.count(), 2);
        QCOMPARE(client.modulation(0), QStringLiteral("lsb"));
        QCOMPARE(client.modulation(1), QStringLiteral("usb"));
        QVERIFY(client.modulation(2).isEmpty());
    }

    void retuningUndUmschaltenWaehrendDerSitzungAktualisieren()
    {
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");

        QWebSocket* remote = nullptr;
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            remote = server.nextPendingConnection();
            QVERIFY(remote);
            remote->sendTextMessage(QStringLiteral(
                "device:Testgeraet;iq_samplerate:48000;audio_samplerate:48000;"
                "vfo:0,0,14164070;modulation:0,usb;ready;"));
        });

        TciClient client;
        QSignalSpy describedSpy(&client, &TciClient::deviceDescribed);
        client.connectToEndpoint(QStringLiteral("127.0.0.1"), server.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(describedSpy.count() >= 1, kWaitMs);
        QCOMPARE(client.vfoHz(0, 0), qint64(14164070));
        QCOMPARE(client.modulation(0), QStringLiteral("usb"));

        QVERIFY(remote);
        remote->sendTextMessage(QStringLiteral("vfo:0,0,7100000;modulation:0,lsb;"));
        QTRY_COMPARE_WITH_TIMEOUT(client.vfoHz(0, 0), qint64(7100000), kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(client.modulation(0), QStringLiteral("lsb"), kWaitMs);
    }

    void kaputteZeilenWerdenIgnoriert()
    {
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");

        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            QWebSocket* remote = server.nextPendingConnection();
            QVERIFY(remote);
            remote->sendTextMessage(QStringLiteral(
                "device:Testgeraet;iq_samplerate:48000;audio_samplerate:48000;"
                "vfo:0,0;vfo:abc,0,123;vfo:0,abc,123;"
                "modulation:;modulation:abc,usb;"
                "ready;"));
        });

        TciClient client;
        QSignalSpy describedSpy(&client, &TciClient::deviceDescribed);
        QSignalSpy vfoSpy(&client, &TciClient::vfoChanged);
        QSignalSpy modSpy(&client, &TciClient::modulationChanged);
        client.connectToEndpoint(QStringLiteral("127.0.0.1"), server.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(describedSpy.count() >= 1, kWaitMs);

        QCOMPARE(vfoSpy.count(), 0);
        QCOMPARE(modSpy.count(), 0);
        QCOMPARE(client.vfoHz(0, 0), qint64(0));
        QVERIFY(client.modulation(0).isEmpty());
    }

    void setVfoFrequencyUndSetModulationSendenNurWennVerbunden()
    {
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
            remote->sendTextMessage(QStringLiteral(
                "device:Testgeraet;iq_samplerate:48000;audio_samplerate:48000;"
                "ready;"));
        });

        TciClient client;

        // Vor der Verbindung: verworfen, kein Absturz, nichts am Draht.
        client.setVfoFrequency(0, 0, 14164070);
        client.setModulation(0, QStringLiteral("usb"));

        QSignalSpy describedSpy(&client, &TciClient::deviceDescribed);
        client.connectToEndpoint(QStringLiteral("127.0.0.1"), server.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(describedSpy.count() >= 1, kWaitMs);
        QVERIFY(!received.contains(QStringLiteral("vfo:0,0,14164070;")));

        client.setVfoFrequency(0, 0, 14164070);
        client.setModulation(0, QStringLiteral("usb"));
        QTRY_VERIFY_WITH_TIMEOUT(
            received.contains(QStringLiteral("vfo:0,0,14164070;")), kWaitMs);
        QVERIFY2(received.contains(QStringLiteral("modulation:0,usb;")),
                 "modulation-Befehl fehlt");
    }
};

QTEST_MAIN(TstTciClientVfoAndModulation)
#include "tst_tci_client_vfo_and_modulation.moc"
