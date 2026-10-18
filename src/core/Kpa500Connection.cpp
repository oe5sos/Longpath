// no-port-check: Longpath-eigene Datei, kein Port. Siehe den Kopf von
// core/Kpa500Connection.h.
// =================================================================
// src/core/Kpa500Connection.cpp  (Longpath-eigen)
// =================================================================

#include "core/Kpa500Connection.h"

#include <QLoggingCategory>

#include <algorithm>

namespace Longpath {

Q_LOGGING_CATEGORY(lcKpa500, "longpath.kpa500")

Kpa500Connection::Kpa500Connection(QObject* parent)
    : QObject(parent)
{
    connect(&m_socket, &QTcpSocket::connected, this, &Kpa500Connection::onTransportUp);
    connect(&m_socket, &QTcpSocket::disconnected, this, &Kpa500Connection::onTransportDown);
    connect(&m_socket, &QTcpSocket::readyRead, this, &Kpa500Connection::onReadyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        onTransportError(m_socket.errorString());
    });

    m_parser.setReplyCallback([this](const Kpa500::Reply& r) { onReply(r); });

    m_reconnectTimer.setSingleShot(true);
    m_reconnectTimer.setInterval(5000);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (m_connected) { return; }
        if (m_mode == Mode::Network && !m_lastHost.isEmpty()) {
            connectNetwork(m_lastHost, m_lastPort);
#ifdef HAVE_SERIALPORT
        } else if (m_mode == Mode::Serial && !m_lastSerialPort.isEmpty()) {
            connectSerial(m_lastSerialPort, m_lastBaud);
#endif
        }
    });

    m_pollTimer.setInterval(m_pollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &Kpa500Connection::pollTick);
}

Kpa500Connection::~Kpa500Connection()
{
    // Dieselbe Falle wie bei SpeConnection: m_socket ist VOR den
    // Zeitgebern erklaert, wird also NACH ihnen abgebaut, und
    // ~QTcpSocket loest disconnected() aus -- das liefe in
    // onTransportDown() und griffe nach schon zerstoerten Zeitgebern.
    // Verbindungen trennen, bevor ein Mitglied abgebaut werden kann.
    m_socket.disconnect(this);
#ifdef HAVE_SERIALPORT
    if (m_serialPort) {
        m_serialPort->disconnect(this);
    }
#endif
    m_pollTimer.stop();
    m_reconnectTimer.stop();
}

void Kpa500Connection::setPollIntervalMs(int ms)
{
    m_pollIntervalMs = std::max(1, ms);
    m_pollTimer.setInterval(m_pollIntervalMs);
}

void Kpa500Connection::setSilentPollLimit(int polls)
{
    m_silentPollLimit = std::max(1, polls);
}

void Kpa500Connection::setReconnectIntervalMs(int ms)
{
    m_reconnectTimer.setInterval(std::max(1, ms));
}

QString Kpa500Connection::description() const
{
    if (m_mode == Mode::Network) {
        return QStringLiteral("%1:%2").arg(m_lastHost).arg(m_lastPort);
    }
#ifdef HAVE_SERIALPORT
    if (m_mode == Mode::Serial) {
        return QStringLiteral("%1 @ %2").arg(m_lastSerialPort).arg(m_lastBaud);
    }
#endif
    return QString();
}

QString Kpa500Connection::sourceLabel() const
{
    switch (m_mode) {
        case Mode::Network: return QStringLiteral("NETWORK");
        case Mode::Serial:  return QStringLiteral("SERIAL");
        default:            return QStringLiteral("—");
    }
}

