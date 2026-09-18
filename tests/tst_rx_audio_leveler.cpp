// =================================================================
// tests/tst_rx_audio_leveler.cpp  (Longpath)
// =================================================================
//
// The RX audio leveler ported from the Zeus station engine
// (src/core/audio/RxAudioLeveler.{h,cpp}), driven with synthetic blocks:
//
//   * boost is fail-closed without RF evidence and opens with it
//   * the boost rate is the upstream 2 dB per 33 ms whatever the block
//     size (Longpath deviation D2)
//   * a loud arrival is cut inside the block and the output never passes
//     the soft-limit ceiling, evidence or not
//   * switching off releases positive makeup to unity over blocks, then
//     the state clears
//   * a pause fades the applied gain to unity but the controller keeps
//     its memory for the hang time
//   * both stereo legs get one gain (D1)
//   * a Custom profile's attack is honoured
//   * the evidence FSM: acquire 3, release 6, ADC veto, age
//   * config normalisation clamps
//
// Longpath-original test. no-port-check: exercises a Zeus port that is
// cited in the file under test.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-18 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>

#include "core/audio/RxAudioLeveler.h"

#include <cmath>
#include <vector>

using namespace Longpath;

namespace {

constexpr int kRate = RxAudioLeveler::kSampleRateHz;

// A sine at `peak` amplitude, `frames` long, continuing from `phase`.
std::vector<float> sine(int frames, double peak, double& phase, double hz = 1000.0)
{
    std::vector<float> v(static_cast<size_t>(frames));
    const double step = 2.0 * M_PI * hz / kRate;
    for (int i = 0; i < frames; ++i) {
        v[static_cast<size_t>(i)] = static_cast<float>(peak * std::sin(phase));
        phase += step;
    }
    return v;
}

double rmsDb(const std::vector<float>& v)
{
    double s = 0.0;
    for (float x : v) { s += static_cast<double>(x) * x; }
    return 20.0 * std::log10(std::sqrt(s / v.size()) + 1e-20);
}

double peakAbs(const std::vector<float>& v)
{
    double p = 0.0;
    for (float x : v) { p = std::max(p, static_cast<double>(std::fabs(x))); }
    return p;
}

RxAudioLeveler::Inputs inputs(bool enabled, bool evidence)
{
    RxAudioLeveler::Inputs in;
    in.enabled = enabled;
    in.rfSignalResolved = evidence;
    in.adcOverloadRisk = !evidence;
    in.levelReferenceOffsetDb = 0.0;
    in.config = RxLevelerConfig{};
    return in;
}

// Runs `seconds` of a steady sine at `peak` through the leveler in
// `blockFrames` blocks; returns the applied gain after the last block.
double runSine(RxAudioLeveler& lv, double peak, double seconds, int blockFrames,
               const RxAudioLeveler::Inputs& in)
{
    double phase = 0.0;
    const int blocks = static_cast<int>(seconds * kRate / blockFrames);
    for (int b = 0; b < blocks; ++b) {
        std::vector<float> l = sine(blockFrames, peak, phase);
        lv.process(l.data(), nullptr, blockFrames, in);
    }
    return lv.state().appliedGainDb;
}

} // namespace

class TestRxAudioLeveler : public QObject {
    Q_OBJECT

private slots:
    void boostIsFailClosedWithoutEvidence()
    {
        // -40 dBFS RMS: 22 dB under the target, well above the -50 dBFS gate.
        RxAudioLeveler lv;
        const double peak = std::pow(10.0, -40.0 / 20.0) * std::sqrt(2.0);
        const double gain = runSine(lv, peak, 1.0, 1600, inputs(true, false));
        QVERIFY2(gain <= 0.0, qPrintable(QString::number(gain)));
        QCOMPARE(lv.state().desiredGainDb, 0.0);
    }

    void boostOpensWithEvidenceAndReachesTarget()
    {
        RxAudioLeveler lv;
        const double peak = std::pow(10.0, -40.0 / 20.0) * std::sqrt(2.0);
        const double gain = runSine(lv, peak, 2.0, 1600, inputs(true, true));
        // Target -18 dBFS RMS from -40 dBFS: +22 dB, under the 24 dB cap.
        QVERIFY2(std::fabs(gain - 22.0) < 0.5, qPrintable(QString::number(gain)));
        QVERIFY(!lv.state().boostSlewLimited);
        // And the output really sits at the target.
        double phase = 0.0;
        std::vector<float> l = sine(1600, peak, phase);
        lv.process(l.data(), nullptr, 1600, inputs(true, true));
        QVERIFY2(std::fabs(rmsDb(l) - (-18.0)) < 0.5, qPrintable(QString::number(rmsDb(l))));
    }

