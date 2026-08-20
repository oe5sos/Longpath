// =================================================================
// tests/tst_signal_history_markers.cpp  (NereusSDR)
// =================================================================
//
// Der zweite Markenkanal im Panadapter: die Marken des S-Verlaufs
// neben den DX-Spots.
//
// Port aus AetherSDR SpectrumWidget.cpp:15719-15741 [@0cd4559].
//
// Geprueft wird der ZUSTAND (welche Marken liegen an, welche Schalter
// stehen wie), nicht das gemalte Bild — dieselbe Entscheidung wie bei
// den uebrigen Anzeigemerkmalen. Was gezeichnet wird, steht in der
// Bank-Matrix.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>

#include "gui/SpectrumWidget.h"

using namespace Longpath;

namespace {

SpectrumWidget::SpotMarker mark(double freqMhz, const QString& source,
                                const QString& label)
{
    SpectrumWidget::SpotMarker m;
    m.callsign = label;
    m.freqMhz  = freqMhz;
    m.source   = source;
    return m;
}

} // namespace

class TestSignalHistoryMarkers : public QObject
{
    Q_OBJECT

private slots:

    // Vorgabe aus — das Merkmal ist neu, es gibt keinen Istzustand zu
    // erhalten. Faellt dieser Fall, hat jemand die Vorgabe gedreht und
    // schaltet allen Betreibern etwas ein, worum sie nicht gebeten
    // haben.
    void bothTogglesAreOffByDefault()
    {
        SpectrumWidget w;
        QVERIFY(!w.showSignalHistory());
        QVERIFY(!w.showSignalHistoryQrm());
    }

    void togglesAreIndependent()
    {
        SpectrumWidget w;
        w.setShowSignalHistory(true);
        QVERIFY(w.showSignalHistory());
        QVERIFY2(!w.showSignalHistoryQrm(),
                 "Stoerungsmarken haengen nicht an den Sprachmarken");

        w.setShowSignalHistoryQrm(true);
        w.setShowSignalHistory(false);
        QVERIFY(w.showSignalHistoryQrm());
    }

    // Der Kanal ist getrennt von den DX-Spots: setSpotMarkers darf die
    // Verlaufsmarken nicht loeschen und umgekehrt. Sonst wuerde die
    // naechste Spot-Lieferung die Erkennung ausradieren.
    void theTwoMarkerChannelsAreSeparate()
    {
        SpectrumWidget w;
        w.setSignalHistoryMarkers({mark(14.205, QStringLiteral("SHistory"),
                                        QStringLiteral("S7"))});
        QCOMPARE(w.signalHistoryMarkersForTest().size(), 1);

        w.setSpotMarkers({mark(14.210, QStringLiteral("DXCluster"),
                               QStringLiteral("DL1ABC"))});
        QCOMPARE(w.signalHistoryMarkersForTest().size(), 1);
        QCOMPARE(w.signalHistoryMarkersForTest().first().callsign,
                 QStringLiteral("S7"));
    }

    void markersCanBeCleared()
    {
        SpectrumWidget w;
        w.setSignalHistoryMarkers({mark(14.205, QStringLiteral("SHistory"),
                                        QStringLiteral("S7"))});
        w.setSignalHistoryMarkers({});
        QVERIFY(w.signalHistoryMarkersForTest().isEmpty());
    }

    // Die Quelle unterscheidet Sprache von Stoerung; daran haengt im
    // Malweg, welcher der beiden Schalter die Marke zeigt und in
    // welcher Farbe.
    void sourceDistinguishesVoiceFromQrm()
    {
        SpectrumWidget w;
        w.setSignalHistoryMarkers({
            mark(14.205, QStringLiteral("SHistory"), QStringLiteral("S7")),
            mark(14.220, QStringLiteral("QRM"),      QStringLiteral("S9")),
        });
        const auto got = w.signalHistoryMarkersForTest();
        QCOMPARE(got.size(), 2);
        QCOMPARE(got[0].source, QStringLiteral("SHistory"));
        QCOMPARE(got[1].source, QStringLiteral("QRM"));
    }

    // Ausschalten wirft die Marken nicht weg: der naechste Durchlauf
    // des Erkenners tut das. Wichtig, weil sonst ein kurzes Aus- und
    // Einschalten die gesammelte Lage vernichten wuerde.
    void switchingOffKeepsTheMarkersForTheNextPass()
    {
        SpectrumWidget w;
        w.setShowSignalHistory(true);
        w.setSignalHistoryMarkers({mark(14.205, QStringLiteral("SHistory"),
                                        QStringLiteral("S7"))});
        w.setShowSignalHistory(false);
        QCOMPARE(w.signalHistoryMarkersForTest().size(), 1);
    }

    // ── Die 3-kHz-Regel ──────────────────────────────────────────────
    //
    // Liegt binnen 3 kHz ein echter DX-Spot, weicht die Verlaufsmarke:
    // der Spot traegt ein Rufzeichen, die Marke nur eine S-Stufe. Ohne
    // diese Regel staenden zwei Etiketten uebereinander, und das
    // wertvollere waere verdeckt.

    void aSpotSuppressesAMarkerWithinThreeKilohertz()
    {
        SpectrumWidget w;
        w.setShowSpots(true);
        w.setShowSignalHistory(true);
        w.setSpotMarkers({mark(14.205000, QStringLiteral("DXCluster"),
                               QStringLiteral("DL1ABC"))});
        w.setSignalHistoryMarkers({mark(14.207000, QStringLiteral("SHistory"),
                                        QStringLiteral("S7"))});  // 2 kHz daneben

        const auto merged = w.mergedMarkersForTest();
        QCOMPARE(merged.size(), 1);
        QCOMPARE(merged.first().callsign, QStringLiteral("DL1ABC"));
    }