#ifdef HAVE_SERIALPORT
void Kpa500Connection::connectSerial(const QString& portName, int baudRate)
{
    m_mode = Mode::Serial;
    m_lastSerialPort = portName;
    m_lastBaud = baudRate;
    m_deliberateDisconnect = false;
    m_reconnectTimer.stop();
    teardownDevice();
    m_parser.reset();

    if (!m_serialPort) {
        m_serialPort = new QSerialPort(this);
        connect(m_serialPort, &QSerialPort::readyRead, this, &Kpa500Connection::onReadyRead);
        connect(m_serialPort, &QSerialPort::errorOccurred, this,
                [this](QSerialPort::SerialPortError err) {
            if (err == QSerialPort::NoError) { return; }
            const QString msg = m_serialPort->errorString();
            qCWarning(lcKpa500) << "Kpa500Connection: serieller Fehler" << err << msg;
            onTransportError(msg);
            if (m_connected) { onTransportDown(); }
        });
    }

    m_serialPort->setPortName(portName);
    // 8N1, kein Handschlag. Die Rate kommt vom Betreiber -- an der
    // falschen kommt NICHTS durch, und sie laesst sich nicht erraten
    // (Elecraft Rev A2, §^BRP: vier Werte, Vorgabe nicht dokumentiert).
    m_serialPort->setBaudRate(baudRate);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        const QString err = m_serialPort->errorString();
        qCWarning(lcKpa500) << "Kpa500Connection: konnte" << portName
                            << "nicht oeffnen:" << err;
        emit connectionFailed(err);
        if (m_autoReconnect) { armReconnect(); }
        return;
    }
    m_device = m_serialPort;
    onTransportUp();
}
#endif

void Kpa500Connection::connectNetwork(const QString& host, quint16 port)
{
    m_mode = Mode::Network;
    m_lastHost = host;
    m_lastPort = port;
    m_deliberateDisconnect = false;
    m_reconnectTimer.stop();
    teardownDevice();
    m_parser.reset();

    m_device = &m_socket;
    qCDebug(lcKpa500) << "Kpa500Connection: verbinde zu" << host << ":" << port;
    m_socket.connectToHost(host, port);
}

void Kpa500Connection::disconnect()
{
    const bool stand = m_connected;
    m_deliberateDisconnect = true;
    m_reconnectTimer.stop();
    m_pollTimer.stop();
    m_connected = false;
    teardownDevice();
    m_parser.reset();
    if (stand) {
        qCDebug(lcKpa500) << "Kpa500Connection: getrennt";
        emit disconnected();
    }
    m_deliberateDisconnect = false;
}

void Kpa500Connection::teardownDevice()
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

void Kpa500Connection::onTransportUp()
{
    m_connected = true;
    m_replySeenSinceTick = false;
    m_silentPolls = 0;
    m_probingBoot = false;
    m_liveness = Liveness::Unknown;
    m_bandIndex = -1;
    m_firmware.clear();
    m_serial.clear();
    qCInfo(lcKpa500) << "Kpa500Connection: verbunden ueber" << description();

    // Die beiden Angaben, die sich nie aendern, einmal holen -- nicht in
    // den Takt. Dazu sofort ein erster Satz Messwerte, damit das Feld
    // nicht ein Intervall lang leer steht.
    sendRaw(Kpa500::buildGet(QString::fromLatin1(Kpa500::Verb::kFirmware)));
    sendRaw(Kpa500::buildGet(QString::fromLatin1(Kpa500::Verb::kSerial)));
    pollTick();
    m_pollTimer.start();

    emit connected();
}

void Kpa500Connection::onTransportDown()
{
    const bool stand = m_connected;
    m_connected = false;
    m_pollTimer.stop();
    m_parser.reset();
    if (stand) {
        qCDebug(lcKpa500) << "Kpa500Connection: getrennt";
        emit disconnected();
    }
    if (!m_deliberateDisconnect && m_autoReconnect) {
        armReconnect();
    }
    m_deliberateDisconnect = false;
}

void Kpa500Connection::onTransportError(const QString& errorString)
{
    qCWarning(lcKpa500) << "Kpa500Connection: Transportfehler" << errorString;
    emit connectionFailed(errorString);
    if (!m_deliberateDisconnect && m_autoReconnect && !m_connected) {
        armReconnect();
    }
}

void Kpa500Connection::armReconnect()
{
    if (!m_reconnectTimer.isActive()) {
        m_reconnectTimer.start();
    }
}

void Kpa500Connection::onReadyRead()
{
    if (!m_device) { return; }
    m_parser.feed(m_device->readAll());
}

