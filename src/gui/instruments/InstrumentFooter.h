#pragma once

// =================================================================
// src/gui/instruments/InstrumentFooter.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// ── Eine Fusszeile für beide Instrumente ─────────────────────────────
//
// OE5SOS, 2026-08-17: „Die Fusszeile ist in beiden Instrumenten gleich
// aufgebaut, damit der Blick beim Wechsel nicht springt."
//
// Deshalb ein eigenes Widget und keine zweimal gebaute Zeile. Zwei
// Aufbauten, die gleich AUSSEHEN SOLLEN, sind zwei Aufbauten, die
// auseinanderlaufen werden — und der Sprung, den der Betreiber
// vermeiden will, ist genau ein Pixel Unterschied in der Grundlinie.
//
// Aufbau, aus dem Entwurf (zeiger-verfeinert.html, .foot):
//   links   die Grösse, Versalzeile in Skalenfarbe
//   mittig  Spitze und Grenze, klein und matt
//   rechts  der Wert, gross und in Messwertfarbe
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QWidget>

class QLabel;

namespace Longpath {

class InstrumentFooter : public QWidget {
    Q_OBJECT

public:
    explicit InstrumentFooter(QWidget* parent = nullptr);

    /// Die Grösse links, z. B. „Stehwelle".
    void setCaption(const QString& text);

    /// Der Wert rechts, schon formatiert.
    void setValueText(const QString& text);

    /// Die Farbe des Werts — sie wechselt mit Zeiger, Sektor und Glut
    /// auf danger, damit die Zahl nicht ruhig bleibt, während das
    /// Instrument warnt.
    void setValueColour(const QColor& c);

    /// Die mittlere Zeile. Leere Zeichenketten lassen den jeweiligen
    /// Teil weg — es gibt Grössen ohne Grenze.
    void setPeakAndLimit(const QString& peakText, const QString& limitText);

private:
    QLabel* m_caption{nullptr};
    QLabel* m_middle{nullptr};
    QLabel* m_value{nullptr};
};

} // namespace Longpath
