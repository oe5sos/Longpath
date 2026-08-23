// SPDX-License-Identifier: GPL-3.0-or-later
//
// BEFUND, keine Forderung: wie stark stuft der Wasserfall wirklich?
//
// Anlass, 2026-08-23. Der Betreiber wies darauf hin, dass die Anvelina
// mehr Dynamikumfang hat als die ANAN 10E. Meine Vermutung daraufhin:
// waterfallIntensity quantelt auf quint8 — 256 Stufen ueber die ganze
// Spanne —, und bei grosser Spanne muesse man das als Baender sehen.
// Ich hatte schon eine Bayer-Streuung dagegen eingebaut.
//
// DIESE PRUEFUNG HAT DIE VERMUTUNG WIDERLEGT. Gemessen an einem
// gleichmaessigen Verlauf ueber 1200 Bildpunkte:
//
//   ohne Streuung: 813 Farben, laengster Lauf 8 px
//   mit  Streuung: 720 Farben, laengster Lauf 8 px
//   40 dB Fenster: 8 px      140 dB Fenster: 8 px
//
// Acht Bildpunkte auf 1200 sind 0,7 Prozent — kein sichtbares Band.
// Und die Zahl aendert sich weder mit der Spanne noch mit der
// Streuung. Die Grenze liegt also NICHT bei der Intensitaet, sondern
// beim 8-Bit-Ausgang der Palette, und dort ist sie unvermeidlich.
//
// Die Streuung ist daraufhin wieder ausgebaut worden. Diese Pruefung
// bleibt — sie ist der Beleg, dass hier nichts zu holen ist, und sie
// wuerde anschlagen, falls sich das je aendert.
//
// Ein Zwischenfall, der festgehalten gehoert: der erste Lauf stand auf
// blackLevel 0 und meldete Baender von 602 px. Das war ein Fehler im
// Pruefblatt — wfBlackLevelOffsetDb(0) schiebt die Untergrenze um
// 50 dB, der halbe Verlauf lag am Anschlag. Eine Messung, deren
// Randbedingungen nicht stimmen, sagt genauso zuverlaessig etwas
// Falsches wie gar keine.

#include <QtTest>
#include <QImage>
#include <QSet>

#include "gui/SpectrumWidget.h"

using namespace Longpath;

namespace {

struct Stats { int distinct; int longestRun; };

// Ein gleichmaessiger Verlauf ueber die volle Spanne, so wie ihn ein
// Rauschteppich mit langsam steigendem Pegel erzeugt.
// blackLevel 125 und colorGain 0 sind die NEUTRALE Stellung: die
// Versaetze sind dann null (wfBlackLevelOffsetDb = (125-125)*0,4) und
// das Fenster liegt genau auf [lowDbm, highDbm].
//
// Der erste Lauf dieser Pruefung stand auf blackLevel 0 — das schiebt
// die Untergrenze um 50 dB nach oben, der halbe Verlauf lag am
// Anschlag, und die Messung war unbrauchbar. Sie meldete "die Streuung
// hilft nicht", was schlicht falsch war.
constexpr int kNeutralBlack = 125;
constexpr int kNeutralGain  = 0;

Stats gradientStats(bool dither, int width, float lowDbm, float highDbm)
{
    QVector<QRgb> row(width);
    // Dieselbe Bayer-Matrix wie im Widget. Sie hier zu wiederholen ist
    // eine Doppelung — aber die Alternative waere, pushWaterfallRow
    // aufzurufen, und das braucht ein lebendes Widget mit Grafikkarte.
    // Fuer eine reine Zahlenfrage ist das der falsche Preis.
    static const float kBayer[16] = {
         0.0f/16, 8.0f/16,  2.0f/16, 10.0f/16,
        12.0f/16, 4.0f/16, 14.0f/16,  6.0f/16,
         3.0f/16, 11.0f/16, 1.0f/16,  9.0f/16,
        15.0f/16, 7.0f/16, 13.0f/16,  5.0f/16,
    };
    constexpr float kOneStep = 1.0f / 255.0f;

    for (int x = 0; x < width; ++x) {
        const float dbm = lowDbm
            + (highDbm - lowDbm) * float(x) / float(width - 1);
        const float f = SpectrumWidget::waterfallIntensityF(
            dbm, lowDbm, highDbm, kNeutralBlack, kNeutralGain);
        const float d = dither ? (kBayer[(x & 3)] - 0.5f) * kOneStep : 0.0f;
        row[x] = SpectrumWidget::waterfallColorForIntensityF(
            f + d, WfColorScheme::ClarityBlue);
    }

    QSet<QRgb> seen;
    int longest = 1;
    int run = 1;
    for (int x = 0; x < width; ++x) {
        seen.insert(row[x]);
        if (x > 0) {
            if (row[x] == row[x - 1]) { ++run; longest = qMax(longest, run); }
            else                      { run = 1; }
        }
    }
    return {int(seen.size()), longest};
}

} // namespace

