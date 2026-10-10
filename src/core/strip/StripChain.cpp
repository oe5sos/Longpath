// =================================================================
// src/core/strip/StripChain.cpp  (Longpath)
// =================================================================
//
// Longpath-original. See StripChain.h for the order, the two rules
// and why the runner is ours rather than ported.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-08 — Created in C++20 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-09-17 — Non-finite guard around every stage; see the header.
// =================================================================

#include "core/strip/StripChain.h"

#include <algorithm>
#include <cmath>
#include <thread>

namespace Longpath {

namespace {

bool allFinite(const float* s, int n) noexcept
{
    for (int i = 0; i < n; ++i) {
        if (!std::isfinite(s[i])) { return false; }
    }
    return true;
}

// Silence in place of every sample that is not a number. Returns
// whether there was one. Used for the input, where there is nothing
// to restore, and for a block too large for the snapshot.
bool zeroNonFinite(float* s, int n) noexcept
{
    bool any = false;
    for (int i = 0; i < n; ++i) {
        if (!std::isfinite(s[i])) { s[i] = 0.0f; any = true; }
    }
    return any;
}

} // namespace

StripChain::StripChain()
{
    for (auto& f : m_stageOn) { f.store(false, std::memory_order_relaxed); }
    for (auto& c : m_nonFiniteBlocks) { c.store(0, std::memory_order_relaxed); }
}

void StripChain::prepare(double sampleRate)
{
    m_sampleRate = sampleRate;
    m_nr.prepare(sampleRate);
    m_clip.prepare(sampleRate);
    m_gate.prepare(sampleRate);
    m_eq.prepare(sampleRate);
    m_deEss.prepare(sampleRate);
    m_comp.prepare(sampleRate);
    m_tube.prepare(sampleRate);
    m_pudu.prepare(sampleRate);
    m_reverb.prepare(sampleRate);
    m_limiter.prepare(sampleRate);

    // Die Einschuebe mit. Ein Plugin, das die neue Rate nicht mag,
    // wird ausgehaengt statt stillschweigend falsch gerechnet -- der
    // Platz ist dann leer, und das ist eine Tatsache, die man sehen
    // kann, kein Klang, den man suchen muss.
    for (Insert& in : m_inserts) {
        if (!in.owned) { continue; }
        in.live.store(nullptr, std::memory_order_release);
        waitUntilIdle(in);
        if (in.owned->prepare(sampleRate, kMaxInsertFrames)) {
            in.live.store(in.owned.get(), std::memory_order_release);
        }
    }
}

// Warten, bis der Audiofaden diesen Platz sicher nicht mehr anfasst.
// Kein Schloss: der Audiofaden darf nie blockieren, also dreht der
// Hauptfaden die Schleife. In der Praxis ist das ein Block, also
// Mikrosekunden; die Obergrenze ist nur da, damit ein haengender
// Audiofaden nicht die Oberflaeche mitnimmt.
void StripChain::waitUntilIdle(Insert& in) noexcept
{
    for (int spins = 0; spins < 100000; ++spins) {
        if (in.busy.load(std::memory_order_acquire) == 0) { return; }
        std::this_thread::yield();
    }
}

void StripChain::setInsertPlugin(int slot, std::unique_ptr<AudioPlugin> plugin)
{
    if (slot < 0 || slot >= kInsertCount) { return; }
    Insert& in = m_inserts[static_cast<size_t>(slot)];

    in.live.store(nullptr, std::memory_order_release);
    waitUntilIdle(in);
    in.owned = std::move(plugin);
    in.nonFinite.store(0, std::memory_order_relaxed);
    if (in.owned && in.owned->prepare(m_sampleRate, kMaxInsertFrames)) {
        in.lastState = in.owned->saveState();
        in.live.store(in.owned.get(), std::memory_order_release);
    } else {
        in.owned.reset();
        in.lastState.clear();
    }
}

AudioPlugin* StripChain::insertPlugin(int slot) const noexcept
{
    if (slot < 0 || slot >= kInsertCount) { return nullptr; }
    return m_inserts[static_cast<size_t>(slot)].owned.get();
}

void StripChain::setInsertPosition(int slot, int position) noexcept
{
    if (slot < 0 || slot >= kInsertCount) { return; }
    m_inserts[static_cast<size_t>(slot)].position.store(
        std::clamp(position, 0, kStageCount), std::memory_order_release);
}

int StripChain::insertPosition(int slot) const noexcept
{
    if (slot < 0 || slot >= kInsertCount) { return 0; }
    return m_inserts[static_cast<size_t>(slot)].position.load(
        std::memory_order_acquire);
}

void StripChain::setInsertEnabled(int slot, bool on) noexcept
{
    if (slot < 0 || slot >= kInsertCount) { return; }
    m_inserts[static_cast<size_t>(slot)].on.store(on, std::memory_order_release);
}

bool StripChain::insertEnabled(int slot) const noexcept
{
    if (slot < 0 || slot >= kInsertCount) { return false; }
    return m_inserts[static_cast<size_t>(slot)].on.load(std::memory_order_acquire);
}

void StripChain::applyInsertState(int slot, const QByteArray& blob)
{
    if (slot < 0 || slot >= kInsertCount) { return; }
    Insert& in = m_inserts[static_cast<size_t>(slot)];
    if (!in.owned || blob.isEmpty()) { return; }
    if (in.lastState == blob) { return; }

    in.live.store(nullptr, std::memory_order_release);
    waitUntilIdle(in);
    if (in.owned->restoreState(blob)) { in.lastState = blob; }
    in.live.store(in.owned.get(), std::memory_order_release);
}

uint32_t StripChain::nonFiniteInsertBlocks(int slot) const noexcept
{
    if (slot < 0 || slot >= kInsertCount) { return 0; }
    return m_inserts[static_cast<size_t>(slot)].nonFinite.load(
        std::memory_order_relaxed);
}

// Derselbe Schutz wie runStage(), und aus demselben Grund nur
// strenger: hier laeuft Code, den wir nicht gelesen haben. Liefert er
// etwas, das keine Zahl ist, bekommt die Kette den Block zurueck, wie
// er hereinkam, das Plugin wird zurueckgesetzt und der Vorfall
// gezaehlt.
void StripChain::runInserts(int position, float* samples, int frames) noexcept
{
    for (Insert& in : m_inserts) {
        if (in.position.load(std::memory_order_acquire) != position) { continue; }
        if (!in.on.load(std::memory_order_acquire)) { continue; }

        in.busy.store(1, std::memory_order_seq_cst);
        AudioPlugin* p = in.live.load(std::memory_order_seq_cst);
        if (p != nullptr) {
            const bool canRestore = frames <= kSnapshotFrames;
            if (canRestore) { std::copy_n(samples, frames, m_snapshot.data()); }

            p->processMono(samples, frames);

            if (!allFinite(samples, frames)) {
                if (canRestore) {
                    std::copy_n(m_snapshot.data(), frames, samples);
                } else {
                    zeroNonFinite(samples, frames);
                }
                p->reset();
                in.nonFinite.fetch_add(1, std::memory_order_relaxed);
                m_nonFiniteTotal.fetch_add(1, std::memory_order_relaxed);
            }
        }
        in.busy.store(0, std::memory_order_seq_cst);
    }
}

void StripChain::setEnabled(bool on) noexcept
{
    m_enabled.store(on, std::memory_order_release);
}

bool StripChain::isEnabled() const noexcept
{
    return m_enabled.load(std::memory_order_acquire);
}

void StripChain::setStageEnabled(Stage s, bool on) noexcept
{
    const int i = static_cast<int>(s);
    if (i < 0 || i >= kStageCount) { return; }
    m_stageOn[static_cast<size_t>(i)].store(on, std::memory_order_release);

    // Mirror into the stage's own switch where it has one. Keeping both
    // in step means there is a single answer to "is the compressor on",
    // whether it is asked of the chain or of the compressor.
    switch (s) {
    case Stage::Denoise: m_nr.setEnabled(on);  break;
    case Stage::Clip:    m_clip.setEnabled(on); break;
    case Stage::Gate:  m_gate.setEnabled(on);  break;
    case Stage::Eq:    m_eq.setEnabled(on);    break;
    case Stage::DeEss: m_deEss.setEnabled(on); break;
    case Stage::Comp:  m_comp.setEnabled(on);  break;
    case Stage::Tube:  m_tube.setEnabled(on);  break;
    case Stage::Pudu:    m_pudu.setEnabled(on);    break;
    // These two were nearly missed. A grep for `void setEnabled(`
    // found neither, because both are written with two spaces after
    // the return type — so the first version of this switch treated
    // them as having no bypass at all, and their own flags would have
    // sat at their defaults while the chain believed it was in
    // control. The lesson is not about spaces: a search that says
    // "this API does not exist" deserves a second look at the header
    // before it becomes a design decision.
    case Stage::Reverb:  m_reverb.setEnabled(on);  break;
    case Stage::Limiter: m_limiter.setEnabled(on); break;
    case Stage::Count:
        break;
    }
}

bool StripChain::stageEnabled(Stage s) const noexcept
{
    const int i = static_cast<int>(s);
    if (i < 0 || i >= kStageCount) { return false; }
    return m_stageOn[static_cast<size_t>(i)].load(std::memory_order_acquire);
}

const char* StripChain::stageName(Stage s) noexcept
{
    switch (s) {
    case Stage::Denoise: return "Denoise";
    case Stage::Clip:    return "Clipper";
    case Stage::Gate:    return "Gate";
    case Stage::Eq:      return "EQ";
    case Stage::DeEss:   return "De-Esser";
    case Stage::Comp:    return "Compressor";
    case Stage::Tube:    return "Tube";
    case Stage::Pudu:    return "PUDU";      // the AetherVoice exciter
    case Stage::Reverb:  return "Reverb";
    case Stage::Limiter: return "Limiter";
    case Stage::Count:   break;
    }
    return "";
}

void StripChain::processMono(float* samples, int frames) noexcept
{
    // One atomic load and out. Not "run every stage and let each one
    // notice it is disabled": that would make the guarantee depend on
    // eight separate stages each getting their own bypass right, and
    // this is the transmit path.
    if (!m_enabled.load(std::memory_order_acquire)) {
        // Not "input twice with no change" — off and unity are
        // different facts. See the note on inputPeakDb().
        m_inPeakDb.store(-120.0f, std::memory_order_relaxed);
        m_outPeakDb.store(-120.0f, std::memory_order_relaxed);
        return;
    }
    if (samples == nullptr || frames <= 0) { return; }

    // The input first. The microphone path — CoreAudio, PortAudio,
    // the VAX bus another program writes into, a TCI client — is not
    // ours to vouch for, and a NaN that gets past here would be fed
    // to the gate's envelope and every delay line after it. Silence
    // for the offending samples, and the block is counted.
    if (zeroNonFinite(samples, frames)) {
        m_nonFiniteInputBlocks.fetch_add(1, std::memory_order_relaxed);
        m_nonFiniteTotal.fetch_add(1, std::memory_order_relaxed);
    }

    auto peakDb = [](const float* s, int n) {
        float p = 0.0f;
        for (int i = 0; i < n; ++i) { p = std::max(p, std::fabs(s[i])); }
        return p > 1e-6f ? 20.0f * std::log10(p) : -120.0f;
    };
    // Erst messen, was ANKOMMT -- das ist die Zahl, aus der die
    // Oberfläche die Empfehlung rechnet.
    m_rawInPeakDb.store(peakDb(samples, frames), std::memory_order_relaxed);

    // ── Den Arbeitspunkt suchen, wenn er gesucht werden soll ────
    //
    // Zwei Sekunden sammeln, dann einen Schritt. Nur wenn in diesen
    // zwei Sekunden etwas war, das nach Sprache aussieht; in der
    // Stille bleibt die Zahl stehen. Der Schritt ist ein Drittel des
    // Fehlers, gedeckelt auf 4 dB -- aus zwanzig Dezibel daneben
    // wird so in gut einer Viertelminute null, ohne dass man eine
    // Regelung hört.
    if (m_inputAuto.load(std::memory_order_relaxed)) {
        for (int i = 0; i < frames; ++i) {
            m_autoPeak = std::max(m_autoPeak, std::fabs(samples[i]));
        }
        m_autoFrames += frames;
        const int fenster = int(m_sampleRate * 2.0);
        if (m_autoFrames >= fenster && fenster > 0) {
            const float spitzeDb = m_autoPeak > 1e-7f
                ? 20.0f * std::log10(m_autoPeak) : -120.0f;
            if (spitzeDb > kSpracheAbDbFs) {
                // Gemessen wird hier die ROHE Spitze -- die
                // Verstaerkung kommt erst danach. Die gesuchte Zahl
                // ist deshalb schlicht der Abstand zum Ziel, und
                // nicht der Abstand abzueglich dessen, was schon
                // eingestellt ist. Die erste Fassung zog die eigene
                // Verstaerkung ab, rechnete sie sich damit jedes
                // Fenster erneut dazu und lief in den Anschlag: 36 dB
                // statt der gebrauchten 27,6.
                const float ist  = m_autoGainDb.load(std::memory_order_relaxed);
                const float soll = kZielSpitzeDbFs - spitzeDb;
                const float schritt = std::clamp((soll - ist) / 3.0f,
                                                 -4.0f, 4.0f);
                m_autoGainDb.store(std::clamp(ist + schritt, -12.0f, 36.0f),
                                   std::memory_order_relaxed);
            }
            m_autoPeak = 0.0f;
            m_autoFrames = 0;
        }
    }

    // Dann verstärken, und zwar vor allem anderen: alle Schwellen
    // dahinter sind in dBFS, und sie sind nur dann richtig, wenn der
    // Pegel stimmt.
    const float gainDb = m_inputAuto.load(std::memory_order_relaxed)
                       ? m_autoGainDb.load(std::memory_order_relaxed)
                       : m_inputGainDb.load(std::memory_order_relaxed);
    if (std::fabs(gainDb) > 0.01f) {
        const float g = std::pow(10.0f, gainDb / 20.0f);
        for (int i = 0; i < frames; ++i) { samples[i] *= g; }
    }

    m_inPeakDb.store(peakDb(samples, frames), std::memory_order_relaxed);

    // Scheitelfaktor, Eingangsseite: Summe und Spitze mitführen, die
    // Division kommt einmal je Sekunde.
    for (int i = 0; i < frames; ++i) {
        const float v = samples[i];
        m_crestSumIn += double(v) * double(v);
        m_crestPeakIn = std::max(m_crestPeakIn, std::fabs(v));
    }

    // Every stage has its own enable and the chain mirrors it, so the
    // two always agree. Skipping the call as well is not redundant: a
    // stage that is off should cost nothing, and going through it
    // anyway would rest the guarantee on eight separate bypasses being
    // bit-exact rather than on not calling them. runStage() does the
    // skip, and wraps each call in the non-finite guard.
    runInserts(0, samples, frames);
    // Zuerst, damit alles danach eine saubere Stimme bekommt -- siehe
    // die Begruendung am Stage-Aufzaehlungstyp.
    runStage(Stage::Denoise, m_nr,    samples, frames);
    runInserts(1, samples, frames);
    runStage(Stage::Gate,   m_gate,   samples, frames);
    runInserts(2, samples, frames);
    runStage(Stage::Eq,     m_eq,     samples, frames);
    runInserts(3, samples, frames);
    runStage(Stage::DeEss,  m_deEss,  samples, frames);
    runInserts(4, samples, frames);
    runStage(Stage::Comp,   m_comp,   samples, frames);
    runInserts(5, samples, frames);
    runStage(Stage::Tube,   m_tube,   samples, frames);
    runInserts(6, samples, frames);
    runStage(Stage::Pudu,   m_pudu,   samples, frames);
    runInserts(7, samples, frames);
    runStage(Stage::Reverb, m_reverb, samples, frames);
    runInserts(8, samples, frames);
    // Direkt vor dem Begrenzer: das Abschneiden nimmt die Spitze
    // weg, der Begrenzer faengt, was danach noch darueber liegt.
    // Umgekehrt waere der Begrenzer schon am Arbeiten, bevor die
    // Spitze ueberhaupt kleiner geworden ist.
    runStage(Stage::Clip,   m_clip,   samples, frames);
    runInserts(9, samples, frames);
    // Last, and for a reason: a brickwall that anything runs after is
    // not a brickwall. Everything above can add gain — the tube, the
    // exciter, the compressor's make-up — and this is what stops the
    // sum reaching the modulator hotter than it should.
    runStage(Stage::Limiter, m_limiter, samples, frames);
    // Hinter dem Begrenzer. Erlaubt, aber mit Ansage: was hier noch
    // Pegel addiert, hebt die Deckelung auf, die eine Zeile darueber
    // gerade gesetzt wurde.
    runInserts(kStageCount, samples, frames);

    m_outPeakDb.store(peakDb(samples, frames), std::memory_order_relaxed);

    for (int i = 0; i < frames; ++i) {
        const float v = samples[i];
        m_crestSumOut += double(v) * double(v);
        m_crestPeakOut = std::max(m_crestPeakOut, std::fabs(v));
    }
    m_crestFrames += frames;
    if (m_crestFrames >= int(m_sampleRate > 1000.0 ? m_sampleRate : 48000.0)) {
        const auto crest = [](double sum, float peak, int n) {
            const double rms = std::sqrt(sum / std::max(1, n));
            // Unter -60 dBFS ist das Stille, und der Scheitelfaktor
            // von Stille ist keine Aussage über eine Stimme.
            if (peak < 0.001f || rms < 1e-7) { return 0.0f; }
            return float(20.0 * std::log10(double(peak) / rms));
        };
        m_inCrestDb.store(crest(m_crestSumIn, m_crestPeakIn, m_crestFrames),
                          std::memory_order_relaxed);
        m_outCrestDb.store(crest(m_crestSumOut, m_crestPeakOut, m_crestFrames),
                           std::memory_order_relaxed);
        m_crestSumIn = m_crestSumOut = 0.0;
        m_crestPeakIn = m_crestPeakOut = 0.0f;
        m_crestFrames = 0;
    }
}

float StripChain::inputCrestDb() const noexcept
{
    return m_inCrestDb.load(std::memory_order_relaxed);
}

float StripChain::outputCrestDb() const noexcept
{
    return m_outCrestDb.load(std::memory_order_relaxed);
}

template <typename S>
void StripChain::runStage(Stage which, S& stage, float* samples,
                          int frames) noexcept
{
    if (!stageEnabled(which)) { return; }

    // Keep what the stage is about to be handed. A copy of 64 floats
    // per enabled stage is the price of being able to say "that block
    // never happened" afterwards, and it is a small one.
    const bool canRestore = frames <= kSnapshotFrames;
    if (canRestore) { std::copy_n(samples, frames, m_snapshot.data()); }

    constexpr int kMono = 1;
    stage.process(samples, frames, kMono);
    if (allFinite(samples, frames)) { return; }

    // The stage turned finite input into something that is not a
    // number. Its parameters have not changed — a NaN in a setting, a
    // division that went wrong for this block — so the honest thing
    // is what the operator's own bypass switch would have done: hand
    // on the block as it arrived. And reset the stage, because the
    // value that came out of it is very likely sitting in its delay
    // line or its envelope, waiting for the next block.
    if (canRestore) { std::copy_n(m_snapshot.data(), frames, samples); }
    else            { zeroNonFinite(samples, frames); }
    stage.reset();

    m_nonFiniteBlocks[static_cast<size_t>(which)].fetch_add(
        1, std::memory_order_relaxed);
    m_nonFiniteTotal.fetch_add(1, std::memory_order_relaxed);
}

uint32_t StripChain::nonFiniteBlocks(Stage s) const noexcept
{
    const int i = static_cast<int>(s);
    if (i < 0 || i >= kStageCount) { return 0; }
    return m_nonFiniteBlocks[static_cast<size_t>(i)].load(
        std::memory_order_relaxed);
}

uint32_t StripChain::nonFiniteInputBlocks() const noexcept
{
    return m_nonFiniteInputBlocks.load(std::memory_order_relaxed);
}

uint32_t StripChain::nonFiniteBlocksTotal() const noexcept
{
    return m_nonFiniteTotal.load(std::memory_order_relaxed);
}

void StripChain::setInputGainDb(float db) noexcept
{
    m_inputGainDb.store(std::clamp(db, -12.0f, 36.0f),
                        std::memory_order_relaxed);
}

float StripChain::inputGainDb() const noexcept
{
    return m_inputGainDb.load(std::memory_order_relaxed);
}

void StripChain::setInputAuto(bool on) noexcept
{
    if (on && !m_inputAuto.load(std::memory_order_relaxed)) {
        // Vom Wert des Betreibers aus loslaufen, nicht von null --
        // wer schon einmal „Übernehmen" gedrückt hat, soll nicht
        // zusehen muessen, wie die Kette seinen Wert neu erfindet.
        m_autoGainDb.store(m_inputGainDb.load(std::memory_order_relaxed),
                           std::memory_order_relaxed);
    }
    m_inputAuto.store(on, std::memory_order_relaxed);
}

bool StripChain::inputAuto() const noexcept
{
    return m_inputAuto.load(std::memory_order_relaxed);
}

float StripChain::effectiveInputGainDb() const noexcept
{
    return m_inputAuto.load(std::memory_order_relaxed)
         ? m_autoGainDb.load(std::memory_order_relaxed)
         : m_inputGainDb.load(std::memory_order_relaxed);
}

float StripChain::rawInputPeakDb() const noexcept
{
    return m_rawInPeakDb.load(std::memory_order_relaxed);
}

float StripChain::inputPeakDb() const noexcept
{
    return m_inPeakDb.load(std::memory_order_relaxed);
}

float StripChain::outputPeakDb() const noexcept
{
    return m_outPeakDb.load(std::memory_order_relaxed);
}

void StripChain::reset() noexcept
{
    // ── Alle zehn, nicht acht ────────────────────────────────────
    //
    // Hier fehlten das Entrauschen und der Clipper -- die beiden
    // Stufen, die nach der urspruenglichen Acht dazugekommen sind.
    // Beide halten Zustand: der Entrauscher eine
    // Verzoegerungsleitung ueber drei Rahmen (1440 Abtastwerte,
    // 30 ms) plus den Zustand des Netzes, der Clipper die
    // Vorgeschichte seines Halbbandfilters. Ohne Ruecksetzen
    // beginnt die naechste Durchsage mit 30 ms der vorigen.
    //
    // Eine Stufe, die beim Hinzufuegen in processMono() eingetragen
    // wird, aber nicht hier, faellt erst im Betrieb auf -- und dann
    // als Knacken, das niemand zuordnet.
    m_nr.reset();
    m_gate.reset();
    m_eq.reset();
    m_deEss.reset();
    m_comp.reset();
    m_tube.reset();
    m_pudu.reset();
    m_reverb.reset();
    m_clip.reset();
    m_limiter.reset();

    // Und die Summierer des Scheitelfaktors: ohne das mischt die
    // einzige Dichtezahl der Oberflaeche ueber die Sendepause
    // hinweg und zeigt einen Wert, den es nie gab.
    m_crestSumIn = 0.0;
    m_crestSumOut = 0.0;
    m_crestPeakIn = 0.0f;
    m_crestPeakOut = 0.0f;
    m_crestFrames = 0;
}

} // namespace Longpath
