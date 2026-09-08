#pragma once

// =================================================================
// src/gui/instruments/BarInstrument.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// ── Das Balkeninstrument ─────────────────────────────────────────────
//
// OE5SOS, 2026-08-17: „Dasselbe in der vorhandenen Balkenform als
// eigenes Widget. HGauge ist da; das Widget ist die Fassung drumherum
// mit Kopf und Fusszeile."
//
// ── Warum es nicht HGauge einbettet ──────────────────────────────────
//
// HGauge malt eine Mulde mit RANDLINIE und füllt sie in drei Zonen
// massiv aus (HGauge.cpp:60-115). Es kennt weder den Innenschatten
// noch den auslaufenden Verlauf noch die Glut — die drei Mittel, die
// dieses Widget mit dem Zeigerinstrument teilen soll.
//
// HGauge dafür umzubauen hiesse, zwanzig bestehende Gauges in zehn
// Applets auf einmal umzugestalten. Das ist eine sichtbare Änderung
// und gehört dem Betreiber, nicht diesem Auftrag. Also zeichnet dieses
// Widget über LinearSpine + InstrumentPainter — dieselbe Form wie
// HGauge, dieselbe Handschrift wie der Zeiger, und HGauge bleibt
// unangetastet.
//
// ── Eine Abweichung vom älteren Entwurf, bewusst ─────────────────────
//
// zeiger-verfeinert.html sagt im Fliesstext: „Der Balken bleibt, wie
// er ist" — also massive Füllung. Der Auftrag desselben Tages sagt
// darüber: „Gemeinsame Handschrift für alle drei — dieselben drei
// Mittel." Hier gilt der Auftrag, also auch beim Balken der
// auslaufende Verlauf. Genau so ein Punkt gehört auf den Schirm
// gesehen, bevor er stehen bleibt.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/instruments/PeakHold.h"

#include <QWidget>

namespace Longpath {

struct ReadingDescriptor;
class InstrumentFooter;

class BarInstrument : public QWidget {
    Q_OBJECT

public:
    explicit BarInstrument(QWidget* parent = nullptr);

    bool setPrimary(int bindingId);
    int  primary() const { return m_primary; }

    /// Die zweite Anzeige steht beim Balken als ZWEITE MULDE darunter,
    /// nicht als zweite Marke in derselben. Ein Balken hat keine
    /// Richtung, in die ein zweiter Zeiger ausweichen könnte.
    bool setSecondary(int bindingId);
    int  secondary() const { return m_secondary; }

    void onReading(int bindingId, double value);

    /// Zurück in den Zustand „keine Messung". Siehe
    /// NeedleInstrument::clearValue — dieselbe Regel, damit beide
    /// Formen dasselbe sagen.
    void clearValue();

    bool hasValue() const { return m_hasValue; }

    /// Die Spitzenhaltung — dieselbe Klasse wie beim Zeigerinstrument,
    /// damit eine Einstellung aus dem Rechtsklickmenue beide Ansichten
    /// erreicht und nicht nur die gerade sichtbare.
    // ── Segmentierte Form ────────────────────────────────────────────
    //
    // Der Betreiber, 2026-08-20, mit einem Bildschirmfoto aus Zeus:
    // das S-Meter als Kette einzelner Felder. Der Balken war bis dahin
    // ein durchgehender Verlauf.
    //
    // Beides bleibt: Segmente liest man als Menge, ohne die Skala zu
    // lesen; der Verlauf zeigt kleine Aenderungen genauer. Die Wahl
    // steht im Rechtsklickmenue des Instruments und wird dort gemerkt.
    void setSegmented(bool on);
    bool isSegmented() const { return m_segmented; }

    // ── Die Roehre ───────────────────────────────────────────────────
    //
    // Der Betreiber am 2026-08-23, nach sechs Tiefenbehandlungen auf
    // einem Vergleichsblatt: "baue D5 einmal mit SWR und mit
    // Stehwelle. danach als option bei den einzelnen widget hinzu."
    //
    // D5 war die Roehre: die Mulde traegt einen Querverlauf und
    // woelbt sich dem Auge entgegen, statt sich einzugraben. Sie ist
    // UNABHAENGIG von der Segmentierung — beides laesst sich
    // kombinieren, und die Kette in der Roehre ist die Fassung, die
    // Zeus am naechsten kommt.
    //
    // Wie die Segmentierung steht die Wahl im Rechtsklickmenue und
    // wird dort gemerkt.
    void setTube(bool on);
    bool isTube() const { return m_tube; }

    // ── Knappe Fassung ───────────────────────────────────────────────
    //
    // Fuer Balken, die als ZUSATZZEILE unter etwas anderem stehen —
    // etwa im Frequenz-Widget. Dort ist die Zeile rund 30 Punkte hoch,
    // und die Fusszeile ("Spitze 5 · Grenze 100") legt sich dann ueber
    // die Skalenzahlen. Auf dem Bildschirmfoto des Betreibers vom
    // 2026-08-23 ist das gut zu sehen: "Grenze 100" steht quer ueber
    // der 30 und der 60.
    //
    // Knapp heisst: keine Fusszeile. Die Zahl selbst steht ohnehin
    // rechts, und Spitze und Grenze liest man in einer Zusatzzeile
    // nicht — dafuer gibt es die eigenstaendige Anzeige.
    void setCompact(bool on);
    bool isCompact() const { return m_compact; }

    PeakHold&       peakHold()       { return m_peak; }
    const PeakHold& peakHold() const { return m_peak; }
    void resetPeak() { m_peak.reset(m_value); update(); }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    // Betreiber, 2026-08-30: das Frequenz-Fenster liess sich nicht so
    // weit ziehen, wie FrequencyInstrument allein erlaubt haette --
    // die dort eingebettete SWR-Zusatzzeile (ein gewoehnliches
    // BarInstrument, siehe FrequencyApplet.cpp) hatte mit 180 einen
    // hoeheren Bodenwert als die Ziffernzeile selbst und bestimmte
    // damit die tatsaechliche Grenze des ganzen Fensters. 90 ist die
    // kleinste Breite, bei der paintInto()s eigene Wache
    // (box.width() < 40.0) noch nicht zuschlaegt (90 - 24 Rand = 66).
    static constexpr int kMinWidth = 90;

public:
    /// Malt das Instrument fuer eine verlangte Groesse in einen fremden
    /// Maler. `bare` laesst die Fusszeile weg — fuer die Einblendung im
    /// Panadapter. Das Widget selbst wird dabei nicht angefasst.
    void paintInto(QPainter& painter, QSize forSize, bool bare);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void refreshFooter();
    void paintOne(class QPainter& p, const QRectF& area,
                  const ReadingDescriptor& d, double value,
                  bool withGlow, bool hasValue);

    InstrumentFooter* m_footer{nullptr};

    int    m_primary{-1};
    int    m_secondary{-1};
    double m_value{0.0};
    double m_secondValue{0.0};
    bool m_segmented{false};
    bool m_tube{false};
    bool m_compact{false};
    PeakHold m_peak;

    /// Ohne gültige Messung: kein Verlauf, keine Glut, keine Wertkante,
    /// Wert als Gedankenstrich. Mulde und Teilung bleiben. Begründung
    /// bei NeedleInstrument.
    bool m_hasValue{false};
    bool m_hasSecond{false};
};

} // namespace Longpath
