// src/gui/applets/AppletPanelWidget.cpp

// =================================================================
// src/gui/applets/AppletPanelWidget.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Scrollable applet-panel pattern (260px fixed width, 16px title
//                 bars) ported from AetherSDR `src/gui/AppletPanel.{h,cpp}`.
// =================================================================

#include "AppletPanelWidget.h"
#include "AppletGrid.h"
#include "AppletWidget.h"
#include "GridCellWidget.h"
#include "gui/StyleConstants.h"
// Task 40 (Phase 3P-II): analog S-Meter replaces the composite MeterWidget
// header.  The right-click context menu (Tasks 38/39) is the only entry
// point for RX/TX mode selection and peak-hold settings; the inline settings
// strip that AetherSDR renders inside the AppletPanel is intentionally absent.

#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QResizeEvent>
#include <QPushButton>
#include <QMenu>
#include <QMouseEvent>
#include <QContextMenuEvent>

namespace NereusSDR {

AppletPanelWidget::AppletPanelWidget(QWidget* parent)
    : QWidget(parent)
{
    // Minimum width matches AetherSDR AppletPanel (260px), but allow
    // dynamic expansion when the user drags the splitter handle wider.
    setMinimumWidth(Style::kAppletPanelW);
    setStyleSheet(QStringLiteral("AppletPanelWidget { background: %1; }")
                      .arg(Style::kPanelBg));

    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(0);

    // ☰ menu button — created here as a hidden child of `this`. When
    // setHeaderWidget runs, wrapWithTitleBar will reparent it into the
    // S-Meter title bar (right end, after the title label). This avoids
    // a dedicated banner row that would compete for clicks with the
    // master-volume strip above the panel.
    // ── Das ☰ ist am 2026-08-18 weggefallen ──────────────────────
    //
    // Es sass in der Titelleiste des festen S-Meter-Kopfes und oeffnete
    // die Applet-Liste. Mit dem Kopf faellt es mit — und das ist kein
    // Verlust: das + in der Kopfleiste und Ansicht > Container >
    // Applets machen dieselbe Arbeit, und der Auswaehler hinter dem +
    // kann mehr (Kategorien, Suche, Schlagwoerter).
    //
    // Vom Betreiber am 2026-08-18 bestaetigt: „Das ≡ fällt weg, ja.
    // Deine Begründung trägt."

    // Fixed header area (above scroll) — for MeterWidget / S-Meter
    m_headerLayout = new QVBoxLayout;
    m_headerLayout->setContentsMargins(0, 0, 0, 0);
    m_headerLayout->setSpacing(0);
    m_rootLayout->addLayout(m_headerLayout);

    // Scrollable area for the applet stack (below header)
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background: %1; border: none; }"
        "QScrollBar:vertical {"
        "  background: %1; width: 8px; border: none;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: %2; border-radius: 4px; min-height: 20px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
    ).arg(Style::kPanelBg, Style::kGroove));

    // Das Raster im Rollbereich. Schritt 1: eine Spalte, ein Applet je
    // Feld — dasselbe Bild wie der Stapel davor, aber Ort statt
    // Reihenfolge. Siehe AppletGrid.h.
    m_grid = new AppletGrid(m_scrollArea);
    m_grid->setStyleSheet(QStringLiteral("background: %1;").arg(Style::kPanelBg));
    // 8 px matches the QScrollBar:vertical width in the stylesheet above;
    // Qt::ScrollBarAsNeeded means the gutter is wasted when the bar hides,
    // but 8 px is negligible and avoids a layout reflow on bar show/hide.
    m_grid->setContentsMargins(0, 0, 8, 0);

    m_scrollArea->setWidget(m_grid);
    m_rootLayout->addWidget(m_scrollArea);

    // ── Kein fester Kopf mehr (2026-08-18) ───────────────────────
    //
    // Hier stand die analoge SMeterWidget-Anzeige als feste Kopfzeile
    // ueber der Spalte. Sie war das zweite S-Meter — das erste ist das
    // Zeigerinstrument, das als Applet in der Spalte steht.
    //
    // Entdoppelt auf Weisung des Betreibers, 2026-08-18: „fester Kopf
    // weg, Zeiger-Instrument bleibt, RX-Quellen und Spitzenhaltung ins
    // Rechtsklickmenü."
    //
    // Alles, was der Kopf konnte, hat vorher eine Heimat bekommen:
    //   vier RX-Quellen      Kennungen in ReadingSource, waehlbar im
    //                        Rechtsklick jedes Instruments
    //   Spitzenhaltung       ebendort (an/aus, Haltezeit, Zuruecksetzen)
    //   Leistung und SWR     TxApplet, samt 2-kW-Skala bei PGXL/RF-Kit
    //   Rauschflur, RADE-SNR eigene Kennungen, ebenfalls waehlbar
    //   Pegel, Kompression   waren tot — setMicMeters hatte im ganzen
    //                        Baum keinen Aufrufer
    //
    // Die Kopfzeilen-Mechanik (setHeaderWidget/clearHeaderWidget) bleibt
    // stehen: sie ist allgemein und kostet nichts, solange sie niemand
    // ruft. Der Panadapter soll im Zielbild selbst ein Feld werden, und
    // dann ist sie womoeglich wieder die richtige Stelle.
}

