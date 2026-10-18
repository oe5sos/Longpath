// =================================================================
// src/core/FlexRadioConnection.cpp  (Longpath)
// =================================================================
//
// Herkunft, Lizenz und der Grund, warum `client gui` hier fehlt:
// siehe FlexRadioConnection.h.
// Ported from AetherSDR src/core/CommandParser.cpp,
// src/core/RadioConnection.cpp, src/models/RadioModel.cpp and
// src/core/backends/flex/FlexBackend.cpp at d58e2b8a (GPLv3, primary
// author Jeremy [KK7GWY]) — project-level citation per
// docs/attribution/HOW-TO-PORT.md rule 6.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-18 — Ported to Longpath by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork).
// =================================================================

#include "FlexRadioConnection.h"

#include <QLoggingCategory>
#include <QStringList>

namespace Longpath {

Q_LOGGING_CATEGORY(lcFlexCmd, "longpath.flex.cmd", QtInfoMsg)

namespace {

// From AetherSDR RadioConnection.cpp:58 [@d58e2b8a] — kMaxReadBuffer.
// Eine Maschine, die 16 MB ohne Zeilenende schickt, redet nicht dieses
// Protokoll; dann wird aufgelegt statt weiter gesammelt.
constexpr int kMaxReadBuffer = 16 * 1024 * 1024;

}  // namespace

// ─── FlexCommandWire ─────────────────────────────────────────────────

namespace FlexCommandWire {

// From AetherSDR CommandParser.cpp:9-22 [@d58e2b8a] — parseKVs().
QMap<QString, QString> parseKVs(const QString& body)
{
    QMap<QString, QString> result;
    // Rumpf sieht aus wie: "freq=14.225000 mode=USB filter_lo=-1500 filter_hi=1500"
    for (const QString& token : body.split(' ', Qt::SkipEmptyParts)) {
        const int eq = token.indexOf('=');
        if (eq < 0) {
            // nacktes Wort — mit leerem Wert ablegen, damit der Leser
            // seine Anwesenheit erkennen kann
            result.insert(token, QString{});
        } else {
            result.insert(token.left(eq), token.mid(eq + 1));
        }
    }
    return result;
}

// From AetherSDR CommandParser.cpp:34-129 [@d58e2b8a] — parseLine().
//
// Satzbau (ASCII, je Zeile mit \n):
//   V3.3.28.0                         → Fassung
//   H0A1B2C3D                         → Clientkennung, hex
//   R1|0|                             → Antwort auf Befehl 1, Code 0 (gut)
//   R2|50001001|No Such Object        → Antwort mit Fehler
//   S0A1B2C3D|slice 0 freq=14.225000  → Zustandsschub einer Scheibe
//   M0A1B2C3D|...                     → Meldung
FlexLine parseLine(const QString& rawLine)
{
    FlexLine msg;
    msg.raw = rawLine.trimmed();

    if (msg.raw.isEmpty()) {
        return msg;
    }

    const QChar   tag  = msg.raw[0];
    const QString body = msg.raw.mid(1);   // alles nach dem Satzzeichen

    switch (tag.toLatin1()) {
    case 'V':
        msg.type   = FlexLineType::Version;
        msg.object = body;   // Fassungszeichenkette
        break;

    case 'H':
        msg.type   = FlexLineType::Handle;
        msg.handle = body.toUInt(nullptr, 16);
        break;

    case 'R': {
        // R<seq>|<code>|<rumpf>
        msg.type = FlexLineType::Response;
        const QStringList parts = body.split('|');
        if (parts.size() >= 1) {
            msg.sequence = parts[0].toUInt();
        }
        if (parts.size() >= 2) {
            // SmartSDR-Ergebniscodes sind vorzeichenlose 32-Bit-Werte.
            // Schwere Codes wie F3000001 haben das oberste Bit gesetzt;
            // QString::toInt() lehnt sie als Ueberlauf ab und gibt
            // null — damit wuerde eine schwere Ablehnung wie Erfolg
            // aussehen. (AetherSDR schreibt genau diesen Hinweis an
            // dieselbe Stelle, CommandParser.cpp:63-70.) Longpath
            // fuehrt den Code deshalb von Anfang an als quint32.
            bool ok = false;
            const quint32 resultCode = parts[1].toUInt(&ok, 16);
            if (ok) {
                msg.resultCode = resultCode;
            }
        }
        if (parts.size() >= 3) {
            msg.object = parts[2];
            msg.kvs    = parseKVs(parts[2]);
        }
        break;
    }

    case 'S': {
        // S<handle>|<objektname> [schluessel=wert ...]
        //
        // Objektnamen koennen mehrteilig sein, z. B. "slice 0" oder
        // "display pan 0x40000000". Schluesselpaare enthalten immer ein
        // '=', Objektwoerter nie. Die Grenze ist also das letzte
        // Leerzeichen VOR dem ersten '='.
        msg.type = FlexLineType::Status;
        const int pipe = body.indexOf('|');
        if (pipe < 0) {
            break;
        }
        msg.handle = body.left(pipe).toUInt(nullptr, 16);
        const QString statusBody = body.mid(pipe + 1);

        const int firstEq = statusBody.indexOf('=');
        if (firstEq < 0) {
            // keine Schluesselpaare — der ganze Rumpf ist der Objektname
            msg.object = statusBody.trimmed();
        } else {
            const int kvStart = statusBody.lastIndexOf(' ', firstEq);
            if (kvStart < 0) {
                // Rumpf beginnt direkt mit einem Schluesselpaar
                msg.kvs = parseKVs(statusBody);
            } else {
                msg.object = statusBody.left(kvStart).trimmed();
                msg.kvs    = parseKVs(statusBody.mid(kvStart + 1));
            }
        }
        break;
    }

    case 'M': {
        msg.type = FlexLineType::Message;
        const int pipe = body.indexOf('|');
        if (pipe >= 0) {
            // Die Zahl vor dem '|' ist hex. Bit 24-25 tragen die
            // Schwere (Info=0, Warning=1, Error=2, Fatal=3) per FlexLib
            // Radio.cs:4498-4516; der Rest ist eine undurchsichtige
            // Meldungsnummer. Sie liegt im selben Feld wie das Handle,
            // damit die Satzstruktur nicht je Satzart anders aussieht.
            msg.handle   = body.left(pipe).toUInt(nullptr, 16);
            msg.severity = static_cast<FlexSeverity>((msg.handle >> 24) & 0x3);
            msg.object   = body.mid(pipe + 1);
        }
        break;
    }

    default:
        msg.type = FlexLineType::Unknown;
        break;
    }

    return msg;
}

// From AetherSDR CommandParser.cpp:133-136 [@d58e2b8a] — buildCommand().
QByteArray buildCommand(quint32 seq, const QString& command)
{
    return QStringLiteral("C%1|%2\n").arg(seq).arg(command).toUtf8();
}

int sliceIdFromObject(const QString& object)
{
    const QStringList parts = object.split(' ', Qt::SkipEmptyParts);
    if (parts.size() < 2 || parts[0] != QLatin1String("slice")) {
        return -1;
    }
    bool ok = false;
    const int id = parts[1].toInt(&ok);
    return ok ? id : -1;
}

// From AetherSDR FlexBackend.cpp:742 [@d58e2b8a] —
// carry(kvs, "RF_frequency", d.frequency), mit der dort begruendeten
// „nur wenn gemeldet, nur wenn lesbar"-Regel
// (FlexKvCarry.h parseReal, FlexBackend.cpp:731-736).
//
// Der Wert kommt in MHz ueber den Draht („RF_frequency=14.074000").
std::optional<double> frequencyHzFromSliceKvs(const QMap<QString, QString>& kvs)
{
    static const QString kKey = QStringLiteral("RF_frequency");
    if (!kvs.contains(kKey)) {
        return std::nullopt;
    }
    bool ok = false;
    const double mhz = kvs.value(kKey).toDouble(&ok);
    if (!ok) {
        return std::nullopt;
    }
    return mhz * 1'000'000.0;
}

}  // namespace FlexCommandWire

// ─── FlexRadioConnection ─────────────────────────────────────────────

FlexRadioConnection::FlexRadioConnection(QObject* parent)
    : QObject(parent)
{
    connect(&m_socket, &QTcpSocket::connected,
            this, &FlexRadioConnection::onSocketConnected);
    connect(&m_socket, &QTcpSocket::disconnected,
            this, &FlexRadioConnection::onSocketDisconnected);
    connect(&m_socket, &QTcpSocket::errorOccurred,
            this, &FlexRadioConnection::onSocketErrorOccurred);
    connect(&m_socket, &QTcpSocket::readyRead,
            this, &FlexRadioConnection::onReadyRead);
}

FlexRadioConnection::~FlexRadioConnection()
{
    m_socket.abort();
}

void FlexRadioConnection::connectToRadio(const QHostAddress& address, quint16 port)
{
    if (m_state != State::Disconnected && m_state != State::Error) {
        return;
    }

    m_readBuffer.clear();
    m_handle  = 0;
    m_nextSeq = 1;
    m_version.clear();
    m_sliceFreqHz.clear();

    qCInfo(lcFlexCmd) << "verbinde nach" << address.toString() << ":" << port;
    setState(State::Connecting);
    m_socket.connectToHost(address, port);
}

void FlexRadioConnection::disconnectFromRadio()
{
    if (m_socket.state() == QAbstractSocket::UnconnectedState) {
        setState(State::Disconnected);
        return;
    }
    // Stufe 1 haelt keine Stroeme und keine Scheiben — es gibt nichts
    // abzuraeumen. AetherSDRs gracefulDisconnect (RadioConnection.cpp:295
    // [@d58e2b8a]) nimmt vorher Stroeme zurueck; das gehoert in die
    // Stufe, die Stroeme oeffnet.
    m_socket.disconnectFromHost();
}

quint32 FlexRadioConnection::sendCommand(const QString& command)
{
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        qCWarning(lcFlexCmd) << "Befehl verworfen, keine Verbindung:" << command;
        return 0;
    }
    const quint32 seq = m_nextSeq++;
    const QByteArray wire = FlexCommandWire::buildCommand(seq, command);
    if (m_socket.write(wire) != wire.size()) {
        qCWarning(lcFlexCmd) << "Schreiben unvollstaendig:" << m_socket.errorString();
    }
    qCDebug(lcFlexCmd) << "TX:" << QStringLiteral("C%1|%2").arg(seq).arg(command);
    return seq;
}

