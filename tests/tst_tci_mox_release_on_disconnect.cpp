// SPDX-License-Identifier: GPL-2.0-or-later
// tests/tst_tci_mox_release_on_disconnect.cpp (NereusSDR)
//
// A TCI client that keys the radio (trx:N,true) and then goes away — WSJT-X
// crashing mid-over, a remote client losing its link — must not leave the
// transmitter keyed. Thetis has exactly that gap; the Zeus station engine
// closes it with a transmit lease and a heartbeat. TciServer's version
// (2026-09-17): remember who keyed, and unkey when that socket vanishes.
//
// The first three tests run without a RadioModel (server(nullptr), the same
// injection path tst_tci_tx_mutex uses) and check the bookkeeping through
// the moxReleasedOnClientLoss signal. The last two put a real RadioModel
// behind the server and check MOX itself — including the case that must
// NOT unkey: the operator keyed after the client let go.
// no-port-check: NereusSDR-original integration test.

#include <QtTest>
#include <QSignalSpy>
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

} // namespace

class TestTciMoxReleaseOnDisconnect : public QObject {
    Q_OBJECT
private slots:
    void a_client_that_keyed_and_vanished_triggers_the_release();
    void a_client_that_unkeyed_before_vanishing_does_not();
    void the_release_follows_the_last_client_that_keyed();
    void with_a_radio_model_the_radio_is_unkeyed();
    void an_operator_who_keyed_after_the_client_let_go_stays_keyed();
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

QTEST_GUILESS_MAIN(TestTciMoxReleaseOnDisconnect)
#include "tst_tci_mox_release_on_disconnect.moc"
