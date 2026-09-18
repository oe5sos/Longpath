#pragma once
// =================================================================
// src/models/PassbandSnrTracker.h  (Longpath)
// =================================================================
//
// Longpath-original glue. Feeds the ported in-passband SNR estimator
// (core/InPassbandSnrEstimator.h, Zeus) from Longpath's own FFT frames
// and turns its verdict into the RX leveler's boost permission
// (core/audio/RxAudioLeveler.h, Zeus). Zeus does the equivalent inside
// DspPipelineService.Tick with a second WDSP analyzer per receiver
// (WdspDspEngine.ConfigureSnrAnalyzer: average-power detector, linear
// recursive averaging with tau 0.75 s at 30 fps, one-hertz normalised);
// Longpath has no WDSP analyzer -- FFTEngine emits the raw linear power
// bins every frame -- so the averaging and the dB/Hz normalisation are
// done here with the same arithmetic WDSP's avenger() would apply
// (analyzer.c case 1 + norm_oneHz), cited at the site.
//
// One instance for the whole radio, owned by RadioModel, main thread:
//
//   feedFrame(stream, bins, enb, dbmOffset, rate, nowMs)
//       MainWindow's FFTEngine::fftReadyLinear lambda, every frame.
//       Updates the stream's running average.
//   every 200 ms per stream (the Zeus 5 Hz cadence: acquire 3 frames
//   = 0.6 s, release 6 = 1.2 s, DspPipelineService.cs:245-248):
//       for each slice bound to the stream: read its passband from the
//       SliceModel, run the estimator, read the WDSP ADC peak meter,
//       advance the evidence state, push (resolved, adcRisk, nowMs) into
//       the slice's RxChannel atomics, emit signalQualityUpdated.
//
// Pull, not push: the passband is read from the SliceModel at
// evaluation time (frequency, filter, RIT, DIG offset, DDC centre), so
// no tuning or filter signal needs to be wired here.
//
// Longpath deviation D3 (see RxAudioLeveler.h): the "current signal"
// cross-check is the current-frame in-passband power from the same
// spectrum, not the WDSP stage meter.
// no-port-check: Longpath-original; the ported logic is in
// core/InPassbandSnrEstimator.{h,cpp} and core/audio/RxAudioLeveler.{h,cpp}.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-18 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "core/InPassbandSnrEstimator.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QVector>

#include <cstdint>
#include <functional>

namespace Longpath {

class SliceModel;
class RxChannel;

class PassbandSnrTracker : public QObject {
    Q_OBJECT
public:
    explicit PassbandSnrTracker(QObject* parent = nullptr);

    // From Zeus station-engine Zeus.Dsp/Wdsp/WdspDspEngine.cs:4517,4534 [@8970f2d]
    //   PipelineFps = 30; SnrPowerAvgTauSec = 0.750
    static constexpr double kAvgTauSec = 0.750;
    // The Zeus stage-meter cadence the evidence counters are defined
    // against (DspPipelineService.cs:245-248 [@8970f2d]: "The 5 Hz stage
    // meter must agree for ~0.6 s to open").
    static constexpr int64_t kEvalPeriodMs = 200;

    // The slice's RxChannel is resolved through this at evaluation time
    // (RadioModel installs a lambda over WdspEngine::rxChannel).
    using ChannelResolver = std::function<RxChannel*(int sliceIndex)>;
    void setChannelResolver(ChannelResolver r) { m_resolveChannel = std::move(r); }

    void bindSlice(SliceModel* slice);
    void unbindSlice(int sliceIndex);
    void setKeyed(bool keyed);
    bool keyed() const { return m_keyed; }

    // One FFT frame: |X[k]|^2 per bin, FFT-shifted (negative frequencies
    // first), `windowEnb` bins of equivalent noise bandwidth, `dbmOffset`
    // the coherent-gain dB the dBm path adds -- exactly what
    // FFTEngine::fftReadyLinear carries. `sampleRateHz` spans the bins.
    void feedFrame(int streamIndex, const QVector<float>& binsLinear,
                   double windowEnb, double dbmOffset,
                   double sampleRateHz, int64_t nowMs);

    // Last verdict per slice (main thread), for tests and a readout.
    struct SliceQuality {
        InPassbandSnrEstimator::Result result{};
        bool    resolved{false};
        bool    adcRisk{true};
        int64_t evidenceMs{INT64_MIN};
        double  currentSignalDb{0.0};
    };
    SliceQuality sliceQuality(int sliceIndex) const;

    // Test seam: the ADC peak normally comes from the RxChannel meter.
    void setAdcPeakOverrideForTest(int sliceIndex, double dbfs);

signals:
    // Emitted at every evaluation of a bound slice (5 Hz), valid or not.
    void signalQualityUpdated(int sliceIndex, double snrDb, double confidence,
                              bool resolved);

private:
    struct StreamState {
        QVector<double> avSum;        // linear running average per bin
        QVector<float>  psdDbPerHz;   // scratch for the estimator
        double sampleRateHz{0.0};
        double windowEnb{1.0};
        double dbmOffset{0.0};
        int64_t lastEvalMs{INT64_MIN};
        int64_t lastFrameMs{INT64_MIN};
        bool primed{false};
        // The current (unaveraged) frame, kept for the D3 cross-check.
        QVector<float> lastLinear;
    };
    struct SliceState {
        QPointer<SliceModel> slice;
        InPassbandSnrEstimator estimator;
        int  acquireHits{0};
        int  releaseMisses{0};
        SliceQuality quality;
        bool hasAdcOverride{false};
        double adcOverrideDbfs{0.0};
    };

    void evaluateStream(int streamIndex, StreamState& st, int64_t nowMs);
    void evaluateSlice(SliceState& ss, const StreamState& st, int streamIndex,
                       int64_t nowMs);
    double adcPeakDbfs(const SliceState& ss) const;

    QHash<int, StreamState> m_streams;
    QHash<int, SliceState>  m_slices;
    ChannelResolver         m_resolveChannel;
    bool                    m_keyed{false};
};

} // namespace Longpath
