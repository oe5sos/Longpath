// =================================================================
// tests/tst_auto_agc_beim_senden.cpp  (Longpath)
// =================================================================
//
// Auto AGC-T darf nicht nach dem eigenen Sendesignal nachstellen.
//
// Befund 2026-11-10 (Abgleich mit Upstream NereusSDR 2026.11.1, dort
// Issue #720 „laut nach dem Zurueckschalten"): bei uns war die
// Sendesperre des AUTO-Takts tot. Sie fragte TransmitModel::isMox(),
// und dieser Riegel wurde mit MoxController nie gesetzt
// (RadioModel::setMox geht nur an den Controller). Mit ihm tot waren
// auch Clarity-Sendepause, Kiwi-Stummschaltung, OC-Anzeige und der
// Ganymede-PA-Schutz.
//
// Dieser Pruefstand haelt fest:
//   1. der Riegel folgt dem Controller (gegen die alte Fassung: rot);
//   2. der Messabschnitt ist gesperrt ohne Verbindung, bei MOX und auf
//      dem ganzen Weg hinein und heraus, und forkIqToTaps stempelt ihn;
//   3. ein Bild, in dessen Fenster noch Sendeproben liegen, geht nie an
//      den Rauschbodenmesser (FFTEngine::autoAgcFrameReady);
//   4. der Messer haelt waehrend des Sendens seinen Wert und nimmt
//      nichts aus einem fremden Abschnitt an;
//   5. der AUTO-Takt stellt AGC-T beim Senden nicht nach.
//
// Messabschnitte uebernommen aus Upstream NereusSDR e5b3997bc (#720,
// J.J. Boyd KG4VCF). Martin Fischer (OE5SOS), KI-gestuetzt mit
// Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/FFTEngine.h"
#include "core/MoxController.h"
#include "core/NoiseFloorTracker.h"
#include "core/RadioConnection.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace Longpath;

namespace {

class VerbundeneAttrappe : public RadioConnection {
    Q_OBJECT
public:
    explicit VerbundeneAttrappe(QObject* parent = nullptr) : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }
    void init() override {}
    void connectToRadio(const Longpath::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

void pump()
{
    for (int i = 0; i < 4; ++i) {
        QCoreApplication::processEvents();
    }
}

QVector<float> block(int pairs, float value)
{
    QVector<float> v;
    v.reserve(pairs * 2);
    for (int i = 0; i < pairs; ++i) { v << value << value; }
    return v;
}

QVector<float> bins(int n, float dbm)
{
    return QVector<float>(n, dbm);
}

// Ein Modell mit sofortigem MoxController und einer Scheibe auf 80 m.
void bereitmachen(RadioModel& model)
{
    model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
    model.addSlice();
    SliceModel* slice = model.activeSlice();
    QVERIFY(slice != nullptr);
    slice->setFrequency(3700000.0);
    slice->setDspMode(DSPMode::LSB);
}

} // namespace

class TstAutoAgcBeimSenden : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── 1. Der Riegel folgt dem Controller ────────────────────────────
    void riegelFolgtDemController()
    {
        RadioModel model;
        bereitmachen(model);
        QSignalSpy riegel(&model.transmitModel(), &TransmitModel::moxChanged);

        model.moxController()->setMox(true);
        pump();
        QVERIFY2(model.moxController()->isMox(), "MOX kam nicht");
        QVERIFY2(model.transmitModel().isMox(),
                 "TransmitModel::isMox() bleibt false, obwohl gesendet wird -- "
                 "alles, was daran haengt, ist tot");
        QVERIFY(riegel.count() >= 1);
        QCOMPARE(riegel.last().at(0).toBool(), true);

        model.moxController()->setMox(false);
        pump();
        QVERIFY(!model.moxController()->isMox());
        QVERIFY(!model.transmitModel().isMox());
        QCOMPARE(riegel.last().at(0).toBool(), false);
    }

    // ── 1b. Ganymede-PA-Schutz tastet wirklich aus ────────────────────
    void ganymedeAusloesungTastetAus()
    {
        RadioModel model;
        bereitmachen(model);
        model.moxController()->setMox(true);
        pump();
        QVERIFY(model.mox());

        model.handleGanymedeTrip(0x01);
        pump();
        QVERIFY(model.paTripped());
        QVERIFY2(!model.mox(),
                 "Ganymede-Ausloesung liess den Sender getastet "
                 "(Andromeda.cs:920: if (_ganymede_pa_issue && MOX) MOX = false)");
        QVERIFY(!model.transmitModel().isMox());
    }

