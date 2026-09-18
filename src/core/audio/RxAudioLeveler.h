#pragma once
// =================================================================
// src/core/audio/RxAudioLeveler.h  (Longpath)
// =================================================================
//
// Source attribution (Zeus station engine -- GPL-2.0-or-later):
//
//   Copyright (C) 2026 Douglas J. Cerrato (KB2UKA), Christian Suarez
//       (N9WAR), and contributors -- per
//       https://github.com/Zeus-SDR/station-engine, LICENSE and
//       ATTRIBUTIONS.md at the repository root.
//
//   This file is a port of the receive-audio leveler ("RX LVLR") in
//   Station.Engine.Hosting/DspPipelineService.cs at @8970f2d (Release
//   v2.0.26): the RxAudioLevelerState struct, the RxLeveler* constants,
//   ApplyRxAudioLeveler / ApplyRxAudioLevelerCore, SoftLimitRxAudioSample,
//   and the RxConstantLoudness* evidence gate (AdvanceRxConstantLoudness-
//   Evidence, RxConstantLoudnessEvidenceIsCurrent, RxConstantLoudness-
//   EvidenceAllowsBoost). The operator-facing profile (RxLevelerConfig)
//   comes from Zeus.Contracts/Dtos.cs and its normalisation from
//   Station.Engine.Hosting/RadioService.cs. There is no Thetis or
//   AetherSDR equivalent ("this always-on leveler (which Thetis lacks)",
//   DspPipelineService.cs:226); Zeus is the sole source. Every function
//   below cites the lines it was taken from. The Zeus station engine is
//   GPL-2.0-or-later; Longpath is GPLv3, so the "or later" option is
//   exercised. "Zeus" and "ZeusSDR" are trademarks of the maintainers;
//   this is nominative use. Record: docs/attribution/ZEUS-PROVENANCE.md.
//
// What it does. WDSP's AGC holds the *signal* at a set level; it cannot
// tell a weak station from band noise, so with AGC-T set for the weak
// one the noise between words comes up just as loud. The leveler sits
// after the whole WDSP chain (post-AGC, post-NR, post-AF-gain) and
// corrects perceived loudness slowly towards a target RMS:
//
//   * CUT is unconditional -- a sudden strong signal is contained within
//     the block, with a per-block peak guard so gain banked across a
//     pause is never dumped onto a loud block ("blasts the speaker").
//   * BOOST is fail-closed -- positive makeup is only allowed while an
//     independent RF measurement (the in-passband SNR estimator fed from
//     the spectrum, see PassbandSnrTracker) says there is a resolved
//     signal in the passband, and never near ADC overload. Audio
//     amplitude alone cannot distinguish weak speech from receiver
//     noise, so noise alone can never open the boost.
//   * Below a gate (-50 dBFS RMS) the applied gain fades to unity while
//     the controller remembers its value for a hang time, so a pause
//     does not restart the attack and does not lift the noise.
//
// Auto mode uses the upstream constants. Custom mode exposes target,
// maximum boost, attack, release and hang, exactly as the Zeus profile.
//
// Longpath deviations (each marked at the site):
//   D1  Stereo. Zeus levels its mono RX bus; Longpath's WDSP output is a
//       left/right pair (dual mono, or binaural). One gain is computed
//       from both legs (RMS over both, peak over both) and applied to
//       both, so the stereo image is never shifted.
//   D2  Block-rate independence. Zeus's "per block" constants are tuned
//       for its 30 Hz audio tick (~1600 frames). Longpath hands the
//       leveler one WDSP output block at a time (256 frames at 192 kHz
//       input, 5.3 ms). Every per-block slew and hold is scaled by
//       blockMs / (1000/30) so the rate in dB/s and the hold in ms match
//       upstream regardless of block size -- the same conversion Zeus
//       itself applies to the Custom attack/release/hang.
//   D3  The evidence cross-check takes the current in-passband power from
//       the same spectrum the noise estimate came from, not from the
//       WDSP stage meter (Zeus: v2.SignalAv + rxCalOffsetDb). Longpath
//       calibrates its S-meter and its spectrum separately, so a meter
//       vs. spectrum comparison would carry that offset; measured
//       against itself the 1.5 / 0.25 dB thresholds mean what they say.
//       The caller passes it as currentSignalDb; the gate arithmetic is
//       unchanged.
//
// Plain C++, no Qt, no allocation in process(); the tests drive it with
// synthetic audio and RxChannel calls it on the DSP thread.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-18 -- Ported for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude, from the Zeus station
//                 engine @8970f2d. Straight C# -> C++ port: same
//                 constants, same arithmetic, same branch order. C#
//                 records became structs, Span<float> became
//                 pointer+count (times two legs, D1), the per-block
//                 constants gained the block-duration scale (D2).
// =================================================================

