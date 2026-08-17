// =================================================================
// src/gui/instruments/InstrumentPainter.cpp  (NereusSDR)
// =================================================================
// Siehe InstrumentPainter.h — die drei Mittel, an einer Stelle.
// =================================================================

#include "gui/instruments/InstrumentPainter.h"

#include "gui/StyleConstants.h"
#include "gui/instruments/InstrumentSpine.h"
#include "gui/instruments/ReadingSource.h"
#include "gui/styles/ThemeQss.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QRectF>

namespace NereusSDR {
namespace Instrument {

namespace {

// ── Die Zahlen aus den Entwürfen ─────────────────────────────────────
//
// ~/Downloads/zeiger-verfeinert.html, sofern nicht anders vermerkt.
// Keine davon ist frei gewählt; wer eine ändert, ändert die
// Handschrift aller drei Widgets auf einmal — und genau dafür stehen
// sie hier zusammen und nicht verstreut.

/// Die dunkle Kante über der Mulde. Entwurf: opacity ".75".
constexpr double kShadowOpacity = 0.75;

/// Der rote Abschnitt oberhalb der Schwelle. Entwurf: opacity ".42".
constexpr double kThresholdOpacity = 0.42;

/// Die helle Kante am Wert. Entwurf: stroke-width 1.4, opacity ".75".
constexpr double kValueEdgeOpacity = 0.75;
constexpr double kValueEdgeWidth   = 1.4;

/// Glut: 16 % in der Mitte, 5 % bei etwas über der halben Strecke,
/// null am Rand.
constexpr double kGlowMidStop     = 0.55;
constexpr double kGlowMidFraction = 0.05 / 0.16;   // relativ zur Mitte

/// Teilung. Entwurf: fein 1 px bei 55 % Deckung, beschriftet 1,2 px.
constexpr double kMinorTickLen     = 4.0;
constexpr double kMinorTickOpacity = 0.55;
constexpr double kMajorTickLen     = 8.0;
constexpr double kLabelOffset      = 22.0;

/// Rückfallwert für „measured-dim". StyleConstants.h führt ihn nicht;
/// HGauge.cpp:13 hält denselben Wert dateilokal.
constexpr auto kMeasuredDimFallback = Style::kAmberDim;

} // namespace

QColor measured()    { return QColor(Style::role("measured", Style::kAmberText)); }
QColor measuredDim() { return QColor(Style::role("measured-dim", kMeasuredDimFallback)); }
QColor danger()      { return QColor(Style::role("danger", Style::kGaugeDanger)); }

// ── Mulde ────────────────────────────────────────────────────────────

void paintTrough(QPainter& p, const Spine& s, double thresholdFraction)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(Qt::NoBrush);

    // Grund
    p.setPen(QPen(QColor(Style::role("inset-bg", Style::kInsetBg)),
                  s.troughWidth(), Qt::SolidLine, Qt::FlatCap));
    p.drawPath(s.troughPath());

    // Der rote Abschnitt liegt IN der Mulde, vor dem Innenschatten —
    // sonst überdeckte er die Kante, die über die ganze Rille läuft.
    if (thresholdFraction >= 0.0 && thresholdFraction <= 1.0) {
        QColor d = danger();
        d.setAlphaF(kThresholdOpacity);
        p.setPen(QPen(d, s.troughWidth(), Qt::SolidLine, Qt::FlatCap));
        p.drawPath(s.troughSpan(thresholdFraction, 1.0));
    }

    // Innenschatten an der oberen bzw. äusseren Kante
    QColor shadow(0, 0, 0);
    shadow.setAlphaF(kShadowOpacity);
    p.setPen(QPen(shadow, s.shadowWidth(), Qt::SolidLine, Qt::FlatCap));
    p.drawPath(s.shadowPath());

    // Aussenlinie, damit die leere Mulde nicht im Panelgrund verschwindet
    // — derselbe Befund wie bei HGauge (HGauge.cpp:60-63).
    p.setPen(QPen(QColor(Style::role("border", Style::kBorder)), 1.0));
    p.drawPath(s.troughPath());

