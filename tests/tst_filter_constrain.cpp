// =================================================================
// tests/tst_filter_constrain.cpp  (NereusSDR)
// =================================================================
//
// Die Filterkanten duerfen nicht ueberall hin.
//
// GEFUNDEN AM 2026-08-20 beim Lesen von Thetis: SliceModel::setFilter
// nahm JEDEN Wert an. Wer in LSB die obere Kante nach rechts ueber die
// Null zog, bekam einen Filter, der in das andere Seitenband
// hineinreicht — und nichts hielt ihn auf. Auch kein Deckel: 200 kHz
// Bandbreite waren moeglich.
//
// Thetis hat dafuer ConstrainFilter (console.cs:34974-35062,
// v2.10.3.15-5-g852bf0e) und laesst JEDE Filteraenderung hindurch, in
// UpdateRX1Filters (console.cs:7510) — dem einen Trichter, durch den
// alles muss.
//
// Zwei Regeln, und nur die erste ist abschaltbar (in Thetis sogar
// VORGABE AUS, console.cs:7446):
//
//   Seitenband — je nach Betriebsart darf eine Kante die Null nicht
//                ueberschreiten.
//   Deckel     — |Kante| <= 10 000 Hz, ausser bei FM. Gilt immer.
//
// Der Unterschied zwischen KANTE ZIEHEN und DURCHLASS VERSCHIEBEN
// steckt in filterShift: beim Verschieben soll die Breite erhalten
// bleiben, also wandert die andere Kante mit, wenn eine anstoesst.
// Ohne diesen Fall waere ein verschobener Durchlass am Rand ploetzlich
// schmaler — und das faellt beim Hoeren auf, nicht beim Lesen.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-20 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>

#include "models/SliceModel.h"

using namespace NereusSDR;

class TestFilterConstrain : public QObject
{
    Q_OBJECT

private slots:

    // ── Der Deckel. Gilt immer, auch ohne Seitenband-Zwang. ──────────

