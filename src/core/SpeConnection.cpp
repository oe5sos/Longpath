// =================================================================
// src/core/SpeConnection.cpp  (Longpath)
// =================================================================
// Ported from AetherSDR src/core/SpeConnection.cpp at d58e2b8a -- see the
// full attribution block and the list of deviations at the head of
// core/SpeConnection.h.
// =================================================================

#include "core/SpeConnection.h"

#include <QLoggingCategory>

#include <algorithm>
#include <cmath>

namespace Longpath {

// AetherSDR schreibt in lcTuner. Die Kategorie gibt es hier nicht; nach
// dem Muster von PgxlConnection.cpp:25 eine eigene.
Q_LOGGING_CATEGORY(lcSpe, "longpath.spe")

SpeConnection::SpeConnection(QObject* parent)
    : QObject(parent)
{
    connect(&m_socket, &QTcpSocket::connected, this, &SpeConnection::onTransportUp);
    connect(&m_socket, &QTcpSocket::disconnected, this, &SpeConnection::onTransportDown);
    connect(&m_socket, &QTcpSocket::readyRead, this, &SpeConnection::onReadyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        onTransportError(m_socket.errorString());
    });

    m_parser.setFrameCallback([this](const Spe::Frame& f) { onFrameReceived(f); });

    // Retries every 5s indefinitely until the amp returns or the user
    // disconnects — same cadence as the other peripheral connections
    // (Pgxl/Tgxl/RF2K-S) for a device that may be power-cycling or unplugged.
    m_reconnectTimer.setSingleShot(true);
    m_reconnectTimer.setInterval(5000);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (m_connected) { return; }
        if (m_mode == Mode::Network && !m_lastHost.isEmpty()) {
            connectNetwork(m_lastHost, m_lastPort);
#ifdef HAVE_SERIALPORT
        } else if (m_mode == Mode::Serial && !m_lastSerialPort.isEmpty()) {
            connectSerial(m_lastSerialPort);
#endif
        }
    });

    m_pollTimer.setInterval(m_pollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &SpeConnection::pollTick);

    m_powerOnTimer.setSingleShot(true);
    connect(&m_powerOnTimer, &QTimer::timeout, this, &SpeConnection::powerOnStep);
}

SpeConnection::~SpeConnection()
{
    // ── Ein echter Fehler, hier gefunden, nicht aus dem Vorbild ──────
    //
    // AetherSDRs SpeConnection hat keinen Destruktor. Beim Abbau werden
    // die Mitglieder in UMGEKEHRTER Erklaerungsreihenfolge zerstoert:
    // zuerst m_powerOnTimer / m_pollTimer / m_reconnectTimer (Zeilen
    // 226..239 im Kopf), DANACH m_socket (Zeile 209). Der Destruktor
    // von QTcpSocket ruft disconnectFromHost(), das loest disconnected()
    // aus, und das haengt noch an onTransportDown() -- die QObject-Basis
    // dieses Objekts lebt zu dem Zeitpunkt ja noch. onTransportDown()
    // greift dann nach m_pollTimer.stop() und ueber armReconnect() nach
    // m_reconnectTimer.start(), und beide sind schon abgebaut.
    //
    // Sichtbar wurde das als Warnung, die nach einem Zeitgeber in einem
    // fremden Faden klingt und mit Faeden nichts zu tun hat -- die
    // Faden-Pruefung in QObject::startTimer schlaegt an, weil die
    // Fadendaten des zerstoerten QTimer schon weg sind:
    //
    //   QObject::startTimer: Timers cannot be started from another thread
    //     QTimer::start <- SpeConnection::onTransportDown <- doActivate
    //     <- QAbstractSocket::disconnectFromHost
    //     <- QAbstractSocket::~QAbstractSocket <- ~SpeConnection
    //
    // Es ist nicht abgestuerzt, also faellt es ohne Hinsehen nie auf. Es
    // IST aber ein Zugriff auf abgebaute Mitglieder, und onTransportDown()
    // meldet dabei ausserdem disconnected() aus einem halb abgebauten
    // Objekt heraus -- jeder angeschlossene Empfaenger laeuft dann gegen
    // ein sterbendes Objekt.
    //
    // Behoben an der Ursache statt mit einer Abbau-Fahne und Waechtern in
    // jedem Zweig: die Verbindungen werden getrennt, BEVOR irgendein
    // Mitglied abgebaut werden kann. Danach kann kein Mitglieds-Destruktor
    // mehr hierher zurueckrufen. Der Pruefstand
    // `keinRueckrufAusDemAbbau` nagelt es fest.
    m_socket.disconnect(this);
#ifdef HAVE_SERIALPORT
    if (m_serialPort) {
        m_serialPort->disconnect(this);
    }
#endif
    m_pollTimer.stop();
    m_reconnectTimer.stop();
    m_powerOnTimer.stop();
}

