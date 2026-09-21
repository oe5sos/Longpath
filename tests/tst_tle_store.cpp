// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_tle_store.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Der TLE-Speicher: holt vom (hier nachgespielten) CelesTrak, schreibt
// die Datei, liest sie beim naechsten Mal, kennt ihr Alter — und laesst
// eine HTML-Fehlerseite nie die gute Datei ueberschreiben. Dazu der
// ADIF-Rundweg des Stempels APP_LONGPATH_SATS.

#include <QtTest>

#include "core/AdifLog.h"
#include "core/sat/TleStore.h"
#include "models/LogEntry.h"

#include <QFile>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

using namespace Longpath;

namespace {

const char* kTle =
    "ISS (ZARYA)\n"
    "1 25544U 98067A   08264.51782528 -.00002182  00000-0 -11606-4 0  2927\n"
    "2 25544  51.6416 247.4627 0006703 130.5360 325.0288 15.72125391563537\n";

// Antwortet auf jede Anfrage mit `body` (Status 200) — GET ohne Body,
// also reicht das Kopfende.
class MockCelestrak : public QTcpServer {
public:
    QByteArray body;
    int hits{0};
    MockCelestrak()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            QTcpSocket* sock = nextPendingConnection();
            connect(sock, &QTcpSocket::readyRead, sock, [this, sock] {
                const QByteArray req = sock->readAll();
                if (!req.contains("\r\n\r\n")) { return; }
                ++hits;
                const QByteArray http =
                    "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
                    + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                sock->write(http);
                sock->flush();
                sock->disconnectFromHost();
            });
        });
    }
};

} // namespace

class TstTleStore : public QObject { Q_OBJECT
private slots:
    void fetchWritesCacheAndReloads()
    {
        QTemporaryDir dir;
        MockCelestrak srv;
        srv.body = kTle;
        QVERIFY(srv.listen(QHostAddress::LocalHost));

        TleStore store;
        store.setCachePath(dir.path() + QStringLiteral("/tle/amateur.tle"));
        store.setSourceUrl(QUrl(QStringLiteral("http://127.0.0.1:%1/gp.php").arg(srv.serverPort())));
        QVERIFY(!store.loadCached());
        QVERIFY(store.isStale());

        QSignalSpy updated(&store, &TleStore::updated);
        QSignalSpy failed(&store, &TleStore::failed);
        store.refresh();
        QVERIFY(store.isBusy());
        QTRY_COMPARE_WITH_TIMEOUT(updated.count(), 1, 5000);
        QCOMPARE(failed.count(), 0);
        QVERIFY(store.hasData());
        QVERIFY(!store.isStale());
        QVERIFY(store.text().contains(QLatin1String("ISS (ZARYA)")));
        QVERIFY(QFile::exists(store.cachePath()));

        // Frisch → refreshIfStale holt nicht noch einmal.
        store.refreshIfStale(24);
        QVERIFY(!store.isBusy());
        QCOMPARE(srv.hits, 1);

        // Ein zweiter Store liest die Datei.
        TleStore again;
        again.setCachePath(store.cachePath());
        QVERIFY(again.loadCached());
        QCOMPARE(again.text(), store.text());
        QVERIFY(again.fetchedAt().isValid());
        QVERIFY(!again.isStale(24));
    }

    void anErrorPageNeverReplacesTheFile()
    {
        QTemporaryDir dir;
        const QString path = dir.path() + QStringLiteral("/amateur.tle");
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(kTle);
        }
        MockCelestrak srv;
        srv.body = "<html><body>No GP data found</body></html>";
        QVERIFY(srv.listen(QHostAddress::LocalHost));

        TleStore store;
        store.setCachePath(path);
        store.setSourceUrl(QUrl(QStringLiteral("http://127.0.0.1:%1/gp.php").arg(srv.serverPort())));
        QVERIFY(store.loadCached());
        QSignalSpy updated(&store, &TleStore::updated);
        QSignalSpy failed(&store, &TleStore::failed);
        store.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 5000);
        QCOMPARE(updated.count(), 0);
        QVERIFY(store.text().contains(QLatin1String("ISS")));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(QString::fromUtf8(f.readAll()).contains(QLatin1String("ISS")));
    }

    void unreachableSourceFails()
    {
        QTemporaryDir dir;
        TleStore store;
        store.setCachePath(dir.path() + QStringLiteral("/amateur.tle"));
        // Port 1 auf localhost: nichts hoert zu.
        store.setSourceUrl(QUrl(QStringLiteral("http://127.0.0.1:1/gp.php")));
        QSignalSpy failed(&store, &TleStore::failed);
        store.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 8000);
        QVERIFY(!store.hasData());
    }

    void theStampSurvivesTheAdifRoundTrip()
    {
        LogEntry e;
        e.call = QStringLiteral("OE3AA");
        e.timeOn = QDateTime(QDate(2026, 9, 21), QTime(12, 0), QTimeZone::UTC);
        e.band = QStringLiteral("20m");
        e.mode = QStringLiteral("SSB");
        e.extras.append(qMakePair(QStringLiteral("APP_LONGPATH_SATS"),
                                  QStringLiteral("ES'HAIL 2 el 34° az 164° · SO-50 el 7° az 279°")));
        const QString rec = e.toAdifRecord();
        QVERIFY(rec.contains(QLatin1String("<APP_LONGPATH_SATS:")));
        const QVector<LogEntry> back = AdifLog::parse(rec + QStringLiteral("<EOR>\n"));
        QCOMPARE(back.size(), 1);
        bool found = false;
        for (const auto& kv : back.first().extras) {
            if (kv.first.toUpper() == QLatin1String("APP_LONGPATH_SATS")) {
                found = true;
                QCOMPARE(kv.second, QStringLiteral("ES'HAIL 2 el 34° az 164° · SO-50 el 7° az 279°"));
            }
        }
        QVERIFY(found);
    }
};

QTEST_MAIN(TstTleStore)
#include "tst_tle_store.moc"
