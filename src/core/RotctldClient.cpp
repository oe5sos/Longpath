// =================================================================
// src/core/RotctldClient.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — see RotctldClient.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-08-10 — Reply watchdog + elevation kept; see RotctldClient.h.
//                 AI-assisted via Anthropic Claude (Cowork), operator
//                 Martin Fischer.
// =================================================================

#include "RotctldClient.h"

#include <QTimer>

#include <cmath>

namespace NereusSDR {

namespace {

// Hamlib treats a rotator as arrived when it is within a degree or so.
// Below that, mechanical slop and encoder resolution mean "moving" and
// "stopped" are indistinguishable, and a Moving state that never ends
// is worse than one that ends slightly early.
constexpr double kArrivedDeg = 1.5;

double norm360(double d)
{
    d = std::fmod(d, 360.0);
    return d < 0.0 ? d + 360.0 : d;
}

double shortestGap(double a, double b)
{
    double d = std::fmod(std::abs(a - b), 360.0);
    return d > 180.0 ? 360.0 - d : d;
}

} // namespace

RotctldClient::RotctldClient(QObject* parent) : RotorController(parent)
{
    m_poll = new QTimer(this);
    m_poll->setInterval(m_pollMs);
    connect(m_poll, &QTimer::timeout, this, [this]() {
        // One outstanding command at a time. If the last poll has not
        // come back, do not stack another — rotctld answers in order,
        // and a queue of stale position requests only delays the next
        // move command behind them.
        if (m_awaiting == Pending::None && m_queue.isEmpty()) {
            send(QByteArrayLiteral("p\n"), Pending::Position);
        }
    });

    m_retry = new QTimer(this);
    m_retry->setSingleShot(true);
    m_retry->setInterval(3000);
    connect(m_retry, &QTimer::timeout, this, [this]() {
        if (m_state == State::Disconnected && !m_host.isEmpty()) {
            connectToRotor();
        }
    });

    // Reply watchdog. Without it, a rotctld that freezes while its TCP
    // side stays open leaves m_awaiting set forever: the poll timer's
    // "one command at a time" guard never passes again, and the client
    // is silently dead until someone reconnects by hand. Cutting the
    // connection routes recovery through the disconnect path above,
    // which already knows how to retry.
    m_deadline = new QTimer(this);
    m_deadline->setSingleShot(true);
    m_deadline->setInterval(kReplyTimeoutMs);
    connect(m_deadline, &QTimer::timeout, this, [this]() {
        if (m_awaiting == Pending::None) { return; }
        emit errorOccurred(
            QStringLiteral("Rotator stopped answering — reconnecting"));
        // abort() emits no disconnected(); do the teardown here and let
        // the retry timer bring the link back.
        m_socket.abort();
        m_poll->stop();
        m_queue.clear();
        m_buffer.clear();
        m_awaiting = Pending::None;
        setState(State::Disconnected);
        if (!m_host.isEmpty()) { m_retry->start(); }
    });

    connect(&m_socket, &QTcpSocket::connected, this, [this]() {
        m_buffer.clear();
        m_queue.clear();
        m_awaiting = Pending::None;
        setState(State::Idle);
        m_poll->start();
        send(QByteArrayLiteral("p\n"), Pending::Position);
    });

    connect(&m_socket, &QTcpSocket::readyRead,
            this, &RotctldClient::onReadyRead);

    connect(&m_socket, &QTcpSocket::disconnected, this, [this]() {
        m_poll->stop();
        m_deadline->stop();
        setState(State::Disconnected);
        // Keep trying. A rotator controller that is power-cycled mid
        // session should come back on its own rather than needing the
        // operator to notice and reconnect by hand.
        if (!m_host.isEmpty()) { m_retry->start(); }
    });

    connect(&m_socket, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
        fail(m_socket.errorString());
    });
}

// ── Configuration ───────────────────────────────────────────────────

void RotctldClient::setTarget(const QString& host, quint16 port)
{
    m_host = host.trimmed();
    m_port = port;
}

void RotctldClient::setPollIntervalMs(int ms)
{
    m_pollMs = qBound(100, ms, 10000);
    m_poll->setInterval(m_pollMs);
}

