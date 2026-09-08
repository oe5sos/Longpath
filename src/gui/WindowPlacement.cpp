// =================================================================
// src/gui/WindowPlacement.cpp  (NereusSDR)
// =================================================================
// Siehe WindowPlacement.h für Herkunft und die zwei bekannten
// Schwächen, die hier bewusst unverändert bleiben.
// =================================================================

#include "gui/WindowPlacement.h"

#include <cmath>

#include <QGuiApplication>
#include <QHash>
#include <QLoggingCategory>
#include <QRect>
#include <QScreen>
#include <QTimer>
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
    // Betreiber 2026-08-31: "vor allem rotor, bandfilter und panadapter
    // sind immer anders als abgespeichert" -- IMMER, nicht gelegentlich.
    // Ursache: alle drei werden waehrend MainWindows eigenem Konstruktor
    // wiederhergestellt (Rotor-ToolWindow ueber detachRotorPanel() beim
    // Start, Bandfilter/Frequenz ueber das Profil-Apply, beides lange vor
    // dem ersten show()). Zu diesem Zeitpunkt hat `anchor` (MainWindow)
    // die Bildschirm-Ebene noch nie erreicht: restoreGeometry() setzt nur
    // die FENSTER-Grosse (z.B. 1280x800), der Vollbild/Maximiert-Zustand
    // greift erst, wenn das Betriebssystem das Fenster tatsaechlich zeigt
    // -- showFullScreen()/showMaximized() dafuer laufen selbst noch
    // spaeter in derselben Profil-Anwenden-Kette. Eine Position, die in
    // einer BREITEN/Vollbild-Sitzung gespeichert wurde, ueberlappt die zu
    // diesem fruehen Zeitpunkt noch schmale Ersatzgroesse fast nie -- und
    // wurde dadurch bei JEDEM Start/Import verworfen, nicht nur manchmal.
    // Deshalb gilt die Ueberlapp-Pruefung nur, wenn der Anker schon
    // sichtbar ist; vorher ist seine jetzige Groesse kein verlaesslicher
    // Massstab, und die gespeicherte Position verdient das Vertrauen.
    bool nearAnchor = true;
    if (anchorRect.isValid() && anchor->window()->isVisible()) {
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

QPoint snappedTopLeft(const QPoint& pos, int grid)
{
    if (grid <= 1) { return pos; }
    const auto snapAxis = [grid](int v) {
        return static_cast<int>(std::lround(v / static_cast<double>(grid))) * grid;
    };
    return QPoint(snapAxis(pos.x()), snapAxis(pos.y()));
}

void snapToGridAfterSettle(QWidget* w, int grid, int delayMs)
{
    if (!w) { return; }

    // Ein Debounce-Timer je Fenster, angelegt beim ersten Aufruf und an
    // `w` gebunden (QObject-Elternschaft), damit keine der vier
    // aufrufenden Klassen selbst ein Timer-Feld fuehren muss -- der
    // Aufruf aus moveEvent() bleibt eine Zeile. `timers` lebt nur
    // hauptthread-seitig, wie jeder Widget-/Timer-Code ohnehin.
    static QHash<QWidget*, QTimer*> timers;
    QTimer*& timer = timers[w];
    if (!timer) {
        timer = new QTimer(w);
        timer->setSingleShot(true);
        QObject::connect(timer, &QTimer::timeout, w, [w, grid, delayMs, timer]() {
            // Betreiber 2026-09-02: TX-Fenster liess sich waehrend
            // eines Kanten-Groessenzugs (oben/links) nicht mehr
            // bewegen. Ursache: ein solcher Zug verschiebt den
            // Fensterursprung genauso wie ein Titelbalken-Zug -- beide
            // laufen ueber denselben nativen OS-Griff
            // (startSystemResize), FramelessResizer haelt selbst
            // keinen abfragbaren Zugzustand, den man hier pruefen
            // koennte. Haengt beim Ablauf dieses Timers noch eine
            // Maustaste, ist der Bediener vermutlich mitten in genau
            // so einem Zug (kurze Pause, nicht losgelassen) --
            // verschieben statt schnappen, sonst kaempft dieses
            // move() mit dem noch laufenden nativen Zug.
            if (QGuiApplication::mouseButtons() != Qt::NoButton) {
                timer->start(delayMs);
                return;
            }
            const QPoint snapped = snappedTopLeft(w->pos(), grid);
            // Nur bewegen, wenn es tatsaechlich abweicht -- der
            // Rueckstoss dieses move() loest selbst ein moveEvent()
            // aus, das diese Funktion erneut aufruft; beim zweiten
            // Mal stimmt die Position schon, also keine dritte Runde.
            if (snapped != w->pos()) { w->move(snapped); }
        });
        QObject::connect(w, &QObject::destroyed, w, [w]() {
            timers.remove(w);
        });
    }
    timer->start(delayMs);
}

} // namespace Longpath