    // ── 2. Der Messabschnitt ──────────────────────────────────────────
    void ohneVerbindungGesperrt()
    {
        RadioModel model;
        QVERIFY(model.autoAgcAcquisitionInhibited());
    }

    void sendenSperrtUndForkStempelt()
    {
        RadioModel model;
        bereitmachen(model);
        auto* verbindung = new VerbundeneAttrappe();
        model.injectConnectionForTest(verbindung);

        // Der AUTO-Takt zieht den Abschnitt nach; ohne Signal hier direkt
        // ueber einen Zustandswechsel des Controllers anstossen.
        model.moxController()->setMox(true);
        pump();
        QVERIFY2(model.autoAgcAcquisitionInhibited(),
                 "Beim Senden muss der Messabschnitt gesperrt sein");
        const quint64 beimSenden = model.autoAgcAcquisitionEpoch();
        QVERIFY((beimSenden & 1U) != 0);

        QSignalSpy gestempelt(&model, &RadioModel::rawIqDataForAutoAgc);
        model.forkIqToTapsForTest(0, block(8, 0.1f));
        QCOMPARE(gestempelt.count(), 1);
        QCOMPARE(gestempelt.at(0).at(2).value<quint64>(), beimSenden);

        model.moxController()->setMox(false);
        pump();
        QVERIFY2(!model.autoAgcAcquisitionInhibited(),
                 "Nach dem Senden (Controller wieder in Rx) muss empfangen werden");
        const quint64 danach = model.autoAgcAcquisitionEpoch();
        QVERIFY(danach > beimSenden);
        QVERIFY((danach & 1U) == 0);

        model.forkIqToTapsForTest(0, block(8, 0.1f));
        QCOMPARE(gestempelt.count(), 2);
        QCOMPARE(gestempelt.at(1).at(2).value<quint64>(), danach);

        model.injectConnectionForTest(nullptr);
        delete verbindung;
    }

    // ── 3. FFTEngine: nur ganze, saubere Fenster ──────────────────────
    void mischfensterGehtNieAnDenMesser()
    {
        FFTEngine engine(0);
        engine.setFftSize(1024);
        engine.setOutputFps(60);
        qint64 uhr = 0;
        engine.setFrameGapClockForTest([&uhr] { return uhr; });
        QSignalSpy alle(&engine, &FFTEngine::fftReady);
        QSignalSpy auto_(&engine, &FFTEngine::autoAgcFrameReady);

        // Empfang (Abschnitt 2): ein volles Fenster -> ein AUTO-Bild.
        engine.feedIQForAutoAgc(block(1024 + 16, 0.1f), 2);
        QVERIFY(alle.count() >= 1);
        QCOMPARE(auto_.count(), alle.count());
        QCOMPARE(auto_.last().at(2).value<quint64>(), quint64(2));
        const int nachEmpfang = auto_.count();

        // Senden (Abschnitt 3): Bilder fuer die Anzeige ja, fuer AUTO nie.
        for (int i = 0; i < 4; ++i) {
            uhr += 20;
            engine.feedIQForAutoAgc(block(1024, 5.0f), 3);
        }
        QVERIFY(alle.count() > nachEmpfang);
        QCOMPARE(auto_.count(), nachEmpfang);

        // Wieder Empfang (Abschnitt 4): das erste Fenster ist gemischt --
        // die Ueberlappung traegt noch Sendeproben. Erst ein ganzes
        // sauberes Fenster darf an den Messer.
        const int anzeigeVorher = alle.count();
        uhr += 20;
        engine.feedIQForAutoAgc(block(64, 0.1f), 4);
        uhr += 20;
        engine.feedIQForAutoAgc(block(64, 0.1f), 4);
        QVERIFY(alle.count() > anzeigeVorher);
        QCOMPARE(auto_.count(), nachEmpfang);

        for (int i = 0; i < 4; ++i) {
            uhr += 20;
            engine.feedIQForAutoAgc(block(1024, 0.1f), 4);
        }
        QVERIFY2(auto_.count() > nachEmpfang,
                 "Nach einem ganzen sauberen Fenster muss AUTO wieder Bilder bekommen");
        QCOMPARE(auto_.last().at(2).value<quint64>(), quint64(4));
    }

