#pragma once

// =================================================================
// src/core/RotctldProcess.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
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
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
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
    bool start(int hamlibModel, const QString& device, int baud,
               quint16 listenPort, QString* error);

    void stop();

signals:
    // rotctld stopped on its own. Carries whatever it wrote to stderr,
    // which is where Hamlib puts the reason — a wrong model number or a
    // serial port that is not there both come out here and nowhere
    // else.
    void exited(int exitCode, const QString& stderrText);

private:
    QProcess m_proc;
};

} // namespace Longpath
