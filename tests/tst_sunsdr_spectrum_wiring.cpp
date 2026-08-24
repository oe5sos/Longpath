// SPDX-License-Identifier: GPL-3.0-or-later
//
// Schritt "Bild": der Panadapter fuer den SunSDR speist sich aus
// derselben FFTEngine/FFTRouter-Infrastruktur, die auch ein echtes
// Funkgeraet benutzt (siehe Kopf von MainWindow_SunSdr.cpp,
// Rechercheergebnis 2026-08-24) -- nicht ueber einen zweiten Pfad.
//
// Diese Pruefung geht durch den tatsaechlichen Weg (connectSunSdr), wie
// schon tst_sunsdr_connect_wiring.cpp fuer den Ton, und deckt die drei
// Stellen ab, an denen die Verdrahtung bisher NICHT geprueft war:
//
//   1. Die Router-Zuordnung (panId -> kSunSdrPseudoStreamIndex) steht
//      SOFORT beim Verbinden, nicht erst wenn ExpertSDR2 antwortet --
//      reassertSunSdrRouterMapping() laeuft synchron in wireSunSdr().
//   2. Nach der Selbstauskunft ("dds:") und einem I/Q-Rahmen zeigt der
//      Panadapter die WAHRE Mittenfrequenz/Rate, nicht die
//      Scheiben-Vorgabe oder 0.
//   3. Selbstheilung: rebuildFftRouting() loescht bei jedem Umbau die
//      Router-Zuordnung fuer JEDEN Panadapter (auch die SunSDR-Pseudo-
//      Scheibe, die dort NICHT mitlaeuft, siehe kSunSdrPseudoStreamIndex-
//      Kommentar) -- reassertSunSdrRouterMapping() haengt an
//      RadioModel::streamBindingsChanged und muss die Zuordnung danach
//      wiederherstellen, sonst verschwaende der SunSDR-Panadapter beim
//      naechsten VFO-Tick eines echten Empfaengers stumm.
//   4. Trennen (Fehler oder regulaer) entfernt die Router-Zuordnung
//      wieder -- Spiegelbild zum Ton-Abbau in connect_wiring.

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocketServer>
#include <QWebSocket>

#include "core/FFTRouter.h"
#include "core/TciBinaryFrame.h"
#include "core/TciClient.h"
#include "gui/MainWindow.h"
#include "gui/SpectrumWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {
constexpr int kWaitMs = 8000;

// Wortgetreu am Selbstauskunfts-Wortlaut gemessen (siehe
// tst_tci_client_ddc_center.cpp) -- dds: ist die wahre Mitte.
const QString kReadyLine = QStringLiteral(
    "device:Testgeraet;iq_samplerate:48000;audio_samplerate:48000;"
    "dds:0,1910670;start;ready;");
} // namespace

class TstSunSdrSpectrumWiring : public QObject
{
    Q_OBJECT

private slots:
    void verbindenSetztDieRouterZuordnungSofort()
    {
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");
        // Absichtlich KEIN newConnection-Handler, der antwortet: diese
        // Pruefung gilt dem Augenblick VOR jeder Netzantwort.

        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);
        auto* router = model->fftRouter();
        QVERIFY(router);