    // Longpath deviation D2: the per-block constants are scaled to the
    // block, so 256-frame blocks (what RxChannel hands over at 192 kHz)
    // climb at the same dB/s as Zeus's 1600-frame ticks.
    void boostRateIsIndependentOfBlockSize()
    {
        const double peak = std::pow(10.0, -40.0 / 20.0) * std::sqrt(2.0);
        RxAudioLeveler big;
        RxAudioLeveler small;
        // 0.2 s: on the 2 dB / 33 ms slope, far from the +22 dB target.
        const double gBig = runSine(big, peak, 0.2, 1600, inputs(true, true));
        const double gSmall = runSine(small, peak, 0.2, 256, inputs(true, true));
        QVERIFY2(gBig > 6.0 && gBig < 20.0, qPrintable(QString::number(gBig)));
        QVERIFY2(std::fabs(gBig - gSmall) < 1.5,
                 qPrintable(QString("big %1 small %2").arg(gBig).arg(gSmall)));
    }

    void loudArrivalIsCutInsideTheBlockWithoutEvidence()
    {
        RxAudioLeveler lv;
        // Peak 0.95: above the 0.74 peak target and the -18 dBFS RMS target.
        double phase = 0.0;
        std::vector<float> l = sine(1600, 0.95, phase);
        lv.process(l.data(), nullptr, 1600, inputs(true, false));
        QVERIFY(lv.state().appliedGainDb < 0.0);
        QVERIFY(lv.state().peakLimited || lv.state().gainDb < 0.0);
        // The soft limiter caps everything at the 0.84 ceiling.
        QVERIFY2(peakAbs(l) <= RxAudioLeveler::kOutputPeakCeiling + 1e-6,
                 qPrintable(QString::number(peakAbs(l))));
        // After a few blocks the cut has settled at the peak-safe gain and
        // the output peak sits at (or just under) the peak target.
        for (int b = 0; b < 30; ++b) {
            l = sine(1600, 0.95, phase);
            lv.process(l.data(), nullptr, 1600, inputs(true, false));
        }
        QVERIFY2(peakAbs(l) <= RxAudioLeveler::kPeakTarget + 0.01,
                 qPrintable(QString::number(peakAbs(l))));
    }

    void heldGainIsNotDumpedOntoALoudBlock()
    {
        // Boost a weak signal up to +22 dB, then hit it with a strong one:
        // the per-block peak guard must keep the output under the ceiling
        // on the very first loud block.
        RxAudioLeveler lv;
        const double weak = std::pow(10.0, -40.0 / 20.0) * std::sqrt(2.0);
        runSine(lv, weak, 2.0, 1600, inputs(true, true));
        QVERIFY(lv.state().appliedGainDb > 20.0);
        double phase = 0.0;
        std::vector<float> l = sine(1600, 0.5, phase);
        lv.process(l.data(), nullptr, 1600, inputs(true, true));
        QVERIFY2(peakAbs(l) <= RxAudioLeveler::kOutputPeakCeiling + 1e-6,
                 qPrintable(QString::number(peakAbs(l))));
        QVERIFY(lv.state().appliedGainDb < 5.0);
    }

    void switchingOffReleasesToUnityThenClears()
    {
        RxAudioLeveler lv;
        const double peak = std::pow(10.0, -40.0 / 20.0) * std::sqrt(2.0);
        runSine(lv, peak, 2.0, 1600, inputs(true, true));
        QVERIFY(lv.state().appliedGainDb > 20.0);

        double phase = 0.0;
        double last = lv.state().appliedGainDb;
        int blocks = 0;
        bool sawRelease = false;
        while (blocks < 60) {
            std::vector<float> l = sine(1600, peak, phase);
            lv.process(l.data(), nullptr, 1600, inputs(false, true));
            ++blocks;
            const double now = lv.state().appliedGainDb;
            if (lv.state().releaseToUnity) { sawRelease = true; }
            QVERIFY2(now <= last + 1e-9, "gain must only fall while releasing");
            last = now;
            if (now <= 0.0 && !lv.state().releaseToUnity) { break; }
        }
        QVERIFY(sawRelease);
        QVERIFY(lv.state().appliedGainDb <= 0.0);
        QVERIFY(!lv.state().releaseToUnity);
        // +24 dB clears in four 33 ms blocks upstream (6 dB per block):
        // +22 dB in about four, never in one.
        QVERIFY2(blocks >= 3 && blocks <= 8, qPrintable(QString::number(blocks)));
    }

