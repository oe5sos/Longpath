// no-port-check: Longpath-original, no Thetis logic (Thetis has no rotor).
//
// =================================================================
// src/gui/widgets/RotorBandWidget.cpp  (Longpath)
// =================================================================
//
// Siehe RotorBandWidget.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11  Martin Fischer  Neu. AI tooling: Anthropic Claude.

#include "RotorBandWidget.h"

#include "gui/StyleConstants.h"

#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

QColor ton(const char* hex) { return QColor(QLatin1String(hex)); }

double norm360(double d)
{
    double r = std::fmod(d, 360.0);
    if (r < 0.0) { r += 360.0; }
    return r;
}

// Vorzeichenbehafteter Abstand von `von` nach `nach`, -180..180.
double abstand(double von, double nach)
{
    return std::fmod(nach - von + 540.0, 360.0) - 180.0;
}

} // namespace

RotorBandWidget::RotorBandWidget(QWidget* parent)
    : QWidget(parent), m_farbe(Style::kAmberText)
{
    setFixedSize(kBreite, kHoehe);
    aktualisiereText();
}

void RotorBandWidget::setStand(double deg)
{
    const double neu = (std::isnan(deg) || deg < 0.0) ? -1.0 : norm360(deg);
    if (neu == m_stand) { return; }
    m_stand = neu;
    aktualisiereText();
    update();
}

void RotorBandWidget::setStation(double deg)
{
    const double neu = (std::isnan(deg) || deg < 0.0) ? -1.0 : norm360(deg);
    if (neu == m_station) { return; }
    m_station = neu;
    aktualisiereText();
    update();
}

void RotorBandWidget::setZiel(bool istZiel)
{
    if (istZiel == m_ziel) { return; }
    m_ziel = istZiel;
    aktualisiereText();
    update();
}

void RotorBandWidget::setFarbe(const char* hex)
{
    if (hex == m_farbe) { return; }
    m_farbe = hex;
    update();
}

void RotorBandWidget::setStrahlbreite(double deg)
{
    m_strahl = std::clamp(deg, 1.0, 180.0);
    update();
}

double RotorBandWidget::mitte() const
{
    if (m_stand >= 0.0) { return m_stand; }
    if (m_station >= 0.0) { return m_station; }
    return 0.0;
}

void RotorBandWidget::aktualisiereText()
{
    // Fuer Bedienungshilfen und den Tooltip: dasselbe in Worten.
    QStringList t;
    t << (m_stand >= 0.0 ? QStringLiteral("Rotor at %1°").arg(qRound(m_stand) % 360)
                         : QStringLiteral("No rotor reading"));
    if (m_station >= 0.0) {
        t << QStringLiteral("%1 %2°").arg(m_ziel ? QStringLiteral("turning to")
                                                 : QStringLiteral("station at"))
                                    .arg(qRound(m_station) % 360);
    }
    const QString s = t.join(QStringLiteral(", "));
    setToolTip(s);
    setAccessibleName(s);
}

void RotorBandWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(QPen(ton(Style::kBorder), 1));
    p.setBrush(ton(Style::kPanelBg));
    p.drawRoundedRect(r, 6, 6);

    const double mitteGrad = mitte();
    const double mitteX = r.center().x();
    const double proGrad = (r.width() - 16.0) / kSpanneGrad;
    const auto x = [&](double az) { return mitteX + abstand(mitteGrad, az) * proGrad; };
    const auto sichtbar = [&](double az) {
        return std::abs(abstand(mitteGrad, az)) <= kSpanneGrad / 2.0;
    };

    // Strahlkeil um die Nadel
    if (m_stand >= 0.0) {
        QColor keil = ton(m_farbe);
        keil.setAlpha(40);
        p.setPen(Qt::NoPen);
        p.setBrush(keil);
        p.drawRect(QRectF(x(m_stand) - m_strahl / 2.0 * proGrad, r.top() + 3,
                          m_strahl * proGrad, r.height() - 6));
    }

    // Teilung oben, Beschriftung unten. Keine Zahl unter der Nadel -- die
    // Ablesung steht als Text daneben.
    QFont f = font();
    f.setPixelSize(9);
    p.setFont(f);
    for (int a = 0; a < 360; a += 10) {
        if (!sichtbar(a)) { continue; }
        const double xx = x(a);
        const bool gross = a % 30 == 0;
        p.setPen(QPen(ton(gross ? Style::kTextScale : Style::kBorder), 1));
        p.drawLine(QPointF(xx, r.top() + 9), QPointF(xx, r.top() + (gross ? 16 : 13)));
        const bool unterNadel = m_stand >= 0.0 && std::abs(abstand(m_stand, a)) < 7.0;
        if (gross && !unterNadel) {
            const QString t = a == 0 ? QStringLiteral("N") : a == 90 ? QStringLiteral("E")
                            : a == 180 ? QStringLiteral("S") : a == 270 ? QStringLiteral("W")
                            : QString::number(a);
            p.setPen(ton(a % 90 == 0 ? Style::kTextPrimary : Style::kTextScale));
            p.drawText(QRectF(xx - 14, r.top() + 18, 28, 12), Qt::AlignCenter, t);
        }
    }

    // Station: Dreieck oben. Hohl = nur gezeigt, gefuellt = Drehziel.
    if (m_station >= 0.0 && sichtbar(m_station)) {
        const double xs = x(m_station);
        QPolygonF tri;
        tri << QPointF(xs - 5, r.top() + 2) << QPointF(xs + 5, r.top() + 2)
            << QPointF(xs, r.top() + 9);
        p.setPen(QPen(ton(Style::kAccent), 1.2));
        p.setBrush(m_ziel ? ton(Style::kAccent) : Qt::transparent);
        p.drawPolygon(tri);
    }

    // Nadel: wo er steht. Ohne Ablesung keine -- eine erfundene Lage
    // waere schlimmer als keine.
    if (m_stand >= 0.0) {
        p.setPen(QPen(ton(m_farbe), 2));
        p.drawLine(QPointF(x(m_stand), r.top() + 2), QPointF(x(m_stand), r.bottom() - 2));
    }
}

} // namespace Longpath
