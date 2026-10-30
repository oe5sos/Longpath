#pragma once

// =================================================================
// src/core/ExternalAudioNr.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Rauschminderung für Ton, der schon demoduliert ankommt ──────────
//
// Der Betreiber am 2026-10-28, nachdem der NR-Knopf endlich die
// Rauschminderung der FLEX schaltete: „aber jeder nr sollte anders
// funktionieren! das ist bei anvelina ja schon!"
//
// Er hat recht, und der Grund ist eine Lücke in der Bauform. An der
// Anvelina kommt I/Q herein, läuft durch WDSP, und NR1..NR4 sind dort
// vier verschiedene Verfahren (anr, emnr, rnnoise, libspecbleach) —
// sie klingen verschieden, weil sie verschieden rechnen. Von der FLEX
// (und vom Kiwi, und über TCI) kommt fertiger TON. Der läuft nie durch
// WDSP, also gab es dort gar kein Verfahren; der Knopf schaltete nur
// den einen Schalter der Maschine, und drei Marken legten denselben um.
//
// Diese Stufe schliesst die Lücke für die Verfahren, die auf TON
// arbeiten können:
//
//   NR3   rnnoise         (Longpath::NoiseReduction, dieselbe Maschine
//                          wie im Sendezug — 48 kHz, 10-ms-Rahmen)
//   MNR   Apple Accelerate (MacNRFilter, nur auf dem Mac)
//
// NR1, NR2 und NR4 bleiben draussen, und das ist keine Faulheit: anr,
// emnr und sbnr sind WDSP-Einheiten INNERHALB eines Empfangskanals.
// Es gibt sie nicht als freistehenden Tonfilter, und einer dieser drei
// Namen auf einem fremden Algorithmus wäre eine Beschriftung, die
// nichts bedeutet. Wer NR1, NR2 oder NR4 waehlt, bekommt weiter die
// Rauschminderung der Maschine — die kann sie, und sie kostet nichts.
//
// ── Wo das sitzt ────────────────────────────────────────────────────
//
// In AudioEngine, direkt NACH dem Wandler auf 48 kHz und VOR
// `rxBlockReady`. Dort ist der Ton genau das, was beide Verfahren
// erwarten (48 kHz, Stereo, verschraenkt), und dort gilt er fuer jede
// fremde Quelle gleich — Flex wie Kiwi, ohne zweiten Weg daneben.
//
// ── Was es kostet ───────────────────────────────────────────────────
//
// Laufzeit: rnnoise rechnet in 10-ms-Rahmen, MacNRFilter mit
// 1024 Punkten ueberlappend (~21 ms). Beides sind Verzoegerungen, die
// ZUSAETZLICH zum Netzweg der Maschine anfallen; im Zweifel hoert man
// sie beim Mithoeren der eigenen Sendung, nicht beim Empfang.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-28 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include <memory>
#include <vector>

#include <QtGlobal>

#include "WdspTypes.h"

namespace Longpath {

class NoiseReduction;   // src/core/strip/NoiseReduction.h (rnnoise)
class MacNRFilter;      // src/core/MacNRFilter.h (Apple Accelerate)

/// Ein Rauschzweig je Scheibe für Ton aus fremder Demodulation.
///
/// Nicht threadsicher gegen gleichzeitige `setSlot`/`process`-Aufrufe
/// aus zwei Fäden: beide laufen im Tonweg derselben Scheibe, und der
/// ist einfädig. Was von der Oberfläche kommt, geht über die
/// Warteschlange von AudioEngine.
class ExternalAudioNr {
public:
    ExternalAudioNr();
    ~ExternalAudioNr();

    ExternalAudioNr(const ExternalAudioNr&) = delete;
    ExternalAudioNr& operator=(const ExternalAudioNr&) = delete;

    /// Welches Verfahren laufen soll. Ein Steckplatz, den diese Stufe
    /// nicht bedienen kann (NR1/NR2/NR4/Off), schaltet sie ab —
    /// `process` ist dann ein Durchlauf ohne Kosten.
    void setSlot(NrSlot slot);
    NrSlot slot() const { return m_slot; }

    /// Stärke, 0..1. Wird auf den Bereich des jeweiligen Verfahrens
    /// abgebildet (rnnoise 0..1, MacNRFilter 0..2 — hier bis 1, der
    /// Übersteuerbereich bleibt dem Sendezug).
    void setAmount(float amount);
    float amount() const { return m_amount; }

    /// True, wenn bei diesem Steckplatz wirklich etwas rechnet. Falsch
    /// heisst: Durchlauf. Die Oberfläche darf das fragen, bevor sie
    /// einen Knopf anbietet.
    bool isActive() const;

    /// 48 kHz, Stereo, verschränkt, an Ort und Stelle.
    void process(float* interleavedStereo, int frames);

    /// Zustand verwerfen — bei Bandwechsel, Stromabriss, Scheibentausch.
    void reset();

private:
    void baueAuf();

    NrSlot m_slot{NrSlot::Off};
    float  m_amount{1.0f};
    std::unique_ptr<NoiseReduction> m_rnnoise;
#ifdef HAVE_MNR
    std::unique_ptr<MacNRFilter>    m_mnr;
#endif
    // Zwischenpuffer fuer MacNRFilter: der will getrennte Kanaele,
    // der Tonweg traegt verschraenkte. Einmal angelegt, nicht je Block.
    std::vector<float> m_links;
    std::vector<float> m_rechts;
};

} // namespace Longpath
