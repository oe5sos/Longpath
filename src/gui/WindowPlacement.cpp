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

namespace Longpath {

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

    QScreen* screen = nullptr;
    QRect anchorRect;
    if (anchor && anchor->window()) {
        screen = anchor->window()->screen();
        anchorRect = anchor->window()->geometry();
    }

    // Betreiber 2026-08-31: "S-Meter usw. liegen frei am Desktop" -- der
    // reine Schirm-Test oben erklaert das nicht, ein Fenster, das
    // grossteils auf dem physischen Bildschirm liegt, gilt danach immer
    // als "in Ordnung", selbst wenn das Hauptfenster laengst kleiner
    // geworden oder woanders hin verschoben ist und die gespeicherte
    // Position weit ausserhalb seiner heutigen Flaeche liegt. Gespeichert
    // wurde die Position ja oft in einer BREITEREN/Vollbild-Sitzung.
    //
    // Zusaetzlicher Test, nur wenn ein anchor da ist: die gespeicherte
    // Geometrie muss das HEUTIGE Hauptfenster in einer nennenswerten
    // Flaeche ueberlappen -- nicht deckungsgleich (ein Fenster darf
    // bewusst an der Kante angedockt sein), aber auch nicht nur um ein
    // paar Bildpunkte. Erster Anlauf hier pruefte nur "beruehrt sich
    // ueberhaupt" (mit einem um die eigene Fenstergroesse aufgeweiteten
    // Rand) -- am Betreiber-Fall gemessen liess das ein Fenster durch,
    // das nur mit 61 von 597 Bildpunkten Breite ueberhaupt noch die
    // Hauptfenster-Kante beruehrte, der Rest hing frei auf dem
    // Schreibtisch. kMinOverlap ist dieselbe Groessenordnung, die
    // WindowPlacement.h fuer die Sichtbarkeitspruefung selbst vorschlaegt
    // (Schwaeche 2 dort): genug Flaeche, um das Fenster mit der Maus
    // wieder greifen zu koennen, nicht nur ein Pixel Kontakt.
    bool nearAnchor = true;
    if (anchorRect.isValid()) {
        static constexpr int kMinOverlapW = 80;
        static constexpr int kMinOverlapH = 40;
        const QRect overlap = anchorRect.intersected(g);
        nearAnchor = overlap.width() >= kMinOverlapW
                  && overlap.height() >= kMinOverlapH;
    }

    if (onScreen && !atOrigin && nearAnchor
        && g.width() >= minW && g.height() >= minH) {
        return;
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

} // namespace Longpath
