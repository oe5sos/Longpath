// Prüfstand: ein gescheiterter Bind muss wiederholt werden — und sichtbar sein.
//
// Warum es das gibt: am 2026-10-02 wechselte der Rechner das Netz. Die
// eingestellte feste Adresse gab es danach nicht mehr, und im Log stand genau
// eine Zeile:
//
//     WRN: TciServer: failed to listen on "172.30.30.121" port 50001
//          "The address is not available"
//
// Danach blieb der Server für immer unten — auch als der Rechner wieder im
// alten Netz hing. Das Einstellungsfeld meldete bloß "Stopped", ohne zu sagen,
// worauf es wartet. Die Fernbedienung am Telefon fand nichts, und der Operator
// hielt das Programm für kaputt.
//
// Geprüft wird darum die NAHT, nicht der Getter: ein echter Socket belegt den
// Port, der Server scheitert daran, meldet das — und holt sich den Port, sobald
// er frei wird. Ein Prüfstand, der nur `bindWartetAufAdresse` abonniert und das
// Hochkommen nicht abwartet, würde die halbe Zusage prüfen.

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QSignalSpy>
#include <QTcpServer>
#include <QElapsedTimer>

#include "core/TciServer.h"

using Longpath::TciServer;

namespace {

/** Wartet, bis die Bedingung gilt — oder die Zeit abläuft.
 *
 *  Bewusst kein `QTest::qWait(n)` mit fester Zahl: der Wiederversuch hängt an
 *  einem 5-s-Timer, und ein belasteter CI-Läufer braucht dafür länger als
 *  dieser Rechner. Feste Wartezeiten sind in genau diesem Testbaum schon
 *  dreimal umgefallen (siehe TciBurstHelfer.h).
 */
template <typename F>
bool warteBis(F bedingung, int grenzeMs)
{
    QElapsedTimer uhr;
    uhr.start();
    while (!bedingung() && uhr.elapsed() < grenzeMs) { QTest::qWait(50); }
    return bedingung();
}

}  // namespace

class TestTciBindWiederversuch : public QObject {
    Q_OBJECT

private slots:

    /** Belegter Port: der Start scheitert, sagt es — und holt den Port nach,
     *  sobald er frei wird. */
    void belegterPortWirdNachgeholt()
    {
        // Ein fremder Socket hält den Port. Das ist dieselbe Lage wie eine
        // Adresse, die es (noch) nicht gibt: listen() scheitert, die Ursache
        // vergeht von selbst.
        QTcpServer blockierer;
        QVERIFY2(blockierer.listen(QHostAddress::LocalHost, 0),
                 "Der Blockierer konnte selbst keinen Port belegen");
        const quint16 port = blockierer.serverPort();

        TciServer server(nullptr);
        QSignalSpy wartet(&server, &TciServer::bindWartetAufAdresse);
        QSignalSpy gestartet(&server, &TciServer::serverStarted);

        // Erster Versuch muss scheitern — der Port ist weg.
        QVERIFY2(!server.start(QHostAddress(QHostAddress::LocalHost), port),
                 "start() meldete Erfolg, obwohl der Port belegt ist");
        QVERIFY2(!server.isRunning(), "Server behauptet zu laufen");

        // ... und das muss nach aussen dringen. Vor dieser Änderung gab es
        // dieses Signal nicht, und die Oberfläche zeigte nur "Stopped".
        QVERIFY2(warteBis([&] { return wartet.count() >= 1; }, 2000),
                 "Kein bindWartetAufAdresse — der Operator erfährt nichts");
        const QList<QVariant> args = wartet.first();
        QCOMPARE(args.at(1).toUInt(), static_cast<uint>(port));
        QVERIFY2(!args.at(2).toString().isEmpty(),
                 "Der Grund fehlt — 'Wartet auf ...' ohne Grund hilft nicht");

        // Jetzt gibt der Blockierer den Port frei. Ab hier darf NICHTS mehr
        // von aussen angestossen werden: genau das ist die Zusage.
        blockierer.close();

        QVERIFY2(warteBis([&] { return server.isRunning(); }, 30000),
                 "Der Server hat den freigewordenen Port nicht von selbst "
                 "genommen — das ist der Fehler vom 2026-10-02");
        QCOMPARE(server.port(), port);
        QVERIFY(gestartet.count() >= 1);

        server.stop();
    }

    /** stop() hebt einen laufenden Wiederversuch auf.
     *
     *  Die Falle: nach einem gescheiterten listen() ist `m_server` null, und
     *  stop() stieg bisher ganz oben mit `if (!m_server) return;` aus. Stünde
     *  das Abbrechen hinter diesem Ausstieg, liefe der Wiederversuch nach
     *  einem stop() munter weiter und holte den Server hinter dem Rücken des
     *  Operators wieder hoch. */
    void stopHebtDenWiederversuchAuf()
    {
        QTcpServer blockierer;
        QVERIFY(blockierer.listen(QHostAddress::LocalHost, 0));
        const quint16 port = blockierer.serverPort();

        TciServer server(nullptr);
        QVERIFY(!server.start(QHostAddress(QHostAddress::LocalHost), port));

        server.stop();          // ausdrücklich: ich will das nicht mehr
        blockierer.close();     // der Grund des Scheiterns vergeht

        // Jetzt DARF er nicht hochkommen. Hier ist Warten der Prüfpunkt: eine
        // Zusage "es passiert nichts" lässt sich nur über Zeit belegen. 12 s
        // sind mehr als zwei Wiederversuchsrunden.
        QVERIFY2(!warteBis([&] { return server.isRunning(); }, 12000),
                 "Der Server kam nach einem ausdrücklichen stop() von selbst "
                 "wieder hoch");
    }

    /** Glückt der Bind sofort, wird kein Wiederversuch geplant — und das
     *  Wartesignal bleibt still. Sonst stünde bei jedem normalen Start kurz
     *  "Wartet auf ..." in der Oberfläche. */
    void glueckterStartMeldetKeinWarten()
    {
        TciServer server(nullptr);
        QSignalSpy wartet(&server, &TciServer::bindWartetAufAdresse);

        QVERIFY(server.start(0));
        QVERIFY(server.isRunning());
        QTest::qWait(300);
        QCOMPARE(wartet.count(), 0);

        server.stop();
    }
};

QTEST_MAIN(TestTciBindWiederversuch)
#include "tst_tci_bind_wiederversuch.moc"

#else   // HAVE_WEBSOCKETS

#include <QtTest>
class TestTciBindWiederversuch : public QObject { Q_OBJECT };
QTEST_MAIN(TestTciBindWiederversuch)
#include "tst_tci_bind_wiederversuch.moc"

#endif
