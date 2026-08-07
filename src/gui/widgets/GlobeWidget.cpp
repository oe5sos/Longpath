// =================================================================
// src/gui/widgets/GlobeWidget.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — see GlobeWidget.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "GlobeWidget.h"
#include "gui/StyleConstants.h"

#include <QDateTime>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QTimer>
#include <QWheelEvent>
#include <QtMath>

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

// Shortest signed difference a→b, in degrees.
double angleDelta(double a, double b) { return norm180(b - a); }

} // namespace

GlobeWidget::GlobeWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    // The open hand is the only affordance saying "this turns". Without
    // it the globe reads as a picture.
    setCursor(Qt::OpenHandCursor);
    setToolTip(QStringLiteral(
        "Drag to turn · wheel to zoom · double-click to reset"));
    useCurrentSubsolarPoint();

    // One timer drives both the look-at animation and the idle spin;
    // it stops itself when there is nothing left to move, so an idle
    // globe costs nothing.
    m_anim = new QTimer(this);
    m_anim->setInterval(33);
    connect(m_anim, &QTimer::timeout, this, [this]() {
        bool moving = false;

        const double d = angleDelta(m_viewLon, m_targetViewLon);
        if (std::abs(d) > 0.4) {
            // Ease in: fast while far, gentle on arrival.
            m_viewLon = norm180(m_viewLon + d * 0.12);
            moving = true;
        } else if (m_autoRotate) {
            m_viewLon = norm180(m_viewLon + 0.15);
            m_targetViewLon = m_viewLon;
            moving = true;
        }

        if (moving) {
            m_frameDirty = true;
            update();
        } else {
            m_anim->stop();
        }
    });
}

// ── Inputs ──────────────────────────────────────────────────────────

bool GlobeWidget::loadTexture(const QString& path)
{
    QImage img(path);
    if (img.isNull()) { return false; }
    // Convert once, on load: sampling a non-native format per pixel
    // would cost more than the whole render.
    m_texture = img.convertToFormat(QImage::Format_RGB32);
    m_frameDirty = true;
    update();
    return true;
}

void GlobeWidget::setHome(double lat, double lon)
{
    m_homeLat = lat;
    m_homeLon = norm180(lon);
    m_hasHome = true;
    update();
}

void GlobeWidget::setTarget(double lat, double lon)
{
    m_targetLat = lat;
    m_targetLon = norm180(lon);
    m_hasTarget = true;
    update();
}

void GlobeWidget::clearTarget()
{
    m_hasTarget = false;
    update();
}

void GlobeWidget::lookAlongBearing(double deg)
{
    if (!m_hasHome) { return; }
    // Put the camera between home and the direction of travel, so both
    // the operator's position and the path stay in view — centring on
    // the target alone pushes home to the rim where it is unreadable.
    const double lat = m_homeLat * kDeg;
    const double b   = deg * kDeg;
    const double d   = 25.0 * kDeg;   // a quarter of the way along
    const double lat2 = std::asin(std::sin(lat) * std::cos(d)
                                  + std::cos(lat) * std::sin(d) * std::cos(b));
    const double lon2 = m_homeLon * kDeg
        + std::atan2(std::sin(b) * std::sin(d) * std::cos(lat),
                     std::cos(d) - std::sin(lat) * std::sin(lat2));

    m_viewLat = std::clamp(lat2 / kDeg, -75.0, 75.0);
    m_targetViewLon = norm180(lon2 / kDeg);
    m_frameDirty = true;
    if (!m_anim->isActive()) { m_anim->start(); }
}

void GlobeWidget::setAutoRotate(bool on)
{
    m_autoRotate = on;
    if (on && !m_anim->isActive()) { m_anim->start(); }
}

void GlobeWidget::setBeamSpread(double deg)
{
    m_beamSpread = std::clamp(deg, 0.0, 60.0);
    update();
}

void GlobeWidget::resetView()
{
    m_zoom = 1.0;
    m_viewLat = 20.0;
    if (m_hasHome) {
        // Home is the useful default centre — it is the one point that
        // is on every path.
        m_viewLat = std::clamp(m_homeLat, -75.0, 75.0);
        m_viewLon = m_homeLon;
    }
    m_targetViewLon = m_viewLon;
    m_frameDirty = true;
    update();
}

// ── Mouse ───────────────────────────────────────────────────────────

