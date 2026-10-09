// no-port-check: Longpath-eigener Pruefstand, kein Port.
// =================================================================
// tests/tst_kpa500_verbindung.cpp  (Longpath)
// =================================================================
//
// Elecraft KPA500: der Transport.
//
// Kein Geraet am Kabel. Stattdessen ein eigener QTcpServer auf
// 127.0.0.1, der die Antworten des Verstaerkers nachspricht: er liest
// die Abfragen, antwortet mit `^XX...;`, und -- das ist hier der
// interessante Teil -- kann sich in den BOOT-ZUSTAND stellen, in dem er
// auf `^XX;` schweigt und nur auf 'I' mit "KPA500" antwortet.
//
// Genau dieser Zustand ist der Grund, warum es diesen Pruefstand gibt.
// Beim SPE (tst_spe_verbindung) kann man „ausgeschaltet" nicht von
// „nicht da" unterscheiden; der KPA500 kann es, weil Elecrafts
// Beschreibung den Boot-Zustand dokumentiert. Ein Weg, der nur auf
// Papier funktioniert, ist kein Weg -- also wird er hier abgefahren.
//
// Die Zeiten sind gestaucht (setPollIntervalMs/setSilentPollLimit),
// sonst dauerte die Stilleerkennung drei Sekunden je Fall.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 -- Neu. Martin Fischer, AI-assisted via Anthropic
//                 Claude (Claude Code).
// =================================================================

#include <QtTest>

#include <QByteArray>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

#include "core/Kpa500Connection.h"
#include "core/Kpa500Protocol.h"

using namespace Longpath;

namespace {

// Der Kunstverstaerker. Zwei Betriebsarten, wie das echte Geraet:
//
//   laufend     -> antwortet auf `^XX;`
//   Boot        -> schweigt auf `^XX;`, antwortet auf 'I' mit "KPA500",
//                  und 'P' schaltet ihn (hier) auf laufend
class KunstKpa : public QObject
{
    Q_OBJECT
public:
    explicit KunstKpa(QObject* parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            m_peer = m_server.nextPendingConnection();
            connect(m_peer, &QTcpSocket::readyRead, this, &KunstKpa::lesen);
        });
    }

    bool hinstellen() { return m_server.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return m_server.serverPort(); }

    // Zustand des Kunstgeraets.
    bool laeuft{true};
    bool stumm{false};        // antwortet auf GAR nichts (falscher Anschluss)
    int  watt{250};
    int  swrMalZehn{15};      // 15 -> 1,5
    int  voltMalZehn{475};
    int  ampMalZehn{320};
    int  tempC{42};
    bool operate{true};
    int  band{5};
    int  fehler{0};

    QByteArray empfangen() const { return m_empfangen; }
    void       empfangenLeeren() { m_empfangen.clear(); }
    int        abfragen() const { return m_abfragen; }
    int        identifyAbfragen() const { return m_identify; }

private slots:
    void lesen()
    {
        if (!m_peer) { return; }
        const QByteArray stueck = m_peer->readAll();
        m_empfangen.append(stueck);
        if (stumm) { return; }

        // ── ERST die gerahmten Befehle heraustrennen ────────────────
        //
        // Der erste Anlauf hat den ganzen Strom Byte fuer Byte nach 'I'
        // und 'P' durchsucht -- und damit auf `^VI;` mit "KPA500"
        // geantwortet, weil darin ein 'I' steckt. Folge: der Pruefstand
        // erkannte den Boot-Zustand auch dann, wenn das Erzeugnis gar
        // kein eigenes 'I' schickte, und die Gegenprobe „kein 'I'"
        // lief gruen durch.
        //
        // Jetzt werden die `^...;`-Stuecke zuerst herausgeschnitten;
        // nur was uebrig bleibt, gilt als Einzelbefehl des Boot-Laders.
        // Das ist die vorsichtige Annahme: ob ein echter KPA500 das 'I'
        // in `^VI;` als Identify liest, sagt die Beschreibung nicht, und
        // darauf darf sich kein Pruefstand stuetzen.
        QByteArray gerahmt;
        QByteArray uebrig;
        {
            QByteArray rest = stueck;
            while (true) {
                const int anf = rest.indexOf('^');
                if (anf < 0) { break; }
                const int ende = rest.indexOf(';', anf);
                if (ende < 0) { break; }
                uebrig.append(rest.left(anf));
                gerahmt.append(rest.mid(anf, ende - anf + 1));
                rest.remove(0, ende + 1);
            }
            uebrig.append(rest);
        }

        // Einzelne Buchstaben: gelten nur im Boot-Zustand.
        for (char c : uebrig) {
            if (c == 'I') {
                ++m_identify;
                if (!laeuft) { m_peer->write(QByteArrayLiteral("KPA500")); }
            } else if (c == 'P') {
                laeuft = true;   // genau das tut 'P' am echten Geraet
            }
        }

        // Dann die gerahmten Befehle -- aber nur, wenn er laeuft.
        if (!laeuft) { return; }
        while (true) {
            const int anf = gerahmt.indexOf('^');
            if (anf < 0) { break; }
            const int ende = gerahmt.indexOf(';', anf);
            if (ende < 0) { break; }
            const QString befehl = QString::fromLatin1(gerahmt.mid(anf + 1, ende - anf - 1));
            gerahmt.remove(0, ende + 1);
            antworten(befehl);
        }
    }

