// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_qrz_logbook_fetcher.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Der Abholer fuer das QRZ-Logbuch, ohne QRZ: erst der Antwort-Parser
// (rohe ADIF-Daten im `&`-getrennten Body, prozentkodierte Variante,
// FAIL mit Grund, hoechste Kennung aus LOGIDS oder aus den Datensaetzen),
// dann das Blaettern gegen einen lokalen HTTP-Mock — zwei Seiten, und die
// zweite Anfrage muss AFTERLOGID auf die hoechste Kennung plus eins
// setzen.

#include <QtTest>

#include "core/QrzLogbookFetcher.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

using namespace Longpath;

namespace {

QByteArray record(int id, const QString& call)
{
    return QStringLiteral("<APP_QRZLOG_LOGID:%1>%2 <CALL:%3>%4 <QSO_DATE:8>20260901 "
                          "<TIME_ON:6>%5 <BAND:3>20m <MODE:3>SSB <EOR>\n")
        .arg(QString::number(id).size()).arg(id).arg(call.size()).arg(call)
        .arg(id % 1440, 6, 10, QChar('0')).toUtf8();
}

/// Ein QRZ, das genau eine Seitengroesse beherrscht: bei AFTERLOGID<=1
/// kommen `first` Datensaetze (Kennungen 1..first), danach `second`.
class MockQrz : public QTcpServer {
public:
    int first{250}, second{2};
    QStringList requests;

    MockQrz()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            QTcpSocket* sock = nextPendingConnection();
            auto* buf = new QByteArray();
            connect(sock, &QTcpSocket::readyRead, sock, [this, sock, buf] {
                buf->append(sock->readAll());
                const int headerEnd = buf->indexOf("\r\n\r\n");
                if (headerEnd < 0) { return; }
                int contentLength = 0;
                for (const QByteArray& line : buf->left(headerEnd).split('\n')) {
                    const QByteArray l = line.trimmed();
                    if (l.toLower().startsWith("content-length:")) {
                        contentLength = l.mid(15).trimmed().toInt();
                    }
                }
                if (buf->size() - (headerEnd + 4) < contentLength) { return; }
                const QString body = QString::fromUtf8(buf->mid(headerEnd + 4));
                requests << body;

                const bool later = body.contains(QStringLiteral("AFTERLOGID"));
                const int from = later ? first + 1 : 1;
                const int n = later ? second : first;
                QByteArray adif; QStringList ids;
                for (int i = 0; i < n; ++i) {
                    adif += record(from + i, QStringLiteral("OE5T%1").arg(i, 3, 10, QChar('0')));
                    ids << QString::number(from + i);
                }
                const QByteArray resp = "RESULT=OK&COUNT=" + QByteArray::number(n)
                    + "&LOGIDS=" + ids.join(QLatin1Char(',')).toUtf8()
                    + "&ADIF=" + adif;
                const QByteArray http =
                    "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
                    + QByteArray::number(resp.size()) + "\r\nConnection: close\r\n\r\n" + resp;
                sock->write(http);
                sock->flush();
                sock->disconnectFromHost();
                delete buf;
            });
        });
    }
};

} // namespace

class TstQrzLogbookFetcher : public QObject { Q_OBJECT
private slots:
    void raw_adif_in_the_body_is_taken_whole()
    {
        const QByteArray body =
            "RESULT=OK&COUNT=2&LOGIDS=1180,1181&ADIF=<CALL:6>OE5SOS <COMMENT:9>R&B music "
            "<APP_QRZLOG_LOGID:4>1180 <EOR>\n<CALL:5>DL1XX <APP_QRZLOG_LOGID:4>1181 <EOR>\n";
        const auto p = QrzLogbookFetcher::parsePage(body);
        QVERIFY(p.ok);
        QCOMPARE(p.count, 2);
        QCOMPARE(p.maxLogId, qint64(1181));
        // Das `&` im Kommentar darf die ADIF-Daten nicht abschneiden.
        QVERIFY2(p.adif.contains(QStringLiteral("R&B music")), qPrintable(p.adif));
        QCOMPARE(p.adif.count(QStringLiteral("<EOR>")), 2);
    }

