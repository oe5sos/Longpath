// no-port-check: Longpath-original widgets, see LogbuchBilderWidgets.h.
//
// src/gui/widgets/LogbuchBilderWidgets.cpp  (Longpath)

#include "gui/widgets/LogbuchBilderWidgets.h"

#include "gui/StyleConstants.h"

#include <QDateTime>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

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

void panel(QPainter& p, const QRectF& r)
{
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0, farbe(Style::kGlassPanelTop));
    g.setColorAt(1.0, farbe(Style::kGlassPanelBot));
    p.setPen(QPen(farbe(Style::kBorder), 1.0));
    p.setBrush(g);
    p.drawRoundedRect(r, Style::kGlassPanelRadius, Style::kGlassPanelRadius);
}
void kopf(QPainter& p, const QRectF& r, const QString& links, const QString& rechts)
{
    p.setFont(Style::capsFont(QFont(), Style::kFontCaption));
    p.setPen(farbe(Style::kTextSecondary));
    p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, links);
    p.setPen(farbe(Style::kTextTertiary));
    p.drawText(r, Qt::AlignRight | Qt::AlignVCenter, rechts);
}
QString drei(double grad) { return QStringLiteral("%1").arg(qRound(grad) % 360, 3, 10, QLatin1Char('0')); }

} // namespace

// ── A  Bedarfs-Matrix ────────────────────────────────────────────────

BedarfsMatrixWidget::BedarfsMatrixWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("bedarfsMatrix"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(150);
}

void BedarfsMatrixWidget::setBedarf(const LogbuchBilder::Bedarf& b)
{
    m_b = b;
    update();
}

