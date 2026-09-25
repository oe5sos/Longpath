// =================================================================
// src/gui/MainWindow_SideArea.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Der Seitenbereich (SideAreaWindow) als weitere
// Heimat für Rotor/Log und die Applets — neben Spalte, eigenem Fenster
// und Kachel. Betreiber 2026-09-25, Variante 2 „Symbolleiste am Rand".
//
// ── Wer wo wohnt ─────────────────────────────────────────────────────
//
// Eine Seite im Seitenbereich ist für den Rest des Programms SICHTBAR
// (AppletVisibilityController::isVisible bleibt true) — ob sie gerade
// die aktive ist, ist Sache des Bereichs. Hinein kommt eine Seite, indem
// sie aus ihrer bisherigen Heimat genommen wird; heraus, indem sie in
// die Spalte (Applet) bzw. ins Dock (Rotor/Log) zurückgeht und von dort
// aus weiterzieht.
//
// Das Profil ("sideArea") wird ZULETZT angewandt: vorher löst die
// Anwendung den Bereich auf (alles zurück in die Grundheimat), dann
// stellt sie wie gewohnt alles andere her, dann baut sie den Bereich neu.
// So muss keine der vorhandenen Wiederherstellungen den Bereich kennen.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-25 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "gui/MainWindow.h"

#include "core/AppSettings.h"
#include "gui/LayoutProfiles.h"
#include "gui/SideAreaWindow.h"
#include "gui/WindowChrome.h"
#include "gui/ToolWindow.h"
#include "gui/WindowPlacement.h"
#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/AppletVisibilityController.h"
#include "gui/applets/AppletWidget.h"
#include "gui/widgets/RotorLogbookPanel.h"

#include <QApplication>
#include <QDockWidget>
#include <QGuiApplication>
#include <QMenu>
#include <QScreen>
#include <QVBoxLayout>

