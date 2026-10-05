#pragma once

// =================================================================
// src/core/audio/AudioRateMatcher.h  (Longpath)
// =================================================================
//
// Das Regelgesetz aus Thetis' `rmatch` — und NUR das.
//
// Hier ist absichtlich kein Resampler, kein Bus, kein Faden. Diese Datei
// beantwortet eine Frage: welches Abtastverhältnis gleicht die Uhrendrift
// zwischen Funkgerät und Tonkarte gerade aus? Die Verdrahtung in den
// Tonweg kommt getrennt zur Durchsicht — so verlangt es der Entwurf
// (docs/architecture/2026-09-27-rx-audio-rate-match-design.md, §4), und
// CLAUDE.md verlangt für Änderungen am Tonweg ohnehin den Betreiber.
//
// ── Das Problem, gemessen ────────────────────────────────────────────
//
// SunSDR2 QRP + MacBook Air, 2026-09-27: der Füllstand des Ausgangsrings
// steigt um rund 0,23 ms je Minute, also etwa 4 ppm. Zwei Quarze, die
// nie genau übereinstimmen. Mit dem 100-ms-Ring läuft er nach etwa fünf
// Stunden über; läuft das Gerät andersherum, läuft er nach derselben
// Zeit leer. Beides knackt.
//
// ── Herkunft ─────────────────────────────────────────────────────────
//
// Abgeleitet von WDSP/ChannelMaster `rmatch.c` (Warren Pratt, NR0V,
// GPL-2.0-or-later), `third_party/wdsp/src/rmatch.c`:
//
//   * `xmav`      — rmatch.c:56-69   (gleitender Mittelwert)
//   * `xaamav`    — rmatch.c:101-126 (vorzeichengetrennter Mittelwert)
//   * `control`   — rmatch.c:256-272 (das Regelgesetz selbst)
//   * Parameter   — rmatch.c:500-526 (create_rmatchV)
//   * Verstärkung — rmatch.c:147     (auf die Nennrate umgerechnet)
//
// ABWEICHUNG VON THETIS, ausdrücklich: Thetis ruft `control()` von BEIDEN
// Seiten auf und schützt `var` mit `cs_var` — also mit einer Sperre, die
// im Geräterückruf genommen wird. Longpath darf im Tonrückruf keine
// Sperre nehmen (CLAUDE.md), und sein Ring ist absichtlich sperrfrei.
// Darum läuft hier alles auf dem Erzeugerfaden: er meldet beide
// Richtungen (die verbrauchten Rahmen liest er am Lesezeiger des Busses
// ab), und `var` wird nur von ihm geschrieben und gelesen. Keine Sperre,
// kein Rückruf berührt diese Klasse.
//
// Eine Ungenauigkeit im Entwurfsdokument sei hier festgehalten: dort
// steht „moving-average in/out ratio · nom_out/nom_in". Die Quelle
// rechnet umgekehrt — `neg/pos` ist VERBRAUCHT/ERZEUGT, und multipliziert
// wird mit `inv_nom_ratio = nom_in/nom_out` (rmatch.c:261 mit :148).
// Maßgeblich ist die Quelle.
//
// =================================================================

#include <cstdint>
#include <vector>

namespace Longpath {

/// Gleitender Mittelwert über ganzzahlige Werte (`xmav`, rmatch.c:56-69).
///
/// Vor `ringMin` Werten liefert er den Nennwert statt eines Mittelwerts
/// aus zu wenigen Zahlen — eine Regelung, die auf drei Messwerte
/// anspringt, regelt auf Rauschen.
class GleitenderMittelwert {
public:
    GleitenderMittelwert(int ringMin, int ringMax, double nennwert);
    double schiebe(int wert);
    void   zuruecksetzen();

private:
    int    m_ringMin, m_ringMax, m_maske;
    double m_nennwert;
    std::vector<int> m_ring;
    std::int64_t m_summe{0};
    int    m_i{0}, m_geladen{0};
};

/// Vorzeichengetrennter Mittelwert (`xaamav`, rmatch.c:101-126).
///
/// Positive Meldungen (erzeugte Rahmen) und negative (verbrauchte) werden
/// GETRENNT summiert; die Ausgabe ist verbraucht/erzeugt. Genau daher
/// weiß die Regelung, in welche Richtung die Uhren auseinanderlaufen,
/// ohne irgendwo eine Zeit zu messen.
class VerhaeltnisMittelwert {
public:
    VerhaeltnisMittelwert(int ringMin, int ringMax, double nennVerhaeltnis);
    double schiebe(int wert);
    void   zuruecksetzen();

private:
    int    m_ringMin, m_ringMax, m_maske;
    double m_nennVerhaeltnis;
    std::vector<int> m_ring;
    std::int64_t m_pos{0}, m_neg{0};
    int    m_i{0}, m_geladen{0};
};

/// Das Regelgesetz (`control`, rmatch.c:256-272).
class AudioRateMatcher {
public:
    struct Einstellungen {
        int    nennRateEin{48000};
        int    nennRateAus{48000};