void BedarfsMatrixWidget::paintEvent(QPaintEvent*)
{
    using U = LogbuchBilder::Bedarf::Urteil;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF karte = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    panel(p, karte);

    const double L = 14.0;
    p.setFont(schrift(Style::kFontBody, QFont::DemiBold));
    p.setPen(farbe(Style::kTextPrimary));
    p.drawText(QRectF(L, 10, 250, 20), Qt::AlignLeft | Qt::AlignVCenter,
               m_b.land.isEmpty() ? QStringLiteral("Unknown prefix") : m_b.landName);
    p.setFont(mono(Style::kFontSmall));
    p.setPen(farbe(Style::kTextSecondary));
    if (m_b.lageBekannt) {
        p.drawText(QRectF(L, 30, 250, 18), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("%1 · %2 km · %3°").arg(m_b.kontinent).arg(qRound(m_b.km)).arg(drei(m_b.grad)));
    }
    p.setFont(schrift(Style::kFontSmall));
    p.drawText(QRectF(L, 52, 250, 18), Qt::AlignLeft | Qt::AlignVCenter,
               m_b.qsosCall > 0 ? QStringLiteral("This call: worked %1×").arg(m_b.qsosCall)
                                : QStringLiteral("This call: never worked"));
    if (m_b.qsosCall > 0) {
        p.setPen(farbe(Style::kTextTertiary));
        p.drawText(QRectF(L, 70, 250, 18), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("last %1 · %2 %3")
                       .arg(m_b.zuletzt.toUTC().toString(QStringLiteral("yyyy-MM-dd")),
                            m_b.zuletztBand, m_b.zuletztMode));
    }
    p.setPen(farbe(Style::kTextSecondary));
    p.drawText(QRectF(L, 90, 250, 18), Qt::AlignLeft | Qt::AlignVCenter,
               m_b.qsosLand > 0 ? QStringLiteral("Entity: %1 QSOs, %2 slots confirmed")
                                      .arg(m_b.qsosLand).arg(m_b.bestaetigt.size())
                                : QStringLiteral("Entity: not in the log"));

    // Urteil -- so breit wie sein Text.
    const QString urteil = m_b.urteilText();
    if (!urteil.isEmpty()) {
        QColor f = farbe(Style::kAmberText);
        if (m_b.urteil == U::Unbestaetigt) { f = farbe(Style::kTextPrimary); }
        if (m_b.urteil == U::Bestaetigt)   { f = farbe(Style::kGreenText); }
        p.setFont(Style::capsFont(QFont(), Style::kFontSmall));
        const double w = QFontMetricsF(p.font()).horizontalAdvance(urteil) + 26.0;
        const QRectF chip(L, 114, w, 26);
        QColor fuell = f; fuell.setAlpha(36);
        p.setPen(QPen(f, 1.2));
        p.setBrush(fuell);
        p.drawRoundedRect(chip, 5, 5);
        p.setPen(f);
        p.drawText(chip, Qt::AlignCenter, urteil);
    }

    // Rechts: die Matrix, so breit es geht.
    const QStringList& baender = LogbuchBilder::baender();
    const QStringList& gruppen = LogbuchBilder::gruppen();
    const double x0 = 330.0;
    const double zw = std::clamp((width() - 16.0 - x0) / baender.size(), 20.0, 40.0);
    const double zh = 30.0, y0 = 30.0;
    const QString jetztBand = m_b.jetzt.section(QLatin1Char('|'), 0, 0);
    const QString jetztGruppe = m_b.jetzt.section(QLatin1Char('|'), 1, 1);
    p.setFont(mono(Style::kFontCaption));
    for (int i = 0; i < baender.size(); ++i) {
        p.setPen(baender[i] == jetztBand ? farbe(Style::kTextPrimary) : farbe(Style::kTextScale));
        p.drawText(QRectF(x0 + i * zw, y0 - 20, zw, 16), Qt::AlignCenter,
                   QString(baender[i]).remove(QLatin1Char('m')));
    }
    for (int j = 0; j < gruppen.size(); ++j) {
        const QString g = gruppen[j];
        p.setPen(g == jetztGruppe ? farbe(Style::kTextPrimary) : farbe(Style::kTextScale));
        p.drawText(QRectF(x0 - 56, y0 + j * zh, 50, zh), Qt::AlignRight | Qt::AlignVCenter,
                   g == QLatin1String("PHONE") ? QStringLiteral("SSB")
                   : g == QLatin1String("DATA") ? QStringLiteral("DIGI") : g);
        for (int i = 0; i < baender.size(); ++i) {
            const QString k = LogbuchBilder::zelle(baender[i], g);
            const QRectF z(x0 + i * zw + 4, y0 + j * zh + 4, zw - 8, zh - 8);
            if (m_b.bestaetigt.contains(k)) {
                p.setPen(Qt::NoPen);
                p.setBrush(farbe(Style::kGreenText, 200));
                p.drawRoundedRect(z, 4, 4);
            } else if (m_b.gearbeitet.contains(k)) {
                p.setPen(QPen(farbe(Style::kAmberText), 1.6));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(z.adjusted(1, 1, -1, -1), 4, 4);
            } else {
                p.setPen(QPen(farbe(Style::kBorder), 1.0));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(z, 4, 4);
            }
            if (m_b.diesesCall.contains(k)) {
                p.setPen(Qt::NoPen);
                p.setBrush(farbe(Style::kTextPrimary));
                p.drawEllipse(z.center(), 3.0, 3.0);
            }
            if (k == m_b.jetzt) {
                const bool neu = !m_b.gearbeitet.contains(k);
                p.setBrush(Qt::NoBrush);
                if (neu) {
                    p.setPen(QPen(farbe(Style::kAmberText, 60), 6.0));
                    p.drawRoundedRect(z.adjusted(-2, -2, 2, 2), 6, 6);
                }
                p.setPen(QPen(neu ? farbe(Style::kAmberText) : farbe(Style::kTextPrimary), 2.0));
                p.drawRoundedRect(z.adjusted(-2, -2, 2, 2), 6, 6);
            }
        }
    }
    // Legende
    const double ly = y0 + 3 * zh + 12;
    p.setFont(schrift(Style::kFontCaption));
    auto legende = [&](double x, int art, const QString& text) {
        const QRectF z(x, ly, 14, 12);
        p.setBrush(Qt::NoBrush);
        if (art == 0) { p.setPen(Qt::NoPen); p.setBrush(farbe(Style::kGreenText, 200)); p.drawRoundedRect(z, 3, 3); }
        if (art == 1) { p.setPen(QPen(farbe(Style::kAmberText), 1.4)); p.drawRoundedRect(z, 3, 3); }
        if (art == 2) { p.setPen(QPen(farbe(Style::kBorder), 1.0)); p.drawRoundedRect(z, 3, 3); }
        if (art == 3) { p.setPen(Qt::NoPen); p.setBrush(farbe(Style::kTextPrimary)); p.drawEllipse(z.center(), 3.0, 3.0); }
        p.setPen(farbe(Style::kTextTertiary));
        p.drawText(QRectF(x + 20, ly - 2, 110, 16), Qt::AlignLeft | Qt::AlignVCenter, text);
    };
    legende(x0, 0, QStringLiteral("confirmed"));
    legende(x0 + 100, 1, QStringLiteral("worked"));
    legende(x0 + 190, 2, QStringLiteral("never"));
    legende(x0 + 270, 3, QStringLiteral("this call"));
}

