// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_station3d_view.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Die WebEngine-Fassung der 3D-Flaeche, so weit es ohne Bildschirm und
// ohne Netz geht (nur gebaut, wenn Qt WebEngine gefunden wurde):
//
//   * die Deutung der Meldungen aus der Seite (Googles Absage -> Satz,
//     Schluesselproblem ja/nein) -- ohne Seite, direkt gefuettert,
//   * die echte Seite in einem echten Chromium, aber mit einer
//     unerreichbaren CesiumJS-Quelle: sie muss ihr Scheitern ueber die
//     Konsole melden, und die Flaeche muss es als failed() weitergeben.
//     Das prueft die Bruecke Seite -> C++ und den Rueckfallweg.
//
// Mit LONGPATH_NETZ_3D=1 laedt der letzte Fall CesiumJS wirklich vom CDN
// (festgenagelt, SRI) und wartet auf "ready" -- oder auf die ehrliche
// Absage "webgl", wenn die Darstellung ohne Bildschirm kein WebGL hat.

#include <QtTest>

#include "gui/widgets/Station3dView.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

using namespace Longpath;

namespace {
// Nimmt Verbindungen an und antwortet nie: so haengt die Seite beim Laden
// von CesiumJS -- weder "ready" noch ein Fehler, nur die Startfrist
// entscheidet. Ohne Netz nach aussen.
class SilentServer {
public:
    SilentServer()
    {
        m_srv.listen(QHostAddress::LocalHost, 0);
        QObject::connect(&m_srv, &QTcpServer::newConnection, [this]() {
            while (QTcpSocket* s = m_srv.nextPendingConnection()) { m_held << s; }
        });
    }
    QString base() const
    {
        return QStringLiteral("http://127.0.0.1:%1/cesium/").arg(m_srv.serverPort());
    }
private:
    QTcpServer m_srv;
    QList<QTcpSocket*> m_held;
};
} // namespace

