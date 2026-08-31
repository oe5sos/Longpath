// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/ToolWindow.cpp  (Longpath)
// =================================================================
// Siehe ToolWindow.h fuer Zweck und Modification history.
// =================================================================

#include "gui/ToolWindow.h"

#include "core/AppSettings.h"
#include "gui/FramelessResizer.h"
#include "gui/MacFloatingWindowBehavior.h"
#include "gui/StyleConstants.h"
#include "gui/WindowChrome.h"
#include "gui/WindowPlacement.h"

#include <QCloseEvent>
#include <QScreen>
#include <QVBoxLayout>

namespace Longpath {

ToolWindow::ToolWindow(QWidget* content, const QString& id,
                       const QString& title, QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint)
    , m_content(content)
    , m_id(id)
{
    setWindowTitle(title);
    setStyleSheet(QStringLiteral("ToolWindow { background: %1; }")
                      .arg(QLatin1String(Style::kPanelBg)));

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_titleBar = new WindowTitleBar(title, this);
    connect(m_titleBar, &WindowTitleBar::closeRequested, this, &QWidget::close);
    connect(m_titleBar, &WindowTitleBar::dockRequested, this, [this]() {
        emit dockRequested(m_id);
    });
    m_titleBar->setLockKey(QStringLiteral("Tool_%1").arg(id));
    lay->addWidget(m_titleBar);

    if (content) {
        content->setParent(this);
        content->show();
        lay->addWidget(content, 1);
    }

    // topMoveReserve == Leistenhoehe: der obere Streifen gehoert dem
    // Ziehen. Sonst schnappt der Resizer den Griff weg und das Fenster
    // laesst sich nicht mehr bewegen (AetherSDR #4266).
    FramelessResizer::install(this, 6, m_titleBar->height());
    attachResizeGrip(this);

    restoreGeometryState();

    // Betreiber 2026-08-31: "S-Meter usw. liegen frei am Desktop" --
    // restoreGeometryState() rief bislang NIE den vorhandenen
    // Klammer-Helfer auf, obwohl AppletFloatingWindow.h genau das schon
    // als gegeben behauptet (Dokumentationsfehler, siehe dort). Ohne
    // diese Zeile blieb eine aus einer breiteren/Vollbild-Sitzung
    // gespeicherte Position auch dann unangetastet, wenn `parent`
    // (i.d.R. MainWindow) seither viel kleiner geworden ist.
    ensureOnVisibleScreen(this, parent, QSize(300, 200));

    // Betreiber, wiederholt gemeldet: siehe AppletFloatingWindow.cpp,
    // derselbe Grund.
    enableFullScreenAuxiliaryBehavior(this);
}

ToolWindow::~ToolWindow() = default;

QWidget* ToolWindow::releaseContent()
{
    QWidget* c = m_content.data();
    if (!c) { return nullptr; }
    // Vor dem Loesen sichern: sowohl das Andocken (Titelleiste, ↙) als
    // auch das Schliessen (closeEvent emittiert dockRequested) laufen
    // beide hier durch, bevor MainWindow das Fenster wegwirft -- der
    // letzte Punkt, an dem geometry() noch die echte, zuletzt gezogene
    // Lage zeigt.
    saveGeometryState();
    // Aus dem Layout UND aus der Elternschaft. Nur removeWidget zu
    // rufen liesse den Inhalt Kind dieses Fensters — er stuerbe mit
    // ihm, und der Aufrufer haette einen baumelnden Zeiger auf etwas,
    // das er gerade zurueckbekommen zu haben glaubt.
    if (layout()) { layout()->removeWidget(c); }
    c->setParent(nullptr);
    m_content.clear();
    return c;
}

void ToolWindow::saveGeometryState()
{
    AppSettings::instance().setValue(
        QStringLiteral("ToolWindowGeometry_%1").arg(m_id), saveGeometry());
}

void ToolWindow::restoreGeometryState()
{
    const QByteArray st = AppSettings::instance()
        .value(QStringLiteral("ToolWindowGeometry_%1").arg(m_id))
        .toByteArray();
    // Leer beim ersten Mal -- dann entscheidet der Aufrufer per
    // applyDefaultSize()/move(), genau wie bisher.
    if (st.isEmpty()) { return; }
    restoreGeometry(st);
    // Verhindert, dass der Aufrufer applyDefaultSize() gleich danach die
    // gerade wiederhergestellte Groesse ueberschreibt -- dieselbe
    // "wer nachher zieht, darf es behalten"-Regel wie dort, nur dass
    // hier schon VOR dem ersten Zug ein gueltiger Zustand da ist.
    m_sizedOnce = true;
}

void ToolWindow::applyDefaultSize(const QSize& want)
{
    if (m_sizedOnce) { return; }   // wer nachher zieht, darf es behalten
    m_sizedOnce = true;

    QSize s = want;
    if (QScreen* sc = screen()) {
        const QSize avail = sc->availableSize();
        s.setWidth (qMin(s.width(),  (avail.width()  * 2) / 3));
        s.setHeight(qMin(s.height(), (avail.height() * 4) / 5));
    }
    resize(s);
}

void ToolWindow::closeEvent(QCloseEvent* ev)
{
    // Betreiber 2026-08-31: beweist, WANN diese Funktion beim Beenden
    // per rotem Punkt (nicht Cmd+Q/Menue) laeuft, im Vergleich zu
    // MainWindow::closeEvent()'s eigener Logzeile -- temporaer, bis der
    // eigentliche Fund feststeht.
    qWarning() << "ToolWindow::closeEvent() fuer" << m_id
               << "-- emittiert dockRequested";
    ev->accept();
    emit dockRequested(m_id);
}

} // namespace Longpath
