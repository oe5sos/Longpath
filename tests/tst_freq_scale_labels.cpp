// no-port-check: Longpath-original test, no Thetis logic.
//
// Frequenzskala (2026-09-25): beim 25-kHz-Raster stand "14.18" an der
// Marke 14,175 -- zwei Nachkommastellen fuer jeden Abstand ab 10 kHz.
// Eine Beschriftung muss die Frequenz nennen, an der sie steht.

#include <QtTest/QtTest>

#include "gui/SpectrumWidget.h"

using namespace Longpath;

class TestFreqScaleLabels : public QObject {
    Q_OBJECT
private slots:
    void labelNamesTheFrequencyItStandsAt()
    {
        // 25 kHz: drei Stellen, sonst luegt jede zweite Marke.
        QCOMPARE(SpectrumWidget::freqScaleLabel(14175000.0, 25000.0), QStringLiteral("14.175"));
        QCOMPARE(SpectrumWidget::freqScaleLabel(14225000.0, 25000.0), QStringLiteral("14.225"));
        QCOMPARE(SpectrumWidget::freqScaleLabel(14200000.0, 25000.0), QStringLiteral("14.200"));
        // 5 kHz (QRP, 48 kHz Spanne): wie bisher drei Stellen.
        QCOMPARE(SpectrumWidget::freqScaleLabel(14105000.0, 5000.0), QStringLiteral("14.105"));
        // 10 und 50 kHz: zwei Stellen genuegen und bleiben.
        QCOMPARE(SpectrumWidget::freqScaleLabel(14110000.0, 10000.0), QStringLiteral("14.11"));
        QCOMPARE(SpectrumWidget::freqScaleLabel(14150000.0, 50000.0), QStringLiteral("14.15"));
        // 100 kHz und mehr: eine Stelle.
        QCOMPARE(SpectrumWidget::freqScaleLabel(14300000.0, 100000.0), QStringLiteral("14.3"));
    }

    // Jede Marke eines Rasters, zurueckgelesen, trifft ihre Frequenz.
    void everyMarkRoundTrips()
    {
        for (double step : {5000.0, 10000.0, 25000.0, 50000.0, 100000.0}) {
            for (double f = 14000000.0; f <= 14350000.0; f += step) {
                const double back = SpectrumWidget::freqScaleLabel(f, step).toDouble() * 1.0e6;
                QVERIFY2(std::fabs(back - f) < 0.5,
                         qPrintable(QStringLiteral("%1 bei Raster %2").arg(f).arg(step)));
            }
        }
    }
};

QTEST_MAIN(TestFreqScaleLabels)
#include "tst_freq_scale_labels.moc"
