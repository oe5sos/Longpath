// =================================================================
// src/gui/MacFloatingWindowBehavior.mm  (Longpath)
// =================================================================
// Siehe MacFloatingWindowBehavior.h fuer Zweck und Begruendung.
// =================================================================

#import <AppKit/AppKit.h>

#include "gui/MacFloatingWindowBehavior.h"

#include <QWidget>
#include <QWindow>

namespace Longpath {

void enableFullScreenAuxiliaryBehavior(QWidget* widget)
{
    if (!widget) { return; }

    // winId() erzwingt die Anlage des nativen Fensters, falls es noch
    // keines hat -- ohne das waere windowHandle() hier oft noch
    // nullptr, je nachdem wie frueh im Aufbau diese Funktion aufgerufen
    // wird.
    widget->winId();
    QWindow* qw = widget->windowHandle();
    if (!qw) { return; }

    // __bridge, nicht reinterpret_cast: unter ARC ist ein rohes
    // reinterpret_cast auf einen Objective-C-Objektzeiger verboten (ARC
    // kennt die Eigentumsregel nicht). __bridge sagt "kein
    // Eigentumsuebergang" -- richtig hier, winId() gehoert weiterhin
    // Qt/AppKit. Unter MRC ist __bridge ein zulaessiges No-Op.
    NSView* view = (__bridge NSView*)reinterpret_cast<void*>(qw->winId());
    if (!view) { return; }
    NSWindow* nsWindow = view.window;
    if (!nsWindow) { return; }

    nsWindow.collectionBehavior = nsWindow.collectionBehavior
        | NSWindowCollectionBehaviorFullScreenAuxiliary;

    // Betreiber 2026-08-31, per Bildschirmfreigabe direkt beobachtet:
    // diese Fenster standen ueber JEDEM anderen Programm auf dem Mac --
    // sogar ueber dem nackten Schreibtisch auf einem VOELLIG anderen
    // Space als das Hauptfenster. FullScreenAuxiliary (oben) regelt nur,
    // WELCHEM Space das Fenster folgt; es aendert nichts an der
    // Fensterebene selbst. Qt::Tool erzeugt auf macOS ein NSPanel mit
    // einem "schwebenden" Level, das laut Cocoa ausdruecklich UEBER
    // allen normalen Fenstern JEDER App liegt -- nicht nur ueber denen
    // von Longpath. hidesOnDeactivate ist genau der dafuer vorgesehene
    // NSPanel-Schalter: das Fenster versteckt sich automatisch, sobald
    // Longpath nicht mehr die aktive App ist, und kommt zurueck, sobald
    // Longpath wieder aktiv wird -- das Verhalten, das jede normale
    // Werkzeugpalette hat, und das diese vier Fensterklassen bisher nie
    // bekamen.
    if ([nsWindow isKindOfClass:[NSPanel class]]) {
        ((NSPanel*)nsWindow).hidesOnDeactivate = YES;
    }
}

} // namespace Longpath
