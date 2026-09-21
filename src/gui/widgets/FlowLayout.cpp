// =================================================================
// src/gui/widgets/FlowLayout.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Zweck: FlowLayout.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "FlowLayout.h"

#include <QWidget>

namespace Longpath {

FlowLayout::FlowLayout(QWidget* parent, int margin, int hSpacing, int vSpacing)
    : QLayout(parent)
    , m_hSpace(hSpacing)
    , m_vSpace(vSpacing)
{
    setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::~FlowLayout()
{
    while (QLayoutItem* item = takeAt(0)) { delete item; }
}

void FlowLayout::addItem(QLayoutItem* item)
{
    m_items.append(item);
}

void FlowLayout::addLayout(QLayout* layout)
{
    addChildLayout(layout);
    addItem(layout);
}

QLayoutItem* FlowLayout::itemAt(int index) const
{
    return m_items.value(index, nullptr);
}

QLayoutItem* FlowLayout::takeAt(int index)
{
    if (index < 0 || index >= m_items.size()) { return nullptr; }
    return m_items.takeAt(index);
}

int FlowLayout::horizontalSpacing() const
{
    return m_hSpace >= 0 ? m_hSpace : smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
}

int FlowLayout::verticalSpacing() const
{
    return m_vSpace >= 0 ? m_vSpace : smartSpacing(QStyle::PM_LayoutVerticalSpacing);
}

int FlowLayout::heightForWidth(int width) const
{
    return doLayout(QRect(0, 0, width, 0), true);
}

void FlowLayout::setGeometry(const QRect& rect)
{
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

QSize FlowLayout::sizeHint() const
{
    // Alles in EINER Zeile -- so breit wuerde es sich gern machen.
    int w = 0, h = 0;
    for (QLayoutItem* item : m_items) {
        if (item->spacerItem()) { continue; }
        const QSize s = item->sizeHint();
        if (w > 0) { w += horizontalSpacing(); }
        w += s.width();
        h = qMax(h, s.height());
    }
    const QMargins m = contentsMargins();
    return QSize(w + m.left() + m.right(), h + m.top() + m.bottom());
}

QSize FlowLayout::minimumSize() const
{
    // Das breiteste einzelne Element -- mehr braucht eine umbrechende
    // Zeile nicht; die Hoehe liefert heightForWidth().
    QSize size;
    for (QLayoutItem* item : m_items) {
        if (item->spacerItem()) { continue; }
        size = size.expandedTo(item->minimumSize());
    }
    const QMargins m = contentsMargins();
    return size + QSize(m.left() + m.right(), m.top() + m.bottom());
}

int FlowLayout::doLayout(const QRect& rect, bool testOnly) const
{
    // Zeilenweise: erst so viele Elemente in die Zeile, wie mit ihren
    // MINDESTbreiten hineinpassen (wie ein QHBoxLayout, das sich
    // zusammenschiebt), dann die Zeile setzen -- bei Platz mit den
    // Wunschbreiten, sonst zwischen Mindest- und Wunschbreite
    // verteilt. So bleibt bei der alten Fensterbreite alles in einer
    // Zeile, und erst wenn selbst die Mindestbreiten nicht mehr
    // reichen, bricht es um.
    const QMargins m = contentsMargins();
    const QRect effective = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
    const int available = effective.width();
    const int hs = horizontalSpacing();
    const int vs = verticalSpacing();
    int y = effective.y();

    QList<QLayoutItem*> real;
    for (QLayoutItem* item : m_items) {
        if (!item->spacerItem()) { real.append(item); }
    }

    int i = 0;
    while (i < real.size()) {
        // 1. Zeile fuellen (nach Mindestbreiten).
        int j = i;
        int sumMin = 0, sumHint = 0, lineHeight = 0;
        while (j < real.size()) {
            const int minW = real[j]->minimumSize().width();
            const int need = sumMin + (j > i ? hs : 0) + minW;
            if (j > i && need > available) { break; }
            sumMin = need;
            sumHint += (j > i ? hs : 0) + real[j]->sizeHint().width();
            lineHeight = qMax(lineHeight, real[j]->sizeHint().height());
            ++j;
        }
        // 2. Zeile setzen.
        if (!testOnly) {
            int x = effective.x();
            const bool squeeze = sumHint > available && sumHint > sumMin;
            const double share = squeeze
                ? double(available - sumMin) / double(sumHint - sumMin)
                : 1.0;
            for (int k = i; k < j; ++k) {
                const int minW = real[k]->minimumSize().width();
                const int hintW = real[k]->sizeHint().width();
                const int w = squeeze
                    ? minW + int((hintW - minW) * qMax(0.0, share))
                    : hintW;
                real[k]->setGeometry(QRect(x, y, w, real[k]->sizeHint().height()));
                x += w + hs;
            }
        }
        y += lineHeight;
        i = j;
        if (i < real.size()) { y += vs; }
    }
    return y - rect.y() + m.bottom();
}

int FlowLayout::smartSpacing(QStyle::PixelMetric pm) const
{
    QObject* p = parent();
    if (!p) { return -1; }
    if (p->isWidgetType()) {
        auto* pw = static_cast<QWidget*>(p);
        return pw->style()->pixelMetric(pm, nullptr, pw);
    }
    return static_cast<QLayout*>(p)->spacing();
}

} // namespace Longpath
