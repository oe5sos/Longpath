// no-port-check: Longpath-original widgets, see LogbuchGesamtbilderWidgets.h.
//
// src/gui/widgets/LogbuchGesamtbilderWidgets.cpp  (Longpath)

#include "gui/widgets/LogbuchGesamtbilderWidgets.h"

#include "core/LogbuchBilder.h"
#include "gui/StyleConstants.h"

#include <QFontMetricsF>
#include <QHelpEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QToolTip>

#include <algorithm>
#include <cmath>
#include <functional>

namespace Longpath {

namespace {

constexpr double kRand = 18.0;       // Seitenrand
constexpr double kKopfHoehe = 62.0;  // Titel + Unterzeile
constexpr double kFussHoehe = 36.0;  // eine bis zwei Zeilen Fuss

QColor farbe(const char* hex, int alpha = 255)
{
    QColor c{QString::fromLatin1(hex)};
    c.setAlpha(alpha);
    return c;
}
QFont schrift(int px, QFont::Weight w = QFont::Normal)
{
    QFont f;
    f.setPixelSize(px);
    f.setWeight(w);
    return f;
}
QFont mono(int px, QFont::Weight w = QFont::Normal) { return Style::monoFont(QFont(), px, w); }
QString zwei(int n) { return QStringLiteral("%1").arg(n, 2, 10, QLatin1Char('0')); }

// Der Grund des Blatts: #0c0c0e wie im Entwurf, mit dem Hausrahmen.
void grund(QPainter& p, const QRect& r)
{
    p.setPen(QPen(farbe(Style::kBorder), 1.0));
    p.setBrush(farbe(Style::kPanelBg));
    p.drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5),
                      Style::kGlassPanelRadius, Style::kGlassPanelRadius);
}
// Titel in Bernstein, darunter ein Satz, was das Bild zeigt.
void kopf(QPainter& p, double breite, const QString& titel, const QString& unter)
{
    const double w = breite - 2 * kRand;
    p.setFont(schrift(Style::kFontSub, QFont::DemiBold));
    p.setPen(farbe(Style::kAmberText));
    p.drawText(QRectF(kRand, 12, w, 24), Qt::AlignLeft | Qt::AlignVCenter, titel);
    p.setFont(schrift(Style::kFontSmall));
    p.setPen(farbe(Style::kTextTertiary));
    p.drawText(QRectF(kRand, 36, w, 18), Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetricsF(p.font()).elidedText(unter, Qt::ElideRight, w));
}
void fuss(QPainter& p, const QRect& r, const QString& text)
{
    p.setFont(schrift(Style::kFontCaption));
    p.setPen(farbe(Style::kTextTertiary));
    p.drawText(QRectF(kRand, r.height() - kFussHoehe, r.width() - 2 * kRand, kFussHoehe - 8),
               Qt::AlignLeft | Qt::AlignBottom | Qt::TextWordWrap, text);
}
void leer(QPainter& p, const QRect& r, const QString& text)
{
    p.setFont(schrift(Style::kFontSmall));
    p.setPen(farbe(Style::kTextTertiary));
    p.drawText(r, Qt::AlignCenter, text);
}

QString kontinentName(const QString& k)
{
    if (k == QLatin1String("EU")) { return QStringLiteral("Europe"); }
    if (k == QLatin1String("NA")) { return QStringLiteral("North America"); }
    if (k == QLatin1String("AS")) { return QStringLiteral("Asia"); }
    if (k == QLatin1String("SA")) { return QStringLiteral("South America"); }
    if (k == QLatin1String("AF")) { return QStringLiteral("Africa"); }
    if (k == QLatin1String("OC")) { return QStringLiteral("Oceania"); }
    return k;
}

// Die Bandfarben der Azimutalkarte -- eine Sprache fuer beide Bilder.
QColor bandFarbe(const QString& band)
{
    static const QHash<QString, const char*> t = {
        {QStringLiteral("160m"), Style::kAzBand160m}, {QStringLiteral("80m"), Style::kAzBand80m},
        {QStringLiteral("60m"), Style::kAzBand60m},   {QStringLiteral("40m"), Style::kAzBand40m},
        {QStringLiteral("30m"), Style::kAzBand30m},   {QStringLiteral("20m"), Style::kAmberText},
        {QStringLiteral("17m"), Style::kAzBand17m},   {QStringLiteral("15m"), Style::kAzBand15m},
        {QStringLiteral("12m"), Style::kAzBand12m},   {QStringLiteral("10m"), Style::kAzBand10m},
        {QStringLiteral("6m"), Style::kAzBand6m},     {QStringLiteral("4m"), Style::kAzBand4m},
        {QStringLiteral("2m"), Style::kAzBand2m},     {QStringLiteral("70cm"), Style::kAzBand70cm},
        {QStringLiteral("23cm"), Style::kAzBand23cm},
    };
    return farbe(t.value(band.toLower(), Style::kTextScale));
}

