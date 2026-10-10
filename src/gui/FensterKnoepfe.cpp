// =================================================================
// src/gui/FensterKnoepfe.cpp  (Longpath)
// =================================================================
// Siehe FensterKnoepfe.h.
//
// Modification history (Longpath):
//   2026-10-10 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Claude Code).
// =================================================================

#include "gui/FensterKnoepfe.h"

#include <QEvent>
#include <QWidget>

namespace Longpath {

bool FensterKnoepfe::knoepfeNachruesten(QWidget* w)
{
    if (w == nullptr || !w->isWindow()) { return false; }

    const Qt::WindowFlags f = w->windowFlags();
    const Qt::WindowType  typ = Qt::WindowType(int(f) & int(Qt::WindowType_Mask));

    // Nur gewöhnliche Fenster und Dialoge. Ein Popup oder ein
    // Werkzeugfenster ist absichtlich ohne Rahmenknöpfe, und ein
    // rahmenloses hat gar keine Stelle, an der sie sitzen könnten.
    if (typ != Qt::Dialog && typ != Qt::Window) { return false; }
    if (f.testFlag(Qt::FramelessWindowHint))    { return false; }
    if (f.testFlag(Qt::Popup))                  { return false; }
    if (f.testFlag(Qt::Tool))                   { return false; }
    if (f.testFlag(Qt::Sheet))                  { return false; }

    // Modal nicht: ein modales Fenster im Dock sperrt das Programm
    // und ist von dort nicht mehr zu erreichen.
    if (w->windowModality() != Qt::NonModal)    { return false; }

    const Qt::WindowFlags will = Qt::WindowMinimizeButtonHint
                               | Qt::WindowMaximizeButtonHint
                               | Qt::WindowCloseButtonHint
                               | Qt::WindowTitleHint
                               | Qt::WindowSystemMenuHint;
    if ((f & will) == will) { return false; }

    // Vor dem ersten Zeigen, also bevor das Fenster beim System
    // angelegt wird -- danach kostet ein Flaggenwechsel ein
    // Neuanlegen samt Flackern, und die Stelle am Bildschirm geht
    // verloren.
    w->setWindowFlags(f | will);
    return true;
}

bool FensterKnoepfe::eventFilter(QObject* o, QEvent* e)
{
    if (e->type() == QEvent::Polish) {
        knoepfeNachruesten(qobject_cast<QWidget*>(o));
    }
    return QObject::eventFilter(o, e);
}

} // namespace Longpath
