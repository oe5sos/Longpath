// no-port-check: Longpath-original, see AzimutalKarteWidget.h.
//
// =================================================================
// src/gui/widgets/AzimutalKarteWidget.cpp  (Longpath)
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Claude Code).
// =================================================================

#include "gui/widgets/AzimutalKarteWidget.h"

#include "gui/StyleConstants.h"
#include "gui/widgets/GlobeWidget.h"
#include "gui/widgets/WorldTexture.h"

#include <QFontMetricsF>
#include <QHash>
#include <QLocale>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kRad = kPi / 180.0;
constexpr double kHalbumfangKm = 20015.0;      // Gegenpunkt, Radius 1
constexpr double kRingeKm[] = {2500.0, 5000.0, 10000.0, 15000.0, 20000.0};

// Rechengroesse des Weltbilds: so fein wie die Scheibe in Geraete-
// punkten, aber hoechstens 1600 (2,6 Mio. Umkehrungen, ~0,2 s) und
// in Hunderterstufen, damit ein Ziehen am Fensterrand nicht jede
// Bildschirmzeile neu rechnet.
constexpr int kRechenMin = 400;
constexpr int kRechenMax = 1600;
// Die Nacht ist weich (Daemmerungssaum ~1300 km) -- eine feste, kleine
// Rechengroesse genuegt und haelt den Minutentakt billig.
constexpr int kNachtPx = 512;

// Unten eine Zeile fuer die Bandlegende, damit sie nicht in die
// 180-Grad-Beschriftung laeuft.
constexpr double kLegendePx = 18.0;

double kmAusBogen(double bogenRad) { return bogenRad * kHalbumfangKm / kPi; }

QColor farbe(const char* hex) { return QColor(QLatin1String(hex)); }

// Schrift ueber dichten Punkten: ein dunkler Hof darunter, sonst geht
// „JN67VV" im Haufen Europa unter (auf dem Entwurfsblatt ebenso).
void textMitHof(QPainter& p, const QPointF& at, const QString& t, const QColor& farbeText)
{
    QPainterPath pfad;
    pfad.addText(at, p.font(), t);
    QColor hof(Style::hexRole(Style::kPanelBg));
    hof.setAlphaF(0.8);
    p.save();
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(hof, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(pfad);
    p.restore();
    p.setPen(farbeText);
    p.drawText(at, t);
}
} // namespace

AzimutalKarteWidget::AzimutalKarteWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);

    // Minuetlich die Nacht. Verdeckt nur als veraltet merken -- gerechnet
    // wird beim naechsten Zeichnen.
    m_nachtTakt = new QTimer(this);
    m_nachtTakt->setInterval(60 * 1000);
    connect(m_nachtTakt, &QTimer::timeout, this, [this]() {
        m_nachtAlt = true;
        update();
    });
    m_nachtTakt->start();

    // Ein anderes Weltbild gewaehlt: neu abtasten (siehe die Notiz bei
    // WorldTexture::Notifier -- wer eine Ansicht baut, verbindet sich).
    connect(&WorldTexture::Notifier::instance(), &WorldTexture::Notifier::changed,
            this, [this]() {
        m_weltKlein = QImage();
        m_weltSchluessel = 0;
        m_hintergrundAlt = true;
        update();
    });
}

// ── Farben ──────────────────────────────────────────────────────────

QColor AzimutalKarteWidget::bandFarbe(const QString& band)
{
    const QString b = band.trimmed().toLower();
    struct Paar { const char* band; const char* farbe; };
    static const Paar kTafel[] = {
        {"160m", Style::kAzBand160m}, {"80m", Style::kAzBand80m},
        {"60m", Style::kAzBand60m},   {"40m", Style::kAzBand40m},
        {"30m", Style::kAzBand30m},   {"20m", Style::kAmberText},
        {"17m", Style::kAzBand17m},   {"15m", Style::kAzBand15m},
        {"12m", Style::kAzBand12m},   {"10m", Style::kAzBand10m},
        {"6m", Style::kAzBand6m},     {"4m", Style::kAzBand4m},
        {"2m", Style::kAzBand2m},     {"70cm", Style::kAzBand70cm},
        {"23cm", Style::kAzBand23cm},
    };
    for (const Paar& p : kTafel) {
        if (b == QLatin1String(p.band)) { return farbe(p.farbe); }
    }
    return farbe(Style::kTextScale);
}

