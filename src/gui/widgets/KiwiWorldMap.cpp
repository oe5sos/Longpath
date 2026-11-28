// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen. Siehe KiwiWorldMap.h.
//
//   2026-11-25 — Neu angelegt.

#include "gui/widgets/KiwiWorldMap.h"

#include "gui/StyleConstants.h"
#include "gui/widgets/KiwiWorldBorders.h"
#include "gui/widgets/KiwiWorldCoast.h"

#include <QFont>
#include <QKeyEvent>
#include <QToolButton>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kWorldLat0 = -58.0;
constexpr double kWorldLat1 = 84.0;
constexpr double kHitRadius = 9.0;

QColor col(const char* hex) { return QColor(QLatin1String(hex)); }

QColor pointColor(const KiwiMapPoint& p, bool heardMode)
{
    if (heardMode && p.heard != KiwiMapPoint::Heard::None) {
        switch (p.heard) {
        case KiwiMapPoint::Heard::Hears:   return col(Style::kGreenText);
        case KiwiMapPoint::Heard::Weak:    return col(Style::kAmberWarn);
        case KiwiMapPoint::Heard::Nothing: return col(Style::kTextGroupHead);
        default:                           return col(Style::kTextSecondary);
        }
    }
    switch (p.state) {
    case KiwiMapPoint::State::Free:    return col(Style::kGreenText);
    case KiwiMapPoint::State::Partial: return col(Style::kGreenText);   // freier Kanal da, nur nicht alle
    case KiwiMapPoint::State::Full:    return col(Style::kAmberWarn);
    case KiwiMapPoint::State::Offline: return col(Style::kTextGroupHead);
    }
    return col(Style::kTextSecondary);
}

} // namespace

KiwiWorldMap::KiwiWorldMap(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setObjectName(QStringLiteral("kiwiWorldMap"));
    setAccessibleName(tr("World map of KiwiSDR receivers"));
    setAccessibleDescription(
        tr("Shows where public KiwiSDR receivers are and whether they are "
           "free, partly busy, full or not reachable. Hover a dot for "
           "details, click it to select the receiver."));
    setFocusPolicy(Qt::ClickFocus);

    // Plus und Minus rechts oben; blau umrandet = anfassbar.
    const QString style = QStringLiteral(
        "QToolButton { color: %1; background: %2; border: 1px solid %3; "
        "border-radius: 6px; font-size: 16px; }"
        "QToolButton:hover { border-color: %4; }"
        "QToolButton:disabled { color: %5; border-color: %6; }")
            .arg(QString::fromLatin1(Style::kTextPrimary),
                 QString::fromLatin1(Style::kInsetBg),
                 QString::fromLatin1(Style::kBlueBorder),
                 QString::fromLatin1(Style::kAccent),
                 QString::fromLatin1(Style::kTextInactive),
                 QString::fromLatin1(Style::kBorder));
    auto make = [this, &style](const QString& text, const char* name,
                               const QString& tip, int direction) {
        auto* b = new QToolButton(this);
        b->setText(text);
        b->setObjectName(QLatin1String(name));
        b->setToolTip(tip);
        b->setAccessibleName(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setFixedSize(28, 24);
        b->setStyleSheet(style);
        connect(b, &QToolButton::clicked, this,
                [this, direction]() { zoomStep(direction); });
        return b;
    };
    m_zoomOutButton = make(QStringLiteral("\u2212"), "kiwiMapZoomOut", tr("Zoom out"), -1);
    m_zoomInButton = make(QStringLiteral("+"), "kiwiMapZoomIn", tr("Zoom in"), +1);
    updateZoomButtons();
}

void KiwiWorldMap::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_zoomInButton->move(width() - 8 - m_zoomInButton->width(), 8);
    m_zoomOutButton->move(m_zoomInButton->x() - 4 - m_zoomOutButton->width(), 8);
}

void KiwiWorldMap::updateZoomButtons()
{
    m_zoomOutButton->setEnabled(m_zoom > 1.0);
    m_zoomInButton->setEnabled(m_zoom < kMaxZoom);
}

