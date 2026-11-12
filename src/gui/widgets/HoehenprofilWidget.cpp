// no-port-check: Longpath-original, no Thetis logic.
//
// src/gui/widgets/HoehenprofilWidget.cpp  (Longpath) -- siehe .h.
//
// Modification history (Longpath):
//   2026-11-11  AI (Anthropic Claude Code), operator Martin Fischer
//               (OE5SOS). Original.

#include "gui/widgets/HoehenprofilWidget.h"

#include "gui/StyleConstants.h"

#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

QColor farbe(const char* hex, double alpha = 1.0)
{
    QColor c(QString::fromLatin1(hex));
    c.setAlphaF(alpha);
    return c;
}

// 1, 2 oder 5 mal einer Zehnerpotenz -- so, dass etwa `ziel` Teilstriche
// in die Spanne passen.
double schritt(double spanne, int ziel)
{
    if (!(spanne > 0.0)) { return 1.0; }
    const double roh = spanne / ziel;
    const double p = std::pow(10.0, std::floor(std::log10(roh)));
    const double n = roh / p;
    const double s = n < 1.5 ? 1.0 : n < 3.0 ? 2.0 : n < 7.0 ? 5.0 : 10.0;
    return s * p;
}

QString zahlText(double v, double schritt)
{
    const int stellen = schritt < 1.0 ? 1 : 0;
    return QString::number(v, 'f', stellen);
}

} // namespace

HoehenprofilWidget::HoehenprofilWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void HoehenprofilWidget::setProfil(const Hoehenprofil::Eingabe& ein,
                                   const Hoehenprofil::Ergebnis& erg,
                                   const Beschriftung& text)
{
    m_ein = ein;
    m_erg = erg;
    m_text = text;
    m_hinweis.clear();
    m_hinweisTitel.clear();
    update();
}

void HoehenprofilWidget::setHinweis(const QString& titel, const QString& satz)
{
    m_erg = Hoehenprofil::Ergebnis{};
    m_hinweisTitel = titel;
    m_hinweis = satz;
    update();
}

QString HoehenprofilWidget::urteilZeile() const
{
    return Hoehenprofil::urteilText(m_erg, m_text.genauigkeit);
}

void HoehenprofilWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), farbe(Style::kPanelBg));

    QFont kopf = font();
    kopf.setPixelSize(Style::kFontBody);
    kopf.setBold(true);
    QFont normal = font();
    normal.setPixelSize(Style::kFontSmall);
    const QFontMetricsF fmK(kopf), fmN(normal);

    const double links = 58.0, rechts = 16.0, oben = 40.0, unten = 44.0;
    const QRectF flaeche(links, oben, std::max(40.0, width() - links - rechts),
                         std::max(40.0, height() - oben - unten));

    // ── Kopfzeile ───────────────────────────────────────────────────
    const QString titel = m_erg.gueltig ? m_text.titel : m_hinweisTitel;
    p.setFont(kopf);
    p.setPen(farbe(Style::kAmberText));
    const double titelBreite = fmK.horizontalAdvance(titel);
    p.drawText(QPointF(links, 24.0), titel);
    if (m_erg.gueltig) {
        p.setFont(normal);
        p.setPen(farbe(Style::kProfilText));
        QString urteil = urteilZeile();
        const double platz = width() - rechts - (links + titelBreite + 24.0);
        urteil = fmN.elidedText(urteil, Qt::ElideRight, std::max(60.0, platz));
        p.drawText(QPointF(width() - rechts - fmN.horizontalAdvance(urteil), 24.0), urteil);
    }

    if (!m_erg.gueltig) {
        p.setFont(normal);
        p.setPen(farbe(Style::kProfilTextLeise));
        p.drawText(flaeche, Qt::AlignCenter | Qt::TextWordWrap, m_hinweis);
        return;
    }

    const Hoehenprofil::Ergebnis& e = m_erg;
    const QVector<double>& d = m_ein.distKm;
    const int n = d.size();
    const double D = e.gesamtKm;

    // ── Massstab ────────────────────────────────────────────────────
    double lo = 1e18, hi = -1e18;
    for (int i = 0; i < n; ++i) {
        lo = std::min({lo, e.gelaende.at(i), e.sicht.at(i)});
        hi = std::max({hi, e.gelaende.at(i), e.sicht.at(i)});
    }
    if (e.hatHorizont) { hi = std::max(hi, e.schnittHoeheM); }
    const double spanne = std::max(50.0, hi - lo);
    const double yUnten = lo - std::max(20.0, 0.12 * spanne);
    const double yOben = hi + std::max(40.0, 0.20 * spanne);
    auto X = [&](double km) { return flaeche.left() + km / D * flaeche.width(); };
    auto Y = [&](double m) {
        return flaeche.top() + (yOben - m) / (yOben - yUnten) * flaeche.height();
    };

    // ── Gitter und Achsen ───────────────────────────────────────────
    p.setFont(normal);
    const double sx = schritt(D, std::max(3, static_cast<int>(flaeche.width() / 110.0)));
    const double sy = schritt(yOben - yUnten, std::max(3, static_cast<int>(flaeche.height() / 55.0)));
    QPen gitter(farbe(Style::kProfilGitter));
    gitter.setWidthF(1.0);
    for (double v = 0.0; v <= D + 1e-9; v += sx) {
        p.setPen(gitter);
        p.drawLine(QPointF(X(v), flaeche.top()), QPointF(X(v), flaeche.bottom()));
        p.setPen(farbe(Style::kProfilTextLeise));
        const QString t = zahlText(v, sx);
        p.drawText(QPointF(X(v) - fmN.horizontalAdvance(t) / 2.0, flaeche.bottom() + 16.0), t);
    }
    for (double v = std::ceil(yUnten / sy) * sy; v <= yOben; v += sy) {
        p.setPen(gitter);
        p.drawLine(QPointF(flaeche.left(), Y(v)), QPointF(flaeche.right(), Y(v)));
        p.setPen(farbe(Style::kProfilTextLeise));
        const QString t = zahlText(v, sy);
        p.drawText(QPointF(flaeche.left() - 8.0 - fmN.horizontalAdvance(t), Y(v) + 4.0), t);
    }
    p.setPen(farbe(Style::kProfilTextLeise));
    const QString xTitel = QStringLiteral("Distance km");
    p.drawText(QPointF(flaeche.center().x() - fmN.horizontalAdvance(xTitel) / 2.0,
                       flaeche.bottom() + 34.0), xTitel);
    p.save();
    p.translate(14.0, flaeche.center().y());
    p.rotate(-90.0);
    const QString yTitel = QStringLiteral("Height m");
    p.drawText(QPointF(-fmN.horizontalAdvance(yTitel) / 2.0, 0.0), yTitel);
    p.restore();

    p.save();
    p.setClipRect(flaeche);

    // ── Wo ein Ende nur ungefaehr steht (Locator): leise hinterlegt ──
    p.setPen(Qt::NoPen);
    p.setBrush(farbe(Style::kProfilGitter, 0.45));
    if (e.unsicherAKm > 0.0) {
        p.drawRect(QRectF(QPointF(X(0.0), flaeche.top()),
                          QPointF(X(e.unsicherAKm), flaeche.bottom())));
    }
    if (e.unsicherBKm > 0.0) {
        p.drawRect(QRectF(QPointF(X(D - e.unsicherBKm), flaeche.top()),
                          QPointF(X(D), flaeche.bottom())));
    }

    // ── Fresnel-Zone ────────────────────────────────────────────────
    QPolygonF zone;
    for (int i = 0; i < n; ++i) { zone << QPointF(X(d.at(i)), Y(e.sicht.at(i) + e.fresnel.at(i))); }
    for (int i = n - 1; i >= 0; --i) { zone << QPointF(X(d.at(i)), Y(e.sicht.at(i) - e.fresnel.at(i))); }
    p.setPen(Qt::NoPen);
    p.setBrush(farbe(Style::kProfilText, 0.07));
    p.drawPolygon(zone);

    // ── Gelaende ────────────────────────────────────────────────────
    QPolygonF boden;
    boden << QPointF(X(0.0), flaeche.bottom() + 2.0);
    for (int i = 0; i < n; ++i) { boden << QPointF(X(d.at(i)), Y(e.gelaende.at(i))); }
    boden << QPointF(X(D), flaeche.bottom() + 2.0);
    p.setBrush(farbe(Style::kProfilGelaende));
    p.drawPolygon(boden);
    QPolygonF kante;
    for (int i = 0; i < n; ++i) { kante << QPointF(X(d.at(i)), Y(e.gelaende.at(i))); }
    QPen kantenStift(farbe(Style::kProfilKante));
    kantenStift.setWidthF(1.0);
    p.setPen(kantenStift);
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(kante);

    // ── Sichtlinie ──────────────────────────────────────────────────
    QPen linie(farbe(Style::kProfilText, 0.85));
    linie.setWidthF(1.3);
    p.setPen(linie);
    p.drawLine(QPointF(X(0.0), Y(e.hoeheA)), QPointF(X(D), Y(e.hoeheB)));

    // ── Bernstein: Gelaende in den inneren 60 % der Zone ────────────
    p.setPen(Qt::NoPen);
    p.setBrush(farbe(Style::kAmberText, 0.35));
    for (int i = 0; i < n;) {
        if (!e.inZone.at(i)) { ++i; continue; }
        int j = i;
        while (j + 1 < n && e.inZone.at(j + 1)) { ++j; }
        QPolygonF stueck;
        for (int k = i; k <= j; ++k) { stueck << QPointF(X(d.at(k)), Y(e.gelaende.at(k))); }
        for (int k = j; k >= i; --k) {
            const double unterkante = std::max(
                e.sicht.at(k) - Hoehenprofil::kZonenAnteil * e.fresnel.at(k), yUnten);
            stueck << QPointF(X(d.at(k)), Y(std::min(unterkante, e.gelaende.at(k))));
        }
        p.drawPolygon(stueck);
        i = j + 1;
    }

    // ── Horizontstrahlen und gemeinsames Volumen ────────────────────
    if (e.hatHorizont) {
        QPen strahl(farbe(Style::kAmberText));
        strahl.setWidthF(1.6);
        strahl.setDashPattern({6.0, 3.0});
        p.setPen(strahl);
        const QPointF a(X(0.0), Y(e.hoeheA));
        const QPointF s(X(e.schnittKm), Y(e.schnittHoeheM));
        const QPointF b(X(D), Y(e.hoeheB));
        p.drawLine(a, s);
        p.drawLine(s, b);
        p.setPen(Qt::NoPen);
        p.setBrush(farbe(Style::kAmberText));
        p.drawEllipse(s, 4.5, 4.5);

        const QString z1 = QStringLiteral("common volume  %1 m above ground")
                               .arg(e.schnittUeberGrundM, 0, 'f', 0);
        const QString z2 = QStringLiteral("scatter angle %1°")
                               .arg(e.streuwinkelGrad, 0, 'f', 2);
        // Ueber dem Punkt, mittig: dort laeuft kein Strahl (beide fallen
        // vom Schnitt aus ab) und kein Gelaende. Nur wenn oben kein Platz
        // ist, darunter.
        const double breite = std::max(fmN.horizontalAdvance(z1), fmN.horizontalAdvance(z2));
        const double tx = std::clamp(s.x() - breite / 2.0, flaeche.left() + 4.0,
                                     flaeche.right() - 4.0 - breite);
        double ty = s.y() - 12.0 - 15.0;           // Grundlinie der ersten Zeile
        if (ty - fmN.ascent() < flaeche.top() + 2.0) { ty = s.y() + 12.0 + fmN.ascent(); }
        p.setPen(farbe(Style::kAmberText));
        p.drawText(QPointF(tx, ty), z1);
        p.drawText(QPointF(tx, ty + 15.0), z2);
    }

    // ── Die beiden Enden ────────────────────────────────────────────
    p.setPen(farbe(Style::kProfilTextLeise));
    p.drawText(QPointF(flaeche.left() + 4.0, flaeche.bottom() - 6.0), m_text.linksUnten);
    p.drawText(QPointF(flaeche.right() - 4.0 - fmN.horizontalAdvance(m_text.rechtsUnten),
                       flaeche.bottom() - 6.0), m_text.rechtsUnten);
    p.restore();
}

} // namespace Longpath