// ── Geometrie ───────────────────────────────────────────────────────

QPointF AzimutalKarteWidget::projektion(double homeLat, double homeLon,
                                        double lat, double lon)
{
    const double az = GlobeWidget::initialBearing(homeLat, homeLon, lat, lon) * kRad;
    const double c  = GlobeWidget::angularDistance(homeLat, homeLon, lat, lon) * kRad;
    const double r  = std::clamp(c / kPi, 0.0, 1.0);
    return QPointF(r * std::sin(az), r * std::cos(az));   // y nach Norden
}

bool AzimutalKarteWidget::umkehrung(double homeLat, double homeLon, double x, double y,
                                    double& lat, double& lon)
{
    const double r = std::hypot(x, y);
    if (r > 1.0) { return false; }
    const double c  = r * kPi;
    const double az = std::atan2(x, y);
    const double la0 = homeLat * kRad, lo0 = homeLon * kRad;
    const double sla = std::clamp(std::sin(la0) * std::cos(c)
                                  + std::cos(la0) * std::sin(c) * std::cos(az), -1.0, 1.0);
    const double la = std::asin(sla);
    const double lo = lo0 + std::atan2(std::sin(az) * std::sin(c) * std::cos(la0),
                                       std::cos(c) - std::sin(la0) * sla);
    lat = la / kRad;
    lon = std::remainder(lo / kRad, 360.0);
    return true;
}

double AzimutalKarteWidget::radiusPx() const
{
    // Rand fuer die Gradzahlen aussen am Ring.
    return std::max(20.0, std::min(double(width()), height() - kLegendePx) * 0.5 - 34.0);
}

QPointF AzimutalKarteWidget::mitte() const
{
    return QPointF(width() * 0.5, (height() - kLegendePx) * 0.5);
}

QPointF AzimutalKarteWidget::bildpunktNormiert(const QPointF& q) const
{
    const double R = radiusPx();
    return mitte() + QPointF(q.x() * R, -q.y() * R);
}

QPointF AzimutalKarteWidget::bildpunkt(double lat, double lon) const
{
    return bildpunktNormiert(projektion(m_homeLat, m_homeLon, lat, lon));
}

int AzimutalKarteWidget::sollRechenPx() const
{
    const double geraet = 2.0 * radiusPx() * devicePixelRatioF();
    const int n = int(std::lround(geraet / 100.0)) * 100;
    return std::clamp(n, kRechenMin, kRechenMax);
}

// ── Eingaenge ───────────────────────────────────────────────────────

void AzimutalKarteWidget::setHome(double lat, double lon, const QString& locator)
{
    m_homeLocator = locator;
    if (m_hasHome && std::abs(lat - m_homeLat) < 1e-6 && std::abs(lon - m_homeLon) < 1e-6) {
        update();
        return;
    }
    m_hasHome = true;
    m_homeLat = lat;
    m_homeLon = lon;
    m_hintergrundAlt = m_nachtAlt = m_strahlenAlt = true;
    projiziereNeu();
    update();
}

void AzimutalKarteWidget::clearHome()
{
    m_hasHome = false;
    m_hintergrund = QImage();
    m_nacht = QImage();
    m_strahlen = QImage();
    m_hintergrundAlt = m_nachtAlt = m_strahlenAlt = true;
    m_hover = -1;
    update();
}

void AzimutalKarteWidget::setPunkte(const QVector<Punkt>& punkte)
{
    m_punkte = punkte;
    m_hover = -1;
    projiziereNeu();
    m_strahlenAlt = true;
    update();
}

