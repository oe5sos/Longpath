// =================================================================
// src/gui/widgets/FlatMapWidget.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — see FlatMapWidget.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "FlatMapWidget.h"
#include "WorldTexture.h"

#include "core/SolarTimes.h"
#include "gui/StyleConstants.h"

#include <QDateTime>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

constexpr double kDeg = M_PI / 180.0;

double norm180(double deg)
{
    double d = std::fmod(deg + 180.0, 360.0);
    if (d < 0.0) { d += 360.0; }
    return d - 180.0;
}

} // namespace

FlatMapWidget::FlatMapWidget(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setCursor(Qt::OpenHandCursor);
    setToolTip(QStringLiteral(
        "Drag to pan · wheel to zoom · double-click to reset"));
}

// ── Inputs ──────────────────────────────────────────────────────────

void FlatMapWidget::setHome(double lat, double lon)
{
    m_homeLat = lat;
    m_homeLon = norm180(lon);
    m_hasHome = true;
    update();
}

void FlatMapWidget::clearHome()
{
    m_hasHome = false;
    update();
}

void FlatMapWidget::setPoints(const QVector<MapPoint>& points)
{
    m_points = points;
    update();
}

void FlatMapWidget::setShowPaths(bool on)
{
    m_showPaths = on;
    update();
}

void FlatMapWidget::setShowTerminator(bool on)
{
    m_showTerminator = on;
    m_nightDirty = true;
    update();
}

void FlatMapWidget::refreshTexture()
{
    WorldTexture::reload();
    update();
}

// ── Geometry ────────────────────────────────────────────────────────

QVector<QPointF> FlatMapWidget::greatCircleSamples(double lat1, double lon1,
                                                   double lat2, double lon2,
                                                   int steps)
{
    QVector<QPointF> out;
    if (steps < 1) { return out; }
    out.reserve(steps + 1);

    const double la1 = lat1 * kDeg, lo1 = lon1 * kDeg;
    const double la2 = lat2 * kDeg, lo2 = lon2 * kDeg;

    const double dLat = la2 - la1;
    const double dLon = lo2 - lo1;
    const double h = std::sin(dLat / 2) * std::sin(dLat / 2)
                   + std::cos(la1) * std::cos(la2)
                         * std::sin(dLon / 2) * std::sin(dLon / 2);
    const double d = 2.0 * std::asin(std::min(1.0, std::sqrt(h)));

    if (d < 1e-9) {
        out.append(QPointF(norm180(lon1), lat1));
        return out;
    }

    for (int i = 0; i <= steps; ++i) {
        const double f = static_cast<double>(i) / steps;
        const double a = std::sin((1 - f) * d) / std::sin(d);
        const double b = std::sin(f * d) / std::sin(d);
        const double x = a * std::cos(la1) * std::cos(lo1)
                       + b * std::cos(la2) * std::cos(lo2);
        const double y = a * std::cos(la1) * std::sin(lo1)
                       + b * std::cos(la2) * std::sin(lo2);
        const double z = a * std::sin(la1) + b * std::sin(la2);

        const double lat = std::atan2(z, std::sqrt(x * x + y * y)) / kDeg;
        const double lon = std::atan2(y, x) / kDeg;
        out.append(QPointF(norm180(lon), lat));
    }
    return out;
}

QVector<QVector<QPointF>>
FlatMapWidget::splitAtAntimeridian(const QVector<QPointF>& lonLat)
{
    QVector<QVector<QPointF>> runs;
    if (lonLat.isEmpty()) { return runs; }

    QVector<QPointF> current;
    current.append(lonLat.first());

    for (int i = 1; i < lonLat.size(); ++i) {
        const double prevLon = lonLat.at(i - 1).x();
        const double lon     = lonLat.at(i).x();

        // A jump of more than half the world between two consecutive
        // samples cannot be real movement — the path crossed the date
        // line and the longitude wrapped. Drawn straight, it would
        // streak all the way back across the map, which is the single
        // most obvious way a world map looks broken.
        if (std::abs(lon - prevLon) > 180.0) {
            runs.append(current);
            current.clear();
        }
        current.append(lonLat.at(i));
    }
    if (!current.isEmpty()) { runs.append(current); }
    return runs;
}

