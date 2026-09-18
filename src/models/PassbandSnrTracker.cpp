// =================================================================
// src/models/PassbandSnrTracker.cpp  (Longpath)
// =================================================================
//
// Longpath-original glue; see PassbandSnrTracker.h. The two ported
// pieces it drives are cited in their own files.
// no-port-check: Longpath-original; ported logic lives in
// core/InPassbandSnrEstimator.cpp and core/audio/RxAudioLeveler.cpp.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-18 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "models/PassbandSnrTracker.h"

#include "core/RxChannel.h"
#include "core/audio/RxAudioLeveler.h"
#include "models/SliceModel.h"

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

// WDSP's own "unavailable" reading, the value Zeus's gate treats as
// no-ADC-telemetry (DspPipelineService.cs:250 [@8970f2d]).
constexpr double kAdcUnavailableDbfs = -200.0;

} // namespace

PassbandSnrTracker::PassbandSnrTracker(QObject* parent)
    : QObject(parent)
{
}

void PassbandSnrTracker::bindSlice(SliceModel* slice)
{
    if (!slice) { return; }
    SliceState& ss = m_slices[slice->sliceIndex()];
    ss.slice = slice;
    ss.estimator.reset();
    ss.acquireHits = 0;
    ss.releaseMisses = 0;
    ss.quality = SliceQuality{};
}

void PassbandSnrTracker::unbindSlice(int sliceIndex)
{
    m_slices.remove(sliceIndex);
}

void PassbandSnrTracker::setKeyed(bool keyed)
{
    m_keyed = keyed;
}

void PassbandSnrTracker::setAdcPeakOverrideForTest(int sliceIndex, double dbfs)
{
    auto it = m_slices.find(sliceIndex);
    if (it == m_slices.end()) { return; }
    it->hasAdcOverride = true;
    it->adcOverrideDbfs = dbfs;
}

PassbandSnrTracker::SliceQuality PassbandSnrTracker::sliceQuality(int sliceIndex) const
{
    const auto it = m_slices.constFind(sliceIndex);
    return (it == m_slices.constEnd()) ? SliceQuality{} : it->quality;
}

void PassbandSnrTracker::feedFrame(int streamIndex, const QVector<float>& binsLinear,
                                   double windowEnb, double dbmOffset,
                                   double sampleRateHz, int64_t nowMs)
{
    const int n = binsLinear.size();
    if (n < 16 || sampleRateHz <= 0.0 || !(windowEnb > 0.0)) { return; }

    StreamState& st = m_streams[streamIndex];
    const bool geometryChanged = st.avSum.size() != n || st.sampleRateHz != sampleRateHz;
    st.sampleRateHz = sampleRateHz;
    st.windowEnb = windowEnb;
    st.dbmOffset = dbmOffset;

    // From WDSP analyzer.c:290,383 [v1.29] -- the average-power detector with
    // one bin per pixel: pixels[k] = psum / bcount * inv_enb, bcount == 1.
    // Zeus configures its SNR analyzer this way (WdspDspEngine.cs:4553-4566
    // [@8970f2d]: DetectorMode 2 = average, AverageMode 1 = linear
    // recursive, NormOneHz).
    const double invEnb = 1.0 / windowEnb;

    if (geometryChanged || !st.primed) {
        st.avSum.resize(n);
        for (int i = 0; i < n; ++i) {
            st.avSum[i] = static_cast<double>(binsLinear[i]) * invEnb;
        }
        st.primed = true;
        st.lastEvalMs = nowMs;   // a new geometry restarts the cadence
        // The estimator's per-key settle (2.25 s) covers the average's own
        // warm-up: the key carries pixelCount + sampleRate.
    } else {
        // From WDSP analyzer.c:498-507 [v1.29] avenger() case 1, "weighted
        // averaging of linear data":
        //   av_sum[i] = av_backmult * av_sum[i] + onem_avb * t_pixels[i];
        // with av_backmult = exp(-1 / (fps * tau)) per frame (Zeus
        // WdspDspEngine.cs:4556). Frames here carry their own timestamp,
        // so the multiplier is exp(-dt / tau) for the actual interval --
        // identical at a steady frame rate, and still right when the FFT
        // rate is changed in Setup.
        const double dtSec = std::clamp((nowMs - st.lastFrameMs) / 1000.0, 0.0, 5.0);
        const double backmult = (st.lastFrameMs == INT64_MIN) ? 0.0 : std::exp(-dtSec / kAvgTauSec);
        const double onemAvb = 1.0 - backmult;
        for (int i = 0; i < n; ++i) {
            st.avSum[i] = backmult * st.avSum[i]
                        + onemAvb * static_cast<double>(binsLinear[i]) * invEnb;
        }
    }
    st.lastLinear = binsLinear;
    st.lastFrameMs = nowMs;

    if (nowMs - st.lastEvalMs >= kEvalPeriodMs) {
        st.lastEvalMs = nowMs;
        evaluateStream(streamIndex, st, nowMs);
    }
}