void FlexRadioConnection::onSocketConnected()
{
    m_socket.setSocketOption(QAbstractSocket::LowDelayOption, 1);
    qCInfo(lcFlexCmd) << "TCP steht;" << "warte auf V/H";
    // Noch NICHT Connected: die Maschine schickt zuerst V (Fassung) und
    // H (Clientkennung). Erst mit dem Handle ist der Kanal brauchbar.
    // From AetherSDR RadioConnection.cpp:552-559 [@d58e2b8a] — dort
    // setzt genau der H-Satz den Zustand auf Connected.
}

void FlexRadioConnection::onSocketDisconnected()
{
    qCInfo(lcFlexCmd) << "TCP getrennt";
    m_handle = 0;
    setState(State::Disconnected);
    emit disconnected();
}

void FlexRadioConnection::onSocketErrorOccurred(QAbstractSocket::SocketError)
{
    const QString msg = m_socket.errorString();
    qCWarning(lcFlexCmd) << "Socketfehler:" << msg;
    setState(State::Error);
    emit errorOccurred(msg);
}

// From AetherSDR RadioConnection.cpp:512-530 [@d58e2b8a] — onReadyRead().
void FlexRadioConnection::onReadyRead()
{
    m_readBuffer.append(m_socket.readAll());
    if (m_readBuffer.size() > kMaxReadBuffer) {
        qCWarning(lcFlexCmd) << "Lesepuffer ueber" << kMaxReadBuffer
                             << "Byte ohne Zeilenende — lege auf";
        m_socket.disconnectFromHost();
        m_readBuffer.clear();
        return;
    }
    int newlinePos;
    while ((newlinePos = m_readBuffer.indexOf('\n')) >= 0) {
        const QString line =
            QString::fromUtf8(m_readBuffer.left(newlinePos)).trimmed();
        m_readBuffer.remove(0, newlinePos + 1);
        if (!line.isEmpty()) {
            processLine(line);
        }
    }
}