    void pauseFadesAppliedGainButKeepsMemoryForHang()
    {
        RxAudioLeveler lv;
        const double peak = std::pow(10.0, -40.0 / 20.0) * std::sqrt(2.0);
        runSine(lv, peak, 2.0, 1600, inputs(true, true));
        const double held = lv.state().gainDb;
        QVERIFY(held > 20.0);

        // Silence (below the -50 dBFS gate): applied gain goes to unity on
        // the first block, the controller memory holds for 18 blocks
        // (600 ms at 33 ms), then decays 4.5 dB per block.
        std::vector<float> quiet(1600, 0.0f);
        lv.process(quiet.data(), nullptr, 1600, inputs(true, true));
        QCOMPARE(lv.state().appliedGainDb, 0.0);
        QCOMPARE(lv.state().gainDb, held);
        for (int b = 0; b < 17; ++b) {
            lv.process(quiet.data(), nullptr, 1600, inputs(true, true));
        }
        QCOMPARE(lv.state().gainDb, held);
        lv.process(quiet.data(), nullptr, 1600, inputs(true, true));
        QVERIFY2(std::fabs((held - lv.state().gainDb) - 4.5) < 1e-6,
                 qPrintable(QString::number(held - lv.state().gainDb)));
    }

    // Longpath deviation D1: one gain for both legs.
    void stereoLegsShareOneGain()
    {
        RxAudioLeveler lv;
        const double peak = std::pow(10.0, -40.0 / 20.0) * std::sqrt(2.0);
        double phaseL = 0.0;
        double phaseR = 0.0;
        for (int b = 0; b < 60; ++b) {
            std::vector<float> l = sine(1600, peak, phaseL);
            std::vector<float> r = sine(1600, peak * 0.5, phaseR);
            lv.process(l.data(), r.data(), 1600, inputs(true, true));
            if (b == 59) {
                // Same gain: the 6 dB level difference is preserved exactly.
                QVERIFY2(std::fabs((rmsDb(l) - rmsDb(r)) - 6.02) < 0.05,
                         qPrintable(QString::number(rmsDb(l) - rmsDb(r))));
            }
        }
        QVERIFY(lv.state().appliedGainDb > 10.0);
    }

    void customAttackIsHonoured()
    {
        const double peak = std::pow(10.0, -40.0 / 20.0) * std::sqrt(2.0);
        RxAudioLeveler fast;
        RxAudioLeveler slow;
        RxAudioLeveler::Inputs inFast = inputs(true, true);
        inFast.config.mode = RxLevelerConfig::Mode::Custom;
        inFast.config.attackMs = 100;
        RxAudioLeveler::Inputs inSlow = inFast;
        inSlow.config.attackMs = 2000;
        const double gFast = runSine(fast, peak, 0.15, 1600, inFast);
        const double gSlow = runSine(slow, peak, 0.15, 1600, inSlow);
        // 24 dB range / 100 ms attack: 22 dB reached within 150 ms;
        // over 2000 ms: about 1.8 dB.
        QVERIFY2(gFast > 20.0, qPrintable(QString::number(gFast)));
        QVERIFY2(gSlow < 3.0, qPrintable(QString::number(gSlow)));
    }

    void afReferenceOffsetMovesTheTarget()
    {
        // AF at -6 dB: the leveler must aim 6 dB lower, not undo the AF.
        RxAudioLeveler lv;
        const double peak = std::pow(10.0, -40.0 / 20.0) * std::sqrt(2.0);
        RxAudioLeveler::Inputs in = inputs(true, true);
        in.levelReferenceOffsetDb = -6.0;
        const double gain = runSine(lv, peak, 2.0, 1600, in);
        QVERIFY2(std::fabs(gain - 16.0) < 0.5, qPrintable(QString::number(gain)));
    }

    void evidenceFsmAcquiresInThreeAndReleasesInSix()
    {
        using L = RxAudioLeveler;
        L::EvidenceDecision d{false, 0, 0, false};
        for (int i = 0; i < 2; ++i) {
            d = L::advanceEvidence(true, true, false, d.resolved, d.acquireHits, d.releaseMisses);
            QVERIFY(!d.resolved);
        }
        d = L::advanceEvidence(true, true, false, d.resolved, d.acquireHits, d.releaseMisses);
        QVERIFY(d.resolved);
        // Five misses hold; the sixth releases.
        for (int i = 0; i < 5; ++i) {
            d = L::advanceEvidence(true, false, false, d.resolved, d.acquireHits, d.releaseMisses);
            QVERIFY(d.resolved);
        }
        d = L::advanceEvidence(true, false, false, d.resolved, d.acquireHits, d.releaseMisses);
        QVERIFY(!d.resolved);
        // A single miss during acquisition restarts the count.
        d = L::advanceEvidence(true, true, false, false, 0, 0);
        d = L::advanceEvidence(true, false, false, d.resolved, d.acquireHits, d.releaseMisses);
        QCOMPARE(d.acquireHits, 0);
        // ADC risk vetoes immediately, resolved or not.
        d = L::advanceEvidence(true, true, true, true, 0, 0);
        QVERIFY(!d.resolved);
        QVERIFY(d.refreshAge);
        // A stale frame changes nothing and does not refresh the age.
        d = L::advanceEvidence(false, true, false, true, 0, 2);
        QVERIFY(d.resolved);
        QCOMPARE(d.releaseMisses, 2);
        QVERIFY(!d.refreshAge);
    }