void PassbandSnrTracker::evaluateStream(int streamIndex, StreamState& st, int64_t nowMs)
{
    // Only convert the running average to dB/Hz when a slice on this
    // stream will read it.
    bool anySlice = false;
    for (auto it = m_slices.cbegin(); it != m_slices.cend(); ++it) {
        if (it->slice && it->slice->streamIndex() == streamIndex) { anySlice = true; break; }
    }
    if (!anySlice) { return; }

    const int n = st.avSum.size();
    st.psdDbPerHz.resize(n);
    // From WDSP analyzer.c:505,553 [v1.29] and CalcBandwidthNormalization
    // (analyzer.c:1093-1097): pixel = 10*log10(scale * av_sum + 1e-60) +
    // norm_oneHz, norm_oneHz = 10*log10(1 / bin_width). `scale` is the
    // window coherent-gain factor FFTEngine's dBm path applies
    // (FFTEngine.h: scale = 10^(dbmOffset/10)), so these bins read the
    // same dBm the panadapter shows, per hertz.
    const double scale = std::pow(10.0, st.dbmOffset / 10.0);
    const double binWidthHz = st.sampleRateHz / n;
    const double normOneHzDb = 10.0 * std::log10(1.0 / binWidthHz);
    for (int i = 0; i < n; ++i) {
        st.psdDbPerHz[i] = static_cast<float>(
            10.0 * std::log10(scale * st.avSum[i] + 1.0e-60) + normOneHzDb);
    }

    for (auto it = m_slices.begin(); it != m_slices.end(); ++it) {
        if (!it->slice || it->slice->streamIndex() != streamIndex) { continue; }
        evaluateSlice(*it, st, streamIndex, nowMs);
    }
}

double PassbandSnrTracker::adcPeakDbfs(const SliceState& ss) const
{
    if (ss.hasAdcOverride) { return ss.adcOverrideDbfs; }
    if (!m_resolveChannel || !ss.slice) { return kAdcUnavailableDbfs; }
    RxChannel* ch = m_resolveChannel(ss.slice->sliceIndex());
    if (!ch || !ch->isActive()) { return kAdcUnavailableDbfs; }
    return ch->getMeter(RxMeterType::AdcPeak);
}

