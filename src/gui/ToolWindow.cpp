// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/ToolWindow.cpp  (Longpath)
// =================================================================
// Siehe ToolWindow.h fuer Zweck und Modification history.
// =================================================================

#include "gui/ToolWindow.h"

#include "gui/FramelessResizer.h"
#include "gui/StyleConstants.h"
#include "gui/WindowChrome.h"

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
}

ToolWindow::~ToolWindow() = default;

QWidget* ToolWindow::releaseContent()
{
    QWidget* c = m_content.data();
    if (!c) { return nullptr; }
    // Aus dem Layout UND aus der Elternschaft. Nur removeWidget zu
    // rufen liesse den Inhalt Kind dieses Fensters — er stuerbe mit
    // ihm, und der Aufrufer haette einen baumelnden Zeiger auf etwas,
    // das er gerade zurueckbekommen zu haben glaubt.
    if (layout()) { layout()->removeWidget(c); }
    c->setParent(nullptr);
    m_content.clear();
    return c;
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
    ev->accept();
    emit dockRequested(m_id);
}

} // namespace Longpath
