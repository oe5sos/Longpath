#pragma once

// =================================================================
// src/core/strip/NoiseReduction.h  (Longpath)
// =================================================================
//
// Longpath-original. Entrauschen im SENDEWEG.
//
// ── Warum es diese Stufe gibt ────────────────────────────────────────
//
// Aus einem Paket von rund zwanzig Plugins, das der Betreiber aus der
// ESSB-Ecke mitbrachte (KC1EGU), waren FUENF Entrauscher. Alles
// andere darin -- Entzerrer, Kompressoren, Begrenzer, Anreicherer --
// hatte in unserer Kette längst eine Entsprechung. Die Entrauscher
// nicht. Das ist kein Zufall, sondern sagt, worauf es im Sendeton
// ankommt: ein Kompressor mit 10 dB Anhebung macht aus einem leisen
// Lüfter ein Rauschen, das mitsendet.
//
// ── Warum das Gate dafür nicht reicht ────────────────────────────────
//
// Das Gate duckt den Pegel ZWISCHEN den Wörtern. Es hört nicht auf,
// solange gesprochen wird -- und genau dann liegt das Raumgeräusch
// unter der Stimme und geht mit in die Kompression. Entrauschen nimmt
// das Geräusch auch waehrend des Sprechens heraus, weil es Geräusch
// und Stimme im Spektrum unterscheidet statt am Pegel.
//
// ── Was darunter arbeitet ────────────────────────────────────────────
//
// rnnoise (Xiph.Org, BSD 3-clause, Jean-Marc Valin) -- dasselbe Netz,
// das im EMPFANG als NR3 läuft, dort über WDSPs rnnr.c. Hier wird es
// direkt gerufen, weil der Kanalzug kein WDSP ist. Das Modell ist das
// mitgelieferte „Default (large)"; Herkunft und Lizenz stehen in
// docs/attribution/RNNOISE-PROVENANCE.md.
//
// ── Der Preis, und er ist nicht klein ────────────────────────────────
//
// rnnoise arbeitet in Blöcken von 480 Abtastwerten bei 48 kHz. Das
// sind **10 ms Laufzeit**, fest, nicht verhandelbar -- und beim
// Mithören hört man 10 ms. Deshalb:
//
//   * die Stufe ist AUS, bis sie jemand einschaltet,
//   * latencySamples() sagt, was sie kostet, und die Oberfläche
//     schreibt es hin,
//   * bei einer anderen Abtastrate als 48 kHz arbeitet sie NICHT.
//     Nicht „irgendwie": rnnoise ist auf 48 kHz trainiert, und
//     ungefragt umzutasten hiesse, ein Netz mit etwas zu füttern, auf
//     das es nicht trainiert ist.
//
// ── Mischung statt Schalter ──────────────────────────────────────────
//
// amount() mischt trocken und entrauscht. Ganz aufgedreht klingt
// rnnoise auf Sprache gelegentlich künstlich; bei 60-80 % ist das
// Geräusch weg und die Stimme noch ganz. Der trockene Anteil läuft
// durch dieselbe Verzögerung, sonst löschen sich die beiden
// gegenseitig aus.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-10 — Created in C++20 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Claude Code).
// =================================================================

#include <atomic>
#include <vector>

struct DenoiseState;

namespace Longpath {

class NoiseReduction {
public:
    NoiseReduction();
    ~NoiseReduction();

    NoiseReduction(const NoiseReduction&)            = delete;
    NoiseReduction& operator=(const NoiseReduction&) = delete;

    // Hauptfaden. Lädt beim ersten Mal das Modell. Bei einer anderen
    // Rate als 48 kHz bleibt die Stufe unbrauchbar und sagt das über
    // isUsable().
    void prepare(double sampleRate);

    void setEnabled(bool on) noexcept;
    bool isEnabled() const noexcept;

    // 0 = trocken, 1 = ganz entrauscht.
    void  setAmount(float a) noexcept;
    float amount() const noexcept;

    // Was die Stufe kostet, in Abtastwerten. 0, solange sie aus oder
    // unbrauchbar ist.
    int latencySamples() const noexcept;

    // Ob Modell und Rate stimmen. Falsch heisst: die Stufe reicht
    // durch, egal wie ihr Schalter steht.
    bool isUsable() const noexcept { return m_usable; }

    // Letzte Sprachwahrscheinlichkeit, die das Netz gemeldet hat
    // (0..1). Ein Instrument, keine Regelgroesse -- es zeigt, ob das
    // Netz die Stimme überhaupt findet.
    float voiceProbability() const noexcept;

    // Wieviel das Netz gerade wegnimmt, in dB (<= 0). Gemessen am
    // Block, nicht gerechnet.
    float reductionDb() const noexcept;

    // Audiofaden. Mono; alles andere reicht unveraendert durch.
    void process(float* samples, int frames, int channels) noexcept;

    void reset() noexcept;

private:
    void runFrame() noexcept;

    DenoiseState* m_state{nullptr};
    bool          m_usable{false};

    std::atomic<bool>  m_on{false};
    std::atomic<float> m_amount{0.8f};
    std::atomic<float> m_voiceProb{0.0f};
    std::atomic<float> m_reductionDb{0.0f};

    // 480 bei 48 kHz, von rnnoise vorgegeben.
    int m_frame{480};
    std::vector<float> m_in;      // füllt sich
    std::vector<float> m_wet;     // fertig entrauscht, wird ausgelesen
    std::vector<float> m_dry;     // derselbe Block unbehandelt
    int m_pos{0};
};

} // namespace Longpath
