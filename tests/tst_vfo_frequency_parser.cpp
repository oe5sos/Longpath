// Regression test fuer den Frequenz-Eingabeparser.
//
// Deckt Fehlerbericht #73 ab: europaeisch getrennte Eingabe
// („7.230.000") war zeitweise zehnmal zu klein, schlichte
// Dezimaleingabe („7.23") sprang auf die Klemmgrenze 61,44 MHz.
//
// 2026-08-18: der Parser stand als statische Methode auf VfoWidget.
// Mit deren Loeschung ist er nach FrequencyInstrument gezogen — die
// Flaeche, die das Abstimmen jetzt traegt. Dort rief commitEdit() bis
// dahin ein blosses toDouble() mit der Annahme MHz; „7.230.000" waere
// still verworfen worden (zwei Punkte) und „7230" als 7230 MHz
// gelandet. Also #73 noch einmal, an einer neuen Stelle.
//
// Dieser Test ist der Grund, warum es aufgefallen ist: er nennt seine
// Faelle, und die Faelle passten nicht mehr zu dem, was die Anwendung
// tat.

#include <QtTest/QtTest>
#include "gui/instruments/FrequencyInstrument.h"

using NereusSDR::FrequencyInstrument;

class TestVfoFrequencyParser : public QObject {
    Q_OBJECT
private slots:
    // --- The issue #73 reproducer cases ------------------------------------

    void euThousandSeparatedIsSevenTwentyThreeMHz() {
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7.230.000")), 7230000.0);
    }
    void mhzDecimalDoesNotHitCeiling() {
        // Previously "7.23" was stripped to "723", treated as Hz, then clamped
        // up to 61.44 MHz. Must now be interpreted as 7.23 MHz.
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7.23")), 7230000.0);
    }
    void mhzDecimalWithTrailingZero() {
        // Previously "7.230" was stripped to "7230", treated as Hz, then
        // clamped to the 100 kHz floor.
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7.230")), 7230000.0);
    }

    // --- Plain-integer range inference -------------------------------------

    void plainInRangeMHz() {
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("14")), 14000000.0);
    }
    void plainInRangeKHz() {
        // 7230 MHz is out of range → falls through to kHz (7.23 MHz).
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7230")), 7230000.0);
    }
    void plainInRangeHz() {
        // 7230000 MHz / kHz are out of range → falls through to Hz (7.23 MHz).
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7230000")), 7230000.0);
    }

    // --- European decimal and US thousand separators ----------------------

    void euDecimalComma() {
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7,23")), 7230000.0);
    }
    void usThousandSeparated() {
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7,230,000")), 7230000.0);
    }
    void mixedThousandsAndDecimal() {
        // US thousand-sep with decimal: "7,230,000.00" → 7230000.00 Hz
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7,230,000.00")), 7230000.0);
    }

    // --- Unit suffixes ----------------------------------------------------

    void explicitMHz()       { QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7.23 MHz")), 7230000.0); }
    void explicitKHz()       { QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7230 kHz")), 7230000.0); }
    void explicitHz()        { QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7230000 Hz")), 7230000.0); }
    void shortSuffixM()      { QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7.23M")), 7230000.0); }
    void shortSuffixK()      { QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7230k")), 7230000.0); }
    void caseInsensitiveMhz(){ QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7.23mhz")), 7230000.0); }
    void multiDotWithUnit() {
        // "7.230.000 Hz" — dots are thousand seps, unit wins.
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7.230.000 Hz")), 7230000.0);
    }
    void singleCommaWithUnitIsThousands() {
        // "7,230 kHz" — single comma + explicit unit + 3-digit tail reads
        // as US thousands, so this is 7,230 kHz = 7.23 MHz, not 7.23 kHz.
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7,230 kHz")), 7230000.0);
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7,230 Hz")), 7230.0);
    }
    void singleCommaWithUnitIsDecimal() {
        // "7,23 MHz" — single comma + explicit unit + 2-digit tail reads
        // as EU decimal: 7.23 MHz.
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7,23 MHz")), 7230000.0);
    }

    // --- Existing display-format round-trip -------------------------------

    void preFilledMhzDecimal() {
        // Die Eingabezeile wird vorbelegt via QString::number(mhz,'f',6).
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("14.225000")), 14225000.0);
    }
    void labelFormatRoundTrip() {
        // The label format "14.225.000" must also parse correctly if users
        // retype from what they see.
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("14.225.000")), 14225000.0);
    }

    // --- Failure cases ----------------------------------------------------

    void emptyStringFails()     { QCOMPARE(FrequencyInstrument::parseUserFrequency(QString()), -1.0); }
    void whitespaceOnlyFails()  { QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("   ")), -1.0); }
    void garbageFails()         { QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("abc")), -1.0); }
    void unitOnlyFails()        { QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("MHz")), -1.0); }
    void negativeFails()        { QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("-7.23")), -1.0); }

    // --- Clamp behavior (at caller level, parser should not clamp) --------

    void parserDoesNotClamp() {
        // 1 GHz — parser returns 1e9 unchanged; the caller is responsible
        // for clamping to the Red Pitaya range.
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("1000 MHz")), 1e9);
    }
    void parserReturnsBelowFloor() {
        // 50 Hz — parser returns 50 unchanged; caller will clamp to 100 kHz.
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("50 Hz")), 50.0);
    }
};

QTEST_MAIN(TestVfoFrequencyParser)
#include "tst_vfo_frequency_parser.moc"