void SpeConnection::setPollIntervalMs(int ms)
{
    m_pollIntervalMs = std::max(1, ms);
    m_pollTimer.setInterval(m_pollIntervalMs);
}

void SpeConnection::setReconnectIntervalMs(int ms)
{
    m_reconnectTimer.setInterval(std::max(1, ms));
}

void SpeConnection::setSilentPollLimit(int polls)
{
    m_silentPollLimit = std::max(1, polls);
}

void SpeConnection::setPowerOnPulseScale(double scale)
{
    m_powerOnPulseScale = (scale > 0.0) ? scale : 1.0;
}

QString SpeConnection::description() const
{
    if (m_mode == Mode::Network) {
        return QStringLiteral("%1:%2").arg(m_lastHost).arg(m_lastPort);
    }
#ifdef HAVE_SERIALPORT
    if (m_mode == Mode::Serial) {
        return m_lastSerialPort;
    }
#endif
    return QString();
}

QString SpeConnection::sourceLabel() const
{
    switch (m_mode) {
        case Mode::Network: return QStringLiteral("NETWORK");
        case Mode::Serial:  return QStringLiteral("SERIAL");
        default:            return QStringLiteral("—");
    }
}

#ifdef HAVE_SERIALPORT
void SpeConnection::connectSerial(const QString& portName)
{
    m_mode = Mode::Serial;
    m_lastSerialPort = portName;
    m_deliberateDisconnect = false;
    m_reconnectTimer.stop();
    teardownDevice();
    m_parser.reset();

    if (!m_serialPort) {
        m_serialPort = new QSerialPort(this);
        connect(m_serialPort, &QSerialPort::readyRead, this, &SpeConnection::onReadyRead);
        connect(m_serialPort, &QSerialPort::errorOccurred, this,
                [this](QSerialPort::SerialPortError err) {
            if (err == QSerialPort::NoError) { return; }
            const QString msg = m_serialPort->errorString();
            qCWarning(lcSpe) << "SpeConnection: serial error" << err << msg;
            onTransportError(msg);
            if (m_connected) { onTransportDown(); }
        });
    }

    m_serialPort->setPortName(portName);
    // 115200 8N1 no handshake — the spec's documented maximum; the amp
    // auto-adapts to lower speeds so there's nothing to configure.
    m_serialPort->setBaudRate(QSerialPort::Baud115200);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        const QString err = m_serialPort->errorString();
        qCWarning(lcSpe) << "SpeConnection: failed to open" << portName << err;
        emit connectionFailed(err);
        if (m_autoReconnect) { armReconnect(); }
        return;
    }
    // Idle line state: DTR HIGH, RTS low — matching the power-ON pulse's
    // terminal step, so the resting state no longer depends on whether ON
    // was pressed this session and a reconnect produces no edge on either
    // line. AetherSDR bench-ruled this on a real 1.5K-FA: with DTR held
    // high the amplifier raises no `R` ("Power switch held by remote")
    // warning, keystrokes including SWITCH OFF work normally, and repeated
    // power cycles behave — the power switch rides the RTS pulse alone,
    // which is why RTS (and only RTS) must stay low at rest. Carried over
    // as stated fact; Longpath has no SPE to re-measure it against.
    m_serialPort->setDataTerminalReady(true);
    m_serialPort->setRequestToSend(false);

    m_device = m_serialPort;
    onTransportUp();
}
#endif

