// no-port-check: Longpath-eigener Prüfstand. Kein Thetis-Vorbild — weder
// Thetis noch deskHPSDR noch AetherSDR prüfen die Herkunft einer
// WebSocket-Verbindung oder decken die Sendezeit; beides ist hier neu.
//
// Zwei Sicherungen, die vor jedem Fernbedienungsbetrieb nötig sind:
//
// 1. HERKUNFTSPRÜFUNG. WebSocket-Verbindungen unterliegen NICHT der
//    Gleiche-Herkunft-Regel. Ohne Prüfung konnte jede Webseite, die der
//    Bediener irgendwo im Browser offen hatte, `new WebSocket(
//    "ws://127.0.0.1:50001")` aufmachen und `trx:0,true;` senden — also die
//    Station tasten. Die Bindung an 127.0.0.1 half dagegen NICHT: sie hält
//    andere Rechner fern, nicht einen Browser auf demselben Rechner.
//    Die Regel muss dabei native Clients (WSJT-X, JTDX, N1MM+, Logger)
//    unangetastet lassen — die senden gar keinen Origin-Kopf.
//
// 2. SENDEZEIT-DECKEL. Der vorhandene Wachhund (2026-09-17) fängt einen
//    Client, dessen Socket stirbt, und einen, der auf Pings nicht mehr
//    antwortet. Er fängt NICHT den Client, der munter weiterantwortet und
//    trotzdem sendet, weil das Telefon in der Tasche liegt. Dagegen hilft
//    nur eine harte Obergrenze je Sendevorgang.

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QSignalSpy>
#include <QNetworkRequest>
#include <QWebSocket>

#include "core/AppSettings.h"
#include "core/TciServer.h"

using namespace Longpath;

class TestTciOriginAndTxTimeCap : public QObject {
    Q_OBJECT

private:
    // Ein Client, der sich wie ein Browser meldet: mit Origin-Kopf.
    static void openWithOrigin(QWebSocket& ws, quint16 port, const QByteArray& origin) {
        QNetworkRequest req{QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port))};
        req.setRawHeader(QByteArrayLiteral("Origin"), origin);
        ws.open(req);
    }

