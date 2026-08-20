// =================================================================
// tests/tst_instrument_no_reading.cpp  (NereusSDR)
// =================================================================
//
// Ein Instrument ohne Messung darf keine Zahl zeigen.
//
// Befund des Betreibers am 2026-08-17, beim ersten Blick auf den
// Schirm: die Stehwelle stand ohne Verbindung auf 1.00, Spitze 1.00,
// und der Zeiger lag am linken Anschlag.
//
// Das ist nicht bloss unschön. 1,00 ist ein hervorragendes SWR — ein
// Instrument, das ohne jede Messung ein hervorragendes Ergebnis
// anzeigt, lügt in die beruhigende Richtung. Bei einer Anzeige, an der
// man abliest, ob man senden darf, ist das die falsche Richtung.
//
// Geprüft wird der Zustand, nicht das Bild: dass vor der ersten
// Messung nichts vorliegt, dass die erste Messung ihn beendet, und
// dass clearValue() ihn wiederherstellt. Wie das Zifferblatt dann
// AUSSIEHT, entscheidet der Blick des Betreibers.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QtTest>

#include "gui/instruments/BarInstrument.h"
#include "gui/instruments/NeedleInstrument.h"
#include "gui/instruments/ReadingSource.h"
#include "gui/meters/MeterPoller.h"

using namespace Longpath;

class TestInstrumentNoReading : public QObject
{
    Q_OBJECT

private slots:

    void needleHasNoValueBeforeTheFirstReading()
    {
        NeedleInstrument n;
        QVERIFY(n.setPrimary(MeterBinding::TxSwr));
        QVERIFY2(!n.hasValue(),
                 "eine zugewiesene Groesse ist noch keine Messung");
    }

    void barHasNoValueBeforeTheFirstReading()
    {
        BarInstrument b;
        QVERIFY(b.setPrimary(MeterBinding::TxSwr));
        QVERIFY(!b.hasValue());
    }

    void theFirstReadingEndsIt()
    {
        NeedleInstrument n;
        QVERIFY(n.setPrimary(MeterBinding::TxSwr));
        n.onReading(MeterBinding::TxSwr, 1.42);
        QVERIFY(n.hasValue());
    }

    // Eine Messung fuer eine ANDERE Groesse darf den Zustand nicht
    // beenden — sonst zeigte die Stehwelle etwas an, sobald irgendwo
    // im Programm irgendein Messwert umlaeuft.
    void areadingForAnotherQuantityDoesNotCount()
    {
        NeedleInstrument n;
        QVERIFY(n.setPrimary(MeterBinding::TxSwr));
        n.onReading(MeterBinding::TxPower, 42.0);
        n.onReading(MeterBinding::SignalAvg, -73.0);
        QVERIFY2(!n.hasValue(),
                 "eine fremde Groesse hat den Zustand beendet");
    }

    void clearValueBringsItBack()
    {
        NeedleInstrument n;
        QVERIFY(n.setPrimary(MeterBinding::TxSwr));
        n.onReading(MeterBinding::TxSwr, 2.1);
        QVERIFY(n.hasValue());
        n.clearValue();
        QVERIFY2(!n.hasValue(),
                 "clearValue muss den Zustand wiederherstellen — es ist "
                 "der Weg fuer 'Funkgeraet getrennt'");
    }

    // Die Groesse zu wechseln heisst, von vorn anzufangen: was fuer
    // die alte gemessen wurde, gilt fuer die neue nicht.
    void changingTheQuantityStartsOver()
    {
        NeedleInstrument n;
        QVERIFY(n.setPrimary(MeterBinding::TxSwr));
        n.onReading(MeterBinding::TxSwr, 2.1);
        QVERIFY(n.hasValue());
        QVERIFY(n.setPrimary(MeterBinding::TxPower));
        QVERIFY(!n.hasValue());
    }

