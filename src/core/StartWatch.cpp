// no-port-check: Longpath-original file; siehe den Kopf von StartWatch.h.

#include "core/StartWatch.h"

#include "core/AppSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

namespace Longpath::StartWatch {

namespace {

/// Den Zaehler aus der Marke lesen. Keine Marke heisst null.
int readMarker()
{
    QFile f(markerPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }
    const QByteArray roh = f.readAll().trimmed();
    f.close();
    bool ok = false;
    const int n = roh.toInt(&ok);
    // Eine unleserliche Marke als EINEN steckengebliebenen Start werten,
    // nicht als keinen: sie ist ja da, also lief ein Start an und kam
    // nicht durch. Nur die Zahl darin ist verloren.
    return ok ? n : 1;
}

/// Die Marke mit `count` schreiben -- sofort und vollstaendig.
bool writeMarker(int count)
{
    const QString pfad = markerPath();
    QDir().mkpath(QFileInfo(pfad).absolutePath());

    // QSaveFile, damit nie eine halbe Marke liegenbleibt: ein Absturz
    // MITTEN im Schreiben ist genau der Fall, um den es hier geht.
    QSaveFile f(pfad);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    {
        QTextStream out(&f);
        out << count << '\n';
    }
    return f.commit();
}

} // namespace

QString markerPath()
{
    return QDir(AppSettings::dataDir()).filePath(QStringLiteral("start-watch"));
}

int beginStart()
{
    // ── Eine Sonde zaehlt nicht mit (2026-10-21) ─────────────────────
    //
    // Gemessen: zwei Sonden hintereinander, jede nach wenigen Sekunden
    // beendet (also vor `kSettleSeconds`), und der dritte Lauf fuhr
    // OHNE die gespeicherte Anordnung und zeigte den Hinweiskasten --
    // womit jede Messung an der Fensteranordnung wertlos war. Richtig
    // ist das auch so nicht: der Waechter soll den Betreiber vor einer
    // Anordnung schuetzen, die den Start umbringt. Eine Sonde mit
    // eigenem Konfigurationsordner hat keine Anordnung zu schuetzen,
    // sie sieht nur nach.
    if (AppSettings::isSandbox()) { return 0; }

    // Was beim letzten Mal uebrigblieb: steht die Marke noch, ist jener
    // Start nicht durchgekommen.
    const int bisher = readMarker();
    writeMarker(bisher + 1);
    return bisher;
}

void markRunning()
{
    QFile::remove(markerPath());
}

int failedStarts()
{
    // Die laufende Marke zaehlt den GERADE laufenden Start mit -- fuer
    // die Frage "wieviele kamen nicht durch" ist das einer zuviel.
    const int marke = readMarker();
    return marke > 0 ? marke - 1 : 0;
}

bool shouldStartSafely()
{
    // Siehe beginStart(): in einer Sandbox gibt es keine Marke, die
    // hochzaehlt -- und damit auch keinen sicheren Start. Ausdruecklich
    // hier noch einmal, damit eine Marke, die von einem Lauf OHNE
    // Sandbox im selben Ordner liegengeblieben ist, die Sonde nicht
    // doch in den sicheren Start schickt.
    if (AppSettings::isSandbox()) { return false; }
    return failedStarts() >= kSafeThreshold;
}

void reset()
{
    QFile::remove(markerPath());
}

QString safeStartNotice(int failedCount)
{
    return QStringLiteral(
        "Longpath ist %1 Mal hintereinander beim Starten steckengeblieben "
        "und ist darum ohne die gespeicherte Anordnung hochgefahren. "
        "Die Einstellungen sind unberührt — nur die Fensteraufteilung "
        "steht auf der Vorgabe, und es wurde keine Verbindung zum Gerät "
        "aufgebaut.\n\n"
        "Läuft es jetzt, war etwas an der gespeicherten Anordnung schuld. "
        "Sie lässt sich neu einrichten und wird beim Beenden wieder "
        "gesichert.").arg(failedCount);
}

} // namespace Longpath::StartWatch
