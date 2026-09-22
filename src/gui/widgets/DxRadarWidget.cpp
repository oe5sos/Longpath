// SPDX-License-Identifier: GPL-3.0-or-later
// src/gui/widgets/DxRadarWidget.cpp  (Longpath) — see DxRadarWidget.h
//
// Longpath-original. No Thetis port.
#include "gui/widgets/DxRadarWidget.h"

#include "gui/StyleConstants.h"
#include "gui/widgets/GlobeWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kEarthKmPerDeg = 111.195;   // mittlerer Erdumfang / 360
// Ringe, die sich ein Funker merken kann.
const double kRingsKm[] = {500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0};
}

DxRadarWidget::DxRadarWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(200, 200);
}

void DxRadarWidget::setHome(double lat, double lon)
{
    m_hasHome = true;
    m_homeLat = lat;
    m_homeLon = lon;
    relayout();
    update();
}

void DxRadarWidget::clearHome()
{
    m_hasHome = false;
    m_placed.clear();
    update();
}

void DxRadarWidget::setPoints(const QVector<MapPoint>& points)
{
    m_points = points;
    m_hover = -1;
    relayout();
    update();
}

void DxRadarWidget::setMaxRangeKm(double km)
{
    m_maxKm = std::clamp(km, 1000.0, 20040.0);
    relayout();
    update();
}

void DxRadarWidget::setBeamHeading(double deg)
{
    m_beamDeg = deg;
    update();
}

double DxRadarWidget::radiusFor(double km, double maxKm)
{
    if (maxKm <= 0.0) { return 0.0; }
    // Wurzel: 500 km liegen bei 16 % des Radius, 5000 km bei 50 %,
    // 20 000 km am Rand — nah genug aufgeloest, fern nicht zerdrueckt.
    return std::sqrt(std::clamp(km / maxKm, 0.0, 1.0));
}

double DxRadarWidget::radiusPx() const
{
    return std::max(10.0, std::min(width(), height()) * 0.5 - 28.0);
}

QPointF DxRadarWidget::centre() const
{
    return QPointF(width() * 0.5, height() * 0.5);
}

QPointF DxRadarWidget::pointAt(double bearingDeg, double km) const
{
    const double r = radiusFor(km, m_maxKm) * radiusPx();
    const double a = bearingDeg * kPi / 180.0;
    return centre() + QPointF(r * std::sin(a), -r * std::cos(a));
}

void DxRadarWidget::relayout()
{
    m_placed.clear();
    if (!m_hasHome) { return; }
    m_placed.reserve(m_points.size());
    for (int i = 0; i < m_points.size(); ++i) {
        const MapPoint& p = m_points.at(i);
        const double bearing = GlobeWidget::initialBearing(m_homeLat, m_homeLon, p.lat, p.lon);
        const double km = GlobeWidget::angularDistance(m_homeLat, m_homeLon, p.lat, p.lon)
                        * kEarthKmPerDeg;
        m_placed.push_back({pointAt(bearing, km), bearing, km, i});
    }
}

int DxRadarWidget::hitTest(const QPointF& pos) const
{
    int best = -1;
    double bestD = 10.0 * 10.0;
    for (const Placed& pl : m_placed) {
        const double dx = pl.px.x() - pos.x();
        const double dy = pl.px.y() - pos.y();
        const double d = dx * dx + dy * dy;
        if (d < bestD) { bestD = d; best = pl.index; }
    }
    return best;
}

void DxRadarWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(Style::hexRole(Style::kPanelBg)));
    // Nach einer Groessenaenderung stimmen die Bildpunkte nicht mehr.
    relayout();

    const QPointF c = centre();
    const double R = radiusPx();
    const QColor grid(Style::kBorderSubtle);
    const QColor text(Style::kTextScale);
    const QColor accent(Style::kAccent);
    const QColor marked(Style::kAmberText);
    const QColor home(Style::kGreenText);

    // Schirm
    QRadialGradient face(c, R);
    face.setColorAt(0.0, QColor(18, 24, 34));
    face.setColorAt(1.0, QColor(10, 12, 16));
    p.setPen(Qt::NoPen);
    p.setBrush(face);
    p.drawEllipse(c, R, R);

    QFont f = p.font();
    f.setPixelSize(9);
    p.setFont(f);
    const QFontMetrics fm(f);

    // Entfernungsringe mit Beschriftung oben rechts am Ring.
    p.setBrush(Qt::NoBrush);
    for (double km : kRingsKm) {
        if (km > m_maxKm * 1.001) { continue; }
        const double r = radiusFor(km, m_maxKm) * R;
        p.setPen(QPen(grid, 1.0, km == m_maxKm ? Qt::SolidLine : Qt::DotLine));
        p.drawEllipse(c, r, r);
        const QString lbl = km >= 1000.0 ? QStringLiteral("%1 000").arg(int(km / 1000))
                                          : QString::number(int(km));
        p.setPen(text);
        p.drawText(QPointF(c.x() + r * 0.7071 + 3.0, c.y() - r * 0.7071 - 2.0), lbl);
    }

    // Peilstriche alle 30°, Himmelsrichtungen benannt.
    for (int deg = 0; deg < 360; deg += 30) {
        const double a = deg * kPi / 180.0;
        const QPointF dir(std::sin(a), -std::cos(a));
        p.setPen(QPen(grid, deg % 90 == 0 ? 1.2 : 0.8));
        p.drawLine(c + dir * (R * 0.06), c + dir * R);
        QString name;
        switch (deg) {
            case 0:   name = QStringLiteral("N"); break;
            case 90:  name = QStringLiteral("E"); break;
            case 180: name = QStringLiteral("S"); break;
            case 270: name = QStringLiteral("W"); break;
            default:  name = QString::number(deg); break;
        }
        const QPointF at = c + dir * (R + 14.0);
        const int tw = fm.horizontalAdvance(name);
        p.setPen(deg % 90 == 0 ? QColor(Style::kTextPrimary) : text);
        p.drawText(QPointF(at.x() - tw / 2.0, at.y() + fm.ascent() / 2.0 - 1.0), name);
    }

    // Richtstrahl der Antenne.
    if (m_beamDeg >= 0.0) {
        const double a = m_beamDeg * kPi / 180.0;
        const QPointF dir(std::sin(a), -std::cos(a));
        p.setPen(QPen(home, 1.5, Qt::DashLine));
        p.drawLine(c, c + dir * R);
    }

    // Kontakte: erst die gewoehnlichen, dann die markierten obendrauf.
    m_painted = 0;
    if (!m_hasHome) {
        p.setPen(text);
        const QString hint = QStringLiteral("no home locator");
        p.drawText(QPointF(c.x() - fm.horizontalAdvance(hint) / 2.0, c.y() + 4.0), hint);
        return;
    }
    for (int pass = 0; pass < 2; ++pass) {
        for (const Placed& pl : m_placed) {
            const MapPoint& mp = m_points.at(pl.index);
            if ((pass == 0) == mp.highlight) { continue; }
            const bool hot = pl.index == m_hover;
            const QColor col = mp.highlight ? marked : accent;
            p.setPen(QPen(QColor(Style::hexRole(Style::kPanelBg)), 1.0));
            p.setBrush(col);
            const double rad = mp.highlight ? 4.5 : (mp.approximate ? 2.2 : 3.0);
            p.drawEllipse(pl.px, rad + (hot ? 1.5 : 0.0), rad + (hot ? 1.5 : 0.0));
            if (mp.highlight || hot) {
                p.setPen(col);
                p.drawText(pl.px + QPointF(7.0, 3.0), mp.label);
            }
            ++m_painted;
        }
    }

    // Zu Hause: der Mittelpunkt.
    p.setPen(QPen(home, 1.5));
    p.setBrush(QColor(Style::hexRole(Style::kPanelBg)));
    p.drawEllipse(c, 4.0, 4.0);
}

void DxRadarWidget::mousePressEvent(QMouseEvent* e)
{
    const int idx = hitTest(e->position());
    if (idx >= 0) {
        const MapPoint& mp = m_points.at(idx);
        emit pointClicked(mp.label, mp.lat, mp.lon);
    }
    QWidget::mousePressEvent(e);
}

void DxRadarWidget::mouseMoveEvent(QMouseEvent* e)
{
    const int idx = hitTest(e->position());
    if (idx != m_hover) {
        m_hover = idx;
        update();
        if (idx >= 0) {
            const Placed* pl = nullptr;
            for (const Placed& q : m_placed) { if (q.index == idx) { pl = &q; break; } }
            if (pl) {
                QToolTip::showText(e->globalPosition().toPoint(),
                                   QStringLiteral("%1 · %2° · %3 km")
                                       .arg(m_points.at(idx).label)
                                       .arg(qRound(pl->bearingDeg))
                                       .arg(qRound(pl->km)),
                                   this);
            }
        } else {
            QToolTip::hideText();
        }
    }
    QWidget::mouseMoveEvent(e);
}

void DxRadarWidget::leaveEvent(QEvent* e)
{
    if (m_hover != -1) { m_hover = -1; update(); }
    QWidget::leaveEvent(e);
}

} // namespace Longpath
