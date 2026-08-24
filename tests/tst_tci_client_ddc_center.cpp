// SPDX-License-Identifier: GPL-3.0-or-later
//
// TciClient::ddcCenterHz() -- die Mittenfrequenz aus "dds:", fuer den
// SunSDR-Panadapter (Schritt "Bild", 2026-08-24).
//
// Anlass: gemessen an ExpertSDR2 (tci_probe): "vfo:R,V,hz" sind die
// beiden VFO-Anzeigen INNERHALB der ZF-Durchlassbreite, "if:R,V,off"
// ist ihr Versatz dazu -- "dds:R,hz" allein ist die Mitte, um die der
// I/Q-Strom selbst liegt. Diese Pruefung geht genau auf den Unterschied:
// eine "vfo:"-Zeile allein darf ddcCenterHz() NICHT veraendern, nur
// "dds:".

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocketServer>
#include <QWebSocket>

#include "core/TciClient.h"

using namespace Longpath;

namespace {
constexpr int kWaitMs = 5000;
} // namespace

class TstTciClientDdcCenter : public QObject
{
    Q_OBJECT

private slots:
    void ddsWirdUebernommenVfoAlleinNicht()
    {
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");

        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            QWebSocket* remote = server.nextPendingConnection();
            QVERIFY(remote);
            // Wortgetreu aus der echten Selbstauskunft (tci_probe,
            // 2026-08-24): vfo/if VOR dds, wie am Geraet gemessen --
            // die Reihenfolge ist absichtlich nicht die "logische".
            remote->sendTextMessage(QStringLiteral(
                "device:Testgeraet;iq_samplerate:48000;audio_samplerate:48000;"
                "vfo:0,0,1920500;vfo:0,1,1900000;"
                "if:0,0,9830;if:0,1,-10670;"
                "dds:0,1910670;"
                "ready;"));
        });

        TciClient client;
        QSignalSpy describedSpy(&client, &TciClient::deviceDescribed);
        QSignalSpy ddcSpy(&client, &TciClient::ddcCenterChanged);
        client.connectToEndpoint(QStringLiteral("127.0.0.1"), server.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(describedSpy.count() >= 1, kWaitMs);

        QCOMPARE(ddcSpy.count(), 1);
        QCOMPARE(ddcSpy.constFirst().at(0).toInt(), 0);
        QCOMPARE(ddcSpy.constFirst().at(1).toLongLong(), qint64(1910670));
        QCOMPARE(client.ddcCenterHz(0), qint64(1910670));

        // Empfaenger, von dem nie eine dds-Zeile kam: unbekannt, nicht 0
        // als Zufallstreffer missverstanden -- 0 ist hier explizit "nicht
        // bekannt", siehe ddcCenterHz() in TciClient.h.
        QCOMPARE(client.ddcCenterHz(1), qint64(0));
    }

    void retuningWaehrendDerSitzungAktualisiertDenWert()
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
                "dds:0,1910670;ready;"));
        });

        TciClient client;
        QSignalSpy describedSpy(&client, &TciClient::deviceDescribed);
        client.connectToEndpoint(QStringLiteral("127.0.0.1"), server.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(describedSpy.count() >= 1, kWaitMs);
        QCOMPARE(client.ddcCenterHz(0), qint64(1910670));

        // Der Betreiber retunt in ExpertSDR2 -- eine neue dds-Zeile
        // kommt MITTEN in der Sitzung, nicht nur in der Selbstauskunft.
        QSignalSpy ddcSpy(&client, &TciClient::ddcCenterChanged);
        QVERIFY(remote);
        remote->sendTextMessage(QStringLiteral("dds:0,7100000;"));
        QTRY_COMPARE_WITH_TIMEOUT(client.ddcCenterHz(0), qint64(7100000), kWaitMs);
        QCOMPARE(ddcSpy.count(), 1);
    }

    void kaputteZeileWirdIgnoriertNichtAlsMuellUebernommen()
    {
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");

        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            QWebSocket* remote = server.nextPendingConnection();
            QVERIFY(remote);
            // Fehlender Wert, nicht-numerischer Wert, nur ein Feld statt
            // zwei -- jede Zeile fuer sich falsch geformt.
            remote->sendTextMessage(QStringLiteral(
                "device:Testgeraet;iq_samplerate:48000;audio_samplerate:48000;"
                "dds:0,;dds:abc,123;dds:0;"
                "ready;"));
        });

        TciClient client;
        QSignalSpy describedSpy(&client, &TciClient::deviceDescribed);
        QSignalSpy ddcSpy(&client, &TciClient::ddcCenterChanged);
        client.connectToEndpoint(QStringLiteral("127.0.0.1"), server.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(describedSpy.count() >= 1, kWaitMs);

        QCOMPARE(ddcSpy.count(), 0);
        QCOMPARE(client.ddcCenterHz(0), qint64(0));
    }
};

QTEST_MAIN(TstTciClientDdcCenter)
#include "tst_tci_client_ddc_center.moc"
