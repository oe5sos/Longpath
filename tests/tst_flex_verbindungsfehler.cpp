// =================================================================
// tests/tst_flex_verbindungsfehler.cpp  (Longpath)
// =================================================================
//
// „Disconnected — Unknown error".
//
// Das stand am 2026-10-28 um 20:41 im Verbindungsfenster, dreimal in
// Folge, und dasselbe stand im Protokoll:
//
//   [20:41:37.006] INF: Flex: verbinde nach "172.30.30.12" : 4992
//   [20:41:37.007] WRN: Socketfehler: "Unknown error"
//
// Eine Millisekunde zwischen Wahl und Fehler, und als Begruendung ein
// Schulterzucken. Kein Ziel, keine Nummer, kein Socketzustand. Zwei
// Wiederholversuche desselben Laufs (20:41:13 und 20:41:21) meldeten
// danach GAR NICHTS mehr — weder „TCP steht" noch einen Fehler.
//
// Zwei Mangel, zwei Pruefungen:
//
//   1. Der Fehlerweg warf die Nummer weg und zeigte nur
//      QAbstractSocket::errorString(). Fuer alles, was Qt nicht kennt,
//      ist das wortwoertlich „Unknown error".
//   2. Es gab keine Frist. Bleibt der Handschlag aus, meldet der
//      Socket nie etwas, und der Zustand haengt fuer immer auf
//      Connecting — genau das Schweigen der beiden Wiederholversuche.
//
// Die Gegenprobe gegen die alte Fassung: Fall 1 erwartet, dass die
// Meldung das Ziel UND die Nummer traegt (alt: nur „Unknown error"),
// Fall 2 pruefte einen Weg, den es vorher nicht gab.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-28 — Original for Longpath by Martin Fischer, AI-assisted
//                 via Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>

#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>

#include "core/FlexRadioConnection.h"

using namespace Longpath;

class TstFlexVerbindungsfehler : public QObject {
    Q_OBJECT

private slots:
    // Jede Nummer, die Qt kennt, bekommt einen Klartext — und keiner
    // davon ist „Unknown error".
    void jedeNummerHatKlartext();

    // Der echte Weg auf dem Verlustband: nichts hoert auf dem Port.
    // Die Meldung muss Ziel und Nummer tragen.
    void abgewieseneVerbindungNenntZielUndNummer();

    // Die Frist, die es vorher nicht gab.
    void fristAbgelaufenMeldetZielUndDauer();
};

void TstFlexVerbindungsfehler::jedeNummerHatKlartext()
{
    const QList<QAbstractSocket::SocketError> alle = {
        QAbstractSocket::ConnectionRefusedError,
        QAbstractSocket::RemoteHostClosedError,
        QAbstractSocket::HostNotFoundError,
        QAbstractSocket::SocketAccessError,
        QAbstractSocket::SocketResourceError,
        QAbstractSocket::SocketTimeoutError,
        QAbstractSocket::DatagramTooLargeError,
        QAbstractSocket::NetworkError,
        QAbstractSocket::AddressInUseError,
        QAbstractSocket::SocketAddressNotAvailableError,
        QAbstractSocket::UnsupportedSocketOperationError,
        QAbstractSocket::UnfinishedSocketOperationError,
        QAbstractSocket::UnknownSocketError,
    };

    for (QAbstractSocket::SocketError e : alle) {
        const QString text = FlexRadioConnection::socketErrorText(e);
        QVERIFY2(!text.isEmpty(),
                 qPrintable(QStringLiteral("Nummer %1 ohne Klartext")
                                .arg(static_cast<int>(e))));
        QVERIFY2(!text.contains(QStringLiteral("Unknown error")),
                 qPrintable(QStringLiteral("Nummer %1 sagt wieder nichts: %2")
                                .arg(static_cast<int>(e)).arg(text)));
    }

    // Der Fall aus dem Protokoll: Qt kennt den errno nicht. Auch dann
    // muss die Meldung sagen, DASS die Zuordnung fehlt, statt zu
    // schweigen — und seit dem 2026-11-01 auch, was zu tun ist. Der
    // Wortlaut stand bis dahin auf Deutsch („errno unbekannt"); die
    // Meldung geht aber in die Verbindungsmaske und wird gelesen,
    // also ist sie englisch wie der Rest der Oberflaeche.
    const QString unbekannt =
        FlexRadioConnection::socketErrorText(QAbstractSocket::UnknownSocketError);
    QVERIFY2(unbekannt.contains(QStringLiteral("no reason"), Qt::CaseInsensitive),
             qPrintable(unbekannt));
    QVERIFY2(unbekannt.contains(QStringLiteral("try again"), Qt::CaseInsensitive),
             qPrintable(unbekannt));
}

void TstFlexVerbindungsfehler::abgewieseneVerbindungNenntZielUndNummer()
{
    // Einen freien Port bestimmen, indem wir einen aufmachen und
    // gleich wieder zumachen: danach hoert dort mit grosser Sicherheit
    // niemand, und die Verbindung wird abgewiesen.
    quint16 freierPort = 0;
    {
        QTcpServer probe;
        QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
        freierPort = probe.serverPort();
        probe.close();
    }
    QVERIFY(freierPort != 0);

    FlexRadioConnection verbindung;
    QSignalSpy fehler(&verbindung, &FlexRadioConnection::errorOccurred);
    verbindung.connectToRadio(QHostAddress::LocalHost, freierPort);

    QVERIFY2(fehler.wait(5000), "kein Fehler gemeldet");
    const QString msg = fehler.takeFirst().at(0).toString();

    // Das Ziel steht drin — das fehlte am 2026-10-28 vollstaendig.
    QVERIFY2(msg.contains(QStringLiteral("127.0.0.1")), qPrintable(msg));
    QVERIFY2(msg.contains(QString::number(freierPort)), qPrintable(msg));
    // Die Nummer steht drin, damit ein Protokoll ohne diese Tabelle
    // auswertbar bleibt. Seit dem 2026-11-01 heisst der Rahmen „no."
    // statt „Nr." — dieselbe Umstellung wie bei den Fehlertexten.
    QVERIFY2(msg.contains(QStringLiteral("no.")), qPrintable(msg));
    QCOMPARE(verbindung.state(), FlexRadioConnection::State::Error);
}

void TstFlexVerbindungsfehler::fristAbgelaufenMeldetZielUndDauer()
{
    FlexRadioConnection verbindung;
    QSignalSpy fehler(&verbindung, &FlexRadioConnection::errorOccurred);

    verbindung.triggerConnectTimeoutForTesting(
        QHostAddress(QStringLiteral("172.30.30.12")), 4992);

    QCOMPARE(fehler.count(), 1);
    const QString msg = fehler.takeFirst().at(0).toString();
    QVERIFY2(msg.contains(QStringLiteral("172.30.30.12:4992")), qPrintable(msg));
    QVERIFY2(msg.contains(QString::number(
                 FlexRadioConnection::kConnectTimeoutMs / 1000)),
             qPrintable(msg));
    QCOMPARE(verbindung.state(), FlexRadioConnection::State::Error);
}

QTEST_MAIN(TstFlexVerbindungsfehler)
#include "tst_flex_verbindungsfehler.moc"
