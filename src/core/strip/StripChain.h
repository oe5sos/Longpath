#pragma once

// =================================================================
// src/core/strip/StripChain.h  (Longpath)
// =================================================================
//
// Longpath-original, over DSP ported from AetherSDR
// (https://github.com/aethersdr/AetherSDR, GPLv3, primary author
// Jeremy [KK7GWY]). The stage order and the stage set follow
// AetherSDR's `defaultChain()` in src/core/AudioEngine.cpp at
// `31b29583`; the runner itself is written for Longpath because
// AetherSDR dispatches its chain from inside AudioEngine and
// Longpath's transmit audio does not pass through there.
//
// The Longpath Audio Channel Strip's stages, in order, as one object
// that can be handed to the transmit pump.
//
//   gate → EQ → de-esser → compressor → tube → PUDU → reverb → limiter
//
// PUDU is the AetherVoice exciter. The phase rotator lives inside the
// compressor upstream and is not a separate link here.
//
// Two rules, both about the fact that this sits in the transmit path:
//
//   Off by default. A fresh install sounds exactly as it did before
//   this file existed, and the operator turns the strip on when they
//   mean to.
//
//   Bypass is bit-exact and total. Master off returns before touching
//   a single sample — not "every stage disabled", which would still
//   depend on nine separate stages each honouring their own bypass.
//   tst_strip_dsp pins the per-stage version of that promise;
//   tst_strip_chain pins this one.
//
// What it cannot do: key the radio. It has no access to anything that
// could. It is a function from samples to samples, called from the
// pump, and the pump's own gates are unchanged — see
// TxChannel::writesToRadio().
//
// =================================================================
// Modification history (Longpath):
//   2026-08-08 — Created in C++20 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-09-17 — Non-finite guard: a NaN or an infinity, from the
//                 microphone or from a stage, is caught per stage, the
//                 stage is bypassed for that block and reset, and the
//                 event is counted. Prompted by a review of other audio
//                 chains (2026-09-17) that repair after every chain slot;
//                 we can do better because we own the stages.
// =================================================================

#include "core/strip/ClientComp.h"
#include "core/strip/ClientDeEss.h"
#include "core/strip/ClientEq.h"
#include "core/strip/ClientFinalLimiter.h"
#include "core/strip/ClientClipper.h"
#include "core/strip/ClientGate.h"
#include "core/strip/NoiseReduction.h"
#include "core/strip/ClientPudu.h"
#include "core/strip/ClientReverb.h"
#include "core/strip/ClientTube.h"
#include "core/strip/AudioPlugin.h"
#include "core/strip/MicSpectrum.h"

#include <array>
#include <memory>
#include <atomic>
#include <cstdint>
#include <vector>

namespace Longpath {

class StripChain {
public:
    // Order matches AetherSDR's defaultChain(); the limiter is not in
    // that list because upstream applies it separately at the very end
    // of the output path, and it is last here for the same reason.
    // Entrauschen steht VORN. Nicht aus Geschmack: das Gate soll
    // seine Schwelle auf einem sauberen Signal setzen, und alles, was
    // danach Pegel anhebt -- Kompressor, Roehre, Anreicherer -- soll
    // das Raumgeraeusch gar nicht erst mitbekommen.
    enum class Stage : uint8_t {
        Denoise = 0, Gate, Eq, DeEss, Comp, Tube, Pudu, Reverb, Clip,
        Limiter, Count
    };
    static constexpr int kStageCount = static_cast<int>(Stage::Count);

    StripChain();
    ~StripChain() = default;

    StripChain(const StripChain&)            = delete;
    StripChain& operator=(const StripChain&) = delete;

    // Main thread. Call before the first process() and on any rate
    // change; every stage reallocates its own delay lines here.
    void prepare(double sampleRate);
    double sampleRate() const noexcept { return m_sampleRate; }

    // Main thread, lock-free. Off by default — see the header note.
    void setEnabled(bool on) noexcept;
    bool isEnabled() const noexcept;

    // Per-stage bypass. Writes both the chain's flag and the stage's
    // own, so there is one answer to "is the compressor on" whichever
    // is asked.
    void setStageEnabled(Stage s, bool on) noexcept;
    bool stageEnabled(Stage s) const noexcept;

