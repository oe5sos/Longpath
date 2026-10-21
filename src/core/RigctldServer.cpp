// =================================================================
// src/core/RigctldServer.cpp  (Longpath)
// =================================================================
//
// Longpath-original — see RigctldServer.h for what this is, what it is
// not, and where the protocol was read from.
//
// AI tooling: Anthropic Claude Code.

#include "RigctldServer.h"

#include "LogCategories.h"

#include <QtCore/QStringList>
#include <QtNetwork/QTcpSocket>

namespace Longpath {

namespace {

// Hamlib answers every setter with RPRT <errno>; 0 is success.
// The two we use: RIG_OK and RIG_EINVAL/-1 for "refused".
const QString kRprtOk      = QStringLiteral("RPRT 0\n");
const QString kRprtRefused = QStringLiteral("RPRT -1\n");
// RIG_ENIMPL — "function not implemented". What a well-behaved rigctld
// answers for a command it knows of but cannot serve; clients treat it
// as "ask for something else", not as a broken link.
const QString kRprtNotImpl = QStringLiteral("RPRT -11\n");

// Hamlib rmode_t bits, from the transcript's dump_state and the mode
// tokens a client sends. Only the modes Longpath has are advertised.
//   AM 0x1, CW 0x2, USB 0x4, LSB 0x8, RTTY 0x10, FM 0x20, CWR 0x80,
//   AMS 0x200, PKTLSB 0x400, PKTUSB 0x800
constexpr unsigned kModeMask = 0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x20
                             | 0x80 | 0x200 | 0x400 | 0x800;   // 0xEBF

// What we tell a client about frequency coverage. This is an ANSWER,
// not a gate: what may actually be keyed is decided by BandPlanGuard
// on the transmit path, exactly as for every other transmit source.
// 100 kHz to 61.44 MHz is the span the supported boards cover
// (HF + 6 m at the highest sample rate).
// VFO mask for the range lists. See the comment at the RX range line:
// this decides whether a client's set_mode/set_freq is even sent.
constexpr unsigned kVfoMask = 0x17e00007;

constexpr double kRxLowHz  = 100000.0;
constexpr double kRxHighHz = 61440000.0;

}  // namespace

RigctldServer::RigctldServer(QObject* radio, QObject* parent)
    : QObject(parent)
    , m_radio(radio)
{
    connect(&m_server, &QTcpServer::newConnection,
            this, &RigctldServer::onNewConnection);
}

RigctldServer::~RigctldServer()
{
    stop();
}

bool RigctldServer::start(const QHostAddress& address, quint16 port)
{
    if (m_server.isListening()) {
        stop();
    }
    if (!m_server.listen(address, port)) {
        qCWarning(lcRigctld) << "RigctldServer: cannot listen on"
                         << address.toString() << port
                         << "—" << m_server.errorString();
        return false;
    }
    qCInfo(lcRigctld) << "RigctldServer: listening on"
                  << address.toString() << m_server.serverPort();
    emit listeningChanged(true);
    return true;
}

void RigctldServer::stop()
{
    const bool wasListening = m_server.isListening();

    for (QTcpSocket* c : std::as_const(m_clients)) {
        c->disconnect(this);
        c->abort();
        c->deleteLater();
    }
    m_clients.clear();
    m_server.close();

    // The transmit gate does not survive a stop. Starting the server
    // again starts it closed, whatever it was before.
    m_pttAllowed = false;

    if (wasListening) {
        qCInfo(lcRigctld) << "RigctldServer: stopped";
        emit listeningChanged(false);
        emit clientCountChanged(0);
    }
}

bool    RigctldServer::isListening() const { return m_server.isListening(); }
quint16 RigctldServer::port() const        { return m_server.serverPort(); }
int     RigctldServer::clientCount() const { return int(m_clients.size()); }
bool    RigctldServer::pttAllowed() const  { return m_pttAllowed; }

void RigctldServer::setPttAllowed(bool on)
{
    if (m_pttAllowed == on) {
        return;
    }
    m_pttAllowed = on;
    qCInfo(lcRigctld) << "RigctldServer: PTT over rigctld is now"
                  << (on ? "ALLOWED" : "refused");
}

void RigctldServer::onNewConnection()
{
    while (QTcpSocket* sock = m_server.nextPendingConnection()) {
        m_clients.append(sock);
        connect(sock, &QTcpSocket::readyRead,
                this, &RigctldServer::onClientReadyRead);
        connect(sock, &QTcpSocket::disconnected,
                this, &RigctldServer::onClientDisconnected);
        qCInfo(lcRigctld) << "RigctldServer: client from"
                      << sock->peerAddress().toString();
        emit clientCountChanged(clientCount());
    }
}

void RigctldServer::onClientReadyRead()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) {
        return;
    }
    while (sock->canReadLine()) {
        const QString line =
            QString::fromLatin1(sock->readLine()).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QString reply = handleLine(line);
        emit commandLogged(line, reply.isNull() ? kRprtOk : reply);
        if (reply.isNull()) {
            // `q` — acknowledge, then let go.
            sock->write(kRprtOk.toLatin1());
            sock->flush();
            sock->disconnectFromHost();
            return;
        }
        sock->write(reply.toLatin1());
    }
    sock->flush();
}

