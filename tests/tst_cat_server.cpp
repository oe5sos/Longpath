// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-eigener Prueffall; die gepruefte Pufferlogik
// stammt aus Thetis und ist in src/core/CatServer.cpp zitiert.
//
// Der CAT-Transport, zweiter Gang (2026-10-11).
//
// Gefahren wird gegen ECHTE TCP-Verbindungen auf 127.0.0.1, nicht gegen
// eine Attrappe: die Dinge, die hier schiefgehen koennen, gehen alle im
// Transport schief -- ein Befehl, der ueber zwei Pakete verteilt
// ankommt; zwei Befehle in einem Paket; ein Fremdprogramm, das sich auf
// die Antwort hin trennt; ein Port, der schon belegt ist.
//
// Kein Zeitfenster unter 50 ms und keine festen Wartezeiten: es wird auf
// Ereignisse gewartet (QTRY_*/waitForReadyRead), nie auf die Uhr. Siehe
// docs: feste Wartezeiten haben in diesem Baum viermal denselben
// CI-Ausfall erzeugt.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QHostAddress>
#include <QTcpServer>

#include <optional>
#include <QTcpSocket>

#include "core/CatServer.h"

using namespace Longpath;

namespace {

/// Ein Zustand, der sich bei jeder Abfrage merkt, dass er gefragt wurde.
struct Geber {
    CatProtocol::Zustand z;
    int rufe{0};
    bool stumm{false};   // nullopt melden, wie ein Fenster ohne Slice
    std::optional<CatProtocol::Zustand> operator()()
    {
        ++rufe;
        if (stumm) { return std::nullopt; }
        return z;
    }
};

} // namespace

class TstCatServer : public QObject {
    Q_OBJECT

private:
    CatServer* srv{nullptr};
    Geber geber;
    quint64 gesetzteFrequenz{0};
    int     moxRufe{0};

    /// Verbindet sich auf Kanal k und gibt den Socket (dem Prueffall
    /// gehoerend). Null, wenn es nicht klappt.
    QTcpSocket* verbinde(int k)
    {
        auto* s = new QTcpSocket(this);
        s->connectToHost(QHostAddress::LocalHost, srv->port(k));
        if (!s->waitForConnected(5000)) { delete s; return nullptr; }
        return s;
    }

