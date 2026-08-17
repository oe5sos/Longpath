// =================================================================
// tests/tst_tx_applet_power_scale.cpp  (NereusSDR)
// =================================================================
//
// Die Leistungsanzeige, wenn ein aeusserer Verstaerker arbeitet.
//
// BEFUND, 2026-08-18. Die 2-kW-Skala fuer PGXL und RF-Kit stand NUR an
// der analogen S-Meter-Anzeige im Panelkopf. Die Leistungsanzeige in
// der TxApplet blieb auf der Barfussdecke des Geraets — bei einem
// ANAN-10E also 0..120 W, waehrend der PGXL 800 W lieferte. Die Anzeige
// klebte bei jedem Sendevorgang am Anschlag und sagte damit nichts mehr
// aus.
//
// OE5SOS: „Die 2-kW-Skala fuer PGXL und RF-Kit geht mit ihnen — sie
// gehoert zur Leistungsanzeige, nicht zum Empfangszeiger."
//
// Zwei Faelle, die man leicht uebersieht und die beide hier stehen:
//
//   1. Zurueck auf die Skala des GERAETS, nicht auf eine feste
//      Barfusszahl. Ein HL2 mit 5 W darf nach dem Abschalten des
//      Verstaerkers nicht auf 0..200 W stehen; er stuende dann im
//      ersten Vierzigstel der Anzeige.
//   2. Ein Bandwechsel bei laufendem Verstaerker darf die Skala nicht
//      zurueckziehen. rescaleFwdGaugeForModel rechnet aus der PA-Decke
//      des Geraets, und das waere waehrend des Verstaerkerbetriebs die
//      falsche Antwort.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-18 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QtTest/QtTest>

#include "core/HpsdrModel.h"
#include "gui/HGauge.h"
#include "gui/applets/TxApplet.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

/// Die Leistungsanzeige der TxApplet. Sie ist die ERSTE HGauge im
/// Aufbau (Leistung), die zweite ist die Stehwelle — siehe die
/// Aufbauliste im Kopf von TxApplet.h.
HGauge* fwdGauge(TxApplet& a)
{
    const QList<HGauge*> g = a.findChildren<HGauge*>();
    return g.isEmpty() ? nullptr : g.first();
}

} // namespace

class TestTxAppletPowerScale : public QObject
{
    Q_OBJECT

private slots:

    void barefootScaleFollowsTheRadiosPaCeiling()
    {
        RadioModel m;
        TxApplet a(&m, nullptr);
        HGauge* g = fwdGauge(a);
        QVERIFY(g);

        a.rescaleFwdGaugeForModel(HPSDRModel::HERMESLITE);
        const double hl2Max = g->maximum();

        a.rescaleFwdGaugeForModel(HPSDRModel::ANAN200D);
        const double bigMax = g->maximum();

        QVERIFY2(hl2Max < bigMax,
                 "ein 5-W-Geraet und ein 200-W-Geraet bekommen dieselbe "
                 "Skala — dann sagt die Anzeige bei einem von beiden "
                 "nichts aus");
    }

    void anAmplifierRaisesTheScaleToTwoKilowatts()
    {
        RadioModel m;
        TxApplet a(&m, nullptr);
        HGauge* g = fwdGauge(a);
        QVERIFY(g);

        a.setPowerScale(/*maxWatts=*/0, /*hasAmplifier=*/true);
        QCOMPARE(g->maximum(), 2000.0);
        QCOMPARE(g->redStart(), 1500.0);
    }

    // Der Fall, an dem eine feste Barfusszahl scheitern wuerde.
    void switchingTheAmplifierOffReturnsToTheRadiosOwnScale()
    {
        RadioModel m;
        TxApplet a(&m, nullptr);
        HGauge* g = fwdGauge(a);
        QVERIFY(g);

        a.rescaleFwdGaugeForModel(HPSDRModel::HERMESLITE);
        const double hl2Max = g->maximum();
        QVERIFY(hl2Max < 100.0);           // 5 W + Luft, nicht 120

        a.setPowerScale(0, true);
        QCOMPARE(g->maximum(), 2000.0);

        a.setPowerScale(0, false);
        QCOMPARE(g->maximum(), hl2Max);
    }

    // Ein Bandwechsel ruft rescaleFwdGaugeForModel. Ohne Vorrang der
    // Verstaerkerskala saesse die Anzeige danach wieder auf der
    // Barfussdecke, und 800 W stuenden weit jenseits des Anschlags.
    void aBandChangeDoesNotPullTheScaleBackWhileAmplifying()
    {
        RadioModel m;
        TxApplet a(&m, nullptr);
        HGauge* g = fwdGauge(a);
        QVERIFY(g);

        a.setPowerScale(0, true);
        QCOMPARE(g->maximum(), 2000.0);

        a.rescaleFwdGaugeForModel(HPSDRModel::ANAN100);
        QCOMPARE(g->maximum(), 2000.0);

        // Und nach dem Abschalten greift der Bandwechsel wieder.
        a.setPowerScale(0, false);
        QVERIFY(g->maximum() < 2000.0);
    }
};

QTEST_MAIN(TestTxAppletPowerScale)
#include "tst_tx_applet_power_scale.moc"
