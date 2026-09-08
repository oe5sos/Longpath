// =================================================================
// src/gui/MacFloatingWindowBehavior.mm  (Longpath)
// =================================================================
// Siehe MacFloatingWindowBehavior.h fuer Zweck und Begruendung.
// =================================================================

#import <AppKit/AppKit.h>

#include "gui/MacFloatingWindowBehavior.h"

#include <QGuiApplication>
#include <QWidget>
#include <QWindow>

namespace Longpath {

void enableFullScreenAuxiliaryBehavior(QWidget* widget)
{
    if (!widget) { return; }

    // Bug fix 2026-09-08 (erster echter CI-Testlauf auf macOS, alle
    // *FloatingWindow-Konstruktoren SEGFAULT): unter QT_QPA_PLATFORM=
    // offscreen (CI, ci.yml) erzeugt winId() KEIN echtes natives
    // NSView -- der zurueckgegebene WId-Wert ist eine synthetische
    // Kennung der Offscreen-Plattform-Engine, kein Zeiger auf ein
    // echtes Cocoa-Objekt. Der `!view`-Nullcheck weiter unten faengt
    // das NICHT ab (der Wert ist nicht null, nur ungueltig) -- der
    // (__bridge NSView*)-Cast liefert einen Zeiger, der beim ersten
    // objc_msgSend (hier: `.window`) sofort abstuerzt (SIGSEGV,
    // reproduziert per lldb: objc_msgSend -> enableFullScreenAuxiliary-
    // Behavior). Diese ganze Funktion ist ohnehin nur auf der echten
    // Cocoa-Plattform sinnvoll (schwebende NSPanel-Fensterebenen
    // existieren nicht ausserhalb davon) -- vor jedem Zugriff pruefen.
    if (QGuiApplication::platformName() != QLatin1String("cocoa")) { return; }

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
