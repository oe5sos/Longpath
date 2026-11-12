// no-port-check: Longpath-original test; the ported formulas are cited in
// src/core/FrequenzEichung.h and src/core/TransverterSetup.h.
//
// ── Einmessen an einem Traeger, KW und UKW (2026-11-08) ─────────────
//
//   1. TransverterSetup: der LO-Fehler (Thetis xvtr.cs) in beiden
//      Richtungen, Einmessen, Speichern.
//   2. FrequenzEichung: Traegersuche mit Parabel, Rauschschwelle, der
//      Korrekturfaktor nach Thetis' CalibrateFreq.
//   3. P1 rechnet den Korrekturfaktor ein (Thetis NetworkIO.cs VFOfreq)
//      -- vorher nur P2.
//   4. Der Ablauf am Panadapter: zehn Spektren, Treffer, Dialog, Ziel.

#include <QtTest/QtTest>
#include <QSignalSpy>

#include <cmath>

#include "core/AppSettings.h"
#include "core/CalibrationController.h"
#include "core/CompanionRigController.h"
#include "core/FrequenzEichung.h"
#include "core/P1RadioConnection.h"
#include "core/TransverterSetup.h"
#include "gui/PanadapterApplet.h"
#include "gui/SpectrumWidget.h"
#include "gui/TraegerEichDialog.h"
#include "models/RadioModel.h"

#include <QMenu>

using namespace Longpath;

namespace {
TransverterSetup zweiMeter(double loErrKhz = 0.0)
{
    TransverterSetup t;
    t.enabled  = true;
    t.ifLowHz  = 28000000;
    t.ifHighHz = 32000000;
    t.offsetHz = 116000000;
    t.loErrorKhz = loErrKhz;
    return t;
}
// Ein gaussfoermiger Traeger: im dB-Bild eine Parabel, also trifft die
// Interpolation ihn exakt -- der Test misst die Rechnung, nicht das Fenster.
QVector<double> traeger(int n, double binHz, double fftUnten, double hz,
                        double spitze = 1.0e-9, double boden = 1.0e-13)
{
    QVector<double> v(n, boden);
    const double k0 = (hz - fftUnten) / binHz - 0.5;
    for (int k = 0; k < n; ++k) {
        const double d = (k - k0) / 1.2;
        v[k] += spitze * std::exp(-d * d);
    }
    return v;
}
} // namespace