// Ein runder Schritt fuer etwa `ziel` Teilstriche bis `maxWert`.
double netterSchritt(double maxWert, int ziel)
{
    if (maxWert <= 0.0) { return 1.0; }
    const double roh = maxWert / ziel;
    const double mag = std::pow(10.0, std::floor(std::log10(roh)));
    for (double m : {1.0, 2.0, 5.0, 10.0}) {
        if (m * mag >= roh) { return std::max(1.0, m * mag); }
    }
    return std::max(1.0, 10.0 * mag);
}

// Eine Linie, die an fehlenden Werten (NaN) absetzt.
void linie(QPainter& p, const QVector<double>& werte, const std::function<QPointF(int, double)>& punkt)
{
    QPolygonF stueck;
    auto zeichne = [&]() {
        if (stueck.size() >= 2) { p.drawPolyline(stueck); }
        stueck.clear();
    };
    for (int i = 0; i < werte.size(); ++i) {
        if (std::isnan(werte[i])) { zeichne(); continue; }
        stueck << punkt(i, werte[i]);
    }
    zeichne();
}

} // namespace

// ── D  Sonnenzyklus ──────────────────────────────────────────────────

SonnenzyklusWidget::SonnenzyklusWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("sonnenzyklus"));
    setMouseTracking(true);
}

void SonnenzyklusWidget::setDaten(const LogbuchBilder::Sonnenzyklus& d, const QDateTime& fluxStand,
                                  bool holtGerade)
{
    m_d = d;
    m_stand = fluxStand;
    m_holt = holtGerade;
    update();
}

QString SonnenzyklusWidget::hinweis() const
{
    if (m_d.monate.isEmpty() || m_d.hatFlux()) { return {}; }
    if (m_holt) { return QStringLiteral("Fetching the solar flux from NOAA SWPC …"); }
    return QStringLiteral("No solar flux data: NOAA SWPC could not be reached and nothing is "
                          "cached yet. Showing the QSOs only.");
}

QString SonnenzyklusWidget::fusszeile() const
{
    if (m_d.monate.isEmpty()) { return {}; }
    QStringList t;
    t << QStringLiteral("%1 QSOs from %2 to %3").arg(m_d.gesamt)
             .arg(m_d.monate.first(), m_d.monate.last());
    if (m_d.besterMonat >= 0) {
        t << QStringLiteral("best month %1 with %2 QSOs").arg(m_d.monate.value(m_d.besterMonat))
                 .arg(m_d.maxQsos);
    }
    if (m_d.hatFlux()) {
        QString f = QStringLiteral("solar flux NOAA SWPC, monthly mean through %1; "
                                   "thick line smoothed over five months").arg(m_d.fluxBis());
        if (m_stand.isValid() && m_stand.secsTo(QDateTime::currentDateTimeUtc()) > 2 * 86400) {
            f += QStringLiteral(" (offline, cached %1)").arg(m_stand.toUTC().toString(QStringLiteral("yyyy-MM-dd")));
        }
        t << f;
    }
    return t.join(QStringLiteral(" · "));
}

QRectF SonnenzyklusWidget::plotFlaeche() const
{
    const double links = kRand + 48.0, rechts = kRand + 50.0;
    return QRectF(links, kKopfHoehe + 22.0, width() - links - rechts,
                  height() - kKopfHoehe - 22.0 - kFussHoehe - 26.0);
}

int SonnenzyklusWidget::monatBei(const QPointF& p) const
{
    const QRectF a = plotFlaeche();
    if (m_d.monate.isEmpty() || !a.adjusted(0, -10, 0, 10).contains(p)) { return -1; }
    const int i = int((p.x() - a.left()) / (a.width() / m_d.monate.size()));
    return i >= 0 && i < m_d.monate.size() ? i : -1;
}

bool SonnenzyklusWidget::event(QEvent* e)
{
    if (e->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(e);
        const int i = monatBei(he->pos());
        if (i < 0) { QToolTip::hideText(); e->ignore(); return true; }
        QString t = QStringLiteral("%1: %2 QSOs").arg(m_d.monate[i]).arg(m_d.qsos.value(i));
        const double f = m_d.flux.value(i, std::nan(""));
        if (!std::isnan(f)) {
            t += QStringLiteral(" · F10.7 %1 sfu (5-month %2)").arg(f, 0, 'f', 1)
                     .arg(m_d.glatt.value(i), 0, 'f', 1);
        }
        QToolTip::showText(he->globalPos(), t, this);
        return true;
    }
    return QWidget::event(e);
}

void SonnenzyklusWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    grund(p, rect());
    kopf(p, width(), QStringLiteral("Your log against the solar cycle"),
         QStringLiteral("QSOs per month (bars) against the 10.7 cm solar flux (line). "
                        "Shows how propagation shaped the log."));
    if (m_d.monate.isEmpty()) {
        leer(p, rect(), QStringLiteral("No contacts in this view."));
        return;
    }
    const QRectF a = plotFlaeche();
    if (a.width() < 60 || a.height() < 60) { return; }
    const int n = int(m_d.monate.size());
    const double slot = a.width() / n;

    // Linke Achse: QSOs je Monat, mit Gitter.
    const double schritt = netterSchritt(m_d.maxQsos, 6);
    const double yMax = std::max(schritt, std::ceil(m_d.maxQsos / schritt) * schritt);
    auto yQ = [&](double v) { return a.bottom() - v / yMax * a.height(); };
    p.setFont(mono(Style::kFontCaption));
    for (double v = 0.0; v <= yMax + 0.5; v += schritt) {
        const double y = yQ(v);
        p.setPen(QPen(farbe(Style::kBorder), 1.0));
        p.drawLine(QPointF(a.left(), y), QPointF(a.right(), y));
        p.setPen(farbe(Style::kTextScale));
        p.drawText(QRectF(a.left() - 52, y - 7, 44, 14), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(qRound(v)));
    }
    // Balken
    p.setPen(Qt::NoPen);
    p.setBrush(farbe(Style::kAmberText, 191));
    const double bw = std::max(1.0, slot * 0.85);
    for (int i = 0; i < n; ++i) {
        const int q = m_d.qsos[i];
        if (q <= 0) { continue; }
        const double y = yQ(q);
        p.drawRect(QRectF(a.left() + i * slot + (slot - bw) / 2.0, y, bw, a.bottom() - y));
    }

    // Rechte Achse und Linien: der Flux.
    if (m_d.hatFlux()) {
        double fMin = 1e9, fMax = 0.0;
        for (double f : m_d.flux) {
            if (!std::isnan(f)) { fMin = std::min(fMin, f); fMax = std::max(fMax, f); }
        }
        const double unten = std::min(50.0, std::floor(fMin / 25.0) * 25.0);
        const double oben = std::max(260.0, fMax + 10.0);
        auto yF = [&](double v) { return a.bottom() - (v - unten) / (oben - unten) * a.height(); };
        const double fSchritt = oben - unten <= 320.0 ? 25.0 : 50.0;
        p.setFont(mono(Style::kFontCaption));
        for (double v = unten; v <= oben; v += fSchritt) {
            const double y = yF(v);
            p.setPen(QPen(farbe(Style::kTextScale), 1.0));
            p.drawLine(QPointF(a.right() + 4, y), QPointF(a.right() + 8, y));
            p.drawText(QRectF(a.right() + 11, y - 7, 40, 14), Qt::AlignLeft | Qt::AlignVCenter,
                       QString::number(qRound(v)));
        }
        auto punkt = [&](int i, double v) { return QPointF(a.left() + (i + 0.5) * slot, yF(v)); };
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(farbe(Style::kTextPrimary, 90), 1.0));
        linie(p, m_d.flux, punkt);
        p.setPen(QPen(farbe(Style::kTextPrimary, 230), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        linie(p, m_d.glatt, punkt);
        // Achsentitel rechts
        p.save();
        p.translate(width() - kRand + 2, a.center().y());
        p.rotate(-90);
        p.setFont(schrift(Style::kFontCaption));
        p.setPen(farbe(Style::kTextTertiary));
        p.drawText(QRectF(-80, -14, 160, 14), Qt::AlignCenter, QStringLiteral("Solar flux (sfu)"));
        p.restore();
    }
    // Achsentitel links
    p.save();
    p.translate(kRand - 2, a.center().y());
    p.rotate(-90);
    p.setFont(schrift(Style::kFontCaption));
    p.setPen(farbe(Style::kTextTertiary));
    p.drawText(QRectF(-80, 0, 160, 14), Qt::AlignCenter, QStringLiteral("QSOs per month"));
    p.restore();

    // Zeitachse: Jahre am Januar; bei langen Logs jedes zweite/fuenfte.
    const int jahrSchritt = n <= 12 * 14 ? 1 : n <= 12 * 30 ? 2 : 5;
    bool jahrGezeigt = false;
    p.setFont(mono(Style::kFontCaption));
    for (int i = 0; i < n; ++i) {
        const QString& m = m_d.monate[i];
        if (!m.endsWith(QLatin1String("-01")) || m.left(4).toInt() % jahrSchritt != 0) { continue; }
        const double x = a.left() + (i + 0.5) * slot;
        p.setPen(QPen(farbe(Style::kTextScale), 1.0));
        p.drawLine(QPointF(x, a.bottom() + 2), QPointF(x, a.bottom() + 6));
        p.drawText(QRectF(x - 30, a.bottom() + 7, 60, 14), Qt::AlignCenter, m.left(4));
        jahrGezeigt = true;
    }
    if (!jahrGezeigt) {
        p.setPen(farbe(Style::kTextScale));
        p.drawText(QRectF(a.left(), a.bottom() + 7, 80, 14), Qt::AlignLeft | Qt::AlignVCenter,
                   m_d.monate.first());
        p.drawText(QRectF(a.right() - 80, a.bottom() + 7, 80, 14), Qt::AlignRight | Qt::AlignVCenter,
                   m_d.monate.last());
    }

    // Ohne Flux: sagen, warum die Linie fehlt.
    const QString h = hinweis();
    if (!h.isEmpty()) {
        p.setFont(schrift(Style::kFontSmall));
        const QRectF kasten(a.left() + 12, a.top() + 4, std::min(a.width() - 24, 620.0), 36);
        p.setPen(QPen(farbe(Style::kBorder), 1.0));
        p.setBrush(farbe(Style::kPanelBg, 230));
        p.drawRoundedRect(kasten, 5, 5);
        p.setPen(farbe(Style::kTextSecondary));
        p.drawText(kasten.adjusted(10, 2, -10, -2), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, h);
    }
    fuss(p, rect(), fusszeile());
}