// ── B  Band x UTC-Stunde ─────────────────────────────────────────────

BandStundeWidget::BandStundeWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("bandStunde"));
}

void BandStundeWidget::setDaten(const LogbuchBilder::BandStunde& d, const QString& titel,
                                const QString& meinFeld)
{
    m_d = d;
    m_titel = titel;
    m_feld = meinFeld;
    updateGeometry();
    update();
}

QSize BandStundeWidget::sizeHint() const
{
    return {64 + 24 * 26 + 166, 64 + std::max<int>(1, m_d.zeilen.size()) * 22 + 62};
}

void BandStundeWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    panel(p, QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
    kopf(p, QRectF(14, 8, width() - 28, 18),
         QStringLiteral("WHEN DOES WHICH BAND WORK FROM %1").arg(m_feld.isEmpty() ? QStringLiteral("HERE") : m_feld.toUpper()),
         QStringLiteral("%1 · %2").arg(m_titel).arg(m_d.gesamt));
    if (m_d.zeilen.isEmpty()) {
        p.setFont(schrift(Style::kFontSmall));
        p.setPen(farbe(Style::kTextTertiary));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("No contacts in this view."));
        return;
    }
    const double x0 = 64.0, y0 = 58.0, zh = 22.0;
    const double zw = std::clamp((width() - x0 - 166.0) / 24.0, 12.0, 30.0);
    p.setFont(mono(Style::kFontCaption));
    p.setPen(farbe(Style::kTextScale));
    for (int h = 0; h < 24; h += 3) {
        p.drawText(QRectF(x0 + h * zw - 6, y0 - 22, zw + 12, 16), Qt::AlignCenter,
                   QStringLiteral("%1").arg(h, 2, 10, QLatin1Char('0')));
    }
    p.setPen(farbe(Style::kTextTertiary));
    p.drawText(QRectF(x0 + 24 * zw + 6, y0 - 22, 60, 16), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("UTC"));
    const int stufen = 5;
    auto ton = [&](int stufe) { QColor f = farbe(Style::kAmberText); f.setAlpha(40 + stufe * 43); return f; };
    for (int r = 0; r < m_d.zeilen.size(); ++r) {
        const QString band = m_d.zeilen[r];
        const QVector<int> v = m_d.anzahl.value(band);
        const double y = y0 + r * zh;
        p.setFont(mono(Style::kFontSmall));
        p.setPen(farbe(Style::kTextSecondary));
        p.drawText(QRectF(4, y, x0 - 12, zh), Qt::AlignRight | Qt::AlignVCenter, band);
        int summe = 0;
        for (int h = 0; h < 24; ++h) {
            summe += v[h];
            const QRectF c(x0 + h * zw + 1.5, y + 1.5, zw - 3, zh - 3);
            if (v[h] == 0) {
                p.setPen(QPen(farbe(Style::kBorder, 120), 1.0));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(c, 3, 3);
                continue;
            }
            // Stufen nach log, damit wenige QSOs sichtbar bleiben.
            const double t = std::log(1.0 + v[h]) / std::log(1.0 + m_d.maxWert);
            p.setPen(Qt::NoPen);
            p.setBrush(ton(std::clamp(int(std::ceil(t * stufen)), 1, stufen)));
            p.drawRoundedRect(c, 3, 3);
        }
        const int beste = m_d.besteStunde(band);
        p.setFont(mono(Style::kFontCaption));
        p.setPen(farbe(Style::kTextTertiary));
        p.drawText(QRectF(x0 + 24 * zw + 8, y, 160, zh), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("%1  best %2–%3").arg(summe, 4)
                       .arg(beste, 2, 10, QLatin1Char('0')).arg((beste + 1) % 24, 2, 10, QLatin1Char('0')));
    }
    // Jetzt
    const QTime t = QDateTime::currentDateTimeUtc().time();
    const double xj = x0 + (t.hour() + t.minute() / 60.0) * zw;
    const double yEnde = y0 + m_d.zeilen.size() * zh;
    p.setPen(QPen(farbe(Style::kTextPrimary, 200), 1.2));
    p.drawLine(QPointF(xj, y0 - 4), QPointF(xj, yEnde + 4));
    p.setFont(Style::capsFont(QFont(), Style::kFontMicro));
    p.drawText(QRectF(xj - 30, yEnde + 6, 60, 12), Qt::AlignCenter,
               QStringLiteral("NOW %1Z").arg(t.toString(QStringLiteral("HHmm"))));
    // Legende
    const double ly = height() - 26.0;
    p.setFont(schrift(Style::kFontCaption));
    p.setPen(farbe(Style::kTextTertiary));
    p.drawText(QRectF(x0, ly, 60, 14), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("fewer"));
    for (int s = 1; s <= stufen; ++s) {
        p.setPen(Qt::NoPen);
        p.setBrush(ton(s));
        p.drawRoundedRect(QRectF(x0 + 40 + s * 18, ly + 1, 14, 12), 3, 3);
    }
    p.setPen(farbe(Style::kTextTertiary));
    p.drawText(QRectF(x0 + 40 + 6 * 18 + 4, ly, 60, 14), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("more"));
}

