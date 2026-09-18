// =================================================================
// src/core/InPassbandSnrEstimator.cpp  (Longpath)
// =================================================================
//
// Source attribution (Zeus station engine -- GPL-2.0-or-later):
//
//   Copyright (C) 2026 Douglas J. Cerrato (KB2UKA), Christian Suarez
//       (N9WAR), and contributors -- per
//       https://github.com/Zeus-SDR/station-engine, LICENSE and
//       ATTRIBUTIONS.md at the repository root.
//
//   Port of Station.Engine.Hosting/InPassbandSnrEstimator.cs at @8970f2d
//   (Release v2.0.26). See InPassbandSnrEstimator.h for the full
//   attribution.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-18 -- Ported for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude, from the Zeus station
//                 engine @8970f2d.
// =================================================================

#include "core/InPassbandSnrEstimator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Longpath {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// From Zeus station-engine Station.Engine.Hosting/InPassbandSnrEstimator.cs:280 [@8970f2d]
//   private static double DbToLinear(double db) => Math.Pow(10.0, db / 10.0);
double dbToLinear(double db)
{
    return std::pow(10.0, db / 10.0);
}

} // namespace

// From Zeus station-engine Station.Engine.Hosting/InPassbandSnrEstimator.cs:28-32 [@8970f2d]
bool InPassbandSnrEstimator::Result::isValid() const
{
    return std::isfinite(snrDb)
        && std::isfinite(signalOnlyDb)
        && std::isfinite(integratedNoiseDb)
        && std::isfinite(confidence) && confidence > 0.0;
}

// From Zeus station-engine Station.Engine.Hosting/InPassbandSnrEstimator.cs:34-35 [@8970f2d]
InPassbandSnrEstimator::Result InPassbandSnrEstimator::Result::invalid()
{
    return Result{kNaN, kNaN, kNaN, 0.0};
}

void InPassbandSnrEstimator::reset()
{
    m_hasKey = false;
    m_key = MeasurementKey{};
    m_keyChangedMs = INT64_MIN;
    m_freshFramesForKey = 0;
    m_wasKeyed = false;
}

// From Zeus station-engine Station.Engine.Hosting/InPassbandSnrEstimator.cs:48-91 [@8970f2d]
InPassbandSnrEstimator::Result InPassbandSnrEstimator::update(
    const float* psdDbPerHz, int count,
    double hzPerPixel,
    double passbandLowOffsetHz,
    double passbandHighOffsetHz,
    const MeasurementKey& key,
    int64_t nowMs,
    bool fresh,
    bool keyed)
{
    if (!m_hasKey || !(m_key == key)) {
        m_hasKey = true;
        m_key = key;
        m_keyChangedMs = nowMs;
        m_freshFramesForKey = 0;
    }

    if (keyed) {
        // RX analyzer state can be starved or contaminated while the radio
        // is transmitting. Restart acquisition on every keyed interval so
        // the first post-MOX frame can never reuse a pre-TX noise estimate.
        m_wasKeyed = true;
        m_keyChangedMs = nowMs;
        m_freshFramesForKey = 0;
        return Result::invalid();
    }
    if (m_wasKeyed) {
        m_wasKeyed = false;
        m_keyChangedMs = nowMs;
        m_freshFramesForKey = 0;
    }
    if (!fresh) { return Result::invalid(); }

    m_freshFramesForKey++;
    if (m_freshFramesForKey < 2 || nowMs - m_keyChangedMs < kSettleMs) {
        return Result::invalid();
    }

    return estimate(psdDbPerHz, count, hzPerPixel,
                    passbandLowOffsetHz, passbandHighOffsetHz);
}