    void feedIqOhneStempelZaehltAlsGesperrt()
    {
        FFTEngine engine(0);
        engine.setFftSize(1024);
        engine.setOutputFps(60);
        QSignalSpy alle(&engine, &FFTEngine::fftReady);
        QSignalSpy auto_(&engine, &FFTEngine::autoAgcFrameReady);
        engine.feedIQ(block(1024 + 16, 0.1f));
        QVERIFY(alle.count() >= 1);
        QCOMPARE(auto_.count(), 0);
    }

    // ── 4. Der Messer haelt und nimmt nichts Fremdes an ───────────────
    void messerHaeltBeimSenden()
    {
        NoiseFloorTracker nf;
        nf.setAttackTimeMs(100.0f);
        nf.setAcquisitionEpoch(2);
        for (int i = 0; i < 10; ++i) {
            nf.feedForEpoch(bins(64, -120.0f), 33.0f, 2);
        }
        QVERIFY(nf.isGood());
        QVERIFY(nf.acquisitionCurrent());
        const float vorher = nf.noiseFloor();

        nf.setAcquisitionEpoch(3);  // Senden
        QVERIFY(!nf.acquisitionCurrent());
        nf.feed(bins(64, -40.0f), 33.0f);                // gesperrt
        nf.feedForEpoch(bins(64, -40.0f), 33.0f, 3);     // gesperrt
        QCOMPARE(nf.noiseFloor(), vorher);

        nf.setAcquisitionEpoch(4);  // wieder Empfang
        nf.feedForEpoch(bins(64, -40.0f), 33.0f, 3);     // verspaetet, fremd
        QCOMPARE(nf.noiseFloor(), vorher);
        QVERIFY(!nf.acquisitionCurrent());
        QVERIFY2(nf.isGood(), "Der Wert von vor dem Senden bleibt gueltig");

        nf.feedForEpoch(bins(64, -120.0f), 33.0f, 4);
        QVERIFY(nf.acquisitionCurrent());
    }

    // ── 5. Der AUTO-Takt stellt beim Senden nichts nach ───────────────
    void autoTaktStehtBeimSenden()
    {
        RadioModel model;
        bereitmachen(model);
        auto* verbindung = new VerbundeneAttrappe();
        model.injectConnectionForTest(verbindung);

        // Der AUTO-Takt entsteht in wireSliceSignals, das ohne Verbindung
        // aussteigt -- darum erst die Verbindung, dann verdrahten.
        model.wireSliceSignalsForTest();

        NoiseFloorTracker nf;
        nf.setAttackTimeMs(33.0f);
        model.setNoiseFloorTracker(&nf);
        QTimer* takt = model.autoAgcTimer();
        QVERIFY2(takt != nullptr, "Kein AUTO-Takt");

        SliceModel* slice = model.activeSlice();
        slice->setAutoAgcEnabled(true);
        auto tick = [takt]() {
            QVERIFY(QMetaObject::invokeMethod(takt, "timeout", Qt::DirectConnection));
        };

        // Empfang: der erste Takt zieht den Abschnitt nach (verbunden,
        // Rx), dann misst der Messer -120 dBm und AUTO stellt danach ein.
        tick();
        QVERIFY2(!model.autoAgcAcquisitionInhibited(),
                 "Verbunden und in Rx muss empfangen werden");
        for (int i = 0; i < 5; ++i) {
            nf.feedForEpoch(bins(64, -120.0f), 33.0f, model.autoAgcAcquisitionEpoch());
        }
        QVERIFY(nf.isGood());
        tick();
        const int agcEmpfang = slice->agcThreshold();

        // Senden. Jetzt kommen Bilder mit dem eigenen Sendesignal (-30 dBm)
        // -- ueber beide Wege, gestempelt und ungestempelt. AGC-T darf
        // sich nicht bewegen.
        model.moxController()->setMox(true);
        pump();
        QVERIFY(model.autoAgcAcquisitionInhibited());
        for (int i = 0; i < 5; ++i) {
            nf.feedForEpoch(bins(64, -30.0f), 33.0f, model.autoAgcAcquisitionEpoch());
            nf.feed(bins(64, -30.0f), 33.0f);
        }
        tick();
        QVERIFY2(slice->agcThreshold() == agcEmpfang,
                 qPrintable(QStringLiteral("AUTO hat beim Senden AGC-T von %1 auf %2 verstellt")
                                .arg(agcEmpfang).arg(slice->agcThreshold())));

        model.setNoiseFloorTracker(nullptr);
        model.moxController()->setMox(false);
        pump();
        model.injectConnectionForTest(nullptr);
        delete verbindung;
    }
};

QTEST_MAIN(TstAutoAgcBeimSenden)
#include "tst_auto_agc_beim_senden.moc"
