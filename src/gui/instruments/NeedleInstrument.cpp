// =================================================================
// src/gui/instruments/NeedleInstrument.cpp  (NereusSDR)
// =================================================================
// Siehe NeedleInstrument.h. Fast alles Sichtbare kommt aus
// InstrumentPainter; hier stehen Zeiger, Nachlaufzeiger und die
// Umrechnung von Fenstergrösse auf Bogenmasse.
// =================================================================

#include "gui/instruments/NeedleInstrument.h"
#include <cmath>
#include <QtMath>

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
// Derselbe Abstand, aber am RADIUS gemessen statt an der Feldhoehe.
// Der Entwurf ist 520 x 190 mit Radius 148 und 22 px unter dem
// Drehpunkt — also 22/148. Siehe die Notiz in paintEvent: an der
// Feldhoehe gemessen wandert der Bogen in einem hohen Feld nach oben
// und laesst darunter Luft stehen.
constexpr double kPivotOfRadius   =  22.0 / 148.0;
// Radius im Verhaeltnis zur Hoehe UEBER dem Drehpunkt. Im Entwurf
// liegt der Drehpunkt 168 px unter der Oberkante und der Radius ist
// 148 — der Bogen laesst also 20 px Luft. Dieses Verhaeltnis begrenzt
// den Radius, wenn das Feld breiter als hoch wird (2026-08-20).
constexpr double kRadiusOfPivotHeight = 148.0 / 168.0;

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

    // ── Der Bogen muss in BEIDE Richtungen passen ────────────────────
    //
    // Hier stand nur `face.width() * kRadiusOfWidth`. Solange das
    // Instrument die Breite der Applet-Spalte hatte (260 px), ging das
    // gut. Zieht man die Spalte breit — der Betreiber hatte am
    // 2026-08-20 gut 700 px — waechst der Radius mit, die Hoehe aber
    // nicht, und der Zeiger laeuft oben aus dem Feld heraus. Genau das
    // war auf seinem Bild zu sehen: „RX1 laesst sich zwar verkleinern,
    // der inhalt aendert sich aber nicht im massstab".
    //
    // Der Entwurf ist 520 x 190 mit Radius 148 und Drehpunkt 22 ueber
    // dem unteren Rand; senkrecht bleiben damit 168 - 148 = 20 px Luft.
    // Beide Verhaeltnisse werden gerechnet und das KLEINERE genommen —
    // so behaelt der Bogen seine Form und passt in jedes Seiten-
    // verhaeltnis, statt in der einen Richtung zu stimmen und in der
    // anderen hinauszulaufen.
    // ── Und er soll die Flaeche AUSFUELLEN ───────────────────────────
    //
    // Der Betreiber, 2026-08-20: „auch der swr zeiger sollte
    // flaechenfuellender sein."
    //
    // Der Drehpunkt hing an der HOEHE des Feldes
    // (face.height() * 22/190). In einem hohen Feld rutscht er damit
    // weit nach oben, der Bogen sitzt in der oberen Haelfte, und
    // darunter bleibt Luft, die niemand braucht. Auf seinem Bild war
    // das Feld rund 460 x 290; der Abstand unter dem Drehpunkt betrug
    // damit 34 px statt der 22 des Entwurfs.
    //
    // Jetzt haengt der Abstand am RADIUS: der Bogen behaelt sein
    // Verhaeltnis und sitzt immer gleich tief ueber dem unteren Rand,
    // egal wie hoch das Feld ist. Weil der Radius selbst aus der Hoehe
    // kommt, muss beides gemeinsam geloest werden — daher erst der
    // Radius aus der vollen Hoehe, dann der Drehpunkt daraus.
    // ── Aus der ECHTEN Bogenbreite, nicht aus einem Verhaeltnis ──────
    //
    // Hier stand `face.width() * 148/520` — die Zahlen des Entwurfs.
    // Der Bogen laeuft von 168° bis 12°, ist also symmetrisch um die
    // Senkrechte und 2·sin(78°) = 1,956 Radien BREIT. Mit dem festen
    // Verhaeltnis nahm er 55 % der Flaechenbreite ein und liess links
    // und rechts je gut 20 % leer.
    //
    // Der Betreiber, 2026-08-20, nach dem ersten Anlauf: „s meter und
    // swr noch immer nicht formatfuellend." Er hatte recht — ich hatte
    // nur den Drehpunkt nach unten geholt, die Breite aber weiter aus
    // dem Entwurfsverhaeltnis genommen.
    //
    // Jetzt umgekehrt gerechnet: aus der Flaeche wird der groesste
    // Radius bestimmt, dessen Bogen samt Mulde noch hineinpasst —
    // waagerecht UND senkrecht, das Kleinere gewinnt. Damit fuellt der
    // Zeiger jedes Format, statt in einem zu stimmen.
    const qreal halfSpanRad = qDegreesToRadians((kDeg0 - kDeg1) / 2.0);
    const qreal arcWidthPerRadius = 2.0 * std::sin(halfSpanRad);
    const qreal pad = 6.0;

    // Waagerecht: Bogenbreite + halbe Mulde auf jeder Seite.
    const qreal radiusByWidth =
        (face.width() - 2.0 * pad)
        / (arcWidthPerRadius + kTroughOfRadius);

    // Senkrecht: vom Drehpunkt bis zum Scheitel ein Radius, plus halbe
    // Mulde, plus der Abstand unter dem Drehpunkt.
    const qreal radiusByHeight =
        (face.height() - pad)
        / (1.0 + kTroughOfRadius / 2.0 + kPivotOfRadius);

    const qreal radius = qMin(radiusByWidth, radiusByHeight);
    const QPointF pivot(face.center().x(),
                        face.bottom() - radius * kPivotOfRadius);
    const qreal trough = radius * kTroughOfRadius;

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
