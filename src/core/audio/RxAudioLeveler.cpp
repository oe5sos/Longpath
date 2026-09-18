// =================================================================
// src/core/audio/RxAudioLeveler.cpp  (Longpath)
// =================================================================
//
// Source attribution (Zeus station engine -- GPL-2.0-or-later):
//
//   Copyright (C) 2026 Douglas J. Cerrato (KB2UKA), Christian Suarez
//       (N9WAR), and contributors -- per
//       https://github.com/Zeus-SDR/station-engine, LICENSE and
//       ATTRIBUTIONS.md at the repository root.
//
//   Port of the RX audio leveler and its constant-loudness evidence gate
//   from Station.Engine.Hosting/DspPipelineService.cs at @8970f2d
//   (Release v2.0.26). See RxAudioLeveler.h for the full attribution and
//   the list of Longpath deviations (D1 stereo, D2 block-rate scaling,
//   D3 spectrum-sourced cross-check).
//
// =================================================================
// Modification history (Longpath):
//   2026-09-18 -- Ported for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude, from the Zeus station
//                 engine @8970f2d.
// =================================================================

#include "core/audio/RxAudioLeveler.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Longpath {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:362-363 [@8970f2d]
//   private static double DbToLinear(double db) =>
//       double.IsFinite(db) ? Math.Pow(10.0, db / 20.0) : 1.0;
double dbToLinear(double db)
{
    return std::isfinite(db) ? std::pow(10.0, db / 20.0) : 1.0;
}

// Longpath deviation D1: the two WDSP legs are one block. `legs` is 1
// for a mono call (right == nullptr) and 2 otherwise.
inline int legCount(const float* right)
{
    return (right != nullptr) ? 2 : 1;
}

} // namespace

// ---------------------------------------------------------------------------
// RxLevelerConfig
// ---------------------------------------------------------------------------

