#pragma once
// no-port-check: Longpath-original, keine Thetis-Logik.
//
// =================================================================
// src/gui/spectrum/Farbschema.h  (Longpath)
// =================================================================
//
// ── Farbschemata des Panadapters (2026-11-21) ────────────────────────
//
// Betreiber zu den Entwurfsblaettern werkzeug/entwuerfe-pan-ft8-look-
// 2026-11-21: „s2,s4,s5 - alles als option!", „na dann nehme dazu" fuer
// S1/S3, „vor allem in der bandwidth", „es sollte oben und unten
// getrennt sein", und zuletzt „es sollte nicht beim senden sein, sondern
// generell, also nicht wie bei ft8".
//
// Ein Schema faerbt den Durchlass -- im Panadapter in 2D UND 3D, im
// Empfang und beim Senden, und in den Bandbreiten-Anzeigen (Fenster
// BANDWIDTH FILTER und der Filter im RX-Teil). Der Wasserfall hat eine
// eigene, getrennte Wahl. Die Spur bleibt Messwert; nur Night dimmt sie.
//
// Hier stehen nur die Tabelle der Durchlassfarben und ein Bote: Pan 0
// meldet sein Spektrumschema, die Bandbreiten-Anzeigen hoeren darauf --
// sie gehoeren keinem Pan und kennen kein SpectrumWidget.

#include <QColor>
#include <QObject>

namespace Longpath {

// Als int gespeichert (DisplaySpectrumScheme / DisplayWaterfallScheme;
// das fruehere gemeinsame DisplayColourScheme wird beim Laden fuer beide
// uebernommen), also nur anhaengen. Die Reihenfolge im Menue steht davon
// getrennt in PanadapterApplet.
enum class PanColourScheme : int {
    Standard = 0,
    Brass,       // S2 Messing, gedaempft (Wasserfall = Muted)
    SlateBlue,   // S4 Graublau, ruhig
    Night,       // S5 Nacht
    Ft8Amber,    // S1 FT8 Bernstein (Betreiber: „na dann nehme dazu")
    Teal,        // S3 Tuerkis-Gruen
    Count
};

namespace Farbschema {

// Durchlass eines Schemas: Flaeche als Verlauf (unten alphaUnten, oben
// alphaOben) und eine Kantenfarbe. Bei Standard ungueltig (`gilt` false):
// dann malt jede Anzeige ihren bisherigen Durchlass.
struct Durchlass {
    bool   gilt{false};
    QColor flaeche;
    float  alphaUnten{0.0f};
    float  alphaOben{0.0f};
    QColor kante;
};

Durchlass durchlassFuer(PanColourScheme s);

// Pan 0 meldet hier sein Spektrumschema; die Bandbreiten-Anzeigen hoeren
// darauf. Ein Bote, kein Zustand der Anzeigen: wer spaeter entsteht,
// fragt bandbreite() und ist sofort richtig.
//
// Dazu eine eigene Wahl fuer die Bandbreiten-Anzeigen (2026-11-21,
// Betreiber im Fenster BANDWIDTH FILTER: „die option fuer den bandwidth
// sollte auch in diesem fenster sein"): kFolgtPanadapter (Vorgabe) heisst
// „wie der Panadapter", sonst ein festes Schema. Gespeichert unter
// BandwidthPassbandScheme.
class Bote : public QObject {
    Q_OBJECT
public:
    static constexpr int kFolgtPanadapter = -1;

    static Bote& instance();
    PanColourScheme durchlass() const { return m_durchlass; }
    void setDurchlass(PanColourScheme s);

    /// Die eigene Wahl der Bandbreiten-Anzeigen (kFolgtPanadapter oder
    /// ein PanColourScheme als int).
    int bandbreitenWahl() const { return m_bandbreitenWahl; }
    void setBandbreitenWahl(int wahl);
    /// Was die Bandbreiten-Anzeigen malen: die eigene Wahl, sonst das
    /// Spektrumschema von Pan 0.
    PanColourScheme bandbreite() const;
signals:
    void durchlassGeaendert(PanColourScheme s);
    void bandbreiteGeaendert(PanColourScheme s);
private:
    Bote();
    PanColourScheme m_durchlass{PanColourScheme::Standard};
    int m_bandbreitenWahl{kFolgtPanadapter};
};

}  // namespace Farbschema
}  // namespace Longpath
