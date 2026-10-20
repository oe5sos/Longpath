// =================================================================
// src/gui/MacVollflaeche.mm  (Longpath)
// =================================================================
// Siehe MacVollflaeche.h.
//
// Dieser Baum uebersetzt Objective-C++ OHNE ARC (-fobjc-arc ist
// nirgends gesetzt). Hier wird nichts behalten und nichts
// freigegeben: alle angefassten Objekte gehoeren dem Fenster
// beziehungsweise AppKit, und wir setzen nur Eigenschaften daran.
//
// Modification history (Longpath):
//   2026-10-10 — Created in C++20/Qt6/Objective-C++ for Longpath by
//                 Martin Fischer, AI-assisted via Anthropic Claude
//                 (Claude Code).
// =================================================================

#include "gui/MacVollflaeche.h"

#include <QWidget>
#include <QWindow>

#import <AppKit/AppKit.h>

namespace Longpath::MacVollflaeche {

bool verfuegbar() { return true; }

bool setzeVollflaeche(QWidget* fenster, bool an)
{
    if (fenster == nullptr) { return false; }

    // Das native Fenster muss es schon geben. Es gibt es: die
    // Applet- und Zeigerfenster brauchen winId() laengst vorher --
    // genau deshalb kostet ein Flaggenwechsel am Hauptfenster ein
    // hide()/show(), und genau das vermeidet dieser Weg.
    fenster->winId();
    QWindow* qw = fenster->windowHandle();
    if (qw == nullptr) { return false; }

    NSView* view = reinterpret_cast<NSView*>(qw->winId());
    if (view == nil) { return false; }
    NSWindow* win = [view window];
    if (win == nil) { return false; }

    if (an) {
        win.titlebarAppearsTransparent = YES;
        win.titleVisibility = NSWindowTitleHidden;
        win.styleMask |= NSWindowStyleMaskFullSizeContentView;
        // Zuklappbar muss ausdruecklich dastehen: ein Fenster, dem
        // AppKit diese Flagge nicht gibt, zeichnet den gelben Punkt
        // grau -- und genau das war die Beschwerde.
        win.styleMask |= NSWindowStyleMaskMiniaturizable;
        win.styleMask |= NSWindowStyleMaskResizable;
        [win setMovableByWindowBackground:NO];
    } else {
        win.titlebarAppearsTransparent = NO;
        win.titleVisibility = NSWindowTitleVisible;
        win.styleMask &= ~NSWindowStyleMaskFullSizeContentView;
    }
    return true;
}

} // namespace Longpath::MacVollflaeche