class TstWaterfallBanding : public QObject
{
    Q_OBJECT

private slots:
    void streuungVerkuerztDieBaender()
    {
        // 100 dB Fenster auf 1200 Bildpunkte — die Lage, die der
        // Betreiber vor sich hat.
        const Stats ohne = gradientStats(false, 1200, -140.0f, -40.0f);
        const Stats mit  = gradientStats(true,  1200, -140.0f, -40.0f);

        qInfo().noquote()
            << QStringLiteral("ohne Streuung: %1 Farben, laengstes Band %2 px")
                   .arg(ohne.distinct).arg(ohne.longestRun);
        qInfo().noquote()
            << QStringLiteral("mit  Streuung: %1 Farben, laengstes Band %2 px")
                   .arg(mit.distinct).arg(mit.longestRun);

        // Der Befund: acht Bildpunkte oder weniger. Wuerde das je
        // deutlich groesser, waere die Ueberlegung von 2026-08-23 doch
        // richtig gewesen und die Sache neu anzusehen.
        QVERIFY2(ohne.longestRun <= 16,
                 qPrintable(QStringLiteral("laengstes Band %1 px — das waere "
                                           "sichtbar").arg(ohne.longestRun)));
        // Und die Streuung aendert daran NICHTS. Genau darum ist sie
        // wieder ausgebaut.
        QCOMPARE(mit.longestRun, ohne.longestRun);
    }

    void beiGrosserSpanneWirdEsSchlimmer()
    {
        // Der eigentliche Punkt des Betreibers: mehr Dynamikumfang,
        // mehr Stufen. Wenn das nicht messbar ist, ist die ganze
        // Ueberlegung falsch.
        const Stats eng  = gradientStats(false, 1200, -100.0f, -60.0f);
        const Stats weit = gradientStats(false, 1200, -160.0f, -20.0f);
        qInfo().noquote()
            << QStringLiteral("40 dB Fenster: laengstes Band %1 px")
                   .arg(eng.longestRun);
        qInfo().noquote()
            << QStringLiteral("140 dB Fenster: laengstes Band %1 px")
                   .arg(weit.longestRun);
        // Hier stand die Erwartung "groessere Spanne, breitere Baender".
        // Sie ist falsch: die Bandbreite haengt an der Palette, nicht an
        // der Spanne. Festgehalten wird jetzt, was WIRKLICH gilt.
        QCOMPARE(weit.longestRun, eng.longestRun);
    }

    void rundenStattAbschneiden()
    {
        // Die alte Palette schnitt mit static_cast<int> ab und verlor im
        // Mittel eine halbe Farbstufe je Kanal. Gegenprobe: die
        // Fliesskomma-Fassung darf an den Stuetzstellen NICHT unter der
        // alten liegen.
        for (int i = 0; i <= 255; ++i) {
            const QRgb neu = SpectrumWidget::waterfallColorForIntensityF(
                float(i) / 255.0f, WfColorScheme::ClarityBlue);
            const QRgb alt = SpectrumWidget::waterfallColorForIntensity(
                quint8(i), WfColorScheme::ClarityBlue);
            QVERIFY2(qRed(neu)   >= qRed(alt)
                     && qGreen(neu) >= qGreen(alt)
                     && qBlue(neu)  >= qBlue(alt),
                     qPrintable(QStringLiteral("bei %1: neu (%2,%3,%4) "
                                               "unter alt (%5,%6,%7)")
                                    .arg(i)
                                    .arg(qRed(neu)).arg(qGreen(neu)).arg(qBlue(neu))
                                    .arg(qRed(alt)).arg(qGreen(alt)).arg(qBlue(alt))));
        }
        qInfo() << "Runden statt Abschneiden: nirgends dunkler als vorher";
    }
};

QTEST_MAIN(TstWaterfallBanding)
#include "tst_waterfall_banding.moc"
