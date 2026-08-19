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

using namespace NereusSDR;

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
};

QTEST_MAIN(TestSignalHistoryMarkers)
#include "tst_signal_history_markers.moc"