private:
    void antworten(const QString& befehl)
    {
        ++m_abfragen;
        auto schick = [this](const QString& s) { m_peer->write(s.toLatin1()); };
        // Drei Buchstaben haben nur diese vier Befehle (Elecraft Rev A2,
        // Uebersicht). Alles andere hat zwei.
        //
        // Der erste Anlauf hat geraten -- „ist das dritte Zeichen ein
        // Buchstabe, sind es drei" -- und damit `^FLC;` als Verb "FLC"
        // gelesen statt als "FL" mit den Daten "C". Der Fehler lag im
        // PRUEFWERKZEUG, nicht im Erzeugnis, sah aber genauso aus wie
        // ein Fehler darin (`einFehlerWirdGemeldetUndLaesstSichLoeschen`
        // fiel).
        static const QStringList dreiBuchstaben = {
            QStringLiteral("RVM"), QStringLiteral("BRP"),
            QStringLiteral("BRX"), QStringLiteral("DMO"),
        };
        QString verb = befehl.left(3);
        if (!dreiBuchstaben.contains(verb)) {
            verb = befehl.left(2);
        }
        const QString daten = befehl.mid(verb.size());

        if (verb == QStringLiteral("WS")) {
            schick(QStringLiteral("^WS%1 %2;")
                       .arg(watt, 3, 10, QLatin1Char('0'))
                       .arg(swrMalZehn, 3, 10, QLatin1Char('0')));
        } else if (verb == QStringLiteral("VI")) {
            schick(QStringLiteral("^VI%1 %2;")
                       .arg(voltMalZehn, 3, 10, QLatin1Char('0'))
                       .arg(ampMalZehn, 3, 10, QLatin1Char('0')));
        } else if (verb == QStringLiteral("TM")) {
            schick(QStringLiteral("^TM%1;").arg(tempC, 3, 10, QLatin1Char('0')));
        } else if (verb == QStringLiteral("OS")) {
            if (!daten.isEmpty()) { operate = (daten == QStringLiteral("1")); }
            schick(QStringLiteral("^OS%1;").arg(operate ? 1 : 0));
        } else if (verb == QStringLiteral("BN")) {
            if (!daten.isEmpty()) { band = daten.toInt(); }
            schick(QStringLiteral("^BN%1;").arg(band, 2, 10, QLatin1Char('0')));
        } else if (verb == QStringLiteral("FL")) {
            if (daten == QStringLiteral("C")) { fehler = 0; }
            schick(QStringLiteral("^FL%1;").arg(fehler, 2, 10, QLatin1Char('0')));
        } else if (verb == QStringLiteral("ON")) {
            if (daten == QStringLiteral("0")) { laeuft = false; return; }
            schick(QStringLiteral("^ON1;"));
        } else if (verb == QStringLiteral("RVM")) {
            schick(QStringLiteral("^RVM01.04;"));
        } else if (verb == QStringLiteral("SN")) {
            schick(QStringLiteral("^SN01234;"));
        }
    }

    QTcpServer  m_server;
    QTcpSocket* m_peer{nullptr};
    QByteArray  m_empfangen;
    int         m_abfragen{0};
    int         m_identify{0};
};

}  // namespace

class TstKpa500Verbindung : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void dieMesswerteKommenAn();
    void dieFesteAuskunftWirdNurEinmalGeholt();
    void einBefehlGehtAufDenDraht();
    void derBootZustandWirdErkanntNichtMitStilleVerwechselt();
    void ausUndWiederEin();
    void wirklichNiemandDaIstStill();
    void einFehlerWirdGemeldetUndLaesstSichLoeschen();
    void keinRueckrufAusDemAbbau();

private:
    bool verbinden();

    KunstKpa*         m_amp{nullptr};
    Kpa500Connection* m_verb{nullptr};
};

void TstKpa500Verbindung::init()
{
    m_amp = new KunstKpa(this);
    QVERIFY(m_amp->hinstellen());
    m_verb = new Kpa500Connection(this);
    m_verb->setPollIntervalMs(20);
    m_verb->setSilentPollLimit(5);
    m_verb->setReconnectIntervalMs(50);
}