void AppletPanelWidget::setHeaderWidget(QWidget* widget, const QString& title,
                                         float aspectRatio)
{
    if (!widget) { return; }
    clearHeaderWidget();

    m_headerAspect = aspectRatio;
    m_headerWidget = widget;

    // Let the widget expand to fill width; height set dynamically in resizeEvent
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Pass the ☰ menu button as the trailing widget so it lives in the
    // S-Meter title bar (right end). See AppletPanelWidget.h comment for
    // why we don't use a dedicated banner row.
    QWidget* wrapped = wrapWithTitleBar(widget, title);
    wrapped->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_headerWrapper = wrapped;
    m_headerLayout->addWidget(wrapped);

    // Set initial height based on current width
    int h = qMax(80, static_cast<int>(width() / aspectRatio));
    widget->setFixedHeight(h);
}

void AppletPanelWidget::clearHeaderWidget()
{
    if (m_headerWrapper) {
        m_headerLayout->removeWidget(m_headerWrapper);
        // Detach from the widget tree SYNCHRONOUSLY before deleteLater.
        // The wrapper's MeterWidget child is a WA_NativeWindow QRhiWidget
        // with a live D3D11 swapchain. If the wrapper lingers in this
        // panel's parent chain across an upcoming container reparent, the
        // new MeterWidget's CreateSwapChainForHwnd returns E_ACCESSDENIED
        // — Windows refuses to attach a second swapchain while the old
        // native child is still under the parent HWND.
        m_headerWrapper->hide();
        m_headerWrapper->setParent(nullptr);
        m_headerWrapper->deleteLater();
        m_headerWrapper = nullptr;
    }
    m_headerWidget = nullptr;
    m_headerAspect = 0.0f;
}

void AppletPanelWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_headerWidget && m_headerAspect > 0.0f) {
        int w = event->size().width();
        int totalH = event->size().height();
        int h = static_cast<int>(w / m_headerAspect);
        // Clamp: min 80px, max 40% of total panel height so applets aren't crowded
        h = qBound(80, h, totalH * 40 / 100);
        m_headerWidget->setFixedHeight(h);
    }
}

void AppletPanelWidget::addApplet(AppletWidget* applet)
{
    if (!applet) { return; }
    if (m_applets.contains(applet)) { return; }  // already present
    m_applets.append(applet);

    GridCellWidget* cell = m_grid->appendInOwnCell(applet);
    if (!cell) { m_applets.removeOne(applet); return; }
    m_wrappers[applet] = cell;
    // Die Kopfleiste ist der Griff — nur sie, nicht das Applet
    // darunter, sonst zöge jeder Regler das ganze Feld mit.
    cell->titleBar()->installEventFilter(this);
    m_titleBars.insert(cell->titleBar(), cell);
}