void RigctldServer::onClientDisconnected()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) {
        return;
    }
    m_clients.removeAll(sock);
    sock->deleteLater();
    qCInfo(lcRigctld) << "RigctldServer: client gone," << clientCount() << "left";
    emit clientCountChanged(clientCount());
}

// ---------------------------------------------------------------------------
// Protocol
//
// Connect sequence a real client runs, read off the wire 2026-10-11:
//   \get_powerstat → 1
//   \chk_vfo       → 0
//   \dump_state    → the block below, ending in "done"
//   v              → VFOA
//   l KEYSPD       → 0
//   f              → frequency
// and on the way out: q → RPRT 0.
// ---------------------------------------------------------------------------

QString RigctldServer::handleLine(const QString& line)
{
    QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return kRprtNotImpl;
    }
    QString cmd = parts.takeFirst();

    // Long form: a client may write \dump_state or +\dump_state. The
    // leading '+' asks for the verbose answer format; we answer the
    // plain one either way, which the clients accept.
    while (cmd.startsWith(QLatin1Char('+')) || cmd.startsWith(QLatin1Char('-'))) {
        cmd.remove(0, 1);
    }
    const bool longForm = cmd.startsWith(QLatin1Char('\\'));
    if (longForm) {
        cmd.remove(0, 1);
    }

    // Long-form getters answer "<value>\nRPRT 0\n" -- value AND status.
    // Found the hard way on 2026-10-11: answering \get_lock_mode with a
    // bare RPRT -11 left the client waiting for a value it never got, and
    // it sat in a 20-second timeout before sending the NEXT command. That
    // is why `M CW 500` appeared to be ignored while `F` worked: the mode
    // command was still stuck behind the stalled lock-mode query. The
    // transcript is in the verification document.
    if (cmd == QStringLiteral("get_lock_mode")) {
        bool locked = false;
        QMetaObject::invokeMethod(m_radio, "lock", Qt::DirectConnection,
                                  Q_RETURN_ARG(bool, locked), Q_ARG(int, 0));
        return QStringLiteral("%1\nRPRT 0\n").arg(locked ? 1 : 0);
    }
    if (cmd == QStringLiteral("set_lock_mode")) {
        if (parts.isEmpty()) {
            return kRprtRefused;
        }
        const bool on = parts.last().trimmed() != QStringLiteral("0");
        QMetaObject::invokeMethod(m_radio, "setLock", Qt::DirectConnection,
                                  Q_ARG(int, 0), Q_ARG(bool, on));
        return kRprtOk;
    }
    if (cmd == QStringLiteral("get_powerstat")) { return QStringLiteral("1\n"); }
    if (cmd == QStringLiteral("set_powerstat")) { return kRprtOk; }
    if (cmd == QStringLiteral("chk_vfo"))       { return QStringLiteral("0\n"); }
    if (cmd == QStringLiteral("dump_state"))    { return dumpState(); }
    if (cmd == QStringLiteral("q") || cmd == QStringLiteral("quit")) {
        return QString();   // null → disconnect
    }

    // Short forms, lower case = get, upper case = set, as in rigctl.
    if (cmd == QStringLiteral("f") || cmd == QStringLiteral("get_freq")) {
        return handleGetFreq();
    }
    if (cmd == QStringLiteral("F") || cmd == QStringLiteral("set_freq")) {
        return handleSetFreq(parts);
    }
    if (cmd == QStringLiteral("m") || cmd == QStringLiteral("get_mode")) {
        return handleGetMode();
    }
    if (cmd == QStringLiteral("M") || cmd == QStringLiteral("set_mode")) {
        return handleSetMode(parts);
    }
    if (cmd == QStringLiteral("v") || cmd == QStringLiteral("get_vfo")) {
        return handleGetVfo();
    }
    if (cmd == QStringLiteral("V") || cmd == QStringLiteral("set_vfo")) {
        return handleSetVfo(parts);
    }
    if (cmd == QStringLiteral("t") || cmd == QStringLiteral("get_ptt")) {
        return handleGetPtt();
    }
    if (cmd == QStringLiteral("T") || cmd == QStringLiteral("set_ptt")) {
        return handleSetPtt(parts);
    }
    if (cmd == QStringLiteral("s") || cmd == QStringLiteral("get_split_vfo")) {
        return handleGetSplit();
    }
    if (cmd == QStringLiteral("S") || cmd == QStringLiteral("set_split_vfo")) {
        return handleSetSplit(parts);
    }
    if (cmd == QStringLiteral("l") || cmd == QStringLiteral("get_level")) {
        return handleGetLevel(parts);
    }

    qCDebug(lcRigctld) << "RigctldServer: no handler for" << line;
    return kRprtNotImpl;
}