QString RotctldClient::description() const
{
    if (m_host.isEmpty()) { return QStringLiteral("rotctld (not set up)"); }
    return QStringLiteral("rotctld %1:%2").arg(m_host).arg(m_port);
}

bool RotctldClient::hasFreshPosition() const
{
    if (!m_lastPosition.isValid()) { return false; }
    return m_lastPosition.msecsTo(QDateTime::currentDateTimeUtc())
           < kStaleAfterMs;
}

// ── Connection ──────────────────────────────────────────────────────

void RotctldClient::connectToRotor()
{
    if (m_host.isEmpty()) {
        emit errorOccurred(QStringLiteral("No rotator address set"));
        return;
    }
    if (m_socket.state() != QAbstractSocket::UnconnectedState) { return; }

    m_retry->stop();
    setState(State::Connecting);
    m_socket.connectToHost(m_host, m_port);
}

void RotctldClient::disconnectFromRotor()
{
    // Explicit: stop retrying too. Otherwise "disconnect" reconnects
    // three seconds later and looks like the button does nothing.
    m_retry->stop();
    m_poll->stop();
    m_deadline->stop();
    m_queue.clear();
    m_awaiting = Pending::None;
    m_socket.abort();
    setState(State::Disconnected);
}

// ── Commands ────────────────────────────────────────────────────────

QByteArray RotctldClient::moveCommand(double azimuthDeg, double elevationDeg)
{
    // Plain decimals. rotctld wants "145.00"; a decimal comma is
    // rejected and the antenna then simply does not move, with nothing
    // in the error to connect it to a number format.
    //
    // As written this is already safe, and it is worth being exact
    // about why: QString::arg(double) formats through QLocaleData::c(),
    // the C locale, regardless of the user's. Only the %L1 form asks
    // for the user's locale. So the danger is not today's code — it is
    // an innocent-looking future edit to %L1, or a switch to
    // QLocale::toString(), either of which would break this silently
    // on every machine outside the English-speaking world. The test
    // sets a German locale and pins the output for exactly that.
    return QStringLiteral("P %1 %2\n")
        .arg(norm360(azimuthDeg), 0, 'f', 2)
        .arg(elevationDeg, 0, 'f', 2)
        .toLatin1();
}

void RotctldClient::moveTo(double azimuthDeg)
{
    if (!isConnected()) {
        emit errorOccurred(QStringLiteral("Rotator is not connected"));
        return;
    }
    m_commanded = norm360(azimuthDeg);
    m_haveCommanded = true;
    // Send the elevation the rotator last reported, not 0: "P az el"
    // sets both axes, and forcing 0 would slam an az/el rotator's
    // elevation down with every azimuth command. For an azimuth-only
    // rotator the reported elevation IS 0, so nothing changes there.
    send(moveCommand(m_commanded, m_elevation), Pending::Report);
    setState(State::Moving);
}

void RotctldClient::stop()
{
    if (!isConnected()) { return; }
    m_haveCommanded = false;
    send(QByteArrayLiteral("S\n"), Pending::Report);
    setState(State::Idle);
}

void RotctldClient::send(const QByteArray& command, Pending expect)
{
    m_queue.enqueue(Command{command, expect});
    pump();
}

void RotctldClient::pump()
{
    if (m_awaiting != Pending::None) { return; }
    if (m_queue.isEmpty()) { return; }
    if (m_socket.state() != QAbstractSocket::ConnectedState) { return; }

    const Command c = m_queue.dequeue();
    m_awaiting = c.expect;
    m_socket.write(c.bytes);
    m_deadline->start();
}

// ── Replies ─────────────────────────────────────────────────────────

bool RotctldClient::parseReport(const QByteArray& reply, int& code)
{
    const QByteArray t = reply.trimmed();
    if (!t.startsWith("RPRT")) { return false; }
    bool ok = false;
    const int v = t.mid(4).trimmed().toInt(&ok);
    if (!ok) { return false; }
    code = v;
    return true;
}

