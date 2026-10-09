// SPE Expert: der Transport.
//
// Hier haengt kein Verstaerker am Kabel, und es wird auch keiner
// behauptet. Stattdessen steht ein eigener QTcpServer auf 127.0.0.1,
// der die Rahmen des Geraets nachspricht: er liest die Abfragen, die
// SpeConnection schickt, und antwortet mit demselben 67-Zeichen-String,
// den SPE in der eigenen Anleitung abgedruckt hat. Das ist genau die
// Topologie, fuer die diese Klasse gebaut ist (ser2net), nur mit einem
// Kunstverstaerker am anderen Ende.
//
// Was damit geprueft ist -- und es ist das, was ein Port am dringendsten
// braucht, weil hier die Entscheidungen liegen, nicht in der
// Bytearithmetik:
//
//   * Der erste Takt kommt SOFORT, nicht erst nach einem Intervall.
//     Sonst sieht der Betreiber beim Verbinden ein leeres Feld.
//   * Die Kennung wird beim ersten Antwortrahmen gemeldet, und nur
//     einmal, nicht bei jedem Takt neu.
//   * STILLE BEI STEHENDER LEITUNG. Der eine Zweig, der ohne echten
//     ser2net-Aufbau niemals auffaellt: der Verstaerker wird
//     ausgeschaltet, die TCP-Verbindung bleibt stehen, und nichts
//     passiert -- es sei denn, die unbeantworteten Takte werden
//     gezaehlt. Dazu `derTaktZaehltNurUnbeantworteteTakte`, der die
//     Gegenrichtung festnagelt: ein Verstaerker, der langsam aber
//     ueberhaupt antwortet, darf nicht fuer stumm erklaert werden.
//   * Die Tastendruecke gehen als die Byte-Folgen hinaus, die in der
//     Anleitung stehen -- hier an der Steckdose abgelesen, nicht am
//     Rueckgabewert einer Baufunktion (das macht tst_spe_protokoll).
//   * Die Einschaltimpulsfolge in der richtigen REIHENFOLGE, und die
//     Meldung am Ende sagt, was der Vermittler zugesagt hat, statt
//     Erfolg zu behaupten.
//   * Nach dem Wegbrechen wird wiederverbunden, nach einem gewollten
//     Trennen nicht.
//
// Was damit NICHT geprueft ist: ob ein echter 1.5K-FA sich so verhaelt.
// Die Zeiten sind fuer den Pruefstand gestaucht (siehe
// setPollIntervalMs/setPowerOnPulseScale im Kopf von SpeConnection.h);
// die Reihenfolge ist dieselbe, die Dauer nicht.

#include <QtTest>

#include <QByteArray>
#include <QList>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

#include "core/SpeConnection.h"
#include "core/SpeProtocol.h"

using namespace Longpath;