// ── E  Kontinent-Uhr ─────────────────────────────────────────────────

namespace {
// Die Ringe in Teilen des Radius, wie im Entwurf: innen 0,30, je Ring
// 0,115; die Zelle fuellt 0,08..0,90 des Rings.
constexpr double kRing0 = 0.30;
constexpr double kRingBreite = 0.115;
double ringInnen(int i, double r) { return r * (kRing0 + i * kRingBreite + kRingBreite * 0.08); }
double ringAussen(int i, double r) { return r * (kRing0 + i * kRingBreite + kRingBreite * 0.90); }
} // namespace

KontinentUhrWidget::KontinentUhrWidget(QWidget* parent)
    : QWidget(parent)
    , m_takt(new QTimer(this))
{
    setObjectName(QStringLiteral("kontinentUhr"));
    setMouseTracking(true);
    m_takt->setSingleShot(true);
    connect(m_takt, &QTimer::timeout, this, [this]() {
        update();
        stelleTakt();
    });
}

void KontinentUhrWidget::setDaten(const LogbuchBilder::KontinentUhr& d)
{
    m_d = d;
    update();
}

QDateTime KontinentUhrWidget::jetzt() const
{
    return m_jetztTest.isValid() ? m_jetztTest.toUTC() : QDateTime::currentDateTimeUtc();
}

// Zur naechsten vollen Minute, dann wieder -- der Zeiger springt mit
// der Uhr, nicht eine Minute versetzt zum Oeffnen des Bilds.
void KontinentUhrWidget::stelleTakt()
{
    if (!isVisible()) { return; }
    const qint64 ms = QDateTime::currentMSecsSinceEpoch();
    m_takt->start(int(60000 - ms % 60000) + 50);
}

void KontinentUhrWidget::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    stelleTakt();
}

void KontinentUhrWidget::hideEvent(QHideEvent* e)
{
    QWidget::hideEvent(e);
    m_takt->stop();
}

void KontinentUhrWidget::geometrie(QPointF* mitte, double* radius) const
{
    const double seite = std::min(width() - 2 * kRand, height() - kKopfHoehe - kFussHoehe - 8.0);
    // Die Stundenzahlen stehen bei 1,04 R, mit etwas Luft nach aussen.
    *radius = std::max(0.0, (seite / 2.0 - 12.0) / 1.04);
    *mitte = QPointF(width() / 2.0, kKopfHoehe + 4.0 + seite / 2.0);
}

bool KontinentUhrWidget::zelleBei(const QPointF& p, int* ring, int* stunde) const
{
    QPointF m;
    double r = 0.0;
    geometrie(&m, &r);
    const double dx = p.x() - m.x(), dy = p.y() - m.y();
    const double d = std::hypot(dx, dy);
    const int ringe = int(LogbuchBilder::KontinentUhr::kontinente().size());
    for (int i = 0; i < ringe; ++i) {
        if (d < ringInnen(i, r) || d > ringAussen(i, r)) { continue; }
        double w = std::atan2(dx, -dy) * 180.0 / M_PI;     // im Uhrzeigersinn ab 12 Uhr
        if (w < 0) { w += 360.0; }
        if (ring) { *ring = i; }
        if (stunde) { *stunde = std::clamp(int(w / 15.0), 0, 23); }
        return true;
    }
    return false;
}