// From AetherSDR RadioConnection.cpp:536-585 [@d58e2b8a] — processLine().
void FlexRadioConnection::processLine(const QString& line)
{
    // GPS-Koordinaten gehoeren in kein Protokoll. AetherSDR haelt sie
    // an genau dieser Stelle aus dem Log heraus (RadioConnection.cpp:539
    // [@d58e2b8a]) — uebernommen, aus demselben Grund.
    const bool isGps = line.contains(QLatin1String("|gps "));
    if (!isGps) {
        qCDebug(lcFlexCmd) << "RX:" << line;
    }

    const FlexLine msg = FlexCommandWire::parseLine(line);
    emit lineReceived(msg);

    switch (msg.type) {
    case FlexLineType::Version:
        m_version = msg.object;
        qCInfo(lcFlexCmd) << "Maschine meldet Fassung" << m_version;
        emit versionReceived(m_version);
        break;

    case FlexLineType::Handle:
        m_handle = msg.handle;
        qCInfo(lcFlexCmd) << "Clientkennung"
                          << QStringLiteral("0x%1").arg(m_handle, 0, 16);
        setState(State::Connected);
        emit handleAssigned(m_handle);
        emit connected();
        sendRegistrationSequence();
        break;

    case FlexLineType::Response:
        emit commandResponse(msg.sequence, msg.resultCode, msg.object);
        if (msg.resultCode != 0) {
            qCWarning(lcFlexCmd) << "Befehl" << msg.sequence << "abgelehnt, Code"
                                 << QStringLiteral("0x%1").arg(msg.resultCode, 0, 16)
                                 << msg.object;
        }
        break;

    case FlexLineType::Status: {
        emit statusReceived(msg.object, msg.kvs);

        const int sliceId = FlexCommandWire::sliceIdFromObject(msg.object);
        if (sliceId >= 0) {
            if (const auto hz = FlexCommandWire::frequencyHzFromSliceKvs(msg.kvs)) {
                // Nur melden, wenn sich wirklich etwas aendert: die
                // Maschine schiebt denselben Zustand mehrfach, und ein
                // unveraenderter Wert ist keine Nachricht.
                const auto it = m_sliceFreqHz.constFind(sliceId);
                if (it == m_sliceFreqHz.constEnd() || !qFuzzyCompare(it.value(), *hz)) {
                    m_sliceFreqHz[sliceId] = *hz;
                    qCInfo(lcFlexCmd) << "Scheibe" << sliceId << "auf"
                                      << QStringLiteral("%1 Hz").arg(*hz, 0, 'f', 0);
                    emit sliceFrequencyChanged(sliceId, *hz);
                }
            }
        }
        break;
    }

    case FlexLineType::Message:
        emit messageFromRadio(msg.severity, msg.object);
        if (msg.severity == FlexSeverity::Error || msg.severity == FlexSeverity::Fatal) {
            qCWarning(lcFlexCmd) << "Meldung der Maschine:" << msg.object;
        }
        break;

    case FlexLineType::Unknown:
        break;
    }
}