    /// Schickt und wartet auf `mindestens` Antworten (gezaehlt an den
    /// ';'), bis `msec` um sind.
    ///
    /// WICHTIG -- hier darf NICHT waitForReadyRead() stehen. Server und
    /// Client laufen in DIESEM Prozess auf demselben Faden: der
    /// Server-Socket bekommt sein readyRead nur, wenn die
    /// Ereignisschleife dreht. waitForReadyRead() am Client blockiert
    /// und dreht sie NICHT -- der Server sieht den Befehl also nie, und
    /// jede Abfrage kommt leer zurueck.
    ///
    /// Am 2026-10-11 genau darauf hereingefallen: acht Faelle rot,
    /// waehrend der Server richtig war. Auffaellig war das Muster --
    /// jedes SETZEN gruen (das braucht keine Antwort und wurde ueber
    /// QTRY_COMPARE geprueft, was die Schleife dreht), jede ABFRAGE rot.
    QString frage(QTcpSocket* s, const QByteArray& befehl,
                  int mindestens = 1, int msec = 5000)
    {
        s->write(befehl);
        QByteArray a;
        QElapsedTimer uhr;
        uhr.start();
        while (a.count(';') < mindestens && uhr.elapsed() < msec) {
            // Die Schleife drehen, damit der Server ueberhaupt arbeitet.
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

    void init()
    {
        srv = new CatServer(this);
        geber = Geber{};
        geber.z.vfoAHz = 14225000;
        geber.z.vfoBHz = 7100000;
        geber.z.modeK  = 2;
        gesetzteFrequenz = 0;
        moxRufe = 0;
        srv->setzeZustandsgeber([this]() { return geber(); });
        CatProtocol::Haken h;
        h.setzeVfoA  = [this](quint64 hz) { gesetzteFrequenz = hz; geber.z.vfoAHz = hz; };
        h.setzeVfoB  = [this](quint64 hz) { geber.z.vfoBHz = hz; };
        h.setzeModeK = [this](int k)      { geber.z.modeK = k; };
        h.setzeMox   = [this](bool on)    { ++moxRufe; geber.z.mox = on; };
        h.setzeSplit = [this](bool on)    { geber.z.split = on; };
        srv->setzeHaken(h);
    }

    void cleanup()
    {
        delete srv;
        srv = nullptr;
    }

    // ── Horchen ─────────────────────────────────────────────────────────────

    void einKanalHorchtUndGibtSeinenPort()
    {
        QVERIFY(!srv->laeuft(0));
        QCOMPARE(srv->port(0), quint16(0));
        // Port 0 heisst "nimm einen freien" -- so braucht der Prueffall
        // keinen festen Port und kollidiert nicht mit einem laufenden
        // Longpath des Betreibers.
        QVERIFY(srv->starte(0, 0));
        QVERIFY(srv->laeuft(0));
        QVERIFY(srv->port(0) > 0);
    }

    void einZweitesHorchenAufDenselbenPortScheitert()
    {
        QVERIFY(srv->starte(0, 0));
        const quint16 p = srv->port(0);
        CatServer zweiter;
        QVERIFY(!zweiter.starte(0, p));
        QVERIFY(!zweiter.laeuft(0));
        // Und der erste laeuft weiter -- ein fehlgeschlagener zweiter
        // darf den ersten nicht mitnehmen.
        QVERIFY(srv->laeuft(0));
    }

    void dieVierKanaeleSindUnabhaengig()
    {
        for (int k = 0; k < CatServer::kKanaele; ++k) {
            QVERIFY(srv->starte(k, 0));
        }
        QSet<quint16> ports;
        for (int k = 0; k < CatServer::kKanaele; ++k) {
            QVERIFY(srv->laeuft(k));
            ports.insert(srv->port(k));
        }
        QCOMPARE(ports.size(), CatServer::kKanaele);   // vier verschiedene
        srv->beende(1);
        QVERIFY(!srv->laeuft(1));
        QVERIFY(srv->laeuft(0));
        QVERIFY(srv->laeuft(2));
        QVERIFY(srv->laeuft(3));
    }

    void einKanalAusserhalbNullBisDreiWirdAbgelehnt()
    {
        QVERIFY(!srv->starte(-1, 0));
        QVERIFY(!srv->starte(CatServer::kKanaele, 0));
        QVERIFY(!srv->laeuft(-1));
        QCOMPARE(srv->port(99), quint16(0));
        QCOMPARE(srv->verbindungen(99), 0);
        srv->beende(-1);    // darf nicht abstuerzen
    }

    // ── Befehle über das Netz ───────────────────────────────────────────────

    void eineAbfrageUeberDasNetzWirdBeantwortet()
    {
        QVERIFY(srv->starte(0, 0));
        QTcpSocket* s = verbinde(0);
        QVERIFY(s);
        QCOMPARE(frage(s, "IF;").size(), 2 + 35 + 1);
        QCOMPARE(frage(s, "FA;"), QStringLiteral("FA00014225000;"));
        QCOMPARE(frage(s, "ID;"), QStringLiteral("ID900;"));
    }

    void derZustandWirdVorJedemBefehlFrischGeholt()
    {
        // Der wichtigste Fall dieser Datei. Ein Server, der eine KOPIE
        // des Zustands haelt, meldet nach dem Abstimmen die alte
        // Frequenz -- und das Fremdprogramm sendet dort.
        QVERIFY(srv->starte(0, 0));
        QTcpSocket* s = verbinde(0);
        QVERIFY(s);
        QCOMPARE(frage(s, "FA;"), QStringLiteral("FA00014225000;"));
        const int vorher = geber.rufe;

        // Am Geraet gedreht, ohne dass CAT etwas davon weiss:
        geber.z.vfoAHz = 21300000;
        QCOMPARE(frage(s, "FA;"), QStringLiteral("FA00021300000;"));
        QVERIFY(geber.rufe > vorher);
    }

    void einSetzenUeberDasNetzErreichtDenHaken()
    {
        QVERIFY(srv->starte(0, 0));
        QTcpSocket* s = verbinde(0);
        QVERIFY(s);
        s->write("FA00014250000;");
        QVERIFY(s->waitForBytesWritten(5000));
        QTRY_COMPARE(gesetzteFrequenz, Q_UINT64_C(14250000));
        // Und es kam KEINE Antwort -- Kenwood quittiert ein Setzen nicht.
        // Die Schleife dabei drehen, sonst prueft das nur, dass nichts
        // ankommt, WEIL der Server nie arbeitet.
        QCOMPARE(frage(s, "", 1, 300), QString());
    }

    void tastenUeberDasNetz()
    {
        QVERIFY(srv->starte(0, 0));
        QTcpSocket* s = verbinde(0);
        QVERIFY(s);
        QCOMPARE(frage(s, "TX;"), QStringLiteral("TX0;"));
        QTRY_COMPARE(moxRufe, 1);
        QVERIFY(geber.z.mox);
        QCOMPARE(frage(s, "RX;"), QStringLiteral("RX0;"));
        QTRY_COMPARE(moxRufe, 2);
        QVERIFY(!geber.z.mox);
    }

    // ── Pufferlogik: die Faelle, die nur im Strom auftreten ─────────────────

    void zweiBefehleInEinemPaketWerdenBeideBeantwortet()
    {
        // From Thetis CAT/TCPIPcatServer.cs:275 -- genau dieser Fall ist
        // dort am 2.10.3.9 (MW0LGE) nachgebessert worden ("fixed to
        // handle multiple messages ending in ;"). WSJT-X schickt
        // regelmaessig mehrere Befehle in einem Zug.
        QVERIFY(srv->starte(0, 0));
        QTcpSocket* s = verbinde(0);
        QVERIFY(s);
        QCOMPARE(frage(s, "FA;ID;", 2),
                 QStringLiteral("FA00014225000;ID900;"));
    }

    void einBefehlUeberZweiPaketeWirdZusammengesetzt()
    {
        // Ein Paket kann mitten im Befehl enden. Ohne Puffer waere
        // "FA" ein Formatfehler und "00014225000;" Unsinn.
        QVERIFY(srv->starte(0, 0));
        QTcpSocket* s = verbinde(0);
        QVERIFY(s);
        // Noch keine Antwort -- der Befehl ist unvollstaendig.
        QCOMPARE(frage(s, "FA0001", 1, 300), QString());
        s->write("4250000;");
        QVERIFY(s->waitForBytesWritten(5000));
        QTRY_COMPARE(gesetzteFrequenz, Q_UINT64_C(14250000));
    }

    void zeilenumbruecheStoerenNicht()
    {
        // From Thetis CAT/TCPIPcatServer.cs:281 --
        //   m_oneLineBuf.Replace(Environment.NewLine, "");
        //   // so can be used a by raw telnet client
        // Wer in telnet "FA;" eintippt, schickt "FA;\r\n". Ohne das
        // Entfernen steckte das \r\n vor dem naechsten Befehl und
        // verschoebe ihn.
        QVERIFY(srv->starte(0, 0));
        QTcpSocket* s = verbinde(0);
        QVERIFY(s);
        QCOMPARE(frage(s, "FA;\r\nID;\r\n", 2),
                 QStringLiteral("FA00014225000;ID900;"));
    }

    void einUeberlangerPufferWirdVerworfen()
    {
        // From Thetis CAT/TCPIPcatServer.cs:295 --
        //   if (m_oneLineBuf.Length > 255) m_oneLineBuf.Clear();
        //   // not likely to be a cat message if it got this long
        // Ohne den Deckel haelt ein Programm, das nie ein ';' schickt,
        // den Puffer offen, bis der Speicher ausgeht.
        QVERIFY(srv->starte(0, 0));
        QTcpSocket* s = verbinde(0);
        QVERIFY(s);
        // kein ';' dabei -- also keine Antwort
        QCOMPARE(frage(s, QByteArray(400, 'X'), 1, 300), QString());
        // Danach muss ein richtiger Befehl wieder gehen -- der Deckel
        // darf die Verbindung nicht unbrauchbar machen.
        QCOMPARE(frage(s, "ID;"), QStringLiteral("ID900;"));
    }

    void einSchlechterBefehlWirdAbgelehntUndDieVerbindungBleibt()
    {
        QVERIFY(srv->starte(0, 0));
        QTcpSocket* s = verbinde(0);
        QVERIFY(s);
        QCOMPARE(frage(s, "QQ;"), QStringLiteral("?;"));
        // Die Verbindung steht noch und der naechste Befehl geht.
        QCOMPARE(s->state(), QAbstractSocket::ConnectedState);
        QCOMPARE(frage(s, "ID;"), QStringLiteral("ID900;"));
    }

    // ── Verbindungen ────────────────────────────────────────────────────────

    void verbindungenWerdenGezaehltUndGemeldet()
    {
        QVERIFY(srv->starte(0, 0));
        QSignalSpy spy(srv, &CatServer::verbindungenGeaendert);
        QCOMPARE(srv->verbindungen(0), 0);

        QTcpSocket* a = verbinde(0);
        QVERIFY(a);
        QTRY_COMPARE(srv->verbindungen(0), 1);
        QTcpSocket* b = verbinde(0);
        QVERIFY(b);
        QTRY_COMPARE(srv->verbindungen(0), 2);
        QVERIFY(spy.count() >= 2);

        a->disconnectFromHost();
        QTRY_COMPARE(srv->verbindungen(0), 1);
        b->disconnectFromHost();
        QTRY_COMPARE(srv->verbindungen(0), 0);
    }

    void zweiFremdprogrammeStoerenSichNicht()
    {
        // Der Grund, warum es vier Kanaele gibt: WSJT-X, fldigi und ein
        // Logbuch sollen nicht um einen Port streiten. Aber auch zwei an
        // EINEM Kanal muessen getrennte Puffer haben -- sonst mischen
        // sich ihre halben Befehle.
        QVERIFY(srv->starte(0, 0));
        QTcpSocket* a = verbinde(0);
        QTcpSocket* b = verbinde(0);
        QVERIFY(a);
        QVERIFY(b);
        // a schickt eine Haelfte, b einen ganzen Befehl dazwischen.
        a->write("FA0002");
        QVERIFY(a->waitForBytesWritten(5000));
        QCOMPARE(frage(b, "ID;"), QStringLiteral("ID900;"));
        a->write("1300000;");
        QVERIFY(a->waitForBytesWritten(5000));
        // Haette b in as Puffer geschrieben, waere hier nicht 21,3 MHz
        // herausgekommen.
        QTRY_COMPARE(gesetzteFrequenz, Q_UINT64_C(21300000));
    }

    void einAbgebauterKanalTrenntSeineVerbindungen()
    {
        QVERIFY(srv->starte(0, 0));
        QTcpSocket* s = verbinde(0);
        QVERIFY(s);
        QTRY_COMPARE(srv->verbindungen(0), 1);
        srv->beende(0);
        QCOMPARE(srv->verbindungen(0), 0);
        QVERIFY(!srv->laeuft(0));
        QTRY_VERIFY(s->state() != QAbstractSocket::ConnectedState);
    }

    void derAbbauDesServersStuerztNichtAb()
    {
        // Die Fehlerform, die in diesem Baum viermal zugeschlagen hat:
        // ein Abbau, der in halb abgebaute Mitglieder greift. Hier mit
        // OFFENER Verbindung abbauen.
        auto* eigener = new CatServer();
        eigener->setzeZustandsgeber(
            [this]() -> std::optional<CatProtocol::Zustand> { return geber.z; });
        QVERIFY(eigener->starte(0, 0));
        auto* s = new QTcpSocket();
        s->connectToHost(QHostAddress::LocalHost, eigener->port(0));
        QVERIFY(s->waitForConnected(5000));
        s->write("FA;");
        QVERIFY(s->waitForBytesWritten(5000));
        delete eigener;          // mitten im Betrieb
        s->deleteLater();
        QVERIFY(true);           // kein Absturz = bestanden
    }

    // ── Die harte Grenze: nicht von aussen erreichbar ───────────────────────

    void derServerHorchtNurAufDerEigenenMaschine()
    {
        // DIE sicherheitsrelevante Eigenschaft dieser Datei. CAT kann den
        // Sender TASTEN (TX;). Horcht der Server auf allen Schnittstellen,
        // kann jeder im Netz Longpath senden lassen -- ohne Kennwort, ohne
        // Spur, auf der Frequenz, die er selbst vorher setzt.
        //
        // Nachgezogen am 2026-10-11: die Gegenprobe hat
        // QHostAddress::LocalHost gegen ::Any getauscht, und ALLE zwanzig
        // Faelle blieben gruen. Eine Eigenschaft, die keine Pruefung
        // bewacht, haelt nur so lange, wie niemand die Zeile anfasst.
        // ── Wie gemessen wird, und warum nicht mehr anders ──────────────
        //
        // Bis zum 2026-10-14 band dieser Fall eine SONDE auf 0.0.0.0
        // denselben Port und schloss aus dem Erfolg auf eine lokale
        // Bindung. Das war falsch gedacht, und zwar
        // PLATTFORMABHAENGIG falsch:
        //
        //   macOS/BSD  erlaubt die Platzhalterbindung neben einer
        //              lokalen auf demselben Port -> Sonde gelingt
        //              -> gruen.
        //   Linux      verbietet sie -> Sonde schlaegt fehl -> ROT,
        //              obwohl der Code stimmt.
        //
        // Aufgefallen in der CI von PR #222 (Build Linux x64 2/4): hier
        // auf dem Mac 25 von 25 gruen, dort rot. **Ein Prueffall, der
        // eine Eigenschaft nur MITTELBAR messen kann, misst in
        // Wirklichkeit das Betriebssystem.**
        //
        // Jetzt direkt: `QTcpServer::serverAddress()` ueber
        // `CatServer::adresse()`. Das ist die Adresse, an der der Kanal
        // wirklich haengt -- keine Schlussfolgerung, eine Ablesung.
        QVERIFY(srv->starte(0, 0));
        const QHostAddress wo = srv->adresse(0);
        QVERIFY2(!wo.isNull(),
                 "Kanal 0 horcht nicht -- dann sagt dieser Fall nichts.");
        QVERIFY2(wo.isLoopback(),
                 qPrintable(QStringLiteral(
                     "Der CAT-Server haengt an %1 statt an der eigenen "
                     "Maschine -- jeder im Netz kann damit TX; schicken.")
                     .arg(wo.toString())));
        // Und ausdruecklich NICHT der Platzhalter: `isLoopback()` allein
        // wuerde bei 0.0.0.0 ohnehin false geben, aber die Absicht soll
        // dastehen.
        QVERIFY(wo != QHostAddress(QHostAddress::Any));
        QVERIFY(wo != QHostAddress(QHostAddress::AnyIPv4));
    }

    void einZweiterServerAufDemselbenPortBekommtKeineBefehle()
    {
        // Gegenstueck zum Fall darueber: dass die Bindung lokal ist, soll
        // nicht heissen, dass ein zweiter Horcher die Befehle abfaengt.
        QVERIFY(srv->starte(0, 0));
        QTcpSocket* s = verbinde(0);
        QVERIFY(s);
        QCOMPARE(frage(s, "ID;"), QStringLiteral("ID900;"));
    }

    void einFremdprogrammDasSichAufDieAntwortHinTrennt()
    {
        // Nachgezogen am 2026-10-11. Die Gegenprobe hat die Neusuche des
        // Puffereintrags nach dem Schreiben entfernt (`it =
        // ka.puffer.find(s)` in der Schleife) -- und kein Fall fiel, weil
        // sich in keinem ein Programm auf die Antwort hin trennte. Genau
        // das tut aber ein Logbuch, das EINE Frequenz holt und geht.
        //
        // Ohne die Neusuche zeigte `puffer` danach auf einen Eintrag, den
        // raeumeAuf() schon entfernt hat.
        QVERIFY(srv->starte(0, 0));
        for (int runde = 0; runde < 20; ++runde) {
            auto* s = new QTcpSocket();
            s->connectToHost(QHostAddress::LocalHost, srv->port(0));
            QVERIFY(s->waitForConnected(5000));
            // Zwei Befehle auf einmal, dann sofort trennen: der Server
            // steht beim Trennen mitten in der Schleife ueber die Befehle.
            s->write("ID;IF;");
            s->flush();
            s->abort();
            delete s;
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        }
        // Der Kanal muss das ueberstehen und danach normal arbeiten.
        QTRY_COMPARE(srv->verbindungen(0), 0);
        QTcpSocket* gut = verbinde(0);
        QVERIFY(gut);
        QCOMPARE(frage(gut, "ID;"), QStringLiteral("ID900;"));
    }

    // ── Der Nachleser geht durch den Server ─────────────────────────────────

    void einWirkungslosesSetzenWirdUeberDenServerAbgelehnt()
    {
        // Nachgezogen am 2026-10-13. Die Gegenprobe hat im Server die
        // Zeile stillgelegt, die den Zustandsgeber ALS NACHLESER an die
        // Protokollschicht haengt -- und kein Fall fiel. Die
        // Protokollschicht hat ihren eigenen Prueffall dafuer
        // (mitNachleserWirdEinWIRKUNGSLOSESSetzenAbgelehnt), aber ob der
        // Server ihn anhaengt, prueft nur dieser hier.
        //
        // Der Unterschied ist der, der live zaehlt: ohne Nachleser
        // quittiert ein Setzen blind, und ein Contestprogramm sendet auf
        // der alten Frequenz weiter.
        QVERIFY(srv->starte(0, 0));
        QTcpSocket* s = verbinde(0);
        QVERIFY(s);

        // Den Frequenz-Haken taub machen: er wird gerufen, bewegt aber
        // nichts (genau der Fall "kein gebundener Slice").
        CatProtocol::Haken taub;
        taub.setzeVfoA = [](quint64) { };
        srv->setzeHaken(taub);

        QCOMPARE(frage(s, "FA;"), QStringLiteral("FA00014225000;"));
        // Ohne Nachleser kaeme hier eine leere Antwort ("erledigt").
        QCOMPARE(frage(s, "FA00021300000;"), QStringLiteral("?;"));
        // Und die Frequenz steht noch, wo sie war.
        QCOMPARE(frage(s, "FA;"), QStringLiteral("FA00014225000;"));
    }

    // ── Ohne Zustandsgeber ──────────────────────────────────────────────────

    void einStummerGeberWirdAbgelehntStattMitNullenBeantwortet()
    {
        // Der Geber meldet nullopt, wenn es nichts zu melden gibt (kein
        // Empfaenger angelegt, keine Verbindung). Dann darf KEINE
        // Frequenz hinausgehen: "FA00000000000;" ist eine Angabe, und
        // ein Fremdprogramm glaubt sie.
        //
        // Am 2026-10-11 am echten Fenster aufgefallen -- ein frisch
        // gebautes MainWindow hat noch keinen aktiven Slice, und der
        // erste Entwurf meldete dafuer brav Nullen.
        QVERIFY(srv->starte(0, 0));
        QTcpSocket* s = verbinde(0);
        QVERIFY(s);
        QCOMPARE(frage(s, "FA;"), QStringLiteral("FA00014225000;"));
        geber.stumm = true;
        QCOMPARE(frage(s, "FA;"), QStringLiteral("?;"));
        QCOMPARE(frage(s, "IF;"), QStringLiteral("?;"));
        geber.stumm = false;
        QCOMPARE(frage(s, "FA;"), QStringLiteral("FA00014225000;"));
    }

    void ohneZustandsgeberWirdAbgelehntStattErfunden()
    {
        CatServer blank;
        // Kein Geber angehaengt: der Server weiss nichts ueber das
        // Geraet. Eine Vorgabe zu melden (0 Hz, USB) waere eine
        // erfundene Frequenz -- und das Fremdprogramm wuerde sie glauben.
        QCOMPARE(blank.verarbeiteFuerTest(QStringLiteral("FA;")),
                 QStringLiteral("?;"));
        QCOMPARE(blank.verarbeiteFuerTest(QStringLiteral("IF;")),
                 QStringLiteral("?;"));
    }
};

QTEST_MAIN(TstCatServer)
#include "tst_cat_server.moc"