void KiwiWorldMap::zoomStep(int direction)
{
    // Um den gewaehlten Kiwi, beim ersten Mal sonst um den eigenen Standort,
    // danach um die Mitte des Ausschnitts.
    double la0, la1, lo0, lo1;
    computeView(la0, la1, lo0, lo1);
    double lat = (la0 + la1) / 2;
    double lon = (lo0 + lo1) / 2;
    if (direction > 0) {
        bool picked = false;
        for (const KiwiMapPoint& p : m_points) {
            if (p.id == m_selected) { lat = p.lat; lon = p.lon; picked = true; break; }
        }
        if (!picked && m_zoom <= 1.0 && m_homeValid) { lat = m_homeLat; lon = m_homeLon; }
    }
    m_zoom = std::clamp(direction > 0 ? m_zoom * kZoomStep : m_zoom / kZoomStep,
                        1.0, kMaxZoom);
    if (m_zoom < 1.01) { m_zoom = 1.0; }
    m_centerLat = lat;
    m_centerLon = lon;
    updateZoomButtons();
    update();
}

void KiwiWorldMap::zoomAt(double factor, const QPointF& anchor)
{
    double la0, la1, lo0, lo1;
    computeView(la0, la1, lo0, lo1);
    if (width() <= 0 || height() <= 0) { return; }
    const double fx = anchor.x() / width();
    const double fy = anchor.y() / height();
    const double lon = lo0 + fx * (lo1 - lo0);
    const double lat = la1 - fy * (la1 - la0);
    double z = std::clamp(m_zoom * factor, 1.0, kMaxZoom);
    if (z < 1.01) { z = 1.0; }
    const double w = (lo1 - lo0) * m_zoom / z;
    const double h = (la1 - la0) * m_zoom / z;
    m_zoom = z;
    m_centerLon = lon + (0.5 - fx) * w;
    m_centerLat = lat - (0.5 - fy) * h;
    updateZoomButtons();
    update();
}

void KiwiWorldMap::wheelEvent(QWheelEvent* event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0) { event->ignore(); return; }
    zoomAt(std::pow(kZoomStep, delta / 240.0), event->position());
    event->accept();
}

void KiwiWorldMap::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Plus) { zoomStep(+1); return; }
    if (event->key() == Qt::Key_Minus) { zoomStep(-1); return; }
    QWidget::keyPressEvent(event);
}

void KiwiWorldMap::setPoints(const QVector<KiwiMapPoint>& points)
{
    m_points = points;
    update();
}

void KiwiWorldMap::setHome(double lat, double lon, bool valid, const QString& label)
{
    m_homeValid = valid;
    m_homeLat = lat;
    m_homeLon = lon;
    m_homeLabel = label;
    update();
}

void KiwiWorldMap::setSelectedId(const QString& id)
{
    if (m_selected == id) { return; }
    m_selected = id;
    update();
}

void KiwiWorldMap::setHeardMode(bool on)
{
    m_heardMode = on;
    update();
}

void KiwiWorldMap::setAutoFit(bool on)
{
    m_autoFit = on;
    update();
}

void KiwiWorldMap::computeView(double& lat0, double& lat1,
                               double& lon0, double& lon1) const
{
    baseView(lat0, lat1, lon0, lon1);
    if (m_zoom <= 1.0) { return; }
    const double w = (lon1 - lon0) / m_zoom;
    const double h = (lat1 - lat0) / m_zoom;
    lat0 = m_centerLat - h / 2; lat1 = m_centerLat + h / 2;
    lon0 = m_centerLon - w / 2; lon1 = m_centerLon + w / 2;
}