namespace {

// Der Zustandsstring aus der Anleitung selbst (§5): 2K-FA, STANDBY,
// Empfang, Band 160 m, Stufe LOW, 33 Grad.
const char* kBeispiel2K =
    "C,20K,S,R,x,1,00,1a,0r,L,0000, 0.00, 0.00, 0.0, 0.0, 33,  0,  0,N,N";
// Ein sendender 1.5K-FA mit Warnung.
const char* kBeispiel15K =
    "C,15K,O,T,A,2,05,2b,0r,H,1350, 1.10, 1.25, 47.5, 32.0, 45, 40, 38,S,N";

QByteArray zustandsRahmen(const char* nutzlast)
{
    const QByteArray p(nutzlast);
    quint16 summe = 0;
    for (char c : p) {
        summe = static_cast<quint16>(summe + static_cast<quint8>(c));
    }
    QByteArray r;
    r.append(3, static_cast<char>(0xAA));
    r.append(static_cast<char>(p.size()));
    r.append(p);
    r.append(static_cast<char>(summe & 0xFF));
    r.append(static_cast<char>((summe >> 8) & 0xFF));
    r.append('\r');
    r.append('\n');
    return r;
}

// Der Kunstverstaerker: nimmt eine Verbindung an, sammelt alles, was
// hereinkommt, und antwortet auf jede Zustandsabfrage (0x90) -- solange
// `antwortet` gesetzt ist. Das Ausschalten des Verstaerkers bei
// stehender Leitung ist genau `antwortet = false`.
class KunstVerstaerker : public QObject
{
    Q_OBJECT
public:
    explicit KunstVerstaerker(QObject* parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            m_peer = m_server.nextPendingConnection();
            connect(m_peer, &QTcpSocket::readyRead, this, &KunstVerstaerker::lesen);
        });
    }

    bool hinstellen()
    {
        return m_server.listen(QHostAddress::LocalHost, 0);
    }
    quint16 port() const { return m_server.serverPort(); }

    void verbindungKappen()
    {
        if (m_peer) {
            m_peer->abort();
            m_peer->deleteLater();
            m_peer = nullptr;
        }
    }

    bool    antwortet{true};
    // Antwortet nur auf jede n-te Abfrage (1 = auf jede). Damit ist ein
    // langsamer, aber antwortender Verstaerker nachstellbar, OHNE auf
    // Wanduhrzeiten zu wetten -- das Verhaeltnis liegt fest, nicht die
    // Dauer.
    int     antwortJedeNte{1};
    QString zustand{QString::fromLatin1(kBeispiel2K)};

    int        abfragen() const { return m_abfragen; }
    QByteArray empfangen() const { return m_empfangen; }
    void       empfangenLeeren() { m_empfangen.clear(); }
    bool       hatGegenstelle() const { return m_peer != nullptr; }

private slots:
    void lesen()
    {
        if (!m_peer) { return; }
        const QByteArray stueck = m_peer->readAll();
        m_empfangen.append(stueck);

        // Jeden vollstaendigen Ein-Byte-Rahmen vom Wirt heraussuchen und,
        // wenn es eine Zustandsabfrage war, antworten.
        for (int i = 0; i + 5 <= stueck.size(); ++i) {
            if (static_cast<quint8>(stueck.at(i)) != 0x55) { continue; }
            if (static_cast<quint8>(stueck.at(i + 1)) != 0x55) { continue; }
            if (static_cast<quint8>(stueck.at(i + 2)) != 0x55) { continue; }
            if (static_cast<quint8>(stueck.at(i + 3)) != 0x01) { continue; }
            const quint8 befehl = static_cast<quint8>(stueck.at(i + 4));
            if (befehl == 0x90) {
                ++m_abfragen;
                const bool dran = (antwortJedeNte <= 1)
                                  || (m_abfragen % antwortJedeNte == 0);
                if (antwortet && dran) {
                    m_peer->write(zustandsRahmen(zustand.toLatin1().constData()));
                }
            } else {
                // Quittung: der Verstaerker spiegelt den Tastendruck.
                QByteArray quittung;
                quittung.append(3, static_cast<char>(0xAA));
                quittung.append(static_cast<char>(0x01));
                quittung.append(static_cast<char>(befehl));
                quittung.append(static_cast<char>(befehl));
                if (antwortet) { m_peer->write(quittung); }
            }
        }
    }

private:
    QTcpServer  m_server;
    QTcpSocket* m_peer{nullptr};
    QByteArray  m_empfangen;
    int         m_abfragen{0};
};

// ── Nachrichtenfaenger fuer `keinRueckrufAusDemAbbau` ───────────────
//
// Der Fehler, den dieser Fall festnagelt, stuerzt nicht ab -- er meldet
// sich NUR als Warnung. Ein Pruefstand, der Warnungen nicht liest, haette
// ihn nie gesehen (und hat ihn in der ersten Fassung dieses Pruefstands
// auch nicht gesehen: die Warnung stand fuenf Laeufe lang im Protokoll,
// waehrend 13 von 13 Faellen gruen meldeten).
QStringList g_warnungen;
QtMessageHandler g_voriger = nullptr;

