// no-port-check: Longpath-original, kein Gegenstueck in Thetis.
//
// Ein Hamlib-Daemon darf das Programm nicht ueberleben.
//
// Gemessen am 2026-11-07 mit einer kleinen Qt-6.11-Sonde: unter macOS
// fuehren Cmd+Q und QCoreApplication::quit() ueber [NSApp terminate:] in
// ein exit() -- noch innerhalb von closeEvent(). aboutToQuit feuert dort
// nicht, app.exec() kehrt nicht zurueck, und das MainWindow auf dem
// Stapel von main() wird nie zerstoert. Nur atexit laeuft. Einzig der
// rote Punkt geht den ordentlichen Weg.
//
// Der Destruktor von HamlibDaemonProcess erreicht den Daemon auf diesen
// beiden Wegen also nie: am selben Tag blieb ein rigctld als Waise
// zurueck und hielt den Anschluss des TS-590 besetzt.
//
// Der Helfer unten stirbt mit std::exit(), genau wie Cocoa es tut: ohne
// Destruktor und ohne aboutToQuit.

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>

#include "core/RigctldProcess.h"

#include <cstdlib>
#include <cstring>

#if defined(Q_OS_UNIX)
#include <csignal>
#include <sys/types.h>
#endif

using namespace Longpath;

namespace {

const char kHelfer[] = "--helfer-stirbt-mit-exit";

// Bleibt liegen, bis ein SIGTERM kommt, und schreibt das dann auf. Die
// Marke statt eines kill(pid, 0): auf einem Laeufer ohne aufraeumenden
// Elternprozess lebt ein Beendeter als Zombie weiter.
const char kDaemon[] =
    "#!/bin/sh\n"
    "trap 'echo beendet > \"$DAEMON_ENDE\"; exit 0' TERM\n"
    "echo $$ > \"$DAEMON_PID\"\n"
    "while :; do sleep 0.1; done\n";

bool dateiErscheint(const QString& pfad, int fristMs)
{
    QElapsedTimer uhr;
    uhr.start();
    while (uhr.elapsed() < fristMs) {
        if (QFile::exists(pfad) && QFile(pfad).size() > 0) { return true; }
        QThread::msleep(20);
    }
    return false;
}

int helfer()
{
    // Absichtlich nie geloescht: Cocoa loescht auch nichts.
    auto* daemon = new RigctldProcess;
    QString fehler;
    if (!daemon->start(1, QString(), 0, RigctldProcess::kDefaultPort, &fehler)) {
        fprintf(stderr, "start fehlgeschlagen: %s\n", qPrintable(fehler));
        return 3;
    }
    if (!dateiErscheint(qEnvironmentVariable("DAEMON_PID"), 5000)) {
        fprintf(stderr, "der Daemon hat sich nicht gemeldet\n");
        return 4;
    }
    std::exit(0);
}

} // namespace

class TstDaemonUeberlebtBeendenNicht : public QObject {
    Q_OBJECT
private slots:
    void exit_ohne_destruktor_nimmt_den_daemon_mit();
};

void TstDaemonUeberlebtBeendenNicht::exit_ohne_destruktor_nimmt_den_daemon_mit()
{
#if !defined(Q_OS_UNIX)
    QSKIP("Die Sendung geht ueber SIGTERM; der Weg ueber exit() ist ein "
          "macOS-Weg.");
#else
    QTemporaryDir ordner;
    QVERIFY(ordner.isValid());

    // Ein Doppel unter dem Namen rigctld: der Pruefstand soll ohne Hamlib
    // und ohne Funkgeraet laufen.
    const QString skript = ordner.filePath(QStringLiteral("rigctld"));
    {
        QFile f(skript);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(kDaemon);
        f.close();
        QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner
                                 | QFile::ExeOwner));
    }

    const QString pidDatei  = ordner.filePath(QStringLiteral("pid"));
    const QString endeDatei = ordner.filePath(QStringLiteral("ende"));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PATH"),
               ordner.path() + QLatin1Char(':') + env.value(QStringLiteral("PATH")));
    env.insert(QStringLiteral("DAEMON_PID"), pidDatei);
    env.insert(QStringLiteral("DAEMON_ENDE"), endeDatei);

    QProcess prozess;
    prozess.setProcessEnvironment(env);
    prozess.start(QCoreApplication::applicationFilePath(),
                  {QString::fromLatin1(kHelfer)});
    QVERIFY(prozess.waitForFinished(15000));
    QVERIFY2(prozess.exitCode() == 0, prozess.readAllStandardError().constData());

    // Voraussetzung: das Doppel lief wirklich und nicht das echte rigctld.
    QVERIFY2(QFile::exists(pidDatei), "das Doppel wurde nie gestartet");
    QFile f(pidDatei);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const qint64 pid = f.readAll().trimmed().toLongLong();
    QVERIFY(pid > 1);

    const bool beendet = dateiErscheint(endeDatei, 5000);

    // Wer rot wird, laesst keine Waise auf dem Laeufer zurueck.
    ::kill(static_cast<pid_t>(pid), SIGKILL);

    QVERIFY2(beendet, "der Daemon hat das Programm ueberlebt");
#endif
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], kHelfer) == 0) { return helfer(); }
    }
    TstDaemonUeberlebtBeendenNicht pruefstand;
    return QTest::qExec(&pruefstand, argc, argv);
}

#include "tst_daemon_ueberlebt_beenden_nicht.moc"