QString KontinentUhrWidget::zellenText(int ring, int stunde) const
{
    const QString k = LogbuchBilder::KontinentUhr::kontinente().value(ring);
    const int n = m_d.anzahl.value(k).value(stunde);
    QString t = QStringLiteral("%1 · %2–%3 UTC: %4 QSOs").arg(kontinentName(k), zwei(stunde),
                                                            zwei((stunde + 1) % 24)).arg(n);
    const int b = m_d.besteStunde(k);
    if (b >= 0) {
        t += QStringLiteral(" · best hour %1–%2 (%3)").arg(zwei(b), zwei((b + 1) % 24)).arg(m_d.maxWert(k));
    }
    return t;
}

bool KontinentUhrWidget::event(QEvent* e)
{
    if (e->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(e);
        int ring = -1, stunde = -1;
        if (m_d.gesamt == 0 || !zelleBei(he->pos(), &ring, &stunde)) {
            QToolTip::hideText();
            e->ignore();
            return true;
        }
        QToolTip::showText(he->globalPos(), zellenText(ring, stunde), this);
        return true;
    }
    return QWidget::event(e);
}

void KontinentUhrWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    grund(p, rect());
    kopf(p, width(), QStringLiteral("Continent clock"),
         QStringLiteral("When you work which continent — UTC. One ring per continent; bright are "
                        "its best hours. The hand points at now."));
    if (m_d.gesamt == 0) {
        leer(p, rect(), QStringLiteral("No contacts with a known continent in this view."));
        return;
    }
    QPointF m;
    double R = 0.0;
    geometrie(&m, &R);
    if (R < 60.0) { return; }
    const QStringList& kont = LogbuchBilder::KontinentUhr::kontinente();

    // Zellen: 24 je Ring, im Uhrzeigersinn ab 00 UTC oben.
    for (int i = 0; i < kont.size(); ++i) {
        const double ri = ringInnen(i, R), ra = ringAussen(i, R);
        const QRectF innen(m.x() - ri, m.y() - ri, 2 * ri, 2 * ri);
        const QRectF aussen(m.x() - ra, m.y() - ra, 2 * ra, 2 * ra);
        for (int h = 0; h < 24; ++h) {
            const double start = 90.0 - h * 15.0 - 0.6;
            const double lauf = -(15.0 - 1.2);
            QPainterPath zelle;
            zelle.arcMoveTo(aussen, start);
            zelle.arcTo(aussen, start, lauf);
            zelle.arcTo(innen, start + lauf, -lauf);
            zelle.closeSubpath();
            const int n = m_d.anzahl.value(kont[i]).value(h);
            QColor c;
            if (n > 0) {
                c = farbe(Style::kAmberText);
                c.setAlphaF(0.08 + 0.92 * std::pow(m_d.anteil(kont[i], h), 0.7));
            } else {
                c = farbe(Style::kGlassPanelTop);
            }
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawPath(zelle);
        }
    }
    // Ringnamen mit Summe, links der 12-Uhr-Linie -- mit dunklem Saum,
    // denn sie stehen auf den Zellen um 23 UTC.
    p.setFont(schrift(Style::kFontSmall));
    const QFontMetricsF fm(p.font());
    for (int i = 0; i < kont.size(); ++i) {
        const double y = m.y() - R * (kRing0 + i * kRingBreite + kRingBreite * 0.49);
        const QString t = QStringLiteral("%1  %2").arg(kontinentName(kont[i])).arg(m_d.summe(kont[i]));
        const QRectF r(m.x() - R * 0.02 - fm.horizontalAdvance(t) - 4, y - 8, fm.horizontalAdvance(t) + 4, 16);
        p.setPen(farbe(Style::kPanelBg, 220));
        for (const QPointF& v : {QPointF(-1, 0), QPointF(1, 0), QPointF(0, -1), QPointF(0, 1)}) {
            p.drawText(r.translated(v), Qt::AlignRight | Qt::AlignVCenter, t);
        }
        p.setPen(farbe(Style::kTextPrimary));
        p.drawText(r, Qt::AlignRight | Qt::AlignVCenter, t);
    }
    // Stundenzahlen
    const double rz = R * 1.04;
    for (int h = 0; h < 24; ++h) {
        const double w = (90.0 - h * 15.0) * M_PI / 180.0;
        const QPointF q(m.x() + std::cos(w) * rz, m.y() - std::sin(w) * rz);
        const bool haupt = h % 6 == 0;
        p.setFont(mono(haupt ? Style::kFontSmall : Style::kFontCaption));
        p.setPen(haupt ? farbe(Style::kTextPrimary) : farbe(Style::kTextScale));
        p.drawText(QRectF(q.x() - 14, q.y() - 8, 28, 16), Qt::AlignCenter, zwei(h));
    }
    // Zeiger auf jetzt
    const QDateTime t = jetzt();
    const double hh = t.time().hour() + t.time().minute() / 60.0;
    const double w = (90.0 - hh * 15.0) * M_PI / 180.0;
    const double laenge = R * (kRing0 + kont.size() * kRingBreite);
    p.setPen(QPen(farbe(Style::kAmberText), 2.4, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(m, QPointF(m.x() + std::cos(w) * laenge, m.y() - std::sin(w) * laenge));
    p.setPen(Qt::NoPen);
    p.setBrush(farbe(Style::kAmberText));
    p.drawEllipse(m, R * 0.03, R * 0.03);
    p.setFont(schrift(Style::kFontBody, QFont::DemiBold));
    p.setPen(farbe(Style::kAmberText));
    p.drawText(QRectF(m.x() - 100, m.y() + R * 0.10, 200, 20), Qt::AlignCenter,
               QStringLiteral("now %1 UTC").arg(t.toString(QStringLiteral("HH:mm"))));

    QString f = QStringLiteral("%1 QSOs · continent from cty.dat").arg(m_d.gesamt);
    if (m_d.ohneZuordnung > 0) { f += QStringLiteral(" · %1 without a match").arg(m_d.ohneZuordnung); }
    f += QStringLiteral(" · brightness per ring relative to its best hour");
    fuss(p, rect(), f);
}

// ── F  DXCC-Wand ─────────────────────────────────────────────────────

namespace {
constexpr double kBlockKopf = 36.0;       // Kontinentname + Zaehlung
constexpr double kBlockLuecke = 18.0;
constexpr double kZeilenLuecke = 16.0;
constexpr double kZeilenVerh = 0.80;      // Zeilenschritt / Spaltenschritt
constexpr double kMaxTeilung = 62.0;

// "VP8/H" -> "VP8/h": cty.dat schreibt den Zusatz klein, der Parser gross.
QString anzeigePraefix(const QString& p)
{
    const int s = int(p.indexOf(QLatin1Char('/')));
    return s < 0 ? p : p.left(s + 1) + p.mid(s + 1).toLower();
}

int bandRang(const QString& b)
{
    const int i = int(LogbuchBilder::baender().indexOf(b));
    return i < 0 ? 100 : i;
}
} // namespace

DxccWandWidget::DxccWandWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("dxccWand"));
    setMouseTracking(true);
}