// From AetherSDR RadioModel.cpp:6523-6596 [@d58e2b8a] — die
// Anmeldefolge, OHNE `client gui`: siehe den Kopf von
// FlexRadioConnection.h fuer den Grund. Was bleibt, ist das Minimum,
// mit dem ein Mitleser Scheibenzustaende bekommt.
void FlexRadioConnection::sendRegistrationSequence()
{
    // „client program <name>" zuerst — die Maschine will die Identitaet
    // des Clients, bevor sie Client-Einstellungen annimmt
    // (RadioModel.cpp:6522-6530 begruendet die Reihenfolge an einem
    // echten Fehler: AetherSDR-Fehlerbericht 2447, low_bw_connect wurde
    // still ignoriert, wenn es vor `client program` kam. Die Nummer hier
    // ohne Raute, weil `verify-style-drift.py` ein „#2447" als namenloses
    // Farbliteral zaehlt — vier Hexstellen hinter einer Raute).
    sendCommand(QStringLiteral("client program Longpath"));

    // Und das eine Abonnement, das Stufe 1 braucht.
    sendCommand(QStringLiteral("sub slice all"));
}

void FlexRadioConnection::setState(State s)
{
    if (m_state == s) {
        return;
    }
    m_state = s;
    emit stateChanged(s);
}

}  // namespace Longpath
