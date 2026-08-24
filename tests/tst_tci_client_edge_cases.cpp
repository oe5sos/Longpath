// SPDX-License-Identifier: GPL-3.0-or-later
//
// TciClient an den Raendern: eine misslungene Selbstauskunft, ein
// Strombefehl vor "ready", und ob eine getrennte Sitzung wirklich
// abgestanden ist statt nur so auszusehen.
//
// Anlass, 2026-08-24: unabhaengige Durchsicht des neuen TciClient
// ("das Grundgerüst muss perfekt sein!"). Vier Pruefer haben den Code
// aus vier Blickwinkeln gelesen; ein Skeptiker hat jeden Befund vorm
// Glauben widerlegen wollen. Drei Luecken haben beide Runden
// ueberstanden -- alle drei existierten schon als Verhalten im Code
// (die Wachen standen), nur ohne Pruefung, die es beim naechsten Mal
// auffangen wuerde:
//
//   1. Eine kaputte Zeile im Textkanal (nicht-numerisch, leer, negativ)
//      darf die Rate nicht auf irgendeine Fantasiezahl setzen.
//   2. Ein Strombefehl vor "ready" darf NICHTS auf die Leitung
//      schicken -- die Sonde tools/tci_probe.cpp haette bei einem
//      solchen verfruehten Befehl vermutlich stillschweigend Muell
//      bekommen, wenn ExpertSDR2 ihn falsch verstanden haette.
//   3. Nach disconnectFromEndpoint() muss die Sitzung wirklich leer
//      sein -- nicht nur state() korrekt, sondern auch deviceName(),
//      iqSampleRate(), isReceiverRunning() usw. Sonst ist eine tote
//      Sitzung nicht von einer lebenden zu unterscheiden.
//
// Alle drei mit einem selbstgebauten TCI-Sender nachgestellt, wie in
// tst_tci_client_ignores_lying_header.cpp -- unabhaengig davon, ob
// ExpertSDR2 gerade laeuft.

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocketServer>
#include <QWebSocket>

#include "core/TciClient.h"

using namespace Longpath;

namespace {
constexpr int kWaitMs = 5000;
} // namespace

