// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/widgets/RasterOverlay.cpp  (Longpath)
// =================================================================
// Siehe RasterOverlay.h — warum es in Bildschirmkoordinaten rechnet.
// =================================================================

#include "gui/widgets/RasterOverlay.h"

#include "gui/StyleConstants.h"
#include "gui/WindowPlacement.h"

#include <QPainter>
#include <QPaintEvent>

namespace Longpath {

RasterOverlay::RasterOverlay(QWidget* parent)
    : QWidget(parent), m_grid(kSnapGridPx)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);
}

void RasterOverlay::setGrid(int grid)
{
    if (grid <= 1 || grid == m_grid) { return; }
    m_grid = grid;
    update();
}

void RasterOverlay::paintEvent(QPaintEvent* ev)
{
    if (m_grid <= 1) { return; }

    QPainter p(this);
    // Haarlinie, kein Rahmen: das Raster soll zeigen, wo die Linien
    // liegen, und nicht selbst ins Auge springen.
    QPen stift(QColor(255, 255, 255, 28));
    stift.setWidth(1);
    stift.setCosmetic(true);
    p.setPen(stift);

    // In BILDSCHIRMKOORDINATEN rechnen — dort rastet
    // `snapToGridAfterSettle`. Der Versatz ist die Lage dieses Blatts
    // auf dem Schirm; ohne ihn zeigte das Bild ein anderes Raster als
    // das, an dem die Fenster tatsaechlich haengen.
    const QPoint ursprung = mapToGlobal(QPoint(0, 0));
    const QRect  feld     = ev->rect();

    const int ersteX = ((ursprung.x() + feld.left()  + m_grid - 1) / m_grid) * m_grid;
    const int ersteY = ((ursprung.y() + feld.top()   + m_grid - 1) / m_grid) * m_grid;

    for (int gx = ersteX; gx <= ursprung.x() + feld.right(); gx += m_grid) {
        const int x = gx - ursprung.x();
        p.drawLine(x, feld.top(), x, feld.bottom());
    }
    for (int gy = ersteY; gy <= ursprung.y() + feld.bottom(); gy += m_grid) {
        const int y = gy - ursprung.y();
        p.drawLine(feld.left(), y, feld.right(), y);
    }
}

}  // namespace Longpath
