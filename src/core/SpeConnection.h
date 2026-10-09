#pragma once

// =================================================================
// src/core/SpeConnection.h  (Longpath)
// =================================================================
//
// Ported from AetherSDR (https://github.com/ten9876/AetherSDR), GPLv3,
// primary author Jeremy [KK7GWY]:
//   src/core/SpeConnection.h at d58e2b8a
//   src/core/SpeConnection.cpp at d58e2b8a
//
// AetherSDR carries no per-file licence headers, so per
// docs/attribution/HOW-TO-PORT.md rule 6 the citation is at project
// level: there is no verbatim block to copy. Both projects are GPLv3,
// so the code carries forward under the same licence per GPLv3 §5.
//
// Schritt 2 von Zeus-Punkt 7: der Transport. Schritt 1 war die reine
// Protokollschicht (core/SpeProtocol.h) -- die liest und baut Bytes und
// braucht dafuer nichts. Hier kommt dazu, was ein Geraet am anderen Ende
// noetig macht: der Abfragetakt (der SPE SPRICHT NICHT von selbst, er
// antwortet nur), das Erkennen von Stille bei stehender Leitung, das
// Wiederverbinden, und der Einschaltimpuls auf den Steuerleitungen.
//
// Longpath hat kein SPE am Kabel. Der Pruefstand
// (tst_spe_verbindung) stellt darum einen eigenen QTcpServer auf
// 127.0.0.1 hin, der die Rahmen des Verstaerkers nachspricht -- das
// prueft jede Entscheidung dieser Klasse (Takt, Stille, Kennung,
// Impulsfolge, Wiederverbinden) ohne Geraet, aber es prueft NICHT, ob
// ein echter 1.5K-FA sich so verhaelt. Was AetherSDR dazu am echten
// Geraet gemessen hat, steht an der jeweiligen Stelle und ist als
// uebernommen gekennzeichnet.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 -- Ported to Longpath by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Claude Code). Namensraum AetherSDR
//                 -> Longpath; Einbindepfade auf "core/"; LogManager.h
//                 -> eigene Kategorie Q_LOGGING_CATEGORY(lcSpe,
//                 "longpath.spe") nach dem Muster von PgxlConnection
//                 (AetherSDR schreibt in lcTuner, das es bei uns nicht
//                 gibt). Dazu drei Stellen fuer den Pruefstand, die
//                 beim Vorbild fehlen und ohne die nichts ohne Geraet
//                 pruefbar waere: pollIntervalMs()/setPollIntervalMs(),
//                 silentPollLimit()/setSilentPollLimit() und
//                 powerOnPulseScale(). Siehe die Begruendung dort.
//                 Transport, Taktlogik, Stilleerkennung, Impulsfolge
//                 und Kommentare sonst unveraendert.
// =================================================================

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTcpSocket>
#include <QTimer>

#ifdef HAVE_SERIALPORT
#include <QSerialPort>
#endif

#include "core/SpeProtocol.h"

namespace Longpath {

// Peripheral transport for an SPE Expert linear amplifier (1.3K-FA/1.5K-FA/
// 2K-FA) — a standalone USB/RS-232 device with no radio awareness at all,
// so this is a peripheral accessory alongside Rf2ksConnection/
// PgxlConnection/TgxlConnection, not a RadioConnection subclass.
//
// The wire protocol is transport-agnostic — the exact same bytes flow
// whether the peer is a local COM port or a raw-mode ser2net TCP proxy —
// so a single Spe::FrameParser instance decodes either transport. Only one
// transport is active at a time, selected by which connect method is called.
//
// Unlike the 4O3A family (which pushes telemetry continuously), the SPE only
// ever speaks when spoken to: the host polls the Status string with command
// 0x90. This class owns that poll loop and emits statusUpdated for every
// valid reply.
class SpeConnection : public QObject {
    Q_OBJECT

public:
    explicit SpeConnection(QObject* parent = nullptr);
    // Beim Vorbild gibt es keinen -- und genau daran lag ein echter
    // Fehler, siehe die Begruendung in SpeConnection.cpp.
    ~SpeConnection() override;

