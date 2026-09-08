// SPDX-License-Identifier: GPL-3.0-or-later
//
// RTTY decoder sensitivity mapping (gui/RttyDecoderSensitivity.h): slider
// 0..100 -> confidence threshold 0.50..0.95 on RttyDecoder's own [0.5, 1.0]
// confidence scale. Adapted from AetherSDR's own
// tests/rtty_decoder_sensitivity_test.cpp (same formula, same expected
// values -- see RttyDecoderSensitivity.h for the full attribution).

#include <QtTest>

#include "gui/RttyDecoderSensitivity.h"

using namespace Longpath;

static_assert(rttyConfThresholdFor(0) == 0.5f,
              "0 = show everything: the threshold is the confidence floor");
static_assert(rttyConfThresholdFor(100) == 0.95f,
              "100 = only near-certain copy");
static_assert(kRttySensitivityDefault == 0,
              "the default is a provable no-op: confidence is >= 0.5 by "
              "construction, so nothing can fall below the floor threshold");
static_assert(rttyConfThresholdFor(kRttySensitivityDefault) == 0.5f,
              "default threshold sits exactly on the confidence floor");

class TstRttyDecoderSensitivity : public QObject
{
    Q_OBJECT

private slots:
    void thresholdMatchesKnownPoints_data()
    {
        QTest::addColumn<int>("sens");
        QTest::addColumn<float>("expected");

        QTest::newRow("floor/default") << 0   << 0.500f;
        QTest::newRow("documented starting value (~3dB lock point)") << 38 << 0.671f;
        QTest::newRow("midpoint") << 50  << 0.725f;
        QTest::newRow("ceiling") << 100 << 0.950f;
        QTest::newRow("clamped below") << -5  << 0.500f;
        QTest::newRow("clamped above") << 250 << 0.950f;
    }

    void thresholdMatchesKnownPoints()
    {
        QFETCH(int, sens);
        QFETCH(float, expected);
        QCOMPARE_LT(std::abs(rttyConfThresholdFor(sens) - expected), 0.0005f);
    }

    void thresholdIsStrictlyMonotonic()
    {
        // More sensitivity never lets MORE noise through.
        for (int s = 1; s <= 100; ++s) {
            QVERIFY2(rttyConfThresholdFor(s) > rttyConfThresholdFor(s - 1),
                     qPrintable(QStringLiteral("not strictly increasing at %1").arg(s)));
        }
    }
};

QTEST_MAIN(TstRttyDecoderSensitivity)
#include "tst_rtty_decoder_sensitivity.moc"