void warnungenSammeln(QtMsgType typ, const QMessageLogContext& ctx, const QString& text)
{
    if (typ == QtWarningMsg || typ == QtCriticalMsg || typ == QtFatalMsg) {
        g_warnungen.append(text);
    }
    if (g_voriger) { g_voriger(typ, ctx, text); }
}

}  // namespace

class TstSpeVerbindung : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void derErsteTaktKommtSofort();
    void dieKennungKommtEinmalNichtJedenTakt();
    void dieStilleBeiStehenderLeitungWirdGemeldet();
    void derTaktZaehltNurUnbeantworteteTakte();
    void dieTastendrueckeGehenAlsNormRahmenHinaus();
    void dieEinschaltfolgeKommtInDerRichtigenReihenfolge();
    void dieImpulsmeldungBehauptetKeinenErfolg();
    void einZweiterImpulsWaehrendDesErstenWirdNichtAngefangen();
    void nachWegbrechenWirdWiederverbunden();
    void nachGewolltemTrennenNicht();
    void einUnlesbarerZustandWirdVerworfenNichtGemeldet();
    void keinRueckrufAusDemAbbau();

private:
    // Hilfsmittel: verbindet und wartet, bis es steht.
    bool verbinden();

    KunstVerstaerker* m_amp{nullptr};
    SpeConnection*    m_verb{nullptr};
};

void TstSpeVerbindung::init()
{
    m_amp = new KunstVerstaerker(this);
    QVERIFY2(m_amp->hinstellen(), "Der Kunstverstaerker kam nicht an den Port");

    m_verb = new SpeConnection(this);
    // Gestauchte Zeiten -- siehe Kopf dieser Datei und von SpeConnection.h.
    m_verb->setPollIntervalMs(10);
    m_verb->setSilentPollLimit(5);
    m_verb->setPowerOnPulseScale(0.02);
    m_verb->setReconnectIntervalMs(50);   // im Betrieb 5 s
}

void TstSpeVerbindung::cleanup()
{
    delete m_verb;
    m_verb = nullptr;
    delete m_amp;
    m_amp = nullptr;
}

bool TstSpeVerbindung::verbinden()
{
    QSignalSpy auf(m_verb, &SpeConnection::connected);
    m_verb->connectNetwork(QStringLiteral("127.0.0.1"), m_amp->port());
    return auf.wait(2000);
}

// ─────────────────────────────────────────────────────────────────────

void TstSpeVerbindung::derErsteTaktKommtSofort()
{
    // Der Zeitgeber feuert erst nach einem vollen Intervall. Kaeme die
    // erste Abfrage von ihm, stuende das Bedienfeld so lange leer.
    //
    // Damit der Fall das wirklich prueft, wird der Takt hier auf eine
    // Minute gestellt: dann kann die Antwort NUR von der sofortigen
    // Abfrage kommen. (Mit den 10 ms aus init() lief der Fall gruen,
    // auch ohne die sofortige Abfrage -- der Zeitgeber holte sie in
    // 10 ms nach. Aufgefallen bei der Gegenprobe, nicht beim Schreiben.)
    m_verb->setPollIntervalMs(60000);

    QSignalSpy zustand(m_verb, &SpeConnection::statusUpdated);
    QVERIFY(verbinden());
    QVERIFY(m_verb->isConnected());

    QVERIFY2(zustand.wait(2000),
             "Nach dem Verbinden kam kein Zustand -- die erste Abfrage wird "
             "dem Zeitgeber ueberlassen, und der feuert erst nach einem "
             "vollen Intervall");
    QCOMPARE(m_amp->abfragen(), 1);
    QCOMPARE(m_verb->lastStatus().id, QStringLiteral("20K"));
    QVERIFY(m_verb->isResponding());
    QCOMPARE(m_verb->sourceLabel(), QStringLiteral("NETWORK"));
    QVERIFY(m_verb->description().startsWith(QStringLiteral("127.0.0.1:")));
}

