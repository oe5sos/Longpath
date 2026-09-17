// =================================================================
// tests/tst_bandwidth_filter_applet.cpp  (NereusSDR)
// =================================================================
//
// Die Kachel um die Durchlassflaechen.
//
// Zwei Dinge werden hier festgenagelt:
//
//   1. EINE FLAECHE JE EMPFAENGER, und keine leere. Eine zweite
//      Haelfte ohne Scheibe dahinter sieht aus wie ein Fehler.
//   2. Die Zahlen und die Flaeche zeigen dasselbe. Zwei Anzeigen
//      desselben Werts, die auseinanderlaufen, sind schlimmer als
//      eine.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-20 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QSpinBox>

#include "gui/applets/BandwidthFilterApplet.h"
#include "gui/widgets/BandwidthFilterPane.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

// Ueber den Namen, nicht ueber den Wert. Der erste Anlauf suchte das
// Feld mit dem Wert 400 — und fand die untere Kante statt der Breite,
// weil beide zufaellig 400 anzeigten. Ein Test, der das falsche Feld
// verstellt, meldet einen Fehler, den es nicht gibt.
QSpinBox* boxNamed(BandwidthFilterApplet& a, const char* name)
{
    return a.findChild<QSpinBox*>(QString::fromLatin1(name));
}

} // namespace

class TestBandwidthFilterApplet : public QObject
{
    Q_OBJECT

private slots:

    void thereIsOnePanePerReceiver()
    {
        RadioModel model;
        BandwidthFilterApplet a(&model);

        // Ohne Empfaenger genau EINE Flaeche, die „no radio" sagt —
        // eine leere Kachel sieht aus wie ein Fehler.
        QCOMPARE(a.panes().size(), 1);

        model.addSlice();
        a.syncFromModel();
        QCOMPARE(a.panes().size(), model.slices().size());

        model.addSlice();
        a.syncFromModel();
        QVERIFY2(a.panes().size() == model.slices().size(),
                 "eine Flaeche je Empfaenger, keine leere daneben");
    }

    void eachPaneCarriesItsOwnSliceFilter()
    {
        RadioModel model;
        model.addSlice();

        SliceModel* s = model.slices().first();
        s->setDspMode(DSPMode::LSB);
        s->setFilter(-2850, -150);

        BandwidthFilterApplet a(&model);
        QVERIFY(!a.panes().isEmpty());
        QCOMPARE(a.panes().first()->filterLow(),  -2850);
        QCOMPARE(a.panes().first()->filterHigh(), -150);
    }

    // Zwei Anzeigen desselben Werts, die auseinanderlaufen, sind
    // schlimmer als eine.
    void thePaneFollowsTheModel()
    {
        RadioModel model;
        model.addSlice();

        BandwidthFilterApplet a(&model);
        SliceModel* s = model.slices().first();
        s->setDspMode(DSPMode::USB);
        s->setFilter(150, 2550);

        QCOMPARE(a.panes().first()->filterLow(),  150);
        QCOMPARE(a.panes().first()->filterHigh(), 2550);
    }

    void theNumbersFollowTheModelToo()
    {
        RadioModel model;
        model.addSlice();

        BandwidthFilterApplet a(&model);
        SliceModel* s = model.activeSlice() ? model.activeSlice()
                                            : model.slices().first();
        s->setDspMode(DSPMode::USB);
        s->setFilter(150, 2550);

        QSpinBox* low   = boxNamed(a, "bwFilterLow");
        QSpinBox* high  = boxNamed(a, "bwFilterHigh");
        QSpinBox* width = boxNamed(a, "bwFilterWidth");
        QVERIFY(low && high && width);

        QCOMPARE(low->value(),   150);
        QCOMPARE(high->value(),  2550);
        QCOMPARE(width->value(), 2400);
    }

    // Der Grund fuer das Zahlenfeld: eine Breite eintippen und die
    // Kanten setzen sich nach der Regel der Betriebsart.
    void typingAWidthPlacesTheEdgesByMode()
    {
        RadioModel model;
        model.addSlice();

        BandwidthFilterApplet a(&model);
        SliceModel* s = model.activeSlice() ? model.activeSlice()
                                            : model.slices().first();
        s->setDspMode(DSPMode::CWU);
        s->setFilter(400, 800);            // Mitte 600

        QSpinBox* width = boxNamed(a, "bwFilterWidth");
        QVERIFY(width);
        QCOMPARE(width->value(), 400);
        width->setValue(200);

        QCOMPARE(s->filterWidth(),  200);
        QVERIFY2(s->filterCenter() == 600,
                 "bei CW muss die Mitte auf dem Mithoerton stehenbleiben");
    }