    void aMarkerFurtherAwayThanThreeKilohertzSurvives()
    {
        SpectrumWidget w;
        w.setShowSpots(true);
        w.setShowSignalHistory(true);
        w.setSpotMarkers({mark(14.205000, QStringLiteral("DXCluster"),
                               QStringLiteral("DL1ABC"))});
        w.setSignalHistoryMarkers({mark(14.209000, QStringLiteral("SHistory"),
                                        QStringLiteral("S7"))});  // 4 kHz daneben

        QCOMPARE(w.mergedMarkersForTest().size(), 2);
    }

    // Die Regel gilt in beide Richtungen der Frequenzachse — ein
    // vertauschtes Vorzeichen faellt sonst nur auf einer Seite auf.
    void theRuleWorksBelowTheSpotToo()
    {
        SpectrumWidget w;
        w.setShowSpots(true);
        w.setShowSignalHistory(true);
        w.setSpotMarkers({mark(14.205000, QStringLiteral("DXCluster"),
                               QStringLiteral("DL1ABC"))});
        w.setSignalHistoryMarkers({mark(14.203000, QStringLiteral("SHistory"),
                                        QStringLiteral("S7"))});
        QCOMPARE(w.mergedMarkersForTest().size(), 1);
    }

    // Ausgeschaltete Sorten kommen gar nicht erst in die Zeichnung.
    void gatingDecidesWhatIsDrawn()
    {
        SpectrumWidget w;
        w.setShowSpots(false);
        w.setSignalHistoryMarkers({
            mark(14.205, QStringLiteral("SHistory"), QStringLiteral("S7")),
            mark(14.220, QStringLiteral("QRM"),      QStringLiteral("S9")),
        });

        QVERIFY2(w.mergedMarkersForTest().isEmpty(),
                 "beide Schalter aus: nichts wird gezeichnet");

        w.setShowSignalHistory(true);
        QCOMPARE(w.mergedMarkersForTest().size(), 1);
        QCOMPARE(w.mergedMarkersForTest().first().source,
                 QStringLiteral("SHistory"));

        w.setShowSignalHistoryQrm(true);
        QCOMPARE(w.mergedMarkersForTest().size(), 2);

        w.setShowSignalHistory(false);
        QCOMPARE(w.mergedMarkersForTest().size(), 1);
        QCOMPARE(w.mergedMarkersForTest().first().source,
                 QStringLiteral("QRM"));
    }

    // Ein unterdrueckter Spot verschwindet nicht aus dem Speicher — nur
    // aus dem Bild. Sonst waere die Marke nach dem Ablaufen des Spots
    // fuer immer weg.
    void suppressionDoesNotDeleteTheMarker()
    {
        SpectrumWidget w;
        w.setShowSpots(true);
        w.setShowSignalHistory(true);
        w.setSpotMarkers({mark(14.205, QStringLiteral("DXCluster"),
                               QStringLiteral("DL1ABC"))});
        w.setSignalHistoryMarkers({mark(14.206, QStringLiteral("SHistory"),
                                        QStringLiteral("S7"))});
        QCOMPARE(w.mergedMarkersForTest().size(), 1);
        QCOMPARE(w.signalHistoryMarkersForTest().size(), 1);

        w.setSpotMarkers({});   // Spot laeuft ab
        QCOMPARE(w.mergedMarkersForTest().size(), 1);
        QCOMPARE(w.mergedMarkersForTest().first().callsign,
                 QStringLiteral("S7"));
    }

    // ── Die zwei Schwellen ───────────────────────────────────────────
    //
    // Sie lagen bis 2026-08-19 nur im Speicher und wurden von niemandem
    // gesetzt — gebaut, an keiner Flaeche. Jetzt gibt es zwei Felder im
    // Setup, und diese Faelle halten fest, dass der Weg durchgeht und die
    // Grenzen dabei greifen.

    void thresholdsStartAtTheirDefaults()
    {
        SpectrumWidget w;
        QCOMPARE(w.signalHistoryQrmGateSeconds(), 6);
        QCOMPARE(w.signalHistoryLifetimeSeconds(), 60);
    }

    void thresholdsGoThroughToTheStore()
    {
        SpectrumWidget w;
        w.setSignalHistoryQrmGateSeconds(12);
        w.setSignalHistoryLifetimeSeconds(180);
        QCOMPARE(w.signalHistoryQrmGateSeconds(), 12);
        QCOMPARE(w.signalHistoryLifetimeSeconds(), 180);
    }

    // Die Grenzen liegen in SignalHistoryStore, nicht in der
    // Bedienflaeche — sonst umgeht sie ein zweiter Weg.
    void thresholdsStayInsideTheirLimits()
    {
        SpectrumWidget w;
        w.setSignalHistoryQrmGateSeconds(1);
        QCOMPARE(w.signalHistoryQrmGateSeconds(), 3);
        w.setSignalHistoryQrmGateSeconds(999);
        QCOMPARE(w.signalHistoryQrmGateSeconds(), 30);

        w.setSignalHistoryLifetimeSeconds(2);
        QCOMPARE(w.signalHistoryLifetimeSeconds(), 15);
        w.setSignalHistoryLifetimeSeconds(9999);
        QCOMPARE(w.signalHistoryLifetimeSeconds(), 300);
    }
};

QTEST_MAIN(TestSignalHistoryMarkers)
#include "tst_signal_history_markers.moc"
