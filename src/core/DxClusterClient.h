// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath - DX cluster telnet client (DX Spider / AR-Cluster / CC-Cluster)
//
// Ported from AetherSDR src/core/DxClusterClient.h [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (Longpath):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task B3. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "Longpath". DxSpot include
//                                    moved to the extracted DxSpot.h
//                                    (Phase 3J-2 Task B1) instead of
//                                    redefining DxSpot inline (upstream
//                                    DxClusterClient.h:13-23 defined it).
//                                    Logging routes through lcSpots
//                                    ("longpath.spots") instead of upstream
//                                    AetherSDR's lcDxCluster. Log file
//                                    path uses Qt's AppConfigLocation
//                                    (already lands under Longpath/)
//                                    instead of upstream's
//                                    GenericConfigLocation +
//                                    "AetherSDR/dxcluster.log". Added
//                                    public test seams
//                                    parseDxSpotLineForTest(),
//                                    isLoginPromptForTest(),
//                                    stripTelnetIACForTest() so unit
//                                    tests can validate the parser, login
//                                    prompt detector, and telnet IAC
//                                    stripper without instantiating a
//                                    QTcpSocket or simulating a telnet
//                                    server. Longpath addition: source-
//                                    label assignment in parseDxSpotLine
//                                    defaults to "Cluster" and promotes
//                                    to "RBN" when the spotter callsign
//                                    starts with "RBN-" (case-insensitive)
//                                    or ends with "-#". This lets a
//                                    single class instance serve both the
//                                    DX cluster and RBN connections
//                                    (RadioModel instantiates two
//                                    DxClusterClient objects for the two
//                                    sources). AI tooling: Anthropic
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
//                                    verworfen. KI-gestuetzt ueber
//                                    Anthropic Claude Code.

#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QFile>
#include <QString>
#include <QTime>
#include <QByteArray>
#include <atomic>

#include "DxSpot.h"

namespace Longpath {

// From AetherSDR src/core/DxClusterClient.h:25-83 [@0cd4559]
//
// Telnet client for DX cluster nodes (DX Spider, AR-Cluster, CC Cluster).
// Connects, logs in with callsign, parses "DX de" spot lines, and emits
// spotReceived() for each parsed spot.
class DxClusterClient : public QObject {
    Q_OBJECT

public:
    explicit DxClusterClient(QObject* parent = nullptr);
    ~DxClusterClient() override;

    void connectToCluster(const QString& host, quint16 port, const QString& callsign);
    void disconnect();
    bool isConnected() const { return m_connected; }

    void sendCommand(const QString& cmd);

    QString host() const { return m_host; }
    quint16 port() const { return m_port; }
    QString logFilePath() const;
    void setLogFileName(const QString& name) { m_logFileName = name; }

    // Public test seams. Same body as the private impls; exists so unit
    // tests can validate the regex, login-prompt detector, and telnet IAC
    // stripper without instantiating a QTcpSocket or simulating a telnet
    // server.
    bool parseDxSpotLineForTest(const QString& line, DxSpot& spot) const {
        return parseDxSpotLine(line, spot);
    }
    bool isLoginPromptForTest(const QString& line) const {
        return isLoginPrompt(line);
    }
    // Wie weit die Treppe des Wiederanlaufs gestiegen ist. Ohne diesen
    // Blick laesst sich nicht pruefen, ob sie ueberhaupt steigt -- und
    // genau das tat sie bis zum 2026-10-14 nicht.
    int reconnectAttemptsForTest() const { return m_reconnectAttempts; }
    void markLoggedInForTest() { markLoggedIn(); }
    // Die Sitzung hat die Standzeit ueberlebt. Erst DAS setzt die
    // Treppe zurueck -- siehe die Begruendung an StableSessionMs.
    void markSessionStableForTest() { onSessionStable(); }
    // Erkennt die Hinauswurf-Zeile eines DXSpider-Knotens. Rein, damit
    // sie ohne Steckdose und ohne Knoten zu pruefen ist.
    static bool istHinauswurfForTest(const QString& line) {
        return istHinauswurf(line);
    }
    void stripTelnetIACForTest(QByteArray& buf) const {
        stripTelnetIACBuffer(buf);
    }

signals:
    void connected();
    void disconnected();
    void connectionError(const QString& error);
    void spotReceived(const DxSpot& spot);
    void rawLineReceived(const QString& line);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError err);
    void onReconnectTimer();
    void onSessionStable();

private:
    // Die Anmeldung ist durch -- erst hier faellt der Wiederanlauf-Zaehler.
    void markLoggedIn();