void AppletPanelWidget::removeApplet(AppletWidget* applet)
{
    if (!applet) { return; }
    if (!m_applets.contains(applet)) { return; }

    GridCellWidget* wrapper = m_wrappers.value(applet, nullptr);
    if (wrapper) {
        // Die Titelleiste MIT austragen. Sie stirbt gleich als Kind der
        // Hülle, ihr Eintrag in m_titleBars aber blieb stehen — ein
        // Zeiger auf Gelöschtes in einer Karte, die der Ereignisfilter
        // bei JEDEM Mausereignis befragt. Solange removeApplet nirgends
        // aufgerufen wurde, war das folgenlos; mit dem Ablösen
        // (2026-08-16) ist es der Normalfall.
        const QList<QWidget*> bars = m_titleBars.keys(wrapper);
        for (QWidget* b : bars) { m_titleBars.remove(b); }
        // takeWidget haengt aus, OHNE zu loeschen, und raeumt das Feld
        // ab, wenn es dabei leer wird. Der Aufrufer bekommt ein
        // lebendes Applet zurueck — darauf beruht das Abloesen.
        m_grid->takeWidget(applet);
        m_wrappers.remove(applet);
    }
    m_applets.removeOne(applet);

    // Ein Applet, das gerade gezogen wird und dabei ausgebaut wird, darf
    // keinen Zeiger hinterlassen: der nächste Mausbericht griffe sonst
    // auf etwas zu, das nicht mehr in dieser Spalte steht.
    if (m_dragApplet == applet) {
        m_dragApplet = nullptr;
        m_dragging = false;
    }
}

void AppletPanelWidget::setAppletVisible(AppletWidget* applet, bool visible)
{
    if (!applet) { return; }
    QWidget* wrapper = m_wrappers.value(applet, nullptr);
    if (!wrapper) { return; }  // applet not in this panel
    wrapper->setVisible(visible);
}

void AppletPanelWidget::addWidget(QWidget* widget, const QString& title)
{
    // Ein rohes Widget bekommt genauso ein Feld wie ein Applet — der
    // Behaelter haelt Widgets, nicht Applets (Festlegung des
    // Betreibers, siehe GridCell.h). Nur traegt es keine Panelkennung
    // und steht darum nicht in m_applets: es laesst sich nicht
    // ausblenden und nicht abloesen.
    if (!widget) { return; }
    GridCellWidget* cell = m_grid->appendInOwnCell(widget);
    if (!cell) { return; }
    cell->setTitle(title);
}

QWidget* AppletPanelWidget::wrapWithTitleBar(QWidget* child, const QString& title,
                                              QWidget* trailing)
{
    // Wrapper container
    auto* wrapper = new QWidget(this);
    auto* wrapLayout = new QVBoxLayout(wrapper);
    wrapLayout->setContentsMargins(0, 0, 0, 0);
    wrapLayout->setSpacing(0);

    // Title bar — from AetherSDR AppletTitleBar:
    // 16px height, gradient, grip dots + title label.
    // Bumped to 22px when a trailing widget is hosted (the S-Meter
    // header gets the ☰ menu button on its right end).
    auto* titleBar = new QWidget(wrapper);
    const int titleBarHeight = trailing ? 22 : Style::kTitleBarH;
    titleBar->setFixedHeight(titleBarHeight);
    titleBar->setStyleSheet(Style::titleBarStyle());

    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(2, 0, 4, 0);
    titleLayout->setSpacing(4);

    // Grip dots (⋮⋮) — from AetherSDR: #607080, 10px
    auto* grip = new QLabel(QStringLiteral("\u22EE\u22EE"), titleBar);
    grip->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; background: transparent; }"
    ).arg(Style::kTextScale));
    titleLayout->addWidget(grip);

    // Title label — from AetherSDR: #8aa8c0, 10px bold
    auto* label = new QLabel(title, titleBar);
    label->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; font-weight: bold;"
        " background: transparent; }"
    ).arg(Style::kTitleText));
    titleLayout->addWidget(label);
    titleLayout->addStretch();

    if (trailing) {
        // Reparent under the title bar (Qt does this automatically on
        // addWidget). The trailing widget keeps its current visibility
        // state — callers manage show/hide via their own API (e.g.
        // setBannerMenu for the ☰ button).
        titleLayout->addWidget(trailing);
    }

    wrapLayout->addWidget(titleBar);
    wrapLayout->addWidget(child);

    // Die Titelleiste ist der Griff. Nur sie — nicht das Applet
    // darunter, sonst zöge jeder Regler das ganze Widget mit.
    titleBar->installEventFilter(this);
    titleBar->setCursor(Qt::OpenHandCursor);
    m_titleBars.insert(titleBar, wrapper);

    return wrapper;
}