    static const char* stageName(Stage s) noexcept;

    // ── Die Reihenfolge der Stufen ───────────────────────────────────
    //
    // Bis zum 2026-10-13 stand sie als Folge von Aufrufen im
    // Rumpf von processMono(). AetherSDR traegt sie dagegen als LISTE
    // im Profil ("chain": ["Tube","Gate","Eq",...]) -- und Martins
    // Profil, das er unter Windows als gut befunden hat, stellt die
    // Roehre VOR das Tor. Das ist kein Geschmack, sondern ein anderes
    // Geraet: vorne saettigt sie das rohe Mikrofon, hinten das schon
    // komprimierte Signal.
    //
    // Gehalten wird die Reihenfolge in EINER atomaren Zahl: zehn
    // Stufen zu je vier Bit. Der Audiofaden liest sie einmal je Block,
    // der Hauptfaden schreibt sie in einem Zug -- kein halb
    // umgestellter Zustand, kein Schloss im Audiofaden.
    using Order = std::array<Stage, kStageCount>;

    // Longpaths eigene Reihenfolge: Entrauschen vorn, Clipper und
    // Begrenzer hinten. Siehe die Begruendung am Aufzaehlungstyp.
    static Order defaultOrder() noexcept;

    // Nimmt NUR eine Permutation an: jede Stufe genau einmal. Alles
    // andere laesst die bisherige Reihenfolge stehen und meldet false
    // -- eine Kette, in der eine Stufe zweimal oder gar nicht
    // vorkommt, ist kein Kanalzug mehr.
    bool setStageOrder(const Order& order) noexcept;
    Order stageOrder() const noexcept;

    // Ein kurzes, UNVERAENDERLICHES Wort je Stufe -- fuer die
    // Einstellungsdatei und fuer Aethers Profile, die ihre Kette als
    // Liste solcher Woerter tragen. Bewusst NICHT stageName(): das ist
    // die Beschriftung, die darf sich aendern und wird uebersetzt.
    // Eine Reihenfolge, die an Beschriftungen haengt, kippt beim
    // naechsten Umbenennen.
    static const char* stageToken(Stage s) noexcept;

    // Nimmt auch Aethers Namen an: "Enh" ist dort der Anreicherer, bei
    // uns heisst dieselbe Stufe "Pudu". Unbekanntes meldet false.
    static bool stageFromToken(const char* token, Stage& out) noexcept;

    // ── Einschubplaetze fuer fremde Effekte ──────────────────────────
    //
    // ZUSATZ, nicht Weg. Beide Plaetze sind leer, solange sie niemand
    // befuellt, und ein leerer Platz kostet im Block eine atomare
    // Ladung und sonst nichts -- siehe die drei Regeln in
    // AudioPlugin.h.
    //
    // Die Stelle in der Kette waehlt der Betreiber: 0 heisst vor dem
    // Gate, kStageCount heisst hinter dem Begrenzer, dazwischen vor
    // der Stufe mit diesem Index. Das ist der ganze Grund, warum es
    // Einschuebe sind und keine neunte Stufe -- ein Kompressor vor dem
    // Entzerrer ist ein anderes Geraet als einer dahinter, und das
    // gehoert dem gewaehlt, der es hoert.
    static constexpr int kInsertCount = 2;

    // Hauptfaden. Uebernimmt das Plugin, bereitet es auf die laufende
    // Rate vor und haengt es ein. nullptr leert den Platz. Die
    // Uebergabe wartet, bis der Audiofaden das alte Plugin sicher
    // nicht mehr in der Hand hat.
    void setInsertPlugin(int slot, std::unique_ptr<AudioPlugin> plugin);
    AudioPlugin* insertPlugin(int slot) const noexcept;

    void setInsertPosition(int slot, int position) noexcept;
    int  insertPosition(int slot) const noexcept;

    void setInsertEnabled(int slot, bool on) noexcept;
    bool insertEnabled(int slot) const noexcept;

    // Wie oft dieser Einschub etwas geliefert hat, das keine Zahl war.
    // Ein fremdes Plugin ist das einzige Stueck dieser Kette, das wir
    // nicht gelesen haben; was es tut, wird gezaehlt wie bei jeder
    // eigenen Stufe.
    uint32_t nonFiniteInsertBlocks(int slot) const noexcept;

