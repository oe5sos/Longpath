// =================================================================
// src/core/UpdateInstaller.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Zweck und Wege je Plattform: UpdateInstaller.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "UpdateInstaller.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace Longpath {

UpdateInstaller::UpdateInstaller(QObject* parent)
    : QObject(parent)
{
}

// ── reine Funktionen ──────────────────────────────────────────────────

QString UpdateInstaller::runningBundlePath(const QString& applicationDirPath)
{
    // .../Longpath.app/Contents/MacOS -> .../Longpath.app. Reine
    // Zeichenkettenarbeit: QDir::cdUp() verlangt, dass der Pfad
    // existiert, und ein Pruefstand rechnet mit erfundenen Pfaden.
    QStringList parts = QDir::cleanPath(applicationDirPath).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.size() < 3) { return {}; }
    if (parts.takeLast() != QStringLiteral("MacOS")) { return {}; }
    if (parts.takeLast() != QStringLiteral("Contents")) { return {}; }
    if (!parts.last().endsWith(QStringLiteral(".app"))) { return {}; }
    return QLatin1Char('/') + parts.join(QLatin1Char('/'));
}

QString UpdateInstaller::installTargetPath(const QString& applicationDirPath,
                                           const QString& envOverride)
{
    if (!envOverride.isEmpty()) { return envOverride; }
    const QString running = runningBundlePath(applicationDirPath);
    if (!running.isEmpty()) { return running; }
    return QStringLiteral("/Applications/Longpath.app");
}

QStringList UpdateInstaller::relaunchScript(qint64 pid, const QString& target, bool viaOpen,
                                            const QStringList& args)
{
    // kill -0 prueft nur, ob der Prozess noch da ist. `open` statt eines
    // direkten exec auf macOS: so laeuft das neue Paket als ordentliche
    // App unter LaunchServices (Dock, Ereignisse), nicht als Kind einer
    // Shell. Die Argumente dieses Laufs (etwa --profile) gehen mit --
    // sonst startete das neue Programm mit den falschen Einstellungen.
    const auto quoted = [](const QString& s) {
        QString q = s; q.replace(QLatin1Char('"'), QStringLiteral("\\\""));
        return QLatin1Char('"') + q + QLatin1Char('"');
    };
    QString launch = viaOpen ? QStringLiteral("open ") + quoted(target)
                             : QStringLiteral("exec ") + quoted(target);
    if (!args.isEmpty()) {
        if (viaOpen) { launch += QStringLiteral(" --args"); }
        for (const QString& a : args) { launch += QLatin1Char(' ') + quoted(a); }
    }
    const QString script = QStringLiteral("while kill -0 %1 2>/dev/null; do sleep 0.3; done; %2")
                               .arg(pid).arg(launch);
    return {QStringLiteral("-c"), script};
}

// ── Ablauf ────────────────────────────────────────────────────────────

bool UpdateInstaller::run(const QString& program, const QStringList& args,
                          QString* output, int timeoutMs)
{
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(program, args);
    if (!p.waitForStarted(10000)) {
        if (output) { *output = QStringLiteral("%1 did not start").arg(program); }
        return false;
    }
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        if (output) { *output = QStringLiteral("%1 timed out").arg(program); }
        return false;
    }
    const QString out = QString::fromUtf8(p.readAll()).trimmed();
    if (output) { *output = out; }
    return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}

void UpdateInstaller::installAndRelaunch(const QString& packagePath)
{
    const QString suffix = QFileInfo(packagePath).suffix().toLower();
    if (suffix == QStringLiteral("dmg"))      { installDmg(packagePath); }
    else if (suffix == QStringLiteral("exe")) { installWindows(packagePath); }
    else if (suffix == QStringLiteral("appimage")) { installAppImage(packagePath); }
    else { emit failed(QStringLiteral("don't know how to install %1").arg(QFileInfo(packagePath).fileName())); }
}