    bool parseDxSpotLine(const QString& line, DxSpot& spot) const;
    bool isLoginPrompt(const QString& line) const;
    void handleLine(const QString& line);
    void stripTelnetIAC();

    // Longpath addition: pure form of stripTelnetIAC() that operates on
    // an externally-supplied buffer. The instance method stripTelnetIAC()
    // is a thin wrapper that calls this on m_readBuffer. Existing as a
    // standalone helper makes the algorithm unit-testable without a live
    // socket.
    static void stripTelnetIACBuffer(QByteArray& buf);

    // ── „Ein anderer hat sich mit deinem Rufzeichen angemeldet" ──────
    //
    // DXSpider laesst genau EINE Sitzung je Rufzeichen zu. Meldet sich
    // ein zweiter Anschluss an, schickt der Knoten dem aelteren
    //
    //   „Reconnected as OE5SOS at 84.113.40.38, this instance is
    //    disconnected"
    //
    // und legt auf. Am 2026-10-28 am Knoten dxc.nc7j.com:7300 live
    // mitgeschrieben. Bis dahin fiel diese Zeile durch handleLine()
    // hindurch -- sie ist kein Spot, also wurde sie stillschweigend
    // verworfen, und der Betreiber sah 216 Neuverbindungen ohne einen
    // Grund dafuer.
    static bool istHinauswurf(const QString& line);

    QTcpSocket* m_socket;
    QByteArray  m_readBuffer;
    QTimer*     m_reconnectTimer;
    QTimer*     m_stableTimer;
    QFile       m_logFile;

    QString m_logFileName{"dxcluster.log"};
    QString m_host;
    quint16 m_port{7300};
    QString m_callsign;
    std::atomic<bool> m_connected{false};
    bool    m_loggedIn{false};
    // Wiederholte Fehlschlaege zusammenfassen statt jeden zu schreiben.
    QString m_lastFailureMsg;
    qint64  m_lastFailureLogMs{0};
    int     m_quietFailures{0};
    bool    m_intentionalDisconnect{false};
    int     m_reconnectAttempts{0};

    static constexpr int MaxReconnectDelayMs    = 60000;
    static constexpr int InitialReconnectDelayMs = 5000;
    static constexpr int ConnectTimeoutMs        = 10000;

    // ── Wann eine Sitzung als brauchbar gilt ─────────────────────────
    //
    // Am 2026-10-14 stand dasselbe schon einmal im Protokoll: 680
    // Versuche, alle 14 s, „The remote host closed the connection", und
    // die Treppe erreichte nie Stufe 2. Damals setzte onConnected()
    // zurueck, und die Behebung schob den Punkt auf markLoggedIn().
    //
    // Am 2026-10-28 war es wieder da: 216 Versuche in 223 min, wieder
    // alle 14 s. Der Grund ist derselbe Fehler eine Station weiter --
    // die ANMELDUNG gelingt jedes Mal. Der Knoten nimmt uns an und
    // wirft uns 8,8 s spaeter hinaus (gemessener Median), also fiel der
    // Zaehler in JEDEM Zyklus auf 0 und die Wartezeit blieb bei 5 s.
    //
    // Eine Anmeldung, die neun Sekunden haelt, ist kein Erfolg. Der
    // Zaehler faellt deshalb erst, wenn die Sitzung eine Minute
    // ueberlebt hat -- deutlich mehr als die gemessenen 8,8 s und
    // gleich dem Deckel der Treppe.
    static constexpr int StableSessionMs         = 60000;
};

} // namespace Longpath