void GlobeWidget::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) { QWidget::mousePressEvent(e); return; }

    m_dragging = true;
    m_dragFrom = e->position().toPoint();
    m_dragStartLat = m_viewLat;
    m_dragStartLon = m_viewLon;

    // Stop any running animation, and stop it from snapping back: the
    // hand on the globe outranks whatever the software wanted to show.
    m_anim->stop();
    m_targetViewLon = m_viewLon;
    setCursor(Qt::ClosedHandCursor);
}

void GlobeWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_dragging) { QWidget::mouseMoveEvent(e); return; }

    const QPoint d = e->position().toPoint() - m_dragFrom;

    // Degrees per pixel scaled to the disc, so dragging across the globe
    // turns it by roughly the angle your finger travelled — and so the
    // feel does not change when the widget is resized or zoomed.
    const double perPx = 90.0 / std::max(1.0, radiusPx());

    m_viewLon = norm180(m_dragStartLon - d.x() * perPx);
    // Clamped short of the poles: at exactly 90 the projection loses its
    // sense of which way is east and the globe appears to jump.
    m_viewLat = std::clamp(m_dragStartLat + d.y() * perPx, -85.0, 85.0);
    m_targetViewLon = m_viewLon;

    m_frameDirty = true;
    update();
}

void GlobeWidget::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void GlobeWidget::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        // A way back. Having turned the globe to somewhere unhelpful,
        // hunting for a reset button is worse than a double click.
        resetView();
        return;
    }
    QWidget::mouseDoubleClickEvent(e);
}

void GlobeWidget::wheelEvent(QWheelEvent* e)
{
    const double steps = e->angleDelta().y() / 120.0;
    if (qFuzzyIsNull(steps)) { QWidget::wheelEvent(e); return; }

    const double before = m_zoom;
    m_zoom = std::clamp(m_zoom * std::pow(1.15, steps), 0.6, 6.0);
    if (!qFuzzyCompare(before, m_zoom)) {
        m_frameDirty = true;
        update();
    }
    e->accept();
}

void GlobeWidget::setSubsolarPoint(double lat, double lon)
{
    m_sunLat = lat;
    m_sunLon = norm180(lon);
    m_frameDirty = true;
    update();
}

void GlobeWidget::useCurrentSubsolarPoint()
{
    double lat = 0, lon = 0;
    subsolarPoint(QDateTime::currentDateTimeUtc(), lat, lon);
    setSubsolarPoint(lat, lon);
}

// ── Geometry ────────────────────────────────────────────────────────

void GlobeWidget::subsolarPoint(const QDateTime& utc, double& lat, double& lon)
{
    // Low-precision solar position: good to a fraction of a degree,
    // which is far better than a terminator drawn on a 300 px globe
    // needs. Declination from the day of year, longitude from UTC.
    const int doy = utc.date().dayOfYear();
    const double frac = utc.time().msecsSinceStartOfDay() / 86400000.0;

    // Axial tilt projected onto the year, with perihelion offset.
    const double g = (2.0 * M_PI / 365.24) * (doy + frac - 1.0);
    lat = 23.44 * std::sin((2.0 * M_PI / 365.24) * (doy + frac - 80.0));

    // The sun is overhead at the meridian where it is local noon.
    // Equation of time shifts that by up to ~16 minutes over the year.
    const double eot = 7.66 * std::sin(g - 0.0489)
                     - 9.87 * std::sin(2.0 * (g + 0.3319));   // minutes
    lon = norm180(180.0 - frac * 360.0 - eot * 0.25);
}

void GlobeWidget::interpolateGreatCircle(double latA, double lonA,
                                         double latB, double lonB,
                                         double f, double& lat, double& lon)
{
    const double la1 = latA * kDeg, lo1 = lonA * kDeg;
    const double la2 = latB * kDeg, lo2 = lonB * kDeg;

    const double dLat = la2 - la1;
    const double dLon = lo2 - lo1;
    const double h = std::sin(dLat / 2) * std::sin(dLat / 2)
                   + std::cos(la1) * std::cos(la2)
                         * std::sin(dLon / 2) * std::sin(dLon / 2);
    const double d = 2.0 * std::asin(std::min(1.0, std::sqrt(h)));

    if (d < 1e-9) { lat = latA; lon = lonA; return; }

    // Spherical linear interpolation — a straight line in lat/lon is
    // NOT a great circle, and drawing one would show the signal taking
    // a path it does not take.
    const double a = std::sin((1 - f) * d) / std::sin(d);
    const double b = std::sin(f * d) / std::sin(d);
    const double x = a * std::cos(la1) * std::cos(lo1)
                   + b * std::cos(la2) * std::cos(lo2);
    const double y = a * std::cos(la1) * std::sin(lo1)
                   + b * std::cos(la2) * std::sin(lo2);
    const double z = a * std::sin(la1) + b * std::sin(la2);

    lat = std::atan2(z, std::sqrt(x * x + y * y)) / kDeg;
    lon = std::atan2(y, x) / kDeg;
}