    // Den gespeicherten Zustand eines Plugins uebernehmen, falls er
    // sich geaendert hat.
    //
    // Gebraucht vom Mithoerer: er hat seinen EIGENEN Kanalzug und
    // damit eine eigene Instanz desselben Plugins. Ohne das hier
    // bliebe sie auf dem Stand, den sie beim Laden hatte -- man
    // drehte an einem Regler und hoerte nichts, was die ganze
    // Uebung sinnlos macht.
    //
    // Haelt den Audiofaden kurz an (derselbe Handschlag wie
    // setInsertPlugin): einem fremden Plugin den Zustand zu setzen,
    // waehrend es rechnet, ist nichts, was man ihm zutrauen darf.
    // Kostet also einen Block, wenn sich wirklich etwas geaendert
    // hat -- und nur dann, darum der Vergleich.
    void applyInsertState(int slot, const QByteArray& blob);

    // Audio thread. Mono, in place. Returns immediately when the
    // master switch is off, so bypass costs one atomic load and the
    // samples are untouched.
    void processMono(float* samples, int frames) noexcept;

    void reset() noexcept;

    // ── What the strip did to the level ──────────────────────────────
    //
    // Peak in and peak out, in dBFS, measured across the last block
    // that was actually processed. The difference between them is the
    // number that matters: every stage above the limiter can add gain —
    // the tube, the exciter, the compressor's make-up — and an operator
    // who cannot see the total is one who finds out from a report of
    // splatter.
    //
    // Held at -120 while the strip is off rather than reporting the
    // input twice. A gain change of zero and a strip that is not
    // running are different facts and should not read the same.
    // ── Eingangsverstärkung ──────────────────────────────────────────
    //
    // „die rohdaten sind bei allen profilen um weiten voraus."
    //
    // Der Grund stand in seinen eigenen Zahlen: die Stimme kam mit
    // -25 bis -35 dBFS an. JEDE Schwelle in dieser Kette ist aber in
    // dBFS angegeben -- Gate, Kompressor, De-Esser, Begrenzer. Bei
    // 15 dB zu wenig Pegel heisst das: das Gate hält die Stimme für
    // den Raum und macht zu, der Kompressor greift nie, der Clipper
    // schneidet nichts ab. Die Kette tut dann genau zwei Dinge --
    // sie verschluckt Wörter und sie färbt --, und gegen das rohe
    // Signal verliert sie zu Recht.
    //
    // Keine Schwelle der Welt hilft dagegen, solange der Pegel nicht
    // stimmt. Also sitzt hier, VOR allem anderen, eine einfache
    // Verstärkung -- und die Oberfläche sagt, welche Zahl gebraucht
    // wird. Gemessen, nicht geraten; gesetzt vom Betreiber, nicht
    // von selbst.
    void  setInputGainDb(float db) noexcept;   // -12 .. +36 dB
    float inputGainDb() const noexcept;

    // ── Und derselbe Pegel, von selbst ───────────────────────────────
    //
    // „mir geht die kraft als vorverstärker ab."
    //
    // Darüber stand: „gesetzt vom Betreiber, nicht von selbst." Das
    // war als Tugend gemeint und war ein Fehler. Ein Vorverstärker,
    // bei dem man erst einen Knopf drücken muss, ehe er verstärkt,
    // ist keiner -- und wer ihn nicht drückt, hört eine Kette, in der
    // JEDE Schwelle am falschen Platz steht, ohne dass ihm irgendwo
    // gesagt würde, warum.
    //
    // Also sucht sie den Arbeitspunkt selbst. Kein Regelkreis über
    // den Ton: die Verstärkung ist weiterhin eine einzige Zahl, sie
    // wird nur alle zwei Sekunden um einen Schritt nachgezogen --
    // und ausschliesslich dann, wenn in diesen zwei Sekunden
    // gesprochen wurde. In der Stille rührt sie sich nicht, sonst
    // zöge sie das Grundgeräusch hoch, bis die Kette es für eine
    // Stimme hält.
    //
    // Der Zielwert ist kZielSpitzeDbFs: dort liegen alle Schwellen
    // der Kette, dort rechnet der Ausgleich des Kompressors, und
    // dort beginnt der Clipper zu arbeiten.
    static constexpr float kZielSpitzeDbFs = -12.0f;
    // Unter diesem Pegel gilt ein Zeitfenster als Pause.
    static constexpr float kSpracheAbDbFs  = -45.0f;

