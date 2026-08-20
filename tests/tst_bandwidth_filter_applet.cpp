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

using namespace NereusSDR;

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
};

QTEST_MAIN(TestBandwidthFilterApplet)
#include "tst_bandwidth_filter_applet.moc"
