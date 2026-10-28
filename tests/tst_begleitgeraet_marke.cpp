// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Marke des Begleitgeraets im Panadapter, und der Versatz in dem,
// was dort beschriftet wird.
//
// ── Die Marke sitzt in der ZF, die Zahl daran auf der Antenne ───────
//
// Das ist der Punkt, an dem man es falsch machen kann, und zwar so,
// dass es lange richtig aussieht: gibt man der Marke eine
// Antennenfrequenz, landet sie 116 MHz neben dem Signal -- also
// ausserhalb des Fensters und damit gar nicht im Bild. Ein Betreiber
// liest das als „die Marke kommt nicht", sucht in der Verbindung, und
// die ist in Ordnung.
//
// Darum prueft dieser Pruefstand beides getrennt: WO sie sitzt
// (Geometrie, in der ZF) und WAS daran steht (Beschriftung, auf der
// Antenne).
//
// ── Und die Gleichheit ohne Transverter ─────────────────────────────
//
// Die wichtigsten Faelle hier sind die, in denen sich NICHTS aendern
// darf. Der Versatz greift in die Frequenzachse und in die
// Bandplan-Abschnitte eines Programms, mit dem der Betreiber Contest
// fahrt. Ohne eingestellten Transverter muessen beide Rechnungen die
// Gleichheit sein -- nicht „fast", sondern bitgleich, ohne llround.

#include <QtTest>
#include <QImage>
#include <QPainter>

#include "core/TransverterSetup.h"
#include "gui/SpectrumWidget.h"

using namespace Longpath;

