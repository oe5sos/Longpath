// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath - WSJT-X UDP multicast client (binary protocol)
//
// Ported from AetherSDR src/core/WsjtxClient.h [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (Longpath):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task B4. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "Longpath". DxSpot include
//                                    moved to the extracted DxSpot.h
//                                    (Phase 3J-2 Task B1) instead of
//                                    upstream's transitive include from
//                                    DxClusterClient.h. Logging routes
//                                    through lcSpots ("longpath.spots")
//                                    instead of upstream AetherSDR's
//                                    lcDxCluster. Log file path uses Qt's
//                                    AppConfigLocation (already lands
//                                    under Longpath/) instead of
//                                    upstream's GenericConfigLocation +
//                                    "AetherSDR/wsjtx.log". Added three
//                                    public test seams
//                                    processPacketForTest(),
//                                    setDialFreqForTest(), and
//                                    extractCallsignForTest() so unit
//                                    tests can drive the binary parser,
//                                    seed dial-freq state, and validate
//                                    the WSJT-X callsign extractor without
//                                    instantiating a QUdpSocket or
//                                    simulating a multicast sender.
//                                    AI tooling: Anthropic Claude Code.
//   2026-08-10  Martin Fischer      Logged-ADIF message (type 12)
//                                    parsed: when WSJT-X logs a QSO it
//                                    broadcasts the finished ADIF
//                                    record, and qsoLogged() hands it
//                                    on as a LogEntry so the logbook
//                                    fills itself during an FT8
//                                    session. AI tooling: Anthropic
//                                    Claude (Cowork).
//   2026-09-21  Martin Fischer      Dial frequency kept PER WSJT-X
//                                    INSTANCE (AetherSDR #3595, fix
//                                    ae15dd7e): two instances on one
//                                    UDP port put one band's decodes on
//                                    the other band's panadapter when a
//                                    single "last dial seen" was used.
//                                    WsjtxDialTracker ported from
//                                    AetherSDR src/core/WsjtxDialTracker.h
//                                    [@ae15dd7e] into this header; Close
//                                    (type 6) forgets the instance. AI
//                                    tooling: Anthropic Claude.

#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QFile>
#include <QHash>
#include <QString>
#include <atomic>
#include <optional>

#include "DxSpot.h"
#include "models/LogEntry.h"

namespace Longpath {

// From AetherSDR src/core/WsjtxDialTracker.h:9-64 [@ae15dd7e]
//
// Per-instance dial-frequency memory for the WSJT-X UDP feed (#3595).
//
// A WSJT-X Decode datagram carries only the audio offset of the decoded
// signal (0-5000 Hz); the dial frequency it must be added to arrives
// separately, in that instance's Status datagram. Every WSJT-X message
// begins with the instance `id` ("WSJT-X", "WSJT-X - 2", ...), and two
// instances sharing one UDP port interleave their traffic freely, so a
// single "last dial frequency seen" is wrong whenever more than one
// instance is running: a decode from the 40 m instance added to the 20 m
// instance's dial paints a 40 m station on the 20 m panadapter.
//
// This keeps one dial frequency per instance id. A decode whose instance
// has not yet reported a dial frequency cannot be placed on any band and
// is refused (nullopt) rather than guessed -- WSJT-X emits a Status with
// every decode cycle, so in practice the only unplaceable decode is the
// first one after Longpath starts listening mid-cycle.
class WsjtxDialTracker {
public:
    // Record the dial frequency `id` reported in its Status message.
    // Non-positive frequencies are ignored: WSJT-X reports 0 Hz while it has
    // no rig connection, and a 0 Hz dial would place every decode at the
    // audio offset itself.
    void noteStatus(const QString& id, double dialFreqHz)
    {
        if (dialFreqHz <= 0.0) {
            return;
        }
        m_dialFreqHzById.insert(id, dialFreqHz);
    }

    // The dial frequency to add to a Decode from `id`, or nullopt when that
    // instance has not reported one yet.
    std::optional<double> dialFreqHzFor(const QString& id) const
    {
        const auto it = m_dialFreqHzById.constFind(id);
        if (it == m_dialFreqHzById.constEnd()) {
            return std::nullopt;
        }
        return it.value();
    }

    // WSJT-X sends a Close (type 6) datagram on exit; forgetting the id then
    // keeps a relaunched instance from inheriting a stale band until its
    // first Status arrives.
    void forget(const QString& id) { m_dialFreqHzById.remove(id); }

    void clear() { m_dialFreqHzById.clear(); }

    int instanceCount() const { return static_cast<int>(m_dialFreqHzById.size()); }

private:
    QHash<QString, double> m_dialFreqHzById;
};

// From AetherSDR src/core/WsjtxClient.h:11-61 [@0cd4559]
//
// WSJT-X UDP multicast client - listens for Decode messages (type 2)
// from WSJT-X and emits spotReceived() for each decoded station.
// Protocol: binary QDataStream on 224.0.0.1:2237 (default).
class WsjtxClient : public QObject {
    Q_OBJECT

public:
    explicit WsjtxClient(QObject* parent = nullptr);
    ~WsjtxClient() override;

    void startListening(const QString& address, quint16 port);
    void stopListening();
    bool isListening() const { return m_listening; }

    QString logFilePath() const;

    // Public test seams. Same bodies as the private impls; exist so unit
    // tests can drive the binary parser, seed dial-freq state, and
    // validate the WSJT-X callsign extractor without instantiating a
    // QUdpSocket or simulating a multicast sender.
    void processPacketForTest(const QByteArray& data) {
        parseMessage(data);
    }
    // Seeds the dial of the default instance ("WSJT-X"); a second
    // instance is seeded with its id.
    void setDialFreqForTest(double dialFreqHz, const QString& mode,
                            const QString& id = QStringLiteral("WSJT-X")) {
        m_dialTracker.noteStatus(id, dialFreqHz);
        m_mode = mode;
    }
    const WsjtxDialTracker& dialTrackerForTest() const { return m_dialTracker; }
    QString extractCallsignForTest(const QString& message) const {
        return extractCallsign(message);
    }

signals:
    void listening();
    void stopped();
    void spotReceived(const DxSpot& spot);
    void rawLineReceived(const QString& line);
    void statusReceived(const QString& id, double dialFreqHz, const QString& mode);
    // WSJT-X logged a contact (message type 12, "Logged ADIF"). The
    // entry is the parsed ADIF record WSJT-X broadcast — band, mode,
    // reports and grid included.
    void qsoLogged(const Longpath::LogEntry& entry);

private slots:
    void onReadyRead();

private:
    static constexpr quint32 WsjtxMagic = 0xadbccbda;

    // QDataStream helpers - parse big-endian Qt-serialized types
    static bool readQString(QDataStream& ds, QString& out);
    static bool readBool(QDataStream& ds, bool& out);

    void parseMessage(const QByteArray& data);
    void parseStatus(QDataStream& ds);
    void parseDecode(QDataStream& ds);
    void parseClose(QDataStream& ds);
    void parseLoggedAdif(QDataStream& ds);
    QString extractCallsign(const QString& message) const;

    QUdpSocket* m_socket;
    QFile       m_logFile;
    QHostAddress m_bindAddr;
    bool        m_isMulticast{false};
    quint16     m_port{2237};
    std::atomic<bool> m_listening{false};

    // Dial frequency from Status messages (type 1), kept PER INSTANCE ID
    // so two WSJT-X instances sharing this port each place their decodes
    // on their own band (#3595) -- see WsjtxDialTracker above.
    WsjtxDialTracker m_dialTracker;
    QString m_mode;
};

} // namespace Longpath
