#pragma once

// =================================================================
// src/core/strip/LoudnessMatch.h  (Longpath)
// =================================================================
//
// Gleich laut vergleichen, sonst vergleicht man nichts.
//
// Am 2026-10-14 hat der Betreiber dreimal den Kopfhoerer abreissen
// muessen, und am Abend noch einmal: „bei dx punchy usw. ist schon
// wieder das total laute uebersteuern", „schon wieder bei fast
// allen", „muss sofort abdrehen und longpath zu machen".
//
// Es war kein Fehler in einer Voreinstellung. Es ist der Bau der
// Sache: JEDE ordentlich eingestellte Kette endet an der Decke des
// Begrenzers -- das ist ihre Aufgabe. „DX - punchy" legt zusaetzlich
// 12 dB Anhebung in den Clipper, ohne Pegelausgleich, und schaltet
// die Nachregelung des Kompressors ein. Das rohe Mikrofon liegt
// zwanzig bis vierzig Dezibel darunter. Wer zwischen beidem
// umschaltet, bekommt diesen Sprung ins Ohr -- jedes Mal.
//
// Und schlimmer als laut: der Vergleich ist wertlos. Lauter klingt
// immer besser. Solange die Kette lauter ist als das rohe Signal,
// beurteilt das Ohr die Lautstaerke und nicht die Bearbeitung.
//
// Also wird der MITHOERWEG angeglichen -- und nur er. Der Sendeton
// bleibt unberuehrt: dort gehoert die Dichte hin, dafuer ist sie
// gemacht. Gemessen wird der Effektivwert vor und nach der Kette,
// ueber 400 ms; der Ausgleich folgt langsam (1,5 s), damit man keine
// Regelung hoert, und ist nach unten auf -30 dB und nach oben auf
// +6 dB begrenzt. In der Stille bewegt sich nichts.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-14 — Original für Longpath von Martin Fischer,
//                 KI-gestützt über Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original.

#include "core/Loudness.h"

#include <atomic>

namespace Longpath {

class LoudnessMatch
{
public:
    /// Untergrenze des Ausgleichs (dB). Tiefer wird nicht gedaempft.
    static constexpr float kMinDb = -30.0f;
    /// Obergrenze (dB). Mehr wird nicht angehoben -- ein Ausgleich,
    /// der verstaerkt, holt vor allem das Grundgeraeusch mit.
    static constexpr float kMaxDb = 6.0f;
    /// Unterhalb dieses Effektivwerts am Eingang wird nicht geregelt.
    /// In der Stille gibt es nichts anzugleichen, und ein Verhaeltnis
    /// aus zwei Nullen ist keine Zahl.
    static constexpr float kStilleDbFs = -60.0f;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setEnabled(bool on) noexcept { m_on.store(on, std::memory_order_relaxed); }
    bool isEnabled() const noexcept { return m_on.load(std::memory_order_relaxed); }

    /// Den Pegel VOR der Kette mitnehmen.
    void feedInput(const float* samples, int frames) noexcept;

    /// Den Pegel NACH der Kette mitnehmen und den Ausgleich anwenden.
    /// Ist der Ausgleich aus, laeuft die Zahl sanft nach 0 dB zurueck,
    /// damit das Abschalten keinen Sprung macht.
    void applyOutput(float* samples, int frames) noexcept;

    /// Was gerade angewendet wird, in dB. Fuer Anzeige und Protokoll.
    float gainDb() const noexcept { return m_gainDb.load(std::memory_order_relaxed); }

private:
    std::atomic<bool>  m_on{true};
    std::atomic<float> m_gainDb{0.0f};

    // ── Gewichtet gemessen, nicht nur quadriert ──────────────────
    //
    // Die K-Gewichtung aus BS.1770 steht schon im Haus
    // (core/Loudness.h, gegen die veroeffentlichte 48-kHz-Tabelle
    // geprueft) -- also wird sie benutzt statt einer zweiten,
    // selbstgebauten Gewichtung. Der Unterschied ist nicht
    // akademisch: eine Kette, die Praesenz bei 3 kHz anhebt, wird
    // vom Ohr lauter gehoert, als ein blosser Effektivwert sagt, und
    // bliebe sonst nach dem Ausgleich die lautere von beiden.
    struct Zweipol {
        Loudness::Biquad k{};
        double x1{0.0}, x2{0.0}, y1{0.0}, y2{0.0};
        double schritt(double x) noexcept
        {
            const double y = k.b0 * x + k.b1 * x1 + k.b2 * x2
                           - k.a1 * y1 - k.a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
        void leeren() noexcept { x1 = x2 = y1 = y2 = 0.0; }
    };
    Zweipol m_kIn1, m_kIn2, m_kOut1, m_kOut2;

    double m_rate{48000.0};
    double m_msIn{0.0};      // geglaetteter Effektivwert im Quadrat
    double m_msOut{0.0};
    // ── Die Messung steigt schnell und faellt langsam ────────────
    //
    // Beide Seiten gleich, sonst verschiebt sich das Verhaeltnis beim
    // ersten Wort. Mit einer symmetrischen Glaettung ueber 400 ms
    // SIEHT der Ausgleich den Sprung erst nach 400 ms -- gemessen im
    // Pruefstand: 23,4 dB zu laut in den ersten hundert
    // Millisekunden, und genau diese hundert Millisekunden sind das,
    // was im Ohr weh tut. Steigend 20 ms, fallend 400 ms.
    double m_alphaAuf{0.0};
    double m_alphaAb{0.0};
    double m_alphaFolg{0.0};  // Glaettung nach OBEN (1,5 s)
    // ── Nach unten schnell, nach oben langsam ────────────────────
    //
    // Eine Voreinstellung wird in einem Augenblick gewaehlt, und ihr
    // Ausgang steht sofort an der Decke des Begrenzers. Braeuchte der
    // Ausgleich dafuer anderthalb Sekunden, waeren das anderthalb
    // Sekunden Vollpegel im Kopfhoerer -- genau das, wovor der
    // Betreiber am 2026-10-14 viermal den Kopfhoerer abgenommen hat.
    // Also faellt er in fuenfzig Millisekunden und steigt in
    // anderthalb Sekunden. Leiser darf schnell gehen; lauter nie.
    double m_alphaSchnell{0.0};
    float  m_gain{1.0f};     // linear, das was wirklich multipliziert wird
};

} // namespace Longpath
