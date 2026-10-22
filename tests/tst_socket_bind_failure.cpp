// no-port-check: Longpath-original test for the UDP bind-failure path
// (2026-10-12).
//
// init() creates the socket, binds it, and then creates the timers the
// rest of the class uses. The bind used to return early on failure --
// before the timers existed -- so the next connect ran straight into
// m_keepAliveTimer->start() (P2) or m_watchdogTimer->start() (P1) on a
// null pointer. Upstream NereusSDR fixed the same thing after its
// 2026.10.0 release ("Avoid a crash on the next connection attempt when
// the initial Protocol 1 or 2 UDP socket could not bind"); we had
// inherited the shape that allows it.
//
// A bind on port 0 cannot be made to fail from the outside, so the test
// subclasses each connection and refuses the bind. Without the fix these
// cases crash rather than fail -- which is the point.

#include <QtTest/QtTest>
#include <QtTest/QSignalSpy>

#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"   // RadioInfo

using namespace Longpath;

namespace {

class UnbindableP1 : public P1RadioConnection {
public:
    using P1RadioConnection::P1RadioConnection;
protected:
    bool bindSocket() override { return false; }
};

class UnbindableP2 : public P2RadioConnection {
public:
    using P2RadioConnection::P2RadioConnection;
protected:
    bool bindSocket() override { return false; }
};

RadioInfo someRadio()
{
    RadioInfo info;
    info.address = QHostAddress(QStringLiteral("192.0.2.1"));  // TEST-NET-1, goes nowhere
    info.macAddress = QStringLiteral("00:11:22:33:44:55");
    return info;
}

}  // namespace

class TestSocketBindFailure : public QObject {
    Q_OBJECT

private slots:
    void p1_reports_the_bind_failure_instead_of_crashing()
    {
        UnbindableP1 conn;
        QSignalSpy errors(&conn, &RadioConnection::errorOccurred);
        QSignalSpy failures(&conn, &RadioConnection::connectFailed);

        conn.init();
        QVERIFY2(errors.count() >= 1, "init() stayed silent about a failed bind");
        QCOMPARE(errors.first().at(0).value<RadioConnectionError>(),
                 RadioConnectionError::SocketBindFailure);

        // The crash was here: connect after a failed bind.
        conn.connectToRadio(someRadio());
        QVERIFY2(failures.count() >= 1, "connect after a failed bind said nothing");
        QCOMPARE(failures.first().at(0).value<ConnectFailure>(),
                 ConnectFailure::Unreachable);
    }

    void p2_reports_the_bind_failure_instead_of_crashing()
    {
        UnbindableP2 conn;
        QSignalSpy errors(&conn, &RadioConnection::errorOccurred);
        QSignalSpy failures(&conn, &RadioConnection::connectFailed);

        conn.init();
        QVERIFY2(errors.count() >= 1, "init() stayed silent about a failed bind");
        QCOMPARE(errors.first().at(0).value<RadioConnectionError>(),
                 RadioConnectionError::SocketBindFailure);

        conn.connectToRadio(someRadio());
        QVERIFY2(failures.count() >= 1, "connect after a failed bind said nothing");
        QCOMPARE(failures.first().at(0).value<ConnectFailure>(),
                 ConnectFailure::Unreachable);
    }

    void a_second_connect_after_a_failed_bind_is_also_survivable()
    {
        // A logger or the auto-reconnect will try again. Every attempt
        // has to end in a refusal, not in a crash on the second pass.
        UnbindableP2 conn;
        conn.init();
        QSignalSpy failures(&conn, &RadioConnection::connectFailed);
        for (int i = 0; i < 3; ++i) {
            conn.connectToRadio(someRadio());
        }
        QCOMPARE(failures.count(), 3);
    }

    void an_ordinary_bind_still_works()
    {
        // The counter-check: the real classes bind on port 0 and connect
        // normally, so the guard above cannot be hiding a broken bind.
        P2RadioConnection conn;
        QSignalSpy errors(&conn, &RadioConnection::errorOccurred);
        conn.init();
        for (const auto& e : errors) {
            QVERIFY2(e.at(0).value<RadioConnectionError>()
                         != RadioConnectionError::SocketBindFailure,
                     "a plain init() reported a bind failure");
        }
    }
};

QTEST_MAIN(TestSocketBindFailure)
#include "tst_socket_bind_failure.moc"
