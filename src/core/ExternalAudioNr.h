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
//   NR2   WDSP-emnr       (freistehend, siehe
//   NR4   WDSP-sbnr        third_party/wdsp/src/longpath_extnr.c)
//   NR3   rnnoise         (Longpath::NoiseReduction, dieselbe Maschine
//                          wie im Sendezug — 48 kHz, 10-ms-Rahmen, mono)
//   MNR   Apple Accelerate (MacNRFilter, nur auf dem Mac)
//
// Alle rechnen auf (L+R)/2 oder je Kanal und liefern beide Kanaele.
// NR1 (anr) bleibt draussen: wer NR1 waehlt, bekommt die
// Rauschminderung der Maschine.
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
//   2026-11-24 — NR2 und NR4 dazu. Martin Fischer, AI-assisted via
//                 Anthropic Claude Code.
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
    /// nicht bedienen kann (NR1/Off), schaltet sie ab —
    /// `process` ist dann ein Durchlauf ohne Kosten.
    void setSlot(NrSlot slot);
    NrSlot slot() const { return m_slot; }

    /// Stärke, 0..1. Wird auf den Bereich des jeweiligen Verfahrens
    /// abgebildet (rnnoise 0..1, MacNRFilter 0..2 — hier bis 1, der
    /// Übersteuerbereich bleibt dem Sendezug).
    void setAmount(float amount);
    float amount() const { return m_amount; }

    /// NR2 (WDSP-emnr): Gewinn-Methode 0..3, Rauschschaetzer 0..2, AE-Filter.
    /// Gilt sofort, auch im Lauf.
    void setNr2(int gainMethod, int npeMethod, bool aeFilter);

    /// NR4 (WDSP-sbnr, libspecbleach): Reduktion 0..20 dB, Glaettung und
    /// Weissung 0..100 %, Neuskalierung, Nachfilter-Schwelle, Skalierungsart.
    void setNr4(float reductionDb, float smoothing, float whitening,
                float rescale, float postThreshold, int scalingType);

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
    void laufeBlockweise(float* interleavedStereo, int frames, void* handle);

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

    // NR2/NR4: dieselben WDSP-Einheiten wie im Empfangskanal der Anvelina,
    // freistehend (third_party/wdsp/src/longpath_extnr.c). Opak, damit der
    // Kopf kein WDSP-C-Header zieht. Sie rechnen in festen Bloecken; der
    // Tonweg liefert beliebige Laengen, also FIFO davor und dahinter.
    void* m_emnr{nullptr};
    void* m_sbnr{nullptr};
    int   m_nr2Gain{2};
    int   m_nr2Npe{0};
    bool  m_nr2Ae{true};
    float m_nr4Red{10.f}, m_nr4Smooth{0.f}, m_nr4White{0.f};
    float m_nr4Rescale{2.f}, m_nr4Post{-10.f};
    int   m_nr4Scaling{0};
    std::vector<float> m_fifoIn;    // Mono, noch nicht gerechnet
    std::vector<float> m_fifoOut;   // Mono, fertig
    std::vector<float> m_block;
    std::vector<float> m_mono;
};

} // namespace Longpath
