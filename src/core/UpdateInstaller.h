// =================================================================
// src/core/UpdateInstaller.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Die zweite Haelfte des Ein-Klick-Updaters (UpdateChecker.h): ein
// geprueftes Paket einspielen und Longpath neu starten.
//
//   macOS    DMG anhaengen (hdiutil), das Programmpaket darin mit ditto
//            an die Stelle des laufenden kopieren (das alte wird vorher
//            beiseitegelegt und danach entfernt -- der laufende Prozess
//            behaelt seine Abbildung), DMG abhaengen, dann ein
//            abgekoppelter Helfer: warten, bis dieser Prozess weg ist,
//            und das Programm neu oeffnen. Kein Kennwort: /Applications
//            ist fuer Verwalter beschreibbar, und ein Paket, das nicht
//            unter /Applications laeuft (Bau-Verzeichnis), wird an
//            seiner eigenen Stelle ersetzt.
//   Windows  den NSIS-Installer starten und sich selbst beenden -- der
//            Installer ersetzt die Dateien, sobald das Programm weg ist.
//   Linux    das AppImage an der Stelle des laufenden ersetzen ($APPIMAGE)
//            und neu starten.
//
// Nur macOS ist am Geraet geprueft (Betreiber: "nur Mac"); die anderen
// beiden Wege sind der einfachste, der dort ueblich ist.
//
// Der Zielpfad laesst sich per Umgebungsvariable LONGPATH_UPDATE_TARGET
// umbiegen -- fuer Pruefstaende und Livetests aus einer Sandbox, damit
// nie das echte /Applications/Longpath.app beruehrt wird.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;

namespace Longpath {

class UpdateInstaller : public QObject {
    Q_OBJECT
public:
    explicit UpdateInstaller(QObject* parent = nullptr);

    // -- reine Funktionen ------------------------------------------------
    // Das laufende Programmpaket (macOS: .../Longpath.app), sonst leer.
    static QString runningBundlePath(const QString& applicationDirPath);
    // Wohin eingespielt wird: LONGPATH_UPDATE_TARGET, sonst das laufende
    // Paket, sonst /Applications/Longpath.app.
    static QString installTargetPath(const QString& applicationDirPath,
                                     const QString& envOverride);
    // Der abgekoppelte Neustart-Helfer: wartet, bis <pid> weg ist, und
    // oeffnet <target> -- auf macOS per `open` (LaunchServices), sonst
    // direkt -- mit denselben Argumenten wie dieser Lauf (--profile!).
    // Als Argumentliste fuer /bin/sh -c.
    static QStringList relaunchScript(qint64 pid, const QString& target, bool viaOpen,
                                      const QStringList& args = {});

    // -- Ablauf ----------------------------------------------------------
    // Spielt <packagePath> ein und startet bei Erfolg neu (quit + Helfer).
    void installAndRelaunch(const QString& packagePath);

signals:
    void progress(const QString& step);
    void failed(const QString& why);
    // Eingespielt; der Aufrufer beendet das Programm, der Helfer oeffnet
    // es wieder. (Getrennt, damit ein Pruefstand nicht neu startet.)
    void installed(const QString& targetPath);

private:
    bool run(const QString& program, const QStringList& args, QString* output,
             int timeoutMs = 120000);
    void installDmg(const QString& dmgPath);
    void installWindows(const QString& exePath);
    void installAppImage(const QString& imagePath);
};

} // namespace Longpath
