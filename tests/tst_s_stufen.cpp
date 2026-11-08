// no-port-check: Longpath-original test, no Thetis logic.
//
// ── S-Stufen mit VHF-Bezug und Transverter-Verstaerkung (2026-11-08) ──────
//
//   1. Der Port aus Thetis common.cs (GetSMeterUnits, die Tabelle von
//      SMeterFromDBM_Spaceless) und die Grenze aus MeterManager.cs.
//   2. Die RX-Verstaerkung des Transverters landet -- wie Thetis'
//      RXCalibrationOffset -- im Pegelversatz von RadioModel, nur solange
//      der VFO im ZF-Fenster steht, und meldet sich per Signal.
//   3. Die S-Striche am Panadapter: KW, 2 m direkt, 2 m ueber den
//      Transverter (28,300 heisst 144,300), jeweils mit Kalibrierversatz.
//   4. Die Instrumente (Zeiger, Balken, S-Text): ueber der Grenze rueckt
//      die LESUNG um 20 dB (Thetis getPerc), die Striche bleiben. Die
//      Kennung fuehrt RadioModel an der Antennenfrequenz nach.

#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QImage>
#include <QPainter>

#include "core/AppSettings.h"
#include "core/CompanionRigController.h"
#include "core/SMeterSkala.h"
#include "core/TransverterSetup.h"
#include "gui/SpectrumWidget.h"
#include "gui/instruments/NeedleInstrument.h"
#include "gui/instruments/ReadingSource.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {
TransverterSetup zweiMeter(double gainDb)
{
    TransverterSetup t;
    t.enabled  = true;
    t.ifLowHz  = 28000000;
    t.ifHighHz = 32000000;
    t.offsetHz = 116000000;
    t.rxGainDb = gainDb;
    return t;
}
float strich(const SpectrumWidget& w, const QString& name)
{
    for (const auto& s : w.sUnitTicksForTest()) {
        if (s.first == name) { return s.second; }
    }
    return 1.0e9f;
}
} // namespace

