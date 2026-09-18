// =================================================================
// src/core/strip/StripChain.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See StripChain.h for the order, the two rules
// and why the runner is ours rather than ported.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-09-17 — Non-finite guard around every stage; see the header.
// =================================================================

#include "core/strip/StripChain.h"

#include <algorithm>
#include <cmath>

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
    m_gate.prepare(sampleRate);
    m_eq.prepare(sampleRate);
    m_deEss.prepare(sampleRate);
    m_comp.prepare(sampleRate);
    m_tube.prepare(sampleRate);
    m_pudu.prepare(sampleRate);
    m_reverb.prepare(sampleRate);
    m_limiter.prepare(sampleRate);
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
    m_inPeakDb.store(peakDb(samples, frames), std::memory_order_relaxed);

    // Every stage has its own enable and the chain mirrors it, so the
    // two always agree. Skipping the call as well is not redundant: a
    // stage that is off should cost nothing, and going through it
    // anyway would rest the guarantee on eight separate bypasses being
    // bit-exact rather than on not calling them. runStage() does the
    // skip, and wraps each call in the non-finite guard.
    runStage(Stage::Gate,   m_gate,   samples, frames);
    runStage(Stage::Eq,     m_eq,     samples, frames);
    runStage(Stage::DeEss,  m_deEss,  samples, frames);
    runStage(Stage::Comp,   m_comp,   samples, frames);
    runStage(Stage::Tube,   m_tube,   samples, frames);
    runStage(Stage::Pudu,   m_pudu,   samples, frames);
    runStage(Stage::Reverb, m_reverb, samples, frames);
    // Last, and for a reason: a brickwall that anything runs after is
    // not a brickwall. Everything above can add gain — the tube, the
    // exciter, the compressor's make-up — and this is what stops the
    // sum reaching the modulator hotter than it should.
    runStage(Stage::Limiter, m_limiter, samples, frames);

    m_outPeakDb.store(peakDb(samples, frames), std::memory_order_relaxed);
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
    m_gate.reset();
    m_eq.reset();
    m_deEss.reset();
    m_comp.reset();
    m_tube.reset();
    m_pudu.reset();
    m_reverb.reset();
    m_limiter.reset();
}

} // namespace Longpath
