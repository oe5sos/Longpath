// =================================================================
// src/core/Gs232Rotor.cpp  (Longpath)
// =================================================================
// Siehe Gs232Rotor.h für Herkunft und Protokoll.
// =================================================================

#include "Gs232Rotor.h"

#include <QIODevice>
#include <QTcpSocket>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QTimer>

#ifdef HAVE_SERIALPORT
#include <QSerialPort>
#endif

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

Q_LOGGING_CATEGORY(lcRotor, "longpath.rotor")

// Eine Zeile ohne CR/LF, die laenger ist als das, ist Rauschen und
// keine Antwort — dann wird der Puffer verworfen, statt ihn wachsen zu
// lassen. Gleiche Grenze wie upstream (RotorConnection.cpp
// kMaxLineBytes).
constexpr int kMaxLineBytes = 256;

double wrap360(double deg)
{
    double w = std::fmod(deg, 360.0);
    if (w < 0.0) { w += 360.0; }
    return w;
}

} // namespace

Gs232Rotor::Gs232Rotor(QObject* parent)
    : RotorController(parent)
{
    m_poll = new QTimer(this);
    m_poll->setInterval(m_pollMs);
    connect(m_poll, &QTimer::timeout, this, &Gs232Rotor::poll);

    // Neu verbinden (Netz, 2026-11-11) -- siehe Gs232Rotor.h.
    m_neu = new QTimer(this);
    m_neu->setSingleShot(true);
    connect(m_neu, &QTimer::timeout, this, [this]() {
        if (m_soll && m_state == State::Disconnected && !m_host.isEmpty()) { connectToRotor(); }
    });
    // Ein Verbindungsaufbau zu einem stummen Ziel haengt sonst so lange,
    // wie das Betriebssystem SYN wiederholt (auf dem Mac ueber eine Minute).
    m_frist = new QTimer(this);
    m_frist->setSingleShot(true);
    m_frist->setInterval(kAnschlussFristMs);
    connect(m_frist, &QTimer::timeout, this, [this]() {
        if (m_state != State::Connecting) { return; }
        netzVerloren(m_dev, QStringLiteral("%1:%2 did not answer the connection")
                                .arg(m_host).arg(m_netPort));
    });
}

void Gs232Rotor::setWiederholungMs(int ms)
{
    m_ersteWartenMs = std::max(10, ms);
    m_wartenMs = m_ersteWartenMs;
}

void Gs232Rotor::setAnschlussFristMs(int ms)
{
    m_frist->setInterval(std::max(10, ms));
}

void Gs232Rotor::netzVerloren(QIODevice* dev, const QString& grund)
{
    // Doppelt gemeldet (errorOccurred, dann disconnected) oder eine alte
    // Verbindung, die schon ersetzt ist: nichts zu tun.
    if (!dev || dev != m_dev) { return; }
    m_poll->stop();
    m_frist->stop();
    m_dev = nullptr;
    dev->disconnect(this);
    if (auto* sock = qobject_cast<QAbstractSocket*>(dev)) { sock->abort(); }
    dev->deleteLater();
    m_buffer.clear();
    m_lastReading = {};
    ++m_fehlversuche;
    // Nur der erste Fehlschlag einer Reihe geht an die Oberflaeche und
    // ins Protokoll; ein ausgeschalteter ARCO soll nicht stuendlich
    // hunderte Zeilen schreiben.
    if (m_fehlversuche == 1) {
        qCWarning(lcRotor) << "GS-232:" << grund;
        emit errorOccurred(grund);
    } else {
        qCDebug(lcRotor) << "GS-232:" << grund << "-- Versuch" << m_fehlversuche;
    }
    setState(State::Disconnected);
    if (m_soll && !m_host.isEmpty()) {
        qCDebug(lcRotor) << "GS-232: neuer Versuch in" << m_wartenMs << "ms";
        m_neu->start(m_wartenMs);
        m_wartenMs = std::min(m_wartenMs * 2, kWiederholungMaxMs);
    }
}

Gs232Rotor::~Gs232Rotor()
{
    // Die Schnittstelle gehoert uns nur, wenn wir sie selbst geoeffnet
    // haben; eine eingehaengte Pruefstandsnaht wird nicht geschlossen.
    // Erst die Signale weg: close() meldet sonst disconnected(), und
    // netzVerloren() sendete mitten im Abbau an Empfaenger, die es
    // vielleicht schon nicht mehr gibt (2026-11-11).
    m_soll = false;
    if (m_dev && m_ownsDev) {
        m_dev->disconnect(this);
        m_dev->close();
    }
}

