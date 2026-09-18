// =================================================================
// tests/tst_passband_snr_tracker.cpp  (Longpath)
// =================================================================
//
// PassbandSnrTracker end to end: linear FFT frames in, RxChannel leveler
// evidence out. A SliceModel on stream 0 (USB, +300..+2700 Hz at the
// DDC centre) and synthetic |X|^2 frames at 30 fps:
//
//   * a tone in the passband: after the 2.25 s settle and three 5 Hz
//     verdicts the slice is "resolved", the RxChannel's boost gate opens
//   * the tone goes away: six misses (1.2 s) later the gate closes
//   * an ADC peak above -3 dBFS vetoes at once
//   * keyed (MOX) drops the verdict and restarts the settle
//   * a slice on another stream is not touched by this stream's frames
//
// Longpath-original test. no-port-check: exercises Longpath-original glue
// over two cited Zeus ports.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-18 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspEngine.h"
#include "core/audio/RxAudioLeveler.h"
#include "models/PassbandSnrTracker.h"
#include "models/SliceModel.h"

#include <cmath>
#include <random>

using namespace Longpath;

namespace {

constexpr int    kBins = 4096;
constexpr double kRate = 48000.0;
constexpr double kHzPerBin = kRate / kBins;
constexpr double kEnb = 1.5;          // a Blackman-ish window, in bins
constexpr double kDbmOffset = -3.0;   // any coherent-gain correction
constexpr double kFloorDbm = -130.0;  // per bin, as the panadapter would show
constexpr int    kFrameMs = 33;

// |X|^2 per bin such that 10*log10(scale * |X|^2 / enb) reads kFloorDbm
// (+ the tone). The tracker divides by enb and multiplies by scale, so
// invert both here.
QVector<float> frame(double toneDbm, double toneHz, std::mt19937& rng)
{
    std::normal_distribution<double> jitter(0.0, 0.5);
    const double scale = std::pow(10.0, kDbmOffset / 10.0);
    QVector<float> v(kBins);
    const double spectrumLow = -0.5 * kBins * kHzPerBin;
    for (int i = 0; i < kBins; ++i) {
        const double center = spectrumLow + (i + 0.5) * kHzPerBin;
        double dbm = kFloorDbm + jitter(rng);
        if (std::isfinite(toneDbm) && std::fabs(center - toneHz) <= 0.5 * kHzPerBin) {
            dbm = 10.0 * std::log10(std::pow(10.0, dbm / 10.0) + std::pow(10.0, toneDbm / 10.0));
        }
        const double linear = std::pow(10.0, dbm / 10.0) * kEnb / scale;
        v[i] = static_cast<float>(linear);
    }
    return v;
}

} // namespace

class TestPassbandSnrTracker : public QObject {
    Q_OBJECT

    // Nested so the WdspEngine friendship granted to this test class covers
    // the m_initialized seed (a nested class is a member of the friend).
    // A real WDSP channel, the way the other channel tests get one (the
    // constructor alone would still build the noise-blanker family against a
    // channel WDSP has never opened).
    struct ChannelRig {
        WdspEngine engine;
        RxChannel* channel{nullptr};
        ChannelRig()
        {
            engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
            channel = engine.createRxChannel(0, bufferSizeForRate(48000), 4096, 48000, 48000, 48000);
        }
    };

    struct Rig : ChannelRig {
        PassbandSnrTracker tracker;
        SliceModel slice{0};
        std::mt19937 rng{7};
        int64_t nowMs{100000};

        Rig()
        {
            slice.setStreamIndex(0);
            slice.setFrequency(7'100'000.0);
            slice.setShiftOffsetHz(0.0);          // DDC centre == dial
            slice.setDspMode(DSPMode::USB);
            slice.setFilter(300, 2700);
            tracker.setChannelResolver([this](int idx) -> RxChannel* {
                return idx == 0 ? channel : nullptr;
            });
            tracker.bindSlice(&slice);
            tracker.setAdcPeakOverrideForTest(0, -20.0);
        }

