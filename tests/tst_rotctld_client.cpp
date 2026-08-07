// The rotator link, tested against a stand-in rotctld rather than
// against a rotator. The operator's rotator is off the network, and a
// protocol written blind and never exercised is a protocol that is
// wrong in a way nobody finds until the antenna does not move.
//
// The stand-in speaks the same line protocol Hamlib does:
//   p        → "<az>\n<el>\n"
//   P az el  → "RPRT 0\n"   (or a configured error)
//   S        → "RPRT 0\n"
// no-port-check: NereusSDR-original.

#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>

#include "core/RotctldClient.h"

using namespace NereusSDR;

// A rotctld that never moves anything. It answers, it can be told to
// refuse, and it can be told to reply in fragments — which is what a
// real socket does and what a line parser gets wrong.
//
// At file scope rather than in an anonymous namespace: moc has a long
// history of trouble with Q_OBJECT inside one, and a test helper that
// fails to generate its metaobject fails at link time with a message
// that points nowhere useful.
class FakeRotctld : public QObject {
    Q_OBJECT
public:
    explicit FakeRotctld(QObject* parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            m_conn = m_server.nextPendingConnection();
            connect(m_conn, &QTcpSocket::readyRead, this, &FakeRotctld::onRead);
        });
    }

    bool listen()
    {
        return m_server.listen(QHostAddress::LocalHost, 0);
    }
    quint16 port() const { return m_server.serverPort(); }

    double azimuth{123.0};
    double elevation{0.0};
    int    moveReport{0};     // what P replies with
    bool   splitReplies{false};
    QStringList received;

    void closeConnection()
    {
        if (m_conn) { m_conn->disconnectFromHost(); }
    }

private slots:
    void onRead()
    {
        m_in += m_conn->readAll();
        int nl;
        while ((nl = m_in.indexOf('\n')) >= 0) {
            const QByteArray line = m_in.left(nl).trimmed();
            m_in.remove(0, nl + 1);
            received << QString::fromLatin1(line);

            if (line == "p") {
                reply(QStringLiteral("%1\n%2\n")
                          .arg(azimuth, 0, 'f', 6)
                          .arg(elevation, 0, 'f', 6).toLatin1());
            } else if (line.startsWith("P ")) {
                if (moveReport == 0) {
                    azimuth = line.split(' ').at(1).toDouble();
                }
                reply(QStringLiteral("RPRT %1\n").arg(moveReport).toLatin1());
            } else if (line == "S") {
                reply(QByteArrayLiteral("RPRT 0\n"));
            } else {
                reply(QByteArrayLiteral("RPRT -1\n"));
            }
        }
    }

private:
    void reply(const QByteArray& bytes)
    {
        if (!m_conn) { return; }
        if (!splitReplies) {
            m_conn->write(bytes);
            return;
        }
        // One byte at a time, flushed. A parser that assumes a reply
        // arrives whole passes every other test and fails here.
        for (int i = 0; i < bytes.size(); ++i) {
            m_conn->write(bytes.mid(i, 1));
            m_conn->flush();
        }
    }

    QTcpServer  m_server;
    QTcpSocket* m_conn{nullptr};
    QByteArray  m_in;
};

class TstRotctldClient : public QObject {
    Q_OBJECT
private slots:
    void position_reply_parses();
    void an_error_report_is_never_a_position();
    void report_codes_read_as_english();
    void move_command_uses_plain_decimals();
    void move_command_normalises_the_bearing();

    void connects_and_reads_a_position();
    void a_move_reaches_the_rotator_and_completes();
    void a_refused_move_reports_and_does_not_pretend();
    void replies_split_across_packets_still_parse();
    void a_dropped_connection_marks_the_position_stale();
    void moving_while_disconnected_complains_rather_than_silently_failing();
};

// ── Pure parsing ────────────────────────────────────────────────────