void Gs232Rotor::setPort(const QString& portName, int baudRate)
{
    m_portName = portName;
    m_baud     = baudRate > 0 ? baudRate : 9600;
}

bool Gs232Rotor::parseNetworkTarget(const QString& text, QString* host,
                                    quint16* port)
{
    // Ein Name oder eine IPv4-Adresse, dann Doppelpunkt und Port. Kein
    // Schraegstrich (Geraetedatei), kein Leerzeichen; „COM3" hat keinen
    // Doppelpunkt und faellt so von selbst heraus.
    static const QRegularExpression re(
        QStringLiteral("^([A-Za-z0-9][A-Za-z0-9.-]*):([0-9]{1,5})$"));
    const QRegularExpressionMatch m = re.match(text.trimmed());
    if (!m.hasMatch()) { return false; }
    const uint p = m.captured(2).toUInt();
    if (p == 0 || p > 65535) { return false; }
    if (host) { *host = m.captured(1); }
    if (port) { *port = static_cast<quint16>(p); }
    return true;
}

void Gs232Rotor::setNetworkTarget(const QString& host, quint16 port)
{
    m_host    = host.trimmed();
    m_netPort = port ? port : 4001;
}

void Gs232Rotor::setPollIntervalMs(int ms)
{
    m_pollMs = qBound(100, ms, 10000);
    m_poll->setInterval(m_pollMs);
}

QString Gs232Rotor::description() const
{
    if (!m_host.isEmpty()) {
        return QStringLiteral("GS-232 über %1:%2").arg(m_host).arg(m_netPort);
    }
    if (m_portName.isEmpty()) {
        return QStringLiteral("GS-232 (keine Schnittstelle gewählt)");
    }
    return QStringLiteral("GS-232 %1, %2 Bd").arg(m_portName).arg(m_baud);
}

bool Gs232Rotor::hasFreshPosition() const
{
    if (!m_lastReading.isValid()) { return false; }
    return m_lastReading.msecsTo(QDateTime::currentDateTimeUtc()) < kStaleAfterMs;
}

// ── Protokoll ───────────────────────────────────────────────────────

bool Gs232Rotor::parsePosition(const QByteArray& zeile, double& azDeg, double& elDeg)
{
    // GS-232B: `AZ=aaa EL=eee` oder nur `AZ=aaa`; der ERC setzt zwei
    // Leerzeichen vor EL.
    static const QRegularExpression kB(QStringLiteral(
        "^AZ=\\s*(\\d{1,3}(?:\\.\\d+)?)(?:\\s+EL=\\s*(\\d{1,3}(?:\\.\\d+)?))?$"));
    // GS-232A: `+0aaa+0eee`. Hamlibs Form; an keinem Geraet beobachtet,
    // darum steht sie hier so eng wie moeglich.
    static const QRegularExpression kA(QStringLiteral(
        "^\\+(\\d{4})(?:\\+(\\d{4}))?$"));

    const QString s = QString::fromLatin1(zeile).trimmed();
    if (s.isEmpty()) { return false; }

    auto mB = kB.match(s);
    if (mB.hasMatch()) {
        azDeg = mB.captured(1).toDouble();
        elDeg = mB.captured(2).isEmpty() ? 0.0 : mB.captured(2).toDouble();
        return true;
    }
    auto mA = kA.match(s);
    if (mA.hasMatch()) {
        azDeg = mA.captured(1).toDouble();
        elDeg = mA.captured(2).isEmpty() ? 0.0 : mA.captured(2).toDouble();
        return true;
    }
    return false;
}

QByteArray Gs232Rotor::moveCommand(double azimuthDeg, double elevationDeg)
{
    const int az = static_cast<int>(qRound(wrap360(azimuthDeg))) % 360;
    const int el = std::clamp(static_cast<int>(qRound(elevationDeg)), 0, 180);
    return QStringLiteral("W%1 %2\r")
        .arg(az, 3, 10, QLatin1Char('0'))
        .arg(el, 3, 10, QLatin1Char('0'))
        .toLatin1();
}

// ── Verbindung ──────────────────────────────────────────────────────

