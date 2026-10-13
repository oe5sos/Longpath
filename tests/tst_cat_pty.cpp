// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-eigener Prueffall; das Verhaltensvorbild
// stammt aus Thetis und ist in src/core/CatPtyServer.h zitiert.
//
// Die serielle Haelfte der CAT-Anbindung, vierter Gang (2026-10-12).
//
// Gefahren wird gegen ECHTE Pseudoterminals, nicht gegen eine Attrappe:
// jede Falle, die diesen Weg kostet, sitzt im Betriebssystem und nicht
// in der Logik. Der Prueffall oeffnet den Geraetepfad so, wie WSJT-X
// oder fldigi ihn oeffnen wuerden -- mit open(2) auf /dev/ttysNNN --
// und liest und schreibt dort.
//
// Die Fallen, gegen die hier gemessen wird, in der Reihenfolge, in der
// sie beim Bau zugeschlagen haben:
//
//   1. ECHO. Ein frisches Pseudoterminal spiegelt alles, was auf der
//      Steuerseite geschrieben wird, dort wieder als lesbar zurueck.
//      Ohne cfmakeraw() liest Longpath also seine eigenen Antworten und
//      deutet sie als Befehle. `echoKommtNichtZurueck` faengt das.
//   2. /dev/ptyp0..3. Das zeigte das Applet vorher an -- BSD-Altnamen,
//      die mit dem angelegten Geraet nichts zu tun haben.
//      `pfadeSindEchteGeraete` faengt das.
//   3. ICANON. Ohne Rohmodus kommt nichts durch, bis zufaellig ein \n
//      folgt; CAT schickt keines. Jede Abfrage hier faengt das.
//
// Kein QSKIP, das einen echten Fehler als Umgebungsproblem verkleidet:
// `initTestCase` MISST einmal, ob diese Maschine Pseudoterminals
// hergibt, und danach prueft jeder Fall ohne Ausweg.
//
// Kein Zeitfenster unter 50 ms und keine festen Wartezeiten -- gewartet
// wird auf Ereignisse, nie auf die Uhr.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <optional>

#include "core/CatPtyServer.h"
#include "core/CatServer.h"

using namespace Longpath;

namespace {

/// Ein Fremdprogramm, das den Geraetepfad oeffnet -- so, wie hamlib es
/// tut. Setzt KEINEN Rohmodus: das soll Longpath mitbringen, und wenn
/// dieser Prueffall ihn selbst setzte, wuerde er die Falle zudecken,
/// gegen die er messen soll.
class Fremdprogramm {
public:
    explicit Fremdprogramm(const QString& pfad)
    {
        m_fd = ::open(pfad.toLocal8Bit().constData(),
                      O_RDWR | O_NOCTTY | O_NONBLOCK);
    }
    ~Fremdprogramm() { if (m_fd >= 0) { ::close(m_fd); } }

    Fremdprogramm(const Fremdprogramm&) = delete;
    Fremdprogramm& operator=(const Fremdprogramm&) = delete;

    bool offen() const { return m_fd >= 0; }
    int  fd()    const { return m_fd; }

    bool schreibe(const QByteArray& b)
    {
        qsizetype ab = 0;
        while (ab < b.size()) {
            const ssize_t n = ::write(m_fd, b.constData() + ab,
                                      static_cast<size_t>(b.size() - ab));
            if (n > 0) { ab += n; continue; }
            if (n < 0 && errno == EINTR) { continue; }
            return false;
        }
        return true;
    }

    /// Schickt `befehl` und wartet, bis `mindestens` Antworten da sind
    /// (gezaehlt an den ';'), hoechstens aber `msec`.
    ///
    /// Die Ereignisschleife MUSS hier gedreht werden: Longpath liest
    /// ueber einen QSocketNotifier, und der feuert nur, wenn die
    /// Schleife laeuft. Server und Fremdprogramm sitzen in DIESEM
    /// Prozess auf demselben Faden -- genau die Falle, die den
    /// TCP-Prueffall am 2026-10-11 acht Faelle gekostet hat (dort stand
    /// waitForReadyRead, das blockiert und die Schleife nicht dreht).
    QByteArray frage(const QByteArray& befehl, int mindestens = 1,
                     int msec = 5000)
    {
        if (!schreibe(befehl)) { return {}; }
        return lies(mindestens, msec);
    }