// ── C  Richtungsrose ─────────────────────────────────────────────────

RichtungsRoseWidget::RichtungsRoseWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("richtungsRose"));
}

void RichtungsRoseWidget::setRose(const LogbuchBilder::Rose& r, const QString& titel)
{
    m_r = r;
    m_titel = titel;
    update();
}

void RichtungsRoseWidget::setRotor(double beamDeg, double breiteDeg, const RotorBereich& bereich)
{
    m_beam = beamDeg;
    m_breite = breiteDeg > 0.0 ? breiteDeg : 60.0;
    m_bereich = bereich;
    update();
}

void RichtungsRoseWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    panel(p, QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
    kopf(p, QRectF(14, 8, width() - 28, 18), QStringLiteral("WHERE YOUR QSOS COME FROM"),
         QStringLiteral("%1 · %2").arg(m_titel).arg(m_r.punkte.size()));

    const double seite = std::min<double>(width() - 40.0, height() - 110.0);
    const double K1 = seite / 2.0;               // aussen am Kranz
    const double K0 = K1 - 34.0;
    const double R = K0 - 10.0;                  // Punkte innen
    if (R < 60.0) { return; }
    const QPointF m(width() / 2.0, 38.0 + K1 + 14.0);
    auto punkt = [&](double grad, double r) {
        const double w = (grad - 90.0) * M_PI / 180.0;
        return QPointF(m.x() + r * std::cos(w), m.y() + r * std::sin(w));
    };

    // Entfernung: ueber 3000 km (KW) logarithmisch 100..20 000 km, sonst
    // linear bis knapp ueber das 90. Perzentil -- zwei Ausreisser (Es,
    // Tropo) sollen die Skala nicht aufziehen; sie stehen dann am Rand.
    QVector<double> kms;
    for (const auto& q : m_r.punkte) { kms << q.km; }
    std::sort(kms.begin(), kms.end());
    const double p90 = kms.isEmpty() ? 1000.0
                     : kms[std::min<int>(kms.size() - 1, int(kms.size() * 0.90))];
    const bool logSkala = p90 > 3000.0;
    double maxKm = 3000.0;
    for (double n : {200.0, 300.0, 500.0, 800.0, 1000.0, 1500.0, 2000.0, 3000.0}) {
        if (n >= p90 * 1.1) { maxKm = n; break; }
    }
    auto radius = [&](double km) {
        if (logSkala) {
            const double t = (std::log10(std::clamp(km, 100.0, 20000.0)) - 2.0)
                           / (std::log10(20000.0) - 2.0);
            return 10.0 + t * (R - 10.0);
        }
        return std::min(km, maxKm) / maxKm * R;
    };

    // Gesperrter Teil des Rotors: abgedunkelt.
    const bool bereich = m_bereich.an;
    if (bereich) {
        const double von = m_bereich.bisGrad;      // gesperrt: bis -> von
        const double laenge = std::fmod(m_bereich.vonGrad - m_bereich.bisGrad + 360.0, 360.0);
        if (laenge > 0.5) {
            QPainterPath sperre(m);
            sperre.arcTo(QRectF(m.x() - K1, m.y() - K1, 2 * K1, 2 * K1), 90.0 - von, -laenge);
            sperre.closeSubpath();
            p.setPen(Qt::NoPen);
            p.setBrush(farbe(Style::kPanelBg, 200));
            p.drawPath(sperre);
        }
    }
    // Ringe
    p.setFont(mono(Style::kFontMicro));
    const QVector<double> ringe = logSkala ? QVector<double>{300.0, 1000.0, 3000.0, 10000.0}
                                           : QVector<double>{maxKm / 4, maxKm / 2, maxKm * 3 / 4};
    auto ringText = [](double km) {
        return km >= 1000 ? QStringLiteral("%1k km").arg(km / 1000) : QStringLiteral("%1 km").arg(km);
    };
    for (double km : ringe) {
        const double r = radius(km);
        p.setPen(QPen(farbe(Style::kBorder), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(m, r, r);
        p.setPen(farbe(Style::kTextScale));
        p.drawText(QRectF(m.x() + 4, m.y() - r - 12, 70, 11), Qt::AlignLeft | Qt::AlignVCenter, ringText(km));
    }
    p.setPen(QPen(farbe(Style::kBorder), 1.0));
    p.drawEllipse(m, R, R);
    p.setPen(farbe(Style::kTextScale));
    p.drawText(QRectF(m.x() + 4, m.y() - R - 12, 70, 11), Qt::AlignLeft | Qt::AlignVCenter,
               logSkala ? QStringLiteral("20k km") : ringText(maxKm));
    // Der Kranz: Anteil je 10 Grad -- eine eigene Achse, nicht die Entfernung.
    p.setPen(QPen(farbe(Style::kBorder, 160), 1.0));
    p.drawEllipse(m, K0, K0);
    const int maxS = *std::max_element(m_r.sektor.cbegin(), m_r.sektor.cend());
    const int bester = m_r.besterSektor();
    for (int s = 0; s < 36 && maxS > 0; ++s) {
        if (m_r.sektor[s] == 0) { continue; }
        const double rr = K0 + (K1 - K0) * std::sqrt(double(m_r.sektor[s]) / maxS);
        QPainterPath keil;
        keil.moveTo(punkt(s * 10.0 + 0.8, K0));
        keil.lineTo(punkt(s * 10.0 + 0.8, rr));
        keil.lineTo(punkt(s * 10.0 + 9.2, rr));
        keil.lineTo(punkt(s * 10.0 + 9.2, K0));
        keil.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(farbe(Style::kAmberText, s == bester ? 240 : 150));
        p.drawPath(keil);
    }
    // Punkte
    p.setPen(Qt::NoPen);
    p.setBrush(farbe(Style::kAmberText, 120));
    for (const auto& q : m_r.punkte) { p.drawEllipse(punkt(q.grad, radius(q.km)), 1.6, 1.6); }
    // Himmelsrichtungen
    p.setFont(Style::capsFont(QFont(), Style::kFontCaption));
    p.setPen(farbe(Style::kTextSecondary));
    const char* namen[4] = {"N", "E", "S", "W"};
    for (int i = 0; i < 4; ++i) {
        const QPointF q = punkt(i * 90.0, K1 + 13.0);
        p.drawText(QRectF(q.x() - 10, q.y() - 8, 20, 16), Qt::AlignCenter, QString::fromLatin1(namen[i]));
    }
    // Rotor: Strahl und erlaubter Bogen
    if (m_beam >= 0.0) {
        QPainterPath kegel(m);
        kegel.arcTo(QRectF(m.x() - R, m.y() - R, 2 * R, 2 * R), 90.0 - m_beam + m_breite / 2.0, -m_breite);
        kegel.closeSubpath();
        p.setPen(QPen(farbe(Style::kTextPrimary, 90), 1.0));
        p.setBrush(farbe(Style::kTextPrimary, 14));
        p.drawPath(kegel);
        p.setPen(QPen(farbe(Style::kTextPrimary), 1.6));
        p.drawLine(m, punkt(m_beam, R));
    }
    if (bereich) {
        const double laenge = std::fmod(m_bereich.bisGrad - m_bereich.vonGrad + 360.0, 360.0);
        p.setPen(QPen(farbe(Style::kGreenText), 3.0, Qt::SolidLine, Qt::FlatCap));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(m.x() - K1 - 5, m.y() - K1 - 5, 2 * K1 + 10, 2 * K1 + 10),
                  int((90.0 - m_bereich.vonGrad) * 16), int(-(laenge < 0.5 ? 360.0 : laenge) * 16));
    }
    // Zusammenfassung
    QString zus;
    if (!m_r.punkte.isEmpty()) {
        zus = QStringLiteral("Most QSOs: %1–%2°").arg(drei(bester * 10.0), drei(bester * 10.0 + 10.0));
        int a = -1, l = 0;
        m_r.groessteLuecke(&a, &l);
        if (l > 0) {
            zus += QStringLiteral("  ·  never worked: %1–%2°").arg(drei(a * 10.0), drei(((a + l) % 36) * 10.0));
        }
    }
    if (m_beam >= 0.0) { zus += QStringLiteral("  ·  beam %1°").arg(drei(m_beam)); }
    if (bereich) {
        zus += QStringLiteral(", allowed %1°→%2°").arg(drei(m_bereich.vonGrad), drei(m_bereich.bisGrad));
    }
    p.setFont(schrift(Style::kFontSmall));
    p.setPen(farbe(Style::kTextSecondary));
    p.drawText(QRectF(14, height() - 46, width() - 28, 18), Qt::AlignLeft | Qt::AlignVCenter, zus);
    QStringList fuss;
    if (m_r.ausLand > 0) { fuss << QStringLiteral("%1 without a locator placed at their country's centre").arg(m_r.ausLand); }
    if (m_r.ohneOrt > 0) { fuss << QStringLiteral("%1 without a locator not shown").arg(m_r.ohneOrt); }
    p.setFont(schrift(Style::kFontCaption));
    p.setPen(farbe(Style::kTextTertiary));
    p.drawText(QRectF(14, height() - 26, width() - 28, 14), Qt::AlignLeft | Qt::AlignVCenter,
               fuss.join(QStringLiteral(" · ")));
}

} // namespace Longpath