class TstFrequenzEichung : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        AppSettings::instance().clear();
        TransverterSetup::setCurrent({});
    }
    void cleanup()
    {
        AppSettings::instance().clear();
        TransverterSetup::setCurrent({});
    }

    // ── 1. LO-Fehler ─────────────────────────────────────────────────
    void derLoFehlerWirktInBeideRichtungen()
    {
        const TransverterSetup ohne = zweiMeter();
        QCOMPARE(ohne.rfFrequencyHz(28400000), qint64(144400000));

        // LO 312 Hz zu hoch: eine Bake auf 144,400 erscheint 312 Hz tiefer.
        // Thetis: ZF = RF - LO + Fehler, also Fehler = -0,312 kHz.
        const TransverterSetup t = zweiMeter(-0.312);
        QCOMPARE(t.loErrorHz(), qint64(-312));
        QCOMPARE(t.ifFrequencyHz(144400000), qint64(28400000 - 312));
        QCOMPARE(t.rfFrequencyHz(28400000 - 312), qint64(144400000));
        for (qint64 rf : {144050000LL, 144300000LL, 145999000LL}) {
            QCOMPARE(t.rfFrequencyHz(t.ifFrequencyHz(rf)), rf);
        }
        // Einmessen: angezeigt 144,399688, wahr 144,400000.
        QVERIFY(std::abs(ohne.loErrorFuer(144399688, 144400000) - (-0.312)) < 1e-9);
        // Nochmal messen mit gesetztem Fehler: es bleibt dabei.
        QVERIFY(std::abs(t.loErrorFuer(144400000, 144400000) - (-0.312)) < 1e-9);
        // Speichern und Laden, mit Klemme wie Thetis (+/-1000 kHz).
        zweiMeter(-0.312).save();
        QCOMPARE(TransverterSetup::load().loErrorKhz, -0.312);
        zweiMeter(5000.0).save();
        QCOMPARE(TransverterSetup::load().loErrorKhz, TransverterSetup::kLoErrorGrenzeKhz);
        QVERIFY(zweiMeter(-0.312).describe().contains(QStringLiteral("LO error -312 Hz")));
        QVERIFY(!zweiMeter().describe().contains(QStringLiteral("LO error")));
    }

    // ── 2. Traegersuche und Faktor ──────────────────────────────────
    void derTraegerWirdAufDasHertzGefunden()
    {
        const int n = 4096;
        const double binHz = 192000.0 / n;          // 46,875 Hz je Bin
        const double unten = 10000000.0 - 96000.0;
        const QVector<double> v = traeger(n, binHz, unten, 10000017.0);
        const FrequenzEichung::Treffer t =
            FrequenzEichung::sucheTraeger(v, binHz, unten, 10000500.0);
        QVERIFY(t.gefunden);
        QVERIFY2(std::abs(t.hz - 10000017.0) < 1.0,
                 qPrintable(QString::number(t.hz - 10000017.0)));
        QVERIFY(t.snrDb > 30.0f);
        // Ausserhalb von +/-2,5 kHz: nicht gefunden.
        QVERIFY(!FrequenzEichung::sucheTraeger(v, binHz, unten, 10004000.0).gefunden);
        // Reines Rauschen: nicht gefunden.
        QVector<double> rauschen(n, 1.0e-13);
        for (int k = 0; k < n; ++k) { rauschen[k] *= 1.0 + 0.5 * ((k * 7) % 5) / 5.0; }
        QVERIFY(!FrequenzEichung::sucheTraeger(rauschen, binHz, unten, 10000000.0).gefunden);
    }

    void derFaktorFolgtThetis()
    {
        // Thetis: correct_factor = 1 - diff / freq, diff = wahr - gemessen.
        // Erscheint 10 MHz um 10 Hz zu hoch, muss die NCO 1 ppm hoeher.
        QVERIFY(std::abs(FrequenzEichung::neuerFaktor(1.0, 10000010.0, 10000000.0)
                         - 1.000001) < 1e-12);
        // Auf den geltenden Faktor angewandt.
        QVERIFY(std::abs(FrequenzEichung::neuerFaktor(0.999998, 10000000.0, 10000000.0)
                         - 0.999998) < 1e-15);
        QVERIFY(std::abs(FrequenzEichung::ppm(10000010.0, 10000000.0) - 1.0) < 1e-9);
    }

    // ── 3. P1 rechnet den Faktor ein ────────────────────────────────
    void p1KorrigiertWieThetis()
    {
        P1RadioConnection p1;
        p1.setBoardForTest(HPSDRHW::HermesII);
        p1.setReceiverFrequency(0, 10000000);
        p1.setTxFrequency(10000000);
        CalibrationController cal;
        p1.setCalibrationController(&cal);
        QCOMPARE(p1.buildCodecContextForTest().rxFreqHz[0], quint64(10000000));
        cal.setFreqCorrectionFactor(1.000001);
        const CodecContext ctx = p1.buildCodecContextForTest();
        QCOMPARE(ctx.rxFreqHz[0], quint64(10000010));
        QCOMPARE(ctx.txFreqHz, quint64(10000010));
    }

    // ── 4. Der Ablauf am Panadapter ─────────────────────────────────
    void zehnSpektrenDannEinTreffer()
    {
        const int n = 4096;
        const double rate = 192000.0;
        const double mitte = 144400000.0 - 116000000.0;   // ZF 28,400
        SpectrumWidget w;
        w.resize(1000, 520);
        w.setSampleRate(rate);
        w.setDdcCenterFrequency(mitte);
        w.setFrequencyRange(mitte, rate);
        TransverterSetup::setCurrent(zweiMeter());

        const double binHz = rate / n;
        const double unten = mitte - rate / 2.0;
        const QVector<double> d = traeger(n, binHz, unten, mitte - 312.0);
        QVector<float> b(n);
        for (int k = 0; k < n; ++k) { b[k] = static_cast<float>(d[k]); }

        w.updateSpectrumLinear(0, b, 1.0, 0.0);      // erst Daten, dann messen
        QSignalSpy spy(&w, &SpectrumWidget::traegerGemessen);
        QVERIFY(w.starteTraegerMessung(mitte + 1000.0));
        for (int i = 0; i < FrequenzEichung::kMittelBilder - 1; ++i) {
            w.updateSpectrumLinear(0, b, 1.0, 0.0);
        }
        QVERIFY2(spy.isEmpty(), "vor dem zehnten Spektrum gemeldet");
        w.updateSpectrumLinear(0, b, 1.0, 0.0);
        QCOMPARE(spy.size(), 1);
        const QList<QVariant> a = spy.first();
        QVERIFY(a.at(0).toBool());
        QVERIFY(std::abs(a.at(1).toDouble() - (mitte - 312.0)) < 1.0);
        QVERIFY(std::abs(a.at(2).toDouble() - (144400000.0 - 312.0)) < 1.0);
        QVERIFY(!w.traegerMessungLaeuft());
    }

    void derDialogStelltDenTransverter()
    {
        RadioModel radio;
        zweiMeter().save();
        radio.companionRig()->einstellungenGeaendert();
        const double geraet = 28400000.0 - 312.0;
        QCOMPARE(radio.eichZielFuer(geraet), RadioModel::EichZiel::Transverter);
        TraegerEichDialog d(&radio, true, geraet, 144400000.0 - 312.0, 30.0f);
        QVERIFY(d.zielTextForTest().contains(QStringLiteral("Transverter LO error")));
        QVERIFY(d.abweichungTextForTest().startsWith(QStringLiteral("-312 Hz")));
        QVERIFY(d.uebernehmenMoeglichForTest());
        d.setWahrHzForTest(144400000.0);
        QVERIFY(d.uebernehmenForTest());
        QCOMPARE(TransverterSetup::load().loErrorKhz, -0.312);
        // Danach zeigt die ZF des Traegers die wahre Frequenz.
        QCOMPARE(TransverterSetup::current().rfFrequencyHz(qRound64(geraet)),
                 qint64(144400000));
    }

    void ohneEichbaresGeraetNurAnzeige()
    {
        RadioModel radio;                       // keine Verbindung
        QCOMPARE(radio.eichZielFuer(10000000.0), RadioModel::EichZiel::NurAnzeige);
        TraegerEichDialog d(&radio, true, 10000010.0, 10000010.0, 30.0f);
        QVERIFY(!d.uebernehmenMoeglichForTest());
        QVERIFY(d.zielTextForTest().contains(QStringLiteral("can't be set")));
        QVERIFY(!radio.uebernimmEichung(10000010.0, 10000010.0, 10000000.0));
    }

    void derMenuepunktSitztImPanadapter()
    {
        PanadapterApplet pan(QStringLiteral("pan-0"));
        QMenu* m = pan.buildContextMenuForTesting();
        QAction* eich = nullptr;
        for (QAction* a : m->actions()) {
            if (a->text().startsWith(QStringLiteral("Calibrate frequency on this carrier"))) {
                eich = a;
            }
        }
        QVERIFY2(eich, "kein Menuepunkt zum Einmessen");
        QVERIFY2(!eich->isEnabled(), "ohne Klickstelle darf er nicht greifen");
        delete m;
    }
};

QTEST_MAIN(TstFrequenzEichung)
#include "tst_frequenz_eichung.moc"