void SpeConnection::connectNetwork(const QString& host, quint16 port)
{
    m_mode = Mode::Network;
    m_lastHost = host;
    m_lastPort = port;
    m_deliberateDisconnect = false;
    m_reconnectTimer.stop();
    teardownDevice();
    m_parser.reset();

    m_device = &m_socket;
    qCDebug(lcSpe) << "SpeConnection: connecting to" << host << ":" << port;
    m_socket.connectToHost(host, port);
    // onTransportUp() fires from the connected() signal (async).
}

void SpeConnection::disconnect()
{
    // Self-contained rather than relying on teardownDevice() to indirectly
    // trigger onTransportDown() — QTcpSocket::abort() emits disconnected()
    // synchronously but QSerialPort::close() emits nothing, and that
    // asymmetry left AetherSDR's ACOM applet stuck at "Connected" on a
    // user-initiated serial disconnect until it was made self-contained.
    const bool wasConnected = m_connected;
    m_deliberateDisconnect = true;
    m_reconnectTimer.stop();
    m_pollTimer.stop();
    m_powerOnTimer.stop();
    m_powerOnStep = -1;
    m_connected = false;
    teardownDevice();
    m_parser.reset();
    if (wasConnected) {
        qCDebug(lcSpe) << "SpeConnection: disconnected";
        emit disconnected();
    }
    m_deliberateDisconnect = false;
}

void SpeConnection::teardownDevice()
{
    if (m_device == &m_socket && m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.abort();
    }
#ifdef HAVE_SERIALPORT
    if (m_serialPort && m_device == m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->close();
    }
#endif
    m_device = nullptr;
}

void SpeConnection::onTransportUp()
{
    m_connected = true;
    m_statusSeenSinceTick = false;
    m_silentPolls = 0;
    m_responding = false;
    m_currentModelId.clear();
    // Renegotiated per connection — a proxy reconfigured between sessions
    // must not be judged on the previous session's answer (nor on the
    // previous session's carried scan tail).
    m_comPortOption = Spe::Rfc2217::OptionReply::None;
    m_rfc2217Tail.clear();
    qCInfo(lcSpe) << "SpeConnection: connected via" << description();

    // First poll immediately — the timer only fires after a full interval,
    // and the applet shouldn't sit blank for it.
    sendRaw(Spe::buildStatusRequest());
    m_pollTimer.start();

    emit connected();
}

void SpeConnection::onTransportDown()
{
    const bool wasConnected = m_connected;
    m_connected = false;
    m_pollTimer.stop();
    m_powerOnTimer.stop();
    m_powerOnStep = -1;
    m_parser.reset();
    if (wasConnected) {
        qCDebug(lcSpe) << "SpeConnection: disconnected";
        emit disconnected();
    }
    if (!m_deliberateDisconnect && m_autoReconnect) {
        armReconnect();
    }
    m_deliberateDisconnect = false;
}

void SpeConnection::onTransportError(const QString& errorString)
{
    qCWarning(lcSpe) << "SpeConnection: transport error" << errorString;
    emit connectionFailed(errorString);
    if (!m_deliberateDisconnect && m_autoReconnect && !m_connected) {
        armReconnect();
    }
}

void SpeConnection::armReconnect()
{
    if (!m_reconnectTimer.isActive()) {
        m_reconnectTimer.start();
    }
}

