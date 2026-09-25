// =================================================================
// tests/tst_waterfall_clarity_anchor.cpp  (Longpath)
// =================================================================
//
// Clarity verankert die Wasserfall-Schwellen am Rauschboden der
// Wasserfall-BILDPUNKTE, nicht am Boden der FFT-Bins.
//
// Befund 2026-09-25 an einer SunSDR2 QRP ohne Antenne (WFDIAG-Protokoll
// der laufenden App, Farbverstaerkung 100 / Schwarzwert 125 wie beim
// Betreiber):
//   * echtes I/Q, spitzes Rauschen: Bildpunkte min/med/max
//     -175/-166/-161 dBm, Clarity-Boden aus den Bins -182,5 dBm
//     -> Rauschen bei 72 % der Farbskala, der Wasserfall ganz rot;
//   * Einkanal-Zustand: Bildpunkte -157/-146/-133 dBm, Clarity-Boden
//     -136,5 dBm -> Rauschen unter der Skala, der Wasserfall schwarz.
// Beide Faelle werden hier mit genau diesen Zahlen nachgestellt. Ziel
// (Betreiber: "einen ruhigen harmonischen Wasserfall", "nicht ganz so
// dunkel"): das Rauschen im unteren Drittel, aber sichtbar.
// =================================================================

#include <QtTest/QtTest>
#include <QVector>
#include <algorithm>

#include "core/AppSettings.h"
#include "gui/SpectrumWidget.h"

using namespace Longpath;

