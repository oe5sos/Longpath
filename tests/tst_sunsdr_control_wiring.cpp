// SPDX-License-Identifier: GPL-3.0-or-later
//
// Schritt "Steuerung": Frequenz UND Betriebsart laufen jetzt in beide
// Richtungen zwischen Longpath und ExpertSDR2 -- nicht nur die
// Panadapter-Mitte (Schritt "Bild", siehe tst_sunsdr_spectrum_wiring.cpp).
//
// Diese Pruefung geht durch den tatsaechlichen Weg (connectSunSdr) und
// deckt sieben Stellen ab:
//
//   1. Eingehend: "vfo:0,0,hz" (VFO-Kanal A von Empfaenger 0) uebernimmt
//      die Scheibenfrequenz, "modulation:0,mode" die Betriebsart.
//   2. Derselbe Bench-Fund wie bei "Bild" (siehe MainWindow_SunSdr.cpp,
//      "ExpertSDR2 hat ZWEI Empfaenger-Plaetze"): eine Zeile fuer
//      Empfaenger 1 oder VFO-Kanal B darf die Scheibe NICHT beeinflussen.
//   3. Ausgehend: eine Bedieneraenderung an der Scheibe (setFrequency/
//      setDspMode) sendet "vfo:0,0,hz;"/"modulation:0,mode;" tatsaechlich
//      an ExpertSDR2 -- und eine EINGEHENDE Aenderung darf NICHT
//      postwendend als Ausgangsbefehl zurueckgehen (Echo-Schutz,
//      m_sunSdrApplyingRemoteState).
//   4. Sicherheitsschranke, Wurzel: connectSunSdr()'s Scheiben-Rueckfall
//      ueberspringt jede Scheibe mit echter DDC-Bindung von vornherein --
//      legt lieber eine neue an, statt eine echte, moeglicherweise
//      sendefaehige Scheibe zu kapern (auch fuer Ton/Panadapter, nicht
//      nur Steuerung -- Bench-Fund 2026-08-24, Grundgeruest-Durchsicht).
//   5. Sicherheitsschranke, spaeter: bindet ein ECHTES Funkgeraet die
//      anfangs ungebundene SunSDR-Zielscheibe NACHTRAEGLICH (bind
//      UnboundSlices() beim naechsten echten Verbindungsaufbau), gibt
//      SunSDR sie GANZ frei -- Ton, Panadapter UND Steuerung, nicht nur
//      eine Richtung (releaseSunSdrSlice(), ausgeloest von SliceModel::
//      streamIndexChanged).
//   6. Wird die Zielscheibe geloescht, waehrend SunSDR verbunden ist,
//      gibt SunSDR sie ebenso frei -- und eine SPAETER angelegte Scheibe,
//      die dieselbe (wiederverwendete) Kennung bekommt, erbt NICHTS davon
//      (releaseSunSdrSlice(), ausgeloest von RadioModel::sliceRemoved).
//   7. Siehe wireSunSdrOutboundControl()/applyRemoteSunSdrFrequency() fuer
//      die Begruendung der Sicherheitsschranke insgesamt (CLAUDE.local.md:
//      "Wo Zurueckhaltung und Sicherheit sich widersprechen, gewinnt die
//      Sicherheit").

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocketServer>
#include <QWebSocket>

#include "core/AudioEngine.h"
#include "core/FFTRouter.h"
#include "core/TciClient.h"
#include "core/WdspTypes.h"
#include "gui/MainWindow.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {
constexpr int kWaitMs = 8000;

const QString kReadyLine = QStringLiteral(
    "device:Testgeraet;iq_samplerate:48000;audio_samplerate:48000;"
    "vfo:0,0,14164070;vfo:0,1,14163000;vfo:1,0,1905000;"
    "modulation:0,usb;modulation:1,lsb;"
    "start;ready;");
} // namespace

