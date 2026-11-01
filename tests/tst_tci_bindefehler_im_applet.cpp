// Prüfstand: der Grund für „port: -" muss DORT stehen, wo der Betreiber
// hinsieht.
//
// `TciServer::bindWartetAufAdresse` trägt Adresse, Port und Grund und gibt
// es seit dem 2026-10-02 (siehe `tst_tci_bind_wiederversuch`). Verdrahtet
// war es aber nur auf der Setup-Seite — also dort, wo man erst nachsieht,
// wenn man schon weiß, dass etwas fehlt. Das TCI-Applet, das der Betreiber
// offen hat, zeigte stattdessen „port: -": dieselbe Anzeige wie bei einem
// Server, den er selbst ausgeschaltet hat.
//
// Aufgefallen am 2026-10-31 beim Zeus-Abgleich: dort nimmt ein gemeinsamer
// Port für CAT und TCI die ganze Maschine beim Start mit (ihr #3295). Bei
// uns stirbt nichts — beide behandeln ein gescheitertes `listen` —, aber
// der Grund kam nicht an.
//
// Geprüft wird darum die NAHT, nicht das Feld: ein echter Socket belegt
// den Port, der Server scheitert daran, und die Kopfzeile des Applets muss
// es sagen — und es wieder vergessen, sobald der Port frei ist.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-31 — Angelegt, KI-gestützt über Anthropic Claude Code,
//                 Betreiber Martin Fischer.
// =================================================================

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QLabel>
#include <QTcpServer>
#include <QElapsedTimer>

#include "core/TciServer.h"
#include "gui/applets/TciApplet.h"

using Longpath::TciApplet;
using Longpath::TciServer;

namespace {

template <typename F>
bool warteBis(F bedingung, int grenzeMs)
{
    QElapsedTimer uhr;
    uhr.start();
    while (!bedingung() && uhr.elapsed() < grenzeMs) { QTest::qWait(50); }
    return bedingung();
}

QLabel* portFeld(TciApplet& applet)
{
    return applet.findChild<QLabel*>(QStringLiteral("tciPortLabel"));
}

}  // namespace

class TestTciBindefehlerImApplet : public QObject
{
    Q_OBJECT

private slots:

    /** Belegter Port: die Kopfzeile sagt „blocked", der Hinweis sagt warum. */
    void belegterPortStehtInDerKopfzeile()
    {
        QTcpServer blockierer;
        QVERIFY2(blockierer.listen(QHostAddress::LocalHost, 0),
                 "Der Blockierer konnte selbst keinen Port belegen");
        const quint16 port = blockierer.serverPort();

        TciServer server(nullptr);
        TciApplet applet(&server, nullptr);

        QLabel* feld = portFeld(applet);
        QVERIFY2(feld != nullptr, "Kein Feld 'tciPortLabel' im Applet");
        QCOMPARE(feld->text(), QStringLiteral("port: -"));

        QVERIFY2(!server.start(QHostAddress(QHostAddress::LocalHost), port),
                 "start() meldete Erfolg, obwohl der Port belegt ist");

        QVERIFY2(warteBis([&] { return feld->text() != QStringLiteral("port: -"); },
                          2000),
                 "Die Kopfzeile steht weiter auf 'port: -' — der Betreiber "
                 "sieht denselben Text wie bei einem Server, den er selbst "
                 "ausgeschaltet hat");
        QCOMPARE(feld->text(), QStringLiteral("port: blocked"));

        // Der ganze Satz gehört in den Hinweis: Adresse, Port, Grund.
        const QString hinweis = feld->toolTip();
        QVERIFY2(!hinweis.isEmpty(), "Kein Hinweis — 'blocked' ohne Grund hilft nicht");
        QVERIFY2(hinweis.contains(QString::number(port)),
                 qPrintable(QStringLiteral("Der Port fehlt im Hinweis: %1").arg(hinweis)));
        QVERIFY2(hinweis.contains(QStringLiteral("127.0.0.1")),
                 qPrintable(QStringLiteral("Die Adresse fehlt im Hinweis: %1").arg(hinweis)));

        // Frei werden lassen: der Server holt den Port nach, und die
        // Kopfzeile muss den Grund VERGESSEN -- sonst stünde „blocked" über
        // einem laufenden Server.
        blockierer.close();
        QVERIFY2(warteBis([&] { return server.isRunning(); }, 30000),
                 "Der Server hat den freigewordenen Port nicht nachgeholt");
        QVERIFY2(warteBis([&] { return feld->text() != QStringLiteral("port: blocked"); },
                          2000),
                 "Die Kopfzeile steht weiter auf 'blocked', obwohl der Server laeuft");
        QCOMPARE(feld->text(), QStringLiteral("port: %1").arg(port));
        QVERIFY2(feld->toolTip().isEmpty(),
                 "Der alte Grund haengt noch im Hinweis");

        server.stop();
    }

    /** Selbst ausgeschaltet ist kein Fehler: dann steht wieder „port: -". */
    void regulaeresStoppenLoeschtDenGrund()
    {
        QTcpServer blockierer;
        QVERIFY2(blockierer.listen(QHostAddress::LocalHost, 0), "kein Port");
        const quint16 port = blockierer.serverPort();

        TciServer server(nullptr);
        TciApplet applet(&server, nullptr);
        QLabel* feld = portFeld(applet);
        QVERIFY(feld != nullptr);

        QVERIFY(!server.start(QHostAddress(QHostAddress::LocalHost), port));
        QVERIFY2(warteBis([&] { return feld->text() == QStringLiteral("port: blocked"); },
                          2000),
                 "Der Grund kam nicht an");

        server.stop();
        QVERIFY2(warteBis([&] { return feld->text() == QStringLiteral("port: -"); },
                          2000),
                 "Nach dem Ausschalten steht weiter 'blocked' da");
        QVERIFY(feld->toolTip().isEmpty());
    }
};

QTEST_MAIN(TestTciBindefehlerImApplet)
#include "tst_tci_bindefehler_im_applet.moc"

#else   // !HAVE_WEBSOCKETS
int main() { return 0; }
#endif
