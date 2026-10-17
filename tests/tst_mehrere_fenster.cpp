// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-eigener Rueckfallpruefstand.
//
// Mehrere MainWindow hintereinander in EINEM Prozess (2026-10-11).
//
// Anlass: ein CAT-Prueffall baute zwei Fenster und beendete den Prozess
// mit SIGTRAP -- "BUG IN CLIENT OF LIBMALLOC: memory corruption of free
// block", Absturz im Qt-Stylesheet-Parser beim Aufbau des ZWEITEN. Mit
// einem Wegwerf-Prueffall nachgemessen, der NUR Fenster baut: erstes
// gruen, zweites tot. Es lag also nicht an CAT.
//
// Die Ursache war der Abbau des ERSTEN Fensters: ~MainWindow setzte
// m_shuttingDown nicht, also lief ConnectionPanels destroyed-Haken
// (restoreFloatingWindowsHiddenBehindConnectMask) auf einem halb
// abgebauten Fenster und schrieb in freigegebenen Speicher. Das Fenster
// danach fiel ueber den Schaden -- und zwar an ganz anderer Stelle, was
// die Suche teuer gemacht hat.
//
// WIE SICHER DIESER WAECHTER IST -- gemessen, nicht behauptet:
//
//   ohne den Fix:  Lauf 1 gruen, Lauf 2 rc=133, Lauf 3 rc=133
//   mit dem Fix:   vier Laeufe, alle gruen
//
// Also faengt er es NICHT bei jedem Lauf. Das liegt in der Natur der
// Sache: eine Speicherverfaelschung faellt nur auf, wenn die naechste
// Belegung ueber die beschaedigte Stelle laeuft, und das haengt an der
// Reihenfolge der Freigaben. Derselbe Fehler kam beim Suchen einmal als
// SIGTRAP im Stylesheet-Parser und einmal als SIGSEGV im
// destroyed-Haken heraus.
//
// Wer hier etwas aendert und EINEN gruenen Lauf sieht, hat darum nichts
// belegt -- drei Laeufe, und bei Verdacht die Faelle unter ASAN
// (tests/CMakeLists.txt kennt den Schalter, siehe
// docs/architecture/2026-08-29-sunsdr-tci-teardown-segfault-investigation.md:
// dort hat ASAN genau so einen Fall deterministisch gemacht, wo der
// normale Bau wuerfelte).
//
// Der ERSTE Fall unten (drei nackte Fenster, ohne CAT) faengt den Fehler
// gar nicht -- auch ohne den Fix bleibt er gruen. Er steht trotzdem da,
// weil er die billigste Form der Frage ist; der zweite, mit offener
// Verbindung, ist der, der wirklich misst.

#include <QtTest/QtTest>

#include <QTcpSocket>

#include "core/CatServer.h"
#include "gui/MainWindow.h"

using namespace Longpath;

class TstMehrereFenster : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Ohne Testmodus liest und schreibt AppSettings die echte
        // Einstellungsdatei des Betreibers.
        QStandardPaths::setTestModeEnabled(true);
    }

    void dreiFensterNacheinanderUeberlebenSich()
    {
        for (int runde = 1; runde <= 3; ++runde) {
            auto* mw = new MainWindow();
            mw->resize(1200, 800);
            mw->show();
            QVERIFY2(QTest::qWaitForWindowExposed(mw, 20000),
                     qPrintable(QStringLiteral("Fenster %1 kam nicht hoch")
                                    .arg(runde)));
            // Ausdruecklich OHNE close(): genau der Pfad, der den
            // Abbau-Waechter nicht setzte.
            delete mw;
            // Die Schleife drehen, damit deleteLater-Reste wirklich
            // abgeraeumt werden, bevor das naechste Fenster entsteht.
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
        QVERIFY(true);   // bis hierher gekommen = bestanden
    }

    void einFensterMitOffenerVerbindungAbbauen()
    {
        // Die Konstellation, in der der Absturz zuerst auftrat: ein
        // Fenster mit laufendem CAT-Server und einer offenen Verbindung
        // wird ohne close() abgebaut, danach kommt das naechste Fenster.
        //
        // Mehrere Runden, weil Speicherverfaelschung nicht bei jedem
        // Lauf an derselben Stelle auffaellt -- der erste Fund kam
        // einmal als SIGTRAP im Stylesheet-Parser und einmal als
        // SIGSEGV im destroyed-Haken heraus.
        for (int runde = 1; runde <= 3; ++runde) {
            auto* mw = new MainWindow();
            mw->resize(1200, 800);
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));

            auto* srv = mw->findChild<CatServer*>();
            QVERIFY(srv);
            QTcpSocket* s = nullptr;
            if (srv->starte(0, 0)) {
                s = new QTcpSocket();
                s->connectToHost(QHostAddress::LocalHost, srv->port(0));
                if (s->waitForConnected(3000)) {
                    s->write("IF;");
                    s->flush();
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
                }
            }
            delete mw;            // mitten im Betrieb, ohne close()
            if (s) { s->abort(); delete s; }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
        QVERIFY(true);
    }

    void einFensterMitCloseUndEinesOhne()
    {
        // Beide Pfade in einem Lauf: closeEvent setzt den Waechter
        // selbst, der Destruktor muss es fuer den anderen tun.
        {
            auto* mw = new MainWindow();
            mw->resize(1000, 700);
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
            mw->close();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            delete mw;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        {
            auto* mw = new MainWindow();
            mw->resize(1000, 700);
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
            delete mw;
        }
        QVERIFY(true);
    }
};

QTEST_MAIN(TstMehrereFenster)
#include "tst_mehrere_fenster.moc"