QString RigctldServer::handleGetFreq()
{
    qint64 hz = 0;
    QMetaObject::invokeMethod(m_radio, "vfoHz", Qt::DirectConnection,
                              Q_RETURN_ARG(qint64, hz),
                              Q_ARG(int, 0), Q_ARG(int, 0));
    return QStringLiteral("%1\n").arg(hz);
}

QString RigctldServer::handleSetFreq(const QStringList& args)
{
    if (args.isEmpty()) {
        return kRprtRefused;
    }
    // Clients send Hz, sometimes with a decimal point ("14250000.000000").
    bool ok = false;
    const double asDouble = args.last().toDouble(&ok);
    if (!ok) {
        return kRprtRefused;
    }
    const qint64 hz = qint64(asDouble);
    if (hz <= 0) {
        return kRprtRefused;
    }
    QMetaObject::invokeMethod(m_radio, "setVfoHz", Qt::DirectConnection,
                              Q_ARG(int, 0), Q_ARG(int, 0), Q_ARG(qint64, hz));

    // Read back and refuse when it did not take. Without a slice -- a
    // client connected before the radio -- the set goes nowhere, and
    // RPRT 0 would be a lie the client cannot see through: Hamlib
    // answers its own `f` from its frequency cache, so it would report
    // the frequency it asked for and never notice. Measured on
    // 2026-10-11: rigctl said 18100000 while the radio still held 0.
    qint64 applied = 0;
    QMetaObject::invokeMethod(m_radio, "vfoHz", Qt::DirectConnection,
                              Q_RETURN_ARG(qint64, applied),
                              Q_ARG(int, 0), Q_ARG(int, 0));
    return applied == hz ? kRprtOk : kRprtRefused;
}

QString RigctldServer::handleGetMode()
{
    QString mode;
    QMetaObject::invokeMethod(m_radio, "mode", Qt::DirectConnection,
                              Q_RETURN_ARG(QString, mode), Q_ARG(int, 0));
    int low = 0;
    int high = 0;
    QMetaObject::invokeMethod(m_radio, "filterLow", Qt::DirectConnection,
                              Q_RETURN_ARG(int, low), Q_ARG(int, 0));
    QMetaObject::invokeMethod(m_radio, "filterHigh", Qt::DirectConnection,
                              Q_RETURN_ARG(int, high), Q_ARG(int, 0));
    // Hamlib wants one passband width, we keep two edges.
    const int width = qMax(0, high - low);
    return QStringLiteral("%1\n%2\n")
        .arg(hamlibModeFromLongpath(mode))
        .arg(width);
}

