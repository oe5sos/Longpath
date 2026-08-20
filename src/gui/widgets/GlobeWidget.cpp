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
//   2026-08-10 — Atmosphere rim off by default, behind
//                 setShowAtmosphere(); see GlobeWidget.h. AI-assisted
//                 via Anthropic Claude (Cowork), operator Martin
//                 Fischer.
// =================================================================

#include "GlobeWidget.h"
#include "WorldTexture.h"
#include "gui/StyleConstants.h"
#include "gui/styles/PopupMenuStyle.h"

#include <QDateTime>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QTimer>
#include <QWheelEvent>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace Longpath {

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
    // Tasten kommen nur an einem Widget an, das Fokus annehmen darf.
    setFocusPolicy(Qt::ClickFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    // Siehe FlatMapWidget: derselbe Geber erreicht beide Ansichten.
    // Ein Bildwechsel erreicht jede Ansicht ueber den Geber in
    // WorldTexture -- kein Aufruf, den eine neue Ansicht vergessen
    // koennte. Siehe die Notiz bei WorldTexture::Notifier.
    connect(&WorldTexture::Notifier::instance(),
            &WorldTexture::Notifier::changed,
            this, qOverload<>(&QWidget::update));

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

        // Breite mit derselben Daempfung (2026-08-19). Vorher sprang sie,
        // was beim Hinfliegen auf einen Ort wie ein Bildfehler aussieht.
        const double dLat = m_targetViewLat - m_viewLat;
        if (std::abs(dLat) > 0.2) {
            m_viewLat = std::clamp(m_viewLat + dLat * 0.12, -85.0, 85.0);
            moving = true;
        }

        // Zoom multiplikativ, nicht additiv: von 1x auf 2x ist derselbe
        // Weg wie von 2x auf 4x, und nur so fuehlt sich das Naeherkommen
        // gleichmaessig an.
        if (std::abs(m_targetZoom - m_zoom) > 0.01) {
            m_zoom += (m_targetZoom - m_zoom) * 0.18;
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
    // Shared with the flat map through WorldTexture: the large Blue
    // Marble is 58 MB decoded, and two widgets each holding their own
    // copy is 58 MB nobody asked for.
    if (!WorldTexture::setPath(path)) { return false; }
    m_frameDirty = true;
    update();
    return true;
}

bool GlobeWidget::hasTexture() const
{
    return !WorldTexture::image().isNull();
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

void GlobeWidget::setPoints(const QVector<MapPoint>& points)
{
    m_points = points;
    update();
}

void GlobeWidget::setShowPointPaths(bool on)
{
    m_showPointPaths = on;
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

void GlobeWidget::flyTo(double lat, double lon, double zoomFactor)
{
    m_targetViewLat = std::clamp(lat, -85.0, 85.0);
    m_targetViewLon = norm180(lon);
    m_targetZoom    = std::clamp(m_zoom * zoomFactor, 0.6, maxZoom());
    m_frameDirty    = true;
    if (!m_anim->isActive()) { m_anim->start(); }
}

void GlobeWidget::setAutoRotate(bool on)
{
    m_autoRotate = on;
    if (on && !m_anim->isActive()) { m_anim->start(); }
}

void GlobeWidget::setShowAtmosphere(bool on)
{
    m_showAtmosphere = on;
    update();
}

void GlobeWidget::setBeamSpread(double deg)
{
    m_beamSpread = std::clamp(deg, 0.0, 60.0);
    update();
}

void GlobeWidget::zoomBy(double factor)
{
    const double before = m_zoom;
    m_zoom = std::clamp(m_zoom * factor, 0.6, maxZoom());
    m_targetZoom = m_zoom;   // Handbetrieb schlaegt die Animation
    if (qFuzzyCompare(before, m_zoom)) { return; }
    m_frameDirty = true;
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
    m_targetViewLat = m_viewLat;   // sonst zieht die Animation zurueck
    m_targetZoom    = m_zoom;
    m_frameDirty = true;
    update();
}

// ── Kontextmenue ────────────────────────────────────────────────────
//
// Warum es das ueberhaupt gibt: der Weg zurueck lag auf dem Doppelklick
// NEBEN die Kugel — und ab etwa 1,15x fuellt die Scheibe das ganze
// Fenster, es gibt also kein „neben" mehr. Der Rueckweg waere genau dann
// unerreichbar, wenn man ihn braucht. Aufgefallen ist das an einem Test,
// dessen Annahme falsch war, nicht im Betrieb (2026-08-19).
//
// Ein Rechtsklick ist bei jedem Zoom da. Zugleich beantwortet das den
// Satz, der schon bei zoomBy() steht: ein Mausrad ist keine
// Bedienflaeche — hier steht jetzt, dass die Kugel zoomt.
void GlobeWidget::contextMenuEvent(QContextMenuEvent* e)
{
    QMenu menu(this);
    // Dieselbe Menue-Optik wie ueberall sonst (RxApplet, VFO-Flagge,
    // Panadapter): sonst sieht ein Menue nach einem anderen Programm aus.
    menu.setStyleSheet(QString::fromLatin1(kPopupMenu));

    QAction* in   = menu.addAction(QStringLiteral("Zoom in"));
    QAction* out  = menu.addAction(QStringLiteral("Zoom out"));
    menu.addSeparator();
    QAction* toTx = menu.addAction(QStringLiteral("Fly to station"));
    toTx->setEnabled(m_hasTarget);
    toTx->setToolTip(m_hasTarget
        ? QStringLiteral("Centre on the station being worked")
        : QStringLiteral("No station set — enter a callsign first"));
    QAction* home = menu.addAction(QStringLiteral("Reset view"));

    const QAction* chosen = menu.exec(e->globalPos());
    if (chosen == in)        { zoomBy(1.3); }
    else if (chosen == out)  { zoomBy(1.0 / 1.3); }
    else if (chosen == toTx && m_hasTarget) { flyTo(m_targetLat, m_targetLon); }
    else if (chosen == home) { resetView(); }
    e->accept();
}

// ── Tastatur ────────────────────────────────────────────────────────
//
// Weil ein Mausrad keine Bedienflaeche ist — derselbe Grund, aus dem
// zoomBy() ueberhaupt oeffentlich ist. Wer die Kugel angeklickt hat, kann
// sie ab 2026-08-19 auch mit den Tasten bewegen.
void GlobeWidget::keyPressEvent(QKeyEvent* e)
{
    constexpr double kStepDeg = 8.0;

    switch (e->key()) {
    case Qt::Key_Plus:
    case Qt::Key_Equal:      zoomBy(1.3);            break;
    case Qt::Key_Minus:      zoomBy(1.0 / 1.3);      break;
    case Qt::Key_0:          resetView();            break;
    case Qt::Key_Left:
        m_targetViewLon = norm180(m_viewLon - kStepDeg);
        if (!m_anim->isActive()) { m_anim->start(); }
        break;
    case Qt::Key_Right:
        m_targetViewLon = norm180(m_viewLon + kStepDeg);
        if (!m_anim->isActive()) { m_anim->start(); }
        break;
    case Qt::Key_Up:
        m_targetViewLat = std::clamp(m_viewLat + kStepDeg, -85.0, 85.0);
        if (!m_anim->isActive()) { m_anim->start(); }
        break;
    case Qt::Key_Down:
        m_targetViewLat = std::clamp(m_viewLat - kStepDeg, -85.0, 85.0);
        if (!m_anim->isActive()) { m_anim->start(); }
        break;
    case Qt::Key_T:
        // Zur Gegenstation fliegen — der haeufigste Wunsch im Betrieb.
        // Ohne gesetztes Ziel absichtlich nichts: eine Taste, die
        // irgendwohin fliegt, ist schlimmer als eine, die schweigt.
        if (m_hasTarget) { flyTo(m_targetLat, m_targetLon); }
        break;
    default:
        QWidget::keyPressEvent(e);
        return;
    }
    e->accept();
}

// ── Mouse ───────────────────────────────────────────────────────────

void GlobeWidget::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) { QWidget::mousePressEvent(e); return; }

    setFocus(Qt::MouseFocusReason);   // sonst erreichen Tasten die Kugel nie

    // Erst die Knoepfe, dann das Ziehen: sonst dreht sich die Kugel,
    // waehrend man auf Plus drueckt.
    if (zoomButtonRect(true).contains(e->position())) {
        zoomBy(1.3);
        e->accept();
        return;
    }
    if (zoomButtonRect(false).contains(e->position())) {
        zoomBy(1.0 / 1.3);
        e->accept();
        return;
    }

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
    m_targetViewLat = m_viewLat;   // Ziehen beendet einen laufenden Flug
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
        // Auf der Kugel: dorthin fliegen (2026-08-19, auf Ansage des
        // Betreibers „wie bei Google Earth"). Das ist dort die Geste, mit
        // der man sich naeher holt.
        double lat = 0.0, lon = 0.0;
        if (unproject(e->position(), lat, lon)) {
            flyTo(lat, lon);
            return;
        }

        // NEBEN der Kugel: der Weg zurueck, wie bisher. Having turned the
        // globe to somewhere unhelpful, hunting for a reset button is
        // worse than a double click — der Satz stimmt weiter, nur die
        // Flaeche ist jetzt die leere Ecke statt der Kugel selbst.
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
    m_zoom = std::clamp(m_zoom * std::pow(1.15, steps), 0.6, maxZoom());
    m_targetZoom = m_zoom;

    // Am oberen Anschlag weiter hineindrehen: die Kugel kann nicht
    // naeher, die flache Karte schon. Den Ort mitgeben, damit dort
    // dieselbe Stelle in der Mitte liegt (2026-08-19).
    if (steps > 0.0 && qFuzzyCompare(before, maxZoom())
            && qFuzzyCompare(m_zoom, maxZoom())) {
        double lat = m_viewLat, lon = m_viewLon;
        unproject(e->position(), lat, lon);   // scheitert am Rand: Mitte bleibt
        emit zoomedInPastCeiling(lat, lon);
        e->accept();
        return;
    }

    if (!qFuzzyCompare(before, m_zoom)) {
        // Zum Zeiger hin, nicht zur Mitte (2026-08-19). Beim Hineinzoomen
        // wandert die Kamera einen Teil des Weges zu dem Ort, auf den der
        // Betreiber zeigt — das ist der Unterschied, den man zu Google
        // Earth am deutlichsten spuert.
        //
        // Ein Teil des Weges, nicht der ganze: den Punkt exakt unter dem
        // Zeiger festzunageln erfordert auf der Kugel eine Drehung um die
        // Sehachse, und die verdreht den Horizont. Ein Drittel je
        // Radschritt fuehlt sich wie Hinfliegen an und laesst Nord oben.
        double lat = 0.0, lon = 0.0;
        if (steps > 0.0 && unproject(e->position(), lat, lon)) {
            constexpr double kEase = 0.34;
            m_viewLat = std::clamp(m_viewLat + (lat - m_viewLat) * kEase,
                                   -85.0, 85.0);
            // Kuerzester Weg in Laengengraden: ohne das dreht die Kugel
            // beim Zoomen auf Kamtschatka einmal falsch herum.
            double dLon = norm180(lon - m_viewLon);
            m_viewLon = norm180(m_viewLon + dLon * kEase);
            m_targetViewLon = m_viewLon;
        }
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

// Lage der beiden Zoomknoepfe, unten rechts uebereinander. Eine
// Funktion, weil Zeichnung UND Trefferpruefung dieselbe Stelle brauchen
// — zwei Rechnungen waeren zwei Gelegenheiten, sie auseinanderlaufen zu
// lassen.
QRectF GlobeWidget::zoomButtonRect(bool plus) const
{
    constexpr double kSize = 22.0;
    constexpr double kPad  = 8.0;
    const double x = width() - kPad - kSize;
    const double y = height() - kPad - kSize - (plus ? kSize + 4.0 : 0.0);
    return QRectF(x, y, kSize, kSize);
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

double GlobeWidget::maxZoom() const
{
    const QImage& tex = WorldTexture::image();
    if (tex.isNull()) { return 6.0; }

    // 6x ist die Decke, die zur kleinen 2048er Textur passt. Groessere
    // Bilder tragen linear mehr; 24x als harte Obergrenze, weil darueber
    // die Kugel nur noch ein Ausschnitt ist und die flache Karte das
    // besser kann.
    const double scale = static_cast<double>(tex.width()) / 2048.0;
    return std::clamp(6.0 * scale, 6.0, 24.0);
}

bool GlobeWidget::unproject(const QPointF& pos, double& lat, double& lon) const
{
    const double r = radiusPx();
    if (r <= 0.0) { return false; }

    const double X = (pos.x() - width() * 0.5) / r;
    const double Y = (height() * 0.5 - pos.y()) / r;
    const double rho2 = X * X + Y * Y;
    if (rho2 > 1.0) { return false; }   // ausserhalb der Scheibe

    const double Z   = std::sqrt(1.0 - rho2);
    const double vla = m_viewLat * kDeg;

    // Umkehrung von projectAlt(): dort ist
    //   sin(la) = cos(vla) * Y + sin(vla) * Z
    //   cos(la) * sin(lo) = X
    //   cos(la) * cos(lo) = Z * cos(vla) - Y * sin(vla)
    const double sinLa = std::cos(vla) * Y + std::sin(vla) * Z;
    lat = std::asin(std::clamp(sinLa, -1.0, 1.0)) / kDeg;
    lon = norm180(m_viewLon
                  + std::atan2(X, Z * std::cos(vla) - Y * std::sin(vla)) / kDeg);
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

    // One local handle to the shared image. QImage is implicitly
    // shared, so this copies a pointer, not 58 MB.
    const QImage texture = WorldTexture::image();
    const bool tex = !texture.isNull();
    const int tw = tex ? texture.width()  : 0;
    const int th = tex ? texture.height() : 0;

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
                base = texture.pixel(tx, ty);
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
                          double opacity, int steps) const
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

    const int kSteps = qBound(16, steps, 240);
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

    // Atmosphere: a soft rim just outside the disc. Optional and off by
    // default — in a narrow dock it reads as a circular bar over the
    // globe rather than as air.
    if (m_showAtmosphere) {
        QRadialGradient halo(c, r * 1.16);
        halo.setColorAt(0.86, QColor(0, 0, 0, 0));
        halo.setColorAt(0.94, QColor(80, 160, 230, 46));
        halo.setColorAt(1.00, QColor(80, 160, 230, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(halo);
        p.drawEllipse(c, r * 1.16, r * 1.16);
        p.setBrush(Qt::NoBrush);
    }

    p.drawImage(0, 0, m_frame);

    // Graticule every 30°, clipped to the visible hemisphere by the
    // projection itself.
    const bool untextured = !hasTexture();
    p.setPen(QPen(QColor(255, 255, 255, untextured ? 44 : 28), 0.8));
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
                drawArc(p, lat, lon, accent, 1.0, 0.34, 160);
            }
        }
        // The live path is one arc and gets the full sample count.
        drawArc(p, m_targetLat, m_targetLon, accent, 1.6, 1.0, 200);
    }

    // Logged contacts. Thinner and dimmer than the live path on purpose:
    // 500 arcs drawn like the one live path would bury it. Contacts
    // marked in the log's table are drawn in a second pass, so they are
    // not buried under the ordinary ones either.
    if (m_hasHome && !m_points.isEmpty()) {
        const QColor faint(Style::kAccent);
        const QColor marked(Style::kAmberText);

        // Sample count against the number of arcs, not the width of
        // one. Five hundred arcs at the single-path count would be
        // eighty thousand slerp evaluations per repaint, and the globe
        // would stop turning smoothly the moment a large selection was
        // shown — which is exactly when the operator is looking at it.
        const int n = m_points.size();
        const int arcSteps = n > 200 ? 40 : (n > 60 ? 72 : 128);

        for (int pass = 0; pass < 2; ++pass) {
            const bool wantMarked = pass == 1;
            const QColor col = wantMarked ? marked : faint;

            // Marked contacts keep their arc even with paths switched
            // off: turning paths off is how you clear the clutter to
            // look at a few marked ones, so removing theirs as well
            // would defeat the switch.
            if (m_showPointPaths || wantMarked) {
                for (const MapPoint& pt : m_points) {
                    if (pt.highlight != wantMarked) { continue; }
                    drawArc(p, pt.lat, pt.lon, col,
                            wantMarked ? 1.3 : 0.7,
                            wantMarked ? 0.9 : 0.22,
                            arcSteps);
                }
            }
            for (const MapPoint& pt : m_points) {
                if (pt.highlight != wantMarked) { continue; }
                QPointF s;
                if (!project(pt.lat, pt.lon, s)) { continue; }
                const double r2 = wantMarked ? 2.8 : 2.0;

                if (pt.approximate) {
                    // A ring: the middle of a country, not a place.
                    p.setBrush(Qt::NoBrush);
                    p.setPen(QPen(col, 1.2));
                    p.drawEllipse(s, r2 + 1.4, r2 + 1.4);
                    continue;
                }
                p.setPen(Qt::NoPen);
                QColor glow = col;
                glow.setAlpha(wantMarked ? 120 : 80);
                p.setBrush(glow);
                p.drawEllipse(s, wantMarked ? 6.5 : 4.5,
                                 wantMarked ? 6.5 : 4.5);
                p.setBrush(col);
                p.drawEllipse(s, r2, r2);
            }
            p.setBrush(Qt::NoBrush);

            if (wantMarked) {
                QFont lf = p.font();
                lf.setPixelSize(9);
                lf.setBold(true);
                p.setFont(lf);
                p.setPen(marked);
                for (const MapPoint& pt : m_points) {
                    if (!pt.highlight || pt.label.isEmpty()) { continue; }
                    QPointF s;
                    if (!project(pt.lat, pt.lon, s)) { continue; }
                    p.drawText(s + QPointF(8, 3), pt.label);
                }
            }
        }
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

    if (untextured) {
        // Say why it looks plain, and what fixes it.
        QFont f = p.font();
        f.setPixelSize(11);
        p.setFont(f);
        p.setPen(QColor(Style::kTextScale));
        const QString hint = QStringLiteral("no world image loaded");
        p.drawText(QPointF((width() - QFontMetrics(f).horizontalAdvance(hint)) / 2.0,
                           height() - 6.0), hint);
    }

    // ── Plus und Minus (2026-08-19) ─────────────────────────────────
    //
    // Auf Ansage des Betreibers: „es sollte in der Grafik auch ein Plus
    // und Minus zum Vergroessern sein." Damit beantwortet die Kugel
    // endlich den Satz, der seit je bei zoomBy() steht: ein Mausrad ist
    // keine Bedienflaeche.
    //
    // Gemalt statt als Kind-Widget: die Kugel rendert sich ohnehin
    // selbst, und zwei QPushButtons darueber wuerden beim Ziehen die
    // Mausereignisse abfangen und die Drehung zerreissen.
    {
        const QRectF plus  = zoomButtonRect(true);
        const QRectF minus = zoomButtonRect(false);
        const QColor face(Style::kPanelBg);
        const QColor edge(Style::kBorderSubtle);
        const QColor text(Style::kTextSecondary);

        QFont f = p.font();
        f.setPixelSize(15);
        f.setBold(true);
        p.setFont(f);

        for (int i = 0; i < 2; ++i) {
            const QRectF r = (i == 0) ? plus : minus;
            const bool atLimit = (i == 0) ? (m_zoom >= maxZoom() - 1e-6)
                                          : (m_zoom <= 0.6 + 1e-6);
            p.setBrush(QColor(face.red(), face.green(), face.blue(), 210));
            p.setPen(QPen(edge, 1));
            p.drawRoundedRect(r, 4, 4);

            // Am Anschlag blass: ein Knopf, der nichts tut, soll das auch
            // zeigen, statt den Betreiber raten zu lassen.
            QColor t = text;
            t.setAlpha(atLimit ? 90 : 235);
            p.setPen(t);
            p.drawText(r, Qt::AlignCenter,
                       (i == 0) ? QStringLiteral("+") : QStringLiteral("\u2212"));
        }
    }
}

} // namespace Longpath