void TstKpa500Verbindung::cleanup()
{
    delete m_verb;
    m_verb = nullptr;
    delete m_amp;
    m_amp = nullptr;
}

bool TstKpa500Verbindung::verbinden()
{
    QSignalSpy auf(m_verb, &Kpa500Connection::connected);
    m_verb->connectNetwork(QStringLiteral("127.0.0.1"), m_amp->port());
    return auf.wait(3000);
}

// ─────────────────────────────────────────────────────────────────────

void TstKpa500Verbindung::dieMesswerteKommenAn()
{
    QSignalSpy mess(m_verb, &Kpa500Connection::telemetryUpdated);
    QVERIFY(verbinden());
    QVERIFY2(mess.wait(2000), "Nach dem Verbinden kamen keine Messwerte");

    QTRY_COMPARE_WITH_TIMEOUT(m_verb->liveness(),
                              Kpa500Connection::Liveness::Running, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->lastPowerSwr().watts, 250, 2000);
    QCOMPARE(m_verb->lastPowerSwr().swr, 1.5f);
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->lastVoltsAmps().volts, 47.5f, 2000);
    QCOMPARE(m_verb->lastVoltsAmps().amps, 32.0f);
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->lastTemperature(), 42, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->lastBandIndex(), 5, 2000);
    QVERIFY(m_verb->isOperate());
    QCOMPARE(m_verb->sourceLabel(), QStringLiteral("NETWORK"));
}

void TstKpa500Verbindung::dieFesteAuskunftWirdNurEinmalGeholt()
{
    // Firmware und Seriennummer aendern sich nie -- sie gehoeren nicht in
    // den Takt. Geholt werden sie beim Verbinden, und danach nie wieder.
    QVERIFY(verbinden());
    QTRY_VERIFY_WITH_TIMEOUT(!m_verb->firmware().isEmpty(), 2000);
    QCOMPARE(m_verb->firmware(), QStringLiteral("01.04"));
    QCOMPARE(m_verb->serialNumber(), QStringLiteral("01234"));

    // Lange genug weiterlaufen lassen, dass der Takt mehrfach durch ist.
    const int vorher = m_amp->abfragen();
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->abfragen() >= vorher + 30, 3000);
    const int rvm = m_amp->empfangen().count(QByteArrayLiteral("^RVM;"));
    QCOMPARE(rvm, 1);
    QCOMPARE(m_amp->empfangen().count(QByteArrayLiteral("^SN;")), 1);
}

void TstKpa500Verbindung::einBefehlGehtAufDenDraht()
{
    QVERIFY(verbinden());
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->abfragen() >= 6, 2000);
    m_amp->empfangenLeeren();

    m_verb->setOperate(false);
    QTRY_VERIFY_WITH_TIMEOUT(
        m_amp->empfangen().contains(QByteArrayLiteral("^OS0;")), 2000);
    // Und das Geraet meldet den neuen Zustand von selbst zurueck -- die
    // Anzeige folgt der Antwort, nicht dem Knopfdruck.
    QTRY_VERIFY_WITH_TIMEOUT(!m_verb->isOperate(), 2000);

    m_verb->setBand(2);
    QTRY_VERIFY_WITH_TIMEOUT(
        m_amp->empfangen().contains(QByteArrayLiteral("^BN02;")), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->lastBandIndex(), 2, 2000);

    // Ein Band, das es nicht gibt, geht NICHT hinaus.
    m_amp->empfangenLeeren();
    m_verb->setBand(11);
    QTest::qWait(200);
    QVERIFY2(!m_amp->empfangen().contains(QByteArrayLiteral("^BN11;")),
             "Ein Band, das der KPA500 nicht kennt, darf nicht hinausgehen");
}

void TstKpa500Verbindung::derBootZustandWirdErkanntNichtMitStilleVerwechselt()
{
    // DER FALL, UM DEN ES GEHT. Der Verstaerker ist an der Front aus,
    // haengt aber am Draht. Auf `^XX;` antwortet er nicht, auf 'I' schon.
    // Das darf nicht als „still" durchgehen, sonst waere der
    // Einschaltknopf grundlos grau.
    QSignalSpy leben(m_verb, &Kpa500Connection::livenessChanged);
    m_amp->laeuft = false;
    QVERIFY(verbinden());

    QTRY_COMPARE_WITH_TIMEOUT(m_verb->liveness(),
                              Kpa500Connection::Liveness::BootMode, 4000);
    QVERIFY2(m_amp->identifyAbfragen() > 0,
             "Es wurde nie ein 'I' geschickt -- dann kann der Boot-Zustand "
             "gar nicht erkannt werden");

    // Zwischenzustand „Silent" darf dabei auftreten (erst merkt er die
    // Stille, dann horcht er nach), aber BootMode muss das letzte Wort
    // haben.
    QCOMPARE(m_verb->liveness(), Kpa500Connection::Liveness::BootMode);
    QVERIFY(!leben.isEmpty());
}

