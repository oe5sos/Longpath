#pragma once

// =================================================================
// src/core/audio/RxRatenAngleich.h  (Longpath)
// =================================================================
//
// Der Driftausgleich am Empfangston — die Verdrahtung.
//
// Das Regelgesetz steht in `AudioRateMatcher` (portiert aus WDSP
// `rmatch.c`). Hier wird es an den Tonweg gehängt: Füllstand ablesen,
// Verhältnis holen, Rahmen damit umtasten, und das Ergebnis in den Bus
// schieben.
//
// ── Warum überhaupt ──────────────────────────────────────────────────
//
// Funkgerät und Tonkarte haben zwei Quarze. Gemessen am 2026-09-27
// (SunSDR2 QRP + MacBook Air): der Ausgangsring füllt sich um 0,23 ms je
// Minute, also rund 4 ppm. Mit dem 100-ms-Ring läuft er nach etwa fünf
// Stunden über; läuft das Gerät andersherum, läuft er nach derselben
// Zeit leer. Beides knackt.
//
// ── Die Entscheidungen des Betreibers (2026-10-06) ───────────────────
//
//   * Zielverzögerung: der HALBE Ring, wie bei Thetis. `rmatch` regelt
//     auf `rsize/2`, und das ist dort die eingestellte Latenz.
//   * NUR die Lautsprecher, nicht der Mithörweg. Der hat im
//     `MasterMixer` schon ein eigenes nachgeführtes Polster; zwei
//     Regelungen auf demselben Strom arbeiten gegeneinander. Und er
//     läuft nur beim Senden — 0,23 ms je Minute sammeln sich in einem
//     Überzug nicht an. Drift ist ein Problem von Stunden.
//
// ── Was NICHT passiert ───────────────────────────────────────────────
//
// Keine Sperre, kein Rückruf. Alles hier läuft auf dem Erzeugerfaden,
// vor `IAudioBus::push()`. Thetis nimmt `cs_var` im Geräterückruf;
// CLAUDE.md verbietet das, und Longpaths Ring ist absichtlich sperrfrei.
//
// Meldet der Bus `queuedFrames() == -1` (Füllstand unbekannt — HAL-Shm,
// PipeWire, FIFO), gehen die Rahmen UNVERÄNDERT durch. Ein Regler ohne
// Messgröße ist keiner.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-06 — Created in C++20/Qt6 for Longpath, operator Martin
//                 Fischer (OE5SOS), AI-assisted via Anthropic Claude
//                 Code. Benutzt WDSP `varsamp` (Warren Pratt, NR0V,
//                 GPL-2.0-or-later) als Umtaster, mit denselben
//                 Parametern, die `rmatch.c:142-143` ihm gibt.
// =================================================================

#include "core/audio/AudioRateMatcher.h"

#include <QtGlobal>

#include <vector>

namespace Longpath {

class RxRatenAngleich {
public:
    /// Zielverzoegerung: der HALBE Ring, wie bei Thetis -- 50 ms.
    /// Betreiberentscheidung 2026-10-06 ("1. wie thetis").
    static constexpr int kZielMs = 50;

    /// Die Ringgroesse, auf deren HAELFTE geregelt wird, in Rahmen.
    ///
    /// Bewusst aus der Zielverzoegerung gerechnet und NICHT beim Bus
    /// erfragt: die 50 ms sind die Entscheidung, nicht die Baugroesse
    /// eines bestimmten Hintergrunds. `PortAudioBus` haelt 100 ms, also
    /// faellt beides zusammen; ein Bus mit anderem Ring bekaeme sonst
    /// stillschweigend eine andere Verzoegerung.
    static constexpr qint64 ringRahmenFuer(int rate)
    {
        return (rate > 0 ? rate : 48000) * 2LL * kZielMs / 1000;
    }

    /// `rate` ist die Ausgaberate, `kanaele` 1 oder 2.
    RxRatenAngleich(int rate, int kanaele);
    ~RxRatenAngleich();

    RxRatenAngleich(const RxRatenAngleich&)            = delete;
    RxRatenAngleich& operator=(const RxRatenAngleich&) = delete;

    /// Rahmen durchreichen und dabei die Drift ausgleichen.
    ///
    /// `fuellungRahmen` ist `IAudioBus::queuedFrames()` (-1 = unbekannt),
    /// `ringRahmen` die Gesamtgroesse des Busringes in Rahmen; geregelt
    /// wird auf dessen HAELFTE.
    ///
    /// Gibt einen Zeiger auf die Ausgaberahmen und ihre Anzahl zurueck.
    /// Bei unbekanntem Fuellstand ist das der EINGABEzeiger selbst --
    /// dann wird nichts kopiert und nichts gerechnet.
    struct Ausgabe { const float* rahmen; int anzahl; };
    Ausgabe verarbeite(const float* ein, int rahmen,
                       qint64 fuellungRahmen, qint64 ringRahmen);

    double verhaeltnis() const { return m_regler.verhaeltnis(); }
    bool   regeltSchon() const { return m_regler.regeltSchon(); }
    /// Wie viele Rahmen der Umtaster insgesamt mehr (oder weniger)
    /// ausgegeben hat, als hineingingen. Fuer die Diagnose.
    qint64 versatz() const { return m_versatz; }

    void zuruecksetzen();

private:
    void baueUmtaster(int blockRahmen);

    int    m_rate;
    int    m_kanaele;
    int    m_blockRahmen{0};
    void*  m_varsamp{nullptr};        // VARSAMP, ohne WDSP-Kopf hier
    std::vector<double> m_ein;        // komplex verschachtelt (2 je Rahmen)
    std::vector<double> m_aus;
    std::vector<float>  m_ausFloat;
    AudioRateMatcher m_regler;
    qint64 m_versatz{0};
    // Fuellstand und Ausgabe des letzten Durchlaufs. Daraus liest der
    // Erzeuger ab, wie viele Rahmen das Geraet inzwischen verbraucht
    // hat -- ohne dass der Rueckruf mitzaehlen oder eine Sperre nehmen
    // muesste. -1 heisst: noch kein Durchlauf.
    qint64 m_letzteFuellung{-1};
    int    m_letzteAusgabe{0};
};

}  // namespace Longpath