    bool isConnected() const { return m_connected; }
    QString description() const;  // "COM4" or "192.168.1.52:64002", for status display
    // "SERIAL" / "NETWORK" for the applet's compact source label — derived
    // from the LIVE transport, never a persisted mode setting.
    QString sourceLabel() const;

#ifdef HAVE_SERIALPORT
    // 115200 8N1, no handshake — the amplifier auto-adapts to lower speeds
    // (spec §1), so the maximum documented rate is used and not made
    // user-configurable.
    void connectSerial(const QString& portName);
#endif
    // Network mode expects a ser2net-style TCP proxy in either raw or
    // telnet mode — both verified against real 1.5K-FA hardware by
    // AetherSDR (their validation station runs telnet mode). Telnet
    // negotiation bytes and IAC-escaping are shrugged off by the parser's
    // sync-run resync; the rare status frame whose checksum byte happens to
    // be 0xFF is dropped and simply re-polled 100 ms later. Telnet mode is
    // also what the remote power-ON pulse needs — it drives the proxy's
    // DTR/RTS lines via RFC 2217 COM-port control, so ser2net must run the
    // port as `accepter: telnet(rfc2217=true),<port>`. A raw or
    // plain-telnet port still monitors and sends keystrokes fine; only
    // powerOn() is affected, and it detects and reports that case rather
    // than assuming.
    void connectNetwork(const QString& host, quint16 port);
    void disconnect();

    void setAutoReconnect(bool on) { m_autoReconnect = on; }

    // Commands (host -> amp). Each is a front-panel keystroke; the amplifier
    // echoes an ACK or replies with a Status string. No-ops when not
    // connected.
    void sendKey(Spe::Key key);
    void toggleOperate() { sendKey(Spe::Key::Operate); }
    void cyclePowerLevel() { sendKey(Spe::Key::Power); }
    void tune() { sendKey(Spe::Key::Tune); }
    void switchOff() { sendKey(Spe::Key::SwitchOff); }

    // Power the amplifier ON — a hardware pulse on the serial connector's
    // control lines, not a protocol command, so it works while the amp is
    // silent. Network mode drives the proxy's DTR/RTS via RFC 2217, which
    // needs ser2net running the port as
    // `accepter: telnet(rfc2217=true),<port>`; serial mode drives the local
    // lines directly. The pulse is always sent — a proxy that ignores
    // COM-port control just discards it — but the completion message reports
    // what the peer actually agreed to (DO / DONT / no answer at all) rather
    // than assuming it landed. The pulse sequence and timing are carried
    // from AetherSDR's field-proven reference application (see
    // Spe::Rfc2217). No-op while a pulse is already in progress.
    void powerOn();

    const Spe::Status& lastStatus() const { return m_lastStatus; }

    // Model ID from the last Status reply ("13K"/"15K"/"20K"), empty until
    // the first reply arrives. The SPE reports its identity in every Status
    // string, so there is no auto-ranging or detection heuristic here at all.
    QString currentModelId() const { return m_currentModelId; }

    bool isResponding() const { return m_responding; }