void TstKpa500Verbindung::ausUndWiederEin()
{
    QVERIFY(verbinden());
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->liveness(),
                              Kpa500Connection::Liveness::Running, 2000);

    // ^ON0; schaltet ihn aus -- danach schweigt er auf alles Regulaere.
    m_verb->powerOff();
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->liveness(),
                              Kpa500Connection::Liveness::BootMode, 4000);

    // Und 'P' holt ihn zurueck. Elecraft: „No response is sent ... after
    // receiving this command" -- dass es geklappt hat, zeigt erst die
    // naechste regulaere Antwort.
    m_amp->empfangenLeeren();
    m_verb->powerOn();
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->empfangen().contains('P'), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->liveness(),
                              Kpa500Connection::Liveness::Running, 4000);
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->lastTemperature(), 42, 2000);
}

void TstKpa500Verbindung::wirklichNiemandDaIstStill()
{
    // Die Gegenprobe zum Boot-Zustand: antwortet auch 'I' nicht, dann ist
    // es wirklich still -- falscher Anschluss, falsche Datenrate, oder
    // niemand da. Das muss von „aus, aber erreichbar" UNTERSCHIEDEN
    // werden, sonst sagt die Anzeige das Falsche.
    m_amp->stumm = true;
    QVERIFY(verbinden());
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->liveness(),
                              Kpa500Connection::Liveness::Silent, 4000);
    QVERIFY2(m_verb->isConnected(),
             "Die Leitung steht -- getrennt ist es nicht");
    // Und er horcht weiter nach, damit ein spaeteres Einschalten auffaellt.
    const int vorher = m_amp->identifyAbfragen();
    m_amp->stumm = false;
    m_amp->laeuft = false;
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->identifyAbfragen() > vorher, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->liveness(),
                              Kpa500Connection::Liveness::BootMode, 4000);
}

void TstKpa500Verbindung::einFehlerWirdGemeldetUndLaesstSichLoeschen()
{
    QSignalSpy fehler(m_verb, &Kpa500Connection::faultChanged);
    QVERIFY(verbinden());
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->liveness(),
                              Kpa500Connection::Liveness::Running, 2000);
    QCOMPARE(m_verb->lastFaultCode(), 0);

    m_amp->fehler = 3;
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->lastFaultCode(), 3, 2000);
    QVERIFY(!fehler.isEmpty());
    QCOMPARE(fehler.last().at(0).toInt(), 3);

    // ^FLC; loescht ihn.
    m_verb->clearFault();
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->lastFaultCode(), 0, 2000);
}

void TstKpa500Verbindung::keinRueckrufAusDemAbbau()
{
    // Dieselbe Falle wie bei SpeConnection: m_socket ist VOR den
    // Zeitgebern erklaert und wird nach ihnen abgebaut; ~QTcpSocket loest
    // disconnected() aus. Ohne Destruktor greift onTransportDown() in
    // schon zerstoerte Zeitgeber -- sichtbar nur als die irrefuehrende
    // Warnung ueber Zeitgeber in fremden Faeden.
    static QStringList warnungen;
    static QtMessageHandler voriger = nullptr;
    warnungen.clear();

    auto* verb = new Kpa500Connection();
    verb->setPollIntervalMs(20);
    verb->setAutoReconnect(true);
    QSignalSpy auf(verb, &Kpa500Connection::connected);
    verb->connectNetwork(QStringLiteral("127.0.0.1"), m_amp->port());
    QVERIFY(auf.wait(3000));
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->abfragen() >= 1, 2000);

    voriger = qInstallMessageHandler([](QtMsgType typ,
                                        const QMessageLogContext& ctx,
                                        const QString& text) {
        if (typ == QtWarningMsg || typ == QtCriticalMsg || typ == QtFatalMsg) {
            warnungen.append(text);
        }
        if (voriger) { voriger(typ, ctx, text); }
    });
    delete verb;
    qInstallMessageHandler(voriger);
    voriger = nullptr;

    for (const QString& w : warnungen) {
        QVERIFY2(!w.contains(QStringLiteral("Timers cannot be started"))
                     && !w.contains(QStringLiteral("startTimer")),
                 qPrintable(QStringLiteral("Beim Abbau wurde in das halb "
                                           "abgebaute Objekt zurueckgerufen: %1")
                                .arg(w)));
    }
}

QTEST_MAIN(TstKpa500Verbindung)
#include "tst_kpa500_verbindung.moc"