namespace Longpath {

namespace {
const QString kRotorPageId = QStringLiteral("WinRotorLog");
constexpr int kNeighbourSlackPx = 16;   // „grenzt an" — Lücke bis hierhin
}

bool MainWindow::sideAreaHas(const QString& id) const
{
    return m_sideArea && m_sideArea->hasPage(id);
}

SideAreaWindow* MainWindow::sideArea() const
{
    return m_sideArea.data();
}

SideAreaWindow* MainWindow::ensureSideArea()
{
    if (m_sideArea) { return m_sideArea; }
    m_sideArea = new SideAreaWindow(this);

    connect(m_sideArea, &SideAreaWindow::addRequested,
            this, &MainWindow::showSideAreaAddMenu);
    connect(m_sideArea, &SideAreaWindow::removeRequested, this,
            [this](const QString& id) {
        removeFromSideArea(id, SideAreaExit::OwnWindow);
        saveLayoutNow();
    });
    connect(m_sideArea, &SideAreaWindow::collapsedChanged,
            this, &MainWindow::onSideAreaCollapsed);
    connect(m_sideArea, &SideAreaWindow::stateSettled,
            this, [this]() { saveLayoutNow(); });
    return m_sideArea;
}

void MainWindow::saveLayoutNow()
{
    if (m_shuttingDown || !m_layoutProfiles) { return; }
    m_layoutProfiles->captureIntoCurrent();
    m_layoutProfiles->save();
}

QString MainWindow::sideAreaTitleFor(const QString& id) const
{
    if (id == kRotorPageId) { return QStringLiteral("Rotor / Log"); }
    if (m_appletVis) {
        const QString name = m_appletVis->displayName(id);
        if (!name.isEmpty()) { return name; }
    }
    return id;
}

// ── Hinein ───────────────────────────────────────────────────────────

bool MainWindow::addToSideArea(const QString& id, bool activate)
{
    if (id.isEmpty()) { return false; }
    SideAreaWindow* area = ensureSideArea();
    if (area->hasPage(id)) {
        if (activate) { area->setActive(id); }
        return true;
    }

    QWidget* content = nullptr;
    if (id == kRotorPageId) {
        RotorLogbookPanel* panel = ensureRotorPanel();
        if (!panel) { return false; }
        // Aus jeder der drei bisherigen Heimaten — dieselben Handgriffe
        // wie detachRotorPanel() / dockRotorPanel(), ohne deren
        // Einstellungs- und Sichtbarkeitsschreiberei.
        if (m_rotorWindow) {
            m_rotorWindow->hide();
            m_rotorWindow->releaseContent();
            m_rotorWindow->deleteLater();
            m_rotorWindow = nullptr;
        }
        if (auto* col = m_belowPane
                            ? qobject_cast<QVBoxLayout*>(m_belowPane->layout())
                            : nullptr) {
            col->removeWidget(panel);
        }
        if (m_rotorDock && m_rotorDock->widget() == panel) {
            m_rotorDock->setWidget(nullptr);
        }
        if (m_rotorHeader) { m_rotorHeader->hide(); }
        if (m_belowPane)   { m_belowPane->hide(); }
        if (m_rotorDock)   { m_rotorDock->hide(); }
        syncOuterSplitterHandle();
        content = panel;
    } else {
        AppletWidget* a = m_appletsById.value(id, nullptr);
        if (!a) { return false; }
        if (AppletFloatingWindow* win = m_floatingApplets.take(id)) {
            m_sideAreaDockIndex.insert(id, win->dockIndex());
            win->hide();
            win->releaseApplet();
            win->deleteLater();
        } else {
            if (m_canvasApplets.contains(id)) { returnAppletFromCanvas(id); }
            if (m_appletPanel && m_appletPanel->applets().contains(a)) {
                m_sideAreaDockIndex.insert(id, m_appletPanel->appletPosition(a));
                m_appletPanel->removeApplet(a);
            }
        }
        content = a;
    }

    area->addPage(id, sideAreaTitleFor(id), content);
    // Erst NACH dem Einlegen sichtbar schalten: applyAppletVisibility
    // sieht die Seite dann schon hier und legt das Applet nicht zurück
    // in die Spalte (siehe dort).
    if (m_appletVis) { m_appletVis->setVisible(id, true); }
    if (activate) { area->setActive(id); }
    if (!area->isVisible()) { area->show(); }
    return true;
}

// ── Heraus ───────────────────────────────────────────────────────────

void MainWindow::removeFromSideArea(const QString& id, SideAreaExit exit)
{
    if (!m_sideArea || !m_sideArea->hasPage(id)) { return; }
    // Wo die Seite stand — ein eigenes Fenster soll genau dort aufgehen.
    QRect here = m_sideArea->geometry();
    here.setWidth(qMax(here.width() - SideAreaWindow::kRailW, 200));
    QWidget* content = m_sideArea->takePage(id);
    if (!content) { return; }

    if (id == kRotorPageId) {
        // Zurück ins Dock; setRotorPanelBelow() kennt beide Grundheimaten
        // und liest m_rotorPanel, nicht den Rückgabewert.
        if (m_rotorDock) { m_rotorDock->setWidget(content); }
        const bool below = AppSettings::instance()
                               .value(QStringLiteral("RotorPanelBelow"),
                                      QStringLiteral("False"))
                               .toString() == QStringLiteral("True");
        switch (exit) {
        case SideAreaExit::OwnWindow:
            detachRotorPanel();
            if (m_rotorWindow) {
                m_rotorWindow->setGeometry(here);
                ensureOnVisibleScreen(m_rotorWindow, this, QSize(100, 80));
                m_rotorWindow->raise();
            }
            break;
        case SideAreaExit::Home:
            setRotorPanelBelow(below);
            break;
        case SideAreaExit::Silent:
            // Profilanwendung: nur zurück ins Dock, die Anwendung stellt
            // die Form selbst her.
            if (m_rotorDock) { m_rotorDock->hide(); }
            break;
        }
        return;
    }

    auto* a = qobject_cast<AppletWidget*>(content);
    if (!a || !m_appletPanel) { return; }
    m_appletPanel->addApplet(a);
    const int idx = m_sideAreaDockIndex.take(id);
    if (idx >= 0) { m_appletPanel->moveApplet(a, idx); }
    switch (exit) {
    case SideAreaExit::OwnWindow:
        m_appletPanel->setAppletVisible(a, true);
        detachApplet(a, m_appletPanel->appletPosition(a), here);
        if (AppletFloatingWindow* win = m_floatingApplets.value(id, nullptr)) {
            win->show();
            win->raise();
            win->activateWindow();
        }
        break;
    case SideAreaExit::Home:
    case SideAreaExit::Silent:
        m_appletPanel->setAppletVisible(
            a, m_appletVis && m_appletVis->isEffectivelyVisible(id));
        break;
    }
}

void MainWindow::dissolveSideArea(SideAreaExit exit)
{
    if (!m_sideArea) { return; }
    const QStringList ids = m_sideArea->pageIds();
    for (const QString& id : ids) { removeFromSideArea(id, exit); }
    m_sideArea->hide();
    m_sideArea->deleteLater();
    m_sideArea = nullptr;
    m_sideAreaDockIndex.clear();
}

// ── Menü: an / aus ───────────────────────────────────────────────────

void MainWindow::setSideAreaEnabled(bool on)
{
    if (!on) {
        // Aus heißt nicht „weg": jede Seite wird ein eigenes Fenster an
        // ihrer Stelle, nichts verschwindet.
        dissolveSideArea(SideAreaExit::OwnWindow);
        saveLayoutNow();
        return;
    }
    if (m_sideArea) {
        m_sideArea->show();
        m_sideArea->raise();
        return;
    }

    // Wohin: dorthin, wo Rotor/Log jetzt schwebt — „im Bereich des
    // Rotors". Sonst an den rechten Rand des Bildschirms.
    QRect where;
    if (m_rotorWindow && m_rotorWindow->isVisible()) {
        where = m_rotorWindow->geometry();
    } else if (QScreen* sc = screen()) {
        const QRect av = sc->availableGeometry();
        where = QRect(av.right() - 330, av.top() + 90, 330, av.height() - 180);
    }

    SideAreaWindow* area = ensureSideArea();
    // Rotor/Log zuerst, dann die Applets, die in der Spalte stehen, sichtbar
    // sein sollen und dort unter schwebenden Fenstern liegen — genau die,
    // die „versteckt" waren (beim Betreiber: RX). Eine frei sichtbare
    // Spalte bleibt, wie sie ist; sonst schluckte der Bereich in der
    // Grundeinstellung sechzehn Applets (tst_side_area_integration).
    // Weggerollt zählt hier nicht: das ist Rollen, nicht Verstecken.
    QStringList hidden;
    if (m_appletPanel && m_appletVis) {
        const auto inColumn = m_appletPanel->applets();
        for (AppletWidget* a : inColumn) {
            const QString id = panelIdFor(a);
            if (!id.isEmpty() && m_appletVis->isEffectivelyVisible(id)
                && a->isVisible() && appletHiddenInColumn(a, false)) {
                hidden << id;
            }
        }
    }
    addToSideArea(kRotorPageId, false);
    for (const QString& id : std::as_const(hidden)) { addToSideArea(id, false); }
    if (where.isValid()) {
        // Der RECHTE Rand bleibt, wo das Rotor-Fenster aufhörte — sonst
        // hinge die Leiste über den Bildschirmrand. Braucht der Bereich
        // mehr Breite als das Rotor-Fenster hatte, rückt ein angrenzender
        // Panadapter um genau so viel nach links ein.
        const int w = qMax(where.width(), area->minimumWidth());
        const int right = where.x() + where.width();
        area->setGeometry(right - w, where.y(), w, where.height());
        if (w > where.width()) {
            resizeNeighbourPans(where.x(), area->geometry(), where.width() - w);
        }
        ensureOnVisibleScreen(area, this, QSize(area->minimumWidth(), 160));
    }
    area->show();
    area->raise();
    saveLayoutNow();
}

void MainWindow::showSideAreaAddMenu(const QPoint& globalPos)
{
    if (!m_sideArea || !m_appletVis) { return; }
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose, true);
    const auto ids = m_appletVis->registeredIds();
    for (const QString& id : ids) {
        const bool isRotor = (id == kRotorPageId);
        if (!isRotor && !m_appletsById.contains(id)) { continue; }   // Win*/Chrome*
        if (m_sideArea->hasPage(id)) { continue; }
        if (!m_appletVis->isAvailable(id)) { continue; }
        menu->addAction(sideAreaTitleFor(id), this, [this, id]() {
            addToSideArea(id, true);
            saveLayoutNow();
        });
    }
    if (menu->isEmpty()) {
        menu->addAction(QStringLiteral("Alles liegt schon hier"))->setEnabled(false);
    }
    menu->popup(globalPos);
}