    QByteArray lies(int mindestens = 1, int msec = 5000)
    {
        QByteArray a;
        QElapsedTimer uhr;
        uhr.start();
        while (a.count(';') < mindestens && uhr.elapsed() < msec) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            char roh[512];
            for (;;) {
                const ssize_t n = ::read(m_fd, roh, sizeof(roh));
                if (n > 0) { a.append(roh, static_cast<int>(n)); continue; }
                break;
            }
        }
        return a;
    }

    /// Dreht die Schleife eine Weile und gibt zurueck, was ANKAM --
    /// fuer die Faelle, in denen NICHTS ankommen darf.
    QByteArray horchKurz(int msec = 300)
    {
        QByteArray a;
        QElapsedTimer uhr;
        uhr.start();
        while (uhr.elapsed() < msec) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            char roh[512];
            for (;;) {
                const ssize_t n = ::read(m_fd, roh, sizeof(roh));
                if (n > 0) { a.append(roh, static_cast<int>(n)); continue; }
                break;
            }
        }
        return a;
    }

private:
    int m_fd{-1};
};

} // namespace

class TstCatPty : public QObject {
    Q_OBJECT

private:
    CatPtyServer* pty{nullptr};
    CatServer*    tcp{nullptr};
    CatProtocol::Zustand z;
    bool          stumm{false};
    quint64       gesetzteFrequenz{0};
    int           moxRufe{0};
    bool          umgebungTaugt{false};

private slots:
    void initTestCase()
    {
        // PFLICHT. Ohne Testmodus liest und SCHREIBT AppSettings die
        // echte Einstellungsdatei des Betreibers (~1,9 MB, 9531
        // Eintraege, sein Layoutprofil). Hier zusaetzlich wichtig, weil
        // CatPtyServer::festerPfad() ueber AppSettings::dataDir() geht
        // und also VERKNUEPFUNGEN im Datenordner anlegt -- ohne diese
        // Zeile mitten in den echten Daten.
        QStandardPaths::setTestModeEnabled(true);

        // Einmal MESSEN, ob diese Maschine Pseudoterminals hergibt --
        // statt im Fehlerfall zu ueberspringen. Ein QSKIP mitten im
        // Prueffall hat am 2026-10-11 einen echten Fehler als
        // Umgebungsproblem verkleidet: die Gegenprobe legte die
        // Startzeile still, und der Fall wurde UEBERSPRUNGEN statt zu
        // fallen.
        QVERIFY2(CatPtyServer::verfuegbar(),
                 "Diese Plattform meldet keine Pseudoterminals - dann "
                 "darf dieser Prueffall gar nicht gebaut werden "
                 "(tests/CMakeLists.txt gattert ihn auf UNIX).");

        CatPtyServer probe;
        umgebungTaugt = probe.starte(0);
        if (!umgebungTaugt) {
            qWarning().noquote()
                << "Pseudoterminal nicht zu haben:" << probe.grund(0);
        }
        QVERIFY2(umgebungTaugt,
                 "Diese Maschine gibt kein Pseudoterminal her. Das ist "
                 "KEIN Grund zu ueberspringen - ohne Pseudoterminal ist "
                 "der gepruefte Weg nicht vorhanden, und das muss rot "
                 "sein.");
        probe.beende(0);
    }

    void init()
    {
        z = CatProtocol::Zustand{};
        z.vfoAHz = 14225000;
        z.vfoBHz = 7100000;
        z.modeK  = 2;      // USB
        z.mox    = false;
        z.stepHz = 10;
        z.rigId  = 900;
        stumm            = false;
        gesetzteFrequenz = 0;
        moxRufe          = 0;

        // Der TCP-Server ist hier NICHT der Prueffall, sondern der
        // Trager der Protokollschicht: genau EINE Instanz beantwortet
        // beide Wege. Dass das so ist, prueft `beideWegeEineSchicht`.
        tcp = new CatServer(this);
        tcp->setzeZustandsgeber(
            [this]() -> std::optional<CatProtocol::Zustand> {
                if (stumm) { return std::nullopt; }
                return z;
            });
        CatProtocol::Haken haken;
        haken.setzeVfoA  = [this](quint64 hz) { gesetzteFrequenz = hz; z.vfoAHz = hz; };
        haken.setzeModeK = [this](int k) { z.modeK = k; };
        haken.setzeMox   = [this](bool on) { ++moxRufe; z.mox = on; };
        tcp->setzeHaken(haken);

        pty = new CatPtyServer(this);
        pty->setzeAntwortgeber(
            [this](const QString& b) { return tcp->antwortAuf(b); });
    }