// From Zeus station-engine Station.Engine.Hosting/RadioService.cs:4248-4266 [@8970f2d]
RxLevelerConfig RxLevelerConfig::normalized(const RxLevelerConfig& in)
{
    RxLevelerConfig out;
    //   var mode = Enum.IsDefined(value.Mode) ? value.Mode : RxLevelerMode.Auto;
    out.mode = (in.mode == Mode::Custom) ? Mode::Custom : Mode::Auto;
    //   double target = double.IsFinite(value.TargetRmsDb)
    //       ? Math.Clamp(value.TargetRmsDb, MinTargetRmsDb, MaxTargetRmsDb)
    //       : DefaultTargetRmsDb;
    out.targetRmsDb = std::isfinite(in.targetRmsDb)
        ? std::clamp(in.targetRmsDb, kMinTargetRmsDb, kMaxTargetRmsDb)
        : kDefaultTargetRmsDb;
    //   double maxBoost = double.IsFinite(value.MaxBoostDb)
    //       ? Math.Clamp(value.MaxBoostDb, MinBoostDb, MaxBoostLimitDb)
    //       : DefaultMaxBoostDb;
    out.maxBoostDb = std::isfinite(in.maxBoostDb)
        ? std::clamp(in.maxBoostDb, kMinBoostDb, kMaxBoostLimitDb)
        : kDefaultMaxBoostDb;
    out.attackMs  = std::clamp(in.attackMs,  kMinAttackMs,  kMaxAttackMs);
    out.releaseMs = std::clamp(in.releaseMs, kMinReleaseMs, kMaxReleaseMs);
    out.hangMs    = std::clamp(in.hangMs,    kMinHangMs,    kMaxHangMs);
    return out;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:4453-4456 [@8970f2d]
//   private static double AudioLinearToDbfsRaw(double value) =>
//       double.IsFinite(value) && value > 0.0
//           ? 20.0 * Math.Log10(Math.Max(value, 1e-12))
//           : double.NaN;
double RxAudioLeveler::linearToDbfs(double value)
{
    return (std::isfinite(value) && value > 0.0)
        ? 20.0 * std::log10(std::max(value, 1e-12))
        : kNaN;
}

// From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:839-848 [@8970f2d]
float RxAudioLeveler::softLimitSample(float sample)
{
    const float a = std::fabs(sample);
    if (a <= static_cast<float>(kOutputSoftKnee)) { return sample; }

    const double kneeWidth = std::max(1.0e-6, kOutputPeakCeiling - kOutputSoftKnee);
    const double over = (a - kOutputSoftKnee) / kneeWidth;
    const double limited = kOutputSoftKnee + kneeWidth * std::tanh(over);
    return std::copysign(static_cast<float>(limited), sample);
}

// From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:10260-10265 [@8970f2d]
bool RxAudioLeveler::adcOverloadRisk(double adcPkDbfs)
{
    return !std::isfinite(adcPkDbfs)
        || adcPkDbfs <= kAdcUnavailableDbfs
        || adcPkDbfs > kAdcVetoDbfs;
}

// ---------------------------------------------------------------------------
// Evidence gate
// ---------------------------------------------------------------------------

// From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:10289-10321 [@8970f2d]
RxAudioLeveler::EvidenceDecision RxAudioLeveler::advanceEvidence(
    bool spectrumFresh, bool measurementAllowsBoost, bool adcRisk,
    bool wasResolved, int acquireHits, int releaseMisses)
{
    if (adcRisk) {
        return {false, 0, 0, true};
    }
    if (!spectrumFresh) {
        return {wasResolved, std::max(0, acquireHits), std::max(0, releaseMisses), false};
    }

    if (!wasResolved) {
        if (!measurementAllowsBoost) {
            return {false, 0, 0, true};
        }
        const int hits = std::max(0, acquireHits) + 1;
        return hits >= kAcquireFrames
            ? EvidenceDecision{true, 0, 0, true}
            : EvidenceDecision{false, hits, 0, true};
    }

    if (measurementAllowsBoost) {
        return {true, 0, 0, true};
    }

    const int misses = std::max(0, releaseMisses) + 1;
    return misses < kReleaseFrames
        ? EvidenceDecision{true, 0, misses, true}
        : EvidenceDecision{false, 0, 0, true};
}

// From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:10323-10328 [@8970f2d]
bool RxAudioLeveler::evidenceIsCurrent(int64_t evidenceMs, int64_t nowMs)
{
    return evidenceMs != std::numeric_limits<int64_t>::min()
        && nowMs >= evidenceMs
        && nowMs - evidenceMs <= kEvidenceMaxAgeMs;
}

// From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:10330-10356 [@8970f2d]
bool RxAudioLeveler::evidenceAllowsBoost(bool qualityValid,
                                         double qualityConfidence,
                                         double integratedNoiseDb,
                                         double currentSignalDb,
                                         double rxCalibrationDb,
                                         double adcPkDbfs,
                                         bool wasResolved)
{
    if (!qualityValid
        || qualityConfidence < kMinConfidence
        || !std::isfinite(currentSignalDb)
        || currentSignalDb <= kMeterUnavailableDbm
        || !std::isfinite(rxCalibrationDb)
        || !std::isfinite(adcPkDbfs)
        || adcPkDbfs <= kAdcUnavailableDbfs
        || adcPkDbfs > kAdcVetoDbfs) {
        return false;
    }

    // The passband estimator is deliberately averaged for stable reports.
    // Cross-check its noise estimate with the current stage meter so a
    // vanished station cannot keep old averaged SNR alive and open makeup.
    // (Longpath deviation D3: the "current" value is the current-frame
    // in-passband power from the same spectrum; see the header.)
    const double currentExcessDb = currentSignalDb - (integratedNoiseDb + rxCalibrationDb);
    const double thresholdDb = wasResolved
        ? kReleaseTotalExcessDb
        : kMinCurrentTotalExcessDb;
    return currentExcessDb >= thresholdDb;
}

// ---------------------------------------------------------------------------
// The leveler
// ---------------------------------------------------------------------------

// From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:384-413 [@8970f2d]
void RxAudioLeveler::process(float* left, float* right, int frames, const Inputs& in)
{
    const RxLevelerConfig& profile = in.config;
    const bool custom = profile.mode == RxLevelerConfig::Mode::Custom;
    if (!in.enabled && (m_state.gainDb > 0.0 || m_state.appliedGainDb > 0.0)) {
        m_state.releaseToUnity = true;
    }

    processCore(
        left, right, frames,
        /* allowBoost */ in.enabled && !m_state.releaseToUnity
            && in.rfSignalResolved && !in.adcOverloadRisk,
        /* maxBoostDb */ custom ? profile.maxBoostDb : kConstantLoudnessMaxBoostDb,
        in.levelReferenceOffsetDb,
        /* targetRmsDb */ custom ? profile.targetRmsDb : kTargetRmsDb,
        custom, profile.attackMs,
        custom, profile.releaseMs,
        custom, profile.hangMs,
        /* forceReleaseToUnity */ m_state.releaseToUnity);

    if (m_state.releaseToUnity && m_state.gainDb <= 0.0 && m_state.appliedGainDb <= 0.0) {
        m_state.releaseToUnity = false;
    }
}

// From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:415-701 [@8970f2d]
//   private static void ApplyRxAudioLevelerCore(...)
// Upstream comments are carried verbatim at the site they explain.
void RxAudioLeveler::processCore(float* left, float* right, int frames,
                                 bool allowBoost, double maxBoostDb,
                                 double levelReferenceOffsetDb, double targetRmsDb,
                                 bool hasCustomAttack, int customAttackMs,
                                 bool hasCustomRelease, int customReleaseMs,
                                 bool hasCustomHang, int customHangMs,
                                 bool forceReleaseToUnity)
{
    if (frames <= 0 || left == nullptr) { return; }
    RxAudioLevelerState& state = m_state;
    const int legs = legCount(right);

    const double blockDurationMs = frames * 1000.0 / kSampleRateHz;
    // Longpath deviation D2: the upstream per-block constants are per
    // 1000/30 ms; scale them to this block so dB/s and ms stay the same.
    const double blockScale = blockDurationMs / kReferenceBlockMs;

    const double customBoostSlewDb = hasCustomAttack
        ? maxBoostDb * blockDurationMs / customAttackMs
        : kNaN;
    // Release time is defined against the fixed +24 dB safety-bounded
    // makeup range, so reducing Max Boost never accidentally makes release
    // slower. Peak-urgent cuts continue to bypass this audible timing.
    const double customReleaseSlewDb = hasCustomRelease
        ? kConstantLoudnessMaxBoostDb * blockDurationMs / customReleaseMs
        : kNaN;
    const int customPauseHoldBlocks = hasCustomHang
        ? static_cast<int>(std::ceil(customHangMs / blockDurationMs))
        : -1;

    double sumSq = 0.0;
    double peak = 0.0;
    for (int i = 0; i < frames; ++i) {
        for (int leg = 0; leg < legs; ++leg) {
            // The final RX bus deliberately permits finite values beyond unity
            // until its soft limiter.  Preserve that waveform here so summed
            // receivers are gain-reduced as a whole instead of hard-clipped
            // sample-by-sample before the peak guard can act.
            const float raw = (leg == 0) ? left[i] : right[i];
            const float s = std::isfinite(raw) ? raw : 0.0f;
            const double a = std::fabs(static_cast<double>(s));
            if (a > peak) { peak = a; }
            sumSq += static_cast<double>(s) * s;
        }
    }

    const double rms = std::sqrt(sumSq / (static_cast<double>(frames) * legs));
    const double inputRmsDbfs = linearToDbfs(rms);
    const double inputPeakDbfs = linearToDbfs(peak);
    targetRmsDb += levelReferenceOffsetDb;
    const double gateRmsDb = kGateRmsDb + levelReferenceOffsetDb;
    const bool belowGate = rms <= 0.0 || !std::isfinite(inputRmsDbfs) || inputRmsDbfs < gateRmsDb;

    double desiredDb = belowGate
        ? 0.0
        : targetRmsDb - inputRmsDbfs;
    desiredDb = std::clamp(desiredDb, kMaxCutDb, maxBoostDb);

    // Audio amplitude cannot distinguish weak speech from receiver noise.
    // Positive makeup therefore requires independent, fresh RF evidence.
    // Cuts remain unconditional so a sudden loud block is still contained.
    if (desiredDb > 0.0 && !allowBoost) {
        desiredDb = 0.0;
    }

    // Soft gate: taper the upward BOOST to zero as the input approaches the
    // floor, so crossing the gate can never snap the full boost on/off (the
    // AGC-T hair-trigger). Cuts (desiredDb < 0, the loud-signal blast-guard)
    // are NEVER tapered. Continuous with belowGate: at the gate the factor is
    // 0, reaching full boost RxLevelerGateSoftWindowDb above it.
    if (!belowGate && desiredDb > 0.0) {
        const double gateFactor = std::clamp(
            (inputRmsDbfs - gateRmsDb) / kGateSoftWindowDb,
            0.0, 1.0);
        desiredDb *= gateFactor;
    }

    double peakHeadroomDb = kNaN;
    bool peakLimited = false;
    if (peak > 1.0e-9) {
        peakHeadroomDb = 20.0 * std::log10(std::max(kPeakTarget, 1.0e-9) / peak);
        if (std::isfinite(peakHeadroomDb) && desiredDb > peakHeadroomDb) {
            desiredDb = std::clamp(peakHeadroomDb, kMaxCutDb, maxBoostDb);
            peakLimited = true;
        }
    }

    double currentDb = std::isfinite(state.gainDb) ? state.gainDb : 0.0;
    currentDb = std::clamp(currentDb, kMaxCutDb, maxBoostDb);
    // GainDb is the controller's pause-memory value. AppliedGainDb is the
    // gain that actually reached the final sample of the previous block.
    // They intentionally differ while audio is below the gate: retain the
    // controller memory, but fade the applied gain to unity so noise is not
    // held up. Keeping both prevents a hard gain step at word boundaries.
    const double appliedStartDb = (state.diagnosticsValid && std::isfinite(state.appliedGainDb))
        ? std::clamp(state.appliedGainDb, kMaxCutDb, maxBoostDb)
        : currentDb;

    // Evidence loss bypasses pause memory so averaged-SNR hangover cannot
    // hold makeup on noise, but releases it over several audio blocks. Peak
    // safety below may still cut faster when the held gain is genuinely unsafe.
    const bool evidenceLossRelease = !allowBoost
        && (currentDb > 0.0 || forceReleaseToUnity);

    // True when the gain we are currently holding would, on its own, drive
    // this block's peak past the limiter target — i.e. a gain "held" high
    // across a quiet gap (pause memory / catch-up) is about to be dumped onto
    // a louder block. The raw input-peak gate below (peak > target) misses
    // this case because the danger is gain × peak, not peak alone: a signal
    // whose input peak is modest still blasts the speaker if the held gain is
    // large. When this is set we cut as urgently as a clipping peak would.
    const bool currentGainOverdrivesPeak =
        std::isfinite(peakHeadroomDb) && appliedStartDb > peakHeadroomDb;

    double nextDb = currentDb;
    if (evidenceLossRelease) {
        state.pauseHoldBlocks = 0;
    } else if (belowGate) {
        const bool holdMemory = state.pauseHoldBlocks > 0;
        if (holdMemory) {
            state.pauseHoldBlocks--;
        }

        const double releaseStep = hasCustomRelease
            ? customReleaseSlewDb
            : kPauseMemoryDecayDbPerBlock * blockScale;
        if (holdMemory) {
            nextDb = currentDb;
        } else if (std::fabs(currentDb) <= releaseStep) {
            nextDb = 0.0;
        } else {
            nextDb = currentDb + (currentDb > 0.0 ? -releaseStep : releaseStep);
        }
    } else {
        state.pauseHoldBlocks = hasCustomHang
            ? customPauseHoldBlocks
            : static_cast<int>(std::ceil(kPauseHoldBlocks / blockScale));
    }

    bool boostSlewLimited = false;
    if (!evidenceLossRelease && !belowGate && desiredDb > currentDb) {
        double boostSlewDb = hasCustomAttack
            ? customBoostSlewDb
            : kBoostSlewDbPerBlock * blockScale;
        if (!hasCustomAttack
            && std::isfinite(peakHeadroomDb) && peakHeadroomDb >= kFastBoostHeadroomDb) {
            boostSlewDb = std::max(boostSlewDb, kFastBoostSlewDbPerBlock * blockScale);
        }
        if (!hasCustomAttack
            && std::isfinite(peakHeadroomDb)
            && peakHeadroomDb >= kVeryFastBoostHeadroomDb
            && inputRmsDbfs >= kVeryFastBoostGateRmsDb + levelReferenceOffsetDb) {
            boostSlewDb = std::max(boostSlewDb, kVeryFastBoostSlewDbPerBlock * blockScale);
        }

        const double crestDb = inputPeakDbfs - inputRmsDbfs;
        if (!hasCustomAttack
            && std::isfinite(crestDb)
            && crestDb >= kCrestCatchupMinCrestDb
            && inputRmsDbfs <= kCrestCatchupMaxRmsDb + levelReferenceOffsetDb
            && inputPeakDbfs >= kCrestCatchupMinPeakDb + levelReferenceOffsetDb
            && desiredDb - currentDb >= kCrestCatchupMinGainGapDb) {
            boostSlewDb = std::max(boostSlewDb, kCrestCatchupBoostSlewDbPerBlock * blockScale);
        }
        if (!hasCustomAttack
            && state.gainDb >= kMemoryCatchupMinGainDb
            && inputRmsDbfs >= kMemoryCatchupGateRmsDb + levelReferenceOffsetDb
            && inputPeakDbfs >= kMemoryCatchupGatePeakDb + levelReferenceOffsetDb) {
            boostSlewDb = std::max(boostSlewDb, kFastBoostSlewDbPerBlock * blockScale);
        }

        nextDb = std::min(desiredDb, currentDb + boostSlewDb);
        boostSlewLimited = nextDb + 1.0e-9 < desiredDb;
    } else if (!belowGate || evidenceLossRelease) {
        const bool peakUrgency = peak > kPeakTarget || peakLimited || currentGainOverdrivesPeak;
        // Pause memory may exceed the gain actually applied before evidence
        // closed. Discard that hidden makeup instead of replaying it.
        const double cutStartDb = evidenceLossRelease
            ? std::min(currentDb, appliedStartDb)
            : currentDb;
        const double smoothCutDb = hasCustomRelease
            ? customReleaseSlewDb
            : kSmoothCutDb * blockScale;
        const double cutSlewDb = peakUrgency
            ? std::max(smoothCutDb, cutStartDb - desiredDb)
            : evidenceLossRelease
                ? kEvidenceLossReleaseDbPerBlock * blockScale
                : smoothCutDb;
        const double cutFloorDb = evidenceLossRelease ? std::min(desiredDb, 0.0) : desiredDb;
        nextDb = std::max(cutFloorDb, cutStartDb - cutSlewDb);
    }
    nextDb = std::clamp(nextDb, kMaxCutDb, maxBoostDb);

    // Hard per-block peak guard. The smooth boost/cut slews above track
    // loudness gently; this is the safety floor that stops a held-high gain
    // from ever being *applied* to a block whose peak would then exceed the
    // limiter target. Without it, gain banked across a quiet gap gets dumped
    // onto the first loud-ish block of a new signal and rides the soft-limit
    // ceiling for several blocks before the slew catches up — the "sudden
    // strong signal blasts the speaker" failure this leveler exists to stop.
    // Cutting straight to the peak-safe gain is inaudible next to that blast.
    if (!belowGate && std::isfinite(peakHeadroomDb) && nextDb > peakHeadroomDb) {
        nextDb = std::clamp(peakHeadroomDb, kMaxCutDb, maxBoostDb);
    }

    const double appliedEndDb = belowGate ? 0.0 : nextDb;
    const double appliedTransitionDb = std::fabs(appliedEndDb - appliedStartDb);
    const int requestedRampSamples = static_cast<int>(std::ceil(
        kGainRampMaxSamples *
        std::max(1.0, appliedTransitionDb / kLargeTransitionDb)));
    int rampSamples = std::clamp(
        std::min(frames, requestedRampSamples), 1, std::max(1, frames));
    double preLimitPeak = 0.0;
    int outputLimitSampleCount = 0;
    const bool emergencyCut = !belowGate && appliedEndDb < appliedStartDb
        && (peak > kPeakTarget || peakLimited || currentGainOverdrivesPeak);
    if (emergencyCut) {
        const double heldGain = dbToLinear(appliedStartDb);
        int firstUnsafeSample = 0;
        while (firstUnsafeSample < frames) {
            const bool leftSafe =
                std::fabs(left[firstUnsafeSample] * heldGain) <= kPeakTarget;
            const bool rightSafe = (right == nullptr)
                || std::fabs(right[firstUnsafeSample] * heldGain) <= kPeakTarget;
            if (!(leftSafe && rightSafe)) { break; }
            firstUnsafeSample++;
        }

        // Complete the safety cut no later than the first sample that the
        // previously applied gain would overdrive. A block loud from sample
        // zero still cuts immediately; a later crest gets a short dezipper
        // ramp instead of forcing an unrelated block-boundary step.
        rampSamples = firstUnsafeSample < frames
            ? std::clamp(firstUnsafeSample + 1, 1, kEmergencyCutRampSamples)
            : std::min(rampSamples, kEmergencyCutRampSamples);
    }
    for (int i = 0; i < frames; ++i) {
        const double ramp = i < rampSamples
            ? (i + 1) / static_cast<double>(rampSamples)
            : 1.0;
        const double gainDb = appliedStartDb + (appliedEndDb - appliedStartDb) * ramp;
        const double gain = dbToLinear(gainDb);
        for (int leg = 0; leg < legs; ++leg) {
            float* buf = (leg == 0) ? left : right;
            const float clean = std::isfinite(buf[i]) ? buf[i] : 0.0f;
            const double scaled = clean * gain;
            const double absScaled = std::fabs(scaled);
            if (absScaled > preLimitPeak) { preLimitPeak = absScaled; }

            const float limited = softLimitSample(static_cast<float>(scaled));
            if (std::fabs(limited) + 1.0e-6 < absScaled) { outputLimitSampleCount++; }
            buf[i] = limited;
        }
    }

    // From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:336-358 [@8970f2d]
    //   Rms() / PeakAbs() over the limited output.
    double outSumSq = 0.0;
    double outputPeak = 0.0;
    for (int i = 0; i < frames; ++i) {
        for (int leg = 0; leg < legs; ++leg) {
            const double s = (leg == 0) ? left[i] : right[i];
            outSumSq += s * s;
            const double value = std::fabs(s);
            if (std::isfinite(value) && value > outputPeak) { outputPeak = value; }
        }
    }
    const double outputRms = std::sqrt(outSumSq / (static_cast<double>(frames) * legs));
    const double outputRmsDbfs = linearToDbfs(outputRms);
    const double outputPeakDbfs = linearToDbfs(outputPeak);
    const double preLimitPeakDbfs = linearToDbfs(preLimitPeak);
    const double outputLimitReductionDb = (preLimitPeak > outputPeak && outputPeak > 0.0)
        ? 20.0 * std::log10(preLimitPeak / outputPeak)
        : 0.0;

    state.gainDb = nextDb;
    state.diagnosticsValid = true;
    state.inputRmsDbfs = inputRmsDbfs;
    state.inputPeakDbfs = inputPeakDbfs;
    state.outputRmsDbfs = outputRmsDbfs;
    state.outputPeakDbfs = outputPeakDbfs;
    state.desiredGainDb = desiredDb;
    state.appliedGainDb = appliedEndDb;
    state.gainDeltaDb = appliedEndDb - appliedStartDb;
    state.peakHeadroomDb = peakHeadroomDb;
    state.preLimitPeakDbfs = preLimitPeakDbfs;
    state.outputLimitReductionDb = outputLimitReductionDb;
    state.outputLimitSampleCount = outputLimitSampleCount;
    state.boostSlewLimited = boostSlewLimited;
    state.peakLimited = peakLimited;
    state.outputLimited = outputLimitSampleCount > 0;
}

} // namespace Longpath