// ── Zuklappen: der Panadapter bekommt die Breite ────────────────────
//
// Betreiber 2026-09-25 zum gewählten Blatt: „Klick auf das schon aktive
// Symbol klappt den Bereich zu — der Panadapter bekommt die Breite
// zurück". Gezogen werden nur schwebende Panadapter, die an den Bereich
// GRENZEN (rechter Rand bis kNeighbourSlackPx an der linken Kante,
// senkrecht überlappend) — um genau die Breite, die frei wird bzw.
// wieder gebraucht wird. Andere Fenster bleiben, wo sie sind.
void MainWindow::onSideAreaCollapsed(bool collapsed, int delta)
{
    if (!m_sideArea || delta <= 0) { return; }
    const QRect area = m_sideArea->geometry();
    // Die linke Kante VOR dieser Änderung: zu → war um delta weiter links,
    // auf → stand um delta weiter rechts (die Leiste).
    const int oldLeft = collapsed ? area.x() - delta : area.x() + delta;
    resizeNeighbourPans(oldLeft, area, collapsed ? delta : -delta);
}

void MainWindow::resizeNeighbourPans(int edgeX, const QRect& area, int dx)
{
    if (dx == 0) { return; }
    const auto tops = QApplication::topLevelWidgets();
    for (QWidget* w : tops) {
        if (!w || !w->isVisible() || !w->inherits("Longpath::PanFloatingWindow")) {
            continue;
        }
        const QRect g = w->geometry();
        const int right = g.x() + g.width();
        const bool touches = qAbs(right - edgeX) <= kNeighbourSlackPx;
        const bool overlapsV = g.y() < area.y() + area.height()
            && area.y() < g.y() + g.height();
        if (!touches || !overlapsV) { continue; }
        w->resize(qMax(w->minimumWidth(), g.width() + dx), g.height());
    }
}