#include <cstdint>

namespace Longpath {

// From Zeus station-engine Zeus.Contracts/Dtos.cs:862-893 [@8970f2d]
//   public enum RxLevelerMode { Auto, Custom }
//   public sealed record RxLevelerConfig(...)
struct RxLevelerConfig {
    enum class Mode : int { Auto = 0, Custom = 1 };

    Mode   mode{Mode::Auto};
    double targetRmsDb{kDefaultTargetRmsDb};
    double maxBoostDb{kDefaultMaxBoostDb};
    int    attackMs{kDefaultAttackMs};
    int    releaseMs{kDefaultReleaseMs};
    int    hangMs{kDefaultHangMs};

    // From Zeus station-engine Zeus.Contracts/Dtos.cs:878-892 [@8970f2d]
    static constexpr double kMinTargetRmsDb     = -30.0;
    static constexpr double kMaxTargetRmsDb     = -6.0;
    static constexpr double kDefaultTargetRmsDb = -18.0;
    static constexpr double kMinBoostDb         = 0.0;
    static constexpr double kMaxBoostLimitDb    = 24.0;
    static constexpr double kDefaultMaxBoostDb  = 24.0;
    static constexpr int    kMinAttackMs        = 20;
    static constexpr int    kMaxAttackMs        = 2000;
    static constexpr int    kDefaultAttackMs    = 400;
    static constexpr int    kMinReleaseMs       = 20;
    static constexpr int    kMaxReleaseMs       = 5000;
    static constexpr int    kDefaultReleaseMs   = 150;
    static constexpr int    kMinHangMs          = 0;
    static constexpr int    kMaxHangMs          = 2000;
    static constexpr int    kDefaultHangMs      = 600;

    // From Zeus station-engine Station.Engine.Hosting/RadioService.cs:4248-4266 [@8970f2d]
    //   NormalizeRxLevelerConfig: clamp every field, non-finite -> default.
    static RxLevelerConfig normalized(const RxLevelerConfig& in);

    bool operator==(const RxLevelerConfig&) const = default;
};

// From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:174-194 [@8970f2d]
//   internal struct RxAudioLevelerState
struct RxAudioLevelerState {
    double gainDb{0.0};
    bool   diagnosticsValid{false};
    double inputRmsDbfs{0.0};
    double inputPeakDbfs{0.0};
    double outputRmsDbfs{0.0};
    double outputPeakDbfs{0.0};
    double desiredGainDb{0.0};
    double appliedGainDb{0.0};
    double gainDeltaDb{0.0};
    double peakHeadroomDb{0.0};
    double preLimitPeakDbfs{0.0};
    double outputLimitReductionDb{0.0};
    int    outputLimitSampleCount{0};
    int    pauseHoldBlocks{0};
    bool   boostSlewLimited{false};
    bool   peakLimited{false};
    bool   outputLimited{false};
    bool   releaseToUnity{false};
};

class RxAudioLeveler {
public:
    // From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:71 [@8970f2d]
    //   public const int AudioOutputRateHz = 48_000;
    static constexpr int kSampleRateHz = 48000;