void AzimutalKarteWidget::setRotorHeading(double deg)
{
    m_rotorDeg = deg < 0.0 ? -1.0 : std::fmod(deg, 360.0);
    update();
}

void AzimutalKarteWidget::setRotorBeamWidth(double deg)
{
    if (deg > 0.0) {
        m_strahlDeg = std::min(deg, 360.0);
        update();
    }
}

void AzimutalKarteWidget::setZiel(const QString& call, double lat, double lon)
{
    m_hatZiel = true;
    m_zielCall = call;
    m_zielLat = lat;
    m_zielLon = lon;
    update();
}

void AzimutalKarteWidget::clearZiel()
{
    m_hatZiel = false;
    m_zielCall.clear();
    update();
}

void AzimutalKarteWidget::setZeitForTest(const QDateTime& utc)
{
    m_zeitTest = utc;
    m_nachtAlt = true;
    update();
}

void AzimutalKarteWidget::projiziereNeu()
{
    m_proj.clear();
    m_reihenfolge.clear();
    if (!m_hasHome) { return; }
    m_proj.reserve(m_punkte.size());
    QHash<QString, int> zahl;
    for (const Punkt& q : m_punkte) {
        m_proj.append(projektion(m_homeLat, m_homeLon, q.lat, q.lon));
        ++zahl[q.band.trimmed().toLower()];
    }
    // Wie auf dem Blatt: das haeufigste Band zuerst, die seltenen
    // obenauf -- sonst verschwinden zwei 160-m-QSOs unter 3000 auf 20 m.
    m_reihenfolge.resize(m_punkte.size());
    for (int i = 0; i < m_punkte.size(); ++i) { m_reihenfolge[i] = i; }
    std::stable_sort(m_reihenfolge.begin(), m_reihenfolge.end(), [&](int a, int b) {
        return zahl.value(m_punkte[a].band.trimmed().toLower())
             > zahl.value(m_punkte[b].band.trimmed().toLower());
    });
}

// ── Ebenen ──────────────────────────────────────────────────────────

void AzimutalKarteWidget::sorgeFuerEbenen()
{
    if (!m_hasHome || width() <= 0 || height() <= 0) { return; }

    // Ein Weltbild, das inzwischen auf der Platte liegt (oder eines,
    // das ein anderer Weg geladen hat), soll ohne Neustart erscheinen.
    const QImage welt = WorldTexture::image();
    if (welt.cacheKey() != m_weltSchluessel) { m_hintergrundAlt = true; }

    const int soll = sollRechenPx();
    const bool zuGrob = m_rechenPx > 0
        && std::abs(soll - m_rechenPx) > m_rechenPx / 5;
    if (m_hintergrundAlt || zuGrob || m_hintergrund.isNull()) {
        baueHintergrund(soll);
        m_hintergrundAlt = false;
    }
    if (m_nachtAlt || m_nacht.isNull()) {
        baueNacht();
        m_nachtAlt = false;
    }
    const qreal dpr = devicePixelRatioF();
    if (m_strahlenAlt || m_strahlen.isNull()
        || m_strahlen.devicePixelRatio() != dpr
        || m_strahlen.size() != size() * dpr) {
        baueStrahlen();
        m_strahlenAlt = false;
    }
}