// ── Layout ──────────────────────────────────────────────────────────

QRectF FlatMapWidget::mapRect() const
{
    // 2:1, centred, as large as fits — then zoom and pan on top.
    const double w = width();
    const double h = height();
    double mw = w;
    double mh = mw / 2.0;
    if (mh > h) { mh = h; mw = mh * 2.0; }

    mw *= m_zoom;
    mh *= m_zoom;
    return QRectF((w - mw) / 2.0 + m_pan.x(),
                  (h - mh) / 2.0 + m_pan.y(), mw, mh);
}

QPointF FlatMapWidget::project(double lat, double lon) const
{
    const QRectF r = mapRect();
    return QPointF(r.left() + (norm180(lon) + 180.0) / 360.0 * r.width(),
                   r.top()  + (90.0 - lat) / 180.0 * r.height());
}

void FlatMapWidget::resizeEvent(QResizeEvent*)
{
    m_nightDirty = true;
}

void FlatMapWidget::buildNightOverlay()
{
    // Coarse on purpose: sun elevation varies smoothly, so a small image
    // scaled up is indistinguishable from a per-pixel one and costs a
    // thousandth as much. At full size this would be 400k trigonometric
    // evaluations every repaint.
    constexpr int kW = 180;
    constexpr int kH = 90;

    m_night = QImage(kW, kH, QImage::Format_ARGB32_Premultiplied);
    m_night.fill(Qt::transparent);
    m_nightDirty = false;
    if (!m_showTerminator) { return; }

    const QDateTime now = QDateTime::currentDateTimeUtc();

    for (int y = 0; y < kH; ++y) {
        const double lat = 90.0 - (y + 0.5) / kH * 180.0;
        auto* line = reinterpret_cast<QRgb*>(m_night.scanLine(y));
        for (int x = 0; x < kW; ++x) {
            const double lon = -180.0 + (x + 0.5) / kW * 360.0;
            const double elev = solarInfo(now, lat, lon).elevationDeg;

            // Full dark well below the horizon, clear above it, and a
            // soft band across civil twilight — which is the grey line,
            // and the part worth being able to see.
            double dark = 0.0;
            if (elev < -6.0)      { dark = 1.0; }
            else if (elev < 6.0)  { dark = (6.0 - elev) / 12.0; }

            const int a = static_cast<int>(dark * 150.0);
            line[x] = qPremultiply(qRgba(0, 4, 16, a));
        }
    }
}

// ── Painting ────────────────────────────────────────────────────────

void FlatMapWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.fillRect(rect(), QColor(Style::kAppBg));

    const QRectF r = mapRect();

    const QImage tex = WorldTexture::image();
    if (!tex.isNull()) {
        p.drawImage(r, tex);
    } else {
        p.fillRect(r, QColor(0x1a, 0x3a, 0x58));
    }

    if (m_showTerminator) {
        if (m_nightDirty || m_night.isNull()) { buildNightOverlay(); }
        p.drawImage(r, m_night);
    }

    // Graticule every 30 degrees, plus the equator picked out.
    p.setPen(QPen(QColor(255, 255, 255, tex.isNull() ? 48 : 30), 0.8));
    for (int lon = -180; lon <= 180; lon += 30) {
        const QPointF a = project(90, lon);
        const QPointF b = project(-90, lon);
        p.drawLine(a, b);
    }
    for (int lat = -60; lat <= 60; lat += 30) {
        p.drawLine(project(lat, -180), project(lat, 180));
    }
    p.setPen(QPen(QColor(255, 255, 255, 55), 1.0));
    p.drawLine(project(0, -180), project(0, 180));

    p.setPen(QPen(QColor(Style::kBorderSubtle), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRect(r);

    // Paths first, so the markers sit on top of them.
    if (m_showPaths && m_hasHome) {
        const QColor accent(Style::kAccent);
        for (const MapPoint& pt : m_points) {
            const QVector<QPointF> samples =
                greatCircleSamples(m_homeLat, m_homeLon, pt.lat, pt.lon, 64);
            for (const QVector<QPointF>& run : splitAtAntimeridian(samples)) {
                if (run.size() < 2) { continue; }
                QPolygonF poly;
                poly.reserve(run.size());
                for (const QPointF& s : run) {
                    poly << project(s.y(), s.x());
                }
                p.setOpacity(0.35);
                p.setPen(QPen(accent, 1.0));
                p.drawPolyline(poly);
            }
        }
        p.setOpacity(1.0);
    }

    // Contacts.
    const QColor dot(Style::kAccent);
    const bool labels = m_points.size() <= 40 && m_zoom >= 1.0;
    QFont f = p.font();
    f.setPixelSize(9);
    p.setFont(f);

    for (const MapPoint& pt : m_points) {
        const QPointF s = project(pt.lat, pt.lon);
        if (!r.contains(s)) { continue; }
        p.setPen(Qt::NoPen);
        QColor glow = dot;
        glow.setAlpha(90);
        p.setBrush(glow);
        p.drawEllipse(s, 5.5, 5.5);
        p.setBrush(dot);
        p.drawEllipse(s, 2.4, 2.4);
        if (labels && !pt.label.isEmpty()) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QColor(Style::kTextPrimary));
            p.drawText(s + QPointF(7, 3), pt.label);
        }
    }

    if (m_hasHome) {
        const QPointF s = project(m_homeLat, m_homeLon);
        const QColor home(Style::kGreenText);
        p.setPen(Qt::NoPen);
        QColor glow = home;
        glow.setAlpha(90);
        p.setBrush(glow);
        p.drawEllipse(s, 8, 8);
        p.setBrush(home);
        p.drawEllipse(s, 3.4, 3.4);
        p.setBrush(Qt::NoBrush);
        p.setPen(home);
        QFont bf = f;
        bf.setBold(true);
        p.setFont(bf);
        p.drawText(s + QPointF(9, 3), QStringLiteral("HOME"));
    }

    if (tex.isNull()) {
        p.setPen(QColor(Style::kTextScale));
        p.setFont(f);
        const QString hint =
            QStringLiteral("no world image loaded — Rotor / Log › Globe › "
                           "World image…");
        p.drawText(QPointF((width() - QFontMetrics(f).horizontalAdvance(hint)) / 2.0,
                           height() - 6.0), hint);
    }
}

// ── Mouse ───────────────────────────────────────────────────────────

void FlatMapWidget::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) { QWidget::mousePressEvent(e); return; }
    m_dragging = true;
    m_dragFrom = e->position().toPoint();
    m_panFrom = m_pan;
    setCursor(Qt::ClosedHandCursor);
}

void FlatMapWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_dragging) { QWidget::mouseMoveEvent(e); return; }
    const QPoint d = e->position().toPoint() - m_dragFrom;
    m_pan = m_panFrom + QPointF(d.x(), d.y());
    update();
}

void FlatMapWidget::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void FlatMapWidget::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        m_zoom = 1.0;
        m_pan = QPointF(0.0, 0.0);
        update();
        return;
    }
    QWidget::mouseDoubleClickEvent(e);
}

void FlatMapWidget::wheelEvent(QWheelEvent* e)
{
    const double steps = e->angleDelta().y() / 120.0;
    if (qFuzzyIsNull(steps)) { QWidget::wheelEvent(e); return; }

    const double before = m_zoom;
    m_zoom = std::clamp(m_zoom * std::pow(1.15, steps), 1.0, 12.0);
    if (!qFuzzyCompare(before, m_zoom)) {
        // Keep whatever is under the pointer under the pointer, so
        // zooming into a region does not throw it off screen.
        const QPointF c(width() / 2.0, height() / 2.0);
        const QPointF mouse = e->position();
        const double k = m_zoom / before;
        m_pan = (m_pan + c - mouse) * k + mouse - c;
        update();
    }
    e->accept();
}

} // namespace NereusSDR