    // ── Vier Griffe, die es beim Vorbild nicht gibt ──────────────────
    //
    // AetherSDR hat die vier Zahlen unten fest verdrahtet (drei als
    // `static constexpr` im privaten Teil, die 5000 ms des
    // Wiederverbinders direkt im Konstruktor). Das ist sauber, solange ein Geraet am Kabel haengt
    // und man zuschauen kann. Hier haengt keines, also muss der
    // Pruefstand die Zeit selbst in die Hand nehmen: 30 unbeantwortete
    // Abfragen bei 100 ms sind drei Sekunden, und der Einschaltimpuls
    // dauert ueber anderthalb. Ein Pruefstand, der das in Echtzeit
    // abwartet, laeuft entweder ewig oder er wird weggelassen -- und
    // dann ist genau die Stilleerkennung ungeprueft, also der eine
    // Zweig, der ohne ser2net-Aufbau gar nicht auffaellt.
    //
    // Darum sind es Felder statt Konstanten, mit den Vorgaben des
    // Vorbilds als Anfangswert. Im Betrieb ruft sie niemand; wer sie
    // ruft, muss es VOR connect() tun.
    int  pollIntervalMs() const { return m_pollIntervalMs; }
    void setPollIntervalMs(int ms);
    int  reconnectIntervalMs() const { return m_reconnectTimer.interval(); }
    void setReconnectIntervalMs(int ms);
    int  silentPollLimit() const { return m_silentPollLimit; }
    void setSilentPollLimit(int polls);
    // Streckt oder staucht NUR die drei Wartezeiten der Impulsfolge
    // (500/100/1000 ms). 1.0 ist der Betrieb; der Pruefstand nimmt 0.02
    // und sieht dieselbe Reihenfolge in 32 ms statt in 1,6 s.
    double powerOnPulseScale() const { return m_powerOnPulseScale; }
    void   setPowerOnPulseScale(double scale);

signals:
    void connected();
    void disconnected();
    void connectionFailed(const QString& errorString);
    void statusUpdated(const Longpath::Spe::Status& status);
    // Fires on the first Status reply of a connection and again if the
    // reported ID ever changes (in practice: never mid-session). The GUI
    // applies gauge ranges and model-dependent layout from this.
    void modelChanged(const QString& modelId);
    // The transport is up but the amplifier has stopped answering polls
    // (or resumed). With ser2net the TCP link outlives the amplifier being
    // switched off, so this — not disconnected() — is the "amp went away"
    // signal for that topology.
    void respondingChanged(bool responding);
    // Longpath-Zusatz: das Ende der Impulsfolge, mit dem, was der
    // Vermittler zugesagt hat. Beim Vorbild steht das nur im Protokoll;
    // ein Pruefstand ohne Geraet hat daran aber die einzige Handhabe,
    // und das Bedienfeld kann es spaeter anzeigen statt es dem Betreiber
    // im Protokoll suchen zu lassen.
    void powerOnPulseFinished(Longpath::Spe::Rfc2217::OptionReply confirmed);

private slots:
    void onReadyRead();

private:
    enum class Mode { None, Serial, Network };

    void onTransportUp();
    void onTransportDown();
    void onTransportError(const QString& errorString);
    void onFrameReceived(const Spe::Frame& frame);
    void teardownDevice();
    void sendRaw(const QByteArray& packet);
    void armReconnect();
    void pollTick();
    void powerOnStep();
    void setControlLines(bool dtr, bool rts);  // transport-appropriate DTR/RTS
    void startPowerOnTimer(int baseMs);

    QIODevice*    m_device{nullptr};
    QTcpSocket    m_socket;
#ifdef HAVE_SERIALPORT
    QSerialPort*  m_serialPort{nullptr};
#endif

    Spe::FrameParser m_parser;
    Spe::Status      m_lastStatus;

    Mode      m_mode{Mode::None};
    QString   m_lastSerialPort;
    QString   m_lastHost;
    quint16   m_lastPort{0};

    bool m_connected{false};
    bool m_autoReconnect{false};
    bool m_deliberateDisconnect{false};

    QTimer m_reconnectTimer;
    // Status poll loop — the spec allows "several times every second". 10/s
    // matches the applets' own 10 Hz readout refresh, so polling faster
    // would only burn link bandwidth on frames the GUI never renders;
    // AetherSDR's field-proven reference application polled at 300 ms and
    // its bar visibly stair-stepped, which this rate fixes.
    QTimer m_pollTimer;
    static constexpr int kDefaultPollIntervalMs = 100;
    int m_pollIntervalMs{kDefaultPollIntervalMs};

    QString m_currentModelId;

    // Power-ON pulse state machine (see powerOn()). -1 = idle.
    QTimer m_powerOnTimer;
    int    m_powerOnStep{-1};
    double m_powerOnPulseScale{1.0};
    // Whether the network peer accepted RFC 2217 COM-port control, learned
    // from its reply to WILL COM-PORT-OPTION. Network mode only; a local
    // serial port drives its own lines and needs no negotiation.
    Spe::Rfc2217::OptionReply m_comPortOption{Spe::Rfc2217::OptionReply::None};
    // Last 2 bytes of the previous network read — prepended to the next scan
    // so a DO/DONT reply split across TCP segments is still seen (the
    // sequence is 3 bytes, so 2 carried bytes always suffice).
    QByteArray m_rfc2217Tail;

    // Responding/silent tracking: a poll counts as unanswered if no Status
    // frame arrived since the previous tick. A few misses are tolerated
    // (serial latency, a busy amp, a marginal link) before flagging silence.
    bool m_statusSeenSinceTick{false};
    int  m_silentPolls{0};
    bool m_responding{false};
    static constexpr int kDefaultSilentPollLimit = 30;  // ~3 s at the 100 ms cadence
    int m_silentPollLimit{kDefaultSilentPollLimit};
};

}  // namespace Longpath
