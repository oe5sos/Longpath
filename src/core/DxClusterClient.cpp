// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath - DX cluster telnet client (implementation)
//
// Ported from AetherSDR src/core/DxClusterClient.cpp [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (Longpath):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task B3. Initial port.
//                                    See DxClusterClient.h for full
//                                    attribution notes. Longpath
//                                    refactor: stripTelnetIAC() is split
//                                    into a pure
//                                    stripTelnetIACBuffer(QByteArray&)
//                                    helper plus a thin instance method
//                                    that calls it on m_readBuffer, so
//                                    the public stripTelnetIACForTest
//                                    seam can exercise the algorithm
//                                    without a live socket. Longpath
//                                    addition: parseDxSpotLine() now
//                                    assigns spot.source = "Cluster" by
//                                    default and promotes to "RBN" when
//                                    the spotter callsign starts with
//                                    "RBN-" (case-insensitive) or ends
//                                    with "-#" (Reverse Beacon Network
//                                    skimmer marker). Upstream
//                                    AetherSDR's parser left source
//                                    unset; the calling site set it
//                                    based on which connection (cluster
//                                    vs. RBN) emitted the spot. Logic,
//                                    field mapping, regex, and reject
//                                    filter are byte-for-byte from
//                                    upstream. AI tooling: Anthropic
//                                    Claude Code.
//   2026-10-28  Martin Fischer (OE5SOS)  Der Wiederanlauf-Zaehler
//                                    faellt erst, wenn die Sitzung
//                                    StableSessionMs ueberlebt hat --
//                                    die Anmeldung allein genuegt
//                                    nicht, weil ein DXSpider-Knoten
//                                    sie annimmt und bei doppeltem
//                                    Rufzeichen 8,8 s spaeter
//                                    hinauswirft (216 Versuche
//                                    gemessen). Dazu istHinauswurf():
//                                    die Begruendung des Knotens wird
//                                    nicht mehr stillschweigend
//                                    verworfen. Und
//                                    protokollOeffnungsart(): das
//                                    Mitschreiben wird nur noch bei
//                                    einer Verbindung geleert, die der
//                                    Betreiber selbst anstoesst -- ein
//                                    Wiederanlauf schreibt fort, sonst
//                                    loescht gerade ein Sturm seine
//                                    eigenen Belege. KI-gestuetzt ueber
//                                    Anthropic Claude Code.

#include "DxClusterClient.h"
#include "core/AppSettings.h"
#include "LogCategories.h"

#include <QRegularExpression>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <algorithm>