QString RigctldServer::handleSetMode(const QStringList& args)
{
    if (args.isEmpty()) {
        return kRprtRefused;
    }
    const QString longpathMode = longpathModeFromHamlib(args.at(0));
    if (longpathMode.isEmpty()) {
        return kRprtRefused;
    }
    QMetaObject::invokeMethod(m_radio, "setMode", Qt::DirectConnection,
                              Q_ARG(int, 0), Q_ARG(QString, longpathMode));
    QString appliedMode;
    QMetaObject::invokeMethod(m_radio, "mode", Qt::DirectConnection,
                              Q_RETURN_ARG(QString, appliedMode), Q_ARG(int, 0));
    if (appliedMode != longpathMode) {
        return kRprtRefused;   // no slice to put the mode on
    }

    // A width of 0 means "keep the current passband" in rigctl.
    if (args.size() >= 2) {
        bool ok = false;
        const int width = args.at(1).toInt(&ok);
        if (ok && width > 0) {
            int low = 0;
            QMetaObject::invokeMethod(m_radio, "filterLow", Qt::DirectConnection,
                                      Q_RETURN_ARG(int, low), Q_ARG(int, 0));
            QMetaObject::invokeMethod(m_radio, "setFilterBand", Qt::DirectConnection,
                                      Q_ARG(int, 0), Q_ARG(int, low),
                                      Q_ARG(int, low + width));
        }
    }
    return kRprtOk;
}

QString RigctldServer::handleGetVfo()
{
    // Longpath has slices, not a VFO pair. Slice 0 is what a rigctld
    // client calls VFOA; the sub-receiver is slice 1 and reported as
    // VFOB by handleGetSplit's second field.
    return QStringLiteral("VFOA\n");
}

QString RigctldServer::handleSetVfo(const QStringList& args)
{
    if (args.isEmpty()) {
        return kRprtRefused;
    }
    const QString vfo = args.at(0).toUpper();
    // VFOA / Main / currVFO all mean slice 0, which is where we already
    // are. Anything else would mean steering a second slice that may
    // not exist, so it is refused rather than silently ignored.
    if (vfo == QStringLiteral("VFOA") || vfo == QStringLiteral("MAIN")
        || vfo == QStringLiteral("CURRVFO") || vfo == QStringLiteral("VFO")) {
        return kRprtOk;
    }
    return kRprtRefused;
}

QString RigctldServer::handleGetPtt()
{
    bool mox = false;
    QMetaObject::invokeMethod(m_radio, "mox", Qt::DirectConnection,
                              Q_RETURN_ARG(bool, mox));
    return QStringLiteral("%1\n").arg(mox ? 1 : 0);
}

QString RigctldServer::handleSetPtt(const QStringList& args)
{
    if (args.isEmpty()) {
        return kRprtRefused;
    }
    const bool wantTx = args.last().trimmed() != QStringLiteral("0");

    // Unkeying is always allowed: a client must be able to stop
    // transmitting even if the gate was closed in the meantime.
    if (wantTx && !m_pttAllowed) {
        qCWarning(lcRigctld) << "RigctldServer: refused PTT — "
                            "transmit over rigctld is turned off";
        emit pttRefused();
        return kRprtRefused;
    }
    QMetaObject::invokeMethod(m_radio, "setMox", Qt::DirectConnection,
                              Q_ARG(bool, wantTx));
    return kRprtOk;
}

QString RigctldServer::handleGetSplit()
{
    bool split = false;
    QMetaObject::invokeMethod(m_radio, "split", Qt::DirectConnection,
                              Q_RETURN_ARG(bool, split), Q_ARG(int, 0));
    // rigctl expects the split state and the TX VFO.
    return QStringLiteral("%1\nVFOB\n").arg(split ? 1 : 0);
}

// Longpath has no split to set. RadioModel::split() is a query-only
// shim: Phase 3F dropped the setter on purpose ("split is replaced with
// XIT for plus or minus 10 kHz tuning offset, or addSliceOnPan to create
// a second slice for full retune" — see RadioModel.h on split()), and
// the supported client configuration is WSJT-X / N1MM "Split Operation:
// None / Fake It". So the getter answers, and the setter says not
// implemented instead of pretending to have switched something.
QString RigctldServer::handleSetSplit(const QStringList& args)
{
    Q_UNUSED(args);
    qCInfo(lcRigctld) << "RigctldServer: set_split_vfo refused — Longpath has "
                         "no split; use XIT or a second slice (\"Fake It\" on "
                         "the client side)";
    return kRprtNotImpl;
}