void PassbandSnrTracker::evaluateSlice(SliceState& ss, const StreamState& st,
                                       int streamIndex, int64_t nowMs)
{
    SliceModel* slice = ss.slice;
    const int n = st.psdDbPerHz.size();
    const double hzPerPixel = st.sampleRateHz / n;

    // The passband as the panadapter draws it: [demodulated RF + filterLow,
    // + filterHigh], expressed against the DDC centre the bins are
    // centred on (RadioModel::composedShiftHz: centre = frequency -
    // shiftOffsetHz).
    const double streamCentreHz = slice->frequency() - slice->shiftOffsetHz();
    const double demodHz = slice->demodulatedRxFrequency();
    const double lowOffsetHz = demodHz - streamCentreHz + slice->filterLow();
    const double highOffsetHz = demodHz - streamCentreHz + slice->filterHigh();

    InPassbandSnrEstimator::MeasurementKey key;
    key.rxId = slice->sliceIndex();
    key.sampleRateHz = static_cast<int>(std::lround(st.sampleRateHz));
    key.pixelCount = n;
    key.analyzerGeneration = streamIndex + 1;
    key.analyzerCenterHz = static_cast<int64_t>(std::llround(streamCentreHz));
    key.effectiveVfoHz = static_cast<int64_t>(std::llround(demodHz));
    key.filterLowHz = slice->filterLow();
    key.filterHighHz = slice->filterHigh();

    const InPassbandSnrEstimator::Result result = ss.estimator.update(
        st.psdDbPerHz.constData(), n, hzPerPixel,
        lowOffsetHz, highOffsetHz, key, nowMs,
        /* fresh */ true, m_keyed);

    // Longpath deviation D3: the current in-passband power from the
    // current (unaveraged) frame, in the same dB units the estimator's
    // integrated noise uses (scale * power / binWidth, integrated over
    // the fractional bin overlaps exactly as IntegrateMeasured does).
    double currentSignalDb = -300.0;
    {
        const double scale = std::pow(10.0, st.dbmOffset / 10.0);
        const double invEnb = 1.0 / st.windowEnb;
        const double binWidthHz = hzPerPixel;
        const double spectrumLow = -0.5 * n * binWidthHz;
        const double passLow = std::min(lowOffsetHz, highOffsetHz);
        const double passHigh = std::max(lowOffsetHz, highOffsetHz);
        double sum = 0.0;
        const int firstBin = std::max(0, static_cast<int>(std::floor((passLow - spectrumLow) / binWidthHz)) - 1);
        const int lastBin = std::min(n - 1, static_cast<int>(std::ceil((passHigh - spectrumLow) / binWidthHz)) + 1);
        for (int i = firstBin; i <= lastBin; ++i) {
            const double binLow = spectrumLow + i * binWidthHz;
            const double overlap = std::min(passHigh, binLow + binWidthHz) - std::max(passLow, binLow);
            if (overlap <= 0.0) { continue; }
            const double linear = (i < st.lastLinear.size()) ? st.lastLinear[i] : 0.0;
            sum += scale * linear * invEnb / binWidthHz * overlap;
        }
        if (sum > 0.0 && std::isfinite(sum)) {
            currentSignalDb = 10.0 * std::log10(sum);
        }
    }

    const double adcPk = adcPeakDbfs(ss);

    // From Zeus station-engine Station.Engine.Hosting/DspPipelineService.cs:10245-10287 [@8970f2d]
    //   UpdateRxConstantLoudnessEvidence
    SliceQuality& q = ss.quality;
    const bool evidenceCurrent = RxAudioLeveler::evidenceIsCurrent(q.evidenceMs, nowMs);
    if (!evidenceCurrent) {
        q.resolved = false;
        ss.acquireHits = 0;
        ss.releaseMisses = 0;
    }

    const bool adcRisk = RxAudioLeveler::adcOverloadRisk(adcPk);
    const bool wasResolved = q.resolved;
    // rxCalibrationDb is 0: in D3 both sides of the comparison come from the
    // same spectrum (Zeus adds the S-meter calibration to bring its stage
    // meter onto the analyzer's scale).
    const bool measurementAllowsBoost = RxAudioLeveler::evidenceAllowsBoost(
        result.isValid(), result.confidence, result.integratedNoiseDb,
        currentSignalDb, 0.0, adcPk, wasResolved);
    const RxAudioLeveler::EvidenceDecision decision = RxAudioLeveler::advanceEvidence(
        /* spectrumFresh */ true, measurementAllowsBoost, adcRisk, wasResolved,
        ss.acquireHits, ss.releaseMisses);

    ss.acquireHits = decision.acquireHits;
    ss.releaseMisses = decision.releaseMisses;
    q.resolved = decision.resolved;
    q.adcRisk = adcRisk;
    q.result = result;
    q.currentSignalDb = currentSignalDb;
    // A processed rejection reports RefreshAge too, but only resolved or
    // acquiring evidence (and the immediate ADC veto) may extend usable age.
    if (decision.refreshAge
        && (decision.resolved || decision.acquireHits > 0 || adcRisk)) {
        q.evidenceMs = nowMs;
    }

    if (m_resolveChannel) {
        if (RxChannel* ch = m_resolveChannel(slice->sliceIndex())) {
            ch->setLevelerEvidence(q.resolved, adcRisk, q.evidenceMs);
        }
    }

    emit signalQualityUpdated(slice->sliceIndex(),
                              result.isValid() ? result.snrDb : std::nan(""),
                              result.confidence, q.resolved);
}

} // namespace Longpath
