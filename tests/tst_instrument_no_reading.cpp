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

using namespace NereusSDR;

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
};

QTEST_MAIN(TestInstrumentNoReading)
#include "tst_instrument_no_reading.moc"
