// =================================================================
// src/gui/instruments/InstrumentSpine.cpp  (NereusSDR)
// =================================================================
// Siehe InstrumentSpine.h — die Geometrie zweimal, die Mittel einmal.
//
// Die Zahlen (Stopps der Verläufe, Sektor-Innenradius, Glutweite)
// stammen aus den Entwürfen des Betreibers:
//   ~/Downloads/zeiger-verfeinert.html      — Bogen
//   ~/Downloads/frequenz-im-zeigerstil.html — Gerade
// =================================================================

#include "gui/instruments/InstrumentSpine.h"

#include <QLinearGradient>
#include <QRadialGradient>
#include <QtMath>

namespace NereusSDR {

// QPainterPath::arcTo rechnet in ganzen Grad, 0° auf drei Uhr, positive
// Winkel gegen den Uhrzeigersinn — dieselbe Zählung wie die Entwürfe.
// Die 1/16-Grad-Zählung von QPainter::drawArc kommt hier nicht vor.

// ── ArcSpine ─────────────────────────────────────────────────────────

ArcSpine::ArcSpine(QPointF centre, qreal radius, qreal width,
                   qreal deg0, qreal deg1)
    : m_c(centre), m_r(radius), m_w(width), m_deg0(deg0), m_deg1(deg1)
{
}

qreal ArcSpine::degreeAt(qreal f) const
{
    return m_deg0 + (m_deg1 - m_deg0) * qBound(0.0, f, 1.0);
}

QPointF ArcSpine::pointAt(qreal r, qreal deg) const
{
    const qreal a = qDegreesToRadians(deg);
    // Minus bei y, weil der Bildschirm nach unten zählt und die
    // Entwürfe nach oben.
    return QPointF(m_c.x() + qCos(a) * r, m_c.y() - qSin(a) * r);
}

QPainterPath ArcSpine::arcPath(qreal r, qreal d0, qreal d1) const
{
    QPainterPath p;
    const QRectF box(m_c.x() - r, m_c.y() - r, 2 * r, 2 * r);
    p.arcMoveTo(box, d0);
    p.arcTo(box, d0, d1 - d0);
    return p;
}

QPainterPath ArcSpine::sectorPath(qreal rOut, qreal rIn,
                                  qreal d0, qreal d1) const
{
    QPainterPath p;
    const QRectF outBox(m_c.x() - rOut, m_c.y() - rOut, 2 * rOut, 2 * rOut);
    const QRectF inBox(m_c.x() - rIn, m_c.y() - rIn, 2 * rIn, 2 * rIn);
    p.arcMoveTo(outBox, d0);
    p.arcTo(outBox, d0, d1 - d0);
    p.arcTo(inBox, d1, d0 - d1);
    p.closeSubpath();
    return p;
}

QPainterPath ArcSpine::troughPath() const
{
    return arcPath(m_r, m_deg0, m_deg1);
}

QPainterPath ArcSpine::shadowPath() const
{
    // Die dunkle Kante sitzt an der ÄUSSEREN Begrenzung der Mulde,
    // knapp innerhalb, damit sie nicht über den Rand hinausragt.
    return arcPath(m_r + m_w / 2.0 - 1.1, m_deg0, m_deg1);
}

qreal ArcSpine::shadowWidth() const { return 2.2; }

QPainterPath ArcSpine::troughSpan(qreal a, qreal b) const
{
    return arcPath(m_r, degreeAt(a), degreeAt(b));
}

QPainterPath ArcSpine::fillArea(qreal f) const
{
    // Vom Drehpunkt bis knapp unter die Rille — der Sektor trägt die
    // Fläche des Zifferblatts, nicht die Mulde.
    return sectorPath(m_r - m_w / 2.0 - 2.0, m_sectorInner,
                      m_deg0, degreeAt(f));
}

QBrush ArcSpine::fadeBrush(const QColor& c) const
{
    QRadialGradient g(m_c, m_r - m_w / 2.0 - 2.0);
    QColor c0 = c; c0.setAlphaF(0.0);
    QColor c1 = c; c1.setAlphaF(0.10);
    QColor c2 = c; c2.setAlphaF(0.26);
    g.setColorAt(0.18, c0);
    g.setColorAt(0.72, c1);
    g.setColorAt(1.00, c2);
    return QBrush(g);
}

QRectF ArcSpine::glowBounds() const
{
    const qreal r = m_r * m_glowExtent;
    return QRectF(m_c.x() - r, m_c.y() - r, 2 * r, 2 * r);
}

QPainterPath ArcSpine::glowClip() const
{
    // Nur der Sektor über dem Drehpunkt. Ohne diese Beschneidung
    // liefe die Glut als voller Kreis unter das Instrument.
    return sectorPath(m_r * m_glowExtent, 0.0, m_deg0, m_deg1);
}

QLineF ArcSpine::tick(qreal f, qreal len) const
{
    const qreal d = degreeAt(f);
    const qreal r0 = m_r - m_w / 2.0 - 1.0;
    return QLineF(pointAt(r0, d), pointAt(r0 - len, d));
}

QPointF ArcSpine::labelAnchor(qreal f, qreal off) const
{
    return pointAt(m_r - m_w / 2.0 - off, degreeAt(f));
}

QLineF ArcSpine::crossAt(qreal f, qreal outward, qreal inward) const
{
    const qreal d = degreeAt(f);
    return QLineF(pointAt(m_r + m_w / 2.0 + outward, d),
                  pointAt(m_r - m_w / 2.0 - inward, d));
}

// ── LinearSpine ──────────────────────────────────────────────────────

LinearSpine::LinearSpine(QRectF trough, qreal cornerRadius)
    : m_r(trough), m_rad(cornerRadius)
{
}

qreal LinearSpine::xAt(qreal f) const
{
    return m_r.left() + m_r.width() * qBound(0.0, f, 1.0);
}

QPainterPath LinearSpine::troughPath() const
{
    QPainterPath p;
    p.addRoundedRect(m_r, m_rad, m_rad);
    return p;
}

QPainterPath LinearSpine::shadowPath() const
{
    // Die Kante liegt an der OBEREN Begrenzung — bei der Geraden ist
    // „aussen" oben, so wie beim Bogen „aussen" weg vom Drehpunkt ist.
    QPainterPath p;
    p.addRect(QRectF(m_r.left(), m_r.top(), m_r.width(), shadowWidth()));
    return p.intersected(troughPath());
}

QPainterPath LinearSpine::troughSpan(qreal a, qreal b) const
{
    QPainterPath p;
    const qreal x0 = xAt(qMin(a, b));
    const qreal x1 = xAt(qMax(a, b));
    p.addRect(QRectF(x0, m_r.top(), x1 - x0, m_r.height()));
    return p.intersected(troughPath());
}

QPainterPath LinearSpine::fillArea(qreal f) const
{
    // Bei der Geraden ist die Fläche das Innere der Mulde selbst,
    // beschnitten auf die abgerundeten Ecken.
    QPainterPath p;
    p.addRect(QRectF(m_r.left(), m_r.top(), xAt(f) - m_r.left(),
                     m_r.height()));
    return p.intersected(troughPath());
}

QBrush LinearSpine::fadeBrush(const QColor& c) const
{
    QLinearGradient g(m_r.topLeft(), m_r.topRight());
    QColor c0 = c; c0.setAlphaF(0.05);
    QColor c1 = c; c1.setAlphaF(0.34);
    g.setColorAt(0.0, c0);
    g.setColorAt(1.0, c1);
    return QBrush(g);
}

QRectF LinearSpine::glowBounds() const
{
    // Wie im Entwurf: 78 % der Breite, anderthalb Höhen, um die Mitte.
    const qreal w = m_r.width() * 0.78;
    const qreal h = m_r.height() * 1.5;
    return QRectF(m_r.center().x() - w / 2.0, m_r.center().y() - h / 2.0,
                  w, h);
}

QPainterPath LinearSpine::glowClip() const
{
    return QPainterPath();   // leer heisst: nicht beschneiden
}

QLineF LinearSpine::tick(qreal f, qreal len) const
{
    const qreal x = xAt(f);
    return QLineF(QPointF(x, m_r.bottom()), QPointF(x, m_r.bottom() + len));
}

QPointF LinearSpine::labelAnchor(qreal f, qreal off) const
{
    return QPointF(xAt(f), m_r.bottom() + off);
}

QLineF LinearSpine::crossAt(qreal f, qreal outward, qreal inward) const
{
    const qreal x = xAt(f);
    return QLineF(QPointF(x, m_r.top() - outward),
                  QPointF(x, m_r.bottom() + inward));
}

} // namespace NereusSDR
