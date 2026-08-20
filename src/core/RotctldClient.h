#pragma once

// =================================================================
// src/core/RotctldClient.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Hamlib's rotctld, over TCP. The protocol is line-based text:
//
//   p            → "145.000000\n0.000000\n"     azimuth, elevation
//   P az el      → "RPRT 0\n"                    0 means accepted
//   S            → "RPRT 0\n"                    stop
//
// A negative RPRT is an error code, not a position. Everything here
// turns on telling those two apart, so the parsing is static and
// tested against a rotctld stand-in rather than against a rotator.
//
// rotctld answers one command at a time, so commands queue. Sending a
// second while the first is outstanding gets the replies crossed, and
// a crossed reply is a position read as an error code or the reverse.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-08-10 — Reply watchdog: an outstanding command that never gets
//                 an answer used to leave m_awaiting set forever — the
//                 poll timer's "one command at a time" guard then never
//                 passed again, and a rotctld that froze without closing
//                 the socket silently stopped the whole client. Now the
//                 connection is cut after kReplyTimeoutMs and the
//                 existing auto-reconnect takes over. Elevation is also
//                 kept instead of discarded, and moveTo() sends the last
//                 reported elevation rather than forcing 0 — an az/el
//                 rotator no longer has its elevation slammed down by
//                 every azimuth command. AI-assisted via Anthropic
//                 Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "RotorController.h"

#include <QByteArray>
#include <QDateTime>
#include <QQueue>
#include <QTcpSocket>

class QTimer;

namespace Longpath {

class RotctldClient : public RotorController {
    Q_OBJECT
public:
    explicit RotctldClient(QObject* parent = nullptr);

    void setTarget(const QString& host, quint16 port);
    QString host() const { return m_host; }
    quint16 port() const { return m_port; }

    // How often to ask where it is. Half a second is well under the
    // time a rotator takes to move a degree, and cheap on a LAN.
    void setPollIntervalMs(int ms);

    // A position older than this is not steered by. Three poll periods:
    // one missed reply is a hiccup, three is a rotator that has gone
    // quiet.
    static constexpr int kStaleAfterMs = 1500;

    // How long an outstanding command may wait for its reply before the
    // link is declared dead and cut. Generous next to kStaleAfterMs on
    // purpose: this is the last resort for a daemon that froze without
    // closing the socket, not a latency budget. Cutting the connection
    // hands recovery to the auto-reconnect that already exists.
    static constexpr int kReplyTimeoutMs = 4000;

    QString description() const override;
    State state() const override { return m_state; }
    double azimuth() const override { return m_azimuth; }
    bool hasFreshPosition() const override;

    // Last reported elevation, degrees. 0 for an azimuth-only rotator —
    // rotctld reports 0 for those, so the two cases are one case.
    double elevation() const { return m_elevation; }

    void connectToRotor() override;
    void disconnectFromRotor() override;
    void moveTo(double azimuthDeg) override;
    void stop() override;

    // ── Protocol, as pure functions ──────────────────────────────────

    // "145.000000\n0.000000\n" → azimuth 145, elevation 0.
    // Returns false for anything that is not two numbers, including an
    // RPRT line, which is the case that must never be read as a
    // position: "RPRT -1" would otherwise become -1 degrees.
    static bool parsePosition(const QByteArray& reply,
                              double& azDeg, double& elDeg);

    // "RPRT 0" → 0. "RPRT -1" → -1. Returns false if not an RPRT line.
    static bool parseReport(const QByteArray& reply, int& code);

    // Hamlib's error numbers, as something an operator can read.
    static QString describeReport(int code);

    // The command that asks a rotator to point somewhere. Formatted
    // here so the number format is one decision: rotctld wants plain
    // decimals, and a locale that writes 145,0 would be rejected.
    static QByteArray moveCommand(double azimuthDeg, double elevationDeg = 0.0);

signals:
    // The elevation from the same position report that fed
    // positionChanged. A separate signal rather than a wider
    // positionChanged: RotorController's contract stays azimuth-only,
    // and every existing connect keeps compiling.
    void elevationChanged(double elevationDeg);

private:
    enum class Pending { None, Position, Report };

    void send(const QByteArray& command, Pending expect);
    void pump();                       // start the next queued command
    void onReadyRead();
    void setState(State s);
    void fail(const QString& why);

    struct Command {
        QByteArray bytes;
        Pending    expect;
    };

    QTcpSocket m_socket;
    QString    m_host;
    quint16    m_port{4533};           // rotctld's default

    QQueue<Command> m_queue;
    Pending    m_awaiting{Pending::None};
    QByteArray m_buffer;

    QTimer* m_poll{nullptr};
    QTimer* m_retry{nullptr};
    QTimer* m_deadline{nullptr};       // reply watchdog, see kReplyTimeoutMs
    int     m_pollMs{500};

    State     m_state{State::Disconnected};
    double    m_azimuth{0.0};
    double    m_elevation{0.0};
    double    m_commanded{0.0};
    bool      m_haveCommanded{false};
    QDateTime m_lastPosition;
};

} // namespace Longpath