void SpeConnection::onReadyRead()
{
    if (!m_device) { return; }
    const QByteArray chunk = m_device->readAll();

    // Watch for the proxy's answer to our WILL COM-PORT-OPTION before the
    // bytes go to the frame parser. Read-only — the parser resyncs past
    // negotiation on its own, so nothing is consumed here; this only records
    // whether RFC 2217 control is actually available, which powerOn() needs
    // to know before it claims the pulse reached the amplifier.
    //
    // Scanned over the previous read's 2-byte tail + this chunk: the 3-byte
    // DO/DONT sequence can straddle a TCP segment boundary, and a stateless
    // per-chunk scan would miss it — reporting "never confirmed" against a
    // correctly configured proxy. Only the tail is carried, never re-scanning
    // whole chunks, so a reply can't be double-counted either.
    if (m_mode == Mode::Network) {
        const auto reply = Spe::Rfc2217::scanComPortOptionReply(m_rfc2217Tail + chunk);
        m_rfc2217Tail = chunk.right(2);
        if (reply != Spe::Rfc2217::OptionReply::None && reply != m_comPortOption) {
            m_comPortOption = reply;
            if (reply == Spe::Rfc2217::OptionReply::Accepted) {
                qCInfo(lcSpe) << "SpeConnection: proxy accepted RFC 2217"
                                 " COM-port control — remote power-ON is"
                                 " available.";
            } else {
                qCWarning(lcSpe) << "SpeConnection: proxy REFUSED RFC 2217"
                                    " COM-port control. Monitoring and"
                                    " keystrokes still work, but remote"
                                    " power-ON needs the ser2net port as"
                                    " `accepter: telnet(rfc2217=true),<port>`.";
            }
        }
    }

    m_parser.feed(chunk);
}

void SpeConnection::pollTick()
{
    // Silence detection first: with ser2net the TCP link happily outlives
    // the amplifier being switched off, so unanswered polls — not a socket
    // drop — are the only "amp went away" evidence that topology produces.
    if (!m_statusSeenSinceTick) {
        if (++m_silentPolls == m_silentPollLimit && m_responding) {
            qCWarning(lcSpe) << "SpeConnection: amplifier stopped answering status"
                                " polls (link is up) — switched off, or not an SPE"
                                " on this port? Polling continues.";
            m_responding = false;
            emit respondingChanged(false);
        }
    } else {
        m_silentPolls = 0;
    }
    m_statusSeenSinceTick = false;

    sendRaw(Spe::buildStatusRequest());
}

void SpeConnection::onFrameReceived(const Spe::Frame& f)
{
    if (f.isAck()) {
        // Keystroke echo — logged for command traceability, nothing to
        // update: the next status poll reflects any resulting state change
        // within one poll interval.
        qCDebug(lcSpe) << "SpeConnection: ACK for command"
                       << QString::number(static_cast<quint8>(f.data.at(0)), 16);
        return;
    }

    const auto status = Spe::parseStatus(f.data);
    if (!status) {
        qCWarning(lcSpe) << "SpeConnection: unparseable status reply ("
                         << f.data.size() << "B) —" << f.data.left(24);
        return;
    }

    m_statusSeenSinceTick = true;
    m_silentPolls = 0;
    if (!m_responding) {
        m_responding = true;
        emit respondingChanged(true);
    }

    m_lastStatus = *status;
    if (status->id != m_currentModelId) {
        m_currentModelId = status->id;
        qCInfo(lcSpe) << "SpeConnection: amplifier identifies as" << m_currentModelId
                      << "(" << Spe::modelSpec(m_currentModelId).displayName << ")";
        emit modelChanged(m_currentModelId);
    }
    emit statusUpdated(*status);
}

void SpeConnection::sendKey(Spe::Key key)
{
    sendRaw(Spe::buildKeyCommand(key));
}

void SpeConnection::setControlLines(bool dtr, bool rts)
{
    if (m_mode == Mode::Network) {
        // The proxy's serial lines, driven remotely via RFC 2217.
        sendRaw(Spe::Rfc2217::buildSetControl(
            dtr ? Spe::Rfc2217::kDtrOn : Spe::Rfc2217::kDtrOff));
        sendRaw(Spe::Rfc2217::buildSetControl(
            rts ? Spe::Rfc2217::kRtsOn : Spe::Rfc2217::kRtsOff));
#ifdef HAVE_SERIALPORT
    } else if (m_mode == Mode::Serial && m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->setDataTerminalReady(dtr);
        m_serialPort->setRequestToSend(rts);
#endif
    }
}

