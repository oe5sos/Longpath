// SPDX-License-Identifier: GPL-3.0-or-later
//
// Steht ein Panadapter auf KiwiSDR, zeigt er den KiwiSDR — und NUR
// den.
//
// KiwiSDR Stufe 6, 2026-08-23. Der Zuschnitt nutzt aus, dass unsere
// Bin->Frequenz-Zuordnung ueber m_ddcCenterHz und m_sampleRateHz
// laeuft: setzt man beide auf den Kiwi-Ausschnitt, ist der Kiwi
// einfach eine weitere Quelle von Bins, und Zoom, Detektor, Mittelung
// und Wasserfall arbeiten unveraendert weiter.
//
// Der gefaehrliche Teil daran ist die ZWEITE Quelle. Der DDC des
// Geraets laeuft ja weiter und schreibt in dieselben Puffer. Ohne
// Sperre saehe man ein Flackern zwischen zwei Baendern — genau das
// Muster, das den Betreiber am 2026-08-22 an "2 frequenzen
// gleichzeitig" denken liess.
//
// Diese Pruefung misst darum nicht, ob die Methode zurueckkehrt,
// sondern was am Ende IM BILD steht: dbmOverRange() liest die
// gerechneten Pegel aus derselben Leitung, aus der auch gezeichnet
// wird.

#include <QtTest>

#include "gui/SpectrumWidget.h"

using namespace Longpath;

namespace {

// Ein Rauschteppich mit einem Traeger in der Mitte.
QVector<float> dbmBins(int n, float floorDb, float peakDb)
{
    QVector<float> v(n, floorDb);
    v[n / 2] = peakDb;
    return v;
}

QVector<float> linearBins(int n, float floorDb, float peakDb)
{
    QVector<float> v(n);
    for (int i = 0; i < n; ++i) {
        v[i] = std::pow(10.0f, floorDb * 0.1f);
    }
    v[n / 2] = std::pow(10.0f, peakDb * 0.1f);
    return v;
}

} // namespace

class TstKiwiDisplaySource : public QObject
{
    Q_OBJECT

private slots:
    void ohneUmschaltungKommtNichtsVomKiwi()
    {
        SpectrumWidget sw;
        sw.resize(600, 300);
        sw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&sw));

        QVERIFY(!sw.kiwiDisplaySource());
        // Der Kiwi schickt, aber es steht nicht auf ihm: nichts darf
        // ankommen. Sonst uebermalte ein Kiwi im Hintergrund das Bild
        // eines Betreibers, der ihn gar nicht sehen will.
        sw.updateKiwiSpectrumDbm(dbmBins(256, -120.0f, -60.0f),
                                 14.000e6, 14.200e6);
        const QVector<float> got = sw.dbmOverRange(14.09e6, 14.11e6, 16);
        bool anySignal = false;
        for (float f : got) { if (f > -100.0f) { anySignal = true; } }
        qInfo() << "ohne Umschaltung, Werte vorhanden:" << !got.isEmpty()
                << "Signal:" << anySignal;
        QVERIFY2(!anySignal,
                 "Der Kiwi hat gezeichnet, obwohl das Geraet gewaehlt ist");
    }

    void nachDemUmschaltenKommtDerKiwiAn()
    {
        SpectrumWidget sw;
        sw.resize(600, 300);
        sw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&sw));

        sw.setKiwiDisplaySource(true);
        QVERIFY(sw.kiwiDisplaySource());

        sw.setFrequencyRange(14.100e6, 200000.0);
        sw.updateKiwiSpectrumDbm(dbmBins(256, -120.0f, -50.0f),
                                 14.000e6, 14.200e6);

        const QVector<float> got = sw.dbmOverRange(14.095e6, 14.105e6, 16);
        QVERIFY(!got.isEmpty());
        float best = -999.0f;
        for (float f : got) { best = qMax(best, f); }
        qInfo() << "Kiwi-Traeger gemessen:" << best << "dBm";
        // Der Traeger sitzt in der Mitte des Kiwi-Ausschnitts, also bei
        // 14,100 MHz. Er muss deutlich ueber dem Rauschen stehen.
        QVERIFY2(best > -90.0f,
                 qPrintable(QStringLiteral("nur %1 dBm gemessen").arg(best)));
    }

    void dasGeraetKommtNichtMehrDurch()
    {
        // DER eigentliche Punkt dieser Stufe. Ohne die Sperre in
        // updateSpectrumLinear schreiben Geraet und Kiwi abwechselnd
        // in dieselben Puffer.
        SpectrumWidget sw;
        sw.resize(600, 300);
        sw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&sw));

        sw.setKiwiDisplaySource(true);
        sw.setFrequencyRange(14.100e6, 200000.0);
        sw.updateKiwiSpectrumDbm(dbmBins(256, -130.0f, -50.0f),
                                 14.000e6, 14.200e6);
        const float kiwiPeak = [&] {
            float b = -999.0f;
            for (float f : sw.dbmOverRange(14.095e6, 14.105e6, 16)) {
                b = qMax(b, f);
            }
            return b;
        }();

        // Jetzt schiebt das Geraet einen ANDEREN Traeger nach. Er darf
        // das Bild nicht veraendern.
        sw.updateSpectrumLinear(0, linearBins(256, -60.0f, -20.0f), 1.0, 0.0);
        const float afterDevice = [&] {
            float b = -999.0f;
            for (float f : sw.dbmOverRange(14.095e6, 14.105e6, 16)) {
                b = qMax(b, f);
            }
            return b;
        }();

        qInfo() << "Kiwi:" << kiwiPeak << "dBm  ·  nach dem Geraet:"
                << afterDevice << "dBm";
        QVERIFY2(qAbs(afterDevice - kiwiPeak) < 1.0f,
                 qPrintable(QStringLiteral(
                     "Das Geraet hat durchgeschlagen: %1 -> %2 dBm")
                                .arg(kiwiPeak).arg(afterDevice)));
    }

    void zurueckAufsGeraetGehtAuch()
    {
        SpectrumWidget sw;
        sw.resize(600, 300);
        sw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&sw));

        sw.setKiwiDisplaySource(true);
        sw.setKiwiDisplaySource(false);
        sw.setFrequencyRange(14.100e6, 200000.0);
        sw.setDdcCenterFrequency(14.100e6);
        sw.setSampleRate(200000.0);
        sw.updateSpectrumLinear(0, linearBins(256, -130.0f, -50.0f), 1.0, 0.0);

        float best = -999.0f;
        for (float f : sw.dbmOverRange(14.095e6, 14.105e6, 16)) {
            best = qMax(best, f);
        }
        qInfo() << "zurueck auf dem Geraet:" << best << "dBm";
        QVERIFY2(best > -90.0f, "Nach dem Zurueckschalten kommt nichts an");
    }
};

QTEST_MAIN(TstKiwiDisplaySource)
#include "tst_kiwi_display_source.moc"
