// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-eigener Prueffall; die Modustabellen stammen aus
// Thetis und sind in src/core/CatModeMap.h zitiert.
//
// CAT, dritter Gang: die Verdrahtung (2026-10-11); seit 2026-10-12 auch
// die serielle Haelfte (PTY) am selben Fenster.
//
// NUR EIN MainWindow JE PROZESS -- und das ist keine Stilfrage.
//
// Zwei MainWindow hintereinander in einem Prozess beenden Longpath mit
// SIGTRAP: "BUG IN CLIENT OF LIBMALLOC: memory corruption of free block",
// Absturz im Qt-Stylesheet-Parser (QCss::Parser::parseExpr, aus
// QStyleSheetStyle::polish beim Aufbau des ZWEITEN Fensters). Am
// 2026-10-11 mit einem Wegwerf-Prueffall nachgemessen, der NUR drei
// MainWindow baut und sonst nichts tut -- ohne eine Zeile CAT: erstes
// Fenster gruen, zweites toetet den Prozess. Der Fehler ist also
// vorbestehend und gehoert nicht hierher; er ist als eigene Aufgabe
// vorgemerkt.
//
// Folge fuer diese Datei: alle Faelle, die ein Fenster brauchen, teilen
// EINES. Wer hier einen zweiten Aufbau einbaut, bekommt einen stillen
// Abbruch ohne Fehlermeldung -- die Fallbeschreibungen sind bis dahin
// gruen, und danach kommt nichts mehr.
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
#include <QLabel>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>

#include <optional>

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
#include <fcntl.h>
#include <unistd.h>
#endif

#include "core/AppSettings.h"
#include "core/CatModeMap.h"
#include "core/CatPtyServer.h"
#include "core/CatServer.h"
#include "gui/MainWindow.h"
#include "gui/applets/CatApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TstCatVerdrahtung : public QObject {
    Q_OBJECT

private:
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    /// Ein Fremdprogramm, das den Geraetepfad oeffnet -- so, wie fldigi
    /// es tut. Setzt KEINEN Rohmodus: den soll Longpath mitbringen.
    /// Gibt, was ankam (gewartet wird auf Ereignisse, nicht auf die Uhr).
    QByteArray fragePty(int fd, const QByteArray& befehl,
                        int mindestens = 1, int msec = 5000)
    {
        if (::write(fd, befehl.constData(),
                    static_cast<size_t>(befehl.size())) < 0) { return {}; }
        QByteArray a;
        QElapsedTimer uhr;
        uhr.start();
        while (a.count(';') < mindestens && uhr.elapsed() < msec) {
            // Die Ereignisschleife MUSS drehen: Longpath liest ueber
            // einen QSocketNotifier, und Fenster und Fremdprogramm
            // sitzen in DIESEM Prozess auf demselben Faden.
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            char roh[512];
            for (;;) {
                const ssize_t n = ::read(fd, roh, sizeof(roh));
                if (n > 0) { a.append(roh, static_cast<int>(n)); continue; }
                break;
            }
        }
        return a;
    }