// From Zeus station-engine Station.Engine.Hosting/InPassbandSnrEstimator.cs:93-196 [@8970f2d]
InPassbandSnrEstimator::Result InPassbandSnrEstimator::estimate(
    const float* psdDbPerHz, int count,
    double hzPerPixel,
    double passbandLowOffsetHz,
    double passbandHighOffsetHz)
{
    if (psdDbPerHz == nullptr || count < 16 || !std::isfinite(hzPerPixel) || hzPerPixel <= 0.0
        || !std::isfinite(passbandLowOffsetHz) || !std::isfinite(passbandHighOffsetHz)) {
        return Result::invalid();
    }

    const double passLow = std::min(passbandLowOffsetHz, passbandHighOffsetHz);
    const double passHigh = std::max(passbandLowOffsetHz, passbandHighOffsetHz);
    const double passWidth = passHigh - passLow;
    if (passWidth <= 0.0) { return Result::invalid(); }

    const double spectrumLow = -0.5 * count * hzPerPixel;
    const double spectrumHigh = spectrumLow + count * hzPerPixel;
    // A report is only meaningful when the complete active filter and both
    // guarded reference regions are represented by the current analyzer.
    if (passLow < spectrumLow || passHigh > spectrumHigh) {
        return Result::invalid();
    }

    const double guardHz = std::max(2.0 * hzPerPixel, 0.25 * passWidth);
    const double referenceHz = std::max(passWidth, 8.0 * hzPerPixel);
    const double leftLow = std::max(spectrumLow, passLow - guardHz - referenceHz);
    const double leftHigh = passLow - guardHz;
    const double rightLow = passHigh + guardHz;
    const double rightHigh = std::min(spectrumHigh, passHigh + guardHz + referenceHz);

    const SideEstimate left = robustSide(psdDbPerHz, count, hzPerPixel, spectrumLow, leftLow, leftHigh);
    const SideEstimate right = robustSide(psdDbPerHz, count, hzPerPixel, spectrumLow, rightLow, rightHigh);
    if (!left.valid || !right.valid) {
        return Result::invalid();
    }

    double coveredHz = 0.0;
    const double totalPower = integrateMeasured(
        psdDbPerHz, count, hzPerPixel, spectrumLow, passLow, passHigh, coveredHz);
    if (!std::isfinite(totalPower) || totalPower <= 0.0
        || coveredHz < passWidth - std::max(1e-6, hzPerPixel * 1e-6)) {
        return Result::invalid();
    }

    // Interpolate the local noise slope in dB/Hz between robust medians on
    // the two guarded sides, then integrate that prediction over the exact
    // same fractional pixel overlaps as P(S+N).
    const double slope = (right.levelDb - left.levelDb) /
        std::max(hzPerPixel, right.centerHz - left.centerHz);
    // A >30 dB/reference-span tilt is almost certainly an occupied sideband
    // rather than a local receiver-noise slope; refuse to manufacture a report.
    if (!std::isfinite(slope) || std::fabs(right.levelDb - left.levelDb) > 30.0) {
        return Result::invalid();
    }

    const double noisePower = integratePredictedNoise(
        hzPerPixel, spectrumLow, count, passLow, passHigh,
        left.levelDb, left.centerHz, slope);
    if (!std::isfinite(noisePower) || noisePower <= 0.0) {
        return Result::invalid();
    }

    const double signalPower = totalPower - noisePower;
    // Sub-noise reports are allowed only when the excess clears uncertainty
    // in both the guarded reference estimate and the in-passband noise mean.
    if (!std::isfinite(signalPower) || signalPower <= 0.0) {
        return Result::invalid();
    }

    const double passbandBinSupport = std::max(1.0, passWidth / hzPerPixel);
    const double leftSeDb = left.sigmaDb / std::sqrt(static_cast<double>(left.usedBins));
    const double rightSeDb = right.sigmaDb / std::sqrt(static_cast<double>(right.usedBins));
    const double referenceSeDb = std::sqrt(leftSeDb * leftSeDb + rightSeDb * rightSeDb) / 2.0;
    const double pooledScatterDb = std::max(left.sigmaDb, right.sigmaDb);
    const double passbandSeDb = pooledScatterDb / std::sqrt(passbandBinSupport);
    const double combinedSigmaDb = std::sqrt(
        kMinimumMeasurementSigmaDb * kMinimumMeasurementSigmaDb +
        referenceSeDb * referenceSeDb + passbandSeDb * passbandSeDb);
    const double relativeNoiseSigma = std::exp(std::log(10.0) / 10.0 * combinedSigmaDb) - 1.0;
    const double requiredExcess = kDetectionSigma * noisePower * relativeNoiseSigma;
    if (!std::isfinite(requiredExcess) || signalPower <= requiredExcess) {
        return Result::invalid();
    }

    const double snrDb = 10.0 * std::log10(signalPower / noisePower);
    const double signalDb = 10.0 * std::log10(signalPower);
    const double noiseDb = 10.0 * std::log10(noisePower);

    // Confidence reflects reference support, side estimator dispersion and
    // subtraction conditioning. It is metadata, not a gate on negative SNR:
    // a resolved 0.1*N excess (-10 dB) remains reportable at lower confidence.
    const double support = std::min(1.0,
        std::min(left.usedBins, right.usedBins) / 12.0);
    const double dispersion = std::exp(-std::max(left.madDb, right.madDb) / 6.0);
    const double conditioning = std::clamp(signalPower / requiredExcess - 1.0, 0.0, 1.0);
    const double agreement = std::exp(-std::fabs(left.levelDb - right.levelDb) / 20.0);
    const double confidence = std::clamp(support * dispersion * agreement *
        (0.15 + 0.85 * conditioning), 0.01, 1.0);

    return Result{snrDb, signalDb, noiseDb, confidence};
}