    // From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:213-280 [@8970f2d]
    // (the RxLeveler* / RxConstantLoudness* constants, in upstream order)
    static constexpr double kTargetRmsDb              = -18.0;   // :213 RxLevelerTargetRmsDb
    static constexpr double kGateRmsDb                = -50.0;   // :220 RxLevelerGateRmsDb
    static constexpr double kGateSoftWindowDb         = 8.0;     // :228 RxLevelerGateSoftWindowDb
    static constexpr double kConstantLoudnessMaxBoostDb = 24.0;  // :238 RxConstantLoudnessMaxBoostDb
    static constexpr double kMinConfidence            = 0.10;    // :240 RxConstantLoudnessMinConfidence
    static constexpr double kMinCurrentTotalExcessDb  = 1.5;     // :243 RxConstantLoudnessMinCurrentTotalExcessDb
    static constexpr double kReleaseTotalExcessDb     = 0.25;    // :244 RxConstantLoudnessReleaseTotalExcessDb
    static constexpr int    kAcquireFrames            = 3;       // :247 RxConstantLoudnessAcquireFrames
    static constexpr int    kReleaseFrames            = 6;       // :248 RxConstantLoudnessReleaseFrames
    static constexpr double kAdcVetoDbfs              = -3.0;    // :249 RxConstantLoudnessAdcVetoDbfs
    static constexpr double kAdcUnavailableDbfs       = -200.0;  // :250 RxConstantLoudnessAdcUnavailableDbfs
    static constexpr double kMeterUnavailableDbm      = -200.0;  // :251 RxConstantLoudnessMeterUnavailableDbm
    static constexpr int64_t kEvidenceMaxAgeMs        = 750;     // :252 RxConstantLoudnessEvidenceMaxAgeMs
    static constexpr double kMaxCutDb                 = -24.0;   // :253 RxLevelerMaxCutDb
    static constexpr double kBoostSlewDbPerBlock      = 2.0;     // :254 RxLevelerBoostSlewDbPerBlock
    static constexpr double kFastBoostSlewDbPerBlock  = 2.5;     // :255 RxLevelerFastBoostSlewDbPerBlock
    static constexpr double kFastBoostHeadroomDb      = 6.0;     // :256 RxLevelerFastBoostHeadroomDb
    static constexpr double kVeryFastBoostSlewDbPerBlock = 3.0;  // :257 RxLevelerVeryFastBoostSlewDbPerBlock
    static constexpr double kVeryFastBoostHeadroomDb  = 10.0;    // :258 RxLevelerVeryFastBoostHeadroomDb
    static constexpr double kVeryFastBoostGateRmsDb   = -45.0;   // :259 RxLevelerVeryFastBoostGateRmsDb
    static constexpr double kCrestCatchupBoostSlewDbPerBlock = 3.0; // :260 RxLevelerCrestCatchupBoostSlewDbPerBlock
    static constexpr double kCrestCatchupMinCrestDb   = 8.0;     // :262 RxLevelerCrestCatchupMinCrestDb
    static constexpr double kCrestCatchupMaxRmsDb     = -28.0;   // :263 RxLevelerCrestCatchupMaxRmsDb
    static constexpr double kCrestCatchupMinPeakDb    = -52.0;   // :264 RxLevelerCrestCatchupMinPeakDb
    static constexpr double kCrestCatchupMinGainGapDb = 6.0;     // :265 RxLevelerCrestCatchupMinGainGapDb
    static constexpr double kMemoryCatchupGateRmsDb   = -66.0;   // :266 RxLevelerMemoryCatchupGateRmsDb
    static constexpr double kMemoryCatchupGatePeakDb  = -56.0;   // :267 RxLevelerMemoryCatchupGatePeakDb
    static constexpr double kMemoryCatchupMinGainDb   = 3.0;     // :268 RxLevelerMemoryCatchupMinGainDb
    static constexpr double kSmoothCutDb              = 6.0;     // :269 RxLevelerSmoothCutDb
    static constexpr double kEvidenceLossReleaseDbPerBlock = 6.0; // :272 RxLevelerEvidenceLossReleaseDbPerBlock
    static constexpr int    kPauseHoldBlocks          = 18;      // :273 RxLevelerPauseHoldBlocks
    static constexpr double kPauseMemoryDecayDbPerBlock = 4.5;   // :274 RxLevelerPauseMemoryDecayDbPerBlock
    static constexpr int    kGainRampMaxSamples       = 256;     // :275 RxLevelerGainRampMaxSamples
    static constexpr double kLargeTransitionDb        = 6.0;     // :276 RxLevelerLargeTransitionDb
    static constexpr int    kEmergencyCutRampSamples  = 64;      // :277 RxLevelerEmergencyCutRampSamples
    static constexpr double kPeakTarget               = 0.74;    // :278 RxLevelerPeakTarget
    static constexpr double kOutputSoftKnee           = 0.74;    // :279 RxLevelerOutputSoftKnee
    static constexpr double kOutputPeakCeiling        = 0.84;    // :280 RxLevelerOutputPeakCeiling
    // Upstream also declares RxLevelerMaxBoostDb = 3.0 (:233) for a legacy
    // audio-only overload and RxLevelerCrestCatchupHeadroomDb = 16.0 (:261)
    // which the core never reads; neither is used on the production path
    // ported here, so neither is carried.