void TstSpeVerbindung::dieKennungKommtEinmalNichtJedenTakt()
{
    QSignalSpy kennung(m_verb, &SpeConnection::modelChanged);
    QSignalSpy zustand(m_verb, &SpeConnection::statusUpdated);
    QVERIFY(verbinden());

    // Mindestens fuenf Antworten abwarten.
    while (zustand.size() < 5) {
        QVERIFY2(zustand.wait(1000), "Der Takt lief nicht weiter");
    }
    QCOMPARE(kennung.size(), 1);
    QCOMPARE(kennung.at(0).at(0).toString(), QStringLiteral("20K"));

    // Wechselt das Geraet die Kennung, kommt die Meldung erneut -- in der
    // Wirklichkeit nie, aber genau daran haengt, dass der Vergleich
    // gegen die GEMELDETE Kennung geht und nicht einmalig gesetzt wird.
    m_amp->zustand = QString::fromLatin1(kBeispiel15K);
    while (kennung.size() < 2) {
        QVERIFY2(kennung.wait(1000), "Die neue Kennung wurde nicht gemeldet");
    }
    QCOMPARE(kennung.at(1).at(0).toString(), QStringLiteral("15K"));
    QCOMPARE(m_verb->currentModelId(), QStringLiteral("15K"));
}

void TstSpeVerbindung::dieStilleBeiStehenderLeitungWirdGemeldet()
{
    // Der Fall, den es ohne ser2net gar nicht gibt: der Betreiber schaltet
    // den Verstaerker aus, die TCP-Verbindung bleibt oben. Kein
    // Socket-Abbruch, kein Fehler -- nur Stille.
    QSignalSpy antwortet(m_verb, &SpeConnection::respondingChanged);
    QSignalSpy weg(m_verb, &SpeConnection::disconnected);
    QVERIFY(verbinden());

    while (antwortet.isEmpty()) {
        QVERIFY2(antwortet.wait(1000), "Das erste Antworten wurde nicht gemeldet");
    }
    QCOMPARE(antwortet.at(0).at(0).toBool(), true);

    // Verstaerker aus, Leitung bleibt.
    m_amp->antwortet = false;
    while (antwortet.size() < 2) {
        QVERIFY2(antwortet.wait(2000), "Die Stille wurde nicht gemeldet");
    }
    QCOMPARE(antwortet.at(1).at(0).toBool(), false);
    QVERIFY(!m_verb->isResponding());

    QVERIFY2(m_verb->isConnected(),
             "Die Leitung stand noch -- die Verbindung darf nicht als "
             "getrennt gelten");
    QVERIFY2(weg.isEmpty(),
             "Stille ist kein Trennen: disconnected() darf nicht kommen");

    // Und der Takt laeuft weiter, damit der Verstaerker beim Einschalten
    // von selbst wieder auftaucht.
    const int vorher = m_amp->abfragen();
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->abfragen() > vorher, 2000);

    m_amp->antwortet = true;
    while (antwortet.size() < 3) {
        QVERIFY2(antwortet.wait(2000), "Die Rueckkehr wurde nicht gemeldet");
    }
    QCOMPARE(antwortet.at(2).at(0).toBool(), true);
}

