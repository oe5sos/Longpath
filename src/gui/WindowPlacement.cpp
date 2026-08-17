// =================================================================
// src/gui/WindowPlacement.cpp  (NereusSDR)
// =================================================================
// Siehe WindowPlacement.h für Herkunft und die zwei bekannten
// Schwächen, die hier bewusst unverändert bleiben.
// =================================================================

#include "gui/WindowPlacement.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QRect>
#include <QScreen>
#include <QWidget>

Q_LOGGING_CATEGORY(lcWindowPlacement, "nereus.windowplacement")

namespace NereusSDR {

void ensureOnVisibleScreen(QWidget* w, QWidget* anchor, QSize minSize)
{
    if (!w) { return; }

    // A frameless Qt::Window with no explicit position defaults to (0,0)
    // on Windows, where it's usually obscured by the main application
    // window — which is how Container #0 "disappeared" when first
    // floated. Saved geometry at (0,0) (e.g., after the user closed the
    // invisible form) perpetuates the trap across restarts.
    const QRect g = w->geometry();
    const int minW = minSize.width();
    const int minH = minSize.height();

    bool onScreen = false;
    for (QScreen* s : QGuiApplication::screens()) {
        if (s && s->availableGeometry().intersects(g)) {
            onScreen = true;
            break;
        }
    }
    const bool atOrigin = (g.x() == 0 && g.y() == 0);
    if (onScreen && !atOrigin && g.width() >= minW && g.height() >= minH) {
        return;
    }

    QScreen* screen = nullptr;
    QRect anchorRect;
    if (anchor && anchor->window()) {
        screen = anchor->window()->screen();
        anchorRect = anchor->window()->geometry();
    }
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    const QRect avail = screen ? screen->availableGeometry()
                               : QRect(100, 100, 800, 600);

    const int width  = qMax(g.width(),  minW > 0 ? minW : 260);
    const int height = qMax(g.height(), minH > 24 ? minH : 300);

    int x = anchorRect.isValid()
        ? anchorRect.center().x() - width / 2
        : avail.x() + (avail.width()  - width) / 2;
    int y = anchorRect.isValid()
        ? anchorRect.center().y() - height / 2
        : avail.y() + (avail.height() - height) / 2;

    x = qBound(avail.x(), x, avail.right()  - width);
    y = qBound(avail.y(), y, avail.bottom() - height);

    w->setGeometry(x, y, width, height);
    qCDebug(lcWindowPlacement) << "repositioned to visible area"
                               << QRect(x, y, width, height) << "on screen"
                               << (screen ? screen->name()
                                          : QStringLiteral("(null)"));
}

} // namespace NereusSDR