private slots:
    void init() {
        // Jeder Prüfpunkt startet mit leerer Erlaubnisliste. (Die
        // Einstellungen liegen im Testmodus-Sandkasten, siehe
        // TestSandboxInit.cpp — die echte Datei des Bedieners wird nie
        // angefasst.)
        AppSettings::instance().setValue(QStringLiteral("TciAllowedOrigins"),
                                         QString());
    }

    // ── Herkunftsprüfung ────────────────────────────────────────────────────

    void native_client_without_origin_is_accepted() {
        TciServer server(nullptr);
        QVERIFY(server.start(0));

        // QWebSocket sendet von sich aus keinen Origin — genau wie WSJT-X,
        // N1MM+ oder TCI Remote. Diese Clients dürfen sich nichts ändern.
        QWebSocket client;
        QSignalSpy connectedSpy(&client, &QWebSocket::connected);
        client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        QVERIFY2(connectedSpy.wait(2000),
                 "Ein Client ohne Herkunftskopf muss weiterhin angenommen werden");
    }

    void browser_origin_is_rejected_by_default() {
        TciServer server(nullptr);
        QVERIFY(server.start(0));

        QWebSocket client;
        QSignalSpy connectedSpy(&client, &QWebSocket::connected);
        openWithOrigin(client, server.port(), QByteArrayLiteral("http://fremde.example"));

        // Der Handschlag muss scheitern. Wir warten die gleiche Zeit wie oben
        // und verlangen, dass NICHTS verbunden wurde.
        QTest::qWait(1500);
        QCOMPARE(connectedSpy.count(), 0);
        QVERIFY2(server.clientCount() == 0,
                 "Eine abgelehnte Herkunft darf keine Sitzung hinterlassen");
    }

    void listed_origin_is_accepted() {
        AppSettings::instance().setValue(
            QStringLiteral("TciAllowedOrigins"),
            QStringLiteral("http://longpath.local, http://192.168.1.5:8080"));

        TciServer server(nullptr);
        QVERIFY(server.start(0));

        QWebSocket client;
        QSignalSpy connectedSpy(&client, &QWebSocket::connected);
        openWithOrigin(client, server.port(), QByteArrayLiteral("http://192.168.1.5:8080"));
        QVERIFY2(connectedSpy.wait(2000),
                 "Eine eingetragene Herkunft muss durchgelassen werden");
    }

    void origin_match_ignores_case_and_spaces() {
        AppSettings::instance().setValue(QStringLiteral("TciAllowedOrigins"),
                                         QStringLiteral("  HTTP://Longpath.Local  "));

        TciServer server(nullptr);
        QVERIFY(server.start(0));

        QWebSocket client;
        QSignalSpy connectedSpy(&client, &QWebSocket::connected);
        openWithOrigin(client, server.port(), QByteArrayLiteral("http://longpath.local"));
        QVERIFY(connectedSpy.wait(2000));
    }

    // ── Sendezeit-Deckel ────────────────────────────────────────────────────

    void tx_time_cap_releases_a_client_that_keys_too_long() {
        qRegisterMetaType<QWebSocket*>("QWebSocket*");

        TciServer server(nullptr);
        QSignalSpy capSpy(&server, &TciServer::moxReleasedOnTimeCap);
        QVERIFY(server.start(0));
        server.setTxTimeCapSeconds(1);   // statt der 180 s aus den Einstellungen

        QWebSocket client;
        QSignalSpy connectedSpy(&client, &QWebSocket::connected);
        client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        QVERIFY(connectedSpy.wait(2000));

        // Tasten — und dann nichts mehr tun. Der Client bleibt dabei
        // ansprechbar; genau das ist der Fall, den der Ping-Wachhund NICHT
        // erkennt (er bekommt ja brav seine Pongs).
        client.sendTextMessage(QStringLiteral("trx:0,true;"));

        QVERIFY2(capSpy.wait(4000),
                 "Nach Ablauf des Deckels muss der Sender abgeworfen werden");
        QCOMPARE(capSpy.first().at(1).toInt(), 1);
    }

    void tx_time_cap_does_not_fire_after_a_normal_unkey() {
        TciServer server(nullptr);
        QSignalSpy capSpy(&server, &TciServer::moxReleasedOnTimeCap);
        QVERIFY(server.start(0));
        server.setTxTimeCapSeconds(1);

        QWebSocket client;
        QSignalSpy connectedSpy(&client, &QWebSocket::connected);
        client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        QVERIFY(connectedSpy.wait(2000));

        client.sendTextMessage(QStringLiteral("trx:0,true;"));
        QTest::qWait(200);
        client.sendTextMessage(QStringLiteral("trx:0,false;"));

        // Wer ordentlich loslässt, darf hinterher nicht noch abgeworfen
        // werden — sonst schlüge der Deckel mitten in den nächsten
        // Sendevorgang eines anderen Clients.
        QTest::qWait(1500);
        QCOMPARE(capSpy.count(), 0);
    }

    void tx_time_cap_zero_means_off() {
        TciServer server(nullptr);
        QSignalSpy capSpy(&server, &TciServer::moxReleasedOnTimeCap);
        QVERIFY(server.start(0));
        server.setTxTimeCapSeconds(0);
        QCOMPARE(server.txTimeCapSeconds(), 0);

        QWebSocket client;
        QSignalSpy connectedSpy(&client, &QWebSocket::connected);
        client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        QVERIFY(connectedSpy.wait(2000));

        client.sendTextMessage(QStringLiteral("trx:0,true;"));
        QTest::qWait(1500);
        QCOMPARE(capSpy.count(), 0);
    }

    void tx_time_cap_rejects_negative_values() {
        TciServer server(nullptr);
        server.setTxTimeCapSeconds(-5);
        QCOMPARE(server.txTimeCapSeconds(), 0);   // geklemmt, nicht übernommen
    }
};

QTEST_MAIN(TestTciOriginAndTxTimeCap)
#include "tst_tci_origin_and_tx_time_cap.moc"

#else   // !HAVE_WEBSOCKETS
int main() { return 0; }
#endif
