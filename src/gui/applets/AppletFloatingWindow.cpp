// =================================================================
// src/gui/applets/AppletFloatingWindow.cpp  (NereusSDR)
// =================================================================
// Siehe AppletFloatingWindow.h für Zweck, Eigentum und Geometrie.
// =================================================================

#include "gui/applets/AppletFloatingWindow.h"

#include "gui/StyleConstants.h"
#include "gui/FramelessMoveHelper.h"
#include "gui/FramelessResizer.h"
#include "gui/WindowChrome.h"
#include "gui/applets/AppletWidget.h"

#include <QCloseEvent>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace Longpath {

AppletFloatingWindow::AppletFloatingWindow(AppletWidget* applet,
                                           const QString& panelId,
                                           int dockIndex, QWidget* parent)
    : QWidget(parent, Qt::Window)
    , m_applet(applet)
    , m_panelId(panelId)
    , m_dockIndex(dockIndex)
{
    setWindowTitle(applet ? applet->appletTitle()
                          : QStringLiteral("Longpath"));
    setStyleSheet(QStringLiteral("AppletFloatingWindow { background: %1; }")
                      .arg(Style::kPanelBg));

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Die eigene Titelleiste. Sie ersetzt die des Betriebssystems, die
    // wir gleich abschalten, und ist der Griff fuer das ganze Fenster.
    m_titleBar = new WindowTitleBar(
        applet ? applet->appletTitle() : QStringLiteral("Longpath"), this);
    connect(m_titleBar, &WindowTitleBar::closeRequested,
            this, &QWidget::close);
    connect(m_titleBar, &WindowTitleBar::dockRequested, this, [this]() {
        emit dockRequested(appletId());
    });
    lay->addWidget(m_titleBar);

    if (applet) {
        // Die Spalte gibt 260 px vor; ein abgelöstes Applet darf breiter
        // werden, aber nicht schmaler als das, wofür es gebaut wurde.
        applet->setParent(this);
        applet->show();
        lay->addWidget(applet);
    }
    setMinimumWidth(Style::kAppletPanelW);

    // ── Ziehen und Groessenaendern, wie bei AetherSDR ────────────────
    //
    // Der Betreiber, sinngemaess zum zehnten Mal am 2026-08-20: „jedes
    // Fenster muss sich an der oberen Leiste ueberall hinschieben
    // lassen, und unten rechts muss ein Griff sein, mit dem ich es
    // groesser und kleiner ziehen kann."
    //
    // Bis heute hing hier ein gewoehnliches Qt::Window mit dem Rahmen
    // des Betriebssystems. Das laesst sich zwar schieben, sieht aber
    // aus wie ein fremdes Fenster und hat oben eine graue Leiste, die
    // nicht zum Programm gehoert.
    //
    // AetherSDR macht es rahmenlos und legt zwei Helfer darunter, die
    // wir am 2026-08-20 portiert haben:
    //   FramelessMoveHelper  — Ziehen an einer Handhabe (hier: die
    //                          Titelleiste des Applets)
    //   FramelessResizer     — Groessenaendern an ALLEN Kanten und
    //                          Ecken, unten rechts eingeschlossen
    //
    // topMoveReserve == kTitleBarH: der obere Streifen gehoert dem
    // Ziehen, nicht dem Groessenaendern. Ohne das schnappt der Resizer
    // den Griff weg, und das Fenster laesst sich nicht mehr bewegen —
    // AetherSDR hat das als #4266 gelernt.
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    FramelessResizer::install(this, 6, m_titleBar->height());
    attachResizeGrip(this);

    m_settleTimer = new QTimer(this);
    m_settleTimer->setSingleShot(true);
    m_settleTimer->setInterval(kSettleMs);
    connect(m_settleTimer, &QTimer::timeout, this, [this]() {
        emit geometrySettled(appletId());
    });
}

AppletFloatingWindow::~AppletFloatingWindow() = default;

AppletWidget* AppletFloatingWindow::releaseApplet()
{
    AppletWidget* a = m_applet.data();
    if (!a) { return nullptr; }
    // Aus dem Layout UND aus der Elternschaft. Nur removeWidget zu
    // rufen liesse das Applet Kind dieses Fensters — es stürbe mit ihm,
    // und der Aufrufer hielte einen baumelnden Zeiger auf etwas, das er
    // gerade zurückbekommen zu haben glaubt.
    if (layout()) { layout()->removeWidget(a); }
    a->setParent(nullptr);
    m_applet.clear();
    return a;
}

void AppletFloatingWindow::closeEvent(QCloseEvent* ev)
{
    // Schliessen HEISST andocken. Ein Applet, das man wegklickt und das
    // danach nirgends mehr auftaucht, ist verloren — es hat, anders als
    // ein Werkzeugfenster, keinen eigenen Menüeintrag zum Wiederholen.
    // Die Sichtbarkeit regelt der AppletVisibilityController, und der
    // sagt zu diesem Applet weiterhin „sichtbar".
    //
    // Vor dem Andocken durchschreiben: der Zug, dem sofort das
    // Schliessen folgt, darf nicht in der Wartezeit hängen bleiben.
    if (m_settleTimer && m_settleTimer->isActive()) {
        m_settleTimer->stop();
        emit geometrySettled(appletId());
    }
    ev->accept();
    emit dockRequested(appletId());
}

void AppletFloatingWindow::moveEvent(QMoveEvent* ev)
{
    QWidget::moveEvent(ev);
    scheduleGeometryReport();
}

void AppletFloatingWindow::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
    scheduleGeometryReport();
}

void AppletFloatingWindow::scheduleGeometryReport()
{
    // isVisible(): das Herstellen beim Start setzt die Geometrie, bevor
    // das Fenster steht. Ohne diese Bedingung meldete allein das
    // Herstellen eine Änderung zurück ins Profil — und schriebe damit
    // gerade das, was es eben gelesen hat.
    if (m_settleTimer && isVisible()) {
        m_settleTimer->start();
    }
}

} // namespace Longpath
