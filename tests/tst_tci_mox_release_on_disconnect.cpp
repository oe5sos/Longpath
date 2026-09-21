// SPDX-License-Identifier: GPL-2.0-or-later
// tests/tst_tci_mox_release_on_disconnect.cpp (NereusSDR)
//
// A TCI client that keys the radio (trx:N,true) and then goes away — WSJT-X
// crashing mid-over, a remote client losing its link — must not leave the
// transmitter keyed. Thetis has exactly that gap; other station servers
// close it with a transmit lease and a heartbeat. TciServer's version
// (2026-09-17): remember who keyed, and unkey when that socket vanishes.
//
// The first three tests run without a RadioModel (server(nullptr), the same
// injection path tst_tci_tx_mutex uses) and check the bookkeeping through
// the moxReleasedOnClientLoss signal. The last two put a real RadioModel
// behind the server and check MOX itself — including the case that must
// NOT unkey: the operator keyed after the client let go.
// no-port-check: NereusSDR-original integration test.

#include <QtTest>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QWebSocket>
#include <QUrl>

#include "core/TciServer.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

QUrl serverUrl(const TciServer& server)
{
    return QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port()));
}

bool connectClient(QWebSocket& ws, const TciServer& server)
{
    QSignalSpy connected(&ws, &QWebSocket::connected);
    ws.open(serverUrl(server));
    return connected.wait(2000);
}

// A hung client. QWebSocket answers pings by itself, deep in the
// protocol layer, so it cannot play a frozen program. This speaks just
// enough WebSocket over a raw TCP socket to shake hands and key the radio
// (one masked text frame, RFC 6455 §5.3), and then answers nothing — the
// socket stays open, the pings pile up unread.
class HungClient {
public:
    // Server and client share this thread, so nothing here may block:
    // the server needs the event loop to accept the socket and answer the
    // handshake. Every wait is a qWait loop.
    bool connectAndKey(quint16 port)
    {
        sock.connectToHost(QHostAddress::LocalHost, port);
        if (!waitUntil([&] { return sock.state() == QAbstractSocket::ConnectedState; })) {
            return false;
        }
        sock.write(QByteArrayLiteral(
            "GET / HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n"));
        sock.flush();
        QByteArray reply;
        if (!waitUntil([&] {
                reply += sock.readAll();
                return reply.contains("\r\n\r\n");
            })) {
            return false;
        }
        if (!reply.startsWith("HTTP/1.1 101")) { return false; }
        sock.write(maskedTextFrame(QByteArrayLiteral("trx:0,true;")));
        sock.flush();
        return true;
    }

    template <typename Pred>
    static bool waitUntil(Pred pred, int timeoutMs = 2000)
    {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < timeoutMs) {
            if (pred()) { return true; }
            QTest::qWait(10);
        }
        return pred();
    }

    static QByteArray maskedTextFrame(const QByteArray& payload)
    {
        Q_ASSERT(payload.size() < 126);
        QByteArray f;
        f.append(char(0x81));                          // FIN + text
        f.append(char(0x80 | payload.size()));         // masked, short length
        const char mask[4] = {0x12, 0x34, 0x56, 0x78};
        f.append(mask, 4);
        for (int i = 0; i < payload.size(); ++i) {
            f.append(char(payload.at(i) ^ mask[i % 4]));
        }
        return f;
    }

    QTcpSocket sock;
};

} // namespace

class TestTciMoxReleaseOnDisconnect : public QObject {
    Q_OBJECT
private slots:
    void a_client_that_keyed_and_vanished_triggers_the_release();
    void a_client_that_unkeyed_before_vanishing_does_not();
    void the_release_follows_the_last_client_that_keyed();
    void with_a_radio_model_the_radio_is_unkeyed();
    void an_operator_who_keyed_after_the_client_let_go_stays_keyed();
    void a_hung_client_that_stops_answering_pings_is_released();
    void a_live_client_that_answers_pings_keeps_the_key();
};

void TestTciMoxReleaseOnDisconnect::a_client_that_keyed_and_vanished_triggers_the_release()
{
    TciServer server(nullptr);
    QVERIFY(server.start(0));
    QSignalSpy released(&server, &TciServer::moxReleasedOnClientLoss);

    QWebSocket client;
    QVERIFY(connectClient(client, server));
    QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 1, 2000);

    // WSJT-X keys without the ",tci" suffix; that is the common case.
    client.sendTextMessage(QStringLiteral("trx:0,true;"));
    // Let the server see the frame before the socket goes.
    QTRY_VERIFY_WITH_TIMEOUT(server.clientCount() == 1 && released.count() == 0, 500);
    QTest::qWait(50);

    client.abort();
    QTRY_COMPARE_WITH_TIMEOUT(released.count(), 1, 2000);
    QVERIFY2(!released.at(0).at(0).toString().isEmpty(), "the peer string is empty");
    QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 0, 2000);
    server.stop();
}

void TestTciMoxReleaseOnDisconnect::a_client_that_unkeyed_before_vanishing_does_not()
{
    TciServer server(nullptr);
    QVERIFY(server.start(0));
    QSignalSpy released(&server, &TciServer::moxReleasedOnClientLoss);

    QWebSocket client;
    QVERIFY(connectClient(client, server));
    QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 1, 2000);

    client.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    client.sendTextMessage(QStringLiteral("trx:0,false;"));
    // The mutex path proves both lines were processed before we pull the plug.
    QTRY_COMPARE_WITH_TIMEOUT(server.activeTxClientCount(), 0, 2000);
    QTest::qWait(50);

    client.abort();
    QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 0, 2000);
    QTest::qWait(100);
    QCOMPARE(released.count(), 0);
    server.stop();
}