void DxccWandWidget::setDaten(const LogbuchBilder::DxccWand& w)
{
    m_w = w;
    update();
}

QString DxccWandWidget::titel() const
{
    return QStringLiteral("DXCC wall — %1 of %2 worked").arg(m_w.gearbeitet).arg(m_w.gesamt());
}

QString DxccWandWidget::fusszeile() const
{
    QStringList t;
    t << QStringLiteral("%1 QSOs").arg(m_w.qsos + m_w.ohneZuordnung);
    t << QStringLiteral("entity from the call's prefix in cty.dat (WAE-only areas count for "
                        "their DXCC entity)");
    if (m_w.bestaetigt == 0) {
        t << QStringLiteral("worked, not confirmed — no confirmations (LoTW, QSL) in the log yet");
    } else {
        t << QStringLiteral("%1 confirmed (green edge)").arg(m_w.bestaetigt);
    }
    if (m_w.ohneZuordnung > 0) { t << QStringLiteral("%1 without an entity").arg(m_w.ohneZuordnung); }
    return t.join(QStringLiteral(" · "));
}

DxccWandWidget::Anordnung DxccWandWidget::anordnen() const
{
    Anordnung a;
    a.kacheln = QVector<QRectF>(m_w.kacheln.size());
    const QStringList& kont = LogbuchBilder::DxccWand::kontinente();
    QVector<QVector<int>> bloecke(kont.size());
    for (int i = 0; i < m_w.kacheln.size(); ++i) {
        const int b = int(kont.indexOf(m_w.kacheln[i].kontinent));
        if (b >= 0) { bloecke[b] << i; }
    }
    const QRectF flaeche(kRand, kKopfHoehe + 10.0, width() - 2 * kRand,
                         height() - kKopfHoehe - 10.0 - kFussHoehe - 6.0);
    if (flaeche.width() < 100 || flaeche.height() < 80) { return a; }

    // Wie ein Periodensystem: die sechs Bloecke nebeneinander, oder in
    // zwei oder drei Baendern -- je nachdem, was die groessten Kacheln
    // ergibt. R ist die Zeilenzahl, auf die ein Block hoechstens kommt.
    const QVector<QVector<QVector<int>>> gruppen = {
        {{0, 1, 2, 3, 4, 5}}, {{0, 1, 2}, {3, 4, 5}}, {{0, 1}, {2, 3}, {4, 5}}};
    QVector<QVector<int>> besteGruppe;
    int besteR = 0;
    for (const auto& g : gruppen) {
        for (int R = 3; R <= 45; ++R) {
            double tw = 1e9;
            int zeilenGesamt = 0;
            for (const QVector<int>& band : g) {
                int spalten = 0, zeilen = 0, bloeckeImBand = 0;
                for (int b : band) {
                    const int n = int(bloecke[b].size());
                    if (n == 0) { continue; }
                    const int c = (n + R - 1) / R;
                    spalten += c;
                    zeilen = std::max(zeilen, (n + c - 1) / c);
                    ++bloeckeImBand;
                }
                if (spalten == 0) { continue; }
                tw = std::min(tw, (flaeche.width() - (bloeckeImBand - 1) * kBlockLuecke) / spalten);
                zeilenGesamt += zeilen;
            }
            if (zeilenGesamt == 0) { continue; }
            const double th = (flaeche.height() - g.size() * kBlockKopf - (g.size() - 1) * kZeilenLuecke)
                            / (zeilenGesamt * kZeilenVerh);
            const double t = std::min({tw, th, kMaxTeilung});
            if (t > a.teilung + 0.01) { a.teilung = t; besteGruppe = g; besteR = R; }
        }
    }
    if (a.teilung < 8.0) { a.teilung = 0.0; return a; }

    // Platzieren: Baender untereinander, gemeinsamer linker Rand, das
    // Ganze mittig in der Flaeche.
    const double t = a.teilung, tz = t * kZeilenVerh;
    double breiteste = 0.0, gesamtHoehe = 0.0;
    QVector<QVector<int>> spaltenJe(besteGruppe.size());
    QVector<int> zeilenJe(besteGruppe.size(), 0);
    for (int z = 0; z < besteGruppe.size(); ++z) {
        double b = 0.0;
        int bl = 0;
        for (int blk : besteGruppe[z]) {
            const int n = int(bloecke[blk].size());
            const int c = n == 0 ? 0 : (n + besteR - 1) / besteR;
            spaltenJe[z] << c;
            if (c == 0) { continue; }
            b += c * t;
            ++bl;
            zeilenJe[z] = std::max(zeilenJe[z], (n + c - 1) / c);
        }
        breiteste = std::max(breiteste, b + std::max(0, bl - 1) * kBlockLuecke);
        gesamtHoehe += kBlockKopf + zeilenJe[z] * tz;
    }
    gesamtHoehe += (besteGruppe.size() - 1) * kZeilenLuecke;
    const double x0 = flaeche.left() + (flaeche.width() - breiteste) / 2.0;
    double y = flaeche.top() + std::max(0.0, (flaeche.height() - gesamtHoehe) / 2.0);
    for (int z = 0; z < besteGruppe.size(); ++z) {
        double x = x0;
        for (int k = 0; k < besteGruppe[z].size(); ++k) {
            const int blk = besteGruppe[z][k];
            const int c = spaltenJe[z][k];
            if (c == 0) { continue; }
            a.koepfe << qMakePair(kont[blk], QPointF(x, y));
            const QVector<int>& liste = bloecke[blk];
            for (int j = 0; j < liste.size(); ++j) {
                const int spalte = j % c, zeile = j / c;     // zeilenweise wie im Entwurf
                a.kacheln[liste[j]] = QRectF(x + spalte * t, y + kBlockKopf + zeile * tz,
                                             t * 0.92, tz * 0.90);
            }
            x += c * t + kBlockLuecke;
        }
        y += kBlockKopf + zeilenJe[z] * tz + kZeilenLuecke;
    }
    return a;
}

