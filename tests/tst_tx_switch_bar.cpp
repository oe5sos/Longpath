// =================================================================
// tests/tst_tx_switch_bar.cpp  (NereusSDR)
// =================================================================
//
// Die vier Sendeschalter der unteren Leiste.
//
// Der Betreiber hat am 2026-08-18 entschieden, dass MOX/VOX/TUNE/PS
// unten erscheinen und ZUGLEICH in der TxApplet bleiben. Zwei Flaechen
// fuer dieselbe Handlung sind sonst der Fehler, den wir an diesem Tag
// zweimal aufgeraeumt haben (doppelter [PROC]-Knopf, doppelter
// S-Meter) — hier ist er ausdruecklich gewollt, und dieser Test ist
// der Preis dafuer.
//
// Er prueft die eine Eigenschaft, die die Doppelung ertraeglich macht:
// BEIDE FLAECHEN HAENGEN AM MODELL UND NICHT ANEINANDER. Keine kennt
// die andere; wer eine umlegt, sieht die andere mitgehen, weil beide
// auf dasselbe Signal hoeren. Laufen sie je auseinander, ist es hier
// zu sehen und nicht erst am Geraet.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-18 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QPushButton>
#include <QSignalSpy>

#include "gui/chrome/TxSwitchBar.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace Longpath;

class TestTxSwitchBar : public QObject
{
    Q_OBJECT

private slots:

    // Die Faltreihenfolge ist eine Festlegung, keine Herleitung: PS
    // faellt zuerst, MOX zuletzt. Der Test NENNT die Sprossen, statt
    // sie nachzurechnen — sonst prueft er nur, dass zwei Formeln
    // uebereinstimmen.
    void foldOrderPutsMoxLast()
    {
        RadioModel m;
        TxSwitch ps  (TxSwitch::Kind::Ps,   &m);
        TxSwitch tune(TxSwitch::Kind::Tune, &m);
        TxSwitch vox (TxSwitch::Kind::Vox,  &m);
        TxSwitch mox (TxSwitch::Kind::Mox,  &m);

        QCOMPARE(ps.rung(),   13);
        QCOMPARE(tune.rung(), 14);
        QCOMPARE(vox.rung(),  15);
        QCOMPARE(mox.rung(),  16);

        // Und die Aussage dahinter, damit eine spaetere Umnummerierung
        // nicht unbemerkt die Bedeutung dreht: niedrig faellt zuerst.
        QVERIFY2(ps.rung() < mox.rung(),
                 "MOX muss die Faltung laenger ueberleben als PS");
    }

    void everySwitchHasALabelledButton()
    {
        RadioModel m;
        struct C { TxSwitch::Kind k; const char* text; };
        for (const C& c : {C{TxSwitch::Kind::Mox,  "MOX"},
                           C{TxSwitch::Kind::Vox,  "VOX"},
                           C{TxSwitch::Kind::Tune, "TUNE"},
                           C{TxSwitch::Kind::Ps,   "PS"}}) {
            TxSwitch s(c.k, &m);
            QVERIFY(s.button());
            QCOMPARE(s.button()->text(), QString::fromLatin1(c.text));
            QVERIFY2(s.button()->isCheckable(),
                     "ein Sendeschalter ohne Zustand waere ein Taster");
        }
    }

    // Der eigentliche Punkt: das Modell fuehrt den Knopf nach, und der
    // Knopf schreibt ins Modell — ohne dass eine Nachfuehrung wieder
    // als Bedienung zaehlt.
    void voxRoundTripsThroughTheModelWithoutEchoing()
    {
        RadioModel m;
        TxSwitch vox(TxSwitch::Kind::Vox, &m);
        TransmitModel& tx = m.transmitModel();

        QSignalSpy spy(&tx, &TransmitModel::voxEnabledChanged);

        // Knopf -> Modell
        vox.button()->setChecked(true);
        QVERIFY(tx.voxEnabled());
        const int afterUser = spy.count();

        // Modell -> Knopf, und KEIN zweites Signal: die Nachfuehrung
        // darf nicht als neue Bedienung zurueckschlagen.
        tx.setVoxEnabled(false);
        QVERIFY(!vox.button()->isChecked());
        QCOMPARE(spy.count(), afterUser + 1);
    }

    // Zwei Flaechen, ein Modell. Hier steht die TxApplet-Seite als
    // zweiter TxSwitch — es geht um die Kopplung ueber das Modell, und
    // die ist dieselbe, egal welche zwei Flaechen es sind.
    void twoSurfacesStayInStepWithoutKnowingEachOther()
    {
        RadioModel m;
        TxSwitch a(TxSwitch::Kind::Vox, &m);
        TxSwitch b(TxSwitch::Kind::Vox, &m);

        a.button()->setChecked(true);
        QVERIFY2(b.button()->isChecked(),
                 "die zweite Flaeche ist der ersten nicht gefolgt");

        b.button()->setChecked(false);
        QVERIFY2(!a.button()->isChecked(),
                 "die erste Flaeche ist der zweiten nicht gefolgt");
        QVERIFY(!m.transmitModel().voxEnabled());
    }

    // Ohne Radio gibt es kein PureSignal. Der Schalter bleibt trotzdem
    // stehen — ein Knopf, der verschwindet und wiederkommt, verschiebt
    // seine Nachbarn, und die untere Leiste haelt ihre Breiten fest.
    void psStaysPutButUnusableWithoutARadio()
    {
        RadioModel m;
        TxSwitch ps(TxSwitch::Kind::Ps, &m);
        QVERIFY(ps.button());
        QCOMPARE(ps.button()->isEnabled(), m.pureSignal() != nullptr);
    }
};

QTEST_MAIN(TestTxSwitchBar)
#include "tst_tx_switch_bar.moc"