#endif

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
    void initTestCase()
    {
        // PFLICHT, und zwar aus zwei Gruenden.
        //
        // 1. Ohne Testmodus liest und SCHREIBT AppSettings die echte
        //    Einstellungsdatei des Betreibers (~1,9 MB, 9531 Eintraege,
        //    sein Layoutprofil, seine Container). Ein Prueffall, der
        //    dort hineinschreibt, kann einen Arbeitsplatz verstellen.
        // 2. Dieser Prueffall baut MEHRERE MainWindow hintereinander.
        //    Mit der echten Datei beendete der zweite Aufbau den
        //    Prozess mit SIGTRAP (rc=133) -- die Fallbeschreibungen
        //    waren gruen bis dahin, es gab keine Fehlermeldung, nur
        //    einen stillen Abbruch nach dem vierten Fall.
        //
        // tst_rx_equalizer.cpp hat die Zeile von Anfang an; diese drei
        // CAT-Prueffaelle nicht, und das ist am 2026-10-11 aufgefallen.
        QStandardPaths::setTestModeEnabled(true);
    }

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

    void amEchtenFenster()
    {
        // Ein Fenster, vier Fragen -- siehe die Notiz im Kopf: ein
        // zweiter Aufbau toetet den Prozess (vorbestehender Fehler).
        auto* mw = new MainWindow();
        mw->resize(1200, 800);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));

        // ── 1. Server und Feld sind da und verbunden ─────────────────────
        const QList<CatServer*> server = mw->findChildren<CatServer*>();
        QCOMPARE(server.size(), 1);
        const QList<CatApplet*> felder = mw->findChildren<CatApplet*>();
        QCOMPARE(felder.size(), 1);
        CatServer* srv = server.first();
        CatApplet* feld = felder.first();

        QPushButton* tcp = nullptr;
        for (QPushButton* b : feld->findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("TCP")) { tcp = b; break; }
        }
        QVERIFY(tcp);
        // Ohne angehaengten Server sperrt setzeCatServer den Schalter --
        // genau das waere der Fall, wenn die Verdrahtung in MainWindow
        // fehlte.
        QVERIFY2(tcp->isEnabled(),
                 "Der TCP-Schalter ist gesperrt -- der Server haengt nicht am Feld.");

        // ── 2. Ohne Empfaenger meldet CAT KEINE Frequenz ─────────────────
        RadioModel* rm = mw->radioModelForTest();
        QVERIFY(rm);
        if (!rm->activeSlice()) {
            // "FA00000000000;" waere eine Angabe, und ein Fremdprogramm
            // glaubt sie. Der Zustandsgeber gibt dafuer std::nullopt.
            QCOMPARE(srv->antwortAuf(QStringLiteral("FA;")),
                     QStringLiteral("?;"));
            QCOMPARE(srv->antwortAuf(QStringLiteral("IF;")),
                     QStringLiteral("?;"));
        }

        // ── 3. Der Schalter startet die Kanaele ──────────────────────────
        //
        // Die Ports VORHER pruefen, nicht hinterher. Hier stand ein QSKIP
        // fuer "Ports belegt, laeuft Longpath schon?" -- die Gegenprobe hat
        // daraufhin die Startzeile im Applet stillgelegt, und der Fall
        // wurde UEBERSPRUNGEN statt zu fallen. Ein echter Fehler als
        // Umgebungsproblem verkleidet.
        QVERIFY(!srv->laeuft(0));
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
            delete mw;
            QSKIP("Die Ports 31001..31004 sind belegt (laeuft Longpath "
                  "schon?) -- das ist die Umgebung, nicht der Code.");
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
        // Und der Schalterzustand steht in den Einstellungen -- sonst
        // muesste der Betreiber nach jedem Start neu klicken, bevor
        // WSJT-X das Geraet findet.
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("CatTcpEnabled")).toString(),
                 QStringLiteral("True"));

        // ── 4. Ein Fremdprogramm stimmt ab ───────────────────────────────
        SliceModel* sl = rm->activeSlice();
        if (!sl) {
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

        // Lesen: was das Fenster zeigt, kommt ueber CAT heraus.
        QCOMPARE(frage(s, "FA;"), QStringLiteral("FA00014225000;"));
        QCOMPARE(frage(s, "MD;"), QStringLiteral("MD2;"));

        // Schreiben: das Fremdprogramm stimmt ab, der Slice folgt.
        s->write("FA00021300000;");
        QTRY_COMPARE(static_cast<quint64>(sl->frequency()), Q_UINT64_C(21300000));
        s->write("MD1;");
        QTRY_COMPARE(sl->dspMode(), DSPMode::LSB);
        // Zurueckgelesen stimmt es auch -- der Zustandsgeber liest frisch,
        // nicht aus einer Kopie.
        QCOMPARE(frage(s, "FA;"), QStringLiteral("FA00021300000;"));
        QCOMPARE(frage(s, "MD;"), QStringLiteral("MD1;"));

        const QString iff = frage(s, "IF;");
        QCOMPARE(iff.size(), 2 + 35 + 1);
        QCOMPARE(iff.mid(2, 11), QStringLiteral("00021300000"));
        QCOMPARE(iff.mid(2 + 27, 1), QStringLiteral("1"));   // LSB

        // Aus: danach horcht niemand mehr, und die Einstellung zieht mit.
        tcp->click();
        for (int k = 0; k < CatServer::kKanaele; ++k) { QVERIFY(!srv->laeuft(k)); }
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("CatTcpEnabled")).toString(),
                 QStringLiteral("False"));

        // ── 5. Dieselbe Pruefung fuer die serielle Haelfte (2026-10-12) ──
        //
        // Am SELBEN Fenster, weil ein zweiter Aufbau den Prozess toetet
        // (siehe die Notiz im Kopf).
        const QList<CatPtyServer*> ptyServer = mw->findChildren<CatPtyServer*>();
        QCOMPARE(ptyServer.size(), 1);
        CatPtyServer* pty = ptyServer.first();

        QPushButton* ptyBtn = nullptr;
        for (QPushButton* b : feld->findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("PTY")) { ptyBtn = b; break; }
        }
        QVERIFY(ptyBtn);

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
        // Das ist der Fang fuer die GANZE Verdrahtung: ohne
        // setzeCatPtyServer() in MainWindow sperrt das Applet den
        // Schalter, und bis 2026-10-12 trug er obendrein eine
        // NyiOverlay-Marke.
        QVERIFY2(ptyBtn->isEnabled(),
                 "Der PTY-Schalter ist gesperrt -- der PTY-Server haengt "
                 "nicht am Feld.");

        // Vor dem Druecken: kein Kanal, und in den Beschriftungen steht
        // ein Strich -- KEIN erfundener Pfad. Hier stand bis 2026-10-12
        // "/dev/ptyp0".."/dev/ptyp3".
        for (int k = 0; k < CatPtyServer::kKanaele; ++k) {
            QVERIFY(!pty->laeuft(k));
        }
        for (QLabel* l : feld->findChildren<QLabel*>()) {
            QVERIFY2(!l->text().contains(QStringLiteral("ptyp")),
                     "Im Feld steht noch ein /dev/ptyp-Altname.");
        }

        // Und dasselbe fuer ein Feld OHNE PTY-Server. Das ist die Stelle,
        // an der der Altname ueberlebt haette: `zeigePfade()` deckt ihn
        // zu, sobald setzeCatPtyServer() gelaufen ist -- die Gegenprobe
        // am 2026-10-12 hat den Platzhalter zurueckgesetzt, und ALLE
        // Faelle blieben gruen. Erst ein Feld, an dem nie ein Server
        // hing (ein zweiter Behaelter, ein kuenftiger Pfad), zeigt ihn.
        //
        // Ein zweites CatApplet ist erlaubt -- nur ein zweites
        // MainWindow toetet den Prozess.
        {
            auto* nackt = new CatApplet(rm, nullptr);
            bool strichGefunden = false;
            for (QLabel* l : nackt->findChildren<QLabel*>()) {
                QVERIFY2(!l->text().contains(QStringLiteral("ptyp")),
                         qPrintable(QStringLiteral(
                             "Ein Feld ohne PTY-Server zeigt den Altnamen "
                             "%1 - einen Pfad, den niemand oeffnen kann.")
                                 .arg(l->text())));
                if (l->text() == QStringLiteral("\u2014")) { strichGefunden = true; }
            }
            QVERIFY2(strichGefunden,
                     "Ein Feld ohne PTY-Server zeigt gar keinen Platzhalter.");
            delete nackt;
        }

        ptyBtn->click();
        for (int k = 0; k < CatPtyServer::kKanaele; ++k) {
            QVERIFY2(pty->laeuft(k),
                     qPrintable(QStringLiteral("PTY-Kanal %1 laeuft nicht: %2")
                                    .arg(k).arg(pty->grund(k))));
            QVERIFY(!pty->pfad(k).isEmpty());
        }
        QVERIFY(ptyBtn->isChecked());
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("CatPtyEnabled")).toString(),
                 QStringLiteral("True"));

        // Die Pfade stehen auch IM FELD -- das war der Auftrag: "sonst
        // findet sie niemand". Gesucht wird der kurze Geraetename, so
        // wie zeigePfade() ihn setzt.
        {
            const QString kurz = pty->pfad(0).section(QLatin1Char('/'), -1);
            bool gefunden = false;
            for (QLabel* l : feld->findChildren<QLabel*>()) {
                if (l->text() == kurz) { gefunden = true; break; }
            }
            QVERIFY2(gefunden,
                     qPrintable(QStringLiteral("Der Geraetename %1 steht in "
                                               "keiner Beschriftung").arg(kurz)));
        }

        // Und jetzt der eigentliche Beleg: ein Fremdprogramm oeffnet den
        // Geraetepfad und stimmt das ECHTE Fenster ab.
        const int fd = ::open(pty->pfad(0).toLocal8Bit().constData(),
                              O_RDWR | O_NOCTTY | O_NONBLOCK);
        QVERIFY2(fd >= 0,
                 qPrintable(QStringLiteral("%1 laesst sich nicht oeffnen")
                                .arg(pty->pfad(0))));

        // Lesen -- und GENAU das, nichts davor: ohne Rohmodus spiegelt
        // die Leitungsdisziplin die Antwort zurueck, Longpath liest sie
        // als Befehl und laeuft im Kreis.
        QCOMPARE(fragePty(fd, "FA;"), QByteArray("FA00021300000;"));
        QCOMPARE(fragePty(fd, "MD;"), QByteArray("MD1;"));

        // Schreiben: der Slice im laufenden Fenster folgt.
        QVERIFY(::write(fd, "FA00050100000;", 14) == 14);
        QTRY_COMPARE(static_cast<quint64>(sl->frequency()), Q_UINT64_C(50100000));
        // Kenwood-Modus 3 ist CWU, nicht CWL -- CWL ist die 7
        // (CatModeMap.h:54, aus Thetis CATCommands.cs). Hier zuerst die
        // falsche Erwartung gehabt und vom Prueffall korrigiert bekommen.
        QVERIFY(::write(fd, "MD3;", 4) == 4);
        QTRY_COMPARE(sl->dspMode(), DSPMode::CWU);
        QCOMPARE(fragePty(fd, "MD;"), QByteArray("MD3;"));
        // Und die Gegenrichtung: 7 muss CWL geben, nicht wieder CWU.
        QVERIFY(::write(fd, "MD7;", 4) == 4);
        QTRY_COMPARE(sl->dspMode(), DSPMode::CWL);
        QCOMPARE(fragePty(fd, "MD;"), QByteArray("MD7;"));

        // Beide Wege sehen denselben Zustand -- EINE Protokollschicht.
        QCOMPARE(srv->antwortAuf(QStringLiteral("FA;")),
                 QStringLiteral("FA00050100000;"));

        ::close(fd);

        // Aus: kein Kanal mehr, in den Beschriftungen wieder der Strich,
        // und die Einstellung zieht mit.
        ptyBtn->click();
        for (int k = 0; k < CatPtyServer::kKanaele; ++k) {
            QVERIFY(!pty->laeuft(k));
            QVERIFY(pty->pfad(k).isEmpty());
        }
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("CatPtyEnabled")).toString(),
                 QStringLiteral("False"));
#else
        // Auf Windows gibt es keine Pseudoterminals. Geprueft wird dann,
        // dass der Schalter GESPERRT ist und sagt, warum -- nicht, dass
        // er sich druecken laesst und nichts tut.
        QVERIFY(!CatPtyServer::verfuegbar());
        QVERIFY(!ptyBtn->isEnabled());
        QVERIFY(!ptyBtn->toolTip().isEmpty());
#endif

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

        QCOMPARE(srv.antwortAuf(QStringLiteral("FB00007100000;")),
                 QStringLiteral("?;"));
        QCOMPARE(srv.antwortAuf(QStringLiteral("FT1;")),
                 QStringLiteral("?;"));
        // Was es kann, geht weiter.
        QCOMPARE(srv.antwortAuf(QStringLiteral("FA;")),
                 QStringLiteral("FA00014225000;"));
    }
};

QTEST_MAIN(TstCatVerdrahtung)
#include "tst_cat_verdrahtung.moc"
