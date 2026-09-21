// =================================================================
// src/core/RotctldProcess.cpp  (Longpath)
// =================================================================
//
// Longpath-original — see RotctldProcess.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "RotctldProcess.h"

#include <QFileInfo>
#include <QHostAddress>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QTcpServer>

namespace Longpath {

namespace {

Q_LOGGING_CATEGORY(lcRotctld, "longpath.rotctld")

// True if nothing on this machine is listening on loopback:port. A
// bind that succeeds is released again at once; rotctld binds it for
// real a moment later.
bool loopbackPortIsFree(quint16 port)
{
    QTcpServer probe;
    return probe.listen(QHostAddress::LocalHost, port);
}

// A port the kernel says is free right now.
quint16 anyFreeLoopbackPort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0)) { return 0; }
    return probe.serverPort();
}

} // namespace

RotctldProcess::RotctldProcess(QObject* parent) : QObject(parent)
{
    connect(&m_proc, &QProcess::finished, this,
            [this](int code, QProcess::ExitStatus) {
        const QString err =
            QString::fromLocal8Bit(m_proc.readAllStandardError()).trimmed();
        if (m_stopRequested) {
            m_stopRequested = false;
            return;
        }
        qCWarning(lcRotctld) << "rotctld exited on its own, code" << code
                             << err;
        emit exited(code, err);
    });
}

RotctldProcess::~RotctldProcess()
{
    stop();
}

QString RotctldProcess::findBinary()
{
    // PATH first: an operator who installed Hamlib somewhere unusual
    // and put it on PATH has already answered this question.
    const QString onPath = QStandardPaths::findExecutable(
        QStringLiteral("rotctld"));
    if (!onPath.isEmpty()) { return onPath; }

    // Then the places package managers actually put it. A GUI launched
    // from Finder does not inherit the shell's PATH, so Homebrew's
    // directories have to be named explicitly or Hamlib is invisible to
    // this program while being plainly present in the terminal.
    static const QStringList kDirs = {
        QStringLiteral("/opt/homebrew/bin"),   // Apple silicon Homebrew
        QStringLiteral("/usr/local/bin"),      // Intel Homebrew, and most else
        QStringLiteral("/opt/local/bin"),      // MacPorts
        QStringLiteral("/usr/bin"),
    };
    for (const QString& dir : kDirs) {
        const QString candidate = dir + QStringLiteral("/rotctld");
        if (QFileInfo(candidate).isExecutable()) { return candidate; }
    }
    return {};
}

QStringList RotctldProcess::arguments(int hamlibModel, const QString& device,
                                      int baud, quint16 listenPort)
{
    QStringList args;
    args << QStringLiteral("-m") << QString::number(hamlibModel);

    // A network model such as Ether6 takes an address where a serial
    // model takes a device node; either way it is -r, and either way
    // an empty one means "let Hamlib use its default".
    if (!device.trimmed().isEmpty()) {
        args << QStringLiteral("-r") << device.trimmed();
    }
    if (baud > 0) {
        args << QStringLiteral("-s") << QString::number(baud);
    }

    // Loopback only. rotctld has no authentication of any kind, and a
    // rotator that anyone on the network can turn is a rotator that
    // will eventually be turned by someone else. An operator who wants
    // it reachable from another machine can run rotctld themselves and
    // point Longpath at it — that is a decision worth making
    // deliberately.
    args << QStringLiteral("-T") << QStringLiteral("127.0.0.1")
         << QStringLiteral("-t") << QString::number(listenPort);
    return args;
}

bool RotctldProcess::isRunning() const
{
    return m_proc.state() != QProcess::NotRunning;
}

bool RotctldProcess::start(int hamlibModel, const QString& device, int baud,
                           quint16 listenPort, QString* error)
{
    if (isRunning()) { return true; }

    const QString binary = findBinary();
    if (binary.isEmpty()) {
        if (error) {
            *error = QStringLiteral(
                "Hamlib's rotctld was not found. Install it with:\n\n"
                "    brew install hamlib\n\n"
                "Or run rotctld yourself and use the network option "
                "instead.");
        }
        return false;
    }

    m_model         = hamlibModel;
    m_device        = device;
    m_baud          = baud;
    m_preferredPort = listenPort;

    quint16 port = listenPort;
    if (!loopbackPortIsFree(port)) {
        const quint16 other = anyFreeLoopbackPort();
        qCWarning(lcRotctld)
            << "port" << port << "is already taken (a leftover rotctld?)"
            << "— using" << other << "instead";
        port = other;
        if (port == 0) {
            if (error) {
                *error = QStringLiteral(
                    "Port %1 is already in use and no free port could be "
                    "found. Something else — most likely a rotctld from "
                    "an earlier session — is still running; quit it and "
                    "try again.").arg(listenPort);
            }
            return false;
        }
    }
    m_listenPort = port;

    m_proc.setProgram(binary);
    m_proc.setArguments(arguments(hamlibModel, device, baud, port));
    m_stopRequested = false;
    m_proc.start();

    if (!m_proc.waitForStarted(3000)) {
        if (error) {
            *error = QStringLiteral("Couldn't start %1: %2")
                         .arg(binary, m_proc.errorString());
        }
        return false;
    }
    qCInfo(lcRotctld) << "started" << binary << m_proc.arguments();
    return true;
}

bool RotctldProcess::restart(QString* error)
{
    if (m_model == 0) {
        if (error) { *error = QStringLiteral("rotctld was never started"); }
        return false;
    }
    stop();
    return start(m_model, m_device, m_baud, m_preferredPort, error);
}

void RotctldProcess::stop()
{
    if (!isRunning()) { return; }

    // Ask first. rotctld closes the serial port on SIGTERM; killed
    // outright it can leave the port held until the device is
    // re-plugged, and the next connection attempt then fails for a
    // reason that has nothing to do with the rotator.
    m_stopRequested = true;
    m_proc.terminate();
    if (!m_proc.waitForFinished(2000)) {
        m_proc.kill();
        m_proc.waitForFinished(1000);
    }
}

} // namespace Longpath