class TstSStufen : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        AppSettings::instance().clear();
        TransverterSetup::setCurrent({});
        SMeter::setzeUeberGrenze(false);
    }
    void cleanup()
    {
        AppSettings::instance().clear();
        TransverterSetup::setCurrent({});
        SMeter::setzeUeberGrenze(false);
    }

    // ── 1. Der Port ──────────────────────────────────────────────────
    void derPortRechnetWieThetis()
    {
        QCOMPARE(SMeter::einheiten(-73.0, false), 9.0);
        QCOMPARE(SMeter::einheiten(-93.0, true), 9.0);
        QVERIFY(std::abs(SMeter::einheiten(-53.0, false) - (9.0 + 20.0 / 6.0)) < 1e-9);
        QCOMPARE(SMeter::dbmFuer(9.0, false), -73.0);
        QCOMPARE(SMeter::dbmFuer(9.0, true), -93.0);
        for (double d : {-140.0, -101.5, -73.0, -20.0}) {
            QVERIFY(std::abs(SMeter::dbmFuer(SMeter::einheiten(d, true), true) - d) < 1e-9);
        }
        // Die Tabelle, Grenzen wortgetreu (> unten & <= oben)
        QCOMPARE(SMeter::text(-73.0, false), QStringLiteral("S9"));
        QCOMPARE(SMeter::text(-93.0, true), QStringLiteral("S9"));
        QCOMPARE(SMeter::text(-73.0, true), QStringLiteral("S9+20"));
        QCOMPARE(SMeter::text(-130.0, true), QStringLiteral("S3"));
        QCOMPARE(SMeter::text(-124.0, false), QStringLiteral("S0"));
        QCOMPARE(SMeter::text(-123.9, false), QStringLiteral("S1"));
        QCOMPARE(SMeter::text(-10.0, false), QStringLiteral("S9+60"));
        // Die Grenze
        QVERIFY(SMeter::ueberGrenze(144.3));
        QVERIFY(!SMeter::ueberGrenze(28.3));
        QVERIFY(SMeter::ueberGrenze(30.0));
        QVERIFY(!SMeter::ueberGrenze(143.0, 144.0));
    }

    // ── 2. Die RX-Verstaerkung im Pegelversatz ───────────────────────
    void dieVerstaerkungGiltNurImFenster()
    {
        TransverterSetup t = zweiMeter(22.0);
        QCOMPARE(t.pegelVersatzDb(28300000), -22.0);
        QCOMPARE(t.pegelVersatzDb(14200000), 0.0);
        t.enabled = false;
        QCOMPARE(t.pegelVersatzDb(28300000), 0.0);
        // Speichern und Laden
        zweiMeter(18.5).save();
        QCOMPARE(TransverterSetup::load().rxGainDb, 18.5);
    }

    void radioModelZiehtSieAbUndMeldetEs()
    {
        RadioModel radio;
        const int idx = radio.addSlice();
        radio.setActiveSlice(idx);
        SliceModel* slice = radio.activeSlice();
        QVERIFY(slice);
        slice->setFrequency(28300000.0);
        const double ohne = radio.rxMeterOffsetDb();

        QSignalSpy geaendert(&radio, &RadioModel::rxMeterOffsetChanged);
        // Wie die Einstellseite: speichern, dann der Controller laedt neu
        // und meldet versatzGeaendert.
        zweiMeter(22.0).save();
        radio.companionRig()->einstellungenGeaendert();
        QVERIFY(std::abs(radio.rxMeterOffsetDb() - (ohne - 22.0)) < 1e-6);
        QVERIFY2(!geaendert.isEmpty(), "die Einstellung meldete keinen neuen Versatz");
        // TCI fragt denselben Wert ab (calibration_ex, Thetis TCIServer.cs).
        QCOMPARE(radio.calibrationXvtr(0), -22.0);
        QCOMPARE(radio.calibrationXvtr(1), 0.0);

        // Aus dem Fenster getunt: der Versatz faellt weg, und es meldet sich.
        geaendert.clear();
        slice->setFrequency(14200000.0);
        QVERIFY(std::abs(radio.rxMeterOffsetDb() - ohne) < 1e-6);
        QVERIFY2(!geaendert.isEmpty(), "ueber die Fenstergrenze getunt, aber kein Signal");
    }

    // ── 3. Die S-Striche am Panadapter ──────────────────────────────
    void dieStricheStehenAmRichtigenPegel()
    {
        SpectrumWidget w;
        w.loadSettings();
        QVERIFY2(!w.sUnitsScaleShown(), "ab Werk dBm");
        w.setSUnitsScaleShown(true);
        w.setDbmCalOffset(10.0f);

        // KW: S9 = -73 dBm an der Antenne -> Rohwert -83 bei +10 dB Versatz
        w.setVfoFrequency(14200000.0);
        QCOMPARE(strich(w, QStringLiteral("S9")), -83.0f);
        QCOMPARE(strich(w, QStringLiteral("+20")), -63.0f);

        // 2 m direkt: S9 = -93
        w.setVfoFrequency(144300000.0);
        QCOMPARE(strich(w, QStringLiteral("S9")), -103.0f);

        // 2 m ueber den Transverter: das Geraet steht auf 28,300, die
        // Antenne auf 144,300 -- es gilt der VHF-Bezug.
        TransverterSetup::setCurrent(zweiMeter(0.0));
        w.setVfoFrequency(28300000.0);
        QCOMPARE(strich(w, QStringLiteral("S9")), -103.0f);

        // Speichern
        w.saveSettings();
        SpectrumWidget w2;
        w2.loadSettings();
        QVERIFY(w2.sUnitsScaleShown());
    }

    // ── 4. Die Instrumente ──────────────────────────────────────────
    void dieKennungFolgtDerAntenne()
    {
        RadioModel radio;
        const int idx = radio.addSlice();
        radio.setActiveSlice(idx);
        SliceModel* slice = radio.activeSlice();
        QVERIFY(slice);
        slice->setFrequency(144300000.0);
        QVERIFY2(SMeter::istUeberGrenze(), "2 m direkt, aber KW-Bezug");
        QCOMPARE(SMeter::versatzUeberGrenze(), 20.0f);
        slice->setFrequency(14200000.0);
        QVERIFY(!SMeter::istUeberGrenze());
        QCOMPARE(SMeter::versatzUeberGrenze(), 0.0f);
        // Ueber den Transverter: das Geraet steht auf 28,300.
        slice->setFrequency(28300000.0);
        QVERIFY(!SMeter::istUeberGrenze());
        zweiMeter(0.0).save();
        radio.companionRig()->einstellungenGeaendert();
        QVERIFY2(SMeter::istUeberGrenze(), "Transverter an, 144,300 an der Antenne");
    }

    void dieLesungRuecktDieStricheNicht()
    {
        const ReadingDescriptor* d = readingFor(MeterBinding::SignalAvg);
        QVERIFY(d);
        const double s9 = d->fraction(kS9Dbm);
        QVERIFY(std::abs(s9 - kS9Fraction) < 1e-9);
        QCOMPARE(d->text(-93.0), QStringLiteral("S6"));
        QVERIFY(d->valueFraction(-93.0) < s9 - 0.05);

        SMeter::setzeUeberGrenze(true);
        QVERIFY2(std::abs(d->valueFraction(-93.0) - s9) < 1e-9, "-93 dBm steht nicht auf S9");
        QCOMPARE(d->text(-93.0), QStringLiteral("S9"));
        QCOMPARE(d->text(-73.0), QStringLiteral("S9+20"));
        QVERIFY2(std::abs(d->fraction(kS9Dbm) - s9) < 1e-9, "der S9-Strich ist mitgewandert");
        // Andere Groessen ruecken nicht.
        const ReadingDescriptor* swr = readingFor(MeterBinding::TxSwr);
        QVERIFY(swr);
        QCOMPARE(swr->valueFraction(2.0), swr->fraction(2.0));
        // Max Bin ist dieselbe Groesse.
        const ReadingDescriptor* mb = readingFor(MeterBinding::SignalMaxBin);
        QVERIFY(mb);
        QVERIFY(std::abs(mb->valueFraction(-93.0) - s9) < 1e-9);
    }

    // Das gezeichnete Zifferblatt: -93 dBm auf 2 m sieht aus wie -73 auf KW.
    void derZeigerStehtAufS9()
    {
        auto bild = [](bool ueber, double dbm) {
            SMeter::setzeUeberGrenze(ueber);
            NeedleInstrument n;
            n.resize(360, 200);
            n.setPrimary(MeterBinding::SignalAvg);
            n.onReading(MeterBinding::SignalAvg, dbm);
            QImage img(360, 200, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::black);
            QPainter p(&img);
            n.paintInto(p, img.size(), true);
            p.end();
            return img;
        };
        const QImage kw  = bild(false, -73.0);
        const QImage vhf = bild(true,  -93.0);
        const QImage alt = bild(false, -93.0);
        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!dir.isEmpty()) {
            kw.save(dir + QStringLiteral("/s9-kw-73dBm.png"));
            vhf.save(dir + QStringLiteral("/s9-2m-93dBm.png"));
            alt.save(dir + QStringLiteral("/vorher-2m-93dBm.png"));
        }
        QVERIFY2(kw == vhf, "S9 auf 2 m zeichnet anders als S9 auf KW");
        QVERIFY2(kw != alt, "der Vergleich unterscheidet nichts");
    }

    void dieContainerMeterRueckenMit()
    {
        BarItem bar;
        bar.setBindingId(MeterBinding::SignalAvg);
        bar.addScaleCalibration(-133.0, 0.0f);
        bar.addScaleCalibration(-73.0, 0.5f);
        bar.addScaleCalibration(-13.0, 0.99f);
        BarItem fremd;
        fremd.setBindingId(MeterBinding::TxPower);
        fremd.addScaleCalibration(0.0, 0.0f);
        fremd.addScaleCalibration(100.0, 1.0f);
        const float fremdVorher = fremd.valueToNormalizedX(50.0);

        QVERIFY(bar.valueToNormalizedX(-93.0) < 0.45f);
        QCOMPARE(MeterItem::formatValue(-93.0f, MeterItem::MeterUnit::S, true,
                                        bar.sVersatz()), QStringLiteral("S6"));
        SMeter::setzeUeberGrenze(true);
        QCOMPARE(bar.valueToNormalizedX(-93.0), 0.5f);
        QCOMPARE(bar.sVersatz(), 20.0f);
        QCOMPARE(MeterItem::formatValue(-93.0f, MeterItem::MeterUnit::S, true,
                                        bar.sVersatz()), QStringLiteral("S9"));
        // Die dBm-Zahl bleibt die Messung.
        QCOMPARE(MeterItem::formatValue(-93.0f, MeterItem::MeterUnit::dBm, false,
                                        bar.sVersatz()), QStringLiteral("-93"));
        QCOMPARE(fremd.sVersatz(), 0.0f);
        QCOMPARE(fremd.valueToNormalizedX(50.0), fremdVorher);
    }
};

QTEST_MAIN(TstSStufen)
#include "tst_s_stufen.moc"