QVector<QRectF> DxccWandWidget::kachelRechtecke() const
{
    return anordnen().kacheln;
}

int DxccWandWidget::kachelBei(const QPointF& p) const
{
    const QVector<QRectF> r = anordnen().kacheln;
    for (int i = 0; i < r.size(); ++i) {
        if (r[i].contains(p)) { return i; }
    }
    return -1;
}

QString DxccWandWidget::kachelText(int index) const
{
    if (index < 0 || index >= m_w.kacheln.size()) { return {}; }
    const LogbuchBilder::DxccKachel& k = m_w.kacheln[index];
    QString t = QStringLiteral("%1 (%2) · %3").arg(k.name, anzeigePraefix(k.praefix), kontinentName(k.kontinent));
    if (!k.gearbeitet()) { return t + QStringLiteral("\nnot worked yet"); }
    QStringList b(k.baender.cbegin(), k.baender.cend());
    std::sort(b.begin(), b.end(), [](const QString& x, const QString& y) {
        const int rx = bandRang(x), ry = bandRang(y);
        return rx != ry ? rx < ry : x < y;
    });
    t += QStringLiteral("\n%1 QSO%2 · %3").arg(k.qsos).arg(k.qsos == 1 ? QString() : QStringLiteral("s"))
             .arg(b.join(QStringLiteral(" · ")));
    if (k.bestaetigt) { t += QStringLiteral(" · confirmed"); }
    return t;
}

