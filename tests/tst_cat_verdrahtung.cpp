// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-eigener Prueffall; die Modustabellen stammen aus
// Thetis und sind in src/core/CatModeMap.h zitiert.
//
// CAT, dritter Gang: die Verdrahtung (2026-10-11).
//
// Geprueft wird am ECHTEN MainWindow mit ECHTEN TCP-Verbindungen. Die
// Protokollschicht (tst_cat_protokoll) und der Transport
// (tst_cat_server) sind fuer sich gruen -- was hier schiefgehen kann,
// sieht keiner von beiden: ein Zustandsgeber, der den falschen Slice
// liest; eine Modusumrechnung, die in einer Richtung stimmt und in der
// anderen nicht; ein Applet, dessen Schalter nichts startet; ein
// Befehl, der quittiert wird, obwohl Longpath ihn nicht kann.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>

#include <optional>

#include "core/CatModeMap.h"
#include "core/CatServer.h"
#include "gui/MainWindow.h"
#include "gui/applets/CatApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TstCatVerdrahtung : public QObject {
    Q_OBJECT

private:
    QString frage(QTcpSocket* s, const QByteArray& befehl,
                  int mindestens = 1, int msec = 5000)
    {
        s->write(befehl);
        QByteArray a;
        QElapsedTimer uhr;
        uhr.start();
        while (a.count(';') < mindestens && uhr.elapsed() < msec) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            if (s->bytesAvailable() > 0) { a += s->readAll(); }
        }
        return QString::fromLatin1(a);
    }

