// =================================================================
// src/core/Gs232Rotor.cpp  (Longpath)
// =================================================================
// Siehe Gs232Rotor.h für Herkunft und Protokoll.
// =================================================================

#include "Gs232Rotor.h"

#include <QIODevice>
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
}

Gs232Rotor::~Gs232Rotor()
{
    // Die Schnittstelle gehoert uns nur, wenn wir sie selbst geoeffnet
    // haben; eine eingehaengte Pruefstandsnaht wird nicht geschlossen.
    if (m_dev && m_ownsDev) {
        m_dev->close();
    }
}

void Gs232Rotor::setPort(const QString& portName, int baudRate)
{
    m_portName = portName;
    m_baud     = baudRate > 0 ? baudRate : 9600;
}

void Gs232Rotor::setPollIntervalMs(int ms)
{
    m_pollMs = qBound(100, ms, 10000);
    m_poll->setInterval(m_pollMs);
}

QString Gs232Rotor::description() const
{
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

    if (m_dev && m_dev->isOpen()) { return; }

    if (m_portName.isEmpty()) {
        fail(QStringLiteral("Keine serielle Schnittstelle gewählt."));
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