class TstSunSdrControlWiring : public QObject
{
    Q_OBJECT

private slots:
    void verbindenUebernimmtFrequenzUndModusVonEmpfaenger0()
    {
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            QWebSocket* remote = server.nextPendingConnection();
            QVERIFY(remote);
            remote->sendTextMessage(kReadyLine);
        });

        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);

        mw->connectSunSdrForTest(
            QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));

        const int sliceId = mw->sunSdrTargetSliceForTest();
        SliceModel* slice = model->sliceById(sliceId);
        QVERIFY(slice);

        TciClient* client = mw->sunSdrClientForTest();
        QVERIFY(client);
        QTRY_COMPARE_WITH_TIMEOUT(client->state(), TciClient::State::Connected,
                                  kWaitMs);

        // vfo:0,0 (Kanal A von Empfaenger 0) -- NICHT vfo:1,0 (1905000)
        // und NICHT vfo:0,1 (Kanal B, 14163000).
        QTRY_COMPARE_WITH_TIMEOUT(slice->frequency(), 14164070.0, kWaitMs);
        QCOMPARE(slice->dspMode(), DSPMode::USB);

        mw->close();
    }

    void empfaenger1UndVfoKanalBWerdenIgnoriert()
    {
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");
        QWebSocket* remote = nullptr;
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            remote = server.nextPendingConnection();
            QVERIFY(remote);
            remote->sendTextMessage(kReadyLine);
        });

        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        mw->connectSunSdrForTest(
            QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));

        const int sliceId = mw->sunSdrTargetSliceForTest();
        SliceModel* slice = model->sliceById(sliceId);
        QVERIFY(slice);

        TciClient* client = mw->sunSdrClientForTest();
        QTRY_COMPARE_WITH_TIMEOUT(client->state(), TciClient::State::Connected,
                                  kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(slice->frequency(), 14164070.0, kWaitMs);
        QVERIFY(remote);

        // Empfaenger 1 retunt/schaltet um -- die Scheibe (an Empfaenger 0
        // gebunden) darf sich davon nicht beeindrucken lassen.
        remote->sendTextMessage(QStringLiteral(
            "vfo:1,0,3573000;modulation:1,cwl;"));
        QTest::qWait(300);
        QCOMPARE(slice->frequency(), 14164070.0);
        QCOMPARE(slice->dspMode(), DSPMode::USB);

        // VFO-Kanal B von Empfaenger 0 -- ebenfalls kein Einfluss, nur
        // Kanal A zaehlt.
        remote->sendTextMessage(QStringLiteral("vfo:0,1,7000000;"));
        QTest::qWait(300);
        QCOMPARE(slice->frequency(), 14164070.0);

        // Gegenprobe: Empfaenger 0 / Kanal A bewegt die Scheibe wirklich.
        remote->sendTextMessage(QStringLiteral("vfo:0,0,7100000;modulation:0,lsb;"));
        QTRY_COMPARE_WITH_TIMEOUT(slice->frequency(), 7100000.0, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(slice->dspMode(), DSPMode::LSB, kWaitMs);

        mw->close();
    }

    void bedienerAenderungSendetVfoUndModulationAus()
    {
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");
        QStringList received;
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            QWebSocket* remote = server.nextPendingConnection();
            QVERIFY(remote);
            connect(remote, &QWebSocket::textMessageReceived, this,
                    [&received](const QString& msg) { received << msg; });
            remote->sendTextMessage(kReadyLine);
        });

        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        mw->connectSunSdrForTest(
            QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));

        const int sliceId = mw->sunSdrTargetSliceForTest();
        SliceModel* slice = model->sliceById(sliceId);
        QVERIFY(slice);

        TciClient* client = mw->sunSdrClientForTest();
        QTRY_COMPARE_WITH_TIMEOUT(client->state(), TciClient::State::Connected,
                                  kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(slice->frequency(), 14164070.0, kWaitMs);
        received.clear();

        // Der Bediener dreht am VFO und schaltet den Modus um. CWU statt
        // CWL absichtlich: Bench-Fund 2026-08-24, ExpertSDR2 kennt am
        // Draht nur das allgemeine "cw", nicht "cwl"/"cwu" -- beide
        // muessen darum auf "cw" abbilden (siehe
        // tciModeStringForDspMode), nicht nur CWL zufaellig richtig
        // aussehen, weil es der einzige hier geprobte Wert war.
        slice->setFrequency(7100000.0);
        slice->setDspMode(DSPMode::CWU);

        QTRY_VERIFY_WITH_TIMEOUT(
            received.contains(QStringLiteral("vfo:0,0,7100000;")), kWaitMs);
        QVERIFY2(received.contains(QStringLiteral("modulation:0,cw;")),
                 "modulation-Ausgangsbefehl fehlt oder noch \"cwu\" statt "
                 "des von ExpertSDR2 verstandenen \"cw\"");

        mw->close();
    }

    void cwZurueckmeldungDrehtEineStehendeCwuWahlNichtAufCwlZurueck()
    {
        // Bench-Fund 2026-08-24: ExpertSDR2 versteht "cwl"/"cwu" nicht,
        // Longpath sendet darum fuer beide das allgemeine "cw" (siehe
        // tciModeStringForDspMode). Meldet ExpertSDR2 daraufhin -- wie
        // jede TCI-Aenderung -- eine frische "modulation:0,cw;" zurueck,
        // darf applyRemoteSunSdrModulation() das NICHT naiv auf CWL
        // abbilden: die eigene CWU-Wahl wuerde sonst bei der naechsten
        // Rueckmeldung still verschwinden, obwohl der Bediener sie nie
        // geaendert hat. Nur ein "cw" auf eine Scheibe, die noch NICHT
        // in CW ist, darf tatsaechlich (auf CWL) umschalten.
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");
        QWebSocket* remote = nullptr;
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            remote = server.nextPendingConnection();
            QVERIFY(remote);
            remote->sendTextMessage(kReadyLine);
        });

        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        mw->connectSunSdrForTest(
            QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));

        const int sliceId = mw->sunSdrTargetSliceForTest();
        SliceModel* slice = model->sliceById(sliceId);
        QVERIFY(slice);

        TciClient* client = mw->sunSdrClientForTest();
        QTRY_COMPARE_WITH_TIMEOUT(client->state(), TciClient::State::Connected,
                                  kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(slice->frequency(), 14164070.0, kWaitMs);

        // Bediener waehlt CWU an der Scheibe (nicht ueber TCI -- das
        // Ausgangsecho selbst ist hier nicht der Pruefgegenstand).
        slice->setDspMode(DSPMode::CWU);
        QCOMPARE(slice->dspMode(), DSPMode::CWU);

        // ExpertSDR2 meldet "cw" zurueck -- genau das erwartete Echo
        // eines Geraets, das die Seitenbandwahl gar nicht kennt.
        QVERIFY(remote);
        remote->sendTextMessage(QStringLiteral("modulation:0,cw;"));
        QTest::qWait(300);
        QCOMPARE(slice->dspMode(), DSPMode::CWU);

        // Gegenprobe: eine Scheibe, die NICHT schon in CW ist, wechselt
        // bei "cw" tatsaechlich (auf CWL, der Thetis-faehige Normalfall).
        remote->sendTextMessage(QStringLiteral("modulation:0,usb;"));
        QTRY_COMPARE_WITH_TIMEOUT(slice->dspMode(), DSPMode::USB, kWaitMs);
        remote->sendTextMessage(QStringLiteral("modulation:0,cw;"));
        QTRY_COMPARE_WITH_TIMEOUT(slice->dspMode(), DSPMode::CWL, kWaitMs);

        mw->close();
    }

    void eingehendeAenderungWirdNichtAlsBefehlZurueckgesendet()
    {
        // Echo-Schutz: die eingehende Zeile darf nicht postwendend als
        // Ausgangsbefehl an dasselbe Geraet zurueckgehen (siehe
        // m_sunSdrApplyingRemoteState in MainWindow_SunSdr.cpp).
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");
        QWebSocket* remote = nullptr;
        QStringList received;
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            remote = server.nextPendingConnection();
            QVERIFY(remote);
            connect(remote, &QWebSocket::textMessageReceived, this,
                    [&received](const QString& msg) { received << msg; });
            remote->sendTextMessage(kReadyLine);
        });

        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        mw->connectSunSdrForTest(
            QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));

        const int sliceId = mw->sunSdrTargetSliceForTest();
        SliceModel* slice = model->sliceById(sliceId);
        QVERIFY(slice);

        TciClient* client = mw->sunSdrClientForTest();
        QTRY_COMPARE_WITH_TIMEOUT(client->state(), TciClient::State::Connected,
                                  kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(slice->frequency(), 14164070.0, kWaitMs);
        QVERIFY(remote);
        received.clear();

        remote->sendTextMessage(QStringLiteral("vfo:0,0,3573000;modulation:0,am;"));
        QTRY_COMPARE_WITH_TIMEOUT(slice->frequency(), 3573000.0, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(slice->dspMode(), DSPMode::AM, kWaitMs);

        // Kurze Gnadenfrist fuer einen etwaigen (falschen) Echo-Befehl,
        // dann pruefen: nichts davon darf beim Testgeraet angekommen sein.
        QTest::qWait(300);
        QVERIFY2(!received.contains(QStringLiteral("vfo:0,0,3573000;")),
                 "eingehende Frequenz wurde als Ausgangsbefehl zurueckgesendet");
        QVERIFY2(!received.contains(QStringLiteral("modulation:0,am;")),
                 "eingehender Modus wurde als Ausgangsbefehl zurueckgesendet");

        mw->close();
    }

    void verbindenUeberspringtEineEchteScheibeUndLegtEineNeueAn()
    {
        // Bench-Fund (Grundgeruest-Durchsicht 2026-08-24, nach Bau von
        // "Steuerung"): der Scheiben-Rueckfall in connectSunSdr() nahm die
        // aktive Scheibe blind, auch mit echter DDC-Bindung (streamIndex()
        // >= 0 -- ein ECHTES, moeglicherweise sendefaehiges Funkgeraet).
        // Die Steuerungsrichtung war dagegen schon abgesichert
        // (wireSunSdrOutboundControl weigerte sich zu verdrahten) -- Ton
        // und Panadapter liefen an der echten Scheibe aber unbemerkt
        // weiter. Fix: der Rueckfall ueberspringt jede gebundene Scheibe
        // von vornherein. Diese Pruefung deckt die Wurzel ab, nicht nur
        // die zweite Schranke weiter unten in wireSunSdrOutboundControl.
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            QWebSocket* remote = server.nextPendingConnection();
            QVERIFY(remote);
            remote->sendTextMessage(kReadyLine);
        });

        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);
        // Einzige vorhandene Scheibe: wird automatisch aktiv (addSlice(),
        // erste Scheibe ueberhaupt) UND bekommt eine nachgestellte echte
        // DDC-Bindung.
        const int realId = model->addSlice();
        SliceModel* realSlice = model->sliceById(realId);
        QVERIFY(realSlice);
        realSlice->setStreamIndex(0);
        const double realFreqBefore = realSlice->frequency();

        mw->connectSunSdrForTest(
            QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));

        const int sunSdrId = mw->sunSdrTargetSliceForTest();
        QVERIFY2(sunSdrId != realId,
                 "SunSDR hat die echte, gebundene Scheibe als Ziel genommen");
        QVERIFY2(sunSdrId >= 0, "SunSDR hat gar keine Zielscheibe bekommen");
        SliceModel* sunSdrSlice = model->sliceById(sunSdrId);
        QVERIFY(sunSdrSlice);
        QVERIFY2(sunSdrSlice->streamIndex() < 0,
                 "die neu angelegte SunSDR-Scheibe hat selbst eine "
                 "DDC-Bindung -- Testannahme verletzt");

        TciClient* client = mw->sunSdrClientForTest();
        QTRY_COMPARE_WITH_TIMEOUT(client->state(), TciClient::State::Connected,
                                  kWaitMs);
        QTest::qWait(300);

        // Die echte Scheibe ist unberuehrt -- weder ihre Frequenz noch
        // ihr Ton wurden von SunSDR uebernommen.
        QCOMPARE(realSlice->frequency(), realFreqBefore);
        AudioEngine* audio = model->audioEngine();
        QVERIFY(audio);
        QVERIFY2(!audio->sunSdrAudioEnabled(realId),
                 "SunSDR-Ton lief in die echte Scheibe statt in die neue");

        mw->close();
    }

    void zielscheibeWirdBeiEchterDdcBindungSpaeterGanzFreigegeben()
    {
        // Bench-Fund (Grundgeruest-Durchsicht 2026-08-24): der Rueckfall
        // oben verhindert, dass SunSDR eine SCHON gebundene Scheibe
        // uebernimmt -- aber eine anfangs UNGEBUNDENE Zielscheibe kann
        // SPAETER doch noch gebunden werden, wenn der Betreiber danach
        // ein echtes Funkgeraet verbindet (RadioModel::bindUnboundSlices(),
        // aufgerufen bei jedem RadioModel::connectToRadio()). Ohne einen
        // eigenen Wachposten haette in diesem Fall nur die
        // Steuerungsrichtung angehalten -- Ton und Panadapter waeren an
        // der jetzt echten Scheibe unbemerkt weitergelaufen. Fix:
        // releaseSunSdrSlice() (ausgeloest von SliceModel::
        // streamIndexChanged) gibt bei einer solchen Umwidmung ALLES frei.
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");
        QStringList received;
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            QWebSocket* remote = server.nextPendingConnection();
            QVERIFY(remote);
            connect(remote, &QWebSocket::textMessageReceived, this,
                    [&received](const QString& msg) { received << msg; });
            remote->sendTextMessage(kReadyLine);
        });

        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);
        mw->connectSunSdrForTest(
            QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));

        const int sliceId = mw->sunSdrTargetSliceForTest();
        SliceModel* slice = model->sliceById(sliceId);
        QVERIFY(slice);

        TciClient* client = mw->sunSdrClientForTest();
        QTRY_COMPARE_WITH_TIMEOUT(client->state(), TciClient::State::Connected,
                                  kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(slice->frequency(), 14164070.0, kWaitMs);
        AudioEngine* audio = model->audioEngine();
        QVERIFY(audio);
        QTRY_VERIFY_WITH_TIMEOUT(audio->sunSdrAudioEnabled(sliceId), kWaitMs);
        auto* router = model->fftRouter();
        QVERIFY(router);
        const int pseudoIndex = MainWindow::sunSdrPseudoStreamIndexForTest();
        QVERIFY(!router->pansForReceiver(pseudoIndex).isEmpty());

        // Ein echtes Funkgeraet uebernimmt die Scheibe -- ohne echte
        // Hardware ist setStreamIndex() der einzige Weg, genau den
        // Zustandswechsel herzustellen, den bindUnboundSlices() ausloest.
        slice->setStreamIndex(0);

        QTRY_VERIFY_WITH_TIMEOUT(mw->sunSdrTargetSliceForTest() < 0, kWaitMs);
        QVERIFY2(!audio->sunSdrAudioEnabled(sliceId),
                 "SunSDR-Ton lief nach der Uebernahme weiter");
        QVERIFY2(router->pansForReceiver(pseudoIndex).isEmpty(),
                 "Panadapter-Zuordnung blieb nach der Uebernahme stehen");

        // Und die Ausgangssteuerung ist ganz stumm -- eine weitere
        // Frequenzaenderung an der jetzt echten Scheibe darf nichts mehr
        // an ExpertSDR2 senden.
        //
        // Erst zur Ruhe kommen lassen: setStreamIndex() oben loest ueber
        // releaseSunSdrSlice() selbst schon Aufraeumarbeiten aus (Ton
        // abschalten, Router-Zuordnung entfernen), und ob dabei am
        // echten Socket noch etwas unterwegs war, soll HIER nicht
        // mitgezaehlt werden -- nur was NACH der Freigabe NEU passiert.
        QVERIFY(client);
        QTest::qWait(300);
        received.clear();
        slice->setFrequency(7100000.0);
        QTest::qWait(300);
        QVERIFY2(received.isEmpty(),
                 qPrintable(QStringLiteral(
                     "Ausgangssteuerung sendete noch nach der "
                     "Uebernahme -- empfangen: %1")
                                .arg(received.join(QStringLiteral(" | ")))));

        mw->close();
    }

    void scheibeLoeschenGibtDasZielFreiUndVerhindertKennungsWiederverwendung()
    {
        // Bench-Fund (Grundgeruest-Durchsicht 2026-08-24): RadioModel::
        // addSlice() vergibt die NIEDRIGSTE freie Kennung (siehe dort,
        // "Lowest-free also keeps the slice-letter contract"). Wird die
        // SunSDR-Zielscheibe geloescht und danach eine neue Scheibe
        // angelegt, kann die neue Scheibe dieselbe Kennung bekommen, die
        // m_sunSdrTargetSliceId noch traegt -- ohne releaseSunSdrSlice()
        // wuerde diese voellig unbeteiligte neue Scheibe stillschweigend
        // SunSDR-Ton/-Steuerung erben.
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");
        QWebSocket* remote = nullptr;
        QStringList received;
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            remote = server.nextPendingConnection();
            QVERIFY(remote);
            connect(remote, &QWebSocket::textMessageReceived, this,
                    [&received](const QString& msg) { received << msg; });
            remote->sendTextMessage(kReadyLine);
        });

        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);
        mw->connectSunSdrForTest(
            QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));

        const int sunSdrId = mw->sunSdrTargetSliceForTest();
        QVERIFY(sunSdrId >= 0);
        TciClient* client = mw->sunSdrClientForTest();
        QTRY_COMPARE_WITH_TIMEOUT(client->state(), TciClient::State::Connected,
                                  kWaitMs);

        // removeSlice() weigert sich, die letzte Scheibe zu loeschen --
        // eine zweite anlegen, damit die SunSDR-Scheibe wirklich weg kann.
        const int otherId = model->addSlice();
        QVERIFY(model->sliceById(otherId));

        model->removeSlice(sunSdrId);
        QTRY_VERIFY_WITH_TIMEOUT(mw->sunSdrTargetSliceForTest() < 0, kWaitMs);
        QVERIFY2(!model->sliceById(sunSdrId),
                 "Testannahme verletzt: Scheibe wurde nicht geloescht");

        // removeSlice() loest selbst einiges aus (DDC-Neuzuordnung,
        // TX-Arbiter-Sync, ein moeglicher activeSliceChanged-Wechsel auf
        // die verbliebene Scheibe) -- QTRY_VERIFY oben kann durchlaufen,
        // sobald m_sunSdrTargetSliceId auf -1 steht, OHNE dass ein
        // dadurch angestossener Netzwerkschreibvorgang (echter Socket,
        // nicht bloss ein Qt-Signal) schon zugestellt wurde. Erst
        // absichtlich zur Ruhe kommen lassen, DANN "received" leeren --
        // sonst zaehlt eine verspaetet ankommende, harmlose Restmeldung
        // aus dem Loeschen selbst faelschlich als Befehl der
        // wiederverwendeten Scheibe.
        QTest::qWait(300);

        // Eine neue Scheibe kann jetzt dieselbe Kennung wiederbekommen
        // (niedrigste freie zuerst) -- gegenpruefen, dass das tatsaechlich
        // passiert, sonst prueft der Rest hier den falschen Fall.
        const int reusedId = model->addSlice();
        SliceModel* reusedSlice = model->sliceById(reusedId);
        QVERIFY(reusedSlice);
        if (reusedId != sunSdrId) {
            QSKIP("Kennung wurde nicht wiederverwendet -- Testvoraussetzung "
                  "diesmal nicht gegeben, nichts zu pruefen");
        }

        // Die wiederverwendete Scheibe darf NICHT von SunSDR gesteuert
        // werden -- weder eingehend noch ausgehend.
        QVERIFY(remote);
        received.clear();
        const double freqBefore = reusedSlice->frequency();
        remote->sendTextMessage(QStringLiteral("vfo:0,0,3500000;"));
        QTest::qWait(300);
        QCOMPARE(reusedSlice->frequency(), freqBefore);

        reusedSlice->setFrequency(7100000.0);
        QTest::qWait(300);
        QVERIFY2(received.isEmpty(),
                 qPrintable(QStringLiteral(
                     "die wiederverwendete Scheibe hat einen TCI-Befehl "
                     "ausgeloest, obwohl sie nie mit SunSDR verbunden "
                     "wurde -- empfangen: %1")
                                .arg(received.join(QStringLiteral(" | ")))));

        mw->close();
    }
};

QTEST_MAIN(TstSunSdrControlWiring)
#include "tst_sunsdr_control_wiring.moc"
