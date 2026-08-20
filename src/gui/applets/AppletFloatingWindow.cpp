// =================================================================
// src/gui/applets/AppletFloatingWindow.cpp  (NereusSDR)
// =================================================================
// Siehe AppletFloatingWindow.h für Zweck, Eigentum und Geometrie.
// =================================================================

#include "gui/applets/AppletFloatingWindow.h"

#include "gui/StyleConstants.h"
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
                          : QStringLiteral("NereusSDR"));
    setStyleSheet(QStringLiteral("AppletFloatingWindow { background: %1; }")
                      .arg(Style::kPanelBg));

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    if (applet) {
        // Die Spalte gibt 260 px vor; ein abgelöstes Applet darf breiter
        // werden, aber nicht schmaler als das, wofür es gebaut wurde.
        applet->setParent(this);
        applet->show();
        lay->addWidget(applet);
    }
    setMinimumWidth(Style::kAppletPanelW);

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
