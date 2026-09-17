#pragma once

// =================================================================
// src/core/RotctldProcess.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Starts Hamlib's rotctld so the operator does not have to.
//
// The rotator link speaks to rotctld over TCP, which is the right
// design — it is one protocol for sixty controllers, it works over the
// network, and it is somebody else's job to keep up with new hardware.
// But it made the installation instructions "open a terminal and run
// rotctld -m 603 -r /dev/tty.usbserial-1410 -T 0.0.0.0", which is a
// reasonable thing to ask of a developer and not of someone who wants
// to point an antenna.
//
// So: pick the controller from a list, pick the port, press Connect.
// This starts rotctld on the loopback interface, and stops it again on
// the way out. An operator who already runs rotctld their own way is
// not affected — that path is still there and still preferred when it
// is already running.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-09-16 — Takes a free port when the preferred one is held (a
//                 rotctld orphaned by an earlier crash was sitting on
//                 4533 with the wrong device, live); restart() for a
//                 daemon whose controller link has died; exited() no
//                 longer fires for a stop this object asked for.
//                 AI-assisted via Anthropic Claude (Claude Code),
//                 operator Martin Fischer.
// =================================================================

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace Longpath {

class RotctldProcess : public QObject {
    Q_OBJECT
public:
    explicit RotctldProcess(QObject* parent = nullptr);
    ~RotctldProcess() override;

    // Where rotctld lives, or empty if it cannot be found.
    //
    // PATH alone is not enough: a GUI application launched from Finder
    // inherits a PATH without /opt/homebrew/bin, so Hamlib installed
    // with brew is invisible to it while being perfectly present in the
    // operator's terminal. That discrepancy would read as "the software
    // cannot find something I can see", which is worse than a plain
    // absence.
    static QString findBinary();

    // The command line, built where it can be checked. Public because
    // the dialog shows it to the operator: a person who can see the
    // exact command can run it by hand when the automatic path fails,
    // and can paste it into a bug report.
    static QStringList arguments(int hamlibModel, const QString& device,
                                 int baud, quint16 listenPort);

    bool isRunning() const;

    // Start rotctld. Returns false and fills `error` if the binary is
    // missing or the process refuses to start; a rotctld that starts
    // and then exits reports through exited() instead, because that
    // failure arrives later.
    //
    // `listenPort` is the port to prefer. If something already holds
    // it, a free one is taken instead — read listenPort() afterwards
    // for the port actually in use. 2026-09-16: found live — a rotctld
    // from an earlier, crashed session was still sitting on 4533 with
    // the wrong device; the new rotctld could not bind, exited at once,
    // and the client then happily talked to the stale one, which never
    // answered. Sidestepping the port is the fix that needs nothing
    // from the operator; killing strangers' processes is not ours to do.
    bool start(int hamlibModel, const QString& device, int baud,
               quint16 listenPort, QString* error);

    // The port the running (or last started) rotctld listens on.
    quint16 listenPort() const { return m_listenPort; }

    void stop();

    // Everything start() needs, kept so the owner can bounce the daemon
    // without re-collecting it: a rotctld whose link to the controller
    // has died (the ARCO drops a silent GS-232A session after ~20 s)
    // never recovers on its own and has to be started afresh.
    bool restart(QString* error);

signals:
    // rotctld stopped on its own. Carries whatever it wrote to stderr,
    // which is where Hamlib puts the reason — a wrong model number or a
    // serial port that is not there both come out here and nowhere
    // else. Not emitted for a stop() this object asked for.
    void exited(int exitCode, const QString& stderrText);

private:
    QProcess m_proc;
    quint16  m_listenPort{0};
    bool     m_stopRequested{false};

    int     m_model{0};
    QString m_device;
    int     m_baud{0};
    quint16 m_preferredPort{0};
};

} // namespace Longpath
