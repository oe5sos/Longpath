// =================================================================
// src/gui/instruments/BarInstrument.cpp  (NereusSDR)
// =================================================================
// Siehe BarInstrument.h.
// =================================================================

#include "gui/instruments/BarInstrument.h"

#include "gui/instruments/InstrumentFooter.h"
#include "gui/instruments/InstrumentPainter.h"
#include "gui/instruments/InstrumentSpine.h"
#include "gui/instruments/ReadingSource.h"

#include <QPainter>
#include <QVBoxLayout>

namespace Longpath {

namespace {
/// Höhe der Mulde. Entwurf: H = 20 bei einem 300 breiten Feld.
constexpr double kTroughHeight = 20.0;
/// Platz unter der Mulde für die Beschriftung der Teilung.
constexpr double kScaleHeight  = 18.0;
/// Abstand zwischen zwei Mulden, wenn eine zweite Anzeige läuft.
constexpr double kGap          = 6.0;
} // namespace

BarInstrument::BarInstrument(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(10, 8, 14, 10);
    lay->setSpacing(0);
    lay->addStretch(1);
    m_footer = new InstrumentFooter(this);
    lay->addWidget(m_footer, 0);
}

bool BarInstrument::setPrimary(int bindingId)
{
    const ReadingDescriptor* d = readingFor(bindingId);
    if (!d || !d->hasScale) { return false; }
    m_primary = bindingId;
    // Kein Startwert — siehe NeedleInstrument::setPrimary.
    clearValue();
    return true;
}

bool BarInstrument::setSecondary(int bindingId)
{
    if (bindingId < 0) { m_secondary = -1; m_hasSecond = false; update(); return true; }
    const ReadingDescriptor* d = readingFor(bindingId);
    if (!d || !d->hasScale) { return false; }
    m_secondary = bindingId;
    m_hasSecond = false;
    update();
    return true;
}

void BarInstrument::clearValue()
{
    m_hasValue = false;
    m_hasSecond = false;
    m_peak.forget();
    refreshFooter();
    update();
}

void BarInstrument::onReading(int bindingId, double value)
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

void BarInstrument::refreshFooter()
{
    // Wortgleich mit NeedleInstrument::refreshFooter — dieselbe
    // Fusszeile, dieselben Felder, damit der Blick beim Umschalten
    // nicht springt.
    const ReadingDescriptor* d = readingFor(m_primary);
    if (!d || !m_footer) { return; }
    m_footer->setCaption(d->thetisName());

    if (!m_hasValue) {
        // Wortgleich mit NeedleInstrument::refreshFooter — dieselbe
        // Fusszeile sagt in beiden Formen dasselbe.
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

void BarInstrument::setSegmented(bool on)
{
    if (m_segmented == on) { return; }
    m_segmented = on;
    update();
}

void BarInstrument::setTube(bool on)
{
    if (m_tube == on) { return; }
    m_tube = on;
    update();
}

void BarInstrument::setCompact(bool on)
{
    if (m_compact == on) { return; }
    m_compact = on;
    if (m_footer) { m_footer->setVisible(!m_compact); }
    updateGeometry();
    update();
}

void BarInstrument::paintOne(QPainter& p, const QRectF& area,
                             const ReadingDescriptor& d, double value,
                             bool withGlow, bool hasValue)
{
    LinearSpine spine(QRectF(area.left(), area.top(), area.width(),
                             kTroughHeight));

    const double f = d.fraction(value);
    const QColor col = Instrument::valueColour(d, value);

    const double thresholdF = d.threshold.has_value()
                                  ? d.fraction(d.threshold.value())
                                  : -1.0;

    // Ohne Messung nur Mulde und Teilung — kein Verlauf, keine Glut,
    // keine Wertkante. Sie behaupten jede eine Zahl.
    // Die Mulde: flach oder als Roehre. Beide Aufrufe muessen
    // mitgehen — die leere Mulde und die gefuellte sind dasselbe
    // Instrument, und eine Roehre, die erst beim ersten Messwert
    // erscheint, waere ein Fehler, den man nur im Betrieb sieht.
    auto trough = [&] {
        if (m_tube) { Instrument::paintTroughTube(p, spine, thresholdF); }
        else        { Instrument::paintTrough(p, spine, thresholdF); }
    };

    if (!hasValue) {
        trough();
        Instrument::paintTicks(p, spine, d);
    } else {
        if (withGlow) { Instrument::paintGlow(p, spine, col); }
        trough();
        if (m_segmented) {
            // Segmente tragen ihre Kante selbst — eine zusaetzliche
            // Wertkante am Ende saehe aus wie ein halbes Feld.
            Instrument::paintSegments(p, spine, f, col);
            Instrument::paintTicks(p, spine, d);
        } else {
            Instrument::paintFade(p, spine, f, col);
            Instrument::paintTicks(p, spine, d);
            Instrument::paintValueEdge(p, spine, f, col);
        }

        // Die Spitze, wie bei Zeus: ein heller Strich, der stehen
        // bleibt und langsam zurueckfaellt. Nur wenn sie ueberhaupt
        // vor dem Wert liegt — sonst zeichnete sie eine zweite Kante
        // auf die erste.
        const double pf = d.fraction(m_peak.value());
        if (pf > f + 0.005) { Instrument::paintPeakMark(p, spine, pf); }
    }

    // Die Schwellenmarke steht ÜBER der Mulde hinaus, damit sie auch
    // dann zu sehen ist, wenn der Verlauf noch weit davor endet.
    if (d.threshold.has_value()) {
        QColor dg = Instrument::danger();
        dg.setAlphaF(0.85);
        p.setPen(QPen(dg, 1.0));
        p.drawLine(spine.crossAt(d.fraction(d.threshold.value()), 3.0, 3.0));
    }
}

void BarInstrument::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    paintInto(p, size(), false);
}

// ── Malen fuer die Einblendung, ohne das Widget anzufassen ──────────
//
// renderTransparent hat frueher das LEBENDE Instrument auf die
// gewuenschte Groesse umgestellt und danach zurueck — mit dem
// Kommentar, die Spalte merke davon nichts. Sie merkt es sehr wohl:
// ein resize() auf ein Widget in einem Layout ist ein Eingriff, den
// das Layout beantwortet, und die Einblendung malt alle 500 ms neu.
// Beim Rotor hat genau das die Rose plattgedrueckt (2026-08-21,
// Bildschirmfoto: „den rotor bitte links wieder einblenden").
//
// Hier stand derselbe Fehler, nur noch unentdeckt. paintInto() malt
// fuer eine verlangte Groesse in einen fremden Maler; das Widget
// bleibt, wie es ist.
void BarInstrument::paintInto(QPainter& painter, QSize forSize, bool bare)
{
    const ReadingDescriptor* d = readingFor(m_primary);
    if (!d || !d->hasScale) { return; }

    const int footerH = (bare || !m_footer) ? 0 : m_footer->height();
    const QRectF box(10.0, 8.0, qMax(0.0, forSize.width() - 24.0),
                     qMax(0.0, forSize.height() - footerH - 18.0));
    if (box.width() < 40.0 || box.height() < kTroughHeight) { return; }

    QPainter& p = painter;
    p.setRenderHint(QPainter::Antialiasing, true);

    const ReadingDescriptor* d2 = readingFor(m_secondary);
    const bool two = (d2 && d2->hasScale);

    paintOne(p, box, *d, m_value, /*withGlow=*/true, m_hasValue);
    if (two) {
        // Zweite Mulde darunter. Ohne Glut: sie hätte keinen eigenen
        // Ort, an dem sie sitzen könnte, und zwei Glutflächen
        // übereinander heben sich gegenseitig auf.
        const QRectF lower(box.left(),
                           box.top() + kTroughHeight + kScaleHeight + kGap,
                           box.width(), kTroughHeight);
        if (lower.bottom() <= box.bottom() + kTroughHeight) {
            paintOne(p, lower, *d2, m_secondValue, /*withGlow=*/false,
                     m_hasSecond);
        }
    }
}

} // namespace Longpath