void TstSpeVerbindung::derTaktZaehltNurUnbeantworteteTakte()
{
    // Die Gegenrichtung zum Fall oben: ein Verstaerker, der zwar
    // ANTWORTET, aber nur jeden dritten Takt, darf nicht fuer stumm
    // erklaert werden. Gezaehlt werden AUFEINANDERFOLGENDE
    // unbeantwortete Takte, nicht Takte -- und der Zaehler muss bei
    // jeder Antwort zurueckgesetzt werden.
    //
    // Nicht auf Wanduhrzeiten gewettet: der Kunstverstaerker antwortet
    // auf jeden dritten Takt, die Schwelle steht auf fuenf. Egal wie
    // viele Takte in der Wartezeit durchlaufen, es koennen nie fuenf
    // hintereinander unbeantwortet bleiben. (Der erste Anlauf dieses
    // Falls liess vier Takte verstreichen und wartete dann -- bei 10 ms
    // Takt und 50 ms Abfragekorn des Pruefrahmens waren es in
    // Wirklichkeit laengst mehr als fuenf, und der Fall fiel, ohne dass
    // am Code etwas falsch war.)
    m_amp->antwortJedeNte = 3;

    QSignalSpy antwortet(m_verb, &SpeConnection::respondingChanged);
    QSignalSpy zustand(m_verb, &SpeConnection::statusUpdated);
    QVERIFY(verbinden());
    while (antwortet.isEmpty()) {
        QVERIFY(antwortet.wait(2000));
    }
    QCOMPARE(antwortet.at(0).at(0).toBool(), true);

    // Lange genug laufen lassen, dass ein nicht zurueckgesetzter Zaehler
    // die Schwelle vielfach gerissen haette: mindestens 90 Takte.
    const int zielTakte = m_amp->abfragen() + 90;
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->abfragen() >= zielTakte, 5000);

    QVERIFY2(zustand.size() >= 20,
             qPrintable(QStringLiteral("nur %1 Antworten -- der Pruefstand "
                                       "hat nicht wirklich gelaufen")
                            .arg(zustand.size())));
    QCOMPARE(antwortet.size(), 1);
    QVERIFY2(m_verb->isResponding(),
             "Ein antwortender Verstaerker wurde fuer stumm erklaert");
}

void TstSpeVerbindung::dieTastendrueckeGehenAlsNormRahmenHinaus()
{
    QVERIFY(verbinden());
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->abfragen() >= 1, 1000);

    m_amp->empfangenLeeren();
    m_verb->toggleOperate();
    m_verb->tune();
    m_verb->cyclePowerLevel();
    m_verb->switchOff();
    m_verb->sendKey(Spe::Key::Antenna);
    m_verb->sendKey(Spe::Key::Input);

    // Auf der Leitung abgelesen, nicht am Rueckgabewert einer Baufunktion.
    QTRY_VERIFY_WITH_TIMEOUT(
        m_amp->empfangen().contains(QByteArray::fromHex("555555010d0d0d0a")), 2000);
    const QByteArray draht = m_amp->empfangen();
    QVERIFY2(draht.contains(QByteArray::fromHex("5555550109090d0a")), "TUNE fehlt");
    QVERIFY2(draht.contains(QByteArray::fromHex("555555010b0b0d0a")), "POWER fehlt");
    QVERIFY2(draht.contains(QByteArray::fromHex("555555010a0a0d0a")), "SWITCH OFF fehlt");
    QVERIFY2(draht.contains(QByteArray::fromHex("5555550104040d0a")), "ANTENNA fehlt");
    QVERIFY2(draht.contains(QByteArray::fromHex("5555550101010d0a")), "INPUT fehlt");
}