namespace Longpath {

// From AetherSDR src/core/DxClusterClient.cpp:12-25 [@0cd4559]
DxClusterClient::DxClusterClient(QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_reconnectTimer(new QTimer(this))
    , m_stableTimer(new QTimer(this))
{
    connect(m_socket, &QTcpSocket::connected,    this, &DxClusterClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &DxClusterClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead,    this, &DxClusterClient::onReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred,
            this, &DxClusterClient::onSocketError);

    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &DxClusterClient::onReconnectTimer);

    m_stableTimer->setSingleShot(true);
    connect(m_stableTimer, &QTimer::timeout, this, &DxClusterClient::onSessionStable);
}

// From AetherSDR src/core/DxClusterClient.cpp:27-34 [@0cd4559]
DxClusterClient::~DxClusterClient()
{
    m_intentionalDisconnect = true;
    m_reconnectTimer->stop();
    m_stableTimer->stop();
    m_logFile.close();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
}

// From AetherSDR src/core/DxClusterClient.cpp:36-40 [@0cd4559]
//
// Longpath uses Qt's AppConfigLocation (which already lands under
// "Longpath/" when QCoreApplication::organizationName/applicationName
// are set) instead of AetherSDR's GenericConfigLocation +
// "AetherSDR/...". This keeps the file co-located with the rest of
// Longpath's per-user state, mirroring the SpotCollectorClient (B1)
// and PotaClient (B2) ports.
QString DxClusterClient::logFilePath() const
{
    return AppSettings::dataDir()
           + "/" + m_logFileName;
}

// From AetherSDR src/core/DxClusterClient.cpp:42-67 [@0cd4559]
void DxClusterClient::connectToCluster(const QString& host, quint16 port, const QString& callsign)
{
    if (m_connected) {
        qCWarning(lcSpots) << "DxClusterClient: already connected";
        return;
    }

    m_host = host;
    m_port = port;
    m_callsign = callsign;
    m_loggedIn = false;
    m_intentionalDisconnect = false;
    m_readBuffer.clear();

    qCInfo(lcSpots) << "DxClusterClient: connecting to" << host << ":" << port
                    << "callsign=" << callsign;
    m_socket->connectToHost(host, port);

    // Connection timeout
    QTimer::singleShot(ConnectTimeoutMs, this, [this] {
        if (!m_connected && m_socket->state() != QAbstractSocket::ConnectedState) {
            // Dieselbe Zurueckhaltung wie bei onSocketError: die Zeile
            // stand 952-mal im Protokoll vom 14.10.
            if (m_quietFailures == 0) {
                qCWarning(lcSpots) << "DxClusterClient: connection timeout";
            }
            m_socket->abort();
            emit connectionError(QStringLiteral("Connection timeout"));
        }
    });
}

// From AetherSDR src/core/DxClusterClient.cpp:69-78 [@0cd4559]
void DxClusterClient::disconnect()
{
    m_intentionalDisconnect = true;
    m_naechsterIstWiederanlauf = false;   // der naechste ist ein neuer
    m_reconnectTimer->stop();
    m_stableTimer->stop();
    if (m_connected) {
        m_socket->write("bye\r\n");
        m_socket->flush();
    }
    m_socket->disconnectFromHost();
}

// From AetherSDR src/core/DxClusterClient.cpp:80-89 [@0cd4559]
void DxClusterClient::sendCommand(const QString& cmd)
{
    if (!m_connected) {
        return;
    }
    qCDebug(lcSpots) << "DxClusterClient TX:" << cmd;
    if (m_logFile.isOpen()) {
        m_logFile.write(("> " + cmd + "\n").toUtf8());
        m_logFile.flush();
    }
    m_socket->write((cmd + "\r\n").toLatin1());
}

// ── Socket slots ────────────────────────────────────────────────────────────

// From AetherSDR src/core/DxClusterClient.cpp:93-112 [@0cd4559]
void DxClusterClient::onConnected()
{
    qCInfo(lcSpots) << "DxClusterClient: TCP connected to" << m_host;
    m_connected = true;
    // Den Wiederanlauf-Zaehler NICHT hier zuruecksetzen (2026-10-14).
    //
    // Gemessen am Betreiber-Rechner: zwischen 14:30 und 17:10 sind 680
    // Versuche gelaufen, alle 14 s einer, und die Treppe hat nie die
    // zweite Stufe erreicht. Der Grund stand hier: ein Cluster NIMMT die
    // TCP-Verbindung an und wirft erst beim Anmelden hinaus ("The remote
    // host closed the connection"). Jeder Zyklus lief also
    // connectToCluster -> onConnected (Zaehler = 0) -> onDisconnected
    // (Verzoegerung = 1<<0 = 5 s) -> wieder von vorn. 4730 Protokollzeilen
    // in zweidreiviertel Stunden, mehr als die Haelfte des ganzen Logs.
    //
    // Ein offener Socket ist noch keine brauchbare Sitzung. Und am
    // 2026-10-28 zeigte sich, dass die ANMELDUNG es auch noch nicht
    // ist: der Knoten nimmt sie an und wirft bei doppeltem Rufzeichen
    // 8,8 s spaeter hinaus. Der Zaehler faellt deshalb erst, wenn die
    // Sitzung gehalten hat -- siehe StableSessionMs und
    // onSessionStable().

    // Mitschreiben: geleert wird nur bei einer Verbindung, die der
    // Betreiber selbst anstoesst -- siehe protokollOeffnungsart().
    const bool wiederanlauf = m_naechsterIstWiederanlauf;
    m_naechsterIstWiederanlauf = false;

    m_logFile.close();
    m_logFile.setFileName(logFilePath());
    QDir().mkpath(QFileInfo(m_logFile).absolutePath());
    if (m_logFile.open(protokollOeffnungsart(wiederanlauf))) {
        m_logFile.write(QString("--- Connected to %1:%2 at %3 ---\n")
            .arg(m_host).arg(m_port)
            .arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss UTC"))
            .toUtf8());
        m_logFile.flush();
    }

    emit connected();
}

// Die Anmeldung ist durch: erst jetzt gilt die Verbindung als brauchbar,
// und erst jetzt faengt die Treppe des Wiederanlaufs wieder unten an.
void DxClusterClient::markLoggedIn()
{
    m_loggedIn = true;

    // Hier faellt der Zaehler NICHT. Der Knoten nimmt die Anmeldung an
    // und wirft 8,8 s spaeter hinaus, wenn ein zweiter Anschluss
    // dasselbe Rufzeichen fuehrt -- siehe StableSessionMs. Erst eine
    // Sitzung, die eine Minute haelt, ist ein Erfolg.
    m_stableTimer->start(StableSessionMs);
}

// Die Sitzung hat die Standzeit ueberlebt: jetzt zaehlt sie als
// gelungen, und die Treppe darf von vorn beginnen. Hier fallen auch
// die Zaehler der Protokoll-Zusammenfassung -- vorher loeschte jeder
// Zyklus m_lastFailureMsg, weshalb dieselbe Steckdosenmeldung am
// 2026-10-28 214 Mal durchkam statt zusammengefasst zu werden.
void DxClusterClient::onSessionStable()
{
    if (!m_loggedIn) {
        return;
    }
    if (m_reconnectAttempts > 0) {
        qCInfo(lcSpots) << "DxClusterClient: Sitzung haelt seit"
                        << StableSessionMs / 1000 << "s --"
                        << m_reconnectAttempts
                        << "Versuche abgeraeumt, Treppe zurueckgesetzt";
    }
    m_reconnectAttempts = 0;
    m_quietFailures = 0;
    m_lastFailureMsg.clear();
}

// Am 2026-10-28 am Knoten dxc.nc7j.com:7300 live mitgeschrieben; die
// Begruendung steht an der Deklaration.
QIODevice::OpenMode DxClusterClient::protokollOeffnungsart(bool wiederanlauf)
{
    const QIODevice::OpenMode grund =
        QIODevice::WriteOnly | QIODevice::Text;
    return wiederanlauf ? (grund | QIODevice::Append)
                        : (grund | QIODevice::Truncate);
}

bool DxClusterClient::istHinauswurf(const QString& line)
{
    return line.contains(QLatin1String("this instance is disconnected"),
                         Qt::CaseInsensitive);
}

// From AetherSDR src/core/DxClusterClient.cpp:114-131 [@0cd4559]
void DxClusterClient::onDisconnected()
{
    qCDebug(lcSpots) << "DxClusterClient: disconnected";
    bool wasConnected = m_connected;
    m_connected = false;
    m_loggedIn = false;
    // Eine Sitzung, die hier endet, hat die Standzeit nicht erreicht.
    m_stableTimer->stop();

    if (wasConnected) {
        emit disconnected();
    }

    if (!m_intentionalDisconnect) {
        // From AetherSDR src/core/DxClusterClient.cpp:124-128 [@0cd4559] — original
        // pattern is `1 << m_reconnectAttempts`. Longpath fix: clamp shift count
        // to avoid signed-int UB. The backoff saturates at MaxReconnectDelayMs
        // well before 30 attempts, so capping has no behavioral effect on a
        // healthy connection.
        const int shiftBits = std::min(m_reconnectAttempts, 30);
        int delay = std::min(InitialReconnectDelayMs * (1 << shiftBits),
                             MaxReconnectDelayMs);
        if (delay >= MaxReconnectDelayMs && m_reconnectAttempts > 0
            && (m_reconnectAttempts & (m_reconnectAttempts - 1)) == 0) {
            // Nur bei 1, 2, 4, 8, ... Versuchen -- aber sichtbar: der
            // Bediener soll erfahren, dass der Cluster ihn dauerhaft
            // abweist, statt es nur im Debug-Kanal zu verstecken.
            qCWarning(lcSpots) << "DxClusterClient:" << m_reconnectAttempts
                               << "Versuche ohne Anmeldung -- naechster in"
                               << delay / 1000 << "s";
        }
        qCDebug(lcSpots) << "DxClusterClient: reconnecting in" << delay << "ms";
        // Das naechste onConnected gehoert zu DIESER Sitzung, nicht zu
        // einer neuen: das Mitschreiben wird dann nicht geleert.
        m_naechsterIstWiederanlauf = true;
        m_reconnectTimer->start(delay);
        m_reconnectAttempts++;
    }
}

// From AetherSDR src/core/DxClusterClient.cpp:133-138 [@0cd4559]
void DxClusterClient::onSocketError(QAbstractSocket::SocketError /*err*/)
{
    QString msg = m_socket->errorString();
    // Dieselbe Meldung nicht hundertfach: einmal beim ersten Mal, danach
    // hoechstens alle 60 s -- dann aber mit der Zahl der Versuche, damit
    // im Protokoll steht, wie lange es schon so geht. Eine NEUE Meldung
    // kommt immer durch; sie ist der Hinweis, dass sich etwas geaendert
    // hat.
    const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
    if (msg != m_lastFailureMsg || jetzt - m_lastFailureLogMs >= 60000) {
        if (m_quietFailures > 0) {
            qCWarning(lcSpots) << "DxClusterClient: socket error:" << msg
                               << "(" << m_quietFailures + 1 << "mal seit der letzten Zeile)";
        } else {
            qCWarning(lcSpots) << "DxClusterClient: socket error:" << msg;
        }
        m_lastFailureMsg = msg;
        m_lastFailureLogMs = jetzt;
        m_quietFailures = 0;
    } else {
        ++m_quietFailures;
    }
    emit connectionError(msg);
}

// From AetherSDR src/core/DxClusterClient.cpp:140-145 [@0cd4559]
void DxClusterClient::onReconnectTimer()
{
    if (m_intentionalDisconnect) {
        return;
    }
    qCDebug(lcSpots) << "DxClusterClient: attempting reconnect";
    connectToCluster(m_host, m_port, m_callsign);
}

// ── Line-buffered read ──────────────────────────────────────────────────────

// From AetherSDR src/core/DxClusterClient.cpp:149-160 [@0cd4559]
//
// Longpath refactor: pure form takes the buffer to operate on as an
// argument so the public stripTelnetIACForTest() seam can exercise the
// algorithm without a live socket. The instance method below is a thin
// wrapper that calls this on m_readBuffer. Algorithm (skip 0xFF + 2
// command bytes per IAC sequence) is byte-for-byte from upstream.
void DxClusterClient::stripTelnetIACBuffer(QByteArray& buf)
{
    // Remove telnet IAC sequences (0xFF + command byte + option byte)
    int i = 0;
    while (i < buf.size()) {
        if (static_cast<unsigned char>(buf[i]) == 0xFF && i + 2 < buf.size()) {
            buf.remove(i, 3);
        } else {
            i++;
        }
    }
}

void DxClusterClient::stripTelnetIAC()
{
    stripTelnetIACBuffer(m_readBuffer);
}

// From AetherSDR src/core/DxClusterClient.cpp:162-198 [@0cd4559]
void DxClusterClient::onReadyRead()
{
    m_readBuffer.append(m_socket->readAll());
    stripTelnetIAC();

    while (true) {
        int idx = m_readBuffer.indexOf('\n');
        if (idx < 0) {
            // No newline yet — check for login prompt (may not end with \n)
            if (!m_loggedIn) {
                QString partial = QString::fromLatin1(m_readBuffer).trimmed();
                qCInfo(lcSpots) << "DxClusterClient: partial buffer (no newline yet)"
                                << "len=" << partial.length()
                                << "tail=" << partial.right(40);
                if (isLoginPrompt(partial)) {
                    qCInfo(lcSpots) << "DxClusterClient: login prompt detected (no newline):" << partial;
                    m_readBuffer.clear();
                    m_socket->write((m_callsign + "\r\n").toLatin1());
                    markLoggedIn();
                    qCInfo(lcSpots) << "DxClusterClient: sent callsign" << m_callsign;
                }
            }
            break;
        }

        QString line = QString::fromLatin1(m_readBuffer.left(idx)).trimmed();
        m_readBuffer.remove(0, idx + 1);

        if (line.isEmpty()) {
            continue;
        }

        // Write to log file
        if (m_logFile.isOpen()) {
            m_logFile.write((line + "\n").toUtf8());
            m_logFile.flush();
        }

        emit rawLineReceived(line);
        handleLine(line);
    }
}

// From AetherSDR src/core/DxClusterClient.cpp:200-218 [@0cd4559]
void DxClusterClient::handleLine(const QString& line)
{
    // Login prompt detection (line-based)
    if (!m_loggedIn && isLoginPrompt(line)) {
        qCInfo(lcSpots) << "DxClusterClient: login prompt (line):" << line;
        m_socket->write((m_callsign + "\r\n").toLatin1());
        markLoggedIn();
        qCInfo(lcSpots) << "DxClusterClient: sent callsign" << m_callsign;
        return;
    }

    // Der Knoten sagt uns, WARUM er auflegt -- und bis zum 2026-10-28
    // fiel genau diese Zeile hier durch, weil sie kein Spot ist. Ohne
    // sie sieht der Betreiber 216 Neuverbindungen ohne einen Grund.
    // Das ist nichts, was ein Programm beheben kann: solange zwei
    // Anschluesse dasselbe Rufzeichen fuehren, werfen sie sich
    // abwechselnd hinaus. Man muss es SEHEN.
    if (istHinauswurf(line)) {
        qCWarning(lcSpots)
            << "DxClusterClient: der Knoten" << m_host
            << "hat uns hinausgeworfen -- ein anderer Anschluss hat sich"
            << "mit demselben Rufzeichen" << m_callsign << "angemeldet."
            << "Ein DXSpider-Knoten laesst nur eine Sitzung je Rufzeichen"
            << "zu; zwei Anschluesse werfen sich endlos abwechselnd"
            << "hinaus. Entweder den anderen beenden oder ihm ein"
            << "eigenes Kennzeichen geben (etwa" << m_callsign + QStringLiteral("-2") << ").";
        qCInfo(lcSpots) << "DxClusterClient: Wortlaut des Knotens:" << line;
        return;
    }

    // Try to parse as a DX spot
    DxSpot spot;
    if (parseDxSpotLine(line, spot)) {
        qCDebug(lcSpots) << "DxClusterClient: spot" << spot.dxCall
                 << spot.freqMhz << "MHz de" << spot.spotterCall;
        emit spotReceived(spot);
    }
}

// ── Login prompt detection ──────────────────────────────────────────────────

// From AetherSDR src/core/DxClusterClient.cpp:222-232 [@0cd4559]
bool DxClusterClient::isLoginPrompt(const QString& line) const
{
    // DX cluster servers vary: "login:", "call:", "callsign:", "Please enter your call",
    // "your call>", "Enter your callsign"
    QString lower = line.toLower();
    if (lower.endsWith(QStringLiteral("login:"))
        || lower.endsWith(QStringLiteral("call:"))
        || lower.endsWith(QStringLiteral("callsign:"))) {
        return true;
    }
    if (lower.contains(QStringLiteral("enter your call"))
        || lower.contains(QStringLiteral("your call"))) {
        return true;
    }
    return false;
}

// ── DX spot line parser ─────────────────────────────────────────────────────

// From AetherSDR src/core/DxClusterClient.cpp:236-260 [@0cd4559]
//
// Standard format: DX de W3LPL:     14025.0  JA1ABC       CW big signal       1824Z
// Z is the terminator — ignore any trailing chars (some nodes append BEL/NUL)
//
// Longpath divergence (source-label assignment moved into the parser):
// upstream AetherSDR left spot.source unset. The calling site set it
// based on which connection (DX cluster vs. RBN) emitted the spot.
// Longpath moves the source assignment into parseDxSpotLine() so unit
// tests that exercise the parser directly (without a live socket) see a
// fully-populated DxSpot, and so a single class instance can serve both
// the DX cluster and RBN connections (RadioModel will instantiate two
// DxClusterClient objects). Default label is "Cluster"; the parser
// promotes to "RBN" when the spotter callsign carries a Reverse Beacon
// Network marker (either the "-#" suffix Skimmer servers append or the
// "RBN-" prefix some clusters use, case-insensitive).
bool DxClusterClient::parseDxSpotLine(const QString& line, DxSpot& spot) const
{
    // Format A — classic "DX de" header (AR-Cluster, some CC-Cluster):
    //   "DX de W3LPL:     14025.0  JA1ABC       CW big signal       1824Z"
    // From AetherSDR src/core/DxClusterClient.cpp:236-260 [@0cd4559].
    static const QRegularExpression rxClassic(
        R"(^DX\s+de\s+(\S+?):\s+(\d+\.?\d*)\s+(\S+)\s+(.*?)\s+(\d{4})Z)",
        QRegularExpression::CaseInsensitiveOption);

    // Format B — DXSpider standard output (NG7M-1, many other DXSpider
    // nodes; not parsed by upstream AetherSDR):
    //   "14310.0 VK2IO/P     12-May-2026 0449Z WWFF VKFF-5514     <OH0M>"
    // Tokens: FREQ_KHZ CALL DD-MMM-YYYY HHMMZ COMMENT... <SPOTTER>
    // 2026-05-12 bench: NG7M-1 (DXSpider V1.57) is the default cluster
    // host shipped in SpotHubDialog defaults; without this fallback
    // every spot from the default cluster gets dropped on the floor
    // even though the TCP + login path works.
    static const QRegularExpression rxDxspider(
        R"(^(\d+\.?\d*)\s+(\S+)\s+\d{1,2}-\w+-\d{4}\s+(\d{4})Z\s+(.*?)\s+<(\S+)>)",
        QRegularExpression::CaseInsensitiveOption);

    QString freqStr;
    QString timeStr;

    auto match = rxClassic.match(line);
    if (match.hasMatch()) {
        spot.spotterCall = match.captured(1);
        freqStr          = match.captured(2);
        spot.dxCall      = match.captured(3);
        spot.comment     = match.captured(4).trimmed();
        timeStr          = match.captured(5);
    } else {
        match = rxDxspider.match(line);
        if (!match.hasMatch()) {
            return false;
        }
        freqStr          = match.captured(1);
        spot.dxCall      = match.captured(2);
        timeStr          = match.captured(3);
        spot.comment     = match.captured(4).trimmed();
        spot.spotterCall = match.captured(5);
    }

    const double freqKhz = freqStr.toDouble();
    spot.freqMhz = freqKhz / 1000.0;

    const int hh = timeStr.left(2).toInt();
    const int mm = timeStr.mid(2, 2).toInt();
    spot.utcTime = QTime(hh, mm);

    if (!(spot.freqMhz > 0.0 && !spot.dxCall.isEmpty())) {
        return false;
    }

    // Longpath addition: source-label assignment.
    spot.source = QStringLiteral("Cluster");
    if (spot.spotterCall.startsWith(QStringLiteral("RBN-"), Qt::CaseInsensitive)
        || spot.spotterCall.endsWith(QStringLiteral("-#"))) {
        spot.source = QStringLiteral("RBN");
    }

    return true;
}

} // namespace Longpath
