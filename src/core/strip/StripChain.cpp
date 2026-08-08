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
// =================================================================

#include "core/strip/StripChain.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

StripChain::StripChain()
{
    for (auto& f : m_stageOn) { f.store(false, std::memory_order_relaxed); }
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
    // bit-exact rather than on not calling them.
    constexpr int kMono = 1;

    if (stageEnabled(Stage::Gate))  { m_gate.process(samples, frames, kMono); }
    if (stageEnabled(Stage::Eq))    { m_eq.process(samples, frames, kMono); }
    if (stageEnabled(Stage::DeEss)) { m_deEss.process(samples, frames, kMono); }
    if (stageEnabled(Stage::Comp))  { m_comp.process(samples, frames, kMono); }
    if (stageEnabled(Stage::Tube))  { m_tube.process(samples, frames, kMono); }
    if (stageEnabled(Stage::Pudu))  { m_pudu.process(samples, frames, kMono); }
    if (stageEnabled(Stage::Reverb)) { m_reverb.process(samples, frames, kMono); }
    // Last, and for a reason: a brickwall that anything runs after is
    // not a brickwall. Everything above can add gain — the tube, the
    // exciter, the compressor's make-up — and this is what stops the
    // sum reaching the modulator hotter than it should.
    if (stageEnabled(Stage::Limiter)) {
        m_limiter.process(samples, frames, kMono);
    }

    m_outPeakDb.store(peakDb(samples, frames), std::memory_order_relaxed);
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

} // namespace NereusSDR
