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
//   2026-08-10 — Grid overlay + clickable markers; see FlatMapWidget.h.
//                 AI-assisted via Anthropic Claude (Cowork), operator
//                 Martin Fischer.
// =================================================================

#include "FlatMapWidget.h"
#include "WorldTexture.h"

#include "core/SolarTimes.h"
#include "gui/StyleConstants.h"
#include "gui/styles/ThemeQss.h"   // Style::role() — Malcode hat kein Stylesheet

#include <QDateTime>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
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

/// Die Beschriftung der Locator-Felder. Warmes Weiß, sehr durchsichtig
/// — sie soll über der Textur liegen und nicht auf ihr. Über eine
/// Rolle, weil hier gemalt wird und kein Stylesheet vorbeikommt.
QColor locatorInk()
{
    QColor c(Style::role("map-locator", "#ffdc96"));
    c.setAlpha(70);
    return c;
}

} // namespace

FlatMapWidget::FlatMapWidget(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setCursor(Qt::OpenHandCursor);
    // Ein Bildwechsel erreicht jede Ansicht ueber den Geber in
    // WorldTexture -- kein Aufruf, den eine neue Ansicht vergessen
    // koennte. Siehe die Notiz bei WorldTexture::Notifier.
    connect(&WorldTexture::Notifier::instance(),
            &WorldTexture::Notifier::changed,
            this, qOverload<>(&QWidget::update));

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

void FlatMapWidget::setStationMarker(const QString& callsign,
                                     const QImage& photo)
{
    m_stationCall = callsign.trimmed().toUpper();
    m_stationPhoto = QPixmap{};

    if (photo.isNull()) {
        update();
        return;
    }

    // ── Einmal schneiden, nicht bei jedem Bild ───────────────────────
    //
    // Die Karte zeichnet beim Ziehen dutzendfach pro Sekunde neu. Ein
    // volles Foto pro Bild zu skalieren und rund zu maskieren würde man
    // als Ruckeln merken, und zwar genau dann, wenn man die Karte
    // bewegt.
    //
    // devicePixelRatio, sonst ist der Kreis auf einem Retina-Bildschirm
    // weich, während alles daneben scharf ist.
    const qreal dpr = devicePixelRatioF();
    const int px = static_cast<int>(kStationMarkerPx * dpr);

    // Quadratisch aus der Mitte, damit ein Querformat nicht gestaucht
    // wird, sondern beschnitten — ein gestauchtes Gesicht fällt auf.
    const int side = std::min(photo.width(), photo.height());
    const QImage square = photo.copy((photo.width()  - side) / 2,
                                     (photo.height() - side) / 2,
                                     side, side)
                              .scaled(px, px, Qt::IgnoreAspectRatio,
                                      Qt::SmoothTransformation);

    QPixmap out(px, px);
    out.fill(Qt::transparent);
    {
        QPainter mp(&out);
        mp.setRenderHint(QPainter::Antialiasing);
        QPainterPath clip;
        clip.addEllipse(0, 0, px, px);
        mp.setClipPath(clip);
        mp.drawImage(0, 0, square);
    }
    out.setDevicePixelRatio(dpr);
    m_stationPhoto = out;
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

void FlatMapWidget::setShowGrid(bool on)
{
    m_showGrid = on;
    if (!on) { m_clickedGrid.clear(); }
    update();
}

void FlatMapWidget::refreshTexture()
{
    WorldTexture::reload();
    update();
}

void FlatMapWidget::zoomBy(double factor)
{
    const double before = m_zoom;
    m_zoom = std::clamp(m_zoom * factor, 1.0, 12.0);
    if (qFuzzyCompare(before, m_zoom)) { return; }

    // Keep the centre of the view where it is. Zooming from a button
    // has no pointer to anchor on, and letting the map drift sideways
    // on every press makes the buttons feel broken.
    m_pan *= m_zoom / before;
    update();
}

void FlatMapWidget::resetView()
{
    m_zoom = 1.0;
    m_pan = QPointF(0.0, 0.0);
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

bool FlatMapWidget::unproject(const QPointF& pos, double& lat, double& lon) const
{
    const QRectF r = mapRect();
    if (!r.contains(pos) || r.width() <= 0.0 || r.height() <= 0.0) {
        return false;
    }
    lon = (pos.x() - r.left()) / r.width() * 360.0 - 180.0;
    lat = 90.0 - (pos.y() - r.top()) / r.height() * 180.0;
    return true;
}

QString FlatMapWidget::gridSquare4(double lat, double lon)
{
    // Clamp instead of wrap: 90°N belongs to the topmost square, not to
    // a wrap back to the bottom of the alphabet.
    const double lo = std::clamp(norm180(lon) + 180.0, 0.0, 359.999);
    const double la = std::clamp(lat + 90.0, 0.0, 179.999);
    QString g;
    g += QChar('A' + static_cast<int>(lo / 20.0));
    g += QChar('A' + static_cast<int>(la / 10.0));
    g += QChar('0' + static_cast<int>(std::fmod(lo, 20.0) / 2.0));
    g += QChar('0' + static_cast<int>(std::fmod(la, 10.0)));
    return g;
}

int FlatMapWidget::pointAt(const QPointF& pos) const
{
    // Nearest marker within a comfortable click radius, not merely the
    // first hit — in a pile-up of dots the nearest one is the one the
    // operator aimed at.
    constexpr double kRadius = 9.0;
    int best = -1;
    double bestD = kRadius;
    for (int i = 0; i < m_points.size(); ++i) {
        const QPointF s = project(m_points.at(i).lat, m_points.at(i).lon);
        const double d = std::hypot(s.x() - pos.x(), s.y() - pos.y());
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
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
        // Der Ozean, wenn keine NASA-Textur da ist. Über eine Rolle,
        // damit die Theme-Datei ihn erreicht — hier wird gemalt, es
        // gibt kein Stylesheet, an dem der ThemeFilter ansetzen könnte.
        p.fillRect(r, QColor(Style::role("map-ocean", "#1a3a58")));
    }

    if (m_showTerminator) {
        if (m_nightDirty || m_night.isNull()) { buildNightOverlay(); }
        p.drawImage(r, m_night);
    }

    // ── Der Urhebervermerk ──────────────────────────────────────────
    //
    // An der FENSTERecke, nicht an der Kartenecke. mapRect() waechst mit
    // dem Zoom ueber das Fenster hinaus -- ein Vermerk an r.bottomRight()
    // waere ab dem ersten Radschritt ausserhalb des Sichtbaren, und zwar
    // genau dann, wenn der Betreiber sich die Karte genauer ansieht.
    //
    // Nicht abschaltbar, solange das Bild gewaehlt ist: wo die Herkunft
    // einen Vermerk verlangt, ist er Bedingung der Nutzung und keine
    // Anzeigeoption.
    if (!tex.isNull()) {
        const QString credit = WorldTexture::requiredAttribution();
        if (!credit.isEmpty()) {
            QFont f = p.font();
            f.setPixelSize(9);
            p.setFont(f);
            const QFontMetrics fm(f);
            const int tw = fm.horizontalAdvance(credit);
            const int x  = width()  - tw - 8;
            const int y  = height() - 6;
            // Dunkle Unterlegung: der Vermerk muss auf hellem Eis
            // genauso lesbar sein wie auf dunklem Ozean.
            QColor box(Style::kAppBg);
            box.setAlpha(170);
            p.setPen(Qt::NoPen);
            p.setBrush(box);
            p.drawRect(QRect(x - 4, y - fm.ascent() - 2,
                             tw + 8, fm.height() + 4));
            p.setBrush(Qt::NoBrush);
            p.setPen(QColor(Style::role("text-scale", Style::kTextScale)));
            p.drawText(x, y, credit);
        }
    }

    // Graticule every 30 degrees, plus the equator picked out.
    //
    // Die Deckung bleibt hier und wandert nicht in die Theme-Datei: sie
    // hängt davon ab, ob eine Textur darunter liegt, und das ist eine
    // Frage der Lesbarkeit, keine der Gestaltung.
    auto graticule = [](int alpha) {
        QColor c(Style::role("map-graticule", "#ffffff"));
        c.setAlpha(alpha);
        return c;
    };
    p.setPen(QPen(graticule(tex.isNull() ? 48 : 30), 0.8));
    for (int lon = -180; lon <= 180; lon += 30) {
        const QPointF a = project(90, lon);
        const QPointF b = project(-90, lon);
        p.drawLine(a, b);
    }
    for (int lat = -60; lat <= 60; lat += 30) {
        p.drawLine(project(lat, -180), project(lat, 180));
    }
    p.setPen(QPen(graticule(55), 1.0));   // Äquator, eine Spur kräftiger
    p.drawLine(project(0, -180), project(0, 180));

    if (m_showGrid) { paintGridOverlay(p, r); }

    p.setPen(QPen(QColor(Style::kBorderSubtle), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRect(r);

    const QColor accent(Style::kAccent);
    const QColor marked(Style::kAmberText);

    // Paths first, so the markers sit on top of them. Marked contacts
    // are drawn in a second pass so their lines are not buried under
    // hundreds of ordinary ones.
    //
    // Marked contacts keep their line even with paths switched off:
    // turning paths off is how you clear the clutter to look at a few
    // marked ones, so removing theirs too would defeat the switch.
    if (m_hasHome) {
        // Sample count against how many paths are coming, not per path.
        const int n = m_points.size();
        const int pathSteps = n > 200 ? 24 : (n > 60 ? 40 : 64);

        for (int pass = 0; pass < 2; ++pass) {
            const bool wantMarked = pass == 1;
            if (!wantMarked && !m_showPaths) { continue; }
            for (const MapPoint& pt : m_points) {
                if (pt.highlight != wantMarked) { continue; }
                const QVector<QPointF> samples =
                    greatCircleSamples(m_homeLat, m_homeLon,
                                       pt.lat, pt.lon, pathSteps);
                for (const QVector<QPointF>& run : splitAtAntimeridian(samples)) {
                    if (run.size() < 2) { continue; }
                    QPolygonF poly;
                    poly.reserve(run.size());
                    for (const QPointF& s : run) {
                        poly << project(s.y(), s.x());
                    }
                    p.setOpacity(wantMarked ? 0.95 : 0.35);
                    p.setPen(QPen(wantMarked ? marked : accent,
                                  wantMarked ? 1.6 : 1.0));
                    p.drawPolyline(poly);
                }
            }
        }
        p.setOpacity(1.0);
    }

    // Contacts. Marked ones last, so they sit on top.
    const bool labels = m_points.size() <= 40 && m_zoom >= 1.0;
    QFont f = p.font();
    f.setPixelSize(9);
    p.setFont(f);

    for (int pass = 0; pass < 2; ++pass) {
        const bool wantMarked = pass == 1;
        for (const MapPoint& pt : m_points) {
            if (pt.highlight != wantMarked) { continue; }
            const QPointF s = project(pt.lat, pt.lon);
            if (!r.contains(s)) { continue; }

            const QColor dot = wantMarked ? marked : accent;
            const double r1 = wantMarked ? 7.5 : 5.5;
            const double r2 = wantMarked ? 3.2 : 2.4;

            if (pt.approximate) {
                // A ring, not a dot: this is the middle of a country,
                // not a place anyone was.
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(dot, 1.4));
                p.drawEllipse(s, r2 + 1.6, r2 + 1.6);
            } else {
                p.setPen(Qt::NoPen);
                QColor glow = dot;
                glow.setAlpha(wantMarked ? 120 : 90);
                p.setBrush(glow);
                p.drawEllipse(s, r1, r1);
                p.setBrush(dot);
                p.drawEllipse(s, r2, r2);
            }

            // A marked contact is always named. Finding it among the
            // rest is the entire reason it was marked.
            if (!pt.label.isEmpty() && (wantMarked || labels)) {
                p.setBrush(Qt::NoBrush);
                p.setPen(wantMarked ? marked : QColor(Style::kTextPrimary));
                p.drawText(s + QPointF(8, 3), pt.label);
            }
        }
    }

    if (m_hasHome) {
        const QPointF s = project(m_homeLat, m_homeLon);
        const QColor ring(Style::kAccent);
        const QString caption = m_stationCall.isEmpty()
                                    ? QStringLiteral("HOME")
                                    : m_stationCall;

        QFont bf = f;
        bf.setBold(true);
        const QFontMetrics bfm(bf);

        double captionTop = s.y() + 6.0;

        if (!m_stationPhoto.isNull()) {
            // ── Das Foto ─────────────────────────────────────────────
            //
            // Auf einer Karte voller Kontaktmarker ist der eigene
            // Standort der einzige, den man sofort finden muss. Ein
            // Bild findet das Auge, bevor es liest.
            const double r = kStationMarkerPx / 2.0;
            const QRectF box(s.x() - r, s.y() - r,
                             kStationMarkerPx, kStationMarkerPx);

            // Ein weicher Schein darunter, damit der Kreis sich auch
            // über hellem Gelände (Wüste, Wolke) vom Grund löst.
            QColor halo = ring;
            halo.setAlpha(70);
            p.setPen(Qt::NoPen);
            p.setBrush(halo);
            p.drawEllipse(s, r + 3.5, r + 3.5);
            p.setBrush(Qt::NoBrush);

            p.drawPixmap(box.toRect(), m_stationPhoto);

            p.setPen(QPen(ring, 2.0));
            p.drawEllipse(s, r, r);
            captionTop = s.y() + r + 5.0;
        } else {
            // Ohne Bild der bisherige Punkt — nur trägt er jetzt das
            // Rufzeichen statt des Wortes „HOME", weil das Rufzeichen
            // die Information ist.
            const QColor home(Style::kGreenText);
            QColor glow = home;
            glow.setAlpha(90);
            p.setPen(Qt::NoPen);
            p.setBrush(glow);
            p.drawEllipse(s, 8, 8);
            p.setBrush(home);
            p.drawEllipse(s, 3.4, 3.4);
            p.setBrush(Qt::NoBrush);
            captionTop = s.y() + 10.0;
        }

        // ── Die Bildunterschrift ─────────────────────────────────────
        //
        // Gerahmt und hinterlegt, nicht frei auf der Karte: über einer
        // Satellitenaufnahme ist jede Textfarbe irgendwo unlesbar.
        const int tw = bfm.horizontalAdvance(caption);
        const QRectF plate(s.x() - tw / 2.0 - 5.0, captionTop,
                           tw + 10.0, bfm.height() + 3.0);
        QColor plateBg(Style::kAppBg);
        plateBg.setAlpha(215);
        p.setPen(QPen(ring, 1.0));
        p.setBrush(plateBg);
        p.drawRoundedRect(plate, 3.0, 3.0);
        p.setBrush(Qt::NoBrush);
        p.setPen(ring);
        p.setFont(bf);
        p.drawText(plate, Qt::AlignCenter, caption);
        p.setFont(f);
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

// ── Maidenhead overlay ──────────────────────────────────────────────

void FlatMapWidget::paintGridOverlay(QPainter& p, const QRectF& r)
{
    // Everything is clipped to the visible slice of the map, and the
    // loops below run only over that slice — at 180 x 180 squares an
    // unclipped label pass would be thirty-two thousand drawTexts.
    const QRectF vis = r.intersected(QRectF(rect()));
    if (vis.isEmpty()) { return; }
    p.save();
    p.setClipRect(vis);

    double latTop = 0, lonLeft = 0, latBot = 0, lonRight = 0;
    unproject(vis.topLeft() + QPointF(1, 1), latTop, lonLeft);
    unproject(vis.bottomRight() - QPointF(1, 1), latBot, lonRight);

    // Nur die Längengrad-Skala. Ein pxPerLat stand hier auch, wurde nie
    // benutzt und hat seit der Maidenhead-Arbeit eine Warnung erzeugt,
    // die in einem Volldurchlauf zwischen tausend Zeilen unterging —
    // sichtbar erst, als diese Datei allein gebaut hat. 2026-08-15.
    //
    // Bei einer gleichabständigen Zylinderprojektion ist es ohnehin
    // dasselbe Verhältnis: r.width()/360 und r.height()/180 sind gleich,
    // solange das Kartenbild sein Seitenverhältnis behält.
    const double pxPerLon = r.width() / 360.0;

    // Fields: 20° x 10°, AA at the south pole / date line.
    //
    // Deliberately faint (2026-08-10, operator feedback): the overlay
    // is a reference, not the subject — at the first alphas the grid
    // shouted over the map it was meant to annotate.
    const QColor gridCol(255, 210, 120, 42);
    const QColor gridColSoft(255, 210, 120, 22);
    p.setPen(QPen(gridCol, 0.8));
    for (int lon = -180; lon <= 180; lon += 20) {
        p.drawLine(project(90, lon), project(-90, lon));
    }
    for (int lat = -90; lat <= 90; lat += 10) {
        p.drawLine(project(lat, -180), project(lat, 180));
    }

    // Squares: 2° x 1°, once they have room to be squares on screen.
    const bool squares = pxPerLon * 2.0 >= 24.0;
    if (squares) {
        p.setPen(QPen(gridColSoft, 0.6));
        const int lonFrom = static_cast<int>(std::floor(lonLeft / 2.0)) * 2;
        const int lonTo   = static_cast<int>(std::ceil(lonRight / 2.0)) * 2;
        const int latFrom = static_cast<int>(std::floor(latBot));
        const int latTo   = static_cast<int>(std::ceil(latTop));
        for (int lon = lonFrom; lon <= lonTo; lon += 2) {
            p.drawLine(project(latTo, lon), project(latFrom, lon));
        }
        for (int lat = latFrom; lat <= latTo; ++lat) {
            p.drawLine(project(lat, lonFrom), project(lat, lonTo));
        }
    }

    // Labels, at whichever coarseness has room. Field letters first.
    QFont f = p.font();
    if (!squares && pxPerLon * 20.0 >= 34.0) {
        f.setPixelSize(std::min(11.0, pxPerLon * 20.0 * 0.20));
        p.setFont(f);
        p.setPen(locatorInk());
        const int lonFrom =
            static_cast<int>(std::floor((lonLeft + 180.0) / 20.0));
        const int lonTo =
            static_cast<int>(std::floor((lonRight + 180.0) / 20.0));
        const int latFrom =
            static_cast<int>(std::floor((latBot + 90.0) / 10.0));
        const int latTo =
            static_cast<int>(std::floor((latTop + 90.0) / 10.0));
        for (int fx = std::max(0, lonFrom); fx <= std::min(17, lonTo); ++fx) {
            for (int fy = std::max(0, latFrom); fy <= std::min(17, latTo);
                 ++fy) {
                const double lon = fx * 20.0 - 180.0 + 10.0;
                const double lat = fy * 10.0 - 90.0 + 5.0;
                const QString name = QString(QChar('A' + fx))
                                     + QChar('A' + fy);
                const QPointF c = project(lat, lon);
                p.drawText(QRectF(c.x() - 20, c.y() - 8, 40, 16),
                           Qt::AlignCenter, name);
            }
        }
    } else if (squares && pxPerLon * 2.0 >= 34.0) {
        f.setPixelSize(std::min(10.0, pxPerLon * 2.0 * 0.18));
        p.setFont(f);
        p.setPen(locatorInk());
        const int lonFrom = static_cast<int>(std::floor(lonLeft / 2.0)) * 2;
        const int lonTo   = static_cast<int>(std::ceil(lonRight / 2.0)) * 2;
        const int latFrom = static_cast<int>(std::floor(latBot));
        const int latTo   = static_cast<int>(std::ceil(latTop));
        for (int lon = lonFrom; lon < lonTo; lon += 2) {
            for (int lat = latFrom; lat < latTo; ++lat) {
                const QPointF c = project(lat + 0.5, lon + 1.0);
                p.drawText(QRectF(c.x() - 22, c.y() - 8, 44, 16),
                           Qt::AlignCenter,
                           gridSquare4(lat + 0.5, lon + 1.0));
            }
        }
    }

    // The square the operator clicked, picked out until the next click.
    if (m_clickedGrid.size() == 4) {
        const int fx = m_clickedGrid.at(0).toLatin1() - 'A';
        const int fy = m_clickedGrid.at(1).toLatin1() - 'A';
        const int sx = m_clickedGrid.at(2).toLatin1() - '0';
        const int sy = m_clickedGrid.at(3).toLatin1() - '0';
        const double lonW = fx * 20.0 - 180.0 + sx * 2.0;
        const double latS = fy * 10.0 - 90.0 + sy;
        const QPointF tl = project(latS + 1.0, lonW);
        const QPointF br = project(latS, lonW + 2.0);
        QColor fill(Style::kAmberText);
        fill.setAlpha(60);
        p.setPen(QPen(QColor(Style::kAmberText), 1.4));
        p.setBrush(fill);
        p.drawRect(QRectF(tl, br));
        p.setBrush(Qt::NoBrush);
        QFont bf = p.font();
        bf.setPixelSize(11);
        bf.setBold(true);
        p.setFont(bf);
        p.drawText(QRectF(tl, br), Qt::AlignCenter, m_clickedGrid);
    }

    p.restore();
}

// ── Mouse ───────────────────────────────────────────────────────────

void FlatMapWidget::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) { QWidget::mousePressEvent(e); return; }
    m_dragging = true;
    m_moved = false;
    m_dragFrom = e->position().toPoint();
    m_panFrom = m_pan;
    setCursor(Qt::ClosedHandCursor);
}

void FlatMapWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_dragging) { QWidget::mouseMoveEvent(e); return; }
    const QPoint d = e->position().toPoint() - m_dragFrom;
    if (!m_moved && d.manhattanLength() > 4) { m_moved = true; }
    if (!m_moved) { return; }
    m_pan = m_panFrom + QPointF(d.x(), d.y());
    update();
}

void FlatMapWidget::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);

        // No movement means this was a click, and a click means a
        // question: which station is that — or, failing a station,
        // which square is that.
        if (!m_moved) {
            const QPointF pos = e->position();
            const int hit = pointAt(pos);
            if (hit >= 0) {
                const MapPoint& pt = m_points.at(hit);
                emit pointClicked(pt.label, pt.lat, pt.lon);
            } else if (m_showGrid) {
                double lat = 0, lon = 0;
                if (unproject(pos, lat, lon)) {
                    m_clickedGrid = gridSquare4(lat, lon);
                    update();
                    emit gridClicked(m_clickedGrid);
                }
            }
        }
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void FlatMapWidget::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        resetView();
        return;
    }
    QWidget::mouseDoubleClickEvent(e);
}

