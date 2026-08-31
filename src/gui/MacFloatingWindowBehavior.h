#pragma once

// =================================================================
// src/gui/MacFloatingWindowBehavior.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Betreiber, wiederholt gemeldet: im nativen macOS-Vollbild bleiben die
// schwebenden Qt::Tool-Fenster (AppletFloatingWindow, ToolWindow,
// FloatingContainer, PanFloatingWindow) auf dem alten Schreibtisch
// zurueck, statt mit dem Hauptfenster in dessen eigenen Vollbild-Space
// zu wandern.
//
// Der Code an allen vier Stellen behauptete bislang nur in Kommentaren,
// Qt::Tool ergaebe auf macOS automatisch ein NSPanel mit der
// Sammelregel "FullScreenAuxiliary" (schwebt ueber einem Vollbild-
// Elternfenster). Das stimmt nicht von selbst -- Qt setzt dieses
// NSWindowCollectionBehavior nicht automatisch fuer Qt::Tool-Fenster.
// Ohne diesen expliziten Aufruf zaehlt das Fenster fuer macOS als
// gewoehnliches Fenster auf dem gewoehnlichen Schreibtisch-Space und
// bleibt dort liegen, wenn das Elternfenster in seinen eigenen
// Vollbild-Space wechselt.
//
// Betreiber 2026-08-31, per geteiltem Bildschirm live beobachtet: ein
// zweiter, schwererer Teil desselben Problems. Diese vier Fenster
// standen nicht nur ueber dem eigenen Hauptfenster, sondern ueber
// JEDER anderen App auf dem Mac -- sogar ueber dem nackten Schreibtisch
// auf einem voellig anderen Space, waehrend das Hauptfenster laengst
// in seinem eigenen Vollbild-Space stand. Grund: Qt::Tool erzeugt ein
// NSPanel mit einem "schwebenden" Fensterlevel, das laut Cocoa
// AUSDRUECKLICH ueber allen normalen Fenstern JEDER App liegt, nicht
// nur denen von Longpath. Die FullScreenAuxiliary-Sammelregel oben
// aendert daran nichts -- sie regelt nur, welchem Space das Fenster
// folgt, nicht seine Ebene relativ zu ANDEREN Apps. Diese Funktion
// setzt darum zusaetzlich NSPanel.hidesOnDeactivate: das Fenster
// versteckt sich automatisch, sobald Longpath nicht mehr die aktive
// App ist, und kommt zurueck, sobald Longpath wieder aktiv wird --
// das Standardverhalten jeder normalen macOS-Werkzeugpalette.
//
// Muss aufgerufen werden, NACHDEM das Fenster sein natives Handle hat
// (winId() erzwingt das noetigenfalls). No-op auf allen anderen
// Plattformen.
//
// <QtGlobal> statt eines Vorwärtsdeklarations-only-Headers: Q_OS_MAC
// muss hier zuverlässig definiert sein, unabhängig davon, in welcher
// Reihenfolge der Aufrufer seine eigenen Includes setzt (in
// MacFloatingWindowBehavior.mm z.B. VOR <QWidget>).
#include <QtGlobal>

class QWidget;

#ifdef Q_OS_MAC
namespace Longpath {
void enableFullScreenAuxiliaryBehavior(QWidget* window);
}
#else
namespace Longpath {
inline void enableFullScreenAuxiliaryBehavior(QWidget*) {}
}
#endif