    // Der Scheitelfaktor, ab dem ein Fenster als Sprache gilt.
    // Gemessen: echte Sprache liegt bei 12-18 dB, Brummen und
    // Rauschen bei 3-6 dB. 9 dB trennt beides mit Abstand nach beiden
    // Seiten. Siehe die Begründung in StripChain.cpp — ohne diese
    // Schranke hebt die Automatik einen Störteppich auf Sprachpegel.
    static constexpr float kScheitelAbDb   = 9.0f;
    void  setInputAuto(bool on) noexcept;
    bool  inputAuto() const noexcept;
    // Was gerade WIRKLICH verstärkt wird -- von Hand oder gefunden.
    // Die Anzeige liest diese Zahl, nicht inputGainDb().
    float effectiveInputGainDb() const noexcept;

    // Was AM EINGANG ankommt, vor der Verstärkung -- also das, was
    // das Mikrofon liefert. Ein Augenblickswert: die Spitze des
    // letzten Blocks, rund zehn Millisekunden. Zum ANSEHEN gut, zum
    // RECHNEN nicht -- siehe speechPeakDb().
    float rawInputPeakDb() const noexcept;

    // ── Der Pegel, mit dem gerechnet werden darf ─────────────────
    //
    // Dieselbe Messung, aus der die Automatik ihren Schritt zieht:
    // die Spitze eines Zwei-Sekunden-Fensters, aber nur aus einem
    // Fenster, das den Scheitelfaktor-Test bestanden hat. Also der
    // Pegel seiner STIMME, nicht der des Raums zwischen zwei Wörtern.
    //
    // Warum es die Zahl gibt: am 2026-10-14 rechnete die Empfehlung
    // („x dB fehlen") und der Knopf daneben beide mit
    // rawInputPeakDb(). Der Knopf wurde in einer Sprechpause
    // gedrückt, traf den Störteppich bei -48 dBFS, setzte die vollen
    // +36 dB -- und der Kopfhörer stand an der Decke des Begrenzers.
    // Es war das dritte Mal an diesem Tag.
    //
    // -120 heisst „noch nichts gehört": dann rechnet niemand, und
    // der Knopf bleibt aus.
    float speechPeakDb() const noexcept;

    float inputPeakDb() const noexcept;

    // ── Scheitelfaktor ───────────────────────────────────────────────
    //
    // Der Abstand zwischen Spitze und Effektivwert, über eine Sekunde.
    // Bei SSB ist das die Zahl, die über die Reichweite entscheidet:
    // die Endstufe wird von der SPITZE begrenzt, gehört wird der
    // EFFEKTIVWERT. 18 dB Abstand heissen, dass die Spitzenleistung
    // kaum als Lautstärke ankommt.
    //
    // Zwei Werte, weil nur der Vergleich etwas sagt: was die Stimme
    // mitbringt, und was die Kette daraus macht. Eine einzelne Zahl
    // wäre nicht zu beurteilen.
    //
    // 0 heisst „noch nicht gemessen" -- es gibt keinen Scheitelfaktor
    // von null, also kann die Null nicht mit einer Messung verwechselt
    // werden.
    float inputCrestDb() const noexcept;
    float outputCrestDb() const noexcept;
    float outputPeakDb() const noexcept;

    // ── Values that are not numbers ──────────────────────────────────
    //
    // A NaN or an infinity in the transmit path is not a loud sample,
    // it is a poison: every filter with memory that touches it keeps
    // it forever (the limiter's envelope goes to infinity and stays
    // there, a biquad's delay line never clears), WDSP's TXA chain and
    // PureSignal's calibration would inherit it, and the conversion to
    // wire integers turns it into full-scale garbage on the air.
    //
    // So processMono() checks the block once at the input and once
    // after every enabled stage. A non-finite input sample becomes
    // silence. A stage whose output is not finite is treated as if it
    // had been bypassed for that block — the block it was handed is
    // restored, which is the chain's own bypass guarantee applied
    // after the fact — and the stage is reset(), so state it may have
    // poisoned does not carry into the next block. Whatever arrives at
    // WDSP is finite. Always.
    //
    // Each event is counted per source, so the pump can report which
    // stage did it and how often. Written on the audio thread, read by
    // anyone; relaxed, like the meters — a count that is one block
    // stale is still a count.
    uint32_t nonFiniteBlocks(Stage s) const noexcept;
    uint32_t nonFiniteInputBlocks() const noexcept;
    uint32_t nonFiniteBlocksTotal() const noexcept;