class TstStation3dView : public QObject { Q_OBJECT
private slots:
    // Live-Befund 2026-11-11 (Probebau logbuch/gesamt): Logbuch offen,
    // Karte verborgen -- 30 s spaeter "the 3D view did not start", und die
    // Flaeche blieb tot, bis man 3D aus- und einschaltete. Eine nie
    // gezeigte QWebEngineView laedt und zeichnet nicht; die Frist lief
    // trotzdem ab dem Konstruktor. Jetzt: die Frist zaehlt nur, solange die
    // Ansicht zu sehen ist, und ein Zeitablauf ist nicht endgueltig.
    void hidden_view_does_not_time_out_and_a_timeout_is_not_final()
    {
        SilentServer silent;
        Station3dView v(nullptr, silent.base());
        v.setStartTimeoutMsForTest(400);
        QSignalSpy failed(&v, &Station3dSurface::failed);

        // Verborgen: auch weit ueber die Frist hinaus kein Fehler.
        QTest::qWait(1500);
        QCOMPARE(failed.count(), 0);

        // Gezeigt: jetzt laeuft die Frist.
        v.resize(320, 200);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v));
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 5000);
        QVERIFY2(failed.at(0).at(0).toString().contains(QLatin1String("did not start")),
                 qPrintable(failed.at(0).at(0).toString()));

        // Nicht endgueltig: der naechste Flug startet die Seite neu, statt
        // sofort aufzugeben -- und bekommt eine neue Frist.
        Station3dFlight f;
        f.call = QStringLiteral("W1AW");
        v.fly(f);
        QTest::qWait(150);
        QCOMPARE(failed.count(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 2, 5000);

        // Verborgen waehrend eines neuen Versuchs: die Frist ruht.
        v.hide();
        v.fly(f);
        QTest::qWait(1200);
        QCOMPARE(failed.count(), 2);
        // Wieder zu sehen: die Frist laeuft weiter (und schlaegt hier zu).
        v.show();
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 3, 5000);
    }

    void google_refusal_from_the_page_is_a_key_problem()
    {
        Station3dView v(nullptr, QStringLiteral("http://127.0.0.1:9/"));
        QSignalSpy failed(&v, &Station3dSurface::failed);
        v.handlePageMessage(QStringLiteral(
            "{\"event\":\"failed\",\"kind\":\"tiles\",\"source\":\"google\",\"status\":403,\"body\":"
            "\"{\\\"error\\\":{\\\"code\\\":403,\\\"message\\\":\\\"x\\\",\\\"status\\\":"
            "\\\"PERMISSION_DENIED\\\",\\\"details\\\":[{\\\"reason\\\":\\\"SERVICE_DISABLED\\\"}]}}\","
            "\"message\":\"Request has failed. Status Code: 403\"}"));
        QCOMPARE(failed.count(), 1);
        QVERIFY2(failed.at(0).at(0).toString().contains(QLatin1String("Map Tiles API")),
                 qPrintable(failed.at(0).at(0).toString()));
        QCOMPARE(failed.at(0).at(1).toBool(), true);

        // Cesium ion: dieselbe Bruecke, Cesiums Absage, Cesiums Satz.
        QSignalSpy ionFailed(&v, &Station3dSurface::failed);
        v.handlePageMessage(QStringLiteral(
            "{\"event\":\"failed\",\"kind\":\"tiles\",\"source\":\"ion\",\"status\":401,"
            "\"body\":\"{\\\"code\\\":\\\"INVALID_TOKEN\\\",\\\"message\\\":"
            "\\\"Invalid access token\\\"}\",\"message\":\"Request has failed.\"}"));
        QCOMPARE(ionFailed.count(), 1);
        QVERIFY2(ionFailed.at(0).at(0).toString().contains(QLatin1String("Cesium ion does not know")),
                 qPrintable(ionFailed.at(0).at(0).toString()));
        QCOMPARE(ionFailed.at(0).at(1).toBool(), true);

        // Kein HTTP-Status: Netz, kein Schluesselproblem.
        v.handlePageMessage(QStringLiteral(
            "{\"event\":\"failed\",\"kind\":\"tiles\",\"status\":0,\"body\":\"\","
            "\"message\":\"offline\"}"));
        QCOMPARE(failed.count(), 3);   // Google, ion, jetzt das Netz
        QCOMPARE(failed.at(2).at(1).toBool(), false);

        // "ready" holt einen wartenden Flug nach; "arrived" wird gemeldet.
        QSignalSpy ready(&v, &Station3dView::ready);
        QSignalSpy arrived(&v, &Station3dSurface::arrived);
        v.handlePageMessage(QStringLiteral("{\"event\":\"ready\",\"cesium\":\"1.146\"}"));
        QCOMPARE(ready.count(), 1);
        QVERIFY(v.isReadyForTest());
        v.handlePageMessage(QStringLiteral("{\"event\":\"arrived\",\"ground\":312.5}"));
        QCOMPARE(arrived.count(), 1);
    }

    void unreachable_cesium_is_reported_by_the_real_page()
    {
        // Port 9 (discard) auf der eigenen Maschine: niemand hoert dort,
        // die Anfrage scheitert sofort und ohne Netz nach aussen.
        Station3dView v(nullptr, QStringLiteral("http://127.0.0.1:9/cesium/"));
        v.resize(400, 300);
        QSignalSpy failed(&v, &Station3dSurface::failed);
        v.show();
        QTRY_VERIFY_WITH_TIMEOUT(failed.count() >= 1, 25000);
        const QString why = failed.at(0).at(0).toString();
        QVERIFY2(why.contains(QLatin1String("CesiumJS")), qPrintable(why));
        QCOMPARE(failed.at(0).at(1).toBool(), false);
        QVERIFY(!v.isReadyForTest());

        // Ein Flug auf eine tote Seite faellt sofort zurueck, statt zu warten.
        Station3dFlight f;
        f.call = QStringLiteral("W1AW");
        v.fly(f);
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 2, 2000);
    }

    void real_cesium_from_the_cdn()
    {
        if (qEnvironmentVariable("LONGPATH_NETZ_3D") != QLatin1String("1")) {
            QSKIP("LONGPATH_NETZ_3D=1 laedt CesiumJS wirklich vom CDN");
        }
        Station3dView v;
        v.resize(640, 400);
        QSignalSpy ready(&v, &Station3dView::ready);
        QSignalSpy failed(&v, &Station3dSurface::failed);
        v.show();
        QTRY_VERIFY_WITH_TIMEOUT(ready.count() + failed.count() >= 1, 60000);
        if (failed.count() > 0) {
            const QString why = failed.at(0).at(0).toString();
            qInfo() << "page reported:" << why;
            // Ohne Bildschirm darf WebGL fehlen -- aber CesiumJS muss
            // geladen sein (SRI passt, CDN erreichbar).
            QVERIFY2(why.contains(QLatin1String("WebGL")), qPrintable(why));
            return;
        }

        // Ein Flug mit einem ERFUNDENEN Schluessel: Google lehnt ab, und
        // die Absage muss den ganzen Weg zurueck -- CesiumJS-Fehler ->
        // Konsole -> interpret() -- als Schluesselproblem ankommen.
        Station3dFlight f;
        f.call = QStringLiteral("W1AW");
        f.lat = 41.7147; f.lon = -72.7272;
        f.hasHome = true; f.homeLat = 48.3; f.homeLon = 14.5;
        f.km = 6700; f.bearingDeg = 296;
        f.secret = QStringLiteral("AIzaPruefstandNichtEchtXXXXXXXXXXXXXXXX");
        v.fly(f);
        QTRY_VERIFY_WITH_TIMEOUT(failed.count() >= 1, 30000);
        const QString why = failed.at(0).at(0).toString();
        qInfo() << "Google with an invented key:" << why;
        QVERIFY2(why.contains(QLatin1String("does not know this key")), qPrintable(why));
        QCOMPARE(failed.at(0).at(1).toBool(), true);

        // Derselbe Weg ueber Cesium ion mit einem erfundenen Zugang -- und
        // danach ein zweiter Versuch: er muss wieder fragen, nicht an der
        // ersten Absage kleben (Cesiums eigener Weg wuerde puffern).
        for (int round = 1; round <= 2; ++round) {
            Station3dFlight g = f;
            g.viaCesiumIon = true;
            g.secret = QStringLiteral(
                "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJqdGkiOiJwcnVlZnN0YW5kIn0.UHJ1ZWZzdGFuZE5pY2h0RWNodA");
            const int before = failed.count();
            v.fly(g);
            QTRY_VERIFY_WITH_TIMEOUT(failed.count() > before, 30000);
            const QString whyIon = failed.last().at(0).toString();
            qInfo() << "Cesium ion with an invented token, round" << round << ":" << whyIon;
            QVERIFY2(whyIon.contains(QLatin1String("Cesium ion does not know this token")),
                     qPrintable(whyIon));
            QCOMPARE(failed.last().at(1).toBool(), true);
        }
    }
};

QTEST_MAIN(TstStation3dView)
#include "tst_station3d_view.moc"
