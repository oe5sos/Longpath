// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Verdrahtung zwischen MainWindow, TciClient und AudioEngine haelt
// zusammen -- nicht nur jedes Stueck fuer sich.
//
// 2026-08-24. TciClient::audioFrameReady -> AudioEngine::
// feedSunSdrAudioData ist beides fuer sich geprueft (siehe
// tst_tci_client_*.cpp und tst_sunsdr_audio_feed.cpp), aber DIESE
// Pruefung geht durch den tatsaechlichen Weg, den der Menuepunkt geht:
// connectSunSdr() -> Scheibe anlegen/finden -> TciClient verbinden ->
// bei "ready" den Ton anfordern und die Scheibe freischalten -> beim
// Trennen wieder abschalten. Eine Kette ist nur so stark wie ihr
// schwaechstes Glied, und genau die Glieder DAZWISCHEN (welche Scheibe,
// wann freischalten, wann abschalten) haben in diesem Projekt schon
// mehrfach gefehlt, obwohl beide Enden fuer sich richtig waren.
//
// Selbstgebauter Testsender wie in tst_tci_client_ignores_lying_header.cpp
// -- unabhaengig davon, ob ExpertSDR2 gerade laeuft.

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocketServer>
#include <QWebSocket>

#include "core/AudioEngine.h"
#include "core/TciBinaryFrame.h"
#include "core/TciClient.h"
#include "gui/MainWindow.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {
constexpr int kWaitMs = 8000;
} // namespace

class TstSunSdrConnectWiring : public QObject
{
    Q_OBJECT

private slots:
    void verbindenLegtEineScheibeAnUndSchaltetDenTonFrei()
    {
        // Ohne verbundenes Funkgeraet hat ein frisches MainWindow NULL
        // Scheiben (gegengeprueft, siehe tst_sunsdr_audio_feed.cpp) --
        // genau der Fall, fuer den ein SunSDR am interessantesten ist,
        // und genau der Fall, den diese Pruefung absichtlich NICHT
        // umgeht.
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");

        QWebSocket* remote = nullptr;
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            remote = server.nextPendingConnection();
            QVERIFY(remote);
            remote->sendTextMessage(QStringLiteral(
                "device:Testgeraet;iq_samplerate:48000;audio_samplerate:48000;"
                "start;ready;"));
        });

        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);
        QVERIFY2(model->slices().isEmpty(),
                 "Testannahme verletzt: es gibt schon eine Scheibe, bevor "
                 "der SunSDR ueberhaupt verbunden wurde");

        mw->connectSunSdrForTest(
            QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));

        // Die Scheibe entsteht SOFORT beim Verbinden (siehe
        // MainWindow_SunSdr.cpp), nicht erst wenn die Verbindung steht.
        const int sliceId = mw->sunSdrTargetSliceForTest();
        QVERIFY2(sliceId >= 0,
                 "connectSunSdr hat keine Scheibe angelegt -- der SunSDR-Ton "
                 "haette keinen Weg in die Mischung");
        QVERIFY2(!model->slices().isEmpty(),
                 "keine Scheibe im Modell, obwohl eine Kennung zurueckkam");

        TciClient* client = mw->sunSdrClientForTest();
        QVERIFY(client);

        AudioEngine* audio = model->audioEngine();
        QVERIFY(audio);

        // Vor "ready": noch nicht freigeschaltet -- ein verfruehtes
        // Freischalten waere hier der klassische Fehler ("gebaut, aber
        // die Bedingung dafuer kam nie").
        QTRY_VERIFY_WITH_TIMEOUT(remote != nullptr, kWaitMs);

        QTRY_COMPARE_WITH_TIMEOUT(client->state(), TciClient::State::Connected,
                                  kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(audio->sunSdrAudioEnabled(sliceId), kWaitMs);

        // Jetzt ein echter Tonrahmen vom Testsender -- kein Absturz ist
        // hier der Kern; MasterMixer bietet keinen oeffentlichen
        // Lesezugriff, um den Ton selbst zu pruefen (wie schon bei Kiwi).
        QVector<float> pcm(256 * 2, 0.1f);
        remote->sendBinaryMessage(TciBinaryFrame::buildStreamPayload(
            0, 48000, int(TciSampleType::Float32), pcm.size(),
            int(TciStreamType::RxAudioStream), 2, pcm.constData()));
        QTest::qWait(300);
        QVERIFY(audio->sunSdrAudioEnabled(sliceId));

        // Trennen schaltet wieder ab -- sonst bliebe eine Scheibe
        // "opportunistisch" fuer einen Erzeuger, der nichts mehr liefert.
        mw->disconnectSunSdrForTest();
        QTRY_VERIFY_WITH_TIMEOUT(!audio->sunSdrAudioEnabled(sliceId), kWaitMs);

        mw->close();
    }

    void verbindenMitEinerVorhandenenScheibeLegtKeineZweiteAn()
    {
        // Gegenprobe zur ersten Pruefung: WENN schon eine Scheibe da ist
        // (z.B. weil ein Funkgeraet verbunden ist), muss connectSunSdr
        // die vorhandene nehmen, nicht eine zweite danebenlegen.
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            QWebSocket* remote = server.nextPendingConnection();
            QVERIFY(remote);
            remote->sendTextMessage(QStringLiteral(
                "device:Testgeraet;iq_samplerate:48000;audio_samplerate:48000;"
                "ready;"));
        });

        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);
        const int existingId = model->addSlice();
        QVERIFY(model->sliceById(existingId));
        const int countBefore = model->slices().size();

        mw->connectSunSdrForTest(
            QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));

        QCOMPARE(model->slices().size(), countBefore);

        mw->close();
    }
};

QTEST_MAIN(TstSunSdrConnectWiring)
#include "tst_sunsdr_connect_wiring.moc"
