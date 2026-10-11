// =================================================================
// src/core/RigctldServer.h  (Longpath)
// =================================================================
//
// Longpath-original. Phase 3K, first step: speak Hamlib's rigctld
// network protocol so logging, contest and digital-mode programs can
// control Longpath the way they control every other radio.
//
// This is NOT a port of Hamlib. Nothing is copied from it. What the
// protocol looks like was read off the wire from a real client:
// `rigctl -m 2` against `rigctld -m 1`, through a logging proxy, on
// 2026-10-11 with Hamlib 4.7.2. The transcript is in
// docs/architecture/2026-10-11-rigctld-verification/README.md and it is
// what the dump_state block and the connect sequence below follow.
//
// Deliberately NOT here yet:
//   - The Kenwood-style CAT command set (ZZ…/FA…/IF…) over TCP and
//     serial, which Thetis also offers. rigctld covers the clients the
//     operator actually uses (N1MM, DXLog, WSJT-X all speak it), so it
//     comes first; CAT proper is the next step of the same phase.
//   - Anything that keys the transmitter without being asked. See
//     setPttAllowed below: PTT is refused unless it is turned on, and
//     the request then takes the same MOX path as every other transmit
//     source, with BandPlanGuard still in front of it.
//
// AI tooling: Anthropic Claude Code.

#pragma once

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QTcpServer>

class QTcpSocket;

namespace Longpath {

// RigctldServer — a rigctld-compatible TCP server in front of RadioModel.
//
// The radio is held as a plain QObject* and reached through
// QMetaObject::invokeMethod against the same Q_INVOKABLE shims
// TciProtocol uses. That keeps this class testable against a mock and
// keeps it out of RadioModel's header.
class RigctldServer : public QObject {
    Q_OBJECT

public:
    explicit RigctldServer(QObject* radio, QObject* parent = nullptr);
    ~RigctldServer() override;

    /// Bind and listen. Returns false (and leaves the server stopped) if
    /// the port is taken, which is the ordinary case when a real rigctld
    /// is already running on 4532.
    bool start(const QHostAddress& address, quint16 port);
    void stop();

    bool     isListening() const;
    quint16  port() const;
    int      clientCount() const;

    /// Transmit gate. Off by default and off after every stop(): a
    /// program that can set the frequency must not be able to key the
    /// radio because someone forgot a checkbox. With it off, `T 1`
    /// answers RPRT -1 and nothing reaches the MOX path.
    void setPttAllowed(bool on);
    bool pttAllowed() const;

    /// Protocol seam. Takes one command line without its newline and
    /// returns everything to send back, newline-terminated (possibly
    /// several lines, as dump_state is). Returns a null QString when the
    /// client is to be disconnected without a further reply.
    /// Public so the tests drive the protocol without a socket.
    QString handleLine(const QString& line);

signals:
    void listeningChanged(bool listening);
    void clientCountChanged(int count);

    /// One line per exchange, for the Setup page's log view.
    void commandLogged(const QString& command, const QString& reply);

    /// Emitted when a client asked to key the radio while pttAllowed()
    /// was false, so the operator can see why their logger's PTT does
    /// nothing instead of guessing.
    void pttRefused();

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    // Individual command handlers. Each returns the reply text.
    QString handleGetFreq();
    QString handleSetFreq(const QStringList& args);
    QString handleGetMode();
    QString handleSetMode(const QStringList& args);
    QString handleGetVfo();
    QString handleSetVfo(const QStringList& args);
    QString handleGetPtt();
    QString handleSetPtt(const QStringList& args);
    QString handleGetSplit();
    QString handleSetSplit(const QStringList& args);
    QString handleGetLevel(const QStringList& args);
    QString dumpState() const;

    // Hamlib mode token <-> Longpath mode name.
    static QString  hamlibModeFromLongpath(const QString& longpathMode);
    static QString  longpathModeFromHamlib(const QString& hamlibMode);

    QObject*    m_radio{nullptr};
    QTcpServer  m_server;
    QList<QTcpSocket*> m_clients;
    bool        m_pttAllowed{false};
};

}  // namespace Longpath
