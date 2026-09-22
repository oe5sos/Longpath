// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_hpsdr_sim_workbench.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// WERKBANK, keine Pruefung im ueblichen Sinn: faehrt Longpaths eigenen
// Verbindungsweg gegen einen HPSDR-Simulator (hpsdrsim aus pihpsdr /
// deskhpsdr, GPL — als Messgeraet benutzt, nicht als Quelle) und sagt,
// was dabei herauskommt. Discovery-Antwort → RadioInfo → RadioModel::
// connectToRadio() → Empfangsstrom → Frequenz setzen → trennen.
//
// Laeuft NUR, wenn LONGPATH_HPSDRSIM gesetzt ist („host:port", z.B.
// 127.0.0.1:1024) — die CI hat keinen Simulator, und ein Test, der
// einen braucht und keinen findet, wuerde dort still scheitern. Mit
// LONGPATH_HPSDRSIM_LOG=<Datei> liest er hinterher das Protokoll des
// Simulators und zeigt, welche Steuerwerte dort ankamen (RX FREQ, Rate,
// PTT, HL2-Bits) — das ist der Teil, den kein Signal-Spy sieht.
//
// Stationen der Werkbank: Discovery → Verbinden → Empfangsstrom →
// Frequenz → S9-Referenzton (der Simulator legt -73 dBm auf 14,100 MHz)
// → Daempfungsglied/LNA (Stufe ueber den StepAttenuatorController wie
// im Hauptfenster) → Abtastrate → TUNE ein/aus (PTT, Vorwaertsleistung
// aus der Telemetrie, RadioStatus) → TUNE ueber den Tune-Regler
// (HL2-Sonderweg) → MOX mit PC-Mikrofon → zweiter Empfaenger → Trennen.
// Am Ende muss der Simulator PTT=0 gesehen haben — ein Traeger, der
// nach dem Trennen weiterlaeuft, ist der eine Fehler, den kein
// Hardware-Test verzeiht.
//
// Mit LONGPATH_HPSDRSIM_NO_TX=1 bleiben TUNE und MOX aus (fuer
// Simulator-Varianten ohne Sendezweig).

#include <QtTest>

#include "core/BoardCapabilities.h"
#include "core/ConnectionState.h"
#include "core/HpsdrModel.h"
#include "core/OcMatrix.h"
#include "core/PttSource.h"
#include "core/RadioDiscovery.h"
#include "core/RadioStatus.h"
#include "core/StepAttenuatorController.h"
#include "core/WdspEngine.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <QFile>
#include <QHostAddress>
#include <QRegularExpression>
#include <QSignalSpy>

#include <algorithm>

using namespace Longpath;

namespace {

// Mittelwert des S-Meters ueber ein paar Ablesungen: WDSP-Rohwert (dBm am
// Eingang) und der Wert, den die Anzeige zeigt (Rohwert + Offset aus
// Kalibrierung und Daempfungsglied, wie rxMeterOffsetDb() ihn liefert).
struct MeterReading { double rawDbm{-200.0}; double shownDbm{-200.0}; };

MeterReading readMeter(Longpath::RadioModel& model, int samples = 8)
{
    double raw = 0.0;
    for (int i = 0; i < samples; ++i) {
        QTest::qWait(120);
        raw += model.wdspEngine()->getRxaSignalAverage(0);
    }
    raw /= samples;
    return { raw, raw + model.rxMeterOffsetDb() };
}

} // namespace