QString RigctldServer::handleGetLevel(const QStringList& args)
{
    if (args.isEmpty()) {
        return kRprtNotImpl;
    }
    const QString level = args.at(0).toUpper();
    // KEYSPD is asked for on every connect (see the transcript). We have
    // no keyer yet (3M-2 is deferred), so 0 is the honest answer and the
    // client carries on.
    if (level == QStringLiteral("KEYSPD")) {
        return QStringLiteral("0\n");
    }
    // STRENGTH is dB relative to S9 in Hamlib, which is where the S
    // scale's reference sits at -73 dBm.
    if (level == QStringLiteral("STRENGTH")) {
        double dbm = -200.0;
        QMetaObject::invokeMethod(m_radio, "rxSignalDbm", Qt::DirectConnection,
                                  Q_RETURN_ARG(double, dbm), Q_ARG(int, 0));
        if (dbm <= -199.0) {
            // No WDSP channel yet: there is no reading to report, and a
            // made-up -127 would show up as a real (very weak) signal.
            return kRprtNotImpl;
        }
        return QStringLiteral("%1\n").arg(qRound(dbm + 73.0));
    }
    return kRprtNotImpl;
}

// ---------------------------------------------------------------------------
// Mode token mapping
//
// Longpath has modes Hamlib does not name (DSB, SPEC, DRM, RADE) and
// Hamlib has modes we do not have. Anything without a counterpart is
// refused on the way in and reported as the nearest sideband on the way
// out, so a client is never told a mode it cannot send back.
// ---------------------------------------------------------------------------

QString RigctldServer::hamlibModeFromLongpath(const QString& longpathMode)
{
    if (longpathMode == QStringLiteral("USB"))  { return QStringLiteral("USB"); }
    if (longpathMode == QStringLiteral("LSB"))  { return QStringLiteral("LSB"); }
    if (longpathMode == QStringLiteral("CWU"))  { return QStringLiteral("CW"); }
    if (longpathMode == QStringLiteral("CWL"))  { return QStringLiteral("CWR"); }
    if (longpathMode == QStringLiteral("AM"))   { return QStringLiteral("AM"); }
    if (longpathMode == QStringLiteral("SAM"))  { return QStringLiteral("AMS"); }
    if (longpathMode == QStringLiteral("FM"))   { return QStringLiteral("FM"); }
    if (longpathMode == QStringLiteral("DIGU")) { return QStringLiteral("PKTUSB"); }
    if (longpathMode == QStringLiteral("DIGL")) { return QStringLiteral("PKTLSB"); }
    // DSB, SPEC, DRM, RADE-U, RADE-L: no Hamlib token. Report the
    // sideband a client can act on rather than something it will reject.
    if (longpathMode == QStringLiteral("RADE-L")) { return QStringLiteral("LSB"); }
    return QStringLiteral("USB");
}

QString RigctldServer::longpathModeFromHamlib(const QString& hamlibMode)
{
    const QString m = hamlibMode.toUpper();
    if (m == QStringLiteral("USB"))    { return QStringLiteral("USB"); }
    if (m == QStringLiteral("LSB"))    { return QStringLiteral("LSB"); }
    if (m == QStringLiteral("CW"))     { return QStringLiteral("CWU"); }
    if (m == QStringLiteral("CWR"))    { return QStringLiteral("CWL"); }
    if (m == QStringLiteral("AM"))     { return QStringLiteral("AM"); }
    if (m == QStringLiteral("AMS"))    { return QStringLiteral("SAM"); }
    if (m == QStringLiteral("FM"))     { return QStringLiteral("FM"); }
    if (m == QStringLiteral("FMN"))    { return QStringLiteral("FM"); }
    if (m == QStringLiteral("PKTUSB")) { return QStringLiteral("DIGU"); }
    if (m == QStringLiteral("PKTLSB")) { return QStringLiteral("DIGL"); }
    if (m == QStringLiteral("RTTY"))   { return QStringLiteral("DIGL"); }
    if (m == QStringLiteral("RTTYR"))  { return QStringLiteral("DIGU"); }
    return QString();   // refused
}

// ---------------------------------------------------------------------------
// dump_state
//
// Shape follows the transcript of Hamlib 4.7.2's own answer (see the
// verification document). The values are ours. Field order matters to
// the client, so the layout is spelled out line by line rather than
// assembled cleverly.
// ---------------------------------------------------------------------------