void KiwiWorldMap::baseView(double& lat0, double& lat1,
                            double& lon0, double& lon1) const
{
    lat0 = kWorldLat0;
    lat1 = kWorldLat1;
    lon0 = -180.0;
    lon1 = 180.0;

    if (m_autoFit) {
        double la0 = 1e9, la1 = -1e9, lo0 = 1e9, lo1 = -1e9;
        const auto add = [&](double la, double lo) {
            la0 = std::min(la0, la); la1 = std::max(la1, la);
            lo0 = std::min(lo0, lo); lo1 = std::max(lo1, lo);
        };
        if (m_homeValid) { add(m_homeLat, m_homeLon); }
        for (const KiwiMapPoint& p : m_points) {
            if (p.open) { add(p.lat, p.lon); }
        }
        if (la0 <= la1) {
            // Rand 15 %, mindestens 40 x 20 Grad.
            double h = std::max(20.0, (la1 - la0) * 1.3);
            double w = std::max(40.0, (lo1 - lo0) * 1.3);
            const double cLat = (la0 + la1) / 2;
            const double cLon = (lo0 + lo1) / 2;
            // Seitenverhaeltnis des Widgets: Grad je Pixel gleich.
            const double aspect = height() > 0 ? double(width()) / height() : 2.0;
            if (w / h < aspect) { w = h * aspect; } else { h = w / aspect; }
            if (w > 360.0) { w = 360.0; h = w / aspect; }
            lat0 = cLat - h / 2; lat1 = cLat + h / 2;
            lon0 = cLon - w / 2; lon1 = cLon + w / 2;
            return;
        }
    }
    // Ganze Welt, die Mitte bleibt bei 0 Grad; das Seitenverhaeltnis wird
    // nicht verzerrt, sondern der Rest bleibt leer (oben/unten).
    const double aspect = height() > 0 ? double(width()) / height() : 2.0;
    const double h = (lon1 - lon0) / aspect;
    const double c = (kWorldLat0 + kWorldLat1) / 2;
    lat0 = c - h / 2; lat1 = c + h / 2;
}

void KiwiWorldMap::viewBounds(double& lat0, double& lat1,
                              double& lon0, double& lon1) const
{
    computeView(lat0, lat1, lon0, lon1);
}

QPointF KiwiWorldMap::toWidget(double lat, double lon) const
{
    double la0, la1, lo0, lo1;
    computeView(la0, la1, lo0, lo1);
    return QPointF((lon - lo0) / (lo1 - lo0) * width(),
                   (la1 - lat) / (la1 - la0) * height());
}

int KiwiWorldMap::pointAt(const QPointF& pos) const
{
    int best = -1;
    double bestD = kHitRadius * kHitRadius;
    for (int i = 0; i < m_points.size(); ++i) {
        const QPointF p = toWidget(m_points[i].lat, m_points[i].lon);
        const double d = (p.x() - pos.x()) * (p.x() - pos.x())
                       + (p.y() - pos.y()) * (p.y() - pos.y());
        if (d <= bestD) { bestD = d; best = i; }
    }
    return best;
}

QString KiwiWorldMap::idAt(const QPointF& pos) const
{
    const int i = pointAt(pos);
    return i < 0 ? QString() : m_points[i].id;
}

void KiwiWorldMap::mouseMoveEvent(QMouseEvent* event)
{
    if (m_pressed && (event->buttons() & Qt::LeftButton) && m_zoom > 1.0) {
        if (!m_dragged
            && (event->position() - m_pressPos).manhattanLength() > 4.0) {
            m_dragged = true;
            QToolTip::hideText();
        }
        if (m_dragged && width() > 0 && height() > 0) {
            double la0, la1, lo0, lo1;
            computeView(la0, la1, lo0, lo1);
            const QPointF d = event->position() - m_dragLast;
            m_centerLon = std::clamp(m_centerLon - d.x() / width() * (lo1 - lo0),
                                     -180.0, 180.0);
            m_centerLat = std::clamp(m_centerLat + d.y() / height() * (la1 - la0),
                                     -90.0, 90.0);
            m_dragLast = event->position();
            setCursor(Qt::ClosedHandCursor);
            update();
            return;
        }
    }
    const int i = pointAt(event->position());
    const QString id = i < 0 ? QString() : m_points[i].id;
    setCursor(i < 0 ? Qt::ArrowCursor : Qt::PointingHandCursor);
    if (id != m_hovered) {
        m_hovered = id;
        emit pointHovered(id);
        update();
    }
    if (i >= 0 && !m_points[i].detail.isEmpty()) {
        QToolTip::showText(event->globalPosition().toPoint(),
                           m_points[i].detail, this);
    } else {
        QToolTip::hideText();
    }
}