class TstTciClientEdgeCases : public QObject
{
    Q_OBJECT

private slots:
    void kaputteRatenzeileWirdIgnoriertNichtFalschUebernommen()
    {
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");

        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            QWebSocket* remote = server.nextPendingConnection();
            QVERIFY(remote);
            // iq_samplerate viermal falsch (nicht-numerisch, leer,
            // negativ, nur ein Doppelpunkt ohne Wert); audio_samplerate
            // EINMAL richtig -- das muss trotzdem ankommen, die kaputten
            // Nachbarzeilen duerfen es nicht mit anstecken.
            remote->sendTextMessage(QStringLiteral(
                "device:Testgeraet;"
                "iq_samplerate:abc;iq_samplerate:;iq_samplerate:-5;"
                "audio_samplerate:48000;ready;"));
        });

        TciClient client;
        QSignalSpy describedSpy(&client, &TciClient::deviceDescribed);
        client.connectToEndpoint(QStringLiteral("127.0.0.1"), server.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(describedSpy.count() >= 1, kWaitMs);

        QCOMPARE(client.state(), TciClient::State::Connected);
        QVERIFY2(client.iqSampleRate() == 0,
                 "eine kaputte iq_samplerate-Zeile hat trotzdem einen Wert gesetzt");
        QCOMPARE(client.audioSampleRate(), 48000);
    }

    void strombefehlVorReadySchicktNichtsAufDieLeitung()
    {
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");

        QWebSocket* remote = nullptr;
        QStringList receivedByServer;
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            remote = server.nextPendingConnection();
            QVERIFY(remote);
            connect(remote, &QWebSocket::textMessageReceived, this,
                    [&](const QString& msg) { receivedByServer << msg; });
            // "ready;" wird absichtlich ZURUECKGEHALTEN -- der Client
            // bleibt in Connecting.
            remote->sendTextMessage(QStringLiteral(
                "device:Testgeraet;iq_samplerate:48000;audio_samplerate:48000;"));
        });

        TciClient client;
        client.connectToEndpoint(QStringLiteral("127.0.0.1"), server.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(remote != nullptr, kWaitMs);
        // Kurz warten, bis die obigen Zeilen sicher angekommen sind, ohne
        // auf "ready" zu warten -- das kommt ja gerade nicht.
        QTest::qWait(300);
        QCOMPARE(client.state(), TciClient::State::Connecting);

        client.startIqStream(0);
        client.startAudioStream(0);
        QTest::qWait(300);

        QVERIFY2(!receivedByServer.join(QLatin1Char(' ')).contains(QStringLiteral("iq_start")),
                 "iq_start wurde vor 'ready' verschickt");
        QVERIFY2(!receivedByServer.join(QLatin1Char(' ')).contains(QStringLiteral("audio_start")),
                 "audio_start wurde vor 'ready' verschickt");

        // Gegenprobe im selben Testlauf: NACH ready darf derselbe Aufruf
        // sehr wohl etwas verschicken -- das beweist, dass die Sperre an
        // den Zustand gebunden ist, nicht dauerhaft blockiert. Hier wird
        // ein tatsaechliches Eintreten erwartet, kein Ausbleiben -- dafuer
        // QTRY_* statt eines festen qWait (in diesem Projekt schon einmal
        // Ursache von Flackern bei Netz-Umlaeufen, siehe
        // tst_rf2ks_connection_control).
        receivedByServer.clear();
        remote->sendTextMessage(QStringLiteral("ready;"));
        QTRY_COMPARE_WITH_TIMEOUT(client.state(), TciClient::State::Connected, kWaitMs);

        client.startIqStream(0);
        QTRY_VERIFY_WITH_TIMEOUT(
            receivedByServer.join(QLatin1Char(' ')).contains(QStringLiteral("iq_start")),
            kWaitMs);
    }

    void getrennteSitzungIstWirklichLeerNichtNurImZustand()
    {
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");

        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            QWebSocket* remote = server.nextPendingConnection();
            QVERIFY(remote);
            remote->sendTextMessage(QStringLiteral(
                "device:Testgeraet;iq_samplerate:192000;audio_samplerate:48000;"
                "start;ready;"));
        });

        TciClient client;
        QSignalSpy describedSpy(&client, &TciClient::deviceDescribed);
        client.connectToEndpoint(QStringLiteral("127.0.0.1"), server.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(describedSpy.count() >= 1, kWaitMs);

        // Vor dem Abbau: die Sitzung ist wirklich befuellt -- sonst
        // beweist der Abbau gleich nichts.
        QCOMPARE(client.deviceName(), QStringLiteral("Testgeraet"));
        QCOMPARE(client.iqSampleRate(), 192000);
        QCOMPARE(client.audioSampleRate(), 48000);
        QVERIFY(client.isReceiverStateKnown());
        QVERIFY(client.isReceiverRunning());

        client.disconnectFromEndpoint();

        QCOMPARE(client.state(), TciClient::State::Disconnected);
        QVERIFY2(client.deviceName().isEmpty(),
                 "deviceName() traegt nach dem Abbau noch die alte Sitzung");
        QVERIFY2(client.iqSampleRate() == 0,
                 "iqSampleRate() traegt nach dem Abbau noch die alte Sitzung");
        QVERIFY2(client.audioSampleRate() == 0,
                 "audioSampleRate() traegt nach dem Abbau noch die alte Sitzung");
        QVERIFY2(!client.isReceiverStateKnown(),
                 "isReceiverStateKnown() bleibt nach dem Abbau faelschlich wahr");
        QVERIFY2(!client.isReceiverRunning(),
                 "isReceiverRunning() bleibt nach dem Abbau faelschlich wahr");
    }

    void abbauOhneJemalsVerbundenGewesenZuSeinIstUngefaehrlich()
    {
        // Der triviale Rand: disconnectFromEndpoint() auf einem frischen
        // Objekt darf nicht abstuerzen und darf keinen Zustandswechsel
        // ausloesen (es gibt ja nichts zu beenden).
        TciClient client;
        QSignalSpy stateSpy(&client, &TciClient::stateChanged);
        client.disconnectFromEndpoint();
        QCOMPARE(client.state(), TciClient::State::Disconnected);
        QCOMPARE(stateSpy.count(), 0);
    }
};

QTEST_MAIN(TstTciClientEdgeCases)
#include "tst_tci_client_edge_cases.moc"
