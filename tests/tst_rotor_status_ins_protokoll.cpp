// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_rotor_status_ins_protokoll.cpp  (Longpath)
// =================================================================
// Der Rotor schreibt seinen Zustand ins Protokoll — einmal je Wechsel.
//
// Am 2026-10-11 stand in den Protokollen des Betreibers keine einzige
// Zeile darueber, ob der Rotor verbunden war. "Rotator connected"
// liess sich nur ueber den Widget-Baum der laufenden Instanz ablesen
// (Automatisierungsbruecke, dumpTree). In einem Protokoll, das sonst
// jede Verbindung mitschreibt, fehlte genau die Zeile, die man beim
// Nachsehen braucht.
//
// Zwei Dinge sind zu pruefen, und das zweite ist der Grund, warum
// diese Zeile nicht einfach in setStatus() steht:
//   1. der Wechsel erscheint ueberhaupt,
//   2. er erscheint GENAU EINMAL. Das Statusschild wird bei jedem
//      Abfragedurchgang neu gesetzt (zweimal je Sekunde), nicht nur
//      wenn sich etwas aendert — ohne Entprellung liefe das Protokoll
//      voll.
//
// Modification history (Longpath):
//   2026-10-11 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>

#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

#include "core/AppSettings.h"
#include "gui/widgets/RotorLogbookPanel.h"

using namespace Longpath;

namespace {

QStringList g_zeilen;
QtMessageHandler g_vorher = nullptr;

void sammle(QtMsgType typ, const QMessageLogContext& ctx, const QString& text)
{
    if (ctx.category && QByteArray(ctx.category) == "longpath.rotor") {
        g_zeilen << text;
    }
    if (g_vorher) { g_vorher(typ, ctx, text); }
}

} // namespace

class TestRotorStatusInsProtokoll : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()   { g_vorher = qInstallMessageHandler(sammle); }
    void cleanupTestCase() { qInstallMessageHandler(g_vorher); }

    void jederWechselEinmal()
    {
        // Ersatz-rotctld: auf "p" zwei Zahlen, sonst die Erfolgsmeldung.
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        QList<QTcpSocket*> angenommen;
        connect(&server, &QTcpServer::newConnection, this, [&]() {
            QTcpSocket* c = server.nextPendingConnection();
            angenommen << c;
            connect(c, &QTcpSocket::readyRead, c, [c]() {
                const QByteArray ein = c->readAll();
                for (const QByteArray& zeile : ein.split('\n')) {
                    const QByteArray z = zeile.trimmed();
                    if (z.isEmpty()) { continue; }
                    if (z.startsWith('p')) { c->write("231.000000\n0.000000\n"); }
                    else                   { c->write("RPRT 0\n"); }
                }
                c->flush();
            });
        });

        AppSettings& s = AppSettings::instance();
        s.setValue(QStringLiteral("RotorUseLocalSerial"), false);
        s.setValue(QStringLiteral("RotorRotctldHost"), QStringLiteral("127.0.0.1"));
        s.setValue(QStringLiteral("RotorRotctldPort"), int(server.serverPort()));

        g_zeilen.clear();
        auto* panel = new RotorLogbookPanel(nullptr, nullptr, nullptr);
        QString err;
        QVERIFY2(panel->connectRotorFromSettings(&err), qPrintable(err));

        // 1. Der Wechsel steht im Protokoll.
        QTRY_VERIFY_WITH_TIMEOUT(
            std::any_of(g_zeilen.cbegin(), g_zeilen.cend(),
                        [](const QString& z) {
                            return z.contains(QStringLiteral("Rotator connected"));
                        }), 8000);

        // 2. Und zwar genau einmal, obwohl zweimal je Sekunde abgefragt
        //    wird. Drei Sekunden sind rund sechs Durchgaenge.
        QTest::qWait(3000);
        const int verbunden =
            int(std::count_if(g_zeilen.cbegin(), g_zeilen.cend(),
                              [](const QString& z) {
                                  return z.contains(QStringLiteral("Rotator connected"));
                              }));
        QCOMPARE(verbunden, 1);

        delete panel;
    }
};

QTEST_MAIN(TestRotorStatusInsProtokoll)
#include "tst_rotor_status_ins_protokoll.moc"
