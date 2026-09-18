#pragma once
// =================================================================
// src/core/InPassbandSnrEstimator.h  (Longpath)
// =================================================================
//
// Source attribution (Zeus station engine -- GPL-2.0-or-later):
//
//   Copyright (C) 2026 Douglas J. Cerrato (KB2UKA), Christian Suarez
//       (N9WAR), and contributors -- per
//       https://github.com/Zeus-SDR/station-engine, LICENSE and
//       ATTRIBUTIONS.md at the repository root.
//
//   This file is a port of Station.Engine.Hosting/InPassbandSnrEstimator.cs
//   at @8970f2d (Release v2.0.26), whole file (290 lines). There is no
//   Thetis or AetherSDR equivalent; Zeus is the sole source. The Zeus
//   station engine is GPL-2.0-or-later; Longpath is GPLv3, so the "or
//   later" option is exercised. "Zeus" and "ZeusSDR" are trademarks of
//   the maintainers; this is nominative use. Record:
//   docs/attribution/ZEUS-PROVENANCE.md.
//
// What it does. Given one spectrum frame as power density in dB per Hz
// (low frequency first) and the active filter's passband as offsets from
// the spectrum centre, it estimates the in-passband signal-to-noise
// ratio: the noise floor is taken from two guarded reference regions on
// either side of the passband (robust median with MAD outlier rejection,
// slope-interpolated across the passband), integrated over the passband
// and subtracted from the measured total power. It refuses to report
// (Result::isValid() == false) whenever the numbers cannot support a
// claim: too few reference bins, a >30 dB tilt between the two sides
// (an occupied neighbouring sideband, not a noise slope), or an excess
// that does not clear 3.5 sigma of the combined measurement uncertainty.
// The result carries a confidence in 0.01..1.
//
// Two consumers in Longpath: the RX leveler's evidence gate (boost is
// only allowed while this says a resolved signal is present, see
// RxAudioLeveler) and, later, an SNR readout.
//
// Plain C++, no Qt. update() keeps the per-key settle state the upstream
// class has (a fresh key -- new filter, new tuning, new analyzer
// geometry -- waits 2.25 s and two frames before reporting; keyed
// intervals restart it). estimate() is the pure single-frame calculation.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-18 -- Ported for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude, from the Zeus station
//                 engine @8970f2d. Straight C# -> C++ port: same
//                 constants, same arithmetic. C# records became structs,
//                 ReadOnlySpan<float> became pointer+count, LINQ
//                 Order()/Select()/Where() became std::sort and loops.
// =================================================================

#include <cstdint>
#include <vector>

namespace Longpath {

class InPassbandSnrEstimator {
public:
    // From Zeus station-engine Station.Engine.Hosting/InPassbandSnrEstimator.cs:12-20 [@8970f2d]
    struct MeasurementKey {
        int     rxId{0};
        int     sampleRateHz{0};
        int     pixelCount{0};
        int64_t analyzerGeneration{0};
        int64_t analyzerCenterHz{0};
        int64_t effectiveVfoHz{0};
        int     filterLowHz{0};
        int     filterHighHz{0};

        bool operator==(const MeasurementKey&) const = default;
    };

    // From Zeus station-engine Station.Engine.Hosting/InPassbandSnrEstimator.cs:22-36 [@8970f2d]
    struct Result {
        double snrDb{0.0};
        double signalOnlyDb{0.0};
        double integratedNoiseDb{0.0};
        double confidence{0.0};

        bool isValid() const;
        static Result invalid();
    };

    // From Zeus station-engine Station.Engine.Hosting/InPassbandSnrEstimator.cs:38-41 [@8970f2d]
    static constexpr int     kMinimumReferenceBinsPerSide = 4;
    static constexpr int64_t kSettleMs = 2250;
    static constexpr double  kMinimumMeasurementSigmaDb = 0.08;
    static constexpr double  kDetectionSigma = 3.5;

    // From Zeus station-engine Station.Engine.Hosting/InPassbandSnrEstimator.cs:48-91 [@8970f2d]
    Result update(const float* psdDbPerHz, int count,
                  double hzPerPixel,
                  double passbandLowOffsetHz,
                  double passbandHighOffsetHz,
                  const MeasurementKey& key,
                  int64_t nowMs,
                  bool fresh,
                  bool keyed);

    // From Zeus station-engine Station.Engine.Hosting/InPassbandSnrEstimator.cs:93-196 [@8970f2d]
    //   Pure single-spectrum calculation, exposed to deterministic tests.
    static Result estimate(const float* psdDbPerHz, int count,
                           double hzPerPixel,
                           double passbandLowOffsetHz,
                           double passbandHighOffsetHz);

    void reset();

private:
    // From Zeus station-engine Station.Engine.Hosting/InPassbandSnrEstimator.cs:198-202 [@8970f2d]
    struct SideEstimate {
        bool   valid{false};
        double levelDb{0.0};
        double centerHz{0.0};
        double madDb{0.0};
        double sigmaDb{0.0};
        int    usedBins{0};
    };

    static SideEstimate robustSide(const float* pixels, int count,
                                   double hzPerPixel, double spectrumLow,
                                   double regionLow, double regionHigh);
    static double integrateMeasured(const float* pixels, int count,
                                    double hzPerPixel, double spectrumLow,
                                    double low, double high,
                                    double& coveredHz);
    static double integratePredictedNoise(double hzPerPixel, double spectrumLow,
                                          int pixelCount, double low, double high,
                                          double referenceDb, double referenceHz,
                                          double slopeDbPerHz);
    static double median(std::vector<double>& ordered);

    bool           m_hasKey{false};
    MeasurementKey m_key{};
    int64_t        m_keyChangedMs{INT64_MIN};
    int            m_freshFramesForKey{0};
    bool           m_wasKeyed{false};
};

} // namespace Longpath