    void evidenceAgesOutAt750ms()
    {
        using L = RxAudioLeveler;
        QVERIFY(L::evidenceIsCurrent(1000, 1000));
        QVERIFY(L::evidenceIsCurrent(1000, 1750));
        QVERIFY(!L::evidenceIsCurrent(1000, 1751));
        QVERIFY(!L::evidenceIsCurrent(1000, 999));
        QVERIFY(!L::evidenceIsCurrent(INT64_MIN, 1000));
    }

    void evidenceAllowsBoostRequiresExcessOverNoise()
    {
        using L = RxAudioLeveler;
        // Valid quality, current signal 2 dB over the integrated noise: opens.
        QVERIFY(L::evidenceAllowsBoost(true, 0.5, -100.0, -98.0, 0.0, -20.0, false));
        // 1 dB over: not enough to acquire (1.5), enough to hold (0.25).
        QVERIFY(!L::evidenceAllowsBoost(true, 0.5, -100.0, -99.0, 0.0, -20.0, false));
        QVERIFY(L::evidenceAllowsBoost(true, 0.5, -100.0, -99.0, 0.0, -20.0, true));
        // Low confidence, invalid quality, ADC hot, ADC unavailable: closed.
        QVERIFY(!L::evidenceAllowsBoost(true, 0.05, -100.0, -90.0, 0.0, -20.0, false));
        QVERIFY(!L::evidenceAllowsBoost(false, 0.9, -100.0, -90.0, 0.0, -20.0, false));
        QVERIFY(!L::evidenceAllowsBoost(true, 0.9, -100.0, -90.0, 0.0, -2.0, false));
        QVERIFY(!L::evidenceAllowsBoost(true, 0.9, -100.0, -90.0, 0.0, -200.0, false));
        QVERIFY(L::adcOverloadRisk(-2.9));
        QVERIFY(!L::adcOverloadRisk(-3.1));
        QVERIFY(L::adcOverloadRisk(-200.0));
    }

    void softLimiterIsMonotonicAndCapped()
    {
        float last = 0.0f;
        for (int i = 0; i <= 400; ++i) {
            const float x = i * 0.01f;
            const float y = RxAudioLeveler::softLimitSample(x);
            QVERIFY(y >= last);
            QVERIFY(y <= static_cast<float>(RxAudioLeveler::kOutputPeakCeiling) + 1e-6f);
            if (x <= RxAudioLeveler::kOutputSoftKnee) { QCOMPARE(y, x); }
            last = y;
        }
        QCOMPARE(RxAudioLeveler::softLimitSample(-2.0f), -RxAudioLeveler::softLimitSample(2.0f));
    }

    void configNormalisationClamps()
    {
        RxLevelerConfig c;
        c.targetRmsDb = -60.0;
        c.maxBoostDb = 99.0;
        c.attackMs = 1;
        c.releaseMs = 99999;
        c.hangMs = -5;
        const RxLevelerConfig n = RxLevelerConfig::normalized(c);
        QCOMPARE(n.targetRmsDb, RxLevelerConfig::kMinTargetRmsDb);
        QCOMPARE(n.maxBoostDb, RxLevelerConfig::kMaxBoostLimitDb);
        QCOMPARE(n.attackMs, RxLevelerConfig::kMinAttackMs);
        QCOMPARE(n.releaseMs, RxLevelerConfig::kMaxReleaseMs);
        QCOMPARE(n.hangMs, RxLevelerConfig::kMinHangMs);
        c.targetRmsDb = std::nan("");
        QCOMPARE(RxLevelerConfig::normalized(c).targetRmsDb, RxLevelerConfig::kDefaultTargetRmsDb);
    }

    void nonFiniteInputIsTreatedAsSilence()
    {
        RxAudioLeveler lv;
        std::vector<float> l(1600, std::numeric_limits<float>::quiet_NaN());
        lv.process(l.data(), nullptr, 1600, inputs(true, true));
        for (float x : l) { QVERIFY(std::isfinite(x)); QCOMPARE(x, 0.0f); }
        QCOMPARE(lv.state().appliedGainDb, 0.0);
    }
};

QTEST_APPLESS_MAIN(TestRxAudioLeveler)
#include "tst_rx_audio_leveler.moc"