void Kpa500Connection::setLiveness(Liveness s)
{
    if (m_liveness == s) { return; }
    m_liveness = s;
    switch (s) {
        case Liveness::Running:
            qCInfo(lcKpa500) << "Kpa500Connection: Verstaerker antwortet";
            break;
        case Liveness::BootMode:
            // Das ist die Auskunft, die der SPE nicht geben kann.
            qCInfo(lcKpa500) << "Kpa500Connection: Verstaerker im Boot-Zustand "
                                "-- aus, aber am Draht erreichbar. Einschalten "
                                "mit 'P' moeglich.";
            break;
        case Liveness::Silent:
            qCWarning(lcKpa500) << "Kpa500Connection: keine Antwort, auch nicht "
                                   "auf 'I' -- falscher Anschluss, falsche "
                                   "Datenrate, oder niemand da.";
            break;
        default:
            break;
    }
    emit livenessChanged(s);
}

void Kpa500Connection::pollTick()
{
    if (!m_replySeenSinceTick) {
        ++m_silentPolls;
        if (m_silentPolls == m_silentPollLimit) {
            // Jetzt wird unterschieden statt bloss "still" gemeldet:
            // ein 'I' beantwortet der Boot-Zustand, die laufende
            // Hauptsoftware nicht (und ignoriert es folgenlos --
            // einzelne Buchstaben sind dort kein Befehl).
            m_probingBoot = true;
            setLiveness(Liveness::Silent);
        }
    } else {
        m_silentPolls = 0;
    }
    m_replySeenSinceTick = false;

    if (m_probingBoot) {
        // ── EIN ECHTER FEHLER, vom Pruefstand gefunden ───────────────
        //
        // Der erste Anlauf schickte hier NUR das 'I' und kehrte zurueck.
        // Das ist eine Falle ohne Ausweg: ein laufender KPA500 antwortet
        // auf 'I' NICHT (einzelne Buchstaben gelten nur im
        // Boot-Zustand), also kam nach einem Einschalten nie wieder eine
        // Antwort -- der Zustand blieb fuer immer auf "still", obwohl
        // der Verstaerker lief. Gefunden von `ausUndWiederEin`, nicht
        // vom Nachdenken.
        //
        // Jetzt wird ABGEWECHSELT: ein Takt horcht mit 'I' nach, der
        // naechste fragt regulaer. Jede Betriebsart beantwortet das
        // ihre und laesst das andere folgenlos liegen (Elecraft Rev A2,
        // §Command Format: „Commands with an incorrect format or an
        // out-of-range parameter are ignored."). Abgewechselt statt
        // beides im selben Takt, damit die beiden Formen nicht
        // aneinanderkleben und sich gegenseitig unlesbar machen.
        // Schlimmster Fall bis zur Erkennung: zwei Takte.
        m_bootProbeToggle = !m_bootProbeToggle;
        if (m_bootProbeToggle) {
            sendRaw(Kpa500::buildBootIdentify());
            return;
        }
        // kein return -- unten gehen die regulaeren Abfragen hinaus
    }

    sendRaw(Kpa500::buildGet(QString::fromLatin1(Kpa500::Verb::kPowerSwr)));
    sendRaw(Kpa500::buildGet(QString::fromLatin1(Kpa500::Verb::kVoltsAmps)));
    sendRaw(Kpa500::buildGet(QString::fromLatin1(Kpa500::Verb::kTemperature)));
    sendRaw(Kpa500::buildGet(QString::fromLatin1(Kpa500::Verb::kOperate)));
    sendRaw(Kpa500::buildGet(QString::fromLatin1(Kpa500::Verb::kBand)));
    sendRaw(Kpa500::buildGet(QString::fromLatin1(Kpa500::Verb::kFault)));
}