void Gs232Rotor::connectToRotor()
{
    // Eine eingehaengte Pruefstandsnaht ist schon offen; die Reihenfolge
    // ist wichtig, sonst kehrt die Wache darunter zurueck, bevor der
    // Zustand steht.
    if (m_dev && !m_ownsDev) {
        setState(State::Idle);
        m_poll->start();
        poll();
        return;
    }

    m_soll = true;
    m_neu->stop();
    if (m_dev && m_dev->isOpen()) { return; }
    if (m_dev && m_ownsDev) {
        // Ein gescheiterter Versuch von vorher: weg damit.
        QIODevice* alt = m_dev;
        m_dev = nullptr;
        alt->disconnect(this);
        alt->deleteLater();
    }

    // Netzserver schlaegt serielle Schnittstelle.
    if (!m_host.isEmpty()) {
        setState(State::Connecting);
        auto* sock = new QTcpSocket(this);
        m_dev     = sock;
        m_ownsDev = true;
        connect(sock, &QIODevice::readyRead, this, &Gs232Rotor::onReadyRead);
        connect(sock, &QTcpSocket::connected, this, [this]() {
            m_frist->stop();
            if (m_fehlversuche > 0) {
                qCInfo(lcRotor) << "GS-232: wieder da nach" << m_fehlversuche << "Versuch(en)";
            }
            m_fehlversuche = 0;
            m_wartenMs = m_ersteWartenMs;
            m_gehoert.restart();
            qCInfo(lcRotor) << "GS-232: Netzserver offen" << m_host << m_netPort;
            setState(State::Idle);
            m_poll->start();
            poll();
        });
        connect(sock, &QTcpSocket::errorOccurred, this,
                [this, sock](QAbstractSocket::SocketError) {
                    // Stand die Verbindung schon, ist sie verloren -- sonst
                    // war das Ziel nicht erreichbar.
                    const bool stand = m_state == State::Idle || m_state == State::Moving;
                    netzVerloren(sock, QStringLiteral("%1:%2 %3: %4")
                                           .arg(m_host).arg(m_netPort)
                                           .arg(stand ? QStringLiteral("connection lost")
                                                      : QStringLiteral("not reachable"))
                                           .arg(sock->errorString()));
                });
        connect(sock, &QTcpSocket::disconnected, this, [this, sock]() {
            netzVerloren(sock, QStringLiteral("%1:%2 closed the connection")
                                   .arg(m_host).arg(m_netPort));
        });
        m_frist->start();
        sock->connectToHost(m_host, m_netPort);
        return;
    }

    if (m_portName.isEmpty()) {
        fail(QStringLiteral("Weder eine serielle Schnittstelle noch ein Netzziel gewählt."));
        return;
    }

#ifndef HAVE_SERIALPORT
    fail(QStringLiteral("Dieser Bau hat keine serielle Unterstützung."));
#else
    setState(State::Connecting);
    auto* port = new QSerialPort(m_portName, this);
    port->setBaudRate(m_baud);
    // 8N1 ohne Handschlag — so spricht der GS-232 (Hamlib gs232a.c /
    // gs232b.c, siehe Gs232Rotor.h).
    port->setDataBits(QSerialPort::Data8);
    port->setParity(QSerialPort::NoParity);
    port->setStopBits(QSerialPort::OneStop);
    port->setFlowControl(QSerialPort::NoFlowControl);

    if (!port->open(QIODevice::ReadWrite)) {
        const QString grund = port->errorString();
        port->deleteLater();
        fail(QStringLiteral("Schnittstelle %1 ließ sich nicht öffnen: %2")
                 .arg(m_portName, grund));
        return;
    }
    m_dev     = port;
    m_ownsDev = true;
    connect(port, &QIODevice::readyRead, this, &Gs232Rotor::onReadyRead);
    connect(port, &QSerialPort::errorOccurred, this,
            [this, port](QSerialPort::SerialPortError e) {
                if (e == QSerialPort::NoError) { return; }
                fail(QStringLiteral("Schnittstelle meldet: %1").arg(port->errorString()));
            });

    qCInfo(lcRotor) << "GS-232: Schnittstelle offen" << m_portName << m_baud;
    setState(State::Idle);
    m_poll->start();
    poll();
#endif
}