void TstRotctldClient::position_reply_parses()
{
    double az = 0, el = 0;
    QVERIFY(RotctldClient::parsePosition("145.000000\n0.000000\n", az, el));
    QCOMPARE(az, 145.0);
    QCOMPARE(el, 0.0);

    QVERIFY(RotctldClient::parsePosition("  12.5 \n -3.25 \n", az, el));
    QCOMPARE(az, 12.5);
    QCOMPARE(el, -3.25);

    // One line is not a position.
    QVERIFY(!RotctldClient::parsePosition("145.000000\n", az, el));
    QVERIFY(!RotctldClient::parsePosition("", az, el));
    QVERIFY(!RotctldClient::parsePosition("north\nup\n", az, el));
}

void TstRotctldClient::an_error_report_is_never_a_position()
{
    // The failure this guard exists for: "RPRT -1" trimmed and split
    // yields a single token that parses as the number -1. Without the
    // check, every error swings the needle to 359 degrees.
    double az = 999, el = 999;
    QVERIFY(!RotctldClient::parsePosition("RPRT -1\n", az, el));
    QVERIFY(!RotctldClient::parsePosition("RPRT 0\n", az, el));
    QCOMPARE(az, 999.0);   // outputs untouched on failure
    QCOMPARE(el, 999.0);
}

void TstRotctldClient::report_codes_read_as_english()
{
    int code = -99;
    QVERIFY(RotctldClient::parseReport("RPRT 0\n", code));
    QCOMPARE(code, 0);
    QVERIFY(RotctldClient::parseReport("RPRT -11\n", code));
    QCOMPARE(code, -11);
    QVERIFY(!RotctldClient::parseReport("145.0\n0.0\n", code));

    // -11 is the one an operator meets in practice: asking for a
    // bearing the rotator cannot reach.
    QVERIFY(RotctldClient::describeReport(-11).contains(QStringLiteral("range")));
    QVERIFY(!RotctldClient::describeReport(-42).isEmpty());
}

void TstRotctldClient::move_command_uses_plain_decimals()
{
    // rotctld rejects "145,00" and the antenna then does not move, with
    // nothing in the error pointing at a decimal comma.
    //
    // This passes today without the guard: QString::arg(double) formats
    // through the C locale whatever the user's is — only %L1 asks for
    // theirs. The test is here for the edit that has not happened yet.
    // Someone tidying this to %L1, or to QLocale::toString(), would
    // break every rotator outside the English-speaking world and see
    // nothing wrong on their own machine.
    const QLocale saved = QLocale();
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
    const QByteArray cmd = RotctldClient::moveCommand(145.0);
    QLocale::setDefault(saved);

    QCOMPARE(cmd, QByteArrayLiteral("P 145.00 0.00\n"));
    QVERIFY(!cmd.contains(','));
}

void TstRotctldClient::move_command_normalises_the_bearing()
{
    QCOMPARE(RotctldClient::moveCommand(-10.0),
             QByteArrayLiteral("P 350.00 0.00\n"));
    QCOMPARE(RotctldClient::moveCommand(370.0),
             QByteArrayLiteral("P 10.00 0.00\n"));
}

// ── Against a stand-in rotctld ──────────────────────────────────────

void TstRotctldClient::connects_and_reads_a_position()
{
    FakeRotctld fake;
    QVERIFY(fake.listen());
    fake.azimuth = 217.5;

    RotctldClient c;
    c.setTarget(QStringLiteral("127.0.0.1"), fake.port());
    QSignalSpy pos(&c, &RotorController::positionChanged);
    c.connectToRotor();

    QVERIFY(pos.wait(3000));
    QCOMPARE(c.azimuth(), 217.5);
    QVERIFY(c.isConnected());
    QVERIFY(c.hasFreshPosition());
}