double GlobeWidget::radiusPx() const
{
    return std::min(width(), height()) * 0.44 * m_zoom;
}

bool GlobeWidget::project(double lat, double lon, QPointF& out) const
{
    return projectAlt(lat, lon, 0.0, out);
}

bool GlobeWidget::projectAlt(double lat, double lon, double alt,
                             QPointF& out) const
{
    const double la  = lat * kDeg;
    const double lo  = (lon - m_viewLon) * kDeg;
    const double vla = m_viewLat * kDeg;

    // Unit vector in camera space; z points at the viewer.
    const double x = std::cos(la) * std::sin(lo);
    const double y = std::cos(vla) * std::sin(la)
                   - std::sin(vla) * std::cos(la) * std::cos(lo);
    const double z = std::sin(vla) * std::sin(la)
                   + std::cos(vla) * std::cos(la) * std::cos(lo);

    const double s = 1.0 + alt;
    const double X = x * s;
    const double Y = y * s;
    const double Z = z * s;

    // Hidden only if it is behind the sphere AND inside its silhouette.
    // A raised point past the limb is still in view — that is the whole
    // reason arcs are lifted, so the path does not vanish at the edge.
    if (Z < 0.0 && (X * X + Y * Y) < 1.0) { return false; }

    const double r = radiusPx();
    out = QPointF(width() * 0.5 + r * X, height() * 0.5 - r * Y);
    return true;
}

// ── Geodesics ───────────────────────────────────────────────────────

double GlobeWidget::angularDistance(double latA, double lonA,
                                    double latB, double lonB)
{
    const double la1 = latA * kDeg, la2 = latB * kDeg;
    const double dLat = la2 - la1;
    const double dLon = (lonB - lonA) * kDeg;
    const double h = std::sin(dLat / 2) * std::sin(dLat / 2)
                   + std::cos(la1) * std::cos(la2)
                         * std::sin(dLon / 2) * std::sin(dLon / 2);
    return 2.0 * std::asin(std::min(1.0, std::sqrt(h))) / kDeg;
}

double GlobeWidget::initialBearing(double latA, double lonA,
                                   double latB, double lonB)
{
    const double la1 = latA * kDeg, la2 = latB * kDeg;
    const double dLon = (lonB - lonA) * kDeg;
    const double y = std::sin(dLon) * std::cos(la2);
    const double x = std::cos(la1) * std::sin(la2)
                   - std::sin(la1) * std::cos(la2) * std::cos(dLon);
    double b = std::atan2(y, x) / kDeg;
    if (b < 0.0) { b += 360.0; }
    return b;
}

void GlobeWidget::destinationPoint(double lat, double lon, double bearingDeg,
                                   double angularDistDeg,
                                   double& outLat, double& outLon)
{
    const double la = lat * kDeg;
    const double b  = bearingDeg * kDeg;
    const double d  = angularDistDeg * kDeg;

    const double la2 = std::asin(std::sin(la) * std::cos(d)
                                 + std::cos(la) * std::sin(d) * std::cos(b));
    const double lo2 = lon * kDeg
        + std::atan2(std::sin(b) * std::sin(d) * std::cos(la),
                     std::cos(d) - std::sin(la) * std::sin(la2));

    outLat = la2 / kDeg;
    outLon = norm180(lo2 / kDeg);
}

// ── Rendering ───────────────────────────────────────────────────────

void GlobeWidget::resizeEvent(QResizeEvent*)
{
    m_frameDirty = true;
}