        // Feeds `ms` of frames at 30 fps.
        void run(int ms, double toneDbm)
        {
            const int frames = ms / kFrameMs;
            for (int i = 0; i < frames; ++i) {
                nowMs += kFrameMs;
                tracker.feedFrame(0, frame(toneDbm, 1000.0, rng), kEnb, kDbmOffset, kRate, nowMs);
            }
        }
    };

private slots:
    void toneInPassbandResolvesAndOpensTheGate()
    {
        Rig rig;
        QSignalSpy spy(&rig.tracker, &PassbandSnrTracker::signalQualityUpdated);

        // Inside the settle window nothing resolves, however strong.
        rig.run(2000, -90.0);
        QVERIFY(!rig.tracker.sliceQuality(0).resolved);
        // 2.25 s settle + 3 verdicts at 200 ms: resolved well within 1.5 s.
        rig.run(1500, -90.0);
        const auto q = rig.tracker.sliceQuality(0);
        QVERIFY(q.resolved);
        QVERIFY(!q.adcRisk);
        QVERIFY(q.result.isValid());
        // -90 dBm tone over a -130 dBm/bin floor across 2400 Hz (205 bins,
        // +23 dB): about +17 dB SNR.
        QVERIFY2(q.result.snrDb > 12.0 && q.result.snrDb < 22.0,
                 qPrintable(QString::number(q.result.snrDb)));
        QVERIFY(spy.count() >= 3);
        QVERIFY(RxAudioLeveler::evidenceIsCurrent(q.evidenceMs, rig.nowMs));

        // The channel's gate is open: a weak block gets boosted.
        rig.channel->setLevelerEnabled(true);
        // The evidence timestamp is the rig's clock; the channel compares
        // against the real clock, so re-stamp with it (the tracker's own
        // wiring uses RxChannel::levelerClockMs for both).
        rig.channel->setLevelerEvidence(q.resolved, q.adcRisk, RxChannel::levelerClockMs());
        QVector<float> weak(1600);
        for (int i = 0; i < 1600; ++i) {
            weak[i] = static_cast<float>(0.01 * std::sin(2.0 * M_PI * 1000.0 * i / 48000.0));
        }
        QVector<float> l;
        QVector<float> r;
        for (int b = 0; b < 30; ++b) {
            l = weak;
            r = weak;
            rig.channel->applyRxLeveler(l.data(), r.data(), 1600);
        }
        QVERIFY2(rig.channel->levelerAppliedGainDb() > 10.0,
                 qPrintable(QString::number(rig.channel->levelerAppliedGainDb())));
    }

    void toneGoneReleasesAfterSixMisses()
    {
        Rig rig;
        rig.run(3500, -90.0);
        QVERIFY(rig.tracker.sliceQuality(0).resolved);
        // Five misses (1.0 s) still hold; the sixth (1.2 s) releases.
        rig.run(900, std::nan(""));
        QVERIFY(rig.tracker.sliceQuality(0).resolved);
        rig.run(600, std::nan(""));
        QVERIFY(!rig.tracker.sliceQuality(0).resolved);
    }

    void adcOverloadVetoesImmediately()
    {
        Rig rig;
        rig.run(3500, -90.0);
        QVERIFY(rig.tracker.sliceQuality(0).resolved);
        rig.tracker.setAdcPeakOverrideForTest(0, -1.0);
        rig.run(250, -90.0);
        const auto q = rig.tracker.sliceQuality(0);
        QVERIFY(!q.resolved);
        QVERIFY(q.adcRisk);
    }