void KiwiWorldMap::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) { return; }
    m_pressed = true;
    m_dragged = false;
    m_pressPos = event->position();
    m_dragLast = event->position();
}

// Ausgewaehlt wird beim Loslassen: Ziehen ist kein Klick.
void KiwiWorldMap::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !m_pressed) { return; }
    m_pressed = false;
    if (m_dragged) { setCursor(Qt::ArrowCursor); return; }
    const QString id = idAt(event->position());
    if (!id.isEmpty()) { emit pointClicked(id); }
}

void KiwiWorldMap::leaveEvent(QEvent*)
{
    if (!m_hovered.isEmpty()) {
        m_hovered.clear();
        emit pointHovered(QString());
        update();
    }
}

void KiwiWorldMap::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), col(Style::kInsetBg));

    double la0, la1, lo0, lo1;
    computeView(la0, la1, lo0, lo1);
    const auto proj = [&](double lat, double lon) {
        return QPointF((lon - lo0) / (lo1 - lo0) * width(),
                       (la1 - lat) / (la1 - la0) * height());
    };

    // Netz alle 30 Grad, kaum sichtbar.
    p.setPen(QPen(col(Style::kPanelBg).lighter(125), 1));
    for (int lo = -180; lo <= 180; lo += 30) {
        p.drawLine(proj(la1, lo), proj(la0, lo));
    }
    for (int la = -60; la <= 90; la += 30) {
        p.drawLine(proj(la, lo0), proj(la, lo1));
    }

    // Land.
    const short* d = kiwiCoastData();
    const int rings = kiwiCoastRingCount();
    p.setPen(Qt::NoPen);
    p.setBrush(col(Style::kButtonBg));
    int at = 0;
    for (int r = 0; r < rings; ++r) {
        const int n = d[at++];
        QPolygonF poly;
        poly.reserve(n);
        for (int i = 0; i < n; ++i) {
            poly << proj(d[at + 2 * i + 1] / 10.0, d[at + 2 * i] / 10.0);
        }
        at += 2 * n;
        p.drawPolygon(poly);
    }

    // Laender mit sichtbarem Umriss (Betreiber 2026-11-28). Die Ringe der
    // Grenzen enthalten die Kuesten, deshalb hat die Flaeche oben keinen Rand.
    const short* b = kiwiBorderData();
    p.setPen(QPen(col(Style::kTextScale), 1));
    int bt = 0;
    for (int r = 0; r < kiwiBorderRingCount(); ++r) {
        const int n = b[bt++];
        QPolygonF poly;
        poly.reserve(n);
        for (int i = 0; i < n; ++i) {
            poly << proj(b[bt + 2 * i + 1] / 100.0, b[bt + 2 * i] / 100.0);
        }
        bt += 2 * n;
        p.drawPolygon(poly);
    }

    // Grosskreislinien vom Standort zu geoeffneten Kiwis.
    if (m_homeValid) {
        for (const KiwiMapPoint& pt : m_points) {
            if (!pt.open) { continue; }
            QColor c = m_heardMode ? pointColor(pt, true) : col(Style::kBlueBorder);
            c.setAlpha(m_heardMode ? 170 : 200);
            p.setPen(QPen(c, 1.2));
            const double a1 = m_homeLat * kPi / 180, o1 = m_homeLon * kPi / 180;
            const double a2 = pt.lat * kPi / 180, o2 = pt.lon * kPi / 180;
            const double v1[3] = {std::cos(a1) * std::cos(o1), std::cos(a1) * std::sin(o1), std::sin(a1)};
            const double v2[3] = {std::cos(a2) * std::cos(o2), std::cos(a2) * std::sin(o2), std::sin(a2)};
            const double dot = std::clamp(v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2], -1.0, 1.0);
            const double om = std::acos(dot);
            QPointF prev;
            double prevLon = 0.0;
            bool have = false;
            for (int i = 0; i <= 48; ++i) {
                const double t = i / 48.0;
                double v[3];
                if (om < 1e-6) {
                    for (int k = 0; k < 3; ++k) { v[k] = v1[k]; }
                } else {
                    for (int k = 0; k < 3; ++k) {
                        v[k] = (std::sin((1 - t) * om) * v1[k] + std::sin(t * om) * v2[k]) / std::sin(om);
                    }
                }
                const double la = std::asin(std::clamp(v[2], -1.0, 1.0)) * 180 / kPi;
                const double lo = std::atan2(v[1], v[0]) * 180 / kPi;
                const QPointF q = proj(la, lo);
                if (have && std::abs(lo - prevLon) < 180.0) { p.drawLine(prev, q); }
                prev = q; prevLon = lo; have = true;
            }
        }
    }

    // Punkte.
    QFont f = font();
    f.setPointSizeF(std::max(7.5, f.pointSizeF() - 1.0));
    p.setFont(f);
    // Weit genug herangeholt und nicht zu viele im Bild: mit Namen, einer
    // ueber dem anderen wird ausgelassen.
    int inView = 0;
    for (const KiwiMapPoint& pt : m_points) {
        if (rect().contains(proj(pt.lat, pt.lon).toPoint())) { ++inView; }
    }
    const bool zoomNames = !m_heardMode && !m_autoFit && m_zoom >= 8.0 && inView <= 60;
    // Belegt sind zuerst alle Punkte und der Standort: kein Name deckt sie zu.
    QVector<QRectF> taken;
    if (zoomNames) {
        for (const KiwiMapPoint& pt : m_points) {
            const QPointF c = proj(pt.lat, pt.lon);
            taken.append(QRectF(c.x() - 5, c.y() - 5, 10, 10));
        }
        if (m_homeValid) {
            const QPointF c = proj(m_homeLat, m_homeLon);
            const int w = p.fontMetrics().horizontalAdvance(m_homeLabel);
            taken.append(QRectF(c.x() - 6, c.y() - 6, 12, 12));
            taken.append(QRectF(c.x() - 8 - w, c.y() + 14 - p.fontMetrics().ascent(),
                                w, p.fontMetrics().height()));
        }
    }
    for (const KiwiMapPoint& pt : m_points) {
        const QPointF c = proj(pt.lat, pt.lon);
        if (c.x() < -10 || c.x() > width() + 10 || c.y() < -10 || c.y() > height() + 10) {
            continue;
        }
        const QColor color = pointColor(pt, m_heardMode);
        const double r = (pt.open || pt.id == m_selected || pt.id == m_hovered) ? 4.5 : 3.4;
        if (pt.open || pt.id == m_selected) {
            p.setPen(QPen(col(Style::kBlueBorder), 1.4));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, r + 3.0, r + 3.0);
        }
        p.setPen(QPen(color, 1.2));
        const bool hollow = pt.state == KiwiMapPoint::State::Offline
            && !(m_heardMode && pt.heard != KiwiMapPoint::Heard::None);
        if (hollow || (m_heardMode && pt.heard == KiwiMapPoint::Heard::Nothing)) {
            p.setBrush(Qt::NoBrush);
        } else {
            p.setBrush(color);
        }
        p.drawEllipse(c, r, r);
        if (pt.state == KiwiMapPoint::State::Partial && !m_heardMode) {
            // halb gefuellt: rechte Haelfte hell, linke dunkel
            QPainterPath half;
            half.moveTo(c.x(), c.y() - r);
            half.arcTo(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r), 90, 180);
            half.closeSubpath();
            p.setPen(Qt::NoPen);
            p.setBrush(col(Style::kInsetBg));
            p.drawPath(half);
        }
        if (!pt.label.isEmpty() && (m_heardMode || m_autoFit)) {
            p.setPen(col(Style::kTextPrimary));
            p.drawText(c + QPointF(r + 5, 4), pt.label);
        }
        if ((m_heardMode || m_autoFit) && !pt.name.isEmpty()) {
            p.setPen(col(Style::kTextSecondary));
            p.drawText(c + QPointF(r + 5, -4), pt.name);
        }
        if (zoomNames && !pt.name.isEmpty()) {
            const QString text = p.fontMetrics().elidedText(pt.name, Qt::ElideRight, 190);
            const double w = p.fontMetrics().horizontalAdvance(text);
            const double h = p.fontMetrics().height();
            // Rechts, sonst links, darunter, darueber.
            const QRectF places[4] = {
                {c.x() + r + 5, c.y() - h / 2, w, h},
                {c.x() - r - 5 - w, c.y() - h / 2, w, h},
                {c.x() - w / 2, c.y() + r + 3, w, h},
                {c.x() - w / 2, c.y() - r - 3 - h, w, h}};
            for (const QRectF& box : places) {
                const bool free = std::none_of(taken.cbegin(), taken.cend(),
                    [&box](const QRectF& o) { return o.intersects(box); });
                if (!free) { continue; }
                taken.append(box);
                p.setPen(col(Style::kTextPrimary));
                p.drawText(box, Qt::AlignLeft | Qt::AlignVCenter, text);
                break;
            }
        }
    }

    // Legende unten links (nur im Zustandsmodus; im Gehoert-Modus sprechen
    // die Beschriftungen selbst).
    if (!m_heardMode && height() >= 150) {
        const QString texts[4] = {tr("free"), tr("partly busy"), tr("full"), tr("not reachable")};
        const KiwiMapPoint::State states[4] = {
            KiwiMapPoint::State::Free, KiwiMapPoint::State::Partial,
            KiwiMapPoint::State::Full, KiwiMapPoint::State::Offline};
        double x = 8.0;
        const double y = height() - 10.0;
        QFont lf = font();
        lf.setPointSizeF(std::max(7.5, lf.pointSizeF() - 1.0));
        p.setFont(lf);
        // Dunkel hinterlegt: sonst laufen die Grenzen durch die Schrift.
        double total = 4.0;
        for (const QString& t : texts) {
            total += 12 + p.fontMetrics().horizontalAdvance(t) + 16;
        }
        QColor back = col(Style::kInsetBg);
        back.setAlpha(215);
        p.setPen(Qt::NoPen);
        p.setBrush(back);
        p.drawRoundedRect(QRectF(2, y - 10, total - 6, 20), 6, 6);
        for (int i = 0; i < 4; ++i) {
            KiwiMapPoint legendPoint;
            legendPoint.state = states[i];
            const QColor c = pointColor(legendPoint, false);
            p.setPen(QPen(c, 1.2));
            p.setBrush(states[i] == KiwiMapPoint::State::Offline ? Qt::NoBrush : QBrush(c));
            p.drawEllipse(QPointF(x + 4, y), 3.4, 3.4);
            if (states[i] == KiwiMapPoint::State::Partial) {
                QPainterPath half;
                half.moveTo(x + 4, y - 3.4);
                half.arcTo(QRectF(x + 0.6, y - 3.4, 6.8, 6.8), 90, 180);
                half.closeSubpath();
                p.setPen(Qt::NoPen);
                p.setBrush(col(Style::kInsetBg));
                p.drawPath(half);
            }
            p.setPen(col(Style::kTextSecondary));
            p.drawText(QPointF(x + 12, y + 4), texts[i]);
            x += 12 + p.fontMetrics().horizontalAdvance(texts[i]) + 16;
        }
    }

    // Standort: Ring, in der TX-Farbe (ich sende).
    if (m_homeValid) {
        const QPointF c = proj(m_homeLat, m_homeLon);
        p.setPen(QPen(col(Style::kRedBorder), 2.0));
        p.setBrush(col(Style::kInsetBg));
        p.drawEllipse(c, 4.5, 4.5);
        if (!m_homeLabel.isEmpty()) {
            p.setPen(col(Style::kTextPrimary));
            p.drawText(c + QPointF(-8 - p.fontMetrics().horizontalAdvance(m_homeLabel), 14),
                       m_homeLabel);
        }
    }
}

} // namespace Longpath