void GlobeWidget::renderSphere()
{
    const int w = width();
    const int h = height();
    if (w <= 0 || h <= 0) { return; }

    m_frame = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    m_frame.fill(Qt::transparent);

    const double r  = radiusPx();
    const double cx = w * 0.5;
    const double cy = h * 0.5;
    const double vla = m_viewLat * kDeg;

    // Sun direction in the camera's frame, for the terminator.
    const double sla = m_sunLat * kDeg;
    const double slo = (m_sunLon - m_viewLon) * kDeg;
    const double sunX = std::cos(sla) * std::sin(slo);
    const double sunY = std::cos(vla) * std::sin(sla)
                      - std::sin(vla) * std::cos(sla) * std::cos(slo);
    const double sunZ = std::sin(vla) * std::sin(sla)
                      + std::cos(vla) * std::cos(sla) * std::cos(slo);

    const bool tex = !m_texture.isNull();
    const int tw = tex ? m_texture.width()  : 0;
    const int th = tex ? m_texture.height() : 0;

    const int x0 = std::max(0, static_cast<int>(cx - r) - 1);
    const int x1 = std::min(w - 1, static_cast<int>(cx + r) + 1);
    const int y0 = std::max(0, static_cast<int>(cy - r) - 1);
    const int y1 = std::min(h - 1, static_cast<int>(cy + r) + 1);

    for (int py = y0; py <= y1; ++py) {
        auto* line = reinterpret_cast<QRgb*>(m_frame.scanLine(py));
        const double dy = (cy - py) / r;
        for (int px = x0; px <= x1; ++px) {
            const double dx = (px - cx) / r;
            const double rr = dx * dx + dy * dy;
            if (rr > 1.0) { continue; }

            // Surface normal in camera space; z towards the viewer.
            const double nz = std::sqrt(1.0 - rr);

            // Un-rotate into geographic coordinates.
            const double lat = std::asin(dy * std::cos(vla) + nz * std::sin(vla));
            const double lon = m_viewLon * kDeg
                + std::atan2(dx, nz * std::cos(vla) - dy * std::sin(vla));

            QRgb base;
            if (tex) {
                int tx = static_cast<int>((norm180(lon / kDeg) + 180.0)
                                          / 360.0 * tw) % tw;
                if (tx < 0) { tx += tw; }
                int ty = static_cast<int>((90.0 - lat / kDeg) / 180.0 * th);
                ty = std::clamp(ty, 0, th - 1);
                base = m_texture.pixel(tx, ty);
            } else {
                // No texture: a plain blue marble so the shape and the
                // terminator still read.
                base = qRgb(0x1a, 0x3a, 0x58);
            }

            // Lambert term against the sun, with a floor so the night
            // side stays legible rather than going black — this is an
            // instrument, not a render.
            double lit = dx * sunX + dy * sunY + nz * sunZ;
            lit = 0.18 + 0.82 * std::clamp(lit, 0.0, 1.0);

            // Limb darkening towards the edge sells the curvature.
            lit *= 0.55 + 0.45 * nz;

            line[px] = qRgb(
                static_cast<int>(qRed(base)   * lit),
                static_cast<int>(qGreen(base) * lit),
                static_cast<int>(qBlue(base)  * lit));
        }
    }
    m_frameDirty = false;
}