        // rmatch.c:500-526 — unverändert übernommen.
        int    ffRingMin{4096};
        int    ffRingMax{262144};      // Zweierpotenz!
        double ffAlpha{0.01};
        int    propRingMin{4096};
        int    propRingMax{16384};     // Zweierpotenz!
        // ABWEICHUNG VON THETIS, gemessen (2026-10-05). Thetis setzt hier
        // 4,0e-6 (rmatch.c:521). Mit dieser Zahl DIVERGIERT der Regelkreis
        // bei Longpaths Aufrufrate: in der Simulation ueber 5,5 Stunden
        // schwingt der Fuellstand ueber den GANZEN Ring (90 ms Hub bei
        // 100 ms Ring) und laeuft eine halbe Million Mal ueber -- auch bei
        // 0 ppm, wo nichts zu regeln ist.
        //
        // Mit 4,0e-7 trifft die Regelung den theoretischen Sollwert auf
        // neun Stellen, bei 0, +-4 und +-100 ppm, mit 0,3 ms Hub und ohne
        // einen einzigen Ueber- oder Leerlauf:
        //
        //     +4 ppm  -> var 0,999996010   (theoretisch 0,999996000)
        //   -100 ppm  -> var 1,000100009   (theoretisch 1,000100010)
        //
        // Der Prueftstand faehrt BEIDE Zahlen: die Thetis-Verstaerkung ist
        // dort die Gegenprobe, die ueberlaufen MUSS.
        //
        // Zweiter gemessener Befund, der nicht an der Verstaerkung haengt:
        // ab einer Blockgroesse von 2048 Rahmen wird es auch mit 4,0e-7
        // instabil. Bei einem 100-ms-Ring (4800 Rahmen) ist ein solcher
        // Block fast die halbe Ringgroesse -- ein einziger Schub bewegt
        // dann 43 % des Rings. Der Ring muss mehrere Bloecke fassen; das
        // ist eine Groessenfrage der Verdrahtung, keine der Regelung.
        double propVerstaerkung{4.0e-7};
        double anlaufSekunden{3.0};

        // rmatch.c:268-270
        double untereGrenze{0.96};
        double obereGrenze{1.04};
    };

    // Zwei Bauweisen statt eines Vorgabewerts `= {}`: der waere im noch
    // unvollstaendigen Klassenrumpf auszuwerten, und `Einstellungen` ist
    // dort gerade erst entstanden.
    AudioRateMatcher();
    explicit AudioRateMatcher(const Einstellungen& e);

    /// Eine Bewegung melden.
    ///
    /// `aenderung` ist POSITIV für erzeugte Rahmen (vor dem Resampeln,
    /// wie rmatch.c:359) und NEGATIV für verbrauchte (rmatch.c:464).
    /// `ringFuellung` und `ringGroesse` sind in Rahmen; geregelt wird auf
    /// die halbe Ringgröße, wie in Thetis (`rsize/2`).
    void melde(int aenderung, std::int64_t ringFuellung, std::int64_t ringGroesse);

    /// Das Verhältnis, mit dem resampelt werden soll. 1,0 bis die
    /// Anlaufzeit vorbei ist.
    double verhaeltnis() const { return m_var; }

    /// Geregelt wird erst nach der Anlaufzeit — vorher sagt das hier false.
    bool regeltSchon() const { return m_regelt; }

    double vorsteuerung() const { return m_vorsteuerung; }
    double mittlereAbweichung() const { return m_mittlereAbweichung; }

    void zuruecksetzen();

private:
    Einstellungen m_e;
    double m_nennVerhaeltnis{1.0};
    double m_invNennVerhaeltnis{1.0};
    double m_prVerstaerkung{4.0e-6};

    VerhaeltnisMittelwert m_ffMav;
    GleitenderMittelwert  m_propMav;

    double m_vorsteuerung{1.0};
    double m_mittlereAbweichung{0.0};
    double m_var{1.0};

    std::int64_t m_erzeugteRahmen{0};
    std::int64_t m_anlaufRahmen{0};
    bool   m_regelt{false};
};

}  // namespace Longpath