void SpeConnection::startPowerOnTimer(int baseMs)
{
    // Longpath-Zusatz: dieselbe Reihenfolge, nur gestaucht, damit der
    // Pruefstand sie ohne 1,6 s Wartezeit sehen kann. Mindestens 1 ms,
    // sonst feuert der Zeitgeber je nach Plattform gar nicht.
    const int ms = static_cast<int>(std::lround(baseMs * m_powerOnPulseScale));
    m_powerOnTimer.start(std::max(1, ms));
}

void SpeConnection::powerOn()
{
    if (!m_connected || m_powerOnStep >= 0) { return; }
    qCInfo(lcSpe) << "SpeConnection: sending power-ON pulse via" << sourceLabel();
    m_powerOnStep = 0;
    if (m_mode == Mode::Network) {
        // Ask the proxy to interpret RFC 2217 frames, then give it a moment
        // — AetherSDR's reference application's own working pacing.
        sendRaw(Spe::Rfc2217::buildWillComPortOption());
        startPowerOnTimer(500);
    } else {
        powerOnStep();  // local serial lines need no negotiation
    }
}

void SpeConnection::powerOnStep()
{
    // Pulse sequence carried from AetherSDR's reference application:
    // DTR on (100 ms), then DTR off + RTS on (1000 ms — the actual power
    // pulse), then DTR on + RTS off to idle.
    switch (m_powerOnStep) {
        case 0:
            setControlLines(true, false);
            m_powerOnStep = 1;
            startPowerOnTimer(100);
            break;
        case 1:
            setControlLines(false, true);
            m_powerOnStep = 2;
            startPowerOnTimer(1000);
            break;
        case 2:
            setControlLines(true, false);
            m_powerOnStep = -1;
            // Report what was actually confirmed rather than assuming. The
            // pulse is always sent: a proxy that ignores COM-port control
            // simply discards the SET-CONTROL frames (or, in raw mode,
            // forwards them to the amp, which rejects them as unframed
            // noise), so sending is harmless — claiming it landed is not.
            if (m_mode == Mode::Network
                && m_comPortOption == Spe::Rfc2217::OptionReply::Refused) {
                // "May", not "did not": AetherSDR measured ser2net 4.3.11
                // with a plain `accepter: telnet` port answering DONT yet
                // still EXECUTING SET-CONTROL, on a real 1.5K-FA. The
                // explicit rfc2217=true config remains the recommendation
                // because that behaviour is unspecified.
                qCWarning(lcSpe) << "SpeConnection: power-ON pulse sent, but"
                                    " the proxy REFUSED RFC 2217 COM-port"
                                    " control, so it may not have reached"
                                    " the amplifier (some ser2net builds"
                                    " act on it anyway — watch whether"
                                    " status polls resume). Recommended"
                                    " config: `accepter:"
                                    " telnet(rfc2217=true),<port>`.";
            } else if (m_mode == Mode::Network
                       && m_comPortOption != Spe::Rfc2217::OptionReply::Accepted) {
                qCWarning(lcSpe) << "SpeConnection: power-ON pulse sent, but"
                                    " the proxy never confirmed RFC 2217"
                                    " COM-port control — if the amplifier"
                                    " stays silent, check that ser2net runs"
                                    " this port as `accepter:"
                                    " telnet(rfc2217=true),<port>` rather"
                                    " than raw.";
            } else {
                qCInfo(lcSpe) << "SpeConnection: power-ON pulse complete — the"
                                 " amp should begin answering status polls"
                                 " shortly.";
            }
            // Am seriellen Anschluss wird nichts ausgehandelt: die
            // Leitungen sind direkt gestellt worden, und mehr
            // Bestaetigung gibt es dort nicht. Darum Accepted statt
            // None -- None hiesse "nie bestaetigt" und waere falsch.
            emit powerOnPulseFinished(m_mode == Mode::Network
                                          ? m_comPortOption
                                          : Spe::Rfc2217::OptionReply::Accepted);
            break;
        default:
            m_powerOnStep = -1;
            break;
    }
}

void SpeConnection::sendRaw(const QByteArray& packet)
{
    if (!m_device || !m_connected) { return; }
    m_device->write(packet);
}

}  // namespace Longpath