class TstHpsdrSimWorkbench : public QObject { Q_OBJECT
private slots:
    void discover_connect_receive_tune_disconnect()
    {
        const QString target = qEnvironmentVariable("LONGPATH_HPSDRSIM");
        if (target.isEmpty()) {
            QSKIP("LONGPATH_HPSDRSIM nicht gesetzt — Werkbank, kein CI-Test.");
        }
        const QStringList hp = target.split(QLatin1Char(':'));
        const QHostAddress addr(hp.value(0, QStringLiteral("127.0.0.1")));
        const quint16 port = static_cast<quint16>(hp.value(1, QStringLiteral("1024")).toUInt());

        // ── 1. Discovery per Unicast-Probe ───────────────────────────
        RadioDiscovery disc;
        QSignalSpy found(&disc, &RadioDiscovery::radioDiscovered);
        QSignalSpy failed(&disc, &RadioDiscovery::probeFailed);
        disc.probeAddress(addr, port);
        QTRY_VERIFY_WITH_TIMEOUT(found.count() > 0 || failed.count() > 0, 8000);
        QVERIFY2(found.count() > 0, "Der Simulator hat auf die Probe nicht geantwortet");
        const RadioInfo info = found.first().first().value<RadioInfo>();
        qInfo() << "DISCOVERED" << info.displayName() << "board" << int(info.boardType)
                << "protocol" << int(info.protocol) << "fw" << info.firmwareVersion
                << "mac" << info.macAddress << "at" << info.address.toString();

        // ── 2. Verbinden ─────────────────────────────────────────────
        RadioModel model;
        // Das Daempfungsglied laeuft wie im Hauptfenster ueber den
        // StepAttenuatorController (MainWindow.cpp: Anlegen im Konstruktor,
        // Verbindung + Grenzen nach dem Verbinden).
        StepAttenuatorController att;
        model.setStepAttController(&att);
        QSignalSpy state(&model, &RadioModel::connectionStateChanged);
        QSignalSpy iq(&model, &RadioModel::rawIqData);
        model.connectToRadio(info);
        QTRY_VERIFY_WITH_TIMEOUT(model.connectionState() == ConnectionState::Connected
                                 || model.connectionState() == ConnectionState::Disconnected
                                    && state.count() > 1, 15000);
        qInfo() << "STATE" << int(model.connectionState()) << "transitions" << state.count();
        QVERIFY2(model.connectionState() == ConnectionState::Connected,
                 "Verbindung kam nicht zustande (siehe STATE oben)");
        att.setRadioConnection(model.connection());
        {
            const auto& caps = BoardCapsTable::forBoard(info.boardType);
            att.setMaxAttenuation(caps.attenuator.maxDb);
            att.setIsHpsdrBoard(info.boardType == HPSDRHW::Atlas);
            qInfo() << "ATT caps present" << caps.attenuator.present
                    << "min" << caps.attenuator.minDb << "max" << caps.attenuator.maxDb
                    << "controller dB" << att.attenuatorDb() << "stepAtt" << att.stepAttEnabled();
        }

        // ── 3. Empfangsstrom ─────────────────────────────────────────
        QTRY_VERIFY_WITH_TIMEOUT(iq.count() >= 20, 10000);
        qInfo() << "IQ blocks" << iq.count() << "slices" << model.slices().size()
                << "active" << (model.activeSlice() ? "yes" : "no")
                << "isConnected" << model.isConnected()
                << "rateHz" << model.connectionSampleRateHz();
        QVERIFY(model.activeSlice());

        // ── 4. Abstimmen ─────────────────────────────────────────────
        SliceModel* s = model.activeSlice();
        const double before = s->frequency();
        s->setFrequency(14'200'000.0);
        QTest::qWait(600);
        qInfo() << "FREQ before" << before << "after" << s->frequency()
                << "dspMode" << int(s->dspMode());
        QCOMPARE(int(s->frequency()), 14'200'000);

        // ── 4a. S9-Referenzton und Daempfungsglied ──────────────────
        // hpsdrsim legt einen -73-dBm-Traeger (S9) auf 14,100 MHz und
        // -150 dBm/Hz Rauschen darunter. Auf 14,200 MHz ist nur Rauschen:
        // das ist der Rauschflur in der Filterbreite. Dann der Ton — er
        // muss deutlich darueber liegen. Dann +10 dB Daempfung: der
        // Rohwert faellt um ~10 dB, die ANZEIGE (mit Offset) bleibt, so
        // wie Thetis es macht (RXOffset = Daempfung + Kalibrierung).
        const MeterReading floorAt142 = readMeter(model);
        // 1 kHz unter dem Ton: in USB liegt der Traeger dann mitten im
        // Durchlass (100…2900 Hz), nicht auf der Filterkante bei 0 Hz.
        s->setFrequency(14'099'000.0);
        QTest::qWait(1500);   // AGC/Mittelung einschwingen lassen
        const MeterReading toneAt0 = readMeter(model);
        qInfo() << "METER floor@14.200" << floorAt142.rawDbm << "shown" << floorAt142.shownDbm
                << "| tone@14.099+1k" << toneAt0.rawDbm << "shown" << toneAt0.shownDbm
                << "offset" << model.rxMeterOffsetDb();
        QVERIFY2(toneAt0.rawDbm > floorAt142.rawDbm + 20.0,
                 qPrintable(QStringLiteral("Der S9-Ton auf 14,100 MHz ist nicht zu sehen: "
                                           "Ton %1 dBm, Flur %2 dBm")
                            .arg(toneAt0.rawDbm).arg(floorAt142.rawDbm)));

        const int attBefore = att.attenuatorDb();
        att.setAttenuation(attBefore + 10);
        QTest::qWait(1500);
        const MeterReading toneAt10 = readMeter(model);
        qInfo() << "METER att" << attBefore << "->" << att.attenuatorDb()
                << "| tone raw" << toneAt10.rawDbm << "shown" << toneAt10.shownDbm
                << "offset" << model.rxMeterOffsetDb();
        const double rawDrop = toneAt0.rawDbm - toneAt10.rawDbm;
        QVERIFY2(rawDrop > 7.0 && rawDrop < 13.0,
                 qPrintable(QStringLiteral("+10 dB Daempfung senkten den Rohwert um %1 dB").arg(rawDrop)));
        const double shownDrift = toneAt0.shownDbm - toneAt10.shownDbm;
        QVERIFY2(qAbs(shownDrift) < 3.0,
                 qPrintable(QStringLiteral("Die Anzeige wanderte um %1 dB, obwohl der Offset "
                                           "die Daempfung ausgleichen muss").arg(shownDrift)));
        att.setAttenuation(attBefore);
        QTest::qWait(300);
        s->setFrequency(14'200'000.0);
        QTest::qWait(300);

        // ── 4b. Abtastrate ───────────────────────────────────────────
        // Der HPSDR-Weg kennt eine Rate fuer alle Stroeme; wir nehmen die
        // hoechste erlaubte, die nicht schon anliegt, damit im Protokoll
        // ein Wechsel sichtbar wird.
        const QVector<int> rates = model.allowedStreamSampleRates();
        const int rateBefore = model.connectionSampleRateHz();
        int rateWanted = 0;
        for (const int r : rates) {
            if (r != rateBefore) { rateWanted = std::max(rateWanted, r); }
        }
        qInfo() << "RATES allowed" << rates << "before" << rateBefore << "wanted" << rateWanted;
        if (rateWanted > 0) {
            model.requestSliceSampleRate(s->sliceIndex(), rateWanted);
            QTRY_COMPARE_WITH_TIMEOUT(model.connectionSampleRateHz(), rateWanted, 8000);
            QTRY_VERIFY_WITH_TIMEOUT(model.connectionState() == ConnectionState::Connected, 8000);
            const int iqBeforeRate = iq.count();
            QTRY_VERIFY_WITH_TIMEOUT(iq.count() >= iqBeforeRate + 20, 10000);
            qInfo() << "RATE now" << model.connectionSampleRateHz()
                    << "text" << model.connectionSampleRateText()
                    << "IQ blocks since" << (iq.count() - iqBeforeRate);
        }

        // ── 4c. TUNE ein/aus — PTT am Simulator ──────────────────────
        const bool noTx = qEnvironmentVariableIsSet("LONGPATH_HPSDRSIM_NO_TX");
        bool tuned = false;
        bool moxed = false;
        if (!noTx) {
            QSignalSpy refused(&model, &RadioModel::tuneRefused);
            QSignalSpy vol(&model.transmitModel(), &TransmitModel::audioVolumeChanged);
            auto lastVol = [&vol]() { return vol.isEmpty() ? -1.0 : vol.last().first().toDouble(); };
            qInfo() << "POWER slider" << model.transmitModel().power()
                    << "tune W" << model.transmitModel().tunePowerForBand(bandFromFrequency(s->frequency()))
                    << "audioVolume before" << lastVol();
            model.setTune(true);
            QTest::qWait(1200);
            qInfo() << "POWER at TUNE: audioVolume" << lastVol() << "changes" << vol.count();
            // Der Simulator rechnet aus dem Sende-IQ eine Vorwaertsleistung
            // und schickt sie als Telemetrie zurueck; RadioStatus muss sie
            // fuer dieses Board richtig auslesen (HL2 legt die Werte anders
            // als Hermes/ANAN).
            const RadioStatus& rs = model.radioStatus();
            qInfo() << "TUNE on: isTune" << model.isTune() << "mox" << model.mox()
                    << "refused" << (refused.isEmpty() ? QString() : refused.first().first().toString())
                    << "| fwd W" << rs.forwardPowerWatts() << "refl W" << rs.reflectedPowerWatts()
                    << "swr" << rs.swrRatio() << "PA C" << rs.paTemperatureCelsius()
                    << "tx?" << rs.isTransmitting();
            tuned = model.isTune();
            const double fwdAtTune = rs.forwardPowerWatts();
            QVERIFY2(fwdAtTune > 0.0,
                     "Bei TUNE kam keine Vorwaertsleistung aus der Telemetrie an");
            QVERIFY2(rs.isTransmitting() && rs.activePttSource() == PttSource::Tune,
                     qPrintable(QStringLiteral("RadioStatus weiss nichts vom TUNE: tx=%1 Quelle=%2")
                                .arg(rs.isTransmitting()).arg(pttSourceLabel(rs.activePttSource()))));
            model.setTune(false);
            QTRY_VERIFY_WITH_TIMEOUT(!model.mox(), 8000);
            QTest::qWait(400);
            qInfo() << "TUNE off: isTune" << model.isTune() << "mox" << model.mox()
                    << "fwd W" << rs.forwardPowerWatts() << "tx?" << rs.isTransmitting();
            QVERIFY2(!model.isTune(), "TUNE liess sich nicht wieder ausschalten");
            QVERIFY2(!rs.isTransmitting(), "RadioStatus meldet nach TUNE-aus noch: sendet");
            const double fwdAtDriveSlider = fwdAtTune;

            // ── 4c'. TUNE ueber den Tune-Regler (HL2-Sonderweg) ──────
            // Voreinstellung wie in Thetis/mi0bot: TUNE nimmt den
            // Drive-Regler (console.cs:46561). Stellt man im Setup „Use
            // Tune Slider" ein, traegt beim HL2 der Ton selbst den Pegel
            // (TXPostGenToneMag, mi0bot: „HL2 only has 15 step output
            // attenuator"), der Drive-Byte geht auf 0. Der Simulator
            // bildet die 16 Stufen des HL2 nach — die Leistung muss
            // deutlich unter der vom Drive-Regler liegen.
            model.transmitModel().setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
            model.setTune(true);
            QTest::qWait(1200);
            const double fwdAtTuneSlider = rs.forwardPowerWatts();
            qInfo() << "TUNE(Tune-Regler) on: tune W"
                    << model.transmitModel().tunePowerForBand(bandFromFrequency(s->frequency()))
                    << "toneMag" << model.transmitModel().txPostGenToneMag()
                    << "audioVolume" << lastVol()
                    << "| fwd W" << fwdAtTuneSlider << "(Drive-Regler:" << fwdAtDriveSlider << ")";
            model.setTune(false);
            QTRY_VERIFY_WITH_TIMEOUT(!model.mox(), 8000);
            QTest::qWait(400);
            model.transmitModel().setTuneDrivePowerSource(DrivePowerSource::DriveSlider);
            // (Der Simulator kennt die 16 Stufen nur als -hermeslite2; sein
            // -hermeslite rechnet Drive 0 linear zu 0 W — deshalb nur die
            // obere Schranke.)
            if (info.boardType == HPSDRHW::HermesLite) {
                QVERIFY2(fwdAtTuneSlider < 0.3 * fwdAtDriveSlider,
                         qPrintable(QStringLiteral("HL2-TUNE ueber den Tune-Regler (1 W) gab %1 W, "
                                                   "ueber den Drive-Regler (100 %) %2 W")
                                    .arg(fwdAtTuneSlider).arg(fwdAtDriveSlider)));
            }

            // ── 4d. MOX mit PC-Mikrofon ──────────────────────────────
            // Der HL2 hat keine Mikrofonbuchse; Longpath nimmt das
            // PC-Mikrofon. MOX muss PTT setzen und den normalen
            // Sendepegel (nicht den TUNE-Pegel) auf den Draht legen.
            model.setMox(true);
            QTRY_VERIFY_WITH_TIMEOUT(model.mox(), 5000);
            QTest::qWait(1000);
            qInfo() << "POWER at MOX: audioVolume" << lastVol() << "changes" << vol.count()
                    << "| fwd W" << rs.forwardPowerWatts();
            qInfo() << "MOX on: mox" << model.mox() << "micSource" << int(model.transmitModel().micSource())
                    << "micLocked" << model.transmitModel().isMicSourceLocked()
                    << "tx?" << rs.isTransmitting();
            moxed = model.mox();
            QVERIFY2(rs.isTransmitting() && rs.activePttSource() == PttSource::Mox,
                     qPrintable(QStringLiteral("RadioStatus weiss nichts vom MOX: tx=%1 Quelle=%2")
                                .arg(rs.isTransmitting()).arg(pttSourceLabel(rs.activePttSource()))));
            model.setMox(false);
            QTRY_VERIFY_WITH_TIMEOUT(!model.mox(), 8000);
            QTest::qWait(400);
            qInfo() << "MOX off: mox" << model.mox();
        }

        // ── 4e. Zweiter Empfaenger ───────────────────────────────────
        // Ein zweiter Empfaenger heisst am Draht ein weiterer DDC mit
        // eigener Frequenz. Der Simulator zeigt RECEIVERS und RX FREQn.
        const int slicesBefore = model.slices().size();
        const int secondId = model.addSlice();
        QTest::qWait(600);
        SliceModel* s2 = model.sliceById(secondId);
        qInfo() << "SLICE2 id" << secondId << "slices" << slicesBefore << "->" << model.slices().size()
                << "stream" << (s2 ? s2->streamIndex() : -99)
                << "rx2Enabled" << model.rx2Enabled()
                << "activeRx" << model.connectionActiveRxCount();
        if (s2) {
            s2->setFrequency(7'050'000.0);
            QTest::qWait(800);
            qInfo() << "SLICE2 freq" << s2->frequency() << "stream" << s2->streamIndex();
            model.removeSlice(secondId);
            QTest::qWait(400);
            qInfo() << "SLICE2 removed, slices" << model.slices().size();
        }

        // ── 4f. N2ADR-Filterplatine am Open-Collector-Bus ────────────
        // Der HL2 hat kein Alex-Board. Sein Vorfilter beim Empfang und
        // sein Oberwellenfilter beim Senden ist die N2ADR-Platine, die an
        // denselben sieben Open-Collector-Pins haengt. Longpath fuellt die
        // OcMatrix beim Verbinden aus dem N2ADR-Preset (Vorgabe: ein,
        // Issue #174), und buildCodecContext() liest daraus je Band und je
        // Senderichtung ein Byte. Am Draht MUSS ein Bandwechsel darum ein
        // anderes OC-Byte erzeugen — sonst sitzt beim Senden das falsche
        // Tiefpassfilter im Weg, und das ist der eine Fehler an einem
        // HL2, den man nicht am Bildschirm sieht, sondern beim Nachbarn.
        //
        //   40 m  Empfang Pins {2,6} = 0x44   Senden Pin {2} = 0x04
        //   20 m  Empfang Pins {3,6} = 0x48   Senden Pin {3} = 0x08
        QList<quint8> ocExpected;
        if (model.boardCapabilities().hasIoBoardHl2) {
            const OcMatrix& oc = model.ocMatrix();
            const quint8 rx40 = oc.maskFor(Band::Band40m, false);
            const quint8 tx40 = oc.maskFor(Band::Band40m, true);
            const quint8 rx20 = oc.maskFor(Band::Band20m, false);
            const quint8 tx20 = oc.maskFor(Band::Band20m, true);
            qInfo("OC matrix 40m rx=0x%02x tx=0x%02x | 20m rx=0x%02x tx=0x%02x",
                  rx40, tx40, rx20, tx20);
            QVERIFY2(rx40 != 0 && rx20 != 0 && rx40 != rx20,
                     "Die OcMatrix ist leer oder kennt keine zwei verschiedenen Baender — "
                     "dann kann am Draht auch nichts umschalten (N2ADR-Preset nicht angewandt?)");

            s->setFrequency(7'050'000.0);
            QTest::qWait(900);
            ocExpected << rx40;
            qInfo() << "OC 40m: slice" << s->frequency() << "erwartet 0x" << QString::number(rx40, 16);

            if (!noTx) {
                model.setMox(true);
                QTRY_VERIFY_WITH_TIMEOUT(model.mox(), 5000);
                QTest::qWait(700);
                ocExpected << tx40;
                model.setMox(false);
                QTRY_VERIFY_WITH_TIMEOUT(!model.mox(), 8000);
                QTest::qWait(500);
                ocExpected << rx40;
                qInfo() << "OC 40m senden: erwartet 0x" << QString::number(tx40, 16);
            }

            s->setFrequency(14'200'000.0);
            QTest::qWait(900);
            ocExpected << rx20;
            qInfo() << "OC 20m: slice" << s->frequency() << "erwartet 0x" << QString::number(rx20, 16);
        }

        // ── 5. Trennen ───────────────────────────────────────────────
        model.disconnectFromRadio();
        QTRY_COMPARE_WITH_TIMEOUT(model.connectionState(), ConnectionState::Disconnected, 8000);

        // ── 6. Was der Simulator gesehen hat ─────────────────────────
        const QString logPath = qEnvironmentVariable("LONGPATH_HPSDRSIM_LOG");
        if (!logPath.isEmpty()) {
            QTest::qWait(300);
            QFile f(logPath);
            if (f.open(QIODevice::ReadOnly)) {
                const QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
                QStringList seen;
                for (const QString& l : lines) {
                    if (l.contains(QLatin1String("FREQ")) || l.contains(QLatin1String("Rate"), Qt::CaseInsensitive)
                        || l.contains(QLatin1String("PTT")) || l.contains(QLatin1String("Hermes"))
                        || l.contains(QLatin1String("DEVICE")) || l.contains(QLatin1String("Start"))
                        || l.contains(QLatin1String("Stop")) || l.contains(QLatin1String("ALEX"))
                        || l.contains(QLatin1String("DRIVE")) || l.contains(QLatin1String("RECEIVERS"))
                        || l.contains(QLatin1String("ATT"))
                        || l.contains(QLatin1String("OpenCollector"))) {
                        seen << l.trimmed();
                    }
                }
                const QString all = seen.join(QLatin1Char('\n'));
                qInfo().noquote() << "SIMULATOR SAW:\n  " + seen.join(QStringLiteral("\n  "));
                QVERIFY2(all.contains(QStringLiteral("14200000")),
                         "Die Frequenz 14,2 MHz kam beim Simulator nie an");

                // Abtastrate: Bit-Paar 0..3 = 48/96/192/384 kHz.
                if (rateWanted > 0) {
                    const int code = (rateWanted == 384000) ? 3 : (rateWanted == 192000) ? 2
                                   : (rateWanted == 96000) ? 1 : 0;
                    const QRegularExpression rateRe(
                        QStringLiteral("SampleRate= 0000000%1").arg(code));
                    QVERIFY2(all.contains(rateRe),
                             qPrintable(QStringLiteral("Die Abtastrate %1 (Code %2) kam beim Simulator nie an")
                                        .arg(rateWanted).arg(code)));
                }

                // PTT: bei TUNE muss 1 gekommen sein — und die LETZTE
                // PTT-Zeile muss 0 sein, sonst sendet das Geraet nach dem
                // Trennen weiter.
                QString lastPtt;
                for (const QString& l : seen) {
                    if (l.contains(QLatin1String("PTT="))) { lastPtt = l; }
                }
                if (tuned) {
                    QVERIFY2(all.contains(QStringLiteral("PTT= 00000001")),
                             "TUNE war an, aber der Simulator sah nie PTT=1");
                }
                if (moxed) {
                    // Drei getrennte Sendephasen (TUNE, TUNE ueber den
                    // Tune-Regler, MOX) → PTT muss mehrfach auf 1 gegangen sein.
                    QVERIFY2(all.count(QStringLiteral("PTT= 00000001")) >= 3,
                             "MOX war an, aber der Simulator sah dafuer kein weiteres PTT=1");
                }
                // Daempfungsglied: die Stufe muss den Draht erreicht haben.
                QVERIFY2(all.contains(QStringLiteral("ATT")),
                         "Der Simulator sah nie eine Daempfungs-Einstellung");
                // N2ADR am OC-Bus: der Simulator meldet jede Aenderung des
                // Open-Collector-Bytes. Die erwarteten Werte muessen in
                // dieser Reihenfolge vorgekommen sein (Zwischenwerte sind
                // erlaubt — der Weg dahin gehoert dem Geraet).
                if (!ocExpected.isEmpty()) {
                    QList<quint8> ocSeen;
                    const QRegularExpression ocRe(
                        QStringLiteral("OpenCollector=\\s*([0-9a-f]{8})"));
                    for (const QString& l : lines) {
                        const auto m = ocRe.match(l);
                        if (m.hasMatch()) {
                            ocSeen << static_cast<quint8>(m.captured(1).toUInt(nullptr, 16));
                        }
                    }
                    QStringList seenHex;
                    for (const quint8 v : ocSeen) { seenHex << QStringLiteral("0x%1").arg(v, 2, 16, QLatin1Char('0')); }
                    QStringList wantHex;
                    for (const quint8 v : ocExpected) { wantHex << QStringLiteral("0x%1").arg(v, 2, 16, QLatin1Char('0')); }
                    qInfo().noquote() << "OC am Draht:" << seenHex.join(QStringLiteral(" "))
                                      << "| erwartet der Reihe nach:" << wantHex.join(QStringLiteral(" "));
                    int at = 0;
                    for (const quint8 want : ocExpected) {
                        while (at < ocSeen.size() && ocSeen.at(at) != want) { ++at; }
                        QVERIFY2(at < ocSeen.size(),
                                 qPrintable(QStringLiteral("Das OC-Byte 0x%1 kam am Simulator nie an. "
                                                           "Gesehen: %2 — erwartet: %3")
                                            .arg(want, 2, 16, QLatin1Char('0'))
                                            .arg(seenHex.join(QLatin1Char(' ')))
                                            .arg(wantHex.join(QLatin1Char(' ')))));
                        ++at;
                    }
                }

                // Zweiter Empfaenger: seine Frequenz muss auf einem
                // eigenen DDC angekommen sein.
                QVERIFY2(all.contains(QStringLiteral("7050000")),
                         "Die Frequenz des zweiten Empfaengers (7,05 MHz) kam beim Simulator nie an");
                if (!lastPtt.isEmpty()) {
                    QVERIFY2(lastPtt.contains(QStringLiteral("PTT= 00000000")),
                             qPrintable(QStringLiteral("Letzter PTT-Stand am Simulator ist nicht 0: %1").arg(lastPtt)));
                }
            }
        }
    }
};

QTEST_MAIN(TstHpsdrSimWorkbench)
#include "tst_hpsdr_sim_workbench.moc"