        mw->connectSunSdrForTest(
            QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));

        const int sliceId = mw->sunSdrTargetSliceForTest();
        QVERIFY2(sliceId >= 0, "keine Scheibe angelegt");
        SliceModel* slice = model->sliceById(sliceId);
        QVERIFY(slice);
        const QString panId = mw->panIdForSlice(slice);
        QVERIFY2(!panId.isEmpty(), "Scheibe hat keinen Panadapter");

        const int pseudoIndex = MainWindow::sunSdrPseudoStreamIndexForTest();
        QCOMPARE(router->pansForReceiver(pseudoIndex),
                 QList<QString>{panId});

        mw->close();
    }

    void ddsUndIqRahmenSetzenDieWahreMittenfrequenzAmPanadapter()
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
        QTRY_VERIFY_WITH_TIMEOUT(remote != nullptr, kWaitMs);

        SpectrumWidget* sw = mw->spectrumForSlice(slice);
        QVERIFY2(sw, "SunSDR-Scheibe hat keinen SpectrumWidget");

        // Vor dem ersten I/Q-Rahmen: dds: allein hat schon die Mitte
        // gesetzt (ddcCenterChanged-Pfad in wireSunSdr()), die Rate aber
        // noch nicht -- die kommt erst aus dem I/Q-Rahmen selbst
        // (iqFrameReady liefert sie, siehe TciClient.cpp).
        QTRY_COMPARE_WITH_TIMEOUT(sw->ddcCenterFrequency(), 1910670.0, kWaitMs);

        // Jetzt ein echter I/Q-Rahmen vom Testsender, wie ExpertSDR2 ihn
        // schicken wuerde (TciStreamType::IqStream, 2 Kanaele).
        QVector<float> iq(256 * 2, 0.1f);
        remote->sendBinaryMessage(TciBinaryFrame::buildStreamPayload(
            0, 48000, int(TciSampleType::Float32), iq.size(),
            int(TciStreamType::IqStream), 2, iq.constData()));

        QTRY_COMPARE_WITH_TIMEOUT(sw->sampleRate(), 48000.0, kWaitMs);
        // Die Mitte bleibt dieselbe -- ein I/Q-Rahmen darf sie nicht
        // stillschweigend auf die Scheiben-Vorgabe zuruecksetzen.
        QCOMPARE(sw->ddcCenterFrequency(), 1910670.0);

        mw->close();
    }

    void routerUmbauWirdSelbstheilendWiederhergestellt()
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
        auto* router = model->fftRouter();
        QVERIFY(router);

        mw->connectSunSdrForTest(
            QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));

        const int sliceId = mw->sunSdrTargetSliceForTest();
        SliceModel* slice = model->sliceById(sliceId);
        QVERIFY(slice);
        const QString panId = mw->panIdForSlice(slice);
        QVERIFY(!panId.isEmpty());
        const int pseudoIndex = MainWindow::sunSdrPseudoStreamIndexForTest();

        QCOMPARE(router->pansForReceiver(pseudoIndex), QList<QString>{panId});

        // Nachgestellter rebuildFftRouting()-Umbau: der loescht laut
        // eigenem Kommentar bei JEDEM Aufruf die Zuordnung fuer JEDEN
        // Panadapter, bevor er nur die echten, gebundenen Scheiben neu
        // eintraegt -- die SunSDR-Pseudoscheibe bleibt dabei aussen vor
        // (kein streamIndex() >= 0), wuerde also ohne Selbstheilung
        // stumm verschwinden.
        router->removePan(panId);
        QVERIFY(router->pansForReceiver(pseudoIndex).isEmpty());

        // Derselbe Ausloeser wie ein echter VFO-Tick -- siehe
        // wireSunSdr()-Anschluss an RadioModel::streamBindingsChanged.
        emit model->streamBindingsChanged(0, QVector<int>{});

        QCOMPARE(router->pansForReceiver(pseudoIndex), QList<QString>{panId});

        mw->close();
    }

    void trennenEntferntDieRouterZuordnungWieder()
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
        auto* router = model->fftRouter();
        QVERIFY(router);

        mw->connectSunSdrForTest(
            QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));

        TciClient* client = mw->sunSdrClientForTest();
        QVERIFY(client);
        QTRY_COMPARE_WITH_TIMEOUT(client->state(), TciClient::State::Connected,
                                  kWaitMs);

        const int pseudoIndex = MainWindow::sunSdrPseudoStreamIndexForTest();
        QVERIFY(!router->pansForReceiver(pseudoIndex).isEmpty());

        mw->disconnectSunSdrForTest();
        QTRY_VERIFY_WITH_TIMEOUT(router->pansForReceiver(pseudoIndex).isEmpty(),
                                 kWaitMs);

        mw->close();
    }
};

QTEST_MAIN(TstSunSdrSpectrumWiring)
#include "tst_sunsdr_spectrum_wiring.moc"