    void a_percent_encoded_body_is_decoded()
    {
        const QByteArray body =
            "RESULT=OK&COUNT=1&ADIF=%3CCALL%3A6%3EOE5SOS%20%3CAPP_QRZLOG_LOGID%3A2%3E42%20%3CEOR%3E";
        const auto p = QrzLogbookFetcher::parsePage(body);
        QVERIFY(p.ok);
        QVERIFY2(p.adif.startsWith(QStringLiteral("<CALL:6>OE5SOS")), qPrintable(p.adif));
        // Ohne LOGIDS kommt die Kennung aus dem Datensatz.
        QCOMPARE(p.maxLogId, qint64(42));
    }

    void fail_carries_the_reason_and_no_records()
    {
        const auto p = QrzLogbookFetcher::parsePage("RESULT=FAIL&REASON=invalid%20api%20key");
        QVERIFY(!p.ok);
        QCOMPARE(p.reason, QStringLiteral("invalid api key"));
        QCOMPARE(p.count, 0);
        QVERIFY(p.adif.isEmpty());
        QCOMPARE(p.maxLogId, qint64(-1));
    }

    void a_missing_count_is_derived_from_the_records()
    {
        const auto p = QrzLogbookFetcher::parsePage(
            "RESULT=OK&ADIF=<CALL:5>A1AAA <EOR><CALL:5>B2BBB <EOR>");
        QVERIFY(p.ok);
        QCOMPARE(p.count, 2);
    }

    void two_pages_are_fetched_and_the_second_asks_after_the_highest_id()
    {
        MockQrz qrz;
        QVERIFY(qrz.listen(QHostAddress::LocalHost, 0));

        QrzLogbookFetcher f;
        f.setApiKey(QStringLiteral("TEST-KEY"));
        f.setEndpointForTest(QUrl(QStringLiteral("http://127.0.0.1:%1/api").arg(qrz.serverPort())));
        QSignalSpy prog(&f, &QrzLogbookFetcher::progress);
        QSignalSpy done(&f, &QrzLogbookFetcher::finished);

        f.fetchAll();
        QVERIFY(f.isBusy());
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 10000);
        QVERIFY(!f.isBusy());

        const QList<QVariant> args = done.first();
        QVERIFY2(args.at(0).toBool(), qPrintable(args.at(3).toString()));
        QCOMPARE(args.at(2).toInt(), 252);
        QCOMPARE(args.at(1).toString().count(QStringLiteral("<EOR>")), 252);
        QCOMPARE(prog.count(), 2);

        QCOMPARE(qrz.requests.size(), 2);
        QVERIFY2(qrz.requests.at(0).contains(QStringLiteral("ACTION=FETCH")), qPrintable(qrz.requests.at(0)));
        QVERIFY2(qrz.requests.at(0).contains(QStringLiteral("MAX%3A250")) || qrz.requests.at(0).contains(QStringLiteral("MAX:250")),
                 qPrintable(qrz.requests.at(0)));
        QVERIFY2(!qrz.requests.at(0).contains(QStringLiteral("AFTERLOGID")), "erste Seite ohne Anker");
        // Zweite Seite: hoechste Kennung der ersten (250) plus eins.
        QVERIFY2(qrz.requests.at(1).contains(QStringLiteral("AFTERLOGID%3A251"))
                 || qrz.requests.at(1).contains(QStringLiteral("AFTERLOGID:251")),
                 qPrintable(qrz.requests.at(1)));
        QVERIFY2(qrz.requests.at(0).contains(QStringLiteral("KEY=TEST-KEY")), "Schluessel fehlt");
    }

    void without_a_key_it_finishes_at_once_with_an_error()
    {
        QrzLogbookFetcher f;
        QSignalSpy done(&f, &QrzLogbookFetcher::finished);
        f.fetchAll();
        QCOMPARE(done.count(), 1);
        QVERIFY(!done.first().at(0).toBool());
    }
};

QTEST_MAIN(TstQrzLogbookFetcher)
#include "tst_qrz_logbook_fetcher.moc"