void UpdateInstaller::installDmg(const QString& dmgPath)
{
    const QString target = installTargetPath(QCoreApplication::applicationDirPath(),
                                             qEnvironmentVariable("LONGPATH_UPDATE_TARGET"));
    const QDir targetDir = QFileInfo(target).absoluteDir();
    if (!targetDir.exists()) { QDir().mkpath(targetDir.absolutePath()); }
    if (!QFileInfo(targetDir.absolutePath()).isWritable()) {
        emit failed(QStringLiteral("%1 is not writable -- install the update by hand from the DMG")
                        .arg(targetDir.absolutePath()));
        return;
    }

    QTemporaryDir mountRoot;
    if (!mountRoot.isValid()) { emit failed(QStringLiteral("no temporary directory")); return; }
    const QString mountPoint = QDir(mountRoot.path()).filePath(QStringLiteral("dmg"));
    QDir().mkpath(mountPoint);

    emit progress(QStringLiteral("Mounting the disk image"));
    QString out;
    if (!run(QStringLiteral("/usr/bin/hdiutil"),
             {QStringLiteral("attach"), QStringLiteral("-nobrowse"), QStringLiteral("-readonly"),
              QStringLiteral("-noautoopen"), QStringLiteral("-mountpoint"), mountPoint, dmgPath},
             &out)) {
        emit failed(QStringLiteral("hdiutil attach: %1").arg(out));
        return;
    }
    const auto detach = [&]() {
        QString o;
        run(QStringLiteral("/usr/bin/hdiutil"), {QStringLiteral("detach"), mountPoint, QStringLiteral("-force")}, &o, 60000);
    };

    const QStringList apps = QDir(mountPoint).entryList({QStringLiteral("*.app")}, QDir::Dirs | QDir::NoDotAndDotDot);
    if (apps.isEmpty()) {
        detach();
        emit failed(QStringLiteral("the disk image holds no application"));
        return;
    }
    const QString source = QDir(mountPoint).filePath(apps.first());

    // Das alte Paket beiseitelegen (gleiches Verzeichnis: ein rename,
    // der laufende Prozess behaelt seine Abbildung), dann kopieren.
    const QString aside = target + QStringLiteral(".previous-") + QString::number(QDateTime::currentSecsSinceEpoch());
    if (QFileInfo::exists(target) && !QDir().rename(target, aside)) {
        detach();
        emit failed(QStringLiteral("cannot move the current application aside (%1)").arg(target));
        return;
    }
    emit progress(QStringLiteral("Copying the new application"));
    if (!run(QStringLiteral("/usr/bin/ditto"), {source, target}, &out, 600000)) {
        // Zurueck auf Los: das alte Paket wieder an seinen Platz.
        QDir(target).removeRecursively();
        if (QFileInfo::exists(aside)) { QDir().rename(aside, target); }
        detach();
        emit failed(QStringLiteral("ditto: %1").arg(out));
        return;
    }
    if (QFileInfo::exists(aside)) { QDir(aside).removeRecursively(); }
    emit progress(QStringLiteral("Unmounting the disk image"));
    detach();
    // Das Paket hat seinen Dienst getan; 77 MB im Temp-Ordner braucht
    // niemand mehr.
    QFile::remove(dmgPath);
    emit installed(target);
}

void UpdateInstaller::installWindows(const QString& exePath)
{
    emit progress(QStringLiteral("Starting the installer"));
    if (!QProcess::startDetached(exePath, {})) {
        emit failed(QStringLiteral("cannot start %1").arg(exePath));
        return;
    }
    emit installed(exePath);
}

void UpdateInstaller::installAppImage(const QString& imagePath)
{
    const QString running = qEnvironmentVariable("APPIMAGE");
    const QString target = !qEnvironmentVariable("LONGPATH_UPDATE_TARGET").isEmpty()
        ? qEnvironmentVariable("LONGPATH_UPDATE_TARGET")
        : (running.isEmpty() ? QDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation))
                                   .filePath(QFileInfo(imagePath).fileName())
                             : running);
    emit progress(QStringLiteral("Replacing the AppImage"));
    QFile::setPermissions(imagePath, QFile::permissions(imagePath) | QFileDevice::ExeOwner
                                         | QFileDevice::ExeGroup | QFileDevice::ExeOther);
    if (QFileInfo::exists(target) && !QFile::remove(target)) {
        emit failed(QStringLiteral("cannot replace %1").arg(target));
        return;
    }
    if (!QFile::rename(imagePath, target) && !QFile::copy(imagePath, target)) {
        emit failed(QStringLiteral("cannot place %1").arg(target));
        return;
    }
    emit installed(target);
}

} // namespace Longpath