    void nobodyGetsATwoHundredKilohertzFilter()
    {
        int low = -100000, high = 100000;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::USB));
        QCOMPARE(low,  -SliceModel::kMaxFilterWidthHz);
        QCOMPARE(high,  SliceModel::kMaxFilterWidthHz);
    }

    void aNormalFilterIsLeftAlone()
    {
        int low = -2850, high = -150;
        QVERIFY2(!SliceModel::constrainFilter(low, high, DSPMode::LSB),
                 "ein gewoehnlicher LSB-Filter darf nicht angefasst werden");
        QCOMPARE(low,  -2850);
        QCOMPARE(high,  -150);
    }

    // ── Das Seitenband. Nur wenn eingeschaltet. ──────────────────────

    void withoutTheSwitchTheEdgeMayCrossZero()
    {
        // Thetis-Vorgabe ist AUS (console.cs:7446). Wer weiss, was er
        // tut, darf — das ist eine Entscheidung, keine Nachlaessigkeit.
        int low = -2850, high = 500;
        SliceModel::constrainFilter(low, high, DSPMode::LSB,
                                    /*filterShift=*/false,
                                    /*limitToSidebands=*/false);
        QCOMPARE(high, 500);
    }

    void inLowerSidebandTheUpperEdgeStopsAtZero()
    {
        int low = -2850, high = 500;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::LSB,
                                            false, true));
        QCOMPARE(high, 0);
        QVERIFY2(low == -2850,
                 "beim Ziehen einer Kante bleibt die andere stehen — die "
                 "Breite darf sich aendern, das ist der Sinn");
    }

    void inUpperSidebandTheLowerEdgeStopsAtZero()
    {
        int low = -400, high = 2700;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::USB,
                                            false, true));
        QCOMPARE(low, 0);
        QCOMPARE(high, 2700);
    }

    void cwFollowsItsSideband()
    {
        int low = -600, high = 200;
        SliceModel::constrainFilter(low, high, DSPMode::CWL, false, true);
        QCOMPARE(high, 0);

        int low2 = -200, high2 = 600;
        SliceModel::constrainFilter(low2, high2, DSPMode::CWU, false, true);
        QCOMPARE(low2, 0);
    }

    void amKeepsZeroInside()
    {
        // Bei AM liegt der Traeger in der Mitte; ein Durchlass, der
        // die Null nicht enthaelt, hoert die Aussendung gar nicht.
        int low = 200, high = 4000;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::AM,
                                            false, true));
        QCOMPARE(low, 0);

        int low2 = -4000, high2 = -200;
        QVERIFY(SliceModel::constrainFilter(low2, high2, DSPMode::SAM,
                                            false, true));
        QCOMPARE(high2, 0);
    }

    void fmIsLeftEntirelyAlone()
    {
        // Bei FM kommt die Bandbreite aus Hub und Hoehenschnitt, nicht
        // von Hand — auch der Deckel gilt dort nicht.
        int low = -50000, high = 50000;
        QVERIFY2(!SliceModel::constrainFilter(low, high, DSPMode::FM,
                                              false, true),
                 "FM wird nicht angefasst");
        QCOMPARE(low,  -50000);
        QCOMPARE(high,  50000);
    }

    // ── Verschieben: die Breite muss erhalten bleiben ────────────────
    //
    // DER FALL, DER BEIM HOEREN AUFFAELLT UND BEIM LESEN NIE.

    void shiftingKeepsTheWidthWhenAnEdgeHitsZero()
    {
        // 2 400 Hz breit, zu weit nach rechts geschoben.
        int low = -1900, high = 500;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::LSB,
                                            /*filterShift=*/true,
                                            /*limitToSidebands=*/true));
        QCOMPARE(high, 0);
        QVERIFY2(high - low == 2400,
                 "beim Verschieben muss die Breite erhalten bleiben — "
                 "sonst wird der Durchlass am Rand heimlich schmaler");
        QCOMPARE(low, -2400);
    }

    void shiftingKeepsTheWidthAtTheOuterLimitToo()
    {
        int low = -11000, high = -8000;   // 3 000 Hz, ueber dem Deckel
        SliceModel::constrainFilter(low, high, DSPMode::LSB, true, false);
        QCOMPARE(low, -SliceModel::kMaxFilterShiftHz);
        QCOMPARE(high - low, 3000);
    }

    // ── Und der Trichter selbst ──────────────────────────────────────

    void setFilterRefusesAZeroWidthFilter()
    {
        // From Thetis console.cs:7512 — „not a good idea to have a 0hz
        // width filter". Stille sieht aus wie ein kaputter Empfaenger.
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setFilter(100, 2900);

        s.setFilter(1500, 1500);
        QCOMPARE(s.filterLow(),  100);
        QCOMPARE(s.filterHigh(), 2900);
    }

    void setFilterAppliesTheCeiling()
    {
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setFilter(0, 250000);
        QCOMPARE(s.filterHigh(), SliceModel::kMaxFilterWidthHz);
    }

    // Ein zweiter Weg an der Pruefung vorbei ist keine Pruefung.
    void theSingleEdgeSettersGoThroughTheSameFunnel()
    {
        SliceModel s;
        s.setDspMode(DSPMode::LSB);
        s.setLimitFiltersToSidebands(true);
        s.setFilter(-2850, -150);

        s.setFilterHigh(900);
        QCOMPARE(s.filterHigh(), 0);
    }

    // Der Schalter muss sofort wirken, nicht erst beim naechsten
    // Verstellen — sonst wirkt er fuer den Bedienenden gar nicht.
    void turningTheSwitchOnPullsTheFilterBackAtOnce()
    {
        SliceModel s;
        s.setDspMode(DSPMode::LSB);
        s.setFilter(-2850, 600);          // erlaubt, Schalter ist aus
        QCOMPARE(s.filterHigh(), 600);

        s.setLimitFiltersToSidebands(true);
        QCOMPARE(s.filterHigh(), 0);
    }
};

QTEST_MAIN(TestFilterConstrain)
#include "tst_filter_constrain.moc"