bool RotctldClient::parsePosition(const QByteArray& reply,
                                  double& azDeg, double& elDeg)
{
    // An RPRT line is never a position. Without this check "RPRT -1"
    // splits into one token that parses as the number -1, and the
    // needle swings to 359 degrees on every error.
    int ignored = 0;
    if (parseReport(reply, ignored)) { return false; }

    const QList<QByteArray> lines = reply.trimmed().split('\n');
    if (lines.size() < 2) { return false; }

    bool okAz = false, okEl = false;
    const double az = lines.at(0).trimmed().toDouble(&okAz);
    const double el = lines.at(1).trimmed().toDouble(&okEl);
    if (!okAz || !okEl) { return false; }

    azDeg = az;
    elDeg = el;
    return true;
}

QString RotctldClient::describeReport(int code)
{
    switch (code) {
    case 0:   return QStringLiteral("accepted");
    case -1:  return QStringLiteral("the rotator does not support that");
    case -2:  return QStringLiteral("invalid parameter");
    case -3:  return QStringLiteral("invalid configuration");
    case -4:  return QStringLiteral("out of memory");
    case -5:  return QStringLiteral("not implemented");
    case -6:  return QStringLiteral("timed out");
    case -8:  return QStringLiteral("input/output error");
    case -9:  return QStringLiteral("internal Hamlib error");
    case -11: return QStringLiteral("target is outside the rotator's range");
    default:  return QStringLiteral("rotctld error %1").arg(code);
    }
}

void RotctldClient::onReadyRead()
{
    m_buffer += m_socket.readAll();

    while (true) {
        if (m_awaiting == Pending::None) { break; }

        // A report is one line; a position is two. Wait for as many as
        // the outstanding command will produce, so half a reply is
        // never parsed as a whole one.
        const int want = (m_awaiting == Pending::Position) ? 2 : 1;
        if (m_buffer.count('\n') < want) { break; }

        int cut = -1;
        for (int i = 0, seen = 0; i < m_buffer.size(); ++i) {
            if (m_buffer.at(i) == '\n' && ++seen == want) { cut = i; break; }
        }
        if (cut < 0) { break; }

        const QByteArray reply = m_buffer.left(cut + 1);
        m_buffer.remove(0, cut + 1);
        const Pending was = m_awaiting;
        m_awaiting = Pending::None;
        m_deadline->stop();

        if (was == Pending::Position) {
            double az = 0.0, el = 0.0;
            if (parsePosition(reply, az, el)) {
                m_azimuth = norm360(az);
                m_lastPosition = QDateTime::currentDateTimeUtc();
                emit positionChanged(m_azimuth);
                if (!qFuzzyCompare(m_elevation + 1.0, el + 1.0)) {
                    m_elevation = el;
                    emit elevationChanged(m_elevation);
                }

                if (m_state == State::Moving && m_haveCommanded
                    && shortestGap(m_azimuth, m_commanded) <= kArrivedDeg) {
                    m_haveCommanded = false;
                    setState(State::Idle);
                }
            } else {
                int code = 0;
                if (parseReport(reply, code) && code != 0) {
                    // The rotator answered a position request with an
                    // error. Report it, and leave the last known
                    // position to go stale on its own rather than
                    // replacing it with a number that is not a bearing.
                    emit errorOccurred(describeReport(code));
                    setState(State::Error);
                }
            }
        } else {
            int code = 0;
            if (parseReport(reply, code) && code != 0) {
                emit errorOccurred(describeReport(code));
                setState(State::Error);
                m_haveCommanded = false;
            } else if (m_state == State::Error) {
                setState(m_haveCommanded ? State::Moving : State::Idle);
            }
        }
    }
    pump();
}

// ── State ───────────────────────────────────────────────────────────

void RotctldClient::setState(State s)
{
    if (m_state == s) { return; }
    m_state = s;
    emit stateChanged(s);
}

void RotctldClient::fail(const QString& why)
{
    // Socket errors arrive alongside disconnected(); do not announce
    // the same failure twice.
    if (m_state != State::Disconnected) {
        emit errorOccurred(why);
    }
}

} // namespace NereusSDR