    // ── The microphone, for drawing over ─────────────────────────────
    //
    // Fed by whoever owns the mic tap, read by the EQ curve. It lives
    // here rather than in the window because the window is opened and
    // closed and the spectrum should not restart each time — an
    // operator who closes the strip to look at something else and comes
    // back should not have to speak for two seconds again.
    //
    // Deliberately NOT fed from processMono(): that only runs when the
    // strip is switched on, and the whole point of the picture is to
    // decide whether to switch it on.
    MicSpectrum&       micSpectrum()       noexcept { return m_micSpectrum; }
    const MicSpectrum& micSpectrum() const noexcept { return m_micSpectrum; }

    // The stages themselves, for the panels to bind to. Not owned by
    // the caller and not valid across a prepare().
    NoiseReduction&     nr()      noexcept { return m_nr; }
    ClientClipper&      clipper() noexcept { return m_clip; }
    ClientGate&         gate()    noexcept { return m_gate; }
    ClientEq&           eq()      noexcept { return m_eq; }
    ClientDeEss&        deEss()   noexcept { return m_deEss; }
    ClientComp&         comp()    noexcept { return m_comp; }
    ClientTube&         tube()    noexcept { return m_tube; }
    ClientPudu&         pudu()    noexcept { return m_pudu; }
    ClientReverb&       reverb()  noexcept { return m_reverb; }
    ClientFinalLimiter& limiter() noexcept { return m_limiter; }

    // Const overloads, so a caller that only READS the chain can say so
    // in its signature. Added for StripCharacters::inEffect(), which
    // compares a stage against every character and must not be able to
    // change one by accident — the stage it is inspecting is being read
    // by the audio thread at the same time.
    const NoiseReduction&     nr()      const noexcept { return m_nr; }
    const ClientClipper&      clipper() const noexcept { return m_clip; }
    const ClientGate&         gate()    const noexcept { return m_gate; }
    const ClientEq&           eq()      const noexcept { return m_eq; }
    const ClientDeEss&        deEss()   const noexcept { return m_deEss; }
    const ClientComp&         comp()    const noexcept { return m_comp; }
    const ClientTube&         tube()    const noexcept { return m_tube; }
    const ClientPudu&         pudu()    const noexcept { return m_pudu; }
    const ClientReverb&       reverb()  const noexcept { return m_reverb; }
    const ClientFinalLimiter& limiter() const noexcept { return m_limiter; }

private:
    double m_sampleRate{48000.0};

    std::atomic<bool> m_enabled{false};
    // Mirrors each stage's own flag. Two copies of one fact is a risk,
    // but the alternative is the audio thread doing eight virtual calls
    // to ask questions it could answer from one cache line — and
    // tst_strip_chain holds the two in step.
    std::array<std::atomic<bool>, kStageCount> m_stageOn;

    // Ein Einschubplatz. m_owned haelt das Plugin, m_live ist das, was
    // der Audiofaden sehen darf, und m_busy ist der Handschlag
    // zwischen beiden: der Audiofaden setzt ihn, BEVOR er den Zeiger
    // liest, der Hauptfaden leert den Zeiger und wartet dann, bis
    // m_busy null ist. Danach kann kein Durchlauf das alte Plugin
    // mehr in die Hand bekommen.
    struct Insert {
        std::unique_ptr<AudioPlugin> owned;
        std::atomic<AudioPlugin*> live{nullptr};
        std::atomic<int>  busy{0};
        std::atomic<bool> on{true};
        std::atomic<int>  position{0};
        std::atomic<uint32_t> nonFinite{0};
        QByteArray lastState;   // nur Hauptfaden
    };
    std::array<Insert, kInsertCount> m_inserts;