private slots:
    // ── Die Modustabellen ───────────────────────────────────────────────────

    void dieModusumrechnungGehtInBeideRichtungen()
    {
        // Eine Tabelle, die nur in EINER Richtung stimmt, ist die
        // haesslichste Form dieses Fehlers: ein Fremdprogramm setzt USB,
        // liest USB zurueck, und das Geraet steht auf DIGU. Darum stehen
        // beide Richtungen in einer Datei und werden hier gegeneinander
        // geprueft.
        const struct { DSPMode m; int k; } paare[] = {
            { DSPMode::LSB,  1 }, { DSPMode::USB,  2 }, { DSPMode::CWU, 3 },
            { DSPMode::FM,   4 }, { DSPMode::AM,   5 }, { DSPMode::DIGL, 6 },
            { DSPMode::CWL,  7 }, { DSPMode::DIGU, 9 },
        };
        for (const auto& pr : paare) {
            QCOMPARE(catKenwoodZahl(pr.m), pr.k);
            QCOMPARE(catModusAusKenwoodZahl(pr.k), pr.m);
        }
    }

    void unbekannteModiWerdenUsb()
    {
        // From Thetis CATCommands.cs: Mode2KString gibt seinen Fehlerwert
        // und IF() setzt dann "2"; KString2Mode hat `default: USB`.
        // Longpaths SAM/DRM/SPEC/RADE kennt Kenwood nicht.
        QCOMPARE(catKenwoodZahl(DSPMode::SAM),    2);
        QCOMPARE(catKenwoodZahl(DSPMode::DRM),    2);
        QCOMPARE(catKenwoodZahl(DSPMode::RADE_U), 2);
        // Die 8 ist in Kenwoods Satz FSK-R und bei Thetis unbesetzt.
        QCOMPARE(catModusAusKenwoodZahl(8),  DSPMode::USB);
        QCOMPARE(catModusAusKenwoodZahl(0),  DSPMode::USB);
        QCOMPARE(catModusAusKenwoodZahl(99), DSPMode::USB);
    }

    // ── Am echten Fenster ───────────────────────────────────────────────────

    void dasFensterBautServerUndFeldUndVerbindetSie()
    {
        auto* mw = new MainWindow();
        mw->resize(1200, 800);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));

        // Beides da, und genau einmal.
        const QList<CatServer*> server = mw->findChildren<CatServer*>();
        QCOMPARE(server.size(), 1);
        const QList<CatApplet*> felder = mw->findChildren<CatApplet*>();
        QCOMPARE(felder.size(), 1);

        // Der TCP-Schalter ist BEDIENBAR -- ohne angehaengten Server
        // sperrt setzeCatServer ihn, und genau das waere der Fall, wenn
        // die Verdrahtung in MainWindow fehlte.
        QPushButton* tcp = nullptr;
        for (QPushButton* b : felder.first()->findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("TCP")) { tcp = b; break; }
        }
        QVERIFY(tcp);
        QVERIFY2(tcp->isEnabled(),
                 "Der TCP-Schalter ist gesperrt -- der Server haengt nicht am Feld.");
        delete mw;
    }

    void derSchalterStartetDieKanaeleUndEinProgrammKannAbstimmen()
    {
        auto* mw = new MainWindow();
        mw->resize(1200, 800);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));

        auto* srv = mw->findChild<CatServer*>();
        auto* feld = mw->findChild<CatApplet*>();
        QVERIFY(srv);
        QVERIFY(feld);
        QPushButton* tcp = nullptr;
        for (QPushButton* b : feld->findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("TCP")) { tcp = b; break; }
        }
        QVERIFY(tcp);

        QVERIFY(!srv->laeuft(0));

        // Die Ports VORHER pruefen, nicht hinterher.
        //
        // Hier stand ein QSKIP fuer den Fall, dass kein Kanal hochkommt
        // ("Ports belegt, laeuft Longpath schon?"). Die Gegenprobe am
        // 2026-10-11 hat daraufhin die Startzeile im Applet stillgelegt
        // -- und der Fall wurde UEBERSPRUNGEN statt zu fallen. Ein
        // echter Fehler als Umgebungsproblem verkleidet; genau die
        // Form, die hier schon oefter Zeit gekostet hat.
        //
        // Jetzt wird zuerst gemessen, ob die vier Ports ueberhaupt zu
        // haben sind. Sind sie es nicht, ist das die Umgebung und der
        // Fall wird uebersprungen. Sind sie es, MUSS der Klick sie
        // starten -- ohne Ausweg.
        bool portsFrei = true;
        {
            QList<QTcpServer*> probe;
            for (int k = 0; k < CatServer::kKanaele; ++k) {
                auto* t = new QTcpServer();
                if (!t->listen(QHostAddress::LocalHost,
                               static_cast<quint16>(31001 + k))) {
                    portsFrei = false;
                }
                probe.append(t);
            }
            qDeleteAll(probe);
        }
        if (!portsFrei) {
            QSKIP("Die Ports 31001..31004 sind auf dieser Maschine belegt "
                  "(laeuft Longpath schon?) -- das ist die Umgebung, nicht "
                  "der Code.");
        }

        tcp->click();
        // Kein Ausweg mehr: die Ports waren frei, also muessen alle vier
        // Kanaele horchen.
        for (int k = 0; k < CatServer::kKanaele; ++k) {
            QVERIFY2(srv->laeuft(k),
                     qPrintable(QStringLiteral("Kanal %1 horcht nicht, obwohl "
                                               "sein Port frei war").arg(k)));
        }
        QVERIFY(tcp->isChecked());

        RadioModel* rm = mw->radioModelForTest();
        QVERIFY(rm);
        SliceModel* sl = rm->activeSlice();
        if (!sl) {
            // Ein frisch gebautes Fenster hat noch keinen Empfaenger --
            // genau der Zustand, in dem CAT nichts melden DARF (eigener
            // Fall unten). Fuer die Abstimmprobe wird einer angelegt.
            rm->configureStreamPool(5, 5, 192000);
            const int id = rm->addSlice();
            sl = rm->sliceById(id);
        }
        QVERIFY(sl);
        sl->setDspMode(DSPMode::USB);
        sl->setFrequency(14225000.0);

        auto* s = new QTcpSocket(this);
        s->connectToHost(QHostAddress::LocalHost, srv->port(0));
        QVERIFY(s->waitForConnected(5000));

        // Lesen: was das Fenster zeigt, muss ueber CAT herauskommen.
        QCOMPARE(frage(s, "FA;"), QStringLiteral("FA00014225000;"));
        QCOMPARE(frage(s, "MD;"), QStringLiteral("MD2;"));

        // Schreiben: ein Fremdprogramm stimmt ab -- und der Slice folgt.
        s->write("FA00021300000;");
        QTRY_COMPARE(static_cast<quint64>(sl->frequency()), Q_UINT64_C(21300000));
        s->write("MD1;");
        QTRY_COMPARE(sl->dspMode(), DSPMode::LSB);
        // Und zurueckgelesen stimmt es auch -- der Zustandsgeber liest
        // frisch, nicht aus einer Kopie.
        QCOMPARE(frage(s, "FA;"), QStringLiteral("FA00021300000;"));
        QCOMPARE(frage(s, "MD;"), QStringLiteral("MD1;"));

        // Der Statussatz traegt dieselbe Frequenz.
        const QString iff = frage(s, "IF;");
        QCOMPARE(iff.size(), 2 + 35 + 1);
        QCOMPARE(iff.mid(2, 11), QStringLiteral("00021300000"));
        QCOMPARE(iff.mid(2 + 27, 1), QStringLiteral("1"));   // LSB

        // Aus: danach horcht niemand mehr.
        tcp->click();
        for (int k = 0; k < CatServer::kKanaele; ++k) { QVERIFY(!srv->laeuft(k)); }
        delete mw;
    }

    void ohneEmpfaengerMeldetCatKeineFrequenz()
    {
        // Ein frisch gebautes Fenster hat keinen aktiven Slice. Dann
        // darf CAT KEINE Frequenz melden -- "FA00000000000;" ist eine
        // Angabe, und ein Fremdprogramm glaubt sie. Der Zustandsgeber in
        // MainWindow gibt dafuer std::nullopt.
        auto* mw = new MainWindow();
        mw->resize(1200, 800);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
        auto* srv = mw->findChild<CatServer*>();
        QVERIFY(srv);
        RadioModel* rm = mw->radioModelForTest();
        QVERIFY(rm);
        if (rm->activeSlice()) {
            QSKIP("Dieses Fenster hat schon einen Empfaenger -- der Fall "
                  "prueft gerade den Zustand davor.");
        }
        QCOMPARE(srv->verarbeiteFuerTest(QStringLiteral("FA;")),
                 QStringLiteral("?;"));
        QCOMPARE(srv->verarbeiteFuerTest(QStringLiteral("IF;")),
                 QStringLiteral("?;"));
        delete mw;
    }

    void wasLongpathNichtKannWirdAbgelehntNichtQuittiert()
    {
        // Longpath hat kein VFO B und keinen Split -- RadioModel::split()
        // gibt bewusst immer false (Phase-3F-Entwurf §3). Die Haken dafuer
        // bleiben leer, und die Protokollschicht lehnt dann ab.
        //
        // Der Unterschied ist nicht Kosmetik: ein quittiertes FB laesst
        // ein Fremdprogramm glauben, es haette den zweiten VFO gesetzt --
        // und es sendet dann dort.
        CatServer srv;
        srv.setzeZustandsgeber([]() {
            CatProtocol::Zustand z;
            z.vfoAHz = 14225000;
            return z;
        });
        CatProtocol::Haken h;
        h.setzeVfoA = [](quint64) {};
        srv.setzeHaken(h);          // kein setzeVfoB, kein setzeSplit

        QCOMPARE(srv.verarbeiteFuerTest(QStringLiteral("FB00007100000;")),
                 QStringLiteral("?;"));
        QCOMPARE(srv.verarbeiteFuerTest(QStringLiteral("FT1;")),
                 QStringLiteral("?;"));
        // Was es kann, geht weiter.
        QCOMPARE(srv.verarbeiteFuerTest(QStringLiteral("FA;")),
                 QStringLiteral("FA00014225000;"));
    }
};

QTEST_MAIN(TstCatVerdrahtung)
#include "tst_cat_verdrahtung.moc"
