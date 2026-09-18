// =================================================================
// tests/tst_in_passband_snr_estimator.cpp  (Longpath)
// =================================================================
//
// The in-passband SNR estimator ported from the Zeus station engine
// (src/core/InPassbandSnrEstimator.{h,cpp}) on synthetic dB/Hz spectra:
//
//   * a tone in the passband over a flat floor reports the right SNR
//   * noise alone is refused (no manufactured report)
//   * a >30 dB tilt between the reference sides is refused
//   * a passband outside the spectrum is refused
//   * update() waits for the settle time and two frames, and a keyed
//     interval restarts it
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

#include "core/InPassbandSnrEstimator.h"

#include <cmath>
#include <random>
#include <vector>

using namespace Longpath;

namespace {

constexpr int    kBins = 4096;
constexpr double kRate = 48000.0;
constexpr double kHzPerBin = kRate / kBins;
constexpr double kFloorDbPerHz = -140.0;

// A flat floor with small deterministic scatter (0.5 dB sigma), plus an
// optional tone of total power `toneDb` spread over one bin, plus an
// optional linear tilt in dB across the whole spectrum.
std::vector<float> spectrum(double toneDb, double toneHz, double tiltDbAcross = 0.0,
                            unsigned seed = 1)
{
    std::mt19937 rng(seed);
    std::normal_distribution<double> jitter(0.0, 0.5);
    std::vector<float> v(kBins);
    const double spectrumLow = -0.5 * kBins * kHzPerBin;
    for (int i = 0; i < kBins; ++i) {
        const double center = spectrumLow + (i + 0.5) * kHzPerBin;
        double db = kFloorDbPerHz + jitter(rng) + tiltDbAcross * (i / static_cast<double>(kBins) - 0.5);
        if (std::isfinite(toneDb)) {
            if (std::fabs(center - toneHz) <= 0.5 * kHzPerBin) {
                // Add the tone's power to the floor in this bin. As dB/Hz the
                // bin carries power / binWidth.
                const double floorLin = std::pow(10.0, db / 10.0);
                const double toneLin = std::pow(10.0, toneDb / 10.0) / kHzPerBin;
                db = 10.0 * std::log10(floorLin + toneLin);
            }
        }
        v[static_cast<size_t>(i)] = static_cast<float>(db);
    }
    return v;
}

} // namespace

class TestInPassbandSnrEstimator : public QObject {
    Q_OBJECT

private slots:
    void toneOverFlatFloorReportsItsSnr()
    {
        // SSB passband +300..+2700 Hz, a -100 dB tone at +1000 Hz. The noise
        // integrated over 2400 Hz is -140 + 10*log10(2400) = -106.2 dB, so
        // the SNR is about +6.2 dB.
        const std::vector<float> psd = spectrum(-100.0, 1000.0);
        const auto r = InPassbandSnrEstimator::estimate(psd.data(), kBins, kHzPerBin, 300.0, 2700.0);
        QVERIFY(r.isValid());
        const double expectedNoise = kFloorDbPerHz + 10.0 * std::log10(2400.0);
        QVERIFY2(std::fabs(r.integratedNoiseDb - expectedNoise) < 0.5,
                 qPrintable(QString::number(r.integratedNoiseDb)));
        QVERIFY2(std::fabs(r.signalOnlyDb - (-100.0)) < 0.7,
                 qPrintable(QString::number(r.signalOnlyDb)));
        QVERIFY2(std::fabs(r.snrDb - (-100.0 - expectedNoise)) < 1.0,
                 qPrintable(QString::number(r.snrDb)));
        QVERIFY(r.confidence > 0.1);
    }

    void strongToneHasHighConfidence()
    {
        const std::vector<float> psd = spectrum(-80.0, 1000.0);
        const auto r = InPassbandSnrEstimator::estimate(psd.data(), kBins, kHzPerBin, 300.0, 2700.0);
        QVERIFY(r.isValid());
        QVERIFY2(r.snrDb > 25.0, qPrintable(QString::number(r.snrDb)));
        QVERIFY2(r.confidence > 0.5, qPrintable(QString::number(r.confidence)));
    }

    void noiseAloneIsRefused()
    {
        const std::vector<float> psd = spectrum(std::nan(""), 0.0);
        const auto r = InPassbandSnrEstimator::estimate(psd.data(), kBins, kHzPerBin, 300.0, 2700.0);
        QVERIFY(!r.isValid());
    }