    void keyedDropsTheVerdictAndRestartsTheSettle()
    {
        Rig rig;
        rig.run(3500, -90.0);
        QVERIFY(rig.tracker.sliceQuality(0).resolved);
        // Keyed: the estimator refuses every frame, so the verdict falls
        // through the ordinary six-miss release (about 1.4 s at the 7-frame
        // evaluation cadence) rather than at once -- as upstream.
        rig.tracker.setKeyed(true);
        rig.run(700, -90.0);
        QVERIFY(rig.tracker.sliceQuality(0).resolved);
        rig.run(1000, -90.0);
        QVERIFY(!rig.tracker.sliceQuality(0).resolved);
        rig.tracker.setKeyed(false);
        rig.run(2000, -90.0);
        QVERIFY(!rig.tracker.sliceQuality(0).resolved);
        rig.run(1500, -90.0);
        QVERIFY(rig.tracker.sliceQuality(0).resolved);
    }

    void otherStreamsFramesAreIgnored()
    {
        Rig rig;
        const int frames = 3500 / kFrameMs;
        for (int i = 0; i < frames; ++i) {
            rig.nowMs += kFrameMs;
            rig.tracker.feedFrame(1, frame(-90.0, 1000.0, rig.rng), kEnb, kDbmOffset, kRate, rig.nowMs);
        }
        QVERIFY(!rig.tracker.sliceQuality(0).resolved);
        QVERIFY(!rig.tracker.sliceQuality(0).result.isValid());
    }

    void channelBypassesWhileOffAndReleasesOnSwitchOff()
    {
        ChannelRig rig;
        QVERIFY(rig.channel != nullptr);
        RxChannel& ch = *rig.channel;
        QVector<float> l(1600);
        for (int i = 0; i < 1600; ++i) {
            l[i] = static_cast<float>(0.01 * std::sin(2.0 * M_PI * 1000.0 * i / 48000.0));
        }
        const QVector<float> original = l;
        // Off: untouched, no state.
        ch.applyRxLeveler(l.data(), nullptr, 1600);
        QCOMPARE(l, original);
        QCOMPARE(ch.levelerAppliedGainDb(), 0.0);

        // On with evidence: boosts.
        ch.setLevelerEnabled(true);
        ch.setLevelerEvidence(true, false, RxChannel::levelerClockMs());
        for (int b = 0; b < 40; ++b) {
            l = original;
            ch.applyRxLeveler(l.data(), nullptr, 1600);
        }
        QVERIFY(ch.levelerAppliedGainDb() > 15.0);

        // Off again: the makeup releases over blocks, then the pass
        // becomes a true bypass (buffer identical to the input).
        ch.setLevelerEnabled(false);
        int releasing = 0;
        for (int b = 0; b < 20; ++b) {
            l = original;
            ch.applyRxLeveler(l.data(), nullptr, 1600);
            if (l != original) { ++releasing; } else { break; }
        }
        QVERIFY2(releasing >= 2 && releasing <= 8, qPrintable(QString::number(releasing)));
        l = original;
        ch.applyRxLeveler(l.data(), nullptr, 1600);
        QCOMPARE(l, original);
        QCOMPARE(ch.levelerAppliedGainDb(), 0.0);
    }

    void staleEvidenceClosesTheGate()
    {
        ChannelRig rig;
        QVERIFY(rig.channel != nullptr);
        RxChannel& ch = *rig.channel;
        ch.setLevelerEnabled(true);
        // Evidence stamped a second ago: older than 750 ms, so no boost.
        ch.setLevelerEvidence(true, false, RxChannel::levelerClockMs() - 1000);
        QVector<float> weak(1600);
        for (int i = 0; i < 1600; ++i) {
            weak[i] = static_cast<float>(0.01 * std::sin(2.0 * M_PI * 1000.0 * i / 48000.0));
        }
        QVector<float> l;
        for (int b = 0; b < 40; ++b) {
            l = weak;
            ch.applyRxLeveler(l.data(), nullptr, 1600);
        }
        QVERIFY(ch.levelerAppliedGainDb() <= 0.0);
        QCOMPARE(l, weak);   // no boost, no cut: the block passed through
    }
};

QTEST_MAIN(TestPassbandSnrTracker)
#include "tst_passband_snr_tracker.moc"