bool DxccWandWidget::event(QEvent* e)
{
    if (e->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(e);
        const int i = kachelBei(he->pos());
        if (i < 0) { QToolTip::hideText(); e->ignore(); return true; }
        QToolTip::showText(he->globalPos(), kachelText(i), this);
        return true;
    }
    return QWidget::event(e);
}

void DxccWandWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    grund(p, rect());
    kopf(p, width(), titel(),
         QStringLiteral("One tile per entity, grouped by continent. Filled = worked, brighter = more "
                        "bands. Dots: 160 · 80 · 40 · 30 · 20 · 17 · 15 · 12 · 10 · 6 m."));
    if (m_w.kacheln.isEmpty()) {
        leer(p, rect(), QStringLiteral("No country file (cty.dat) loaded."));
        return;
    }
    const Anordnung a = anordnen();
    if (a.teilung <= 0.0) { return; }
    const double t = a.teilung;

    // Bloecke: Name in Bernstein, darunter gearbeitet / alle.
    for (const auto& [k, ort] : a.koepfe) {
        int alle = 0, gearb = 0;
        m_w.zaehle(k, &alle, &gearb);
        p.setFont(schrift(Style::kFontSmall, QFont::DemiBold));
        p.setPen(farbe(Style::kAmberText));
        p.drawText(QRectF(ort.x(), ort.y(), 200, 16), Qt::AlignLeft | Qt::AlignVCenter, kontinentName(k));
        p.setFont(mono(Style::kFontCaption));
        p.setPen(farbe(Style::kTextTertiary));
        p.drawText(QRectF(ort.x(), ort.y() + 16, 200, 14), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("%1 / %2").arg(gearb).arg(alle));
    }

    // Die Schrift der Kacheln: die groesste der Leiter, die passt.
    int px = 0;
    if (t >= 46.0) { px = Style::kFontSmall; }
    else if (t >= 30.0) { px = Style::kFontCaption; }
    else if (t >= 22.0) { px = Style::kFontMicro; }
    const QStringList& punkte = LogbuchBilder::wandBaender();
    for (int i = 0; i < m_w.kacheln.size(); ++i) {
        const QRectF r = a.kacheln[i];
        if (r.isEmpty()) { continue; }
        const LogbuchBilder::DxccKachel& k = m_w.kacheln[i];
        const int nb = int(k.baender.size());
        const double hell = 0.22 + 0.78 * std::min(nb, 6) / 6.0;
        if (k.gearbeitet()) {
            p.setPen(k.bestaetigt ? QPen(farbe(Style::kGreenText), 1.4) : QPen(Qt::NoPen));
            p.setBrush(farbe(Style::kAmberText, int(255 * hell * 0.55)));
        } else {
            p.setPen(QPen(farbe(Style::kBorder), 0.8));
            p.setBrush(farbe(Style::kGlassPanelTop));
        }
        p.drawRoundedRect(r, 2.5, 2.5);
        if (px > 0) {
            const QString text = anzeigePraefix(k.praefix);
            QFont f = schrift(px, k.gearbeitet() ? QFont::DemiBold : QFont::Normal);
            // Lange Praefixe ("VP8/h") eine Stufe kleiner, statt anzustossen.
            if (QFontMetricsF(f).horizontalAdvance(text) > r.width() - 3 && px > Style::kFontMicro) {
                f.setPixelSize(px == Style::kFontSmall ? Style::kFontCaption : Style::kFontMicro);
            }
            p.setFont(f);
            p.setPen(!k.gearbeitet() ? farbe(Style::kTextTertiary)
                     : hell > 0.75   ? farbe(Style::kPanelBg)
                                     : farbe(Style::kTextPrimary));
            p.drawText(QRectF(r.left(), r.top(), r.width(), r.height() * 0.74), Qt::AlignCenter, text);
        }
        // Zehn Bandpunkte unten
        const double rp = std::max(0.9, r.width() * 0.03);
        p.setPen(Qt::NoPen);
        for (int j = 0; j < punkte.size(); ++j) {
            const bool hat = k.baender.contains(punkte[j]);
            p.setBrush(hat ? bandFarbe(punkte[j]) : farbe(Style::kBorder));
            p.drawEllipse(QPointF(r.left() + r.width() * (0.1 + j * 0.089), r.top() + r.height() * 0.84),
                          rp, rp);
        }
    }
    fuss(p, rect(), fusszeile());
}

} // namespace Longpath