QString RigctldServer::dumpState() const
{
    QStringList out;

    out << QStringLiteral("1");          // protocol version
    out << QStringLiteral("1");          // rig model (1 = "other", we are not a Hamlib model)
    out << QStringLiteral("0");          // ITU region, 0 = none claimed

    // RX range list: low, high, modes, low_power, high_power, vfo, ant.
    // -1/-1 for power says "receive only" in this list.
    //
    // The vfo field is a MASK, and Hamlib uses it to pick the range it
    // validates a set_mode / set_freq against: a mask that does not
    // contain the VFO the client is on means no range matches, and the
    // client then refuses the command LOCALLY -- it never reaches us.
    // Found exactly that way on 2026-10-11: `M CW 500` never appeared on
    // the wire while `F` did (see the verification document). The mask
    // below is the one Hamlib's own dummy rig publishes.
    out << QStringLiteral("%1 %2 0x%3 -1 -1 0x%4 0x1")
               .arg(kRxLowHz, 0, 'f', 6)
               .arg(kRxHighHz, 0, 'f', 6)
               .arg(kModeMask, 0, 16)
               .arg(kVfoMask, 0, 16);
    out << QStringLiteral("0 0 0 0 0 0 0");   // end of RX list

    // TX range list. Advertising the same span is an answer about the
    // hardware, not permission: BandPlanGuard decides what may be keyed.
    out << QStringLiteral("%1 %2 0x%3 1000 100000 0x%4 0x1")
               .arg(kRxLowHz, 0, 'f', 6)
               .arg(kRxHighHz, 0, 'f', 6)
               .arg(kModeMask, 0, 16)
               .arg(kVfoMask, 0, 16);
    out << QStringLiteral("0 0 0 0 0 0 0");   // end of TX list

    // Tuning steps: mode mask, step in Hz.
    out << QStringLiteral("0x%1 1").arg(kModeMask, 0, 16);
    out << QStringLiteral("0x%1 10").arg(kModeMask, 0, 16);
    out << QStringLiteral("0 0");             // end of step list

    // Filter list: mode mask, passband width. Our own defaults.
    out << QStringLiteral("0xc 2400");        // USB|LSB
    out << QStringLiteral("0xc 1800");
    out << QStringLiteral("0xc 3000");
    out << QStringLiteral("0x82 500");        // CW|CWR
    out << QStringLiteral("0x82 250");
    out << QStringLiteral("0x1 6000");        // AM
    out << QStringLiteral("0x20 15000");      // FM
    out << QStringLiteral("0 0");             // end of filter list

    out << QStringLiteral("9999");            // max_rit
    out << QStringLiteral("9999");            // max_xit
    out << QStringLiteral("0");               // max_ifshift
    out << QStringLiteral("0");               // announces
    out << QStringLiteral("0 ");              // preamp list (reported via CAT, not here)
    out << QStringLiteral("0 ");              // attenuator list (same)

    // Function and level bitmaps. We claim none of Hamlib's optional
    // functions: a client then asks only for what the commands above
    // actually serve, instead of being told we can do things we cannot.
    out << QStringLiteral("0x0");             // has_get_func
    out << QStringLiteral("0x0");             // has_set_func
    out << QStringLiteral("0x0");             // has_get_level
    out << QStringLiteral("0x0");             // has_set_level
    out << QStringLiteral("0x0");             // has_get_parm
    out << QStringLiteral("0x0");             // has_set_parm

    out << QStringLiteral("vfo_ops=0x0");
    out << QStringLiteral("ptt_type=0x1");    // RIG_PTT_RIG — PTT through this link
    // targetable_vfo is a MASK of what can be set without switching VFO
    // first: RIG_TARGETABLE_FREQ (0x1) and RIG_TARGETABLE_MODE (0x2).
    // With 0x1 alone a client sends `F` but silently refuses `M` on its
    // own side -- found at the wire on 2026-10-11, where `M CW 500` never
    // arrived while `F 14250000` did. Both are true for us: every command
    // addresses the one slice behind VFOA.
    out << QStringLiteral("targetable_vfo=0x3");
    out << QStringLiteral("has_set_vfo=1");
    out << QStringLiteral("has_get_vfo=1");
    out << QStringLiteral("has_set_freq=1");
    out << QStringLiteral("has_get_freq=1");
    out << QStringLiteral("has_set_conf=0");
    out << QStringLiteral("has_get_conf=0");
    out << QStringLiteral("has_power2mW=0");
    out << QStringLiteral("has_mW2power=0");
    out << QStringLiteral("has_get_ant=0");
    out << QStringLiteral("has_set_ant=0");
    out << QStringLiteral("timeout=0");
    out << QStringLiteral("rig_model=1");
    out << QStringLiteral("rigctld_version=Longpath rigctld bridge");
    out << QStringLiteral("done");

    return out.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

}  // namespace Longpath