void TstSpeVerbindung::dieEinschaltfolgeKommtInDerRichtigenReihenfolge()
{
    QVERIFY(verbinden());
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->abfragen() >= 1, 1000);

    // Der Verstaerker soll waehrend des Impulses NICHT antworten -- so
    // sieht es in der Wirklichkeit aus (er ist ja aus), und so bleibt die
    // Leitung frei von Zustandsrahmen zwischen den Steuerrahmen.
    m_amp->antwortet = false;
    m_amp->empfangenLeeren();

    QSignalSpy fertig(m_verb, &SpeConnection::powerOnPulseFinished);
    m_verb->powerOn();
    QVERIFY2(fertig.wait(3000), "Die Impulsfolge wurde nie fertig");

    // Die Meldung kommt aus demselben Zeitgeberaufruf, der die letzten
    // Bytes schreibt -- der Kunstverstaerker hat sie dann noch nicht
    // gelesen. Also auf die Leitung warten, nicht auf die Meldung.
    // (Der erste Anlauf las hier sofort und vermisste Schritt 3: nicht
    // weil er fehlte, sondern weil er noch unterwegs war.)
    const QByteArray letzterSchritt =
        Spe::Rfc2217::buildSetControl(Spe::Rfc2217::kRtsOff);
    QTRY_VERIFY_WITH_TIMEOUT(
        m_amp->empfangen().lastIndexOf(letzterSchritt)
            > m_amp->empfangen().indexOf(
                  Spe::Rfc2217::buildSetControl(Spe::Rfc2217::kRtsOn)),
        2000);

    const QByteArray draht = m_amp->empfangen();
    // Erwartete Reihenfolge: WILL COM-PORT-OPTION, dann DTR an / RTS aus,
    // dann DTR aus / RTS AN (der eigentliche Impuls), dann DTR an /
    // RTS aus.
    const QByteArray will  = Spe::Rfc2217::buildWillComPortOption();
    const QByteArray dtrAn = Spe::Rfc2217::buildSetControl(Spe::Rfc2217::kDtrOn);
    const QByteArray dtrAus = Spe::Rfc2217::buildSetControl(Spe::Rfc2217::kDtrOff);
    const QByteArray rtsAn = Spe::Rfc2217::buildSetControl(Spe::Rfc2217::kRtsOn);
    const QByteArray rtsAus = Spe::Rfc2217::buildSetControl(Spe::Rfc2217::kRtsOff);

    const int iWill = draht.indexOf(will);
    QVERIFY2(iWill >= 0, "WILL COM-PORT-OPTION fehlt");
    // Schritt 1: DTR an + RTS aus.
    const int iDtrAn1 = draht.indexOf(dtrAn, iWill + will.size());
    QVERIFY2(iDtrAn1 > iWill, "Schritt 1 (DTR an) fehlt oder kam zu frueh");
    const int iRtsAus1 = draht.indexOf(rtsAus, iDtrAn1 + dtrAn.size());
    QVERIFY2(iRtsAus1 > iDtrAn1, "Schritt 1 (RTS aus) fehlt");
    // Schritt 2: DTR aus + RTS AN -- der Leistungsimpuls.
    const int iDtrAus = draht.indexOf(dtrAus, iRtsAus1 + rtsAus.size());
    QVERIFY2(iDtrAus > iRtsAus1, "Schritt 2 (DTR aus) fehlt");
    const int iRtsAn = draht.indexOf(rtsAn, iDtrAus + dtrAus.size());
    QVERIFY2(iRtsAn > iDtrAus,
             "Schritt 2 (RTS AN) fehlt -- der Leistungsimpuls liegt auf RTS, "
             "nicht auf DTR");
    // Schritt 3: zurueck in die Ruhelage, DTR an + RTS aus.
    const int iDtrAn2 = draht.indexOf(dtrAn, iRtsAn + rtsAn.size());
    QVERIFY2(iDtrAn2 > iRtsAn, "Schritt 3 (DTR an) fehlt");
    const int iRtsAus2 = draht.indexOf(rtsAus, iDtrAn2 + dtrAn.size());
    QVERIFY2(iRtsAus2 > iDtrAn2,
             "Schritt 3 (RTS aus) fehlt -- die Folge muss mit RTS unten enden");
}

void TstSpeVerbindung::dieImpulsmeldungBehauptetKeinenErfolg()
{
    // Ein Vermittler im Rohmodus antwortet auf WILL COM-PORT-OPTION gar
    // nicht und verschluckt jeden SET-CONTROL. Genau dann darf nicht
    // Erfolg gemeldet werden -- hier antwortet der Kunstverstaerker nie,
    // also muss "nie bestaetigt" herauskommen.
    QVERIFY(verbinden());
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->abfragen() >= 1, 1000);
    m_amp->antwortet = false;

    QSignalSpy fertig(m_verb, &SpeConnection::powerOnPulseFinished);
    m_verb->powerOn();
    QVERIFY(fertig.wait(3000));
    const auto zugesagt = fertig.at(0).at(0).value<Spe::Rfc2217::OptionReply>();
    QVERIFY2(zugesagt == Spe::Rfc2217::OptionReply::None,
             "Ohne Antwort des Vermittlers darf nicht Erfolg gemeldet werden");
}

