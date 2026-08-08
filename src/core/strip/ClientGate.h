#pragma once

// =================================================================
// src/core/strip/ClientGate.h  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR (https://github.com/aethersdr/AetherSDR),
// GPLv3, primary author Jeremy [KK7GWY]:
//   src/core/ClientGate.h at 31b29583
//
// AetherSDR carries no per-file licence headers, so per
// docs/attribution/HOW-TO-PORT.md rule 6 the citation is at project
// level: there is no verbatim block to copy. Both projects are GPLv3,
// so the code carries forward under the same licence per GPLv3 section 5.
//
// This is the Aetherial Audio Channel Strip's DSP, ported before any of
// its user interface. The stages are self-contained, Qt-free apart from
// QtGlobal, and lock-free by construction — which makes them testable
// without a radio, an audio device or a window, and that is the whole
// reason for taking them first.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Ported to NereusSDR by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork). Namespace AetherSDR →
//                 NereusSDR; include paths rebased onto core/strip/.
//                 DSP unchanged.
// =================================================================


#include <atomic>
#include <cstdint>
#include <vector>

namespace NereusSDR {

// Downward expander / noise gate — TX DSP chain Phase 2 (#1661).  The
// same DSP core covers both behaviours: a low ratio with a shallow range
// gives gentle expansion that preserves natural decay, while a high ratio
// with a deep range gives a hard gate that slams shut below threshold.
// The Mode setter snaps ratio + range to preset pairs so the UI can
// offer a one-click Expander ↔ Gate toggle without hiding the underlying
// knobs from power users.
//
// Thread model mirrors ClientComp: UI thread writes via set*() which
// update atomics + bump a version counter; the audio thread reads the
// version once per block and recaches values.  No locks, no allocations
// in process(), no exceptions.
class ClientGate {
public:
    enum class Mode : uint8_t {
        Expander = 0,   // soft — ratio 2:1, range -15 dB
        Gate     = 1,   // hard — ratio 10:1, range -40 dB
    };

    ClientGate();
    ~ClientGate() = default;

    ClientGate(const ClientGate&)            = delete;
    ClientGate& operator=(const ClientGate&) = delete;

    // Main thread — call before first process() and on sample-rate change.
    void prepare(double sampleRate);

    // Main thread — global enable / bypass. Lock-free.
    void setEnabled(bool on) noexcept;
    bool isEnabled() const noexcept;

    // Snap ratio + range to canonical presets.  Does not affect other
    // parameters (attack/release/hold/threshold); user can fine-tune
    // from either preset.
    void setMode(Mode m) noexcept;
    Mode mode() const noexcept;

    void  setThresholdDb(float db) noexcept;       // -80 .. 0 dB
    float thresholdDb() const noexcept;
    void  setRatio(float ratio) noexcept;          // 1.0 (off) .. 10.0 (hard gate)
    float ratio() const noexcept;
    void  setAttackMs(float ms) noexcept;          // 0.1 .. 100 ms
    float attackMs() const noexcept;
    void  setReleaseMs(float ms) noexcept;         // 5 .. 2000 ms
    float releaseMs() const noexcept;
    void  setHoldMs(float ms) noexcept;            // 0 .. 500 ms — gain freeze before release
    float holdMs() const noexcept;
    void  setFloorDb(float db) noexcept;           // -80 .. 0 dB, max attenuation floor
    float floorDb() const noexcept;
    void  setReturnDb(float db) noexcept;          // 0 .. 20 dB hysteresis — gate stays open
                                                    // until envelope drops to threshold - return
    float returnDb() const noexcept;
    void  setLookaheadMs(float ms) noexcept;       // 0 .. 5 ms — delays signal so detector
                                                    // sees transients before the gain stage
    float lookaheadMs() const noexcept;

    // Audio thread — process in place.  channels must be 1 or 2.
    void process(float* interleaved, int frames, int channels) noexcept;

    // Audio thread — flush envelope/hold state (e.g. on TX start).
    void reset() noexcept;

    // UI thread — read-only snapshots of the latest detector state.
    float inputPeakDb() const noexcept;
    float outputPeakDb() const noexcept;
    float gainReductionDb() const noexcept;   // ≤ 0 dB (attenuation)
    bool  gateOpen() const noexcept;          // true when signal is above threshold

    double sampleRate() const noexcept { return m_sampleRate; }

private:
    struct Atomics {
        std::atomic<bool>     enabled{false};
        std::atomic<uint8_t>  mode{static_cast<uint8_t>(Mode::Expander)};
        std::atomic<float>    thresholdDb{-40.0f};
        std::atomic<float>    ratio{2.0f};
        std::atomic<float>    attackMs{0.5f};
        std::atomic<float>    releaseMs{100.0f};
        std::atomic<float>    holdMs{20.0f};
        std::atomic<float>    floorDb{-15.0f};
        std::atomic<float>    returnDb{2.0f};
        std::atomic<float>    lookaheadMs{0.0f};
        std::atomic<uint64_t> version{0};
    };

    struct Cached {
        float thresholdDb{-40.0f};
        float closeThresholdDb{-42.0f}; // thresholdDb - returnDb (hysteresis)
        float slope{1.0f};              // ratio - 1, pre-computed
        float attackCoeff{0.0f};        // gain-smoother attack (1 - exp(-1/(fs·τ)))
        float releaseCoeff{0.0f};       // gain-smoother release
        float envReleaseCoeff{0.0f};    // fast envelope-detector release (fixed 10 ms)
        int   holdSamples{0};           // hold_ms converted to sample count
        float floorDb{-15.0f};          // max attenuation floor (negative dB)
        int   lookaheadSamples{0};      // sample-count delay for look-ahead path
    };

    struct Meters {
        std::atomic<float> inputPeakDb{-120.0f};
        std::atomic<float> outputPeakDb{-120.0f};
        std::atomic<float> gainReductionDb{0.0f};
        std::atomic<bool>  gateOpen{true};
    };

    void recacheIfDirty() noexcept;
    float staticCurveGainDb(float envDb) const noexcept;

    double   m_sampleRate{24000.0};
    Atomics  m_atomics;
    Cached   m_cached;
    Meters   m_meters;

    // Audio-thread state — accessed only from process().
    uint64_t m_lastVersion{0};
    float    m_envLin{0.0f};          // peak envelope (linear)
    float    m_currentGainDb{0.0f};   // the gain currently being applied (≤ 0)
    int      m_holdCountdown{0};      // samples remaining in hold phase
    bool     m_isOpen{false};         // Schmitt-trigger state — above-threshold latch

    // Look-ahead delay line — allocated in prepare() with enough headroom
    // for the max lookahead_ms setting.  Stereo interleaved like the
    // external buffer so process() can do one read per frame.
    static constexpr int kMaxLookaheadMs = 5;
    std::vector<float> m_delay;        // stereo-interleaved delay line
    int m_delayCap{0};                 // capacity in frames
    int m_delayWrite{0};               // write index (frame-space)
};

} // namespace NereusSDR