void AzimutalKarteWidget::baueHintergrund(int n)
{
    m_rechenPx = n;
    const QImage welt = WorldTexture::image();
    const bool anderesBild = welt.cacheKey() != m_weltSchluessel;
    m_weltSchluessel = welt.cacheKey();

    // Einmal verkleinern statt bei jedem Bildpunkt aus 5400 Punkten
    // Breite einen herauszugreifen. Anderthalbfache Rechenbreite reicht:
    // an der Mitte liegen dann gut ein Weltpunkt auf einem Scheibenpunkt,
    // und das Bild ist vorher weich gerechnet statt abgetastet.
    if (!welt.isNull()) {
        const int breite = std::min(welt.width(), n * 3 / 2);
        if (m_weltKlein.isNull() || anderesBild || m_weltKlein.width() != breite) {
            m_weltKlein = (breite == welt.width())
                ? welt
                : welt.scaled(breite, breite / 2, Qt::IgnoreAspectRatio,
                              Qt::SmoothTransformation);
            m_weltKlein = m_weltKlein.convertToFormat(QImage::Format_RGB32);
        }
    } else {
        m_weltKlein = QImage();
    }

    QImage bild(n, n, QImage::Format_ARGB32_Premultiplied);
    bild.fill(Qt::transparent);

    // Ohne Weltbild eine Scheibe in Tiefsee -- die Farbe, die auch eine
    // fehlende Kachel der Kugel bekommt. Gitter, Strahlen und Rotor
    // tragen die Karte dann allein.
    const QRgb ozean = farbe(Style::kMapOceanDeep).rgba();
    const int ww = m_weltKlein.width(), wh = m_weltKlein.height();

    for (int y = 0; y < n; ++y) {
        auto* zeile = reinterpret_cast<QRgb*>(bild.scanLine(y));
        const double fy = 1.0 - (y + 0.5) / n * 2.0;
        for (int x = 0; x < n; ++x) {
            const double fx = (x + 0.5) / n * 2.0 - 1.0;
            double lat = 0.0, lon = 0.0;
            if (!umkehrung(m_homeLat, m_homeLon, fx, fy, lat, lon)) { continue; }
            if (ww == 0) { zeile[x] = ozean; continue; }
            const int u = std::clamp(int((lon + 180.0) / 360.0 * ww), 0, ww - 1);
            const int v = std::clamp(int((90.0 - lat) / 180.0 * wh), 0, wh - 1);
            const QRgb w = reinterpret_cast<const QRgb*>(m_weltKlein.constScanLine(v))[u];
            // Wie auf dem Blatt: 45 % Farbe, 55 % Grau, auf 42 %
            // abgedunkelt, leicht warm -- damit die Strahlen tragen.
            const double r = qRed(w) / 255.0, g = qGreen(w) / 255.0, b = qBlue(w) / 255.0;
            const double grau = (r + g + b) / 3.0;
            const auto mix = [&](double k, double t) {
                return std::clamp(int((0.45 * k + 0.55 * grau) * 0.42 * t * 255.0 + 0.5), 0, 255);
            };
            zeile[x] = qRgb(mix(r, 1.0), mix(g, 0.96), mix(b, 0.88));
        }
    }
    m_hintergrund = bild;
}

void AzimutalKarteWidget::baueNacht()
{
    m_nacht = QImage();
    if (!m_hasHome) { return; }
    const int n = kNachtPx;
    const QDateTime jetzt = m_zeitTest.isValid() ? m_zeitTest
                                                 : QDateTime::currentDateTimeUtc();
    double sLat = 0.0, sLon = 0.0;
    GlobeWidget::subsolarPoint(jetzt, sLat, sLon);
    const double sd = std::sin(sLat * kRad), cd = std::cos(sLat * kRad);
    const QColor nachtFarbe = farbe(Style::kAzNacht);

    QImage bild(n, n, QImage::Format_ARGB32_Premultiplied);
    bild.fill(Qt::transparent);
    for (int y = 0; y < n; ++y) {
        auto* zeile = reinterpret_cast<QRgb*>(bild.scanLine(y));
        const double fy = 1.0 - (y + 0.5) / n * 2.0;
        for (int x = 0; x < n; ++x) {
            const double fx = (x + 0.5) / n * 2.0 - 1.0;
            double la = 0.0, lo = 0.0;
            if (!umkehrung(m_homeLat, m_homeLon, fx, fy, la, lo)) { continue; }
            // Sonnenhoehe ueber dem Horizont an diesem Ort.
            const double el = std::asin(std::clamp(
                std::sin(la * kRad) * sd + std::cos(la * kRad) * cd * std::cos((lo - sLon) * kRad),
                -1.0, 1.0)) / kRad;
            // Tag 0, Nacht 0,45; dazwischen die buergerliche bis
            // nautische Daemmerung (0 .. -12 Grad) als Saum.
            const double t = std::clamp(-el / 12.0, 0.0, 1.0);
            const int a = int(t * 0.45 * 255.0 + 0.5);
            if (a > 0) {
                zeile[x] = qPremultiply(qRgba(nachtFarbe.red(), nachtFarbe.green(),
                                              nachtFarbe.blue(), a));
            }
        }
    }
    m_nacht = bild;
}