    // ── LOW und HIGH sind Audio-Begriffe, bei beiden Seitenbaendern ──
    //
    // Der Betreiber am 2026-09-17, auf 40 m LSB: "100 - 3000 ergibt
    // 2900?!?!?" und "hört sich auf 40 meter katastrophal an". Die
    // Felder zeigten die Betraege der INNEREN Kanten, und die stehen
    // bei LSB verkehrt: LOW zeigte die ferne Kante (2950), HIGH die
    // nahe (150). Wer "LOW 100" tippte, setzte die ferne Kante auf
    // -100, und WIDTH 3000 zaehlte von dort nach oben: -100 … +2900,
    // quer ueber den Traeger. Auf 20 m USB stimmte alles ("50 und 3000
    // sind 3050"), weil dort innere und Audio-Richtung zusammenfallen.

    void lsbShowsNearEdgeAsLowAndFarEdgeAsHigh()
    {
        RadioModel model;
        model.addSlice();
        BandwidthFilterApplet a(&model);
        SliceModel* s = model.activeSlice() ? model.activeSlice()
                                            : model.slices().first();
        s->setDspMode(DSPMode::LSB);
        s->setFilter(-2950, -150);

        QCOMPARE(boxNamed(a, "bwFilterLow")->value(),   150);   // nahe Kante
        QCOMPARE(boxNamed(a, "bwFilterHigh")->value(),  2950);  // ferne Kante
        QCOMPARE(boxNamed(a, "bwFilterWidth")->value(), 2800);
    }

    void lsbLowPlusWidthGivesHighOnTheSameSideband()
    {
        // Genau die Eingabe des Betreibers: LOW 100, dann WIDTH 3000.
        RadioModel model;
        model.addSlice();
        BandwidthFilterApplet a(&model);
        SliceModel* s = model.activeSlice() ? model.activeSlice()
                                            : model.slices().first();
        s->setDspMode(DSPMode::LSB);
        s->setFilter(-2950, -150);

        boxNamed(a, "bwFilterLow")->setValue(100);
        QCOMPARE(s->filterHigh(), -100);            // die NAHE Kante wanderte
        QCOMPARE(s->filterLow(),  -2950);           // die ferne blieb

        boxNamed(a, "bwFilterWidth")->setValue(3000);
        QCOMPARE(s->filterHigh(), -100);
        QVERIFY2(s->filterLow() == -3100,
                 qPrintable(QStringLiteral(
                     "100 + 3000 muss 3100 geben, nicht %1 — und nie ueber "
                     "den Traeger hinweg").arg(qAbs(s->filterLow()))));
        QCOMPARE(boxNamed(a, "bwFilterLow")->value(),  100);
        QCOMPARE(boxNamed(a, "bwFilterHigh")->value(), 3100);
    }

    void usbLowPlusWidthGivesHigh()
    {
        // "20 meter: 50 und 3000 sind 3050" — das muss so bleiben.
        RadioModel model;
        model.addSlice();
        BandwidthFilterApplet a(&model);
        SliceModel* s = model.activeSlice() ? model.activeSlice()
                                            : model.slices().first();
        s->setDspMode(DSPMode::USB);
        s->setFilter(150, 2950);

        boxNamed(a, "bwFilterLow")->setValue(50);
        boxNamed(a, "bwFilterWidth")->setValue(3000);
        QCOMPARE(s->filterLow(),  50);
        QCOMPARE(s->filterHigh(), 3050);
    }

    void widthNeverPushesAnSsbFilterAcrossTheCarrier()
    {
        // HIGH zuletzt gesetzt, dann eine Breite, die groesser ist als
        // HIGH: die nahe Kante darf nicht unter den Traeger rutschen.
        // Sie bleibt bei 0, und die Breite wird von dort aus gehalten.
        RadioModel model;
        model.addSlice();
        BandwidthFilterApplet a(&model);
        SliceModel* s = model.activeSlice() ? model.activeSlice()
                                            : model.slices().first();
        s->setDspMode(DSPMode::LSB);
        s->setFilter(-2950, -150);

        boxNamed(a, "bwFilterHigh")->setValue(2000);   // ferne Kante
        QCOMPARE(s->filterLow(), -2000);
        boxNamed(a, "bwFilterWidth")->setValue(3000);
        QVERIFY2(s->filterHigh() <= 0 && s->filterLow() <= 0,
                 "bei LSB muessen beide Kanten unter dem Traeger bleiben");
        QCOMPARE(s->filterHigh(), 0);
        QCOMPARE(s->filterLow(),  -3000);
    }
};

QTEST_MAIN(TestBandwidthFilterApplet)
#include "tst_bandwidth_filter_applet.moc"