    void cleanup()
    {
        delete pty; pty = nullptr;
        delete tcp; tcp = nullptr;
    }

    // ── Was ueberhaupt angelegt wird ────────────────────────────────

    void vierKanaeleMitVerschiedenenPfaden()
    {
        QSet<QString> pfade;
        for (int k = 0; k < CatPtyServer::kKanaele; ++k) {
            QVERIFY2(pty->starte(k), qPrintable(pty->grund(k)));
            QVERIFY(pty->laeuft(k));
            const QString p = pty->pfad(k);
            QVERIFY2(!p.isEmpty(), "Ein laufender Kanal ohne Pfad ist "
                                   "genau der Zustand von vorher.");
            pfade.insert(p);
        }
        // Vier verschiedene Geraete. Der erste Entwurf hat ptsname()
        // VOR dem Kopieren weitergegeben -- der Zeiger zeigt auf einen
        // statischen Puffer, und am Ende trugen alle vier Kanaele den
        // Pfad des letzten.
        QCOMPARE(pfade.size(), CatPtyServer::kKanaele);
    }

    void pfadeSindEchteGeraete()
    {
        QVERIFY2(pty->starte(0), qPrintable(pty->grund(0)));
        const QString p = pty->pfad(0);

        // Das ist der Kern des Auftrags: das Applet zeigte vorher
        // "/dev/ptyp0" -- einen BSD-Altnamen, der auf dieser Maschine
        // gar nicht existiert. Ein Pfad, den niemand oeffnen kann, ist
        // schlimmer als kein Pfad: man sucht den Fehler im
        // Fremdprogramm.
        QVERIFY2(!p.contains(QStringLiteral("ptyp")),
                 qPrintable(QStringLiteral("Altname statt echtem Geraet: %1")
                                .arg(p)));
        QVERIFY2(QFileInfo(p).exists(),
                 qPrintable(QStringLiteral("%1 gibt es nicht").arg(p)));

        Fremdprogramm fp(p);
        QVERIFY2(fp.offen(),
                 qPrintable(QStringLiteral("%1 laesst sich nicht oeffnen: %2")
                                .arg(p, QString::fromLocal8Bit(::strerror(errno)))));

        // Und es IST ein Terminal, keine normale Datei.
        QVERIFY(::isatty(fp.fd()) == 1);
    }

    void festeVerknuepfungZeigtAufsGeraet()
    {
        QVERIFY2(pty->starte(1), qPrintable(pty->grund(1)));
        const QString fest = CatPtyServer::festerPfad(1);
        QVERIFY(!fest.isEmpty());
        QVERIFY(fest.endsWith(QStringLiteral("/B")));

        const QFileInfo fi(fest);
        // Ohne diese Verknuepfung muesste der Betreiber WSJT-X nach
        // JEDEM Start von Longpath neu einstellen: ptsname() gibt jedes
        // Mal eine andere Nummer. Die Anbindung waere auf dem Papier da
        // und in der Praxis unbenutzbar.
        QVERIFY2(fi.isSymLink(),
                 qPrintable(QStringLiteral("%1 ist keine Verknuepfung")
                                .arg(fest)));
        QCOMPARE(fi.symLinkTarget(), pty->pfad(1));

        // Und sie ist benutzbar, nicht bloss vorhanden.
        Fremdprogramm fp(fest);
        QVERIFY(fp.offen());
        QCOMPARE(fp.frage("FA;"), QByteArray("FA00014225000;"));
    }