    // Eine Groesse ohne belegte Skala wird abgelehnt, und der Zustand
    // bleibt, wie er war — die Ablehnung darf kein halbes Instrument
    // hinterlassen.
    void anUnscaledQuantityIsRefusedWithoutSideEffects()
    {
        NeedleInstrument n;
        QVERIFY(n.setPrimary(MeterBinding::TxSwr));
        n.onReading(MeterBinding::TxSwr, 2.1);
        QVERIFY(n.hasValue());

        QVERIFY2(!n.setPrimary(MeterBinding::HwTemperature),
                 "Temperatur hat keine belegte Skala und darf nicht "
                 "angenommen werden");
        QCOMPARE(n.primary(), MeterBinding::TxSwr);
        QVERIFY2(n.hasValue(),
                 "die abgelehnte Zuweisung hat die laufende Messung "
                 "verworfen");
    }

    // Beide Formen muessen dasselbe sagen — sonst springt beim
    // Umschalten nicht nur der Blick, sondern die Aussage.
    void bothFormsAgree()
    {
        NeedleInstrument n;
        BarInstrument    b;
        QVERIFY(n.setPrimary(MeterBinding::TxSwr));
        QVERIFY(b.setPrimary(MeterBinding::TxSwr));
        QCOMPARE(n.hasValue(), b.hasValue());

        n.onReading(MeterBinding::TxSwr, 1.8);
        b.onReading(MeterBinding::TxSwr, 1.8);
        QCOMPARE(n.hasValue(), b.hasValue());

        n.clearValue();
        b.clearValue();
        QCOMPARE(n.hasValue(), b.hasValue());
    }

    // ── Zwei Arten von „kein Messwert" ───────────────────────────────
    //
    // Der Betreiber hat gemeldet, die Stehwelle habe keinen Zeiger. Sie
    // hatte auch keinen Messwert — die Regel griff also richtig, und
    // trotzdem sah es kaputt aus. Die Entscheidung vom 2026-08-18
    // trennt darum die beiden Faelle:
    //
    //   ohne Radio          -> Zeiger ruht am Anschlag, matt
    //   mit Radio, kein Wert -> kein Zeiger (er behauptete eine Messung)
    //
    // Geprueft wird der ZUSTAND, nicht das Bild: ein Farbvergleich auf
    // dem gemalten Bogen haenge an Deckkraft und Rundung und braeche
    // beim naechsten Feinschliff, ohne dass die Regel verletzt waere.
    void offlineIsNotTheSameAsNoReading()
    {
        NeedleInstrument n;
        QVERIFY(n.setPrimary(MeterBinding::TxSwr));

        // Vorgabe: verbunden gedacht, also kein Zeiger ohne Wert.
        QVERIFY(!n.isOffline());
        QVERIFY(!n.hasValue());

        n.setOffline(true);
        QVERIFY(n.isOffline());
        QVERIFY2(!n.hasValue(),
                 "Ruhelage ist kein Messwert — sie darf hasValue() nicht setzen");

        // Kommt ein Wert herein, gewinnt er: dann ist der Zeiger echt.
        n.onReading(MeterBinding::TxSwr, 2.1);
        QVERIFY(n.hasValue());
        QVERIFY(n.isOffline());   // die Verbindung sagt das Applet, nicht der Wert

        n.setOffline(false);
        QVERIFY(!n.isOffline());
    }

    // Zweimal derselbe Zustand darf nicht zweimal neu zeichnen. Bei
    // einem Instrument, das an einem 1-Hz-Takt haengt, ist das der
    // Unterschied zwischen ruhig und flackernd.
    void settingTheSameOfflineStateTwiceIsIdempotent()
    {
        NeedleInstrument n;
        QVERIFY(n.setPrimary(MeterBinding::TxSwr));
        n.setOffline(true);
        n.setOffline(true);
        QVERIFY(n.isOffline());
        n.setOffline(false);
        n.setOffline(false);
        QVERIFY(!n.isOffline());
    }
};

QTEST_MAIN(TestInstrumentNoReading)
#include "tst_instrument_no_reading.moc"