void AzimutalKarteWidget::baueStrahlen()
{
    m_strahlen = QImage();
    if (!m_hasHome || width() <= 0 || height() <= 0) { return; }
    const qreal dpr = devicePixelRatioF();
    QImage bild(size() * dpr, QImage::Format_ARGB32_Premultiplied);
    bild.setDevicePixelRatio(dpr);
    bild.fill(Qt::transparent);
    QPainter p(&bild);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF c = mitte();

    // Erst die Strahlen, dann die Punkte; markierte zuletzt und kraeftig.
    // Die Deckung je Strahl ist so niedrig, dass einer allein kaum zu
    // sehen ist -- hell wird erst, wo viele uebereinander liegen.
    for (int pass = 0; pass < 2; ++pass) {
        for (int i : m_reihenfolge) {
            const Punkt& q = m_punkte.at(i);
            if ((pass == 1) != q.markiert) { continue; }
            QColor f = bandFarbe(q.band);
            f.setAlphaF(q.markiert ? 0.75 : 0.05);
            p.setPen(QPen(f, q.markiert ? 1.4 : 0.6, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(c, bildpunktNormiert(m_proj.at(i)));
        }
    }
    p.setPen(Qt::NoPen);
    for (int pass = 0; pass < 2; ++pass) {
        for (int i : m_reihenfolge) {
            const Punkt& q = m_punkte.at(i);
            if ((pass == 1) != q.markiert) { continue; }
            QColor f = bandFarbe(q.band);
            f.setAlphaF(q.markiert ? 1.0 : (q.genau ? 0.7 : 0.4));
            p.setBrush(f);
            const double r = q.markiert ? 3.2 : (q.genau ? 1.6 : 1.3);
            p.drawEllipse(bildpunktNormiert(m_proj.at(i)), r, r);
        }
    }
    p.end();
    m_strahlen = bild;
}

void AzimutalKarteWidget::resizeEvent(QResizeEvent*)
{
    m_strahlenAlt = true;
}

// ── Zeichnen ────────────────────────────────────────────────────────

void AzimutalKarteWidget::paintEvent(QPaintEvent*)
{
    sorgeFuerEbenen();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.fillRect(rect(), QColor(Style::hexRole(Style::kPanelBg)));

    const QPointF c = mitte();
    const double R = radiusPx();
    const QRectF scheibe(c.x() - R, c.y() - R, 2 * R, 2 * R);

    if (!m_hasHome) {
        p.setPen(farbe(Style::kTextScale));
        p.drawText(rect(), Qt::AlignCenter,
                   QStringLiteral("No home locator — set MY in the Rotor / Log panel"));
        return;
    }
    if (!m_hintergrund.isNull()) { p.drawImage(scheibe, m_hintergrund); }
    if (!m_nacht.isNull()) {
        // Die Nacht ist grob gerechnet und weich skaliert -- an der
        // Scheibe abschneiden, sonst blutet sie ueber den Rand.
        p.save();
        QPainterPath rand;
        rand.addEllipse(scheibe);
        p.setClipPath(rand);
        p.drawImage(scheibe, m_nacht);
        p.restore();
    }
    zeichneSkala(p);
    zeichneRotor(p);
    if (!m_strahlen.isNull()) { p.drawImage(QPointF(0, 0), m_strahlen); }
    zeichneZiel(p);

    // Der eigene Standort.
    const QColor amber = farbe(Style::kAmberText);
    p.setPen(QPen(QColor(Style::hexRole(Style::kPanelBg)), 1.5));
    p.setBrush(amber);
    p.drawEllipse(c, 4.5, 4.5);
    if (!m_homeLocator.isEmpty()) {
        QFont f = p.font();
        f.setPixelSize(11);
        f.setBold(true);
        p.setFont(f);
        textMitHof(p, c + QPointF(8.0, 15.0), m_homeLocator, amber);
    }

    zeichneHover(p);
    zeichneLegende(p);

    // Was das gewaehlte Weltbild an Vermerk verlangt, unten rechts.
    const QString vermerk = WorldTexture::requiredAttribution();
    if (!vermerk.isEmpty() && !m_hintergrund.isNull()) {
        QFont f = p.font();
        f.setPixelSize(9);
        f.setBold(false);
        p.setFont(f);
        p.setPen(farbe(Style::kTextScale));
        p.drawText(rect().adjusted(0, 0, -8, -6), Qt::AlignRight | Qt::AlignBottom, vermerk);
    }
}

void AzimutalKarteWidget::zeichneSkala(QPainter& p) const
{
    const QPointF c = mitte();
    const double R = radiusPx();
    const QColor gitter = farbe(Style::kAzRing);
    const QColor schrift = farbe(Style::kTextScale);
    QFont klein = p.font();
    klein.setPixelSize(9);
    klein.setBold(false);
    QFont grad = klein;
    grad.setPixelSize(11);
    const QFontMetricsF fmGrad(grad);
    const QLocale ort;

    p.setBrush(Qt::NoBrush);
    p.setFont(klein);
    for (double km : kRingeKm) {
        const double r = km / kHalbumfangKm * R;
        p.setPen(QPen(gitter, 0.8));
        p.drawEllipse(c, r, r);
        // Zu eng beieinander, um zu lesen: nur der Ring.
        if (r < 18.0) { continue; }
        p.setPen(schrift);
        p.drawText(QPointF(c.x() + 3.0, c.y() - r - 3.0),
                   QStringLiteral("%1 km").arg(ort.toString(int(km))));
    }
    p.setPen(QPen(gitter, 1.0));
    p.drawEllipse(c, R, R);

    for (int g = 0; g < 360; g += 10) {
        const double a = g * kRad;
        const QPointF dir(std::sin(a), -std::cos(a));
        const bool dreissig = g % 30 == 0;
        p.setPen(QPen(dreissig ? schrift : gitter, 1.0));
        p.drawLine(c + dir * R, c + dir * (R + (dreissig ? 9.0 : 4.0)));
        if (!dreissig) { continue; }
        const QString t = QStringLiteral("%1°").arg(g);
        const QPointF at = c + dir * (R + 20.0);
        p.setFont(grad);
        p.setPen(QColor(Style::kTextPrimary));
        p.drawText(QPointF(at.x() - fmGrad.horizontalAdvance(t) / 2.0,
                           at.y() + fmGrad.ascent() / 2.0 - 1.0), t);
    }
}

void AzimutalKarteWidget::zeichneRotor(QPainter& p) const
{
    if (m_rotorDeg < 0.0) { return; }
    const QPointF c = mitte();
    const double R = radiusPx();
    const QColor amber = farbe(Style::kAmberText);

    // Der Keil in Strahlbreite. QPainter: 0 Grad = 3 Uhr, gegen den
    // Uhrzeigersinn, in 1/16 Grad; Peilung: 0 = Norden, im Uhrzeigersinn.
    QColor keil = amber;
    keil.setAlphaF(0.13);
    p.setPen(Qt::NoPen);
    p.setBrush(keil);
    const double start = 90.0 - (m_rotorDeg + m_strahlDeg / 2.0);
    p.drawPie(QRectF(c.x() - R, c.y() - R, 2 * R, 2 * R),
              int(std::lround(start * 16.0)), int(std::lround(m_strahlDeg * 16.0)));

    // Die Nadel.
    const double a = m_rotorDeg * kRad;
    const QPointF dir(std::sin(a), -std::cos(a));
    p.setPen(QPen(amber, 1.6));
    p.drawLine(c, c + dir * R);

    // Beschriftung an der Nadel entlang, wie auf dem Blatt -- aufrecht
    // lesbar, also auf der linken Haelfte um 180 Grad gedreht.
    QFont f = p.font();
    f.setPixelSize(11);
    f.setBold(false);
    p.setFont(f);
    double winkel = std::atan2(dir.y(), dir.x()) / kRad;
    if (winkel > 90.0)  { winkel -= 180.0; }
    if (winkel < -90.0) { winkel += 180.0; }
    const QString t = QStringLiteral("Rotor %1°").arg(m_rotorDeg, 0, 'f', 0);
    const QFontMetricsF fm(f);
    p.save();
    p.translate(c + dir * (R * 0.62));
    p.rotate(winkel);
    p.setPen(amber);
    p.drawText(QPointF(-fm.horizontalAdvance(t) / 2.0, -5.0), t);
    p.restore();
}

void AzimutalKarteWidget::zeichneZiel(QPainter& p) const
{
    if (!m_hatZiel) { return; }
    const QPointF c = mitte();
    const double R = radiusPx();
    const double sp = GlobeWidget::initialBearing(m_homeLat, m_homeLon, m_zielLat, m_zielLon);
    const double km = kmAusBogen(GlobeWidget::angularDistance(m_homeLat, m_homeLon,
                                                              m_zielLat, m_zielLon) * kRad);
    const double lp = std::fmod(sp + 180.0, 360.0);
    const QPointF z = bildpunkt(m_zielLat, m_zielLon);
    const QColor hell = farbe(Style::kAzZiel);

    // Langer Weg: aus der Mitte in Richtung LP bis zum Rand (dem
    // Gegenpunkt), von dort auf der SP-Linie herein bis zur Station.
    QColor lang = hell;
    lang.setAlphaF(0.55);
    p.setPen(QPen(lang, 1.3, Qt::DashLine));
    const QPointF dirLp(std::sin(lp * kRad), -std::cos(lp * kRad));
    const QPointF dirSp(std::sin(sp * kRad), -std::cos(sp * kRad));
    p.drawLine(c, c + dirLp * R);
    p.drawLine(c + dirSp * R, z);

    // Kurzer Weg: kraeftig.
    p.setPen(QPen(hell, 2.2, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(c, z);
    p.setBrush(hell);
    p.setPen(QPen(QColor(Style::hexRole(Style::kPanelBg)), 1.2));
    p.drawEllipse(z, 5.0, 5.0);

    // Rufzeichen und die beiden Wege; im Widget gehalten, auch wenn die
    // Station am rechten Rand liegt.
    QFont fCall = p.font();
    fCall.setPixelSize(13);
    fCall.setBold(true);
    QFont fWeg = fCall;
    fWeg.setPixelSize(11);
    fWeg.setBold(false);
    const QString weg = QStringLiteral("SP %1° · %2 km   LP %3° · %4 km")
                            .arg(sp, 0, 'f', 0).arg(km, 0, 'f', 0)
                            .arg(lp, 0, 'f', 0).arg(2.0 * kHalbumfangKm - km, 0, 'f', 0);
    const double breite = std::max(QFontMetricsF(fCall).horizontalAdvance(m_zielCall),
                                   QFontMetricsF(fWeg).horizontalAdvance(weg));
    double x = z.x() + 9.0;
    if (x + breite > width() - 6.0) { x = z.x() - 9.0 - breite; }
    x = std::max(6.0, x);
    p.setFont(fCall);
    textMitHof(p, QPointF(x, z.y() - 8.0), m_zielCall, hell);
    p.setFont(fWeg);
    textMitHof(p, QPointF(x, z.y() + 8.0), weg, hell);
}

void AzimutalKarteWidget::zeichneHover(QPainter& p) const
{
    if (m_hover < 0 || m_hover >= m_punkte.size()) { return; }
    const Punkt& q = m_punkte.at(m_hover);
    const double az = GlobeWidget::initialBearing(m_homeLat, m_homeLon, q.lat, q.lon);
    const double km = kmAusBogen(GlobeWidget::angularDistance(m_homeLat, m_homeLon,
                                                              q.lat, q.lon) * kRad);
    const QString t = QStringLiteral("%1  ·  %2  ·  %3°  ·  %4 km%5")
                          .arg(q.call, q.band).arg(az, 0, 'f', 0).arg(km, 0, 'f', 0)
                          .arg(q.genau ? QString() : QStringLiteral("  (country centre)"));
    QFont f = p.font();
    f.setPixelSize(11);
    f.setBold(false);
    p.setFont(f);
    const QFontMetricsF fm(f);
    const QPointF pt = bildpunktNormiert(m_proj.at(m_hover));
    double x = pt.x() + 8.0;
    if (x + fm.horizontalAdvance(t) > width() - 6.0) { x = pt.x() - 8.0 - fm.horizontalAdvance(t); }
    textMitHof(p, QPointF(std::max(6.0, x), pt.y() - 6.0), t, QColor(Style::kTextPrimary));
}

void AzimutalKarteWidget::zeichneLegende(QPainter& p) const
{
    // Die haeufigsten Baender unten links, mit Zahl.
    QMap<QString, int> zahl;
    for (const Punkt& q : m_punkte) { ++zahl[q.band.trimmed().toLower()]; }
    QVector<QPair<int, QString>> sortiert;
    for (auto it = zahl.cbegin(); it != zahl.cend(); ++it) {
        if (!it.key().isEmpty()) { sortiert.append({it.value(), it.key()}); }
    }
    std::stable_sort(sortiert.begin(), sortiert.end(),
                     [](const auto& a, const auto& b) { return a.first > b.first; });
    QFont f = p.font();
    f.setPixelSize(11);
    f.setBold(false);
    p.setFont(f);
    const QFontMetricsF fm(f);
    double x = 10.0;
    const double y = height() - 8.0;
    for (int i = 0; i < sortiert.size() && i < 10; ++i) {
        const QString t = QStringLiteral("● %1 %2").arg(sortiert[i].second).arg(sortiert[i].first);
        const double w = fm.horizontalAdvance(t);
        if (x + w > width() - 10.0) { break; }
        p.setPen(bandFarbe(sortiert[i].second));
        p.drawText(QPointF(x, y), t);
        x += w + 12.0;
    }
}

// ── Maus ────────────────────────────────────────────────────────────

void AzimutalKarteWidget::mouseMoveEvent(QMouseEvent* e)
{
    int best = -1;
    double bestD = 8.0 * 8.0;
    for (int i = 0; i < m_proj.size(); ++i) {
        const QPointF d = bildpunktNormiert(m_proj.at(i)) - e->position();
        const double dd = d.x() * d.x() + d.y() * d.y();
        if (dd < bestD) { bestD = dd; best = i; }
    }
    if (best != m_hover) {
        m_hover = best;
        update();
    }
}

void AzimutalKarteWidget::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && m_hover >= 0 && m_hover < m_punkte.size()) {
        const Punkt& q = m_punkte.at(m_hover);
        emit pointClicked(q.call, q.lat, q.lon);
        return;
    }
    QWidget::mousePressEvent(e);
}

void AzimutalKarteWidget::leaveEvent(QEvent*)
{
    if (m_hover != -1) {
        m_hover = -1;
        update();
    }
}

} // namespace Longpath