// ── Profil ───────────────────────────────────────────────────────────

void MainWindow::applySideAreaState(const QVariantMap& s)
{
    const QStringList pages = s.value(QStringLiteral("pages")).toStringList();
    if (pages.isEmpty()) { return; }
    SideAreaWindow* area = ensureSideArea();
    for (const QString& id : pages) {
        const QString key = (id == kRotorPageId) ? id : canonicalAppletKey(id);
        addToSideArea(key, false);
    }
    const QRect r(s.value(QStringLiteral("x")).toInt(),
                  s.value(QStringLiteral("y")).toInt(),
                  s.value(QStringLiteral("w")).toInt(),
                  s.value(QStringLiteral("h")).toInt());
    if (r.width() >= 100 && r.height() >= 80) {
        area->setGeometry(r);
        ensureOnVisibleScreen(area, this, QSize(area->minimumWidth(), 160));
    }
    const QString active = s.value(QStringLiteral("active")).toString();
    if (area->hasPage(active)) { area->setActive(active); }
    area->show();
    if (s.value(QStringLiteral("collapsed")).toBool()) {
        // Nach show(): zuklappen rechnet mit der echten Lage. Die
        // Panadapter-Nachbarn wurden im Zugeklappt-Zustand gespeichert
        // und stehen schon breit — nicht noch einmal ziehen.
        QSignalBlocker block(area);
        area->setCollapsed(true);
    }
}

} // namespace Longpath