// ── Verschieben ──────────────────────────────────────────────────────

int AppletPanelWidget::stackIndexOf(QWidget* wrapper) const
{
    auto* cell = qobject_cast<GridCellWidget*>(wrapper);
    if (!cell || !m_grid) { return -1; }
    return m_grid->positionOf(cell->cellId());
}

AppletWidget* AppletPanelWidget::appletForWrapper(QWidget* wrapper) const
{
    for (auto it = m_wrappers.constBegin(); it != m_wrappers.constEnd(); ++it) {
        if (it.value() == wrapper) { return it.key(); }
    }
    return nullptr;   // Kopfleiste oder ein rohes Widget — kein Applet
}

bool AppletPanelWidget::moveApplet(AppletWidget* applet, int toIndex)
{
    if (!applet || !m_grid) { return false; }
    GridCellWidget* wrapper = m_wrappers.value(applet, nullptr);
    if (!wrapper) { return false; }

    const int from = stackIndexOf(wrapper);
    if (from < 0) { return false; }

    // Die Grenze ist jetzt die Zahl der FELDER. Vorher war es die Zahl
    // der Stapeleintraege minus der Dehnung am Ende — dieselbe Sache,
    // nur dass das Raster keine Dehnung als Eintrag fuehrt.
    const int to = qBound(0, toIndex, qMax(0, m_grid->cells().size() - 1));
    if (to == from) { return false; }

    m_grid->moveCell(wrapper->cellId(), to);

    // m_applets führt dieselbe Reihenfolge nach. Sie ist das, was
    // applets() herausgibt und was gespeichert wird — liefe sie
    // auseinander, käme nach dem Neustart eine andere Anordnung heraus
    // als die, die man hinterlassen hat.
    //
    // Die Stelle kommt jetzt DIREKT aus dem Raster: es ist die einzige
    // Quelle fuer die Anzeigefolge. appletPosForStackIndex() hat sie
    // frueher nachgerechnet, weil im Stapel auch Nicht-Applets lagen;
    // im Raster ist die Zuordnung Feld → Inhalt eindeutig, und eine
    // Nachrechnung waere eine zweite Quelle fuer dieselbe Aussage.
    const QList<AppletWidget*> inGrid = m_grid->applets();
    QList<AppletWidget*> reordered;
    for (AppletWidget* a : inGrid) {
        if (m_applets.contains(a)) { reordered.append(a); }
    }
    for (AppletWidget* a : m_applets) {
        if (!reordered.contains(a)) { reordered.append(a); }
    }
    m_applets = reordered;
    return true;
}

void AppletPanelWidget::setAppletOrder(const QList<AppletWidget*>& order)
{
    int slot = 0;
    for (AppletWidget* a : order) {
        if (!a || !m_wrappers.contains(a)) { continue; }
        moveApplet(a, slot);
        ++slot;
    }
    // Was nicht in der Liste stand, bleibt hinten stehen. Es hier
    // wegzuräumen wäre der bequeme Weg und der falsche: nach einem
    // Update kennt die gespeicherte Liste die neuen Widgets nicht, und
    // sie sollen erscheinen, nicht verschwinden.
}