void TestTciMoxReleaseOnDisconnect::the_release_follows_the_last_client_that_keyed()
{
    // Two clients key in turn. The one that keyed last owns the key-up;
    // the earlier one vanishing must not release anything.
    TciServer server(nullptr);
    QVERIFY(server.start(0));
    QSignalSpy released(&server, &TciServer::moxReleasedOnClientLoss);

    QWebSocket first, second;
    QVERIFY(connectClient(first, server));
    QVERIFY(connectClient(second, server));
    QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 2, 2000);

    first.sendTextMessage(QStringLiteral("trx:0,true;"));
    QTest::qWait(50);
    second.sendTextMessage(QStringLiteral("trx:0,true;"));
    QTest::qWait(50);

    first.abort();
    QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 1, 2000);
    QTest::qWait(100);
    QCOMPARE(released.count(), 0);

    second.abort();
    QTRY_COMPARE_WITH_TIMEOUT(released.count(), 1, 2000);
    server.stop();
}

void TestTciMoxReleaseOnDisconnect::with_a_radio_model_the_radio_is_unkeyed()
{
    RadioModel model;
    TciServer  server(&model);
    QVERIFY(server.start(0));
    QSignalSpy released(&server, &TciServer::moxReleasedOnClientLoss);

    QWebSocket client;
    QVERIFY(connectClient(client, server));
    QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 1, 2000);

    client.sendTextMessage(QStringLiteral("trx:0,true;"));
    QTRY_VERIFY_WITH_TIMEOUT(model.mox(), 2000);

    client.abort();
    QTRY_VERIFY_WITH_TIMEOUT(!model.mox(), 2000);
    QCOMPARE(released.count(), 1);
    server.stop();
}

void TestTciMoxReleaseOnDisconnect::an_operator_who_keyed_after_the_client_let_go_stays_keyed()
{
    // The rule must never unkey the operator. The client keys, the radio
    // is unkeyed (locally, or by the client — MOX off from any source
    // clears the owner), then the operator keys from the console. When
    // the client now vanishes, MOX stays on.
    RadioModel model;
    TciServer  server(&model);
    QVERIFY(server.start(0));
    QSignalSpy released(&server, &TciServer::moxReleasedOnClientLoss);

    QWebSocket client;
    QVERIFY(connectClient(client, server));
    QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 1, 2000);

    client.sendTextMessage(QStringLiteral("trx:0,true;"));
    QTRY_VERIFY_WITH_TIMEOUT(model.mox(), 2000);

    model.setMox(false);            // operator unkeys at the console
    QTRY_VERIFY_WITH_TIMEOUT(!model.mox(), 2000);
    model.setMox(true);             // ... and keys again, locally
    QTRY_VERIFY_WITH_TIMEOUT(model.mox(), 2000);

    client.abort();
    QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 0, 2000);
    QTest::qWait(150);
    QVERIFY2(model.mox(), "the operator's own key-up was released when the client vanished");
    QCOMPARE(released.count(), 0);

    model.setMox(false);
    server.stop();
}

void TestTciMoxReleaseOnDisconnect::a_hung_client_that_stops_answering_pings_is_released()
{
    // The other way a keying client goes missing: not a closed socket but
    // a frozen program. The watchdog pings the owner and lets go after
    // three unanswered pings — the socket stays open.
    TciServer server(nullptr);
    server.setKeyedWatchdog(100, 3);      // 1000 ms / 3 in the app; fast here
    QVERIFY(server.start(0));
    QSignalSpy released(&server, &TciServer::moxReleasedOnClientLoss);

    HungClient hung;
    QVERIFY2(hung.connectAndKey(server.port()), "raw WebSocket handshake failed");
    QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 1, 2000);

    // ≥ 3 unanswered pings at 100 ms → released well inside 2 s.
    QTRY_COMPARE_WITH_TIMEOUT(released.count(), 1, 2000);
    // And the socket is still there: the watchdog releases the key, it
    // does not throw the client out.
    QCOMPARE(hung.sock.state(), QAbstractSocket::ConnectedState);
    QCOMPARE(server.clientCount(), 1);

    hung.sock.abort();
    QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 0, 2000);
    // The disconnect must not release a second time — ownership is gone.
    QTest::qWait(100);
    QCOMPARE(released.count(), 1);
    server.stop();
}

void TestTciMoxReleaseOnDisconnect::a_live_client_that_answers_pings_keeps_the_key()
{
    // The control: a client whose event loop runs answers every ping, and
    // the watchdog must never fire on it, however long it stays keyed.
    TciServer server(nullptr);
    server.setKeyedWatchdog(50, 3);
    QVERIFY(server.start(0));
    QSignalSpy released(&server, &TciServer::moxReleasedOnClientLoss);

    QWebSocket client;
    QVERIFY(connectClient(client, server));
    QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 1, 2000);
    client.sendTextMessage(QStringLiteral("trx:0,true;"));

    // 1 s at 50 ms = ~20 pings, each answered by QWebSocket itself.
    QTest::qWait(1000);
    QCOMPARE(released.count(), 0);
    QCOMPARE(server.clientCount(), 1);

    client.close();
    server.stop();
}

QTEST_GUILESS_MAIN(TestTciMoxReleaseOnDisconnect)
#include "tst_tci_mox_release_on_disconnect.moc"
