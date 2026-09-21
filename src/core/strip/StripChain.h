#pragma once

// =================================================================
// src/core/strip/StripChain.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original, over DSP ported from AetherSDR
// (https://github.com/aethersdr/AetherSDR, GPLv3, primary author
// Jeremy [KK7GWY]). The stage order and the stage set follow
// AetherSDR's `defaultChain()` in src/core/AudioEngine.cpp at
// `31b29583`; the runner itself is written for NereusSDR because
// AetherSDR dispatches its chain from inside AudioEngine and
// NereusSDR's transmit audio does not pass through there.
//
// The Nereus Audio Channel Strip's stages, in order, as one object
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
// Modification history (NereusSDR):
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
#include "core/strip/ClientGate.h"
#include "core/strip/ClientPudu.h"
#include "core/strip/ClientReverb.h"
#include "core/strip/ClientTube.h"
#include "core/strip/MicSpectrum.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace Longpath {

class StripChain {
public:
    // Order matches AetherSDR's defaultChain(); the limiter is not in
    // that list because upstream applies it separately at the very end
    // of the output path, and it is last here for the same reason.
    enum class Stage : uint8_t {
        Gate = 0, Eq, DeEss, Comp, Tube, Pudu, Reverb, Limiter, Count
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
    float inputPeakDb() const noexcept;
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

    // Written on the audio thread once per block, read by the meter.
    // Plain relaxed stores: a meter that is one block stale is a meter,
    // and paying for ordering on the transmit path to make a picture
    // slightly fresher is the wrong trade.
    std::atomic<float> m_inPeakDb{-120.0f};
    std::atomic<float> m_outPeakDb{-120.0f};

    // The non-finite guard. One stage at a time: snapshot the block,
    // run the stage, and if what came out is not finite put the
    // snapshot back and reset the stage. Defined in the .cpp; every
    // instantiation lives there.
    template <typename S>
    void runStage(Stage which, S& stage, float* samples, int frames) noexcept;

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