bool AppletPanelWidget::eventFilter(QObject* watched, QEvent* event)
{
    auto* bar = qobject_cast<QWidget*>(watched);
    if (!bar || !m_titleBars.contains(bar)) {
        return QWidget::eventFilter(watched, event);
    }
    QWidget* wrapper = m_titleBars.value(bar);
    AppletWidget* applet = appletForWrapper(wrapper);

    switch (event->type()) {
    case QEvent::ContextMenu: {
        // Der zuverlässige Weg. Auch erreichbar, wenn der Zug misslingt
        // — auf einem Trackpad, mit einer zittrigen Hand, oder wenn die
        // Spalte so breit gezogen ist, dass 40 px seitlich nicht
        // ausreichen, um sie zu verlassen.
        if (!applet) { break; }
        auto* ce = static_cast<QContextMenuEvent*>(event);
        showTitleBarMenu(bar, ce->globalPos());
        return true;
    }
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() != Qt::LeftButton || !applet) { break; }
        m_dragApplet = applet;
        m_dragStartY = static_cast<int>(me->globalPosition().y());
        m_dragging = false;
        break;
    }
    case QEvent::MouseMove: {
        if (!m_dragApplet) { break; }
        auto* me = static_cast<QMouseEvent*>(event);
        const int y = static_cast<int>(me->globalPosition().y());
        if (!m_dragging) {
            // Erst ab einer Schwelle. Ohne sie verschiebt ein Klick mit
            // zitternder Hand das Widget, und der Betreiber sucht danach
            // sein Panel.
            if (qAbs(y - m_dragStartY) < kDragThresholdPx) { break; }
            m_dragging = true;
            bar->setCursor(Qt::ClosedHandCursor);
        }
        // Nur senkrecht, nur innerhalb dieser Spalte. Warum hier kein
        // Ablösen stattfindet, steht bei appletDetachRequested im
        // Header.
        dragTo(y);
        return true;
    }
    case QEvent::MouseButtonRelease: {
        const bool moved = m_dragging;
        if (m_dragApplet) { bar->setCursor(Qt::OpenHandCursor); }
        m_dragApplet = nullptr;
        m_dragging = false;
        if (moved) {
            // Einmal am Ende, nicht bei jedem Pixel.
            emit appletsReordered();
            return true;
        }
        break;
    }
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

int AppletPanelWidget::appletPosition(AppletWidget* applet) const
{
    return m_applets.indexOf(applet);
}

QMenu* AppletPanelWidget::buildTitleBarMenuForTesting(AppletWidget* applet)
{
    if (!applet || !m_wrappers.contains(applet)) { return nullptr; }
    auto* menu = new QMenu(this);
    // Wortlaut vom Betreiber vorgegeben (2026-08-16). Die Auslösung
    // hängt AN DER AKTION, nicht am Rückgabewert von exec() — nur so
    // prüft ein Test denselben Weg, den der Bediener nimmt, statt einen
    // nachgebauten daneben.
    QAction* detach = menu->addAction(QStringLiteral("Als Fenster ablösen"));
    connect(detach, &QAction::triggered, this,
            [this, applet]() {
        emit appletDetachRequested(applet, appletPosition(applet));
    });
    return menu;
}

void AppletPanelWidget::showTitleBarMenu(QWidget* titleBar,
                                          const QPoint& globalPos)
{
    QWidget* wrapper = m_titleBars.value(titleBar, nullptr);
    AppletWidget* applet = appletForWrapper(wrapper);
    if (!applet) { return; }

    QMenu* menu = buildTitleBarMenuForTesting(applet);
    if (!menu) { return; }
    // Kein WA_DeleteOnClose dazu: das schlösse mit dem deleteLater()
    // unten zwei Freigaben für dasselbe Menü ein.
    menu->exec(globalPos);
    menu->deleteLater();
}

void AppletPanelWidget::dragTo(int globalY)
{
    GridCellWidget* wrapper = m_wrappers.value(m_dragApplet, nullptr);
    if (!wrapper || !m_grid) { return; }
    const int here = stackIndexOf(wrapper);
    if (here < 0) { return; }

    // Live umsortieren statt ein Einfügezeichen zu malen: sobald der
    // Zeiger die Mitte eines Nachbarn überschreitet, tauschen die
    // beiden die Plätze. Das Bild zeigt dann immer, was beim Loslassen
    // herauskommt — bei einem Einfügestrich muss man es sich denken.
    const QList<GridCellWidget*> fields = m_grid->cells();
    for (int i = 0; i < fields.size(); ++i) {
        if (i == here) { continue; }
        QWidget* other = fields.at(i);
        if (!other || !other->isVisible()) { continue; }
        // Nur mit Applets tauschen. Die Kopfleiste des S-Meters ist
        // fest verbaut; sie unter ein Applet zu schieben, wäre ein Zug,
        // den niemand rückgängig machen kann, weil sie keinen Eintrag
        // im Auswähler hat.
        if (!appletForWrapper(other)) { continue; }

        const QPoint topLeft = other->mapToGlobal(QPoint(0, 0));
        const int mid = topLeft.y() + other->height() / 2;
        if ((i < here && globalY < mid) || (i > here && globalY > mid)) {
            moveApplet(m_dragApplet, i);
            return;
        }
    }
}

} // namespace NereusSDR
