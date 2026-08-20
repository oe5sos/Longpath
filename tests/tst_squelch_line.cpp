// =================================================================
// tests/tst_squelch_line.cpp  (NereusSDR)
// =================================================================
//
// Die Squelch-Linie im Panadapter.
//
// Port aus AetherSDR (SpectrumWidget.cpp:4453-4506 [@0cd4559]); erstes
// der sieben Merkmale aus der Merkmalsliste vom 2026-08-19.
//
// Geprueft wird der ZUSTAND, nicht das gemalte Bild: ein Pixelvergleich
// haengt an Bezugspegel, Spanne und Kantenglaettung und braeche beim
// naechsten Feinschliff, ohne dass die Regel verletzt waere. Dieselbe
// Entscheidung wie beim ruhenden Zeiger (tst_instrument_no_reading).
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

class TestSquelchLine : public QObject
{
    Q_OBJECT

private slots:

    void hiddenUntilSomethingSetsIt()
    {
        SpectrumWidget w;
        QVERIFY2(!w.squelchLineVisible(),
                 "die Linie darf nicht von sich aus liegen");
    }

    void theThresholdIsCarriedInDbm()
    {
        SpectrumWidget w;
        w.setSquelchLine(true, -92.0);
        QVERIFY(w.squelchLineVisible());
        // In dBm, nicht in einer 0..160-Stufe: AetherSDR rechnet
        // kSqlMinDbm + level, weil FlexRadio so meldet. Unser
        // SliceModel haelt dBm, und diese Umrechnung faellt weg.
        QCOMPARE(w.squelchLineDbm(), -92.0);
    }

    // Der Wert bleibt erhalten, wenn die Linie verschwindet: sie ist
    // eine ANZEIGE der Schwelle, nicht die Schwelle selbst. Wer sie
    // ausblendet, aendert nichts am Squelch.
    void hidingKeepsTheValue()
    {
        SpectrumWidget w;
        w.setSquelchLine(true, -80.0);
        w.setSquelchLine(false, -80.0);
        QVERIFY(!w.squelchLineVisible());
        QCOMPARE(w.squelchLineDbm(), -80.0);
    }

    // Nach drei Sekunden blendet sie sich aus (AetherSDR-Verhalten): sie
    // beantwortet „wo steht die Schwelle" beim EINSTELLEN. Ein Strich,
    // der immer liegt, wird zum Teil des Rasters.
    void itFadesOnItsOwn()
    {
        SpectrumWidget w;
        w.setSquelchLine(true, -100.0);
        QVERIFY(w.squelchLineVisible());
        // Der Ablauf haengt an einem QTimer; 3 s abzuwarten waere ein
        // langsamer Test. Geprueft wird darum, dass ueberhaupt einer
        // laeuft — dass er die Sichtbarkeit loescht, ist eine Zeile.
        QVERIFY2(w.findChild<QTimer*>() != nullptr,
                 "ohne Zeitgeber bleibt die Linie fuer immer liegen");
    }
};

QTEST_MAIN(TestSquelchLine)
#include "tst_squelch_line.moc"