void TstSpeVerbindung::einZweiterImpulsWaehrendDesErstenWirdNichtAngefangen()
{
    // Der Waechter in powerOn() (`m_powerOnStep >= 0`) soll einen zweiten
    // Impuls abweisen, solange der erste laeuft -- sonst setzt der zweite
    // Aufruf den Schrittzaehler auf 0 zurueck und der Verstaerker bekommt
    // einen abgehackten Impuls statt einer vollen Sekunde auf RTS.
    //
    // Gezaehlt wird WILL COM-PORT-OPTION auf der Leitung: je Impulsfolge
    // genau eins. Der erste Anlauf dieses Falls rief powerOn() dreimal
    // unmittelbar hintereinander -- das lief auch OHNE Waechter gruen,
    // weil alle drei Aufrufe denselben Einmal-Zeitgeber neu starten und
    // damit ohnehin nur eine Folge uebrig bleibt. Der Waechter greift
    // erst MITTEN in der Folge, also muss der zweite Aufruf auch dort
    // hineinfallen.
    QVERIFY(verbinden());
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->abfragen() >= 1, 1000);
    m_amp->antwortet = false;
    m_amp->empfangenLeeren();

    const QByteArray will  = Spe::Rfc2217::buildWillComPortOption();
    const QByteArray rtsAn = Spe::Rfc2217::buildSetControl(Spe::Rfc2217::kRtsOn);

    QSignalSpy fertig(m_verb, &SpeConnection::powerOnPulseFinished);
    m_verb->powerOn();

    // Warten, bis Schritt 2 heraus ist -- jetzt ist die Folge mitten
    // drin (Schritt 3 steht noch aus).
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->empfangen().contains(rtsAn), 2000);
    QVERIFY2(fertig.isEmpty(), "Die Folge war schon fertig -- der Fall prueft "
                               "dann nichts");

    m_verb->powerOn();   // muss verpuffen
    m_verb->powerOn();

    QVERIFY2(fertig.wait(3000), "Die Impulsfolge wurde nie fertig");
    QVERIFY2(!fertig.wait(300), "Eine zweite Impulsfolge lief an");
    QCOMPARE(fertig.size(), 1);
    QCOMPARE(m_amp->empfangen().count(will), 1);
}

void TstSpeVerbindung::nachWegbrechenWirdWiederverbunden()
{
    m_verb->setAutoReconnect(true);
    QVERIFY(verbinden());
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->abfragen() >= 1, 1000);

    QSignalSpy weg(m_verb, &SpeConnection::disconnected);
    QSignalSpy auf(m_verb, &SpeConnection::connected);
    m_amp->verbindungKappen();
    QVERIFY2(weg.wait(2000), "Das Wegbrechen wurde nicht gemeldet");
    QVERIFY(!m_verb->isConnected());

    // Der Wiederverbinder wartet im Betrieb 5 s; hier 50 ms (init()).
    QVERIFY2(auf.wait(3000), "Es wurde nicht wiederverbunden");
    QVERIFY(m_verb->isConnected());
}

void TstSpeVerbindung::nachGewolltemTrennenNicht()
{
    m_verb->setAutoReconnect(true);
    QVERIFY(verbinden());
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->abfragen() >= 1, 1000);

    QSignalSpy weg(m_verb, &SpeConnection::disconnected);
    QSignalSpy auf(m_verb, &SpeConnection::connected);
    m_verb->disconnect();
    QCOMPARE(weg.size(), 1);
    QVERIFY(!m_verb->isConnected());

    // Und es darf nichts nachkommen. In ZWEI Fenstern gemessen, nicht in
    // einem: beim Trennen koennen noch Bytes unterwegs sein, die der
    // Kunstverstaerker erst danach liest -- der Zaehler steigt also
    // einmal noch, ohne dass der Takt laeuft. (Daran fiel der erste
    // Anlauf dieses Falls, in einem von fuenf Laeufen. Der Code war in
    // Ordnung, die Messung nicht.) Was die Aussage traegt, ist das
    // ZWEITE Fenster: nach dem Leerlaufen kann nichts mehr kommen,
    // solange kein Zeitgeber laeuft.
    QTest::qWait(300);
    const int nachLeerlauf = m_amp->abfragen();
    QTest::qWait(300);
    QCOMPARE(m_amp->abfragen(), nachLeerlauf);

    // Und der Wiederverbinder darf nicht armiert sein. Die 600 ms oben
    // sind dafuer reichlich: init() stellt ihn auf 50 ms. (Mit den 5 s
    // des Betriebs waere dieser Satz nicht geprueft, sondern nur
    // behauptet -- die Gegenprobe "auch nach gewolltem Trennen
    // wiederverbinden" lief damit gruen durch.)
    QVERIFY2(auf.isEmpty(), "Nach gewolltem Trennen wurde wiederverbunden");
    QVERIFY(!m_verb->isConnected());
}

