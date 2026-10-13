// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_rotor_panel_abbau.cpp  (Longpath)
// =================================================================
// Das Rotor-Feld abbauen, waehrend der Rotor verbunden ist.
//
// Der Betreiber hatte den Rotor am 2026-10-10 wieder im Netz — und
// Longpath stuerzte danach bei JEDEM Beenden ab: sechs
// Absturzberichte zwischen 2026-10-10 16:05 und 2026-10-11 11:08,
// alle mit demselben Weg:
//
//   MainWindow::closeEvent -> ~ToolWindow -> ~RotorLogbookPanel
//     -> QObjectPrivate::deleteChildren -> ~RotctldClient
//     -> ~QAbstractSocket -> disconnectFromHost()
//     -> RotorController::stateChanged(Disconnected)
//     -> das Lambda aus ensureRotor() -> setStatus()
//     -> QLabel::setText  == SIGSEGV
//
// Es ist die Fehlerform, die in diesem Programm schon vier Mal
// steckte (SpeConnection, MainWindow, PgxlConnection,
// TgxlConnection): ein Rueckruf laeuft noch, weil die QObject-Basis
// lebt, und greift in Mitglieder, die der Abbau schon weggeraeumt
// hat. Hier ist der Socket kein Mitglied, sondern Enkelkind — die
// Wirkung ist dieselbe, denn ~QWidget loescht die Kinder in der
// Reihenfolge, in der sie angelegt wurden: das Statusschild (frueh)
// liegt dann schon tot da, wenn der Rotctld-Client (spaet) seinen
// Socket abbaut.
//
// Dieser Pruefstand fuehrt genau diesen Abbau herbei: ein
// Ersatz-rotctld am Ort (QTcpServer), eine WIRKLICH stehende
// Verbindung — ohne sie meldet ~QTcpSocket nichts und der Pruefstand
// prueft nichts — und dann delete auf das Feld.
//
// Gegenprobe gefahren (Regel: gruen mit der Behebung belegt nichts):
// gegen die Fassung ohne ~RotorLogbookPanel() faellt er mit
// SIGSEGV, mit ihr laeuft er durch.
//
// Modification history (Longpath):
//   2026-10-11 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>

#include <QHostAddress>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>

#include "core/AppSettings.h"
#include "gui/widgets/RotorLogbookPanel.h"

using namespace Longpath;

class TestRotorPanelAbbau : public QObject
{
    Q_OBJECT
private slots:
    void dasFeldAbbauenWaehrendDerRotorHaengt()
    {
        // ── Ersatz-rotctld ───────────────────────────────────────────
        // Nur so viel, dass der Client die Verbindung fuer gut haelt:
        // auf "p" zwei Zahlen, auf alles andere die Erfolgsmeldung.
        QTcpServer server;
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "der Ersatz-rotctld muss horchen koennen");
        QList<QTcpSocket*> angenommen;
        connect(&server, &QTcpServer::newConnection, this, [&]() {
            QTcpSocket* c = server.nextPendingConnection();
            angenommen << c;
            connect(c, &QTcpSocket::readyRead, c, [c]() {
                const QByteArray ein = c->readAll();
                for (const QByteArray& zeile : ein.split('\n')) {
                    const QByteArray z = zeile.trimmed();
                    if (z.isEmpty()) { continue; }
                    if (z.startsWith('p')) {
                        c->write("231.000000\n0.000000\n");
                    } else {
                        c->write("RPRT 0\n");
                    }
                }
                c->flush();
            });
        });

        AppSettings& s = AppSettings::instance();
        s.setValue(QStringLiteral("RotorUseLocalSerial"), false);
        s.setValue(QStringLiteral("RotorRotctldHost"),
                   QStringLiteral("127.0.0.1"));
        s.setValue(QStringLiteral("RotorRotctldPort"),
                   int(server.serverPort()));

        auto* panel = new RotorLogbookPanel(nullptr, nullptr, nullptr);
        QString err;
        QVERIFY2(panel->connectRotorFromSettings(&err),
                 qPrintable(QStringLiteral("Verbinden abgelehnt: %1").arg(err)));

        // Ohne stehende Verbindung prueft der Abbau nichts.
        QTRY_VERIFY_WITH_TIMEOUT(!angenommen.isEmpty(), 5000);
        QTest::qWait(400);

        QPointer<RotorLogbookPanel> wacht(panel);
        delete panel;              // hier starb 0.6.8 sechs Mal
        QVERIFY(wacht.isNull());

        // Nachzuegler: alles, was der Abbau noch in die Schlange
        // gelegt hat, muss durchlaufen, ohne etwas Totes anzufassen.
        QTest::qWait(300);
    }
};

QTEST_MAIN(TestRotorPanelAbbau)
#include "tst_rotor_panel_abbau.moc"