namespace {

// Gleichmaessig verteilte Bildpunkte zwischen lo und hi — ohne Zufall,
// damit die Pruefung jedes Mal dasselbe rechnet.
QVector<float> row(float lo, float hi, int n = 600)
{
    QVector<float> r;
    r.reserve(n);
    for (int i = 0; i < n; ++i) {
        // Durchmischt, damit keine Reihenfolge etwas vortaeuscht.
        const int k = (i * 7919) % n;
        r.append(lo + (hi - lo) * float(k) / float(n - 1));
    }
    return r;
}

float median(QVector<float> v)
{
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

// Wo landet das Rauschen (Median der Bildpunkte) auf der Farbskala?
float noiseLevel(SpectrumWidget& w, const QVector<float>& px)
{
    return SpectrumWidget::waterfallIntensityF(
        median(px), w.wfActiveLowThreshold(), w.wfActiveHighThreshold(),
        /*blackLevel=*/125, /*colorGain=*/100);
}

}  // namespace

class TstWaterfallClarityAnchor : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }

    // Gegenprobe: mit den Clarity-Schwellen allein (ohne Boden, also
    // ohne Verankerung) tritt der gemeldete Fehler mit diesen Zahlen auf.
    void withoutAnchorTheIqCaseIsRed()
    {
        SpectrumWidget w;
        w.setWfColorGain(100);
        w.setWfBlackLevel(125);
        w.setClarityActive(true);
        w.setClarityWaterfallThresholds(-187.505f, -127.505f);
        const QVector<float> px = row(-175.0f, -161.0f);
        for (int i = 0; i < 100; ++i) { w.composeWaterfallActiveThresholds(px); }
        const float lvl = noiseLevel(w, px);
        qDebug() << "ohne Verankerung: Rauschen bei" << lvl;
        QVERIFY2(lvl > 0.6f, "die Gegenprobe muss den roten Wasserfall zeigen");
    }

    void iqNoiseSitsLowButVisible()
    {
        SpectrumWidget w;
        w.setWfColorGain(100);
        w.setWfBlackLevel(125);
        w.setClarityActive(true);
        // Clarity: Boden -182,5 aus den Bins, Fenster Boden -5 .. +55.
        w.setClarityWaterfallThresholds(-187.505f, -127.505f, -182.505f);
        const QVector<float> px = row(-175.0f, -161.0f);
        for (int i = 0; i < 200; ++i) { w.composeWaterfallActiveThresholds(px); }
        const float lvl = noiseLevel(w, px);
        qDebug() << "I/Q: Rauschen bei" << lvl << "Schwellen"
                 << w.wfActiveLowThreshold() << w.wfActiveHighThreshold();
        QVERIFY2(lvl < 0.4f, qPrintable(QStringLiteral("Rauschen zu hoch: %1").arg(lvl)));
        QVERIFY2(lvl > 0.1f, qPrintable(QStringLiteral("Rauschen zu dunkel: %1").arg(lvl)));
        // Clarity's Fensterbreite bleibt erhalten, nur verschoben.
        QCOMPARE(w.wfActiveHighThreshold() - w.wfActiveLowThreshold(), 60.0f);
    }

    void singleChannelNoiseIsNotBlack()
    {
        SpectrumWidget w;
        w.setWfColorGain(100);
        w.setWfBlackLevel(125);
        w.setClarityActive(true);
        w.setClarityWaterfallThresholds(-141.552f, -81.5515f, -136.552f);
        const QVector<float> px = row(-157.0f, -133.0f);
        for (int i = 0; i < 200; ++i) { w.composeWaterfallActiveThresholds(px); }
        const float lvl = noiseLevel(w, px);
        qDebug() << "Einkanal: Rauschen bei" << lvl;
        QVERIFY2(lvl > 0.1f, qPrintable(QStringLiteral("Rauschen zu dunkel: %1").arg(lvl)));
        QVERIFY2(lvl < 0.4f, qPrintable(QStringLiteral("Rauschen zu hoch: %1").arg(lvl)));
    }

    // Ruhig heisst: eine einzelne ausreissende Zeile verschiebt die
    // Farben kaum (geglaetteter Boden), ein echter Wechsel (anderes Band)
    // wird ueber Clarity's Sprung sofort neu angesetzt.
    void oneLoudRowBarelyMovesTheColours()
    {
        SpectrumWidget w;
        w.setWfColorGain(100);
        w.setWfBlackLevel(125);
        w.setClarityActive(true);
        w.setClarityWaterfallThresholds(-187.505f, -127.505f, -182.505f);
        const QVector<float> px = row(-175.0f, -161.0f);
        for (int i = 0; i < 200; ++i) { w.composeWaterfallActiveThresholds(px); }
        const float before = w.wfActiveLowThreshold();
        w.composeWaterfallActiveThresholds(row(-145.0f, -131.0f));   // 30 dB lauter
        const float after = w.wfActiveLowThreshold();
        qDebug() << "eine laute Zeile verschiebt um" << (after - before) << "dB";
        QVERIFY2(after - before < 2.0f, "eine einzelne Zeile darf die Farben nicht springen lassen");
    }

    void aClarityJumpReanchorsAtOnce()
    {
        SpectrumWidget w;
        w.setClarityActive(true);
        w.setClarityWaterfallThresholds(-187.505f, -127.505f, -182.505f);
        for (int i = 0; i < 200; ++i) { w.composeWaterfallActiveThresholds(row(-175.0f, -161.0f)); }
        // Anderes Band: Clarity springt um 30 dB, die Bildpunkte auch.
        w.setClarityWaterfallThresholds(-157.505f, -97.505f, -152.505f);
        const QVector<float> px = row(-145.0f, -131.0f);
        w.composeWaterfallActiveThresholds(px);
        const float lvl = SpectrumWidget::waterfallIntensityF(
            median(px), w.wfActiveLowThreshold(), w.wfActiveHighThreshold(), 125, 100);
        qDebug() << "nach dem Sprung, erste Zeile: Rauschen bei" << lvl;
        QVERIFY2(lvl > 0.1f && lvl < 0.4f, "nach einem Bandwechsel sofort richtig, nicht erst nach Sekunden");
    }

    // Ohne Boden (zwei Argumente) bleibt alles wie bisher.
    void twoArgumentCallKeepsTheOldBehaviour()
    {
        SpectrumWidget w;
        w.setClarityActive(true);
        w.setClarityWaterfallThresholds(-150.0f, -10.0f);
        w.composeWaterfallActiveThresholds(row(-120.0f, -100.0f));
        QCOMPARE(w.wfActiveLowThreshold(), -150.0f);
        QCOMPARE(w.wfActiveHighThreshold(), -10.0f);
    }
};

QTEST_MAIN(TstWaterfallClarityAnchor)
#include "tst_waterfall_clarity_anchor.moc"