void TstSpeVerbindung::einUnlesbarerZustandWirdVerworfenNichtGemeldet()
{
    // Ein Rahmen mit richtiger Pruefsumme, aber zu wenigen Feldern: der
    // Parser gibt ihn heraus, parseStatus() lehnt ihn ab. Dann darf kein
    // Zustand gemeldet und der vorige nicht ueberschrieben werden.
    QSignalSpy zustand(m_verb, &SpeConnection::statusUpdated);
    QVERIFY(verbinden());
    while (zustand.isEmpty()) {
        QVERIFY(zustand.wait(1000));
    }
    QCOMPARE(m_verb->lastStatus().id, QStringLiteral("20K"));
    const int bisher = zustand.size();

    m_amp->zustand = QStringLiteral("C,20K,S,R,x,1,00");  // abgeschnitten
    QTest::qWait(200);
    QCOMPARE(zustand.size(), bisher);
    QCOMPARE(m_verb->lastStatus().id, QStringLiteral("20K"));

    // Und ein wieder gueltiger Rahmen kommt durch -- der Zweig hat die
    // Verbindung nicht vergiftet.
    m_amp->zustand = QString::fromLatin1(kBeispiel15K);
    QTRY_VERIFY_WITH_TIMEOUT(zustand.size() > bisher, 2000);
    QCOMPARE(m_verb->lastStatus().id, QStringLiteral("15K"));
}

void TstSpeVerbindung::keinRueckrufAusDemAbbau()
{
    // Beim Abbau werden die Mitglieder in umgekehrter Reihenfolge
    // zerstoert: die drei Zeitgeber ZUERST, der Socket DANACH.
    // ~QTcpSocket loest disconnected() aus, und ohne Destruktor laeuft das
    // noch in onTransportDown(), das dann nach den schon abgebauten
    // Zeitgebern greift. Die ganze Begruendung steht im Destruktor von
    // SpeConnection.
    //
    // Hier wird eine EIGENE Verbindung auf- und abgebaut, nicht die aus
    // init() -- die raeumt cleanup() ab, und dort wuerde die Warnung
    // ausserhalb dieses Falls landen.
    QSignalSpy* auf = nullptr;
    auto* verb = new SpeConnection();
    verb->setPollIntervalMs(10);
    auf = new QSignalSpy(verb, &SpeConnection::connected);
    verb->setAutoReconnect(true);   // nur so ruft onTransportDown() armReconnect()
    verb->connectNetwork(QStringLiteral("127.0.0.1"), m_amp->port());
    QVERIFY2(auf->wait(2000), "Die eigene Verbindung kam nicht zustande");
    delete auf;
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->abfragen() >= 1, 1000);
    QVERIFY(verb->isConnected());

    g_warnungen.clear();
    g_voriger = qInstallMessageHandler(warnungenSammeln);
    delete verb;                    // <- hier passierte es
    qInstallMessageHandler(g_voriger);
    g_voriger = nullptr;

    for (const QString& w : g_warnungen) {
        QVERIFY2(!w.contains(QStringLiteral("Timers cannot be started"))
                     && !w.contains(QStringLiteral("startTimer")),
                 qPrintable(QStringLiteral("Beim Abbau wurde noch in das halb "
                                           "abgebaute Objekt zurueckgerufen: %1")
                                .arg(w)));
    }
}

QTEST_MAIN(TstSpeVerbindung)
#include "tst_spe_verbindung.moc"
