// tests/tst_signal_reading.cpp  (NereusSDR)
//
// NereusSDR-original. No Thetis port.
//
// Ohne Verbindung stand an drei Stellen "-395 dBm" -- Zifferblatt,
// Zahlenfeld, VFO-Leiste. Das ist kein schwaches Signal, das ist gar
// keins: zweihundert Dezibel unter dem thermischen Rauschen jedes
// Empfaengers.
//
// HAUSSTIL Regel 7: "Unbekannt ist ein Strich, keine Null. Eine Null
// sieht aus wie eine Messung." Eine Zahl auch, und -395 sogar wie eine
// sehr genaue.
//
// Geprueft wird die BEDINGUNG, nicht das Bild: ab wann gilt ein Pegel
// als Messung. Absichtlich nicht, wie der Strich aussieht -- das ist
// Gestaltung und gehoert dem Betreiber.

#include <QtTest/QtTest>

#include "gui/widgets/SignalReading.h"

#include <limits>

using namespace NereusSDR;

class TestSignalReading : public QObject
{
    Q_OBJECT

private slots:

    void ordinaryLevelsAreMeasurements()
    {
        QVERIFY(SignalReading::isMeasurement(-73.0f));    // S9
        QVERIFY(SignalReading::isMeasurement(-127.0f));   // sehr leise
        QVERIFY(SignalReading::isMeasurement(-150.0f));   // stiller Empfaenger
        QVERIFY(SignalReading::isMeasurement(0.0f));
    }

    void theSentinelIsNot()
    {
        QVERIFY(!SignalReading::isMeasurement(-395.0f));
    }

    void theThresholdCatchesTheImpossibleNotTheUnlikely()
    {
        // -180 ist unwahrscheinlich, aber nicht unmoeglich -- die Zahl
        // soll der Betreiber sehen. Die Schwelle faengt das ab, was
        // physikalisch nicht sein kann.
        QVERIFY(SignalReading::isMeasurement(-180.0f));
        QVERIFY(!SignalReading::isMeasurement(-250.0f));
    }

    void nonsenseIsNotAMeasurement()
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float inf = std::numeric_limits<float>::infinity();
        QVERIFY(!SignalReading::isMeasurement(nan));
        QVERIFY(!SignalReading::isMeasurement(-inf));
        // Auch plus unendlich nicht: ein Pegel ohne Obergrenze ist
        // genauso wenig gemessen wie einer ohne Untergrenze.
        QVERIFY(!SignalReading::isMeasurement(inf));
    }

    void theTextCarriesNoUnitWhenThereIsNoReading()
    {
        // "-- dBm" waere eine halbe Behauptung: es gibt keine Dezibel,
        // wenn es keine Messung gibt.
        const QString t = SignalReading::text(-395.0f);
        QCOMPARE(t, SignalReading::noReadingText());
        QVERIFY2(!t.contains(QStringLiteral("dBm")),
                 "der Strich traegt eine Einheit");
    }

    void theTextIsRoundedAndCarriesTheUnitWhenThereIs()
    {
        QCOMPARE(SignalReading::text(-73.4f), QStringLiteral("-73 dBm"));
        QCOMPARE(SignalReading::text(-72.5f, QStringLiteral("dB")),
                 QStringLiteral("-72 dB"));
    }
};

QTEST_MAIN(TestSignalReading)
#include "tst_signal_reading.moc"