void TstRotctldClient::a_move_reaches_the_rotator_and_completes()
{
    FakeRotctld fake;
    QVERIFY(fake.listen());
    fake.azimuth = 0.0;

    RotctldClient c;
    c.setPollIntervalMs(100);
    c.setTarget(QStringLiteral("127.0.0.1"), fake.port());
    QSignalSpy pos(&c, &RotorController::positionChanged);
    c.connectToRotor();
    QVERIFY(pos.wait(3000));

    c.moveTo(298.0);
    QCOMPARE(c.state(), RotorController::State::Moving);

    // The stand-in jumps straight there, so the client should notice it
    // has arrived and drop back to Idle by itself. A Moving state that
    // never ends leaves the dial claiming the mast is still turning.
    QTRY_COMPARE_WITH_TIMEOUT(c.state(), RotorController::State::Idle, 3000);
    QVERIFY(fake.received.contains(QStringLiteral("P 298.00 0.00")));
    QVERIFY(qAbs(c.azimuth() - 298.0) < 0.01);
}

void TstRotctldClient::a_refused_move_reports_and_does_not_pretend()
{
    FakeRotctld fake;
    QVERIFY(fake.listen());
    fake.moveReport = -11;          // outside the rotator's range

    RotctldClient c;
    c.setTarget(QStringLiteral("127.0.0.1"), fake.port());
    QSignalSpy pos(&c, &RotorController::positionChanged);
    QSignalSpy err(&c, &RotorController::errorOccurred);
    c.connectToRotor();
    QVERIFY(pos.wait(3000));

    const double before = c.azimuth();
    c.moveTo(45.0);
    QVERIFY(err.wait(3000));
    QVERIFY2(err.first().first().toString().contains(QStringLiteral("range")),
             qPrintable(err.first().first().toString()));
    QCOMPARE(c.state(), RotorController::State::Error);
    // The refusal must not move the reported position.
    QCOMPARE(c.azimuth(), before);
}

void TstRotctldClient::replies_split_across_packets_still_parse()
{
    // TCP does not preserve message boundaries. A parser that reads
    // whatever arrived and assumes it is a whole reply works on a LAN
    // and fails over a congested link — the worst kind of bug to find
    // later.
    FakeRotctld fake;
    QVERIFY(fake.listen());
    fake.splitReplies = true;
    fake.azimuth = 88.25;

    RotctldClient c;
    c.setTarget(QStringLiteral("127.0.0.1"), fake.port());
    QSignalSpy pos(&c, &RotorController::positionChanged);
    c.connectToRotor();

    QVERIFY(pos.wait(3000));
    QCOMPARE(c.azimuth(), 88.25);
}

void TstRotctldClient::a_dropped_connection_marks_the_position_stale()
{
    FakeRotctld fake;
    QVERIFY(fake.listen());
    fake.azimuth = 45.0;

    RotctldClient c;
    c.setTarget(QStringLiteral("127.0.0.1"), fake.port());
    QSignalSpy pos(&c, &RotorController::positionChanged);
    c.connectToRotor();
    QVERIFY(pos.wait(3000));
    QVERIFY(c.hasFreshPosition());

    fake.closeConnection();
    QTRY_VERIFY_WITH_TIMEOUT(!c.isConnected(), 3000);

    // The last reading is still remembered, but it must stop counting
    // as fresh — a needle showing a stale position with no hint that it
    // is stale is worse than a needle showing nothing.
    QTRY_VERIFY_WITH_TIMEOUT(!c.hasFreshPosition(),
                             RotctldClient::kStaleAfterMs + 1500);
    QCOMPARE(c.azimuth(), 45.0);
}

void TstRotctldClient::moving_while_disconnected_complains_rather_than_silently_failing()
{
    RotctldClient c;
    c.setTarget(QStringLiteral("127.0.0.1"), 1);   // nothing listening
    QSignalSpy err(&c, &RotorController::errorOccurred);

    c.moveTo(180.0);
    QCOMPARE(err.count(), 1);
    QVERIFY(err.first().first().toString()
                .contains(QStringLiteral("not connected")));
}

QTEST_MAIN(TstRotctldClient)
#include "tst_rotctld_client.moc"