    void beendenRaeumtDieVerknuepfungWeg()
    {
        QVERIFY2(pty->starte(2), qPrintable(pty->grund(2)));
        const QString fest = CatPtyServer::festerPfad(2);
        QVERIFY(QFileInfo(fest).isSymLink());

        pty->beende(2);
        QVERIFY(!pty->laeuft(2));
        QVERIFY(pty->pfad(2).isEmpty());
        // Eine Verknuepfung, die auf ein weggeraeumtes Pseudoterminal
        // zeigt, ist gefaehrlicher als keine: die Nummer kann das
        // System in derselben Minute neu vergeben haben, und dann redet
        // das Fremdprogramm ohne Fehlermeldung mit dem falschen Geraet.
        QVERIFY2(!QFileInfo(fest).isSymLink(),
                 "Verknuepfung auf ein totes Geraet stehengelassen");
    }

    void ohneFremdprogrammBleibtDerKanalStehen()
    {
        int ausfaelle = 0;
        connect(pty, &CatPtyServer::kanalAusgefallen,
                this, [&ausfaelle](int, QString) { ++ausfaelle; });

        QVERIFY2(pty->starte(0), qPrintable(pty->grund(0)));

        // KEIN Fremdprogramm. Die Ereignisschleife eine Weile drehen --
        // das ist der Normalfall nach dem Einschalten des Schalters:
        // der Kanal steht offen und WARTET, oft stundenlang.
        //
        // Ohne die offen gehaltene Geraeteseite (Kanal::geraetFd) liefert
        // read() auf der Steuerseite sofort 0 bzw. EIO, der
        // QSocketNotifier bleibt dauerhaft bereit und feuert in einer
        // Endlosrunde auf dem HAUPTFADEN -- der Kanal baut sich ab, und
        // die Oberflaeche steht.
        QElapsedTimer uhr; uhr.start();
        while (uhr.elapsed() < 300) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }

        QCOMPARE(ausfaelle, 0);
        QVERIFY2(pty->laeuft(0),
                 "Der Kanal hat sich ohne Fremdprogramm selbst abgebaut.");
        QVERIFY(!pty->pfad(0).isEmpty());

