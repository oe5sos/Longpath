// no-port-check: Longpath test file; the rule comes from upstream NereusSDR
// (419dd88c4 / dd800ce9b / 89e41cd91), the cited Thetis lines are in
// SliceModel.cpp.
// =================================================================
// tests/tst_rate_schritt_je_scheibe.cpp  (Longpath)
// =================================================================
//
// Ein Abstimmschritt und eine Abtastrate je Scheibe, auf jedem Band.
//
// Martin am 2026-11-11: „Ja, umstellen" -- wie Upstream NereusSDR am
// 2026-11-05 (J.J. Boyd, KG4VCF): vorher wurden Schritt und Rate je Band
// gespeichert, und jeder Bandwechsel holte den alten Wert des Bandes
// zurueck. Die Rate gilt zusaetzlich je Geraet, damit eine Anvelina bei
// 1536 kHz und ein ANAN bei 192 kHz sich nicht ueberschreiben.
//
// Gegen die alte Fassung (Schritt und Rate je Band gespeichert und beim
// Wiederherstellen gelesen) fallen alle sechs Faelle um, am 2026-11-11
// gefahren.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11 — Neu. Martin Fischer (OE5SOS), KI-gestuetzt mit
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "models/Band.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

const QString kMacA = QStringLiteral("aa:bb:cc:00:00:01");
const QString kMacB = QStringLiteral("aa:bb:cc:00:00:02");

void aufraeumen()
{
    auto& s = AppSettings::instance();
    for (const QString& band : {QStringLiteral("40m"), QStringLiteral("20m"),
                                QStringLiteral("15m")}) {
        const QString bp = QStringLiteral("Slice0/Band") + band + QStringLiteral("/");
        for (const QString& feld : {QStringLiteral("Frequency"), QStringLiteral("DspMode"),
                                    QStringLiteral("StepHz"), QStringLiteral("SampleRate")}) {
            s.remove(bp + feld);
        }
    }
    s.remove(QStringLiteral("Slice0/StepHz"));
    s.remove(QStringLiteral("Slice0/LastBand"));
    for (const QString& mac : {kMacA, kMacB}) {
        s.remove(QStringLiteral("hardware/%1/slices/0/SampleRate").arg(mac));
    }
}

} // namespace

class TstRateSchrittJeScheibe : public QObject {
    Q_OBJECT

private slots:
    void init() { aufraeumen(); }
    void cleanup() { aufraeumen(); }

    // Fall 1: der Schritt wird je Scheibe gespeichert, nicht je Band.
    void schrittIstJeScheibe()
    {
        SliceModel sl;
        sl.setSliceIndex(0);
        sl.setStepHz(500);
        sl.saveToSettings(Band::Band20m);

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("Slice0/StepHz")).toInt(), 500);
        QVERIFY(!s.contains(QStringLiteral("Slice0/Band20m/StepHz")));
    }

    // Fall 2: ein Bandwechsel laesst den Schritt stehen, auch wenn das Ziel
    // noch einen alten Schritt je Band liegen hat.
    void bandwechselLaesstSchrittStehen()
    {
        auto& s = AppSettings::instance();
        SliceModel sl;
        sl.setSliceIndex(0);
        sl.setStepHz(1000);
        sl.saveToSettings(Band::Band40m);
        s.setValue(QStringLiteral("Slice0/Band20m/StepHz"), 50);   // alt, je Band

        sl.setStepHz(2500);            // neuer, noch nicht gespeichert
        sl.restoreFromSettingsForBandChange(Band::Band20m);
        QCOMPARE(sl.stepHz(), 2500);
    }

    // Fall 3: die Rate wird je Scheibe und Geraet gespeichert, und ein
    // Wiederherstellen holt keine alte Rate je Band zurueck.
    void rateIstJeScheibeUndGeraet()
    {
        auto& s = AppSettings::instance();
        SliceModel sl;
        sl.setSliceIndex(0);
        sl.setSettingsRadioMac(kMacA);
        sl.setSampleRateHz(1536000);
        sl.saveToSettings(Band::Band20m);

        QCOMPARE(s.value(QStringLiteral("hardware/%1/slices/0/SampleRate").arg(kMacA)).toInt(),
                 1536000);
        QVERIFY(!s.contains(QStringLiteral("Slice0/Band20m/SampleRate")));

        s.setValue(QStringLiteral("Slice0/Band40m/SampleRate"), 48000);   // alt, je Band
        s.setValue(QStringLiteral("Slice0/Band40m/DspMode"), 0);
        sl.restoreFromSettingsForBandChange(Band::Band40m);
        QCOMPARE(sl.sampleRateHz(), 1536000);
        QCOMPARE(sl.savedSampleRateHz(), 1536000);
    }

    // Fall 4: zwei Geraete ueberschreiben sich nicht.
    void zweiGeraeteBehaltenIhreRate()
    {
        SliceModel sl;
        sl.setSliceIndex(0);
        sl.setSettingsRadioMac(kMacA);
        sl.setSampleRateHz(1536000);
        sl.saveToSettings(Band::Band20m);
        sl.setSettingsRadioMac(kMacB);
        sl.setSampleRateHz(192000);
        sl.saveToSettings(Band::Band20m);

        sl.setSettingsRadioMac(kMacA);
        QCOMPARE(sl.savedSampleRateHz(), 1536000);
        sl.setSettingsRadioMac(kMacB);
        QCOMPARE(sl.savedSampleRateHz(), 192000);
    }

    // Fall 5: aeltere Einstellungen -- Schritt und Rate des Bandes, auf dem
    // die Scheibe zuletzt stand.
    void aeltereEinstellungenGeltenWeiter()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Slice0/LastBand"), QStringLiteral("40m"));
        s.setValue(QStringLiteral("Slice0/Band40m/StepHz"), 250);
        s.setValue(QStringLiteral("Slice0/Band40m/SampleRate"), 384000);
        s.setValue(QStringLiteral("Slice0/Band20m/DspMode"), 0);

        SliceModel sl;
        sl.setSliceIndex(0);
        sl.setSettingsRadioMac(kMacA);
        sl.restoreFromSettings(Band::Band20m);   // der Start, kein Bandwechsel
        QCOMPARE(sl.stepHz(), 250);
        QCOMPARE(sl.savedSampleRateHz(), 384000);
    }

    // Fall 6: ohne bekanntes Geraet wird keine Rate gespeichert.
    void ohneGeraetKeineRate()
    {
        SliceModel sl;
        sl.setSliceIndex(0);
        sl.setSampleRateHz(768000);
        sl.saveToSettings(Band::Band20m);
        QCOMPARE(sl.savedSampleRateHz(), 0);
    }
};

QTEST_MAIN(TstRateSchrittJeScheibe)
#include "tst_rate_schritt_je_scheibe.moc"