void FlatMapWidget::centreOn(double lat, double lon)
{
    // mapRect() ist die Grundlage plus m_pan. Also erst ausrechnen, wo
    // der Ort OHNE Schwenk laege, dann den Schwenk so setzen, dass er in
    // der Fenstermitte sitzt.
    const double w = width();
    const double h = height();
    double mw = w;
    double mh = mw / 2.0;
    if (mh > h) { mh = h; mw = mh * 2.0; }
    mw *= m_zoom;
    mh *= m_zoom;

    const double baseLeft = (w - mw) / 2.0;
    const double baseTop  = (h - mh) / 2.0;
    const double x = baseLeft + (norm180(lon) + 180.0) / 360.0 * mw;
    const double y = baseTop  + (90.0 - std::clamp(lat, -90.0, 90.0)) / 180.0 * mh;

    m_pan = QPointF(w / 2.0 - x, h / 2.0 - y);
    update();
}

void FlatMapWidget::wheelEvent(QWheelEvent* e)
{
    const double steps = e->angleDelta().y() / 120.0;
    if (qFuzzyIsNull(steps)) { QWidget::wheelEvent(e); return; }

    const double before = m_zoom;
    m_zoom = std::clamp(m_zoom * std::pow(1.15, steps), 1.0, 12.0);

    // Am unteren Anschlag weiter herausdrehen heisst: der Betreiber will
    // MEHR sehen, als eine flache Weltkarte zeigen kann. Das kann nur die
    // Kugel; das Fenster schaltet um (2026-08-19).
    if (steps < 0.0 && qFuzzyCompare(before, 1.0) && qFuzzyCompare(m_zoom, 1.0)) {
        emit zoomedOutPastFloor();
        e->accept();
        return;
    }

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