    void lsbPassbandWorksToo()
    {
        const std::vector<float> psd = spectrum(-90.0, -1500.0);
        const auto r = InPassbandSnrEstimator::estimate(psd.data(), kBins, kHzPerBin, -2700.0, -300.0);
        QVERIFY(r.isValid());
        QVERIFY(r.snrDb > 10.0);
    }

    void steepTiltIsRefused()
    {
        // 300 dB across the spectrum: the two reference sides differ by far
        // more than 30 dB.
        const std::vector<float> psd = spectrum(-80.0, 1000.0, 300.0);
        const auto r = InPassbandSnrEstimator::estimate(psd.data(), kBins, kHzPerBin, 300.0, 2700.0);
        QVERIFY(!r.isValid());
    }

    void gentleTiltIsFollowed()
    {
        // 20 dB across 48 kHz is 1 dB across the passband and its guards;
        // the slope interpolation must still find the tone.
        const std::vector<float> psd = spectrum(-90.0, 1000.0, 20.0);
        const auto r = InPassbandSnrEstimator::estimate(psd.data(), kBins, kHzPerBin, 300.0, 2700.0);
        QVERIFY(r.isValid());
        QVERIFY2(std::fabs(r.signalOnlyDb - (-90.0)) < 1.0,
                 qPrintable(QString::number(r.signalOnlyDb)));
    }

    void passbandOutsideSpectrumIsRefused()
    {
        const std::vector<float> psd = spectrum(-80.0, 1000.0);
        const auto r = InPassbandSnrEstimator::estimate(psd.data(), kBins, kHzPerBin, 23000.0, 25000.0);
        QVERIFY(!r.isValid());
        QVERIFY(!InPassbandSnrEstimator::estimate(psd.data(), 8, kHzPerBin, 300.0, 2700.0).isValid());
        QVERIFY(!InPassbandSnrEstimator::estimate(psd.data(), kBins, 0.0, 300.0, 2700.0).isValid());
        QVERIFY(!InPassbandSnrEstimator::estimate(psd.data(), kBins, kHzPerBin, 300.0, 300.0).isValid());
    }

    void updateWaitsForSettleAndRestartsOnKeying()
    {
        InPassbandSnrEstimator est;
        const std::vector<float> psd = spectrum(-80.0, 1000.0);
        InPassbandSnrEstimator::MeasurementKey key;
        key.sampleRateHz = 48000;
        key.pixelCount = kBins;
        key.analyzerGeneration = 1;
        key.filterLowHz = 300;
        key.filterHighHz = 2700;

        auto run = [&](int64_t nowMs, bool keyed) {
            return est.update(psd.data(), kBins, kHzPerBin, 300.0, 2700.0, key, nowMs, true, keyed);
        };
        // Frames inside the settle window are refused, however many.
        QVERIFY(!run(0, false).isValid());
        QVERIFY(!run(1000, false).isValid());
        QVERIFY(!run(2000, false).isValid());
        // Past 2.25 s with two frames seen: reports.
        QVERIFY(run(2300, false).isValid());
        // Keying restarts the clock; the first post-MOX frame is refused
        // and so is everything until 2.25 s after the last keyed frame.
        QVERIFY(!run(2400, true).isValid());
        QVERIFY(!run(2500, false).isValid());
        QVERIFY(!run(4700, false).isValid());
        QVERIFY(run(4800, false).isValid());
        // A new key (filter change) restarts it too.
        key.filterHighHz = 2400;
        QVERIFY(!est.update(psd.data(), kBins, kHzPerBin, 300.0, 2400.0, key, 4900, true, false).isValid());
        QVERIFY(!est.update(psd.data(), kBins, kHzPerBin, 300.0, 2400.0, key, 7000, true, false).isValid());
        QVERIFY(est.update(psd.data(), kBins, kHzPerBin, 300.0, 2400.0, key, 7200, true, false).isValid());
        // A stale frame never reports.
        QVERIFY(!est.update(psd.data(), kBins, kHzPerBin, 300.0, 2400.0, key, 7300, false, false).isValid());
    }
};

QTEST_APPLESS_MAIN(TestInPassbandSnrEstimator)
#include "tst_in_passband_snr_estimator.moc"
