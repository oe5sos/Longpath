#pragma once
// no-port-check: Longpath-original, keine Thetis-Logik.
//
// =================================================================
// src/gui/spectrum/Durchlass.h  (Longpath)
// =================================================================
//
// ── Durchlass und Bandbreite (2026-11-08) ────────────────────────────
//
// Betreiber: „was koennen wir rein im Panadapter und im Bandwith
// aendern, optimieren, verbessern, einzigartig machen", dann an den
// Entwurfsblaettern: „Alle vier bauen":
//
//   1  Bandbreite ablesbar  -- Zahl am Durchlass, Kantenwerte beim Ziehen,
//                              Rast auf 50 Hz
//   5  Signal-Klammern      -- jedes Signal: belegte Breite und SNR
//   8  Durchlass-Lupe       -- +/-10 kHz um den VFO aus den vollen Bins
//   9  Lueckenfinder        -- freie Stellen zum CQ-Rufen
//
// Hier steht nur das, was ohne Fenster und ohne GPU rechnet: Erkennen,
// Messen, Formatieren. Gezeichnet wird in SpectrumWidget. So laesst sich
// jede Zusage in tst_durchlass pruefen, ohne einen Schirm.
//
// Vorbilder, nicht Vorlagen: Zeus (WidebandSignalDetector: Signal ab
// Rauschen + Schwelle, Bestaetigung vor dem Zeigen; ein eigener
// Bandbreiten-Spektrumkanal), AetherSDR (MiniPan, OccupiedRegion). Kein
// Code daraus uebernommen -- die Rechnung hier ist eigene und einfacher.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-08 — Neu, Martin Fischer (OE5SOS), AI-assisted via
//                Anthropic Claude Code.
// =================================================================

#include <QString>
#include <QVector>

namespace Longpath::Durchlass {

// ── 1  Bandbreite ablesbar ──────────────────────────────────────────

/// "2.70 kHz" ab 1 kHz, sonst "500 Hz". Die Oberflaeche ist englisch
/// (tst_oberflaeche_englisch), also Dezimalpunkt.
QString bandbreiteText(int loHz, int hiHz);

/// Kurzform fuer einen schmalen Durchlass neben der Marke: "2.7k",
/// "500". Mit Seitenband davor, wenn es eindeutig ist: "USB 2.7k".
QString bandbreiteKurz(int loHz, int hiHz);

/// "USB" wenn beide Kanten ueber dem Traeger liegen, "LSB" wenn beide
/// darunter, sonst leer (AM, FM, symmetrisch).
QString seitenband(int loHz, int hiHz);

/// Raster fuer das Ziehen einer Kante.
inline constexpr int kKantenRastHz = 50;

/// Auf das naechste Vielfache von `rastHz` runden.
int raste(int hz, int rastHz = kKantenRastHz);

// ── 5 und 9: Signale und Luecken auf den ANGEZEIGTEN Werten ────────

/// Ein erkanntes Signal, in Anzeigepunkten (Index in renderedPixels).
struct Signal {
    int    a{0};          ///< erster Punkt
    int    e{0};          ///< hinter dem letzten Punkt
    double mittePx{0.0};  ///< leistungsgewichtete Mitte
    float  spitzeDbm{0};  ///< hoechster Wert
    float  snrDb{0};      ///< Spitze minus Rauschboden
    double breiteHz{0};   ///< (e - a) * Hz je Punkt
};

/// Rauschboden = Median der angezeigten Werte. Ein Band voller Signale
/// hebt ihn kaum, solange weniger als die Haelfte belegt ist.
float rauschboden(const QVector<float>& px);

/// Signale: zusammenhaengende Punkte ueber Boden + minSnrDb. Luecken von
/// hoechstens `brueckePx` Punkten verbinden (eine SSB-Aussendung zerfaellt
/// sonst an den Silben in Stuecke).
QVector<Signal> erkenneSignale(const QVector<float>& px, double hzJePx,
                               float bodenDbm, float minSnrDb = 6.0f,
                               int brueckePx = 2);

/// Eine freie Stelle, in Anzeigepunkten.
struct Luecke {
    int a{0};
    int e{0};
};

/// Freie Stellen in [vonPx, bisPx): kein Punkt ueber Boden + freiDb,
/// nach Abzug von `schutzHz` je Seite noch mindestens `mindestHz` breit.
/// Einzelne Rauschspitzen (ein Punkt) zaehlen nicht als Belegung.
QVector<Luecke> findeLuecken(const QVector<float>& px, double hzJePx,
                             int vonPx, int bisPx, float bodenDbm,
                             double mindestHz = 3000.0,
                             double schutzHz = 500.0,
                             float freiDb = 4.0f);

/// Klammern halten still: ein Signal erscheint erst, wenn es in zwei
/// Auswertungen hintereinander da war, und verschwindet erst nach zwei
/// Auswertungen ohne es. Zeus macht dasselbe (confirm/hold), aus
/// demselben Grund: ein einzelnes Rauschbild soll keine Klammer zeigen.
class Haltung {
public:
    /// Neue Auswertung einspielen; liefert die zu zeigenden Signale.
    const QVector<Signal>& einspielen(const QVector<Signal>& neu);
    const QVector<Signal>& gezeigt() const { return m_gezeigt; }
    void leeren();

private:
    struct Eintrag {
        Signal s;
        int gesehen{0};
        int verpasst{0};
    };
    QVector<Eintrag> m_eintraege;
    QVector<Signal> m_gezeigt;
};

// ── 8  Lupe ─────────────────────────────────────────────────────────

/// Unter dieser Breite (logische Punkte) ist der Durchlass im Hauptbild
/// zu schmal, um darin etwas zu erkennen -- die Lupe erscheint (Modus
/// "auto"). 12, nicht 40: bei 40 stand sie schon in jeder gewoehnlichen
/// KW-Ansicht (192 kHz, 2,8 kHz Filter = 14 Punkte) und deckte ein
/// Viertel des Spektrums zu (tst_spectrum_trace_on_grid, 2026-11-08).
/// Gedacht ist sie fuer breite Bilder: 2 m mit 500 kHz (6 Punkte) oder
/// 4 MHz (1 Punkt).
inline constexpr int kLupeAbPunkten = 12;

/// Halbe Breite der Lupe.
inline constexpr double kLupeHalbHz = 10000.0;

/// Erster und letzter Bin (einschliesslich), die [vonHz, bisHz] eines
/// FFT abdecken, dessen erster Bin bei `fftUntenHz` beginnt. Auf den
/// vorhandenen Bereich geklemmt; leer (erster > letzter), wenn nichts
/// davon im FFT liegt.
struct BinBereich {
    int erster{0};
    int letzter{-1};
    bool leer() const { return letzter < erster; }
};
BinBereich lupenBins(int binZahl, double binHz, double fftUntenHz,
                     double vonHz, double bisHz);

} // namespace Longpath::Durchlass
