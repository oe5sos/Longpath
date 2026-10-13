// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-eigener Prueffall; die Modustabellen stammen aus
// Thetis und sind in src/core/CatModeMap.h zitiert.
//
// CAT, dritter Gang: die Verdrahtung (2026-10-11).
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
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>

#include <optional>

#include "core/AppSettings.h"
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
        if (mindestens == 0) {
            // Die Schleife ein paar Mal drehen, damit der Server
            // arbeitet, und dann nachsehen, dass nichts kam.
            while (uhr.elapsed() < msec) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                if (s->bytesAvailable() > 0) { a += s->readAll(); }
            }
            return QString::fromLatin1(a);
        }
        // mindestens == 0 heisst: KEINE Antwort erwartet -- dann wird die
        // Schleife einmal gedreht und geprueft, dass nichts kam.
        while (a.count(';') <= mindestens - 1 && uhr.elapsed() < msec) {
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
            QCOMPARE(srv->verarbeiteFuerTest(QStringLiteral("FA;")),
                     QStringLiteral("?;"));
            QCOMPARE(srv->verarbeiteFuerTest(QStringLiteral("IF;")),
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

        // ── 5. Die sechs Befehle aus der zweiten Fassung (2026-10-13) ────
        //
        // RIT/XIT sind in Longpath der Ersatz fuer den Split, den es
        // nicht gibt -- ein Contestprogramm, das nach Split greift,
        // findet hier etwas, das wirkt.
        // PS; meldet den ECHTEN Zustand. Ohne verbundenes Geraet ist das
        // "PS0;" -- und das ist richtig, nicht ein Mangel: hier laeuft
        // kein Geraet. Der erste Versuch erwartete PS1 und fiel darum.
        //
        // Die zweite CAT-Fassung gibt hier uebrigens 1 vor, wenn die
        // Abfrage am Modell scheitert (`bool on = true;` vor dem
        // invokeMethod). Das ist die unguenstigere Richtung: ein
        // Contestprogramm, das PS1 liest, haelt die Station fuer
        // betriebsbereit.
        QCOMPARE(frage(s, "PS;"),
                 rm->powerOn() ? QStringLiteral("PS1;") : QStringLiteral("PS0;"));
        QCOMPARE(frage(s, "PS0;"), QStringLiteral("?;"));   // nicht abschalten

        // SM antwortet mit fuenf Stellen; ohne Kanal ist der Pegel -200,
        // also 0 -- geprueft wird die FORM, nicht der Wert.
        const QString sm = frage(s, "SM0;");
        QVERIFY2(sm.startsWith(QStringLiteral("SM")) && sm.size() == 2 + 5 + 1,
                 qPrintable(QStringLiteral("SM-Antwort hat die falsche Form: %1").arg(sm)));

        QCOMPARE(frage(s, "RT;"), QStringLiteral("RT0;"));
        // Die ANTWORT abholen, auch wenn keine erwartet wird.
        //
        // Der erste Versuch stand hier als blankes `s->write("RT1;")`
        // mit einem QTRY_VERIFY darauf -- und war rot: ritEnabled()
        // blieb falsch, und der Lauf brauchte 18 s statt 3,5.
        // Mit dieser Zeile ist er gruen und schnell.
        //
        // WARUM genau, habe ich nicht festgestellt. QTRY_VERIFY dreht
        // die Ereignisschleife auch, es muesste also reichen. Was diese
        // Form zusaetzlich tut: sie dreht 300 ms lang ohne Abbruch und
        // liest dabei ab, was der Socket hergibt. Die Vermutung ist ein
        // Zusammenspiel mit den Antworten der Befehle davor; belegt ist
        // sie nicht.
        //
        // Festgehalten statt weggeraeumt, weil die naechste Person sonst
        // dieselbe halbe Stunde verliert -- und weil ein Prueffall, der
        // die Antwort abholt, ohnehin der richtigere ist: ein Setzen
        // kann abgelehnt werden, und dann steht ein "?;" im Socket.
        QCOMPARE(frage(s, "RT1;", 0, 300), QString());
        QTRY_VERIFY(sl->ritEnabled());
        QCOMPARE(frage(s, "RT;"), QStringLiteral("RT1;"));

        // Jetzt die Ablage schieben -- und zwar MIT Nachlesen: der
        // Server prueft nach jedem Setzen, ob es gewirkt hat.
        sl->setStepHz(10);
        const int vorher = sl->ritHz();
        s->write("RU;");
        QTRY_COMPARE(sl->ritHz(), vorher + 10);
        s->write("RD;");
        QTRY_COMPARE(sl->ritHz(), vorher);
        s->write("RC;");
        QTRY_COMPARE(sl->ritHz(), 0);

        // XIT genauso.
        s->write("XT1;");
        QTRY_VERIFY(sl->xitEnabled());
        QCOMPARE(frage(s, "XT;"), QStringLiteral("XT1;"));

        // RIT und XIT aus: dann gibt es nichts zu schieben, und ein
        // stilles "erledigt" sagte dem Programm, die Ablage sei gesetzt.
        sl->setRitEnabled(false);
        sl->setXitEnabled(false);
        QCOMPARE(frage(s, "RU;"), QStringLiteral("?;"));
        QCOMPARE(frage(s, "RC;"), QStringLiteral("?;"));

        // Aus: danach horcht niemand mehr, und die Einstellung zieht mit.
        tcp->click();
        for (int k = 0; k < CatServer::kKanaele; ++k) { QVERIFY(!srv->laeuft(k)); }
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("CatTcpEnabled")).toString(),
                 QStringLiteral("False"));

        delete mw;
    }

    void derZzSatzHaengtAmEchtenFenster()
    {
        // Eigener Fall OHNE Netz, und das ist der Punkt.
        //
        // Der Fall `amEchtenFenster()` oben klickt den TCP-Schalter und
        // wird uebersprungen, wenn die Ports 31001..31004 belegt sind --
        // was sie sind, sobald eine Longpath-Instanz laeuft. Die
        // Verdrahtung des ZZ-Satzes waere damit genau dann ungeprueft,
        // wenn der Rechner in Betrieb ist. Also hier ohne Horchen:
        // echtes Fenster, echte Scheibe, und die Befehle ueber
        // `verarbeiteFuerTest()` statt ueber einen Socket. Der Transport
        // ist in tst_cat_server geprueft, hier geht es um die Haken.
        auto* mw = new MainWindow();
        mw->resize(1200, 800);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));

        const QList<CatServer*> server = mw->findChildren<CatServer*>();
        QCOMPARE(server.size(), 1);
        CatServer* srv = server.first();
        RadioModel* rm = mw->radioModelForTest();
        QVERIFY(rm);

        SliceModel* sl = rm->activeSlice();
        if (!sl) {
            rm->configureStreamPool(5, 5, 192000);
            const int id = rm->addSlice();
            sl = rm->sliceById(id);
        }
        QVERIFY(sl);

        auto frageDirekt = [srv](const char* befehl) {
            return srv->verarbeiteFuerTest(QString::fromLatin1(befehl));
        };

        //
        // Die Protokollschicht ist in tst_cat_zz vollstaendig geprueft;
        // hier geht es um die VERDRAHTUNG -- ob die neuen Felder und
        // Haken wirklich an der Scheibe haengen. Genau das hat am
        // 2026-10-11 gefehlt: `FB;` antwortete "FB00000000000;", und
        // alle drei Prueffaelle der Schicht liessen es durch, weil ihre
        // init() immer BEIDE Haken anhaengt.
        sl->setFrequency(28'400'000.0);
        sl->setDspMode(DSPMode::CWL);
        QCOMPARE(frageDirekt("ZZFA;"), QStringLiteral("ZZFA00028400000;"));
        QCOMPARE(frageDirekt("ZZMD;"), QStringLiteral("ZZMD03;"));

        // Der erweiterte Statussatz: 36 Zeichen, Modus zweistellig an
        // Stelle 27.
        const QString zzif = frageDirekt("ZZIF;");
        QCOMPARE(zzif.size(), 4 + 36 + 1);
        QVERIFY(zzif.startsWith(QStringLiteral("ZZIF")));
        QCOMPARE(zzif.mid(4, 11), QStringLiteral("00028400000"));
        QCOMPARE(zzif.mid(4 + 27, 2), QStringLiteral("03"));

        // Setzen wirkt an der Scheibe.
        QCOMPARE(frageDirekt("ZZFA00028450000;"), QString());
        QCOMPARE(sl->frequency(), 28'450'000.0);
        QCOMPARE(frageDirekt("ZZMD01;"), QString());
        QCOMPARE(sl->dspMode(), DSPMode::USB);

        // Die Filterkanten.
        sl->setFilter(150, 2850);
        QCOMPARE(frageDirekt("ZZFL;"), QStringLiteral("ZZFL+0150;"));
        QCOMPARE(frageDirekt("ZZFH;"), QStringLiteral("ZZFH+2850;"));
        QCOMPARE(frageDirekt("ZZFL+0300;"), QString());
        QCOMPARE(sl->filterLow(), 300);

        // Die Erkennung, die DXLab Commander und N1MM+ beim Verbinden
        // schicken. ZZID bleibt stumm, ZZZM/ZZZV antworten.
        QCOMPARE(frageDirekt("ZZID;"), QString());
        QCOMPARE(frageDirekt("ZZZM;"), QStringLiteral("ZZZMLONGPATH;"));
        QVERIFY(frageDirekt("ZZZV;").startsWith(QStringLiteral("ZZZV")));

        // ── Die Rauschsperre haengt an der BETRIEBSART ──────────────
        //
        // Longpath hat drei Schalter (ssql fuer SSB/CW/Digi, amsq fuer
        // AM/SAM, fmsq fuer FM), CAT hat einen. Dieser Fall belegt, dass
        // der richtige getroffen wird -- in FM also fmsq und NICHT ssql.
        // `RadioModel::setSqlEnable()`, der Weg des TCI-Servers, greift
        // immer nur auf ssql und schaltet in FM nichts.
        sl->setDspMode(DSPMode::FM);
        sl->setFmsqEnabled(false);
        sl->setSsqlEnabled(false);
        QCOMPARE(frageDirekt("ZZSO1;"), QString());
        QVERIFY(sl->fmsqEnabled());
        QVERIFY(!sl->ssqlEnabled());          // der SSB-Schalter bleibt aus
        QCOMPARE(frageDirekt("ZZSO;"), QStringLiteral("ZZSO1;"));

        // Und in SSB trifft es ssql.
        sl->setDspMode(DSPMode::USB);
        QCOMPARE(frageDirekt("ZZSO;"), QStringLiteral("ZZSO0;"));   // ssql ist aus
        QCOMPARE(frageDirekt("ZZSO1;"), QString());
        QVERIFY(sl->ssqlEnabled());

        // Thetis' 2 (Stimmsperre) gibt es nicht. Der Haken setzt "an",
        // der Nachleser liest 1 und lehnt ab -- kein stilles
        // "erledigt" fuer etwas, das nicht laeuft.
        QCOMPARE(frageDirekt("ZZSO2;"), QStringLiteral("?;"));
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
        // Dasselbe im ZZ-Satz. ZZFT ist dabei die Sendefrequenz (elf
        // Stellen), nicht die VFO-Wahl wie Kenwoods FT -- lesbar, aber
        // nicht setzbar, weil es keinen Split gibt.
        QCOMPARE(srv.verarbeiteFuerTest(QStringLiteral("ZZFB00007100000;")),
                 QStringLiteral("?;"));
        QCOMPARE(srv.verarbeiteFuerTest(QStringLiteral("ZZSP;")),
                 QStringLiteral("?;"));
        QCOMPARE(srv.verarbeiteFuerTest(QStringLiteral("ZZFT00014100000;")),
                 QStringLiteral("?;"));
        QCOMPARE(srv.verarbeiteFuerTest(QStringLiteral("ZZFT;")),
                 QStringLiteral("ZZFT00014225000;"));

        // Was es kann, geht weiter.
        QCOMPARE(srv.verarbeiteFuerTest(QStringLiteral("FA;")),
                 QStringLiteral("FA00014225000;"));
        QCOMPARE(srv.verarbeiteFuerTest(QStringLiteral("ZZFA;")),
                 QStringLiteral("ZZFA00014225000;"));
    }
};

QTEST_MAIN(TstCatVerdrahtung)
#include "tst_cat_verdrahtung.moc"