    // Groesster Block, den ein Einschub vorbereiten muss. Die
    // Sendepumpe rechnet in 64 Rahmen, der Mithoerer auch; der
    // Spielraum ist da, damit ein Offline-Durchlauf mit groesseren
    // Haeppchen kein Plugin ueberfaehrt.
    static constexpr int kMaxInsertFrames = 2048;

    // Laeuft die Einschuebe, die an dieser Stelle der Kette haengen.
    void runInserts(int position, float* samples, int frames) noexcept;
    static void waitUntilIdle(Insert& in) noexcept;

    // Written on the audio thread once per block, read by the meter.
    // Plain relaxed stores: a meter that is one block stale is a meter,
    // and paying for ordering on the transmit path to make a picture
    // slightly fresher is the wrong trade.
    std::atomic<float> m_inCrestDb{0.0f};
    std::atomic<float> m_outCrestDb{0.0f};
    // Sammelt über eine Sekunde, dann einmal veröffentlichen.
    double m_crestSumIn{0.0};
    double m_crestSumOut{0.0};
    float  m_crestPeakIn{0.0f};
    float  m_crestPeakOut{0.0f};
    int    m_crestFrames{0};

    std::atomic<float> m_inputGainDb{0.0f};
    // ── Der gefundene Pegel ──────────────────────────────────────
    //
    // Getrennt vom Wert des Betreibers, und zwar mit Absicht: der
    // Mithörer spiegelt die Einstellungen der Sendekette, und wenn
    // beide dieselbe Zahl benutzten, würde jede Spiegelung die
    // gefundene Verstärkung der anderen überschreiben. So sucht
    // jede Kette ihren eigenen Pegel aus ihrem eigenen Strom, und
    // kopiert wird nur, was der Betreiber eingestellt hat.
    std::atomic<bool>  m_inputAuto{false};
    std::atomic<float> m_autoGainDb{0.0f};
    std::atomic<float> m_speechPeakDb{-120.0f};
    float m_autoPeak{0.0f};
    double m_autoSumSq{0.0};        // Spitze im laufenden Zeitfenster
    int   m_autoFrames{0};
    std::atomic<float> m_rawInPeakDb{-120.0f};
    std::atomic<float> m_inPeakDb{-120.0f};
    std::atomic<float> m_outPeakDb{-120.0f};

    // The non-finite guard. One stage at a time: snapshot the block,
    // run the stage, and if what came out is not finite put the
    // snapshot back and reset the stage. Defined in the .cpp; every
    // instantiation lives there.
    template <typename S>
    void runStage(Stage which, S& stage, float* samples, int frames) noexcept;

    // Der Verteiler: aus einer Stufennummer das zugehoerige Mitglied.
    // Nur hier steht, welche Stufe welches Objekt ist.
    void runStageAt(Stage which, float* samples, int frames) noexcept;

    // Zehn Stufen zu vier Bit, Stufe 0 in den untersten vier.
    static uint64_t packOrder(const Order& order) noexcept;
    static Order unpackOrder(uint64_t packed) noexcept;
    std::atomic<uint64_t> m_order;

    // The block as it was before the stage that is running now. Fixed
    // size and part of the object, so the audio thread never allocates
    // for it; the pump's blocks are 64 frames and the offline preview's
    // are too. A block larger than this still comes out finite — the
    // offending samples are zeroed instead of the stage being bypassed.
    static constexpr int kSnapshotFrames = 2048;
    std::array<float, kSnapshotFrames> m_snapshot{};

    // Blocks in which a non-finite value was caught: one counter per
    // stage, one for the input, and the sum, so "did anything happen"
    // is one load.
    std::array<std::atomic<uint32_t>, kStageCount> m_nonFiniteBlocks;
    std::atomic<uint32_t> m_nonFiniteInputBlocks{0};
    std::atomic<uint32_t> m_nonFiniteTotal{0};

    NoiseReduction     m_nr;
    ClientClipper      m_clip;
    ClientGate         m_gate;
    ClientEq           m_eq;
    ClientDeEss        m_deEss;
    ClientComp         m_comp;
    ClientTube         m_tube;
    ClientPudu         m_pudu;
    ClientReverb       m_reverb;
    ClientFinalLimiter m_limiter;
    MicSpectrum        m_micSpectrum;
};

} // namespace Longpath