namespace {

constexpr qint64 k28_300  = 28300000;
constexpr qint64 k144_300 = 144300000;

TransverterSetup zweiMeter()
{
    TransverterSetup t;
    t.enabled  = true;
    t.ifLowHz  = 28000000;
    t.ifHighHz = 30000000;
    t.offsetHz = 116000000;
    return t;
}

/// Das Fenster, in dem alle Faelle hier spielen: 28,2 bis 28,4 MHz.
/// Die Mitte liegt auf 28,3 -- dort, wo das Geraet steht.
void stelleFenster(SpectrumWidget& sw)
{
    sw.setFrequencyRange(28300000.0, 200000.0);
}

QImage maleMarke(SpectrumWidget& sw, const QRect& r)
{
    QImage img(r.size(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    QPainter p(&img);
    p.translate(-r.topLeft());
    sw.drawCompanionMarkerForTest(p, r);
    p.end();
    return img;
}

/// Wieviele Spalten des Bildes tragen ueberhaupt Farbe? Die Marke ist
/// eine senkrechte Strichlinie plus ein Schild -- ein paar Spalten,
/// nicht null und nicht alle.
int bemalteSpalten(const QImage& img)
{
    int n = 0;
    for (int x = 0; x < img.width(); ++x) {
        for (int y = 0; y < img.height(); ++y) {
            if (img.pixelColor(x, y) != QColor(Qt::black)) { ++n; break; }
        }
    }
    return n;
}

} // namespace

class TstBegleitgeraetMarke : public QObject {
    Q_OBJECT

private slots:

    void init()
    {
        TransverterSetup::setCurrent(TransverterSetup{});
    }

    // ── Die Umrechnung an den zwei Malstellen ───────────────────────

    void ohneTransverterIstDieRechnungDieGleichheit()
    {
        SpectrumWidget sw;

        // Bitgleich, und zwar fuer Werte, bei denen ein llround etwas
        // tun WUERDE: 28300000.4 ist kein ganzzahliges Hz.
        QCOMPARE(sw.rfFuerAnzeigeForTest(28300000.4), 28300000.4);
        QCOMPARE(sw.zfFuerAchseForTest(28300000.4),  28300000.4);
        QCOMPARE(sw.rfFuerAnzeigeForTest(14200000.0), 14200000.0);
        QCOMPARE(sw.rfFuerAnzeigeForTest(0.0),        0.0);
    }

    void mitTransverterRechnetDieAnzeigeHoch()
    {
        TransverterSetup::setCurrent(zweiMeter());
        SpectrumWidget sw;

        QCOMPARE(sw.rfFuerAnzeigeForTest(28300000.0), 144300000.0);
        QCOMPARE(sw.zfFuerAchseForTest(144300000.0),   28300000.0);
    }

    void mitTransverterBleibtAusserhalbUnberuehrt()
    {
        TransverterSetup::setCurrent(zweiMeter());
        SpectrumWidget sw;

        // 20 m mit eingeschaltetem 2-m-Transverter.
        QCOMPARE(sw.rfFuerAnzeigeForTest(14200000.0), 14200000.0);
    }

    void dieRechnungGehtHinUndZurueck()
    {
        TransverterSetup::setCurrent(zweiMeter());
        SpectrumWidget sw;

        for (qint64 zf : {28000000LL, 28300000LL, 29700000LL, 30000000LL}) {
            const double rf = sw.rfFuerAnzeigeForTest(static_cast<double>(zf));
            QCOMPARE(sw.zfFuerAchseForTest(rf), static_cast<double>(zf));
        }
    }

    // ── Wo die Marke sitzt ──────────────────────────────────────────

    void ausgeschaltetMaltNichts()
    {
        SpectrumWidget sw;
        stelleFenster(sw);
        const QRect r(0, 0, 400, 200);

        QVERIFY(!sw.companionMarkerShown());
        QCOMPARE(bemalteSpalten(maleMarke(sw, r)), 0);
    }

    void verbundenAberOhneFrequenzMaltNichts()
    {
        SpectrumWidget sw;
        stelleFenster(sw);
        const QRect r(0, 0, 400, 200);

        // 0 Hz heisst „noch nichts gelesen". Ohne diese Pruefung malte
        // die Marke am linken Rand, und mit Transverter stuende
        // „116.000" daran -- eine Zahl, die das Geraet nie gemeldet hat.
        sw.setCompanionMarker(true, 0);
        QCOMPARE(bemalteSpalten(maleMarke(sw, r)), 0);
    }

    void dieMarkeSitztInDerZf()
    {
        TransverterSetup::setCurrent(zweiMeter());
        SpectrumWidget sw;
        stelleFenster(sw);
        const QRect r(0, 0, 400, 200);

        // Das Geraet steht auf 28,3 MHz ZF -- der Mitte des Fensters.
        sw.setCompanionMarker(true, k28_300);
        QVERIFY(bemalteSpalten(maleMarke(sw, r)) > 0);

        // Und die ANTENNENfrequenz derselben Station liegt weit
        // ausserhalb: gibt jemand die hier hinein, ist die Marke weg.
        // Das ist der Fehler, den dieser Fall festnagelt.
        sw.setCompanionMarker(true, k144_300);
        QCOMPARE(bemalteSpalten(maleMarke(sw, r)), 0);
    }

    void ausserhalbDesFenstersMaltNichts()
    {
        SpectrumWidget sw;
        stelleFenster(sw);          // 28,2 .. 28,4 MHz
        const QRect r(0, 0, 400, 200);

        sw.setCompanionMarker(true, 28900000);
        QCOMPARE(bemalteSpalten(maleMarke(sw, r)), 0);
    }

    void dieMarkeBleibtSchmal()
    {
        SpectrumWidget sw;
        stelleFenster(sw);
        const QRect r(0, 0, 400, 200);
        sw.setCompanionMarker(true, k28_300);

        // Hausstil: Farbe bedeckt hoechstens zwei Prozent der Flaeche.
        // Eine Strichlinie und ein kurzes Schild -- kein Streifen. Die
        // Grenze hier ist grosszuegig (ein Fuenftel der Spalten), weil
        // das Schild Schrift traegt und Schrift je nach Fassung
        // unterschiedlich breit faellt; sie faengt die Richtung, in die
        // es schiefgehen koennte.
        const int spalten = bemalteSpalten(maleMarke(sw, r));
        QVERIFY2(spalten > 0, "Nichts gemalt -- dann prueft die Grenze nichts.");
        QVERIFY2(spalten < r.width() / 5,
                 qPrintable(QStringLiteral("zu breit: %1 von %2 Spalten")
                                .arg(spalten).arg(r.width())));
    }

    void amRechtenRandKipptDasSchildNachLinks()
    {
        SpectrumWidget sw;
        // Mitte so legen, dass die Marke knapp am rechten Rand sitzt.
        sw.setFrequencyRange(28250000.0, 100000.0);   // 28,20 .. 28,30
        const QRect r(0, 0, 400, 200);
        sw.setCompanionMarker(true, 28299000);

        const QImage img = maleMarke(sw, r);

        // Die Marke selbst sitzt bei etwa x = 396 von 400. Gekippt
        // heisst: das Schild reicht von dort nach LINKS, die erste
        // bemalte Spalte liegt also deutlich vor dem rechten Rand.
        // Waere es nicht gekippt, begaenne das Schild erst bei x+3 und
        // waere auf drei Spalten abgeschnitten -- dann lage die erste
        // bemalte Spalte bei der Marke selbst.
        int erste = -1;
        for (int x = 0; x < img.width() && erste < 0; ++x) {
            for (int y = 0; y < img.height(); ++y) {
                if (img.pixelColor(x, y) != QColor(Qt::black)) {
                    erste = x; break;
                }
            }
        }
        QVERIFY2(erste >= 0, "Nichts gemalt -- dann prueft der Fall nichts.");
        QVERIFY2(erste < r.width() - 20,
                 qPrintable(QStringLiteral(
                     "erste bemalte Spalte %1 von %2 -- das Schild ist nicht "
                     "nach links gekippt").arg(erste).arg(r.width())));
    }

    // ── Der Setter meldet nur echte Aenderungen ─────────────────────

    void derSetterMerktSichDenStand()
    {
        SpectrumWidget sw;
        sw.setCompanionMarker(true, k28_300);
        QVERIFY(sw.companionMarkerShown());
        QCOMPARE(sw.companionMarkerHz(), k28_300);

        sw.setCompanionMarker(false, 0);
        QVERIFY(!sw.companionMarkerShown());
    }

    // ── Die Frequenzachse ───────────────────────────────────────────

    void dieAchsenbeschriftungBleibtEineBeschriftung()
    {
        // freqScaleLabel selbst rechnet NICHT -- der Versatz sitzt an
        // der Aufrufstelle. Dieser Fall haelt das fest: wer die
        // Umrechnung spaeter hierher zieht, rechnet zweimal.
        const QString a = SpectrumWidget::freqScaleLabelForTest(28300000.0, 5000.0);
        TransverterSetup::setCurrent(zweiMeter());
        const QString b = SpectrumWidget::freqScaleLabelForTest(28300000.0, 5000.0);
        QCOMPARE(a, b);
    }

    // ── Der Bandplan ────────────────────────────────────────────────

    void bandplanOhneVerwalterMaltNichts()
    {
        // Kein BandPlanManager gesetzt: drawBandPlan muss das aushalten
        // und nicht in einen Nullzeiger greifen -- der Pruefstand baut
        // das Widget nackt.
        SpectrumWidget sw;
        stelleFenster(sw);
        const QRect r(0, 0, 400, 200);

        QImage img(r.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::black);
        QPainter p(&img);
        sw.drawBandPlanForTest(p, r);
        p.end();
        QCOMPARE(bemalteSpalten(img), 0);
    }
};

QTEST_MAIN(TstBegleitgeraetMarke)
#include "tst_begleitgeraet_marke.moc"
