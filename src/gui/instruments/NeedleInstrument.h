#pragma once

// =================================================================
// src/gui/instruments/NeedleInstrument.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// ── Das Zeigerinstrument ─────────────────────────────────────────────
//
// OE5SOS, 2026-08-17:
//
//   „Zeigt eine oder zwei Anzeigen, Auswahl je Instrument aus derselben
//    Quelle wie die vorhandenen Gauges. Bogen mit Mulde, Sektor bis zum
//    Zeiger, Glut am Drehpunkt, blasser Nachlaufzeiger für die Spitze
//    der letzten Sekunden, feine Zwischenstriche zwischen den
//    beschrifteten. Über der Schwelle wechseln Zeiger, Sektor und Glut
//    gemeinsam auf danger; die Schwelle selbst bleibt eine harte Kante."
//
// Dieses Widget zeichnet fast nichts selbst. Mulde, Verlauf, Glut,
// Wertkante und Teilung kommen aus InstrumentPainter; die Geometrie
// aus ArcSpine; Bereich, Schwelle und Teilung aus ReadingSource. Was
// hier steht, sind Zeiger, Nachlaufzeiger und die Umrechnung von
// Fenstergrösse auf Bogenmasse.
//
// ── Zwei Anzeigen ────────────────────────────────────────────────────
//
// Die zweite bekommt einen Zeiger in measured-dim und SONST nichts:
// kein zweiter Sektor, keine zweite Glut. Zwei auslaufende Sektoren
// übereinander werden matschig, und die Glut hat nur einen Drehpunkt.
// Vorgeschlagen am 2026-08-17, vom Betreiber nicht widersprochen.
//
// ── Die Fusszeile ────────────────────────────────────────────────────
//
// Gleich aufgebaut wie beim Balkeninstrument, „damit der Blick beim
// Wechsel nicht springt": links die Grösse, mittig Spitze und Grenze,
// rechts der Wert. Sie steht in InstrumentFooter, damit die Gleichheit
// nicht von zwei Absichten abhängt.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QElapsedTimer>
#include <QWidget>

namespace NereusSDR {

struct ReadingDescriptor;
class InstrumentFooter;

class NeedleInstrument : public QWidget {
    Q_OBJECT

public:
    explicit NeedleInstrument(QWidget* parent = nullptr);

    /// Die Hauptanzeige — sie trägt Sektor, Glut und Schwelle.
    /// Kennungen ohne belegte Skala werden abgelehnt (false), statt
    /// ein Instrument ohne Bereich zu zeigen.
    bool setPrimary(int bindingId);
    int  primary() const { return m_primary; }

    /// Die zweite Anzeige, nur als Zeiger. -1 schaltet sie ab.
    bool setSecondary(int bindingId);
    int  secondary() const { return m_secondary; }

    /// Ein Messwert. Kennungen, die dieses Instrument nicht zeigt,
    /// werden verworfen — so kann das Widget direkt an
    /// MeterPoller::readingUpdated hängen.
    void onReading(int bindingId, double value);

    /// Zurück in den Zustand „keine Messung". Für den Fall, dass die
    /// Quelle versiegt — Funkgerät getrennt, Sender losgelassen. Wer
    /// das ruft, sagt damit: was hier stand, gilt nicht mehr.
    void clearValue();

    /// Liegt eine Messung vor? Ohne sie zeichnet das Instrument sein
    /// Zifferblatt und sonst nichts.
    bool hasValue() const { return m_hasValue; }

    QSize sizeHint() const override { return {320, 210}; }
    QSize minimumSizeHint() const override { return {200, 140}; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    /// Die Spitze der letzten Sekunden. Vergisst, was älter ist —
    /// ein Nachlaufzeiger, der alles behält, ist eine Grenzmarke und
    /// kein Nachlaufzeiger.
    void notePeak(double value);
    void refreshFooter();

    InstrumentFooter* m_footer{nullptr};

    int    m_primary{-1};
    int    m_secondary{-1};
    double m_value{0.0};
    double m_secondValue{0.0};
    double m_peak{0.0};
    QElapsedTimer m_peakAge;

    // ── Ohne Messung wird nicht gemessen ─────────────────────────────
    //
    // OE5SOS, 2026-08-17, nach dem ersten Blick auf den Schirm: die
    // Stehwelle stand ohne Verbindung auf 1.00, Spitze 1.00, und der
    // Zeiger lag am linken Anschlag. Das ist keine Anzeige einer
    // Ruhelage, das ist eine Behauptung: 1,00 ist ein hervorragendes
    // SWR, und ein Instrument, das ohne Messung ein hervorragendes
    // Ergebnis zeigt, lügt in die richtige Richtung — die gefährlichste.
    //
    // Ohne gültige Messung: kein Zeiger, kein Sektor, keine Glut, Wert
    // als Gedankenstrich. Das Zifferblatt bleibt stehen, damit man
    // sieht, WAS nicht gemessen wird.
    bool m_hasValue{false};
    bool m_hasSecond{false};

    /// Wie lange die Spitze stehen bleibt. „Der Spitze der letzten
    /// Sekunden" — drei ist die kleinste Zahl, die im Plural stimmt und
    /// lang genug für ein Sprachsignal.
    static constexpr qint64 kPeakHoldMs = 3000;
};

} // namespace NereusSDR