void Kpa500Connection::onReply(const Kpa500::Reply& r)
{
    m_replySeenSinceTick = true;
    m_silentPolls = 0;

    if (r.isBootIdentify) {
        m_probingBoot = true;      // weiter nur nachhorchen
        setLiveness(Liveness::BootMode);
        return;
    }
    if (r.isBareSemicolon) {
        return;                    // Antwort auf die Probe, kein Messwert
    }

    // Irgendeine regulaere Antwort heisst: die Hauptsoftware laeuft.
    // Damit endet auch das Nachhorchen.
    m_probingBoot = false;
    setLiveness(Liveness::Running);

    bool neueWerte = false;
    if (r.verb == QLatin1String(Kpa500::Verb::kPowerSwr)) {
        if (const auto p = Kpa500::parsePowerSwr(r.data)) {
            m_powerSwr = *p;
            neueWerte = true;
        }
    } else if (r.verb == QLatin1String(Kpa500::Verb::kVoltsAmps)) {
        if (const auto v = Kpa500::parseVoltsAmps(r.data)) {
            m_voltsAmps = *v;
            neueWerte = true;
        }
    } else if (r.verb == QLatin1String(Kpa500::Verb::kTemperature)) {
        if (const auto t = Kpa500::parseTemperature(r.data)) {
            m_tempC = *t;
            neueWerte = true;
        }
    } else if (r.verb == QLatin1String(Kpa500::Verb::kOperate)) {
        if (const auto o = Kpa500::parseOperate(r.data)) {
            if (*o != m_operate) {
                m_operate = *o;
                emit operateChanged(m_operate);
            }
            neueWerte = true;
        }
    } else if (r.verb == QLatin1String(Kpa500::Verb::kBand)) {
        if (const auto b = Kpa500::parseBand(r.data)) {
            m_bandIndex = *b;
            neueWerte = true;
        }
    } else if (r.verb == QLatin1String(Kpa500::Verb::kFault)) {
        if (const auto f = Kpa500::parseFault(r.data)) {
            if (f->code != m_faultCode) {
                m_faultCode = f->code;
                emit faultChanged(m_faultCode);
            }
            neueWerte = true;
        }
    } else if (r.verb == QLatin1String(Kpa500::Verb::kFirmware)) {
        m_firmware = r.data.trimmed();
    } else if (r.verb == QLatin1String(Kpa500::Verb::kSerial)) {
        m_serial = r.data.trimmed();
    } else if (r.verb == QLatin1String(Kpa500::Verb::kPowerState)) {
        // ^ONn; -- kommt nur, wenn danach gefragt wurde oder als
        // Nachhall eines ^ON0;. Keine eigene Anzeige: dass er laeuft,
        // sagt bereits jede andere Antwort.
    } else {
        qCDebug(lcKpa500) << "Kpa500Connection: unbeachtete Antwort"
                          << r.verb << r.data;
    }

    if (neueWerte) {
        emit telemetryUpdated();
    }
}

void Kpa500Connection::setOperate(bool operate)
{
    sendRaw(Kpa500::buildOperateSet(operate));
}

void Kpa500Connection::clearFault()
{
    sendRaw(Kpa500::buildFaultClear());
}

void Kpa500Connection::powerOff()
{
    sendRaw(Kpa500::buildPowerOff());
    // Danach antwortet er auf `^XX;` nicht mehr -- das WISSEN wir hier,
    // also wird gleich nachgehorcht statt erst die Stilleerkennung
    // ablaufen zu lassen. Das spart dem Betreiber drei Sekunden
    // „still", in denen der Einschaltknopf grundlos grau waere.
    m_probingBoot = true;
    m_bootProbeToggle = false;
}

void Kpa500Connection::powerOn()
{
    // Elecraft Rev A2, §BootLoader 'P': „This is the command to use to
    // remotely power up the KPA500. No response is sent from the KPA500
    // after receiving this command." -- also keine Quittung erwarten.
    // Dass es geklappt hat, zeigt die naechste regulaere Antwort.
    sendRaw(Kpa500::buildBootPowerOn());
}

void Kpa500Connection::setBand(int bandIndex)
{
    const QByteArray b = Kpa500::buildBandSet(bandIndex);
    if (b.isEmpty()) {
        qCWarning(lcKpa500) << "Kpa500Connection: Band" << bandIndex
                            << "gibt es beim KPA500 nicht -- nichts gesendet";
        return;
    }
    sendRaw(b);
}

void Kpa500Connection::sendRaw(const QByteArray& bytes)
{
    if (!m_device || !m_connected) { return; }
    m_device->write(bytes);
}

}  // namespace Longpath