void Gs232Rotor::disconnectFromRotor()
{
    // Ausdruecklich getrennt: auch nicht mehr neu verbinden.
    m_soll = false;
    m_neu->stop();
    m_frist->stop();
    m_fehlversuche = 0;
    m_wartenMs = m_ersteWartenMs;
    if (m_dev && m_ownsDev) { m_dev->disconnect(this); }
    m_poll->stop();
    if (m_dev && m_ownsDev) {
        m_dev->close();
        m_dev->deleteLater();
        m_dev = nullptr;
    }
    m_buffer.clear();
    m_lastReading = {};
    setState(State::Disconnected);
}

void Gs232Rotor::moveTo(double azimuthDeg)
{
    if (!m_dev || !m_dev->isOpen()) {
        emit errorOccurred(QStringLiteral("Der Rotor ist nicht verbunden — nichts gedreht."));
        return;
    }
    if (!pruefeZiel(azimuthDeg)) { return; }      // erlaubter Bereich
    send(moveCommand(azimuthDeg, 0.0));
    setState(State::Moving);
}

void Gs232Rotor::stop()
{
    if (!m_dev || !m_dev->isOpen()) { return; }
    send(stopCommand());
    setState(State::Idle);
}

// ── Innereien ───────────────────────────────────────────────────────

void Gs232Rotor::poll()
{
    if (!m_dev || !m_dev->isOpen()) { return; }
    // Stummwache (Netz): ein ARCO, dem der Strom fehlt, schliesst keine
    // Verbindung -- die TCP-Seite merkt es erst nach Minuten. Wer auf die
    // Abfragen so lange nichts sagt, gilt als weg; dann neu verbinden.
    if (!m_host.isEmpty() && m_ownsDev && m_gehoert.isValid()
        && m_gehoert.elapsed() > m_stummMs) {
        netzVerloren(m_dev, QStringLiteral("%1:%2 stopped answering — reconnecting")
                                .arg(m_host).arg(m_netPort));
        return;
    }
    send(pollCommand());
}

void Gs232Rotor::send(const QByteArray& command)
{
    if (!m_dev || !m_dev->isOpen()) { return; }
    m_dev->write(command);
}

void Gs232Rotor::onReadyRead()
{
    if (!m_dev) { return; }
    m_gehoert.restart();
    m_buffer.append(m_dev->readAll());

    for (;;) {
        qsizetype ende = -1;
        for (qsizetype i = 0; i < m_buffer.size(); ++i) {
            const char c = m_buffer.at(i);
            if (c == '\r' || c == '\n') { ende = i; break; }
        }
        if (ende < 0) {
            // Noch keine ganze Zeile. Eine, die nie zu Ende geht, ist
            // Rauschen — die wird verworfen, nicht gesammelt.
            if (m_buffer.size() > kMaxLineBytes) { m_buffer.clear(); }
            break;
        }
        const QByteArray zeile = m_buffer.left(ende);
        m_buffer.remove(0, ende + 1);
        if (zeile.trimmed().isEmpty()) { continue; }

        double az = 0.0;
        double el = 0.0;
        if (!parsePosition(zeile, az, el)) {
            // Der GS-232 quittiert Fahrbefehle nicht; was hier
            // ankommt und keine Lage ist, ist nichts, worauf man
            // warten muss. Nur ins Protokoll, nicht an die Oberflaeche.
            qCDebug(lcRotor) << "GS-232: unverstandene Zeile" << zeile;
            continue;
        }

        m_lastReading = QDateTime::currentDateTimeUtc();
        if (!qFuzzyCompare(1.0 + az, 1.0 + m_azimuth)) {
            m_azimuth = az;
            emit positionChanged(m_azimuth);
        } else {
            m_azimuth = az;
        }
        if (!qFuzzyCompare(1.0 + el, 1.0 + m_elevation)) {
            m_elevation = el;
            emit elevationChanged(m_elevation);
        }
    }
}

void Gs232Rotor::setState(State s)
{
    if (s == m_state) { return; }
    m_state = s;
    emit stateChanged(m_state);
}

void Gs232Rotor::fail(const QString& why)
{
    qCWarning(lcRotor) << "GS-232:" << why;
    emit errorOccurred(why);
    setState(State::Error);
}

#ifdef LONGPATH_BUILD_TESTS
void Gs232Rotor::useDeviceForTest(QIODevice* dev)
{
    m_dev     = dev;
    m_ownsDev = false;
    if (dev) {
        connect(dev, &QIODevice::readyRead, this, &Gs232Rotor::onReadyRead);
    }
}
#endif

} // namespace Longpath
