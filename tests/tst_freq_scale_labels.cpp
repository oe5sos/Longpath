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
    // Bis 500 kHz an einem normal breiten Fenster (1130 Punkte) bleibt die
    // alte Treppe Wert fuer Wert (2026-11-08) -- niemand soll auf seinem
    // gewohnten Bild eine Zahl wandern sehen.
    void dieAlteTreppeBleibtBisFuenfhundertKilohertz()
    {
        QCOMPARE(SpectrumWidget::frequenzSchrittForTest(48000.0, 1130), 5000.0);
        QCOMPARE(SpectrumWidget::frequenzSchrittForTest(96000.0, 1130), 10000.0);
        QCOMPARE(SpectrumWidget::frequenzSchrittForTest(192000.0, 1130), 25000.0);
        QCOMPARE(SpectrumWidget::frequenzSchrittForTest(500000.0, 1130), 25000.0);
        QCOMPARE(SpectrumWidget::frequenzSchrittForTest(768000.0, 1130), 50000.0);
    }

    // Breiter als die alte Treppe reichte: 2 m ganz (4 MHz) und das, was
    // der Flex kann (14 MHz). Die Zahlen duerfen nicht enger als 55 Punkte
    // stehen -- vorher waren es bei 4 MHz 80 Zahlen auf 1130 Punkte.
    void breiteBilderBekommenLesbareZahlen()
    {
        for (double bw : {1.0e6, 2.0e6, 4.0e6, 7.68e6, 14.0e6}) {
            for (int px : {600, 1130, 2000}) {
                const double s = SpectrumWidget::frequenzSchrittForTest(bw, px);
                QVERIFY2(px * s / bw >= 55.0,
                         qPrintable(QStringLiteral("%1 Hz auf %2 Punkte: Schritt %3")
                                        .arg(bw).arg(px).arg(s)));
            }
        }
        // 4 MHz auf 1130 Punkte: 250 kHz, also 16 Zahlen wie 144.25, 144.50.
        QCOMPARE(SpectrumWidget::frequenzSchrittForTest(4.0e6, 1130), 250000.0);
        QCOMPARE(SpectrumWidget::freqScaleLabelForTest(144250000.0, 250000.0),
                 QStringLiteral("144.25"));
    }

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