    // Longpath deviation D2: the block length the per-block constants
    // above were tuned for (Zeus TickPeriod = 1000/30 ms,
    // DspPipelineService.cs:74 [@8970f2d]).
    static constexpr double kReferenceBlockMs = 1000.0 / 30.0;

    struct Inputs {
        bool   enabled{false};
        bool   rfSignalResolved{false};
        bool   adcOverloadRisk{true};
        double levelReferenceOffsetDb{0.0};
        RxLevelerConfig config{};
    };

    // From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:384-413 [@8970f2d]
    //   ApplyRxAudioLeveler(samples, ref state, enabled, rfSignalResolved,
    //                       adcOverloadRisk, levelReferenceOffsetDb, config)
    // Longpath deviation D1: `left`/`right` are the two WDSP output legs
    // (`right` may be nullptr for a mono block, e.g. the tests); one
    // gain is derived from and applied to both.
    void process(float* left, float* right, int frames, const Inputs& in);

    const RxAudioLevelerState& state() const { return m_state; }
    void reset() { m_state = RxAudioLevelerState{}; }

    // From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:839-848 [@8970f2d]
    static float softLimitSample(float sample);

    // ── Evidence gate (constant-loudness boost permission) ────────────
    struct EvidenceDecision {
        bool resolved{false};
        int  acquireHits{0};
        int  releaseMisses{0};
        bool refreshAge{false};
    };

    // From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:10289-10321 [@8970f2d]
    static EvidenceDecision advanceEvidence(bool spectrumFresh,
                                            bool measurementAllowsBoost,
                                            bool adcRisk,
                                            bool wasResolved,
                                            int acquireHits,
                                            int releaseMisses);

    // From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:10323-10328 [@8970f2d]
    static bool evidenceIsCurrent(int64_t evidenceMs, int64_t nowMs);

    // From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:10330-10356 [@8970f2d]
    //   quality.IsValid / quality.Confidence / quality.IntegratedNoiseDb are
    //   passed as scalars so this header does not depend on the estimator.
    static bool evidenceAllowsBoost(bool qualityValid,
                                    double qualityConfidence,
                                    double integratedNoiseDb,
                                    double currentSignalDb,
                                    double rxCalibrationDb,
                                    double adcPkDbfs,
                                    bool wasResolved);

    // From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:10260-10265 [@8970f2d]
    //   bool adcRisk = !float.IsFinite(adcPkDbfs) || adcPkDbfs <= Unavailable || adcPkDbfs > Veto;
    static bool adcOverloadRisk(double adcPkDbfs);

    // From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:4453-4456 [@8970f2d]
    static double linearToDbfs(double value);

private:
    // From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:415-701 [@8970f2d]
    void processCore(float* left, float* right, int frames,
                     bool allowBoost, double maxBoostDb,
                     double levelReferenceOffsetDb, double targetRmsDb,
                     bool hasCustomAttack, int customAttackMs,
                     bool hasCustomRelease, int customReleaseMs,
                     bool hasCustomHang, int customHangMs,
                     bool forceReleaseToUnity);

    RxAudioLevelerState m_state;
};

} // namespace Longpath