// From Zeus station-engine Station.Engine.Hosting/InPassbandSnrEstimator.cs:204-236 [@8970f2d]
InPassbandSnrEstimator::SideEstimate InPassbandSnrEstimator::robustSide(
    const float* pixels, int count,
    double hzPerPixel, double spectrumLow,
    double regionLow, double regionHigh)
{
    if (regionHigh <= regionLow) { return SideEstimate{}; }

    struct Value { double db; double hz; };
    std::vector<Value> values;
    for (int i = 0; i < count; ++i) {
        const double center = spectrumLow + (i + 0.5) * hzPerPixel;
        if (center < regionLow || center >= regionHigh) { continue; }
        const float db = pixels[i];
        if (std::isfinite(db) && db > -300.0f && db < 200.0f) {
            values.push_back({static_cast<double>(db), center});
        }
    }
    if (static_cast<int>(values.size()) < kMinimumReferenceBinsPerSide) {
        return SideEstimate{};
    }

    std::vector<double> ordered;
    ordered.reserve(values.size());
    for (const Value& v : values) { ordered.push_back(v.db); }
    std::sort(ordered.begin(), ordered.end());
    const double med = median(ordered);
    std::vector<double> deviations;
    deviations.reserve(ordered.size());
    for (const double x : ordered) { deviations.push_back(std::fabs(x - med)); }
    std::sort(deviations.begin(), deviations.end());
    const double mad = median(deviations);
    const double highGate = med + std::max(3.0, 3.5 * mad);
    const double lowGate = med - std::max(6.0, 4.0 * mad);
    std::vector<Value> kept;
    kept.reserve(values.size());
    for (const Value& v : values) {
        if (v.db >= lowGate && v.db <= highGate) { kept.push_back(v); }
    }
    if (static_cast<int>(kept.size()) < kMinimumReferenceBinsPerSide
        || kept.size() < values.size() / 2) {
        return SideEstimate{};
    }

    std::vector<double> keptDb;
    keptDb.reserve(kept.size());
    double hzSum = 0.0;
    for (const Value& v : kept) {
        keptDb.push_back(v.db);
        hzSum += v.hz;
    }
    std::sort(keptDb.begin(), keptDb.end());
    const double robustDb = median(keptDb);
    const double centerHz = hzSum / static_cast<double>(kept.size());
    return SideEstimate{true, robustDb, centerHz, mad, 1.4826 * mad,
                        static_cast<int>(kept.size())};
}

// From Zeus station-engine Station.Engine.Hosting/InPassbandSnrEstimator.cs:238-260 [@8970f2d]
double InPassbandSnrEstimator::integrateMeasured(
    const float* pixels, int count,
    double hzPerPixel, double spectrumLow,
    double low, double high,
    double& coveredHz)
{
    double sum = 0.0;
    coveredHz = 0.0;
    for (int i = 0; i < count; ++i) {
        const double binLow = spectrumLow + i * hzPerPixel;
        const double overlap = std::min(high, binLow + hzPerPixel) - std::max(low, binLow);
        if (overlap <= 0.0) { continue; }
        const float db = pixels[i];
        if (!std::isfinite(db) || db <= -300.0f || db >= 200.0f) {
            return kNaN;
        }
        sum += dbToLinear(db) * overlap;
        coveredHz += overlap;
    }
    return sum;
}

// From Zeus station-engine Station.Engine.Hosting/InPassbandSnrEstimator.cs:262-278 [@8970f2d]
double InPassbandSnrEstimator::integratePredictedNoise(
    double hzPerPixel, double spectrumLow,
    int pixelCount, double low, double high,
    double referenceDb, double referenceHz,
    double slopeDbPerHz)
{
    double sum = 0.0;
    for (int i = 0; i < pixelCount; ++i) {
        const double binLow = spectrumLow + i * hzPerPixel;
        const double overlapLow = std::max(low, binLow);
        const double overlapHigh = std::min(high, binLow + hzPerPixel);
        if (overlapHigh <= overlapLow) { continue; }
        const double midpoint = 0.5 * (overlapLow + overlapHigh);
        const double predictedDb = referenceDb + slopeDbPerHz * (midpoint - referenceHz);
        sum += dbToLinear(predictedDb) * (overlapHigh - overlapLow);
    }
    return sum;
}

// From Zeus station-engine Station.Engine.Hosting/InPassbandSnrEstimator.cs:282-288 [@8970f2d]
double InPassbandSnrEstimator::median(std::vector<double>& ordered)
{
    const int n = static_cast<int>(ordered.size());
    return (n & 1) != 0
        ? ordered[n / 2]
        : 0.5 * (ordered[n / 2 - 1] + ordered[n / 2]);
}

} // namespace Longpath
