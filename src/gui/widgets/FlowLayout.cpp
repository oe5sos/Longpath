// SPDX-License-Identifier: GPL-3.0-or-later
// src/gui/widgets/FlowLayout.cpp  (Longpath) — see FlowLayout.h
//
// Longpath-original. No Thetis port.
#include "gui/widgets/FlowLayout.h"

#include <QSpacerItem>
#include <QWidget>

namespace Longpath {

FlowLayout::FlowLayout(QWidget* parent, int hSpacing, int vSpacing)
    : QLayout(parent)
    , m_hSpace(hSpacing)
    , m_vSpace(vSpacing)
{
    setContentsMargins(0, 0, 0, 0);
}

FlowLayout::~FlowLayout()
{
    while (QLayoutItem* it = takeAt(0)) { delete it; }
}

void FlowLayout::addItem(QLayoutItem* item)
{
    m_items.append(item);
    invalidate();
}

void FlowLayout::addStretch()
{
    addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
}

QLayoutItem* FlowLayout::takeAt(int i)
{
    if (i < 0 || i >= m_items.size()) { return nullptr; }
    QLayoutItem* it = m_items.takeAt(i);
    invalidate();
    return it;
}

bool FlowLayout::wantsWidth(QLayoutItem* item)
{
    if (QSpacerItem* sp = item->spacerItem()) {
        return sp->sizePolicy().horizontalPolicy() & QSizePolicy::ExpandFlag;
    }
    if (const QWidget* w = item->widget()) {
        return w->sizePolicy().horizontalPolicy() & QSizePolicy::ExpandFlag;
    }
    return item->expandingDirections() & Qt::Horizontal;
}

int FlowLayout::heightForWidth(int width) const
{
    return layOut(QRect(0, 0, width, 0), false);
}

QSize FlowLayout::sizeHint() const
{
    // Alles in einer Zeile: so breit wie die Summe, so hoch wie das
    // hoechste Element. Der Umbruch kommt erst, wenn weniger da ist.
    int w = 0, h = 0, n = 0;
    for (QLayoutItem* it : m_items) {
        if (it->spacerItem()) { continue; }
        const QSize s = it->sizeHint();
        w += s.width() + (n > 0 ? m_hSpace : 0);
        h = qMax(h, s.height());
        ++n;
    }
    const QMargins m = contentsMargins();
    return QSize(w + m.left() + m.right(), h + m.top() + m.bottom());
}

QSize FlowLayout::minimumSize() const
{
    int w = 0, h = 0;
    for (QLayoutItem* it : m_items) {
        if (it->spacerItem()) { continue; }
        const QSize s = it->minimumSize();
        w = qMax(w, s.width());
        h = qMax(h, s.height());
    }
    const QMargins m = contentsMargins();
    return QSize(w + m.left() + m.right(), h + m.top() + m.bottom());
}

void FlowLayout::setGeometry(const QRect& rect)
{
    QLayout::setGeometry(rect);
    layOut(rect, true);
}

// Zeile fuer Zeile: Elemente aufsammeln, bis eines nicht mehr passt;
// dann die Zeile setzen — was uebrig ist, an die dehnbaren Elemente —
// und weiter. Liefert die Gesamthoehe.
int FlowLayout::layOut(const QRect& rect, bool apply) const
{
    const QMargins m = contentsMargins();
    const QRect area = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
    const int width = qMax(0, area.width());

    struct Placed { QLayoutItem* item; int w; int h; bool stretch; };
    QList<Placed> row;
    int rowsDone = 0;
    int y = area.top();

    auto flush = [&]() {
        if (row.isEmpty()) { return; }
        int used = 0, rowH = 0, stretchers = 0, visible = 0;
        for (const Placed& p : row) {
            used += p.w;
            rowH = qMax(rowH, p.h);
            if (p.stretch) { ++stretchers; }
            if (!p.item->spacerItem()) { ++visible; }
        }
        // Abstaende nur zwischen sichtbaren Elementen; eine Luecke
        // (addStretch) ist keins und kostet keinen.
        used += m_hSpace * qMax(0, visible - 1);
        const int spare = qMax(0, width - used);
        const int each = stretchers > 0 ? spare / stretchers : 0;
        int extra = stretchers > 0 ? spare - each * stretchers : 0;
        if (apply) {
            int x = area.left();
            for (const Placed& p : row) {
                int w = p.w;
                if (p.stretch) {
                    w += each + (extra > 0 ? 1 : 0);
                    if (extra > 0) { --extra; }
                }
                if (p.item->spacerItem()) {
                    x += w;          // die Luecke selbst, ohne Abstand
                } else {
                    p.item->setGeometry(QRect(x, y, w, rowH));
                    x += w + m_hSpace;
                }
            }
        }
        y += rowH + m_vSpace;
        ++rowsDone;
        row.clear();
    };

    int lineWidth = 0;   // Breite der sichtbaren Elemente samt Abstaenden
    int visibleInRow = 0;
    for (QLayoutItem* it : m_items) {
        const bool spacer = it->spacerItem() != nullptr;
        if (spacer) {
            row.append({it, 0, 0, wantsWidth(it)});
            continue;
        }
        const QSize hint = it->sizeHint();
        const int w = qMax(hint.width(), it->minimumSize().width());
        const int h = hint.height();
        const int needed = lineWidth + (visibleInRow > 0 ? m_hSpace : 0) + w;
        if (visibleInRow > 0 && needed > width) {
            flush();
            lineWidth = 0;
            visibleInRow = 0;
        }
        row.append({it, w, h, wantsWidth(it)});
        lineWidth += (visibleInRow > 0 ? m_hSpace : 0) + w;
        ++visibleInRow;
    }
    flush();
    m_rows = rowsDone;
    const int total = y - area.top() - (rowsDone > 0 ? m_vSpace : 0);
    return total + m.top() + m.bottom();
}

} // namespace Longpath