void GlobeWidget::drawArc(QPainter& p, double endLat, double endLon,
                          const QColor& col, double width,
                          double opacity) const
{
    const double dist = angularDistance(m_homeLat, m_homeLon, endLat, endLon);
    if (dist < 0.05) { return; }

    // Lift the arc off the surface, highest in the middle. A great
    // circle lying flat projects to a straight line whenever the camera
    // sits in its plane — which is precisely where lookAlongBearing puts
    // it, so the one view built to show the path was the view that made
    // it look like a ruled line. Raised, it reads as the curve it is.
    //
    // Height scales with distance: a 300 km hop that ballooned as high
    // as a transatlantic path would misrepresent both.
    const double peak = 0.04 + 0.26 * std::min(1.0, dist / 180.0);

    constexpr int kSteps = 160;
    QPointF prev;
    bool havePrev = false;

    for (int i = 0; i <= kSteps; ++i) {
        const double f = static_cast<double>(i) / kSteps;
        double lat = 0, lon = 0;
        interpolateGreatCircle(m_homeLat, m_homeLon, endLat, endLon,
                               f, lat, lon);
        const double alt = peak * std::sin(M_PI * f);

        QPointF pt;
        if (!projectAlt(lat, lon, alt, pt)) {
            // Behind the globe. Pick the arc up again when it comes
            // round rather than drawing a chord across the disc.
            havePrev = false;
            continue;
        }
        if (havePrev) {
            p.setOpacity(opacity * 0.30);
            p.setPen(QPen(col, width * 2.2, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(prev, pt);
            p.setOpacity(opacity);
            p.setPen(QPen(col, width, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(prev, pt);
        }
        prev = pt;
        havePrev = true;
    }
    p.setOpacity(1.0);
}

void GlobeWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(Style::kPanelBg));

    if (m_frameDirty || m_frame.size() != size()) { renderSphere(); }

    const double r  = radiusPx();
    const QPointF c(width() * 0.5, height() * 0.5);

    // Atmosphere: a soft rim just outside the disc.
    QRadialGradient halo(c, r * 1.16);
    halo.setColorAt(0.86, QColor(0, 0, 0, 0));
    halo.setColorAt(0.94, QColor(80, 160, 230, 46));
    halo.setColorAt(1.00, QColor(80, 160, 230, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(halo);
    p.drawEllipse(c, r * 1.16, r * 1.16);
    p.setBrush(Qt::NoBrush);

    p.drawImage(0, 0, m_frame);

    // Graticule every 30°, clipped to the visible hemisphere by the
    // projection itself.
    p.setPen(QPen(QColor(255, 255, 255, m_texture.isNull() ? 44 : 28), 0.8));
    for (int lonLine = -180; lonLine < 180; lonLine += 30) {
        QPointF prev;
        bool havePrev = false;
        for (int lat = -90; lat <= 90; lat += 3) {
            QPointF pt;
            if (project(lat, lonLine, pt)) {
                if (havePrev) { p.drawLine(prev, pt); }
                prev = pt;
                havePrev = true;
            } else {
                havePrev = false;
            }
        }
    }
    for (int latLine = -60; latLine <= 60; latLine += 30) {
        QPointF prev;
        bool havePrev = false;
        for (int lon = -180; lon <= 180; lon += 3) {
            QPointF pt;
            if (project(latLine, lon, pt)) {
                if (havePrev) { p.drawLine(prev, pt); }
                prev = pt;
                havePrev = true;
            } else {
                havePrev = false;
            }
        }
    }

    // The path, and the antenna's main lobe either side of it.
    //
    // Three arcs rather than one: the centre is where the beam is
    // pointed, the flanking pair are the half-power edges. Seeing them
    // spread apart over 8000 km is what makes it obvious that a rotator
    // five degrees off still covers the station — or does not.
    if (m_hasHome && m_hasTarget) {
        const double dist = angularDistance(m_homeLat, m_homeLon,
                                            m_targetLat, m_targetLon);
        const double bear = initialBearing(m_homeLat, m_homeLon,
                                           m_targetLat, m_targetLon);
        const QColor accent(Style::kAccent);

        if (m_beamSpread > 0.0 && dist > 1.0) {
            for (double off : {-m_beamSpread, m_beamSpread}) {
                double lat = 0, lon = 0;
                destinationPoint(m_homeLat, m_homeLon, bear + off, dist,
                                 lat, lon);
                drawArc(p, lat, lon, accent, 1.0, 0.34);
            }
        }
        drawArc(p, m_targetLat, m_targetLon, accent, 1.6, 1.0);
    }

    auto marker = [&](double lat, double lon, const QColor& col,
                      const QString& label) {
        QPointF pt;
        if (!project(lat, lon, pt)) { return; }
        p.setPen(Qt::NoPen);
        QColor glow = col;
        glow.setAlpha(80);
        p.setBrush(glow);
        p.drawEllipse(pt, 7, 7);
        p.setBrush(col);
        p.drawEllipse(pt, 3.2, 3.2);
        p.setBrush(Qt::NoBrush);
        if (!label.isEmpty()) {
            QFont f = p.font();
            f.setPixelSize(9);
            f.setBold(true);
            p.setFont(f);
            p.setPen(col);
            p.drawText(pt + QPointF(9, 3), label);
        }
    };

    if (m_hasHome) {
        marker(m_homeLat, m_homeLon, QColor(Style::kGreenText),
               QStringLiteral("HOME"));
    }
    if (m_hasTarget) {
        marker(m_targetLat, m_targetLon, QColor(Style::kAccent), QString{});
    }

    if (m_texture.isNull()) {
        // Say why it looks plain, and what fixes it.
        QFont f = p.font();
        f.setPixelSize(10);
        p.setFont(f);
        p.setPen(QColor(Style::kTextScale));
        const QString hint = QStringLiteral("no world image loaded");
        p.drawText(QPointF((width() - QFontMetrics(f).horizontalAdvance(hint)) / 2.0,
                           height() - 6.0), hint);
    }
}

} // namespace NereusSDR
