// no-port-check: test fixture asserts the SpectrumAvenger reset matches
// Thetis wdsp/analyzer.c ResetPixelBuffers [@852bf0e] (av_sum = 1.0e-12 in
// recursive-linear mode, -160 dB in recursive-log mode).
//
// 2026-09-25: every panadapter resize flashed red because the port started
// recursive-log averaging from av_sum = 0, i.e. 0 dB.

#include <QtTest/QtTest>

#include <cmath>

#include "gui/spectrum/SpectrumAvenger.h"

using namespace Longpath;

namespace {
constexpr double kAlpha = 0.9;
const QVector<double> kNoCorrection;
}

class TestSpectrumAvengerReset : public QObject {
    Q_OBJECT
private slots:
    // Log mode after resize: starts at -160 dB, never near the top.
    void logModeStartsAtMinus160AfterResize()
    {
        SpectrumAvenger av;
        av.resize(4);
        const QVector<float> in(4, 1.0e-14f);          // -140 dB
        QVector<float> out;
        av.apply(in, 3, kAlpha, 1.0, kNoCorrection, false, 0.0, out);
        const double inDb = 10.0 * std::log10(1.0e-14);
        const double expect = kAlpha * -160.0 + (1.0 - kAlpha) * inDb;
        QCOMPARE(out.size(), 4);
        QVERIFY(std::fabs(out[0] - expect) < 1e-3);
        QVERIFY(out[0] < -139.0f);
    }

    // Linear mode after resize: av_sum starts at 1.0e-12.
    void linearModeStartsAt1em12AfterResize()
    {
        SpectrumAvenger av;
        av.resize(3);
        const QVector<float> in(3, 1.0e-14f);
        QVector<float> out;
        av.apply(in, 1, kAlpha, 1.0, kNoCorrection, false, 0.0, out);
        const double expect = 10.0 * std::log10(kAlpha * 1.0e-12
                                                + (1.0 - kAlpha) * 1.0e-14
                                                + 1.0e-60);
        QVERIFY(std::fabs(out[0] - expect) < 1e-3);
    }

    // clear() resets the same way, and only once: the next frame
    // continues the recursion instead of starting over.
    void clearResetsOnceThenRecursionContinues()
    {
        SpectrumAvenger av;
        av.resize(2);
        const QVector<float> loud(2, 1.0e-10f);        // -100 dB
        QVector<float> out;
        for (int k = 0; k < 200; ++k) {
            av.apply(loud, 3, kAlpha, 1.0, kNoCorrection, false, 0.0, out);
        }
        QVERIFY(std::fabs(out[0] - (-100.0f)) < 0.01f);

        av.clear();
        const QVector<float> quiet(2, 1.0e-14f);       // -140 dB
        av.apply(quiet, 3, kAlpha, 1.0, kNoCorrection, false, 0.0, out);
        const double first = kAlpha * -160.0 + (1.0 - kAlpha) * -140.0;
        QVERIFY(std::fabs(out[0] - first) < 1e-3);
        av.apply(quiet, 3, kAlpha, 1.0, kNoCorrection, false, 0.0, out);
        const double second = kAlpha * first + (1.0 - kAlpha) * -140.0;
        QVERIFY(std::fabs(out[0] - second) < 1e-3);
    }

    // Peak hold and window mode keep the zeroed state (WDSP "default").
    void peakHoldStillStartsFromZero()
    {
        SpectrumAvenger av;
        av.resize(2);
        const QVector<float> in(2, 1.0e-14f);
        QVector<float> out;
        av.apply(in, -1, kAlpha, 1.0, kNoCorrection, false, 0.0, out);
        QVERIFY(std::fabs(out[0] - (-140.0f)) < 0.01f);
    }
};

QTEST_GUILESS_MAIN(TestSpectrumAvengerReset)
#include "tst_spectrum_avenger_reset.moc"
