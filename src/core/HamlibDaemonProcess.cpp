// =================================================================
// src/core/HamlibDaemonProcess.cpp  (Longpath)
// =================================================================
//
// Longpath-original — siehe HamlibDaemonProcess.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Als RotctldProcess angelegt (Cowork).
//   2026-10-27 — Herausgezogen und ueber den Binaernamen allgemein
//                gemacht. Der Rumpf ist unveraendert; nur dort, wo
//                vorher "rotctld" stand, steht jetzt m_binaryName.
//                KI-gestuetzt ueber Anthropic Claude Code.
// =================================================================

#include "HamlibDaemonProcess.h"

#include <QFileInfo>
#include <QHostAddress>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QTcpServer>

namespace Longpath {

namespace {

Q_LOGGING_CATEGORY(lcHamlibDaemon, "longpath.hamlib")

// True if nothing on this machine is listening on loopback:port. A
// bind that succeeds is released again at once; the daemon binds it
// for real a moment later.
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

HamlibDaemonProcess::HamlibDaemonProcess(QString binaryName,
                                         quint16 defaultPort,
                                         QObject* parent)
    : QObject(parent)
    , m_binaryName(std::move(binaryName))
    , m_defaultPort(defaultPort)
{
    connect(&m_proc, &QProcess::finished, this,
            [this](int code, QProcess::ExitStatus) {
        const QString err =
            QString::fromLocal8Bit(m_proc.readAllStandardError()).trimmed();
        if (m_stopRequested) {
            m_stopRequested = false;
            return;
        }
        qCWarning(lcHamlibDaemon) << m_binaryName
                                  << "exited on its own, code" << code
                                  << err;
        emit exited(code, err);
    });
}

HamlibDaemonProcess::~HamlibDaemonProcess()
{
    stop();
}

QString HamlibDaemonProcess::grundAus(const QString& stderrText)
{
    // Begruendung im Kopf. Hamlibs stderr ist ein Ablaufprotokoll; die
    // letzte Zeile ist die Folge, nicht die Ursache.
    static const QStringList kUrsachen = {
        QStringLiteral("timed out"), QStringLiteral("refused"),
        QStringLiteral("no such"),   QStringLiteral("in use"),
        QStringLiteral("permission"),QStringLiteral("not permitted"),
        QStringLiteral("unavailable"),
    };
    static const QStringList kFehlschlag = {
        QStringLiteral("failed"), QStringLiteral("error"),
        QStringLiteral("cannot"), QStringLiteral("unable"),
    };

    const QStringList zeilen = stderrText.split(QLatin1Char('\n'));
    QString ursache, fehlschlag, letzte;
    for (const QString& rohe : zeilen) {
        const QString z = rohe.trimmed();
        if (z.isEmpty()) { continue; }
        letzte = z;
        for (const QString& w : kUrsachen) {
            if (z.contains(w, Qt::CaseInsensitive)) { ursache = z; break; }
        }
        // Eine Zeile zaehlt nur als Auskunft, wenn sie auch etwas BENENNT:
        // einen Rechner, einen Port, einen Pfad, eine Nummer. Der Pruefstand
        // hat diese Bedingung erzwungen -- "IO error" steht bei Hamlib immer
        // zuletzt und enthaelt das Wort "error", also gewann es jedes Mal
        // gegen die Zeile, die tatsaechlich etwas sagte.
        bool benennt = z.contains(QLatin1Char(':'));
        if (!benennt) {
            for (const QChar c : z) {
                if (c.isDigit()) { benennt = true; break; }
            }
        }
        if (benennt) {
            for (const QString& w : kFehlschlag) {
                if (z.contains(w, Qt::CaseInsensitive)) { fehlschlag = z; break; }
            }
        }
    }
    if (!ursache.isEmpty())    { return ursache; }
    if (!fehlschlag.isEmpty()) { return fehlschlag; }
    return letzte;
}

QString HamlibDaemonProcess::findBinaryNamed(const QString& binaryName)
{
    // PATH first: an operator who installed Hamlib somewhere unusual
    // and put it on PATH has already answered this question.
    const QString onPath = QStandardPaths::findExecutable(binaryName);
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
        const QString candidate = dir + QLatin1Char('/') + binaryName;
        if (QFileInfo(candidate).isExecutable()) { return candidate; }
    }
    return {};
}

QStringList HamlibDaemonProcess::arguments(int hamlibModel,
                                           const QString& device,
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

    // Loopback only. Neither daemon has authentication of any kind, and
    // a rotator that anyone on the network can turn is a rotator that
    // will eventually be turned by someone else — the same goes for a
    // transmitter that anyone can key. An operator who wants it
    // reachable from another machine can run the daemon themselves and
    // point Longpath at it — that is a decision worth making
    // deliberately.
    args << QStringLiteral("-T") << QStringLiteral("127.0.0.1")
         << QStringLiteral("-t") << QString::number(listenPort);
    return args;
}

bool HamlibDaemonProcess::isRunning() const
{
    return m_proc.state() != QProcess::NotRunning;
}

bool HamlibDaemonProcess::start(int hamlibModel, const QString& device,
                                int baud, quint16 listenPort, QString* error)
{
    if (isRunning()) { return true; }

    const QString binary = findBinaryNamed(m_binaryName);
    if (binary.isEmpty()) {
        if (error) {
            *error = QStringLiteral(
                "Hamlib's %1 was not found. Install it with:\n\n"
                "    brew install hamlib\n\n"
                "Or run %1 yourself and use the network option "
                "instead.").arg(m_binaryName);
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
        qCWarning(lcHamlibDaemon)
            << "port" << port << "is already taken (a leftover"
            << m_binaryName << "?) — using" << other << "instead";
        port = other;
        if (port == 0) {
            if (error) {
                *error = QStringLiteral(
                    "Port %1 is already in use and no free port could be "
                    "found. Something else — most likely a %2 from "
                    "an earlier session — is still running; quit it and "
                    "try again.").arg(QString::number(listenPort),
                                      m_binaryName);
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
    qCInfo(lcHamlibDaemon) << "started" << binary << m_proc.arguments();
    return true;
}

bool HamlibDaemonProcess::restart(QString* error)
{
    if (m_model == 0) {
        if (error) {
            *error = QStringLiteral("%1 was never started").arg(m_binaryName);
        }
        return false;
    }
    stop();
    return start(m_model, m_device, m_baud, m_preferredPort, error);
}

void HamlibDaemonProcess::stop()
{
    if (!isRunning()) { return; }

    // Ask first. The daemon closes the serial port on SIGTERM; killed
    // outright it can leave the port held until the device is
    // re-plugged, and the next connection attempt then fails for a
    // reason that has nothing to do with the hardware.
    m_stopRequested = true;
    m_proc.terminate();
    if (!m_proc.waitForFinished(2000)) {
        m_proc.kill();
        m_proc.waitForFinished(1000);
    }
}

} // namespace Longpath
