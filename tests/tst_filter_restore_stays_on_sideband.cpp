// =================================================================
// tests/tst_filter_restore_stays_on_sideband.cpp  (Longpath)
// =================================================================
//
// Ein gespeicherter Durchlass, der bei LSB/USB ueber den Traeger reicht,
// kommt NICHT wieder.
//
// Der Betreiber am 2026-09-17: "hört sich auf 40 meter katastrophal an"
// — und nach der Behebung des Bandfilter-Fehlers: "wieder das gleiche".
// In seinen Einstellungen stand fuer 40 m LSB ein Durchlass von
// -100 … +2900 Hz (Slice0/Band40m/ModeLSB/FilterLow = -100, FilterHigh =
// 2900), entstanden aus dem LOW/WIDTH-Fehler des Bandfilters. Der
// Fehler war behoben, aber der gespeicherte Wert kam bei jedem Start
// und jedem Bandwechsel zurueck.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-17 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest/QtTest>

#include "models/SliceModel.h"
#include "models/Band.h"
#include "core/AppSettings.h"

using namespace Longpath;

class TestFilterRestoreStaysOnSideband : public QObject {
    Q_OBJECT

private:
    static void clearKeys()
    {
        auto& s = AppSettings::instance();
        for (const QString& k : {
                 QStringLiteral("Slice0/Band40m/ModeLSB/FilterLow"),
                 QStringLiteral("Slice0/Band40m/ModeLSB/FilterHigh"),
                 QStringLiteral("Slice0/Band40m/ModeUSB/FilterLow"),
                 QStringLiteral("Slice0/Band40m/ModeUSB/FilterHigh"),
                 QStringLiteral("Slice0/Band40m/FilterLow"),
                 QStringLiteral("Slice0/Band40m/FilterHigh"),
                 QStringLiteral("Slice0/Band40m/DspMode"),
                 QStringLiteral("Slice0/Band40m/Frequency") }) {
            s.remove(k);
        }
    }

private slots:
    void init()    { clearKeys(); }
    void cleanup() { clearKeys(); }

    void crossingIsOnlyAQuestionForSingleSidebandModes()
    {
        QVERIFY( SliceModel::filterCrossesCarrier(-100,  2900, DSPMode::LSB));
        QVERIFY(!SliceModel::filterCrossesCarrier(-3100, -100, DSPMode::LSB));
        QVERIFY( SliceModel::filterCrossesCarrier(-100,  2900, DSPMode::USB));
        QVERIFY(!SliceModel::filterCrossesCarrier(100,   3100, DSPMode::USB));
        QVERIFY( SliceModel::filterCrossesCarrier(-850,   350, DSPMode::CWL));
        QVERIFY(!SliceModel::filterCrossesCarrier(-850,  -350, DSPMode::CWL));
        // Zweiseitig: der Traeger liegt absichtlich in der Mitte.
        QVERIFY(!SliceModel::filterCrossesCarrier(-4000, 4000, DSPMode::AM));
        QVERIFY(!SliceModel::filterCrossesCarrier(-8000, 8000, DSPMode::FM));
    }

    void aPersistedLsbFilterAcrossTheCarrierComesBackAsTheDefault()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Slice0/Band40m/DspMode"),
                   static_cast<int>(DSPMode::LSB));
        s.setValue(QStringLiteral("Slice0/Band40m/ModeLSB/FilterLow"),  -100);
        s.setValue(QStringLiteral("Slice0/Band40m/ModeLSB/FilterHigh"), 2900);

        SliceModel slice;
        slice.setSliceIndex(0);
        slice.setFrequency(7'090'000.0);
        slice.restoreFromSettings(Band::Band40m);

        QCOMPARE(slice.dspMode(), DSPMode::LSB);
        QVERIFY2(slice.filterHigh() <= 0 && slice.filterLow() < slice.filterHigh(),
                 qPrintable(QStringLiteral(
                     "40 m LSB kam mit %1 … %2 zurueck — ueber den Traeger, "
                     "genau das 'wieder das gleiche' des Betreibers")
                     .arg(slice.filterLow()).arg(slice.filterHigh())));
        const auto def = SliceModel::defaultFilterForMode(DSPMode::LSB);
        QCOMPARE(slice.filterLow(),  def.first);
        QCOMPARE(slice.filterHigh(), def.second);
    }

    void aSanePersistedLsbFilterIsLeftAlone()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Slice0/Band40m/DspMode"),
                   static_cast<int>(DSPMode::LSB));
        s.setValue(QStringLiteral("Slice0/Band40m/ModeLSB/FilterLow"),  -3100);
        s.setValue(QStringLiteral("Slice0/Band40m/ModeLSB/FilterHigh"), -100);

        SliceModel slice;
        slice.setSliceIndex(0);
        slice.setFrequency(7'090'000.0);
        slice.restoreFromSettings(Band::Band40m);

        QCOMPARE(slice.filterLow(),  -3100);
        QCOMPARE(slice.filterHigh(), -100);
    }

    void switchingModeDoesNotRestoreACrossingFilterEither()
    {
        // Derselbe Wert, aber ueber den zweiten Weg: der Betriebsart-
        // wechsel holt den je (Band, Betriebsart) gespeicherten Durchlass.
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Slice0/Band40m/ModeLSB/FilterLow"),  -100);
        s.setValue(QStringLiteral("Slice0/Band40m/ModeLSB/FilterHigh"), 2900);

        SliceModel slice;
        slice.setSliceIndex(0);
        slice.setFrequency(7'090'000.0);
        slice.setDspMode(DSPMode::USB);
        slice.setDspMode(DSPMode::LSB);

        QVERIFY2(slice.filterHigh() <= 0,
                 qPrintable(QStringLiteral("nach dem Wechsel auf LSB: %1 … %2")
                     .arg(slice.filterLow()).arg(slice.filterHigh())));
    }
};

QTEST_MAIN(TestFilterRestoreStaysOnSideband)
#include "tst_filter_restore_stays_on_sideband.moc"
