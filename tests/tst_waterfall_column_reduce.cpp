// no-port-check: Longpath-original test, no Thetis logic.
//
// Wasserfall auf Retina (2026-09-25): die Pipeline liefert Geraete-Pixel,
// das Wasserfallbild hat logische -- zwei Quellpunkte je Zeilenpunkt. Die
// alte Punktabtastung nahm den ersten und verlor jeden zweiten; ein
// schmaler Traeger verschwand. Jetzt fasst die Regel des Wasserfall-
// Detektors zusammen.

#include <QtTest/QtTest>

#include "gui/SpectrumWidget.h"

using namespace Longpath;

class TestWaterfallColumnReduce : public QObject {
    Q_OBJECT
private slots:
    // Ein Traeger im ZWEITEN Punkt eines Paares: Peak zeigt ihn voll,
    // Average mit -3 dB (halbe Leistung), Sample -- wie bestellt -- nicht.
    void carrierInTheSecondOfTwoColumns()
    {
        const QVector<float> src{-140.f, -73.f, -140.f, -140.f};
        const float peak = SpectrumWidget::wfRowSourceReduce(src, 0, 2.0f, SpectrumDetector::Peak);
        const float avg  = SpectrumWidget::wfRowSourceReduce(src, 0, 2.0f, SpectrumDetector::Average);
        const float smp  = SpectrumWidget::wfRowSourceReduce(src, 0, 2.0f, SpectrumDetector::Sample);
        QCOMPARE(peak, -73.0f);
        QVERIFY(std::fabs(avg - (-76.0f)) < 0.1f);
        QCOMPARE(smp, -140.0f);
        // Der zweite Zeilenpunkt sieht nur sein eigenes Paar.
        QCOMPARE(SpectrumWidget::wfRowSourceReduce(src, 1, 2.0f, SpectrumDetector::Peak), -140.0f);
    }

    // 1:1 und Aufweiten: genau der eine Punkt, egal welcher Detektor.
    void oneToOneAndUpscaleTakeTheNearestPoint()
    {
        const QVector<float> src{-100.f, -90.f, -80.f};
        for (auto d : {SpectrumDetector::Peak, SpectrumDetector::Average, SpectrumDetector::Sample}) {
            QCOMPARE(SpectrumWidget::wfRowSourceReduce(src, 1, 1.0f, d), -90.0f);
            QCOMPARE(SpectrumWidget::wfRowSourceReduce(src, 3, 0.5f, d), -90.0f);
        }
    }

    // Am rechten Rand nicht ueber das Feld hinaus.
    void lastColumnStaysInside()
    {
        const QVector<float> src{-1.f, -2.f, -3.f, -4.f, -5.f};
        QCOMPARE(SpectrumWidget::wfRowSourceReduce(src, 2, 2.5f, SpectrumDetector::Peak), -5.0f);
        QVERIFY(SpectrumWidget::wfRowSourceReduce({}, 0, 2.0f, SpectrumDetector::Peak) == 0.0f);
    }
};

QTEST_GUILESS_MAIN(TestWaterfallColumnReduce)
#include "tst_waterfall_column_reduce.moc"