    p.restore();
}

// ── Verlauf ──────────────────────────────────────────────────────────

void paintFade(QPainter& p, const Spine& s, double f, const QColor& c)
{
    if (f <= 0.0) { return; }
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(s.fadeBrush(c));
    p.drawPath(s.fillArea(f));
    p.restore();
}

// ── Glut ─────────────────────────────────────────────────────────────

void paintGlow(QPainter& p, const QRectF& bounds, const QPainterPath& clip,
               const QColor& c, double intensity)
{
    if (bounds.isEmpty()) { return; }
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    if (!clip.isEmpty()) { p.setClipPath(clip); }

    QRadialGradient g(bounds.center(), bounds.width() / 2.0);
    // Eine Ellipse statt eines Kreises, wo die Fläche nicht quadratisch
    // ist: der Verlauf wird über die Höhe gestreckt.
    if (!qFuzzyCompare(bounds.width(), bounds.height())) {
        g.setCoordinateMode(QGradient::ObjectBoundingMode);
        g.setCenter(0.5, 0.5);
        g.setFocalPoint(0.5, 0.5);
        g.setRadius(0.5);
    }
    QColor c0 = c; c0.setAlphaF(intensity);
    QColor c1 = c; c1.setAlphaF(intensity * kGlowMidFraction);
    QColor c2 = c; c2.setAlphaF(0.0);
    g.setColorAt(0.0,           c0);
    g.setColorAt(kGlowMidStop,  c1);
    g.setColorAt(1.0,           c2);

    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(g));
    p.drawEllipse(bounds);
    p.restore();
}

void paintGlow(QPainter& p, const Spine& s, const QColor& c, double intensity)
{
    paintGlow(p, s.glowBounds(), s.glowClip(), c, intensity);
}

// ── Helle Kante am Wert ──────────────────────────────────────────────

void paintValueEdge(QPainter& p, const Spine& s, double f, const QColor& c)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    QColor e = c;
    e.setAlphaF(kValueEdgeOpacity);
    p.setPen(QPen(e, kValueEdgeWidth));
    p.drawLine(s.crossAt(f, 0.0, 0.0));
    p.restore();
}

// ── Teilung ──────────────────────────────────────────────────────────

void paintTicks(QPainter& p, const Spine& s, const ReadingDescriptor& d)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor scale(Style::kTextScale);

    QColor minor = scale;
    minor.setAlphaF(kMinorTickOpacity);
    p.setPen(QPen(minor, 1.0));
    for (double v : d.minorTicks) {
        p.drawLine(s.tick(d.fraction(v), kMinorTickLen));
    }

    p.setPen(QPen(scale, 1.2));
    for (const ReadingTick& t : d.ticks) {
        p.drawLine(s.tick(d.fraction(t.value), kMajorTickLen));
    }

    // Die Teilung traegt Zahlen, also Monospace auf der Versalstufe.
    // Sie erbte bisher die Schrift des Aufrufers — damit sah dieselbe
    // Skala je nach Widget anders aus.
    p.setFont(Style::monoFont(p.font(), Style::kFontCaption));
    const QFontMetricsF fm(p.font());
    p.setPen(scale);
    for (const ReadingTick& t : d.ticks) {
        const QPointF a = s.labelAnchor(d.fraction(t.value), kLabelOffset);
        const qreal w = fm.horizontalAdvance(t.label);
        p.drawText(QPointF(a.x() - w / 2.0, a.y() + fm.ascent() / 2.0),
                   t.label);
    }
    p.restore();
}

// ── Farbe ────────────────────────────────────────────────────────────

QColor valueColour(const ReadingDescriptor& d, double value)
{
    if (d.threshold.has_value() && value > d.threshold.value()) {
        return danger();
    }
    return measured();
}

} // namespace Instrument
} // namespace NereusSDR