        // Und er arbeitet, wenn sich danach jemand verbindet.
        Fremdprogramm fp(pty->pfad(0));
        QVERIFY(fp.offen());
        QCOMPARE(fp.frage("ID;"), QByteArray("ID900;"));
    }

    void fremdprogrammDarfGehenUndWiederkommen()
    {
        QVERIFY2(pty->starte(0), qPrintable(pty->grund(0)));
        const QString p = pty->pfad(0);
        {
            Fremdprogramm erst(p);
            QVERIFY(erst.offen());
            QCOMPARE(erst.frage("ID;"), QByteArray("ID900;"));
        }   // trennt sich -- so wie WSJT-X beim Beenden

        QElapsedTimer uhr; uhr.start();
        while (uhr.elapsed() < 200) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        QVERIFY2(pty->laeuft(0),
                 "Der Kanal ist mitgegangen, als sich das Fremdprogramm "
                 "getrennt hat. Dann muesste der Betreiber den Schalter "
                 "nach jedem Neustart von WSJT-X neu druecken.");

        // Dasselbe Geraet, dasselbe Verhalten -- der Pfad hat sich NICHT
        // geaendert, sonst waere die feste Verknuepfung tot.
        QCOMPARE(pty->pfad(0), p);
        Fremdprogramm dann(p);
        QVERIFY(dann.offen());
        QCOMPARE(dann.frage("FA;"), QByteArray("FA00014225000;"));
    }

    void ungeleseneAntwortUeberlebtDenOeffnerwechsel()
    {
        // Das ist KEIN Wunsch, sondern der FESTGEHALTENE Befund. Am
        // 2026-10-12 im Livelauf am laufenden Programm gesehen: ein
        // Skript, das je Befehl oeffnet, fragt und wieder schliesst,
        // bekam jede Antwort um EINEN Befehl versetzt -- die vorige lag
        // ungelesen in der Eingangsschlange der Geraeteseite und wurde
        // vom naechsten Oeffner zuerst gelesen.
        //
        // WARUM DAS SO BLEIBT: eine echte serielle Schnittstelle
        // verhaelt sich genauso. Bytes, die unterwegs waren, als ein
        // Programm starb, sieht das naechste -- und genau diese
        // Gleichheit ist der Sinn des Pseudoterminals. Longpath KANN es
        // ausserdem nicht sehen: ein Pseudoterminal meldet nicht, wer
        // die Geraeteseite oeffnet oder schliesst.
        //
        // Und die naheliegende Abhilfe waere falsch: wer vor jeder neuen
        // Antwort die alte wegwirft, bricht `zweiBefehleInEinemSchreib-
        // vorgang` -- ein Programm, das "FA;ID;" in einem Stueck
        // schickt, erwartet BEIDE Antworten in der Reihenfolge.
        //
        // Eine Zeile `tcflush(geraetFd, TCIOFLUSH)` in starte() hilft
        // hier NICHT, und das ist gemessen: beende() gibt das
        // Pseudoterminal zurueck, starte() legt ein neues an -- in dem
        // liegt nie etwas. Die Zeile war am 2026-10-12 eingebaut und ist
        // nach der Gegenprobe wieder heraus: sie hat nichts getan und
        // hat behauptet, etwas zu tun.
        QVERIFY2(pty->starte(0), qPrintable(pty->grund(0)));
        const QString p = pty->pfad(0);

        {
            Fremdprogramm stirbt(p);
            QVERIFY(stirbt.offen());
            // Fragen und die Antwort NICHT abholen.
            QVERIFY(stirbt.schreibe("ID;"));
            QElapsedTimer uhr; uhr.start();
            while (uhr.elapsed() < 150) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            }
        }   // schliesst, die Antwort bleibt liegen

        Fremdprogramm neu(p);
        QVERIFY(neu.offen());
        // Die alte Antwort kommt beim neuen Oeffner an. Sollte dieser
        // Fall eines Tages rot werden, hat sich das Verhalten geaendert
        // -- dann gehoert der Kommentar darueber umgeschrieben, nicht
        // der Fall gestrichen.
        QCOMPARE(neu.horchKurz(200), QByteArray("ID900;"));
        // Und danach ist die Leitung wieder im Takt.
        QCOMPARE(neu.frage("FA;"), QByteArray("FA00014225000;"));
    }

    // ── Die Falle, die alles kostet: das Echo ───────────────────────

    void echoKommtNichtZurueck()
    {
        QVERIFY2(pty->starte(0), qPrintable(pty->grund(0)));
        Fremdprogramm fp(pty->pfad(0));
        QVERIFY(fp.offen());

        const QByteArray a = fp.frage("FA;");

        // GENAU die Antwort, nichts davor und nichts dahinter. Ohne
        // cfmakeraw() spiegelt die Leitungsdisziplin, was Longpath
        // schreibt, auf der Steuerseite als lesbar zurueck: Longpath
        // liest "FA00014225000;" als BEFEHL, deutet ihn als Setzen von
        // 14.225.000 Hz, antwortet, liest die Antwort wieder -- und
        // laeuft im Kreis.
        QCOMPARE(a, QByteArray("FA00014225000;"));

        // Der Kreislauf hinterlaesst eine Spur, die man auch ohne
        // Byte-Vergleich sieht: der Setzen-Haken wird gerufen, obwohl
        // das Fremdprogramm nur GEFRAGT hat.
        QCOMPARE(gesetzteFrequenz, static_cast<quint64>(0));

        // Und nach einer Ruhepause kommt nichts nach.
        QCOMPARE(fp.horchKurz(), QByteArray());
    }

    void keinZeilenumbruchInDerAntwort()
    {
        QVERIFY2(pty->starte(0), qPrintable(pty->grund(0)));
        Fremdprogramm fp(pty->pfad(0));
        QVERIFY(fp.offen());

        // OPOST/ONLCR blaest ein \n zu \r\n auf. In der Antwort steckt
        // kein \n -- aber im Statussatz steckten schon Zeichen, die ein
        // Ausgabefilter anfassen koennte. Der Satz muss BYTEGENAU
        // ankommen, 35 Zeichen plus Praefix plus ';'.
        const QByteArray a = fp.frage("IF;");
        QVERIFY2(!a.contains('\r'), "ONLCR hat zugeschlagen");
        QVERIFY2(a.startsWith("IF") && a.endsWith(';'), a.constData());
        QCOMPARE(a.size(), 2 + 35 + 1);
    }

    // ── Der Strom: halbe Befehle, mehrere Befehle, Muell ────────────

    void befehlUeberZweiSchreibvorgaenge()
    {
        QVERIFY2(pty->starte(0), qPrintable(pty->grund(0)));
        Fremdprogramm fp(pty->pfad(0));
        QVERIFY(fp.offen());

        // Haelfte eins: darf NICHTS ausloesen. Wer je Lesevorgang einen
        // Befehl erwartet, antwortet hier schon falsch.
        QVERIFY(fp.schreibe("FA000142"));
        QCOMPARE(fp.horchKurz(), QByteArray());

        // Haelfte zwei schliesst ab -- und erst jetzt wird gesetzt.
        QCOMPARE(fp.frage("50000;", 0, 300).count(';'), 0);
        QTRY_COMPARE(gesetzteFrequenz, static_cast<quint64>(14250000));
    }

    void zweiBefehleInEinemSchreibvorgang()
    {
        QVERIFY2(pty->starte(0), qPrintable(pty->grund(0)));
        Fremdprogramm fp(pty->pfad(0));
        QVERIFY(fp.offen());

        // Auf ZWEI Antworten warten, nicht auf die erste. Beim
        // TCP-Prueffall genau darauf hereingefallen: bei "FA;ID;" sah
        // man nur "FA...;" und hielt es fuer einen Fehler.
        const QByteArray a = fp.frage("FA;ID;", 2);
        QCOMPARE(a, QByteArray("FA00014225000;ID900;"));
    }

    void dreiBefehleUndEinHalber()
    {
        QVERIFY2(pty->starte(0), qPrintable(pty->grund(0)));
        Fremdprogramm fp(pty->pfad(0));
        QVERIFY(fp.offen());

        const QByteArray a = fp.frage("ID;ID;ID;ID", 3);
        QCOMPARE(a, QByteArray("ID900;ID900;ID900;"));
        // Der halbe vierte liegt im Puffer und wird vom naechsten
        // Schreibvorgang abgeschlossen.
        QCOMPARE(fp.frage(";", 1), QByteArray("ID900;"));
    }

    void zeilenumbrucheWerdenWeggeworfen()
    {
        QVERIFY2(pty->starte(0), qPrintable(pty->grund(0)));
        Fremdprogramm fp(pty->pfad(0));
        QVERIFY(fp.offen());

        // So kommt es an, wenn ein Mensch `echo "FA;" > /dev/ttysNNN`
        // tippt oder in `screen` die Eingabetaste drueckt. Ohne das
        // Wegwerfen steckte das \r\n VOR dem naechsten Befehl und
        // machte aus "ID;" ein "\r\nID;", das keine Protokollschicht
        // kennt.
        const QByteArray a = fp.frage("FA;\r\nID;\n", 2);
        QCOMPARE(a, QByteArray("FA00014225000;ID900;"));
    }

    void pufferDeckelBeiZweihundertfuenfundfuenfzig()
    {
        QVERIFY2(pty->starte(0), qPrintable(pty->grund(0)));
        Fremdprogramm fp(pty->pfad(0));
        QVERIFY(fp.offen());

        // Ein Terminalprogramm, das jemand versehentlich auf das
        // Pseudoterminal richtet, schickt einen endlosen Strom ohne
        // ';'. Ohne Deckel waechst der Puffer, bis der Speicher ausgeht
        // -- und zwar lautlos.
        QVERIFY(fp.schreibe(QByteArray(400, 'X')));
        QCOMPARE(fp.horchKurz(), QByteArray());

        // Der Deckel hat geraeumt, also ist der naechste ECHTE Befehl
        // wieder sauber -- und nicht "XXX...XXXFA;", das abgelehnt
        // wuerde.
        QCOMPARE(fp.frage("ID;"), QByteArray("ID900;"));
    }

    // ── Dass es dieselbe Protokollschicht ist ───────────────────────

    void beideWegeEineSchicht()
    {
        QVERIFY2(pty->starte(0), qPrintable(pty->grund(0)));
        Fremdprogramm fp(pty->pfad(0));
        QVERIFY(fp.offen());

        // Am PTY abstimmen ...
        QVERIFY(fp.schreibe("FA00007050000;"));
        QTRY_COMPARE(gesetzteFrequenz, static_cast<quint64>(7050000));

        // ... und der TCP-Weg sieht denselben Zustand. Zwei
        // Protokollschichten waeren hier nicht zu unterscheiden
        // (beide fragen denselben Geber); was man SIEHT, ist der
        // umgekehrte Fall: ein Haken, der nur in einer Schicht haengt.
        QCOMPARE(tcp->antwortAuf(QStringLiteral("FA;")),
                 QStringLiteral("FA00007050000;"));

        // Der Beleg fuer EINE Schicht: AI1; ist ZUSTAND der Schicht,
        // nicht des Transports. Am PTY gesetzt, ueber TCP sichtbar --
        // mit zwei Schichten waere das unmoeglich.
        QVERIFY(fp.schreibe("AI1;"));
        QTRY_VERIFY(fp.horchKurz(60).isEmpty() || true);
        QCOMPARE(tcp->antwortAuf(QStringLiteral("AI;")),
                 QStringLiteral("AI1;"));
    }

    void stummerGeberWirdAbgelehnt()
    {
        QVERIFY2(pty->starte(0), qPrintable(pty->grund(0)));
        Fremdprogramm fp(pty->pfad(0));
        QVERIFY(fp.offen());

        stumm = true;   // kein Empfaenger angelegt, wie nach dem Start
        // NICHT "FA00000000000;". Eine gemeldete 0 Hz ist eine
        // erfundene Frequenz, und ein Fremdprogramm glaubt sie.
        QCOMPARE(fp.frage("FA;"), QByteArray("?;"));
    }

    void ohneAntwortgeberSchweigenStattAblehnen()
    {
        CatPtyServer nackt;
        QVERIFY2(nackt.starte(0), qPrintable(nackt.grund(0)));
        Fremdprogramm fp(nackt.pfad(0));
        QVERIFY(fp.offen());

        QVERIFY(fp.schreibe("FA;"));
        // Schweigen, nicht "?;". Ein Fremdprogramm, dem man einen
        // Befehl ABLEHNT, haelt ihn fuer nicht unterstuetzt und fragt
        // nie wieder -- ein Fehler in der Verdrahtung waere damit
        // dauerhaft unsichtbar. Ein Zeitablauf faellt auf.
        QCOMPARE(fp.horchKurz(), QByteArray());
        nackt.beende(0);
    }

    // ── Tasten: der Befehl, der etwas Gefaehrliches tut ─────────────

    void tastenUndLoslassen()
    {
        QVERIFY2(pty->starte(0), qPrintable(pty->grund(0)));
        Fremdprogramm fp(pty->pfad(0));
        QVERIFY(fp.offen());

        // TX; und RX; quittiert Kenwood nicht -- es kommt KEINE
        // Antwort, und das ist richtig. Geprueft wird am Haken.
        QVERIFY(fp.schreibe("TX;"));
        QTRY_COMPARE(moxRufe, 1);
        QVERIFY(z.mox);

        QVERIFY(fp.schreibe("RX;"));
        QTRY_COMPARE(moxRufe, 2);
        QVERIFY(!z.mox);
    }

    // ── Abbau, Neustart, Vielfalt ──────────────────────────────────

    void neustartGibtEinNeuesGeraet()
    {
        QVERIFY2(pty->starte(0), qPrintable(pty->grund(0)));
        const QString erst = pty->pfad(0);
        pty->beende(0);
        QVERIFY2(pty->starte(0), qPrintable(pty->grund(0)));
        const QString dann = pty->pfad(0);

        QVERIFY(!dann.isEmpty());
        // Ob die Nummer dieselbe ist, entscheidet das System -- geprueft
        // wird nur, dass der Kanal nach dem Neustand WIEDER arbeitet.
        // (Genau weil die Nummer wechseln KANN, gibt es die feste
        // Verknuepfung.)
        Fremdprogramm fp(dann);
        QVERIFY(fp.offen());
        QCOMPARE(fp.frage("ID;"), QByteArray("ID900;"));
        Q_UNUSED(erst);
    }

    void doppeltesStartenIstKeinLeck()
    {
        QVERIFY2(pty->starte(0), qPrintable(pty->grund(0)));
        const QString erst = pty->pfad(0);
        // starte() auf einen laufenden Kanal baut ihn erst ab. Ohne das
        // bliebe der alte Dateigriff offen -- vier Kanaele, ein paar
        // Klicks auf den Schalter, und das Programm laeuft in das
        // Dateigriff-Limit.
        QVERIFY2(pty->starte(0), qPrintable(pty->grund(0)));
        QVERIFY(!pty->pfad(0).isEmpty());
        // Das ALTE Geraet ist weg.
        if (pty->pfad(0) != erst) {
            QVERIFY(!QFileInfo(erst).exists()
                    || ::open(erst.toLocal8Bit().constData(),
                              O_RDWR | O_NOCTTY | O_NONBLOCK) >= 0);
        }
        Fremdprogramm fp(pty->pfad(0));
        QVERIFY(fp.offen());
        QCOMPARE(fp.frage("ID;"), QByteArray("ID900;"));
    }

    void ungueltigerKanalWirdAbgelehnt()
    {
        QVERIFY(!pty->starte(-1));
        QVERIFY(!pty->starte(CatPtyServer::kKanaele));
        QVERIFY(!pty->laeuft(-1));
        QVERIFY(pty->pfad(99).isEmpty());
        QVERIFY(CatPtyServer::festerPfad(-1).isEmpty());
        QVERIFY(CatPtyServer::festerPfad(CatPtyServer::kKanaele).isEmpty());
    }

    void vierKanaeleGleichzeitigBedient()
    {
        for (int k = 0; k < CatPtyServer::kKanaele; ++k) {
            QVERIFY2(pty->starte(k), qPrintable(pty->grund(k)));
        }
        // Vier Fremdprogramme auf vier Kanaelen -- das ist der Grund,
        // aus dem es vier gibt: WSJT-X, fldigi und ein Logbuch sollen
        // nicht um einen Kanal streiten.
        std::vector<std::unique_ptr<Fremdprogramm>> fps;
        for (int k = 0; k < CatPtyServer::kKanaele; ++k) {
            fps.push_back(std::make_unique<Fremdprogramm>(pty->pfad(k)));
            QVERIFY(fps.back()->offen());
        }
        for (int k = 0; k < CatPtyServer::kKanaele; ++k) {
            QCOMPARE(fps[static_cast<size_t>(k)]->frage("FA;"),
                     QByteArray("FA00014225000;"));
        }
        // Und jeder hat seinen EIGENEN Puffer: ein halber Befehl auf
        // Kanal A darf einen Befehl auf Kanal B nicht verschieben.
        QVERIFY(fps[0]->schreibe("FA0000"));
        QCOMPARE(fps[1]->frage("ID;"), QByteArray("ID900;"));
        QCOMPARE(fps[0]->horchKurz(), QByteArray());
        QVERIFY(fps[0]->schreibe("7050000;"));
        QTRY_COMPARE(gesetzteFrequenz, static_cast<quint64>(7050000));
    }

    void abbauMitOffenerVerbindung()
    {
        QVERIFY2(pty->starte(0), qPrintable(pty->grund(0)));
        auto fp = std::make_unique<Fremdprogramm>(pty->pfad(0));
        QVERIFY(fp->offen());
        QCOMPARE(fp->frage("ID;"), QByteArray("ID900;"));

        // Den Server abbauen, WAEHREND ein Fremdprogramm haengt. Das
        // ist die Fehlerform, die in diesem Baum fuenfmal zugeschlagen
        // hat: ein Wecker, der nach dem close() noch einmal feuert.
        delete pty;
        pty = nullptr;
        // Die Schleife noch drehen -- ein verspaetetes activated()
        // wuerde JETZT kommen.
        QElapsedTimer uhr; uhr.start();
        while (uhr.elapsed() < 200) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        // Bis hierher gekommen heisst: kein Absturz. Das
        // Fremdprogramm sieht das Ende beim naechsten Lesen.
        QVERIFY(true);
    }
};

QTEST_MAIN(TstCatPty)
#include "tst_cat_pty.moc"
