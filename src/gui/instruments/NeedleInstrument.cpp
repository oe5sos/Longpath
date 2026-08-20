// =================================================================
// src/gui/instruments/NeedleInstrument.cpp  (NereusSDR)
// =================================================================
// Siehe NeedleInstrument.h. Fast alles Sichtbare kommt aus
// InstrumentPainter; hier stehen Zeiger, Nachlaufzeiger und die
// Umrechnung von Fenstergrösse auf Bogenmasse.
// =================================================================

#include "gui/instruments/NeedleInstrument.h"

#include "gui/instruments/InstrumentFooter.h"
#include "gui/instruments/InstrumentPainter.h"
#include "gui/instruments/InstrumentSpine.h"
#include "gui/instruments/ReadingSource.h"

#include <QPainter>
#include <QVBoxLayout>
#include "gui/StyleConstants.h"

namespace Longpath {

namespace {

// ── Bogenmasse ───────────────────────────────────────────────────────
//
// Der Entwurf zeichnet in ein 520 × 190 grosses Feld mit Drehpunkt
// (260, 168), Radius 148 und Rillenbreite 13 — also Drehpunkt knapp
// über der Unterkante, Radius etwa 0,285 der Breite, Rille 0,088 des
// Radius.
//
// Hier stehen nur noch die drei Masse des ZIFFERBLATTS. Alles, was
// darauf gezeichnet wird — Zeigerbreite, Nabe, Teilstriche, Abstände —
// hängt an Spine::unit() und wächst dort mit; siehe InstrumentSpine.h,
// Abschnitt „Der Massstab, in dem die Entwürfe notiert sind".
constexpr double kRadiusOfWidth   = 148.0 / 520.0;
constexpr double kTroughOfRadius  =  13.0 / 148.0;
constexpr double kPivotFromBottom =  22.0 / 190.0;   // 190 - 168

/// Der Sweep aus dem Entwurf: 168 Grad links, 12 Grad rechts.
constexpr double kDeg0 = 168.0;
constexpr double kDeg1 =  12.0;

} // namespace

NeedleInstrument::NeedleInstrument(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(10, 8, 14, 10);
    lay->setSpacing(0);
    lay->addStretch(1);                 // die Fläche, in die gemalt wird
    m_footer = new InstrumentFooter(this);
    lay->addWidget(m_footer, 0);
}

bool NeedleInstrument::setPrimary(int bindingId)
{
    const ReadingDescriptor* d = readingFor(bindingId);
    // Ohne belegte Skala kein Instrument. Lieber eine abgelehnte
    // Auswahl als ein Zifferblatt, dessen Bereich niemand verantwortet.
    if (!d || !d->hasScale) { return false; }
    m_primary = bindingId;
    // Kein Startwert. Ein Instrument, dem man die Grösse zuweist, hat
    // deswegen noch nichts gemessen.
    clearValue();
    return true;
}

bool NeedleInstrument::setSecondary(int bindingId)
{
    if (bindingId < 0) { m_secondary = -1; m_hasSecond = false; update(); return true; }
    const ReadingDescriptor* d = readingFor(bindingId);
    if (!d || !d->hasScale) { return false; }
    m_secondary = bindingId;
    m_hasSecond = false;
    update();
    return true;
}

void NeedleInstrument::clearValue()
{
    m_hasValue = false;
    m_hasSecond = false;
    m_peak.forget();
    refreshFooter();
    update();
}

void NeedleInstrument::setOffline(bool offline)
{
    if (m_offline == offline) { return; }
    m_offline = offline;
    update();
}

void NeedleInstrument::onReading(int bindingId, double value)
{
    if (bindingId == m_primary) {
        m_value = value;
        m_hasValue = true;
        m_peak.note(value);
        refreshFooter();
        update();
    } else if (bindingId == m_secondary) {
        m_secondValue = value;
        m_hasSecond = true;
        update();
    }
}

void NeedleInstrument::refreshFooter()
{
    const ReadingDescriptor* d = readingFor(m_primary);
    if (!d || !m_footer) { return; }
    m_footer->setCaption(d->thetisName());

    if (!m_hasValue) {
        // Gedankenstrich statt Zahl, und in der matten Farbe: ein
        // Strich ist keine Messung und soll auch nicht wie eine
        // aussehen. Die GRENZE bleibt stehen — sie ist eine
        // Eigenschaft der Skala und stimmt auch ohne Messung. Wenn du
        // sie auch weg willst, ist es eine Zeile.
        m_footer->setValueText(QStringLiteral("—"));
        m_footer->setValueColour(Instrument::measuredDim());
        m_footer->setPeakAndLimit(
            QStringLiteral("—"),
            d->threshold.has_value() ? d->text(d->threshold.value()) : QString());
        return;
    }

    m_footer->setValueText(d->unit.isEmpty()
                               ? d->text(m_value)
                               : QStringLiteral("%1 %2")
                                     .arg(d->text(m_value), d->unit));
    m_footer->setValueColour(Instrument::valueColour(*d, m_value));
    m_footer->setPeakAndLimit(
        m_peak.enabled() ? d->text(m_peak.value()) : QStringLiteral("—"),
        d->threshold.has_value() ? d->text(d->threshold.value()) : QString());
}

void NeedleInstrument::paintEvent(QPaintEvent*)
{
    const ReadingDescriptor* d = readingFor(m_primary);
    if (!d || !d->hasScale) { return; }

    const int footerH = m_footer ? m_footer->height() : 0;
    const QRectF face(0, 0, width(), qMax(0, height() - footerH));
    if (face.width() < 40.0 || face.height() < 30.0) { return; }

    const qreal radius = face.width() * kRadiusOfWidth;
    const qreal trough = radius * kTroughOfRadius;
    const QPointF pivot(face.center().x(),
                        face.bottom() - face.height() * kPivotFromBottom);

    ArcSpine spine(pivot, radius, trough, kDeg0, kDeg1);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const double f = d->fraction(m_value);
    const QColor col = Instrument::valueColour(*d, m_value);
    const double thresholdF = d->threshold.has_value()
                                  ? d->fraction(d->threshold.value())
                                  : -1.0;

    // ── Ohne Messung nur das Zifferblatt ─────────────────────────────
    //
    // Mulde und Teilung ja — sie sagen, WAS hier gemessen würde. Glut,
    // Sektor, Zeiger, Nachlaufzeiger und Wertkante nein: jedes davon
    // behauptet eine Zahl.
    if (!m_hasValue) {
        Instrument::paintTrough(p, spine, thresholdF);
        Instrument::paintTicks(p, spine, *d);
        // Ohne Verbindung ruht der Zeiger am Anschlag — matt, damit die
        // Ruhelage nicht wie ein gemessener Kleinstwert aussieht. Mit
        // Verbindung, aber ohne Messwert bleibt er weg: dort WAERE eine
        // Messung moeglich, und ein Zeiger behauptete sie.
        if (m_offline) {
            p.save();
            p.setOpacity(0.30);
            Instrument::paintNeedle(p, spine, 0.0,
                                    QColor(Style::kTextScale));
            p.restore();
        }
        // Die Nabe bleibt: sie ist der Drehpunkt des Instruments, kein
        // Messwert. Ein Zifferblatt ohne Nabe sähe unfertig aus statt
        // ruhig.
        Instrument::paintHub(p, spine);
        return;
    }

    // Reihenfolge wie im Entwurf: Glut ganz hinten, dann die Mulde,
    // dann der Sektor, dann Teilung, Zeiger und Kante.
    Instrument::paintGlow(p, spine, col);
    Instrument::paintTrough(p, spine, thresholdF);
    Instrument::paintFade(p, spine, f, col);
    Instrument::paintTicks(p, spine, *d);

    // Nachlaufzeiger: ein kurzer Strich am äusseren Rand, matt.
    if (m_peak.enabled() && m_peak.value() > m_value) {
        Instrument::paintPeakNeedle(p, spine, d->fraction(m_peak.value()),
                                    Instrument::measuredDim());
    }

    // Die zweite Anzeige — nur ein Zeiger, kein zweiter Sektor und
    // keine zweite Glut. Zwei auslaufende Sektoren übereinander werden
    // matschig, und die Glut hat nur einen Drehpunkt. Auch keine helle
    // Kante: sie sagt „HIER ist der Wert", und das gilt für einen.
    if (const ReadingDescriptor* d2 = m_hasSecond ? readingFor(m_secondary)
                                                  : nullptr) {
        if (d2->hasScale) {
            Instrument::paintNeedle(p, spine, d2->fraction(m_secondValue),
                                    Instrument::measuredDim(),
                                    Instrument::NeedleWeight::Secondary,
                                    /*withEdge=*/false);
        }
    }

    // Der Zeiger, und die helle Kante auf seiner vorderen Flanke.
    Instrument::paintNeedle(p, spine, f, col);

    // Die Nabe deckt den Zeigerfuss ab.
    Instrument::paintHub(p, spine);
}

} // namespace Longpath
