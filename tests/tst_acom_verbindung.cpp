// no-port-check: Longpath-eigener Pruefstand. AetherSDR hat fuer
// AcomConnection keinen; dies ist keine Portierung.
// =================================================================
// tests/tst_acom_verbindung.cpp  (Longpath)
// =================================================================
//
// ACOM S-Serie: der Transport.
//
// Kein Geraet am Kabel. Stattdessen ein eigener QTcpServer auf
// 127.0.0.1, der sich wie der echte ACOM verhaelt -- und das heisst
// hier etwas anderes als bei SPE und KPA500: er SCHIEBT seine
// Telemetrie von selbst, sobald sie eingeschaltet wurde (0x92), und
// hoert auf zu schieben, wenn sie abgeschaltet wird (0x91).
//
// Drei Dinge sind nur so pruefbar und tragen diesen Pruefstand:
//
//   1. `derWachtaktSchaltetDieTelemetrieWiederEin` -- das Geraet
//      QUITTIERT das Einschalten nicht. Bleibt es still, muss der
//      Wachtakt es wiederholen. Ohne das steht ein Feld fuer immer
//      leer, obwohl die Leitung haelt.
//   2. `dieSystemauskunftWirdFuenfmalVersuchtUndDannAufgegeben` --
//      manche Firmware kennt die Abfrage nicht. Dann darf nicht
//      endlos gefragt und auch nicht blockiert werden.
//   3. `dieSelbstskalierungSpringtNurNachZweiRahmen` -- die
//      Entprellung. Ein einzelner verfaelschter Rahmen, dessen
//      8-Bit-Pruefsumme zufaellig aufgeht, wuerde sonst alle Balken
//      fuer die ganze Sitzung verstellen.
//
// Die Zeiten sind gestaucht (setKeepaliveIntervalMs usw.), sonst
// dauerte Fall 2 allein vier Sekunden.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Neu. Martin Fischer (OE5SOS), KI-gestuetzt mit
//                Claude Code.
// =================================================================

#include <QtTest>

#include <QByteArray>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

#include "core/AcomConnection.h"
#include "core/AcomProtocol.h"

using namespace Longpath;

namespace {

void setLe16(QByteArray& d, int offset, quint16 value)
{
    d[offset]     = static_cast<char>(value & 0xFF);
    d[offset + 1] = static_cast<char>((value >> 8) & 0xFF);
}

// Der Kunstverstaerker. Er SCHIEBT -- das ist der Unterschied zu den
// Kunstverstaerkern von SPE und KPA500, die nur antworten.
class KunstAcom : public QObject
{
    Q_OBJECT
public:
    explicit KunstAcom(QObject* parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            m_peer = m_server.nextPendingConnection();
            connect(m_peer, &QTcpSocket::readyRead, this, &KunstAcom::lesen);
        });
        // Der Schiebetakt. Beim echten Geraet rund 100 ms; hier schneller,
        // damit die Faelle nicht warten muessen.
        m_push.setInterval(10);
        connect(&m_push, &QTimer::timeout, this, &KunstAcom::schieben);
        m_push.start();
    }

    bool hinstellen() { return m_server.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return m_server.serverPort(); }

    // Zustand, den die Faelle verstellen.
    quint16 forwardW{450};
    quint16 reflectedW{8};
    quint16 swrMalHundert{130};
    quint16 tempRaw{305};
    quint8  modeNibble{0x7};        // OperateTx
    quint8  errorCode{0xFF};
    // Antwortet der Verstaerker auf die Anfrage der Systemauskunft?
    // Manche Firmware tut das nicht -- genau der Fall, den Pruefung 2
    // abfaehrt.
    bool antwortetAufSystemauskunft{true};
    quint8 amplifierType{1};        // 1 = A600S, der einzige dokumentierte

    bool    schiebtGerade() const { return m_telemetrieAn; }
    int     einschaltbefehle() const { return m_enableCount; }
    int     auskunftsanfragen() const { return m_cfgRequests; }
    int     quittungen() const { return m_acks; }
    QByteArray empfangen() const { return m_empfangen; }
    void    empfangenLeeren() { m_empfangen.clear(); }
    // Das Einschalten VERGESSEN, ohne dass der Wirt es erfaehrt -- so,
    // als haette der Verstaerker einen Neustart hinter sich oder als
    // waere der Befehl 0x92 unterwegs verloren gegangen. Genau dafuer
    // gibt es den Wachtakt: das Geraet quittiert das Einschalten nicht,
    // also kann der Wirt es nur daran merken, dass nichts mehr kommt.
    void vergissDasEinschalten() { m_telemetrieAn = false; }

    // Fuer die Entprellung: EINEN Rahmen mit abweichender Leistung
    // schieben, dann wieder normal.
    void einmaligSchieben(quint16 watt)
    {
        const quint16 alt = forwardW;
        forwardW = watt;
        schieben();
        forwardW = alt;
    }

private slots:
    void lesen()
    {
        if (!m_peer) { return; }
        const QByteArray stueck = m_peer->readAll();
        m_empfangen.append(stueck);

        Acom::FrameParser p;
        p.setFrameCallback([this](const Acom::Frame& f) {
            switch (f.address) {
                case 0x92: m_telemetrieAn = true;  ++m_enableCount; break;
                case 0x91: m_telemetrieAn = false; break;
                case 0x86: ++m_acks; break;
                case 0x02:
                    ++m_cfgRequests;
                    if (!f.data.isEmpty()
                        && static_cast<quint8>(f.data.at(0)) == 0x11
                        && antwortetAufSystemauskunft) {
                        schickeSystemauskunft();
                    }
                    break;
                case 0x81:
                    // Betriebsart wechseln: Unterbefehl 0x02, Zielwert im
                    // dritten Datenbyte.
                    if (f.data.size() >= 3
                        && static_cast<quint8>(f.data.at(0)) == 0x02) {
                        const quint8 ziel = static_cast<quint8>(f.data.at(2));
                        if (ziel == 0x05) { modeNibble = 0x5; }       // Standby
                        else if (ziel == 0x06) { modeNibble = 0x6; }  // Operate/RX
                        else if (ziel == 0x0A) { modeNibble = 0xA; }  // Aus
                    } else if (f.data.size() >= 1
                               && static_cast<quint8>(f.data.at(0)) == 0x08) {
                        errorCode = 0xFF;   // Fehler geloescht
                    }
                    break;
                default: break;
            }
        });
        p.feed(stueck);
    }

    void schieben()
    {
        if (!m_peer || !m_telemetrieAn) { return; }
        QByteArray nutz(68, '\0');
        nutz[0] = static_cast<char>(modeNibble << 4);
        setLe16(nutz, 9, 0);
        setLe16(nutz, 11, 1000);
        setLe16(nutz, 13, tempRaw);
        setLe16(nutz, 19, forwardW);
        setLe16(nutz, 21, reflectedW);
        setLe16(nutz, 23, swrMalHundert);
        setLe16(nutz, 35, 261);
        setLe16(nutz, 37, 502);
        setLe16(nutz, 41, 9400);
        setLe16(nutz, 45, 14245);
        nutz[63] = static_cast<char>(errorCode);
        nutz[66] = static_cast<char>((2 << 4) | 5);
        m_peer->write(Acom::buildFrame(0x2F, nutz));
        // Und die Fehlerwoerter, wie das echte Geraet, dazwischen.
        m_peer->write(Acom::buildFrame(0x21, QByteArray(20, '\0')));
    }

    void schickeSystemauskunft()
    {
        if (!m_peer) { return; }
        QByteArray nutz(26, '\0');
        nutz[0]  = static_cast<char>(amplifierType);
        nutz[1]  = static_cast<char>(0x02);
        nutz[2]  = static_cast<char>(0x09);
        for (int i = 0; i < 12; ++i) {
            nutz[13 + i] = static_cast<char>(0xA0 + i);
        }
        nutz[25] = static_cast<char>(0x03);
        m_peer->write(Acom::buildFrame(0x11, nutz));
    }

private:
    QTcpServer  m_server;
    QTcpSocket* m_peer{nullptr};
    QTimer      m_push;
    QByteArray  m_empfangen;
    bool        m_telemetrieAn{false};
    int         m_enableCount{0};
    int         m_cfgRequests{0};
    int         m_acks{0};
};

}  // namespace

class TstAcomVerbindung : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void dieTelemetrieWirdEingeschaltetUndKommtAn();
    void derWachtaktSchaltetDieTelemetrieWiederEin();
    void dieSystemauskunftWirdQuittiertUndBestaetigtDasModell();
    void dieSystemauskunftWirdFuenfmalVersuchtUndDannAufgegeben();
    void dieSelbstskalierungSpringtNurNachZweiRahmen();
    void dieSelbstskalierungFaelltNieZurueck();
    void dieBetriebsartGehtHinausUndKommtZurueck();
    void einFehlerLaesstSichLoeschen();
    void keinRueckrufAusDemAbbau();

private:
    bool verbinden();

    KunstAcom*      m_amp{nullptr};
    AcomConnection* m_verb{nullptr};
};

void TstAcomVerbindung::init()
{
    m_amp = new KunstAcom(this);
    QVERIFY(m_amp->hinstellen());
    m_verb = new AcomConnection(this);
    m_verb->setKeepaliveIntervalMs(30);
    m_verb->setSystemConfigRetryMs(25);
    m_verb->setReconnectIntervalMs(50);
}

void TstAcomVerbindung::cleanup()
{
    delete m_verb;
    m_verb = nullptr;
    delete m_amp;
    m_amp = nullptr;
}

bool TstAcomVerbindung::verbinden()
{
    QSignalSpy auf(m_verb, &AcomConnection::connected);
    m_verb->connectNetwork(QStringLiteral("127.0.0.1"), m_amp->port());
    return auf.wait(3000);
}

// ─────────────────────────────────────────────────────────────────────

void TstAcomVerbindung::dieTelemetrieWirdEingeschaltetUndKommtAn()
{
    QSignalSpy tele(m_verb, &AcomConnection::telemetryUpdated);
    QSignalSpy fehlerWoerter(m_verb, &AcomConnection::rawErrorCodesUpdated);
    QVERIFY(verbinden());

    // Beim Verbinden muss 0x92 hinausgehen -- ohne das schiebt der
    // Verstaerker nichts und das Feld bleibt leer.
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->schiebtGerade(), 2000);
    QVERIFY2(tele.wait(2000), "Es kam keine Telemetrie");

    const auto t = m_verb->lastTelemetry();
    QVERIFY(t.mode == Acom::Mode::OperateTx);
    QCOMPARE(t.forwardPowerW, quint16(450));
    QCOMPARE(t.reflectedPowerW, quint16(8));
    QCOMPARE(t.swr_x100, quint16(130));
    QCOMPARE(t.paTempRaw, quint16(305));
    QCOMPARE(t.activeBand, quint8(5));
    QCOMPARE(t.errorCode, quint8(0xFF));

    // Die Fehlerwoerter kommen eigens -- sie stecken nicht in der
    // Telemetrie.
    QVERIFY2(!fehlerWoerter.isEmpty() || fehlerWoerter.wait(2000),
             "Die Fehlerwoerter (0x21) kamen nicht an");
    QCOMPARE(m_verb->sourceLabel(), QStringLiteral("NETWORK"));

    // ── UND DIE GESCHOBENEN RAHMEN WERDEN NICHT QUITTIERT ───────────
    //
    // Die Beschreibung verlangt eine Quittung (0x86) je Nachricht --
    // gelesen wie fuer einen Frage/Antwort-Wechsel. Die geschobenen
    // Rahmen sind keiner: sie kommen rund zehnmal je Sekunde, ohne dass
    // jemand fragt. Jeden davon zu quittieren hiesse, die Leitung mit
    // Quittungen zu fuellen; AetherSDRs Vergleichsprogramm quittiert
    // ueberhaupt nichts und laeuft so an echter Hardware. Quittiert wird
    // darum nur die einmalig ANGEFRAGTE Systemauskunft.
    //
    // Hier laufen inzwischen dutzende geschobene Rahmen durch. Waeren
    // sie quittiert, stuende der Zaehler entsprechend hoch.
    // Erst genug Rahmen auflaufen lassen -- mit vier traegt die Aussage
    // nicht (gefunden, als die Gegenprobe mit 3 gegen 4 Rahmen
    // dasselbe Bild gab).
    QTRY_VERIFY_WITH_TIMEOUT(tele.size() >= 20, 3000);
    QVERIFY2(m_amp->quittungen() <= 2,
             qPrintable(QStringLiteral("%1 Quittungen bei %2 geschobenen "
                                       "Rahmen -- die geschobenen duerfen "
                                       "nicht quittiert werden")
                            .arg(m_amp->quittungen()).arg(tele.size())));
}

void TstAcomVerbindung::derWachtaktSchaltetDieTelemetrieWiederEin()
{
    // DER FALL, DEN ES BEIM SPE UND KPA500 NICHT GIBT.
    //
    // Der ACOM quittiert das Einschalten der Telemetrie (0x92) NICHT.
    // Hoert er auf zu schieben -- Neustart, verlorener Befehl --, erfaehrt
    // der Wirt das nur daran, dass nichts mehr kommt. Ohne Wachtakt
    // stuende das Feld danach fuer immer leer, obwohl die Leitung haelt
    // und keine Fehlermeldung auftritt.
    QVERIFY(verbinden());
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->schiebtGerade(), 2000);

    QSignalSpy tele(m_verb, &AcomConnection::telemetryUpdated);
    QVERIFY2(tele.wait(2000), "Es kam zunaechst keine Telemetrie");
    const int einschaltenVorher = m_amp->einschaltbefehle();

    // Der Verstaerker vergisst es -- ohne ein Wort darueber.
    m_amp->vergissDasEinschalten();
    QVERIFY(!m_amp->schiebtGerade());

    // Jetzt muss der Wachtakt das Einschalten wiederholen und das
    // Schieben wieder in Gang bringen.
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->einschaltbefehle() > einschaltenVorher, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->schiebtGerade(), 3000);

    QSignalSpy wieder(m_verb, &AcomConnection::telemetryUpdated);
    QVERIFY2(wieder.wait(2000),
             "Nach dem Wiedereinschalten kam keine Telemetrie -- der Wachtakt "
             "hat die Leitung nicht wiederbelebt");

    // Und solange Rahmen kommen, wird NICHT dauernd nachgeschaltet --
    // der Wachtakt soll die Leitung nicht mit 0x92 zupflastern.
    const int ruhigVorher = m_amp->einschaltbefehle();
    QTest::qWait(300);
    QCOMPARE(m_amp->einschaltbefehle(), ruhigVorher);
}

void TstAcomVerbindung::dieSystemauskunftWirdQuittiertUndBestaetigtDasModell()
{
    QSignalSpy modell(m_verb, &AcomConnection::modelChanged);
    QSignalSpy auskunft(m_verb, &AcomConnection::systemConfigReceived);
    QVERIFY(verbinden());

    // Beim Verbinden steht die Stufe erst auf dem Vorgabewert.
    QVERIFY(!modell.isEmpty());
    QCOMPARE(modell.at(0).at(0).toString(), QStringLiteral("600S"));
    QCOMPARE(modell.at(0).at(1).toString(), QStringLiteral("default"));

    // Dann kommt die Auskunft, und sie wird QUITTIERT (0x86) -- anders
    // als die geschobenen Rahmen, die niemand quittiert.
    QVERIFY2(auskunft.wait(2000), "Die Systemauskunft kam nicht");
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->quittungen() >= 1, 2000);

    QTRY_VERIFY_WITH_TIMEOUT(modell.size() >= 2, 2000);
    QCOMPARE(modell.last().at(0).toString(), QStringLiteral("600S"));
    QVERIFY2(modell.last().at(1).toString() == QStringLiteral("confirmed"),
             "Nach der Auskunft muss der Grund von 'default' auf 'confirmed' "
             "wechseln -- derselbe Name, aber eine andere Aussage");

    // Und sie wird NICHT weiter angefragt, sobald sie da ist.
    const int anfragen = m_amp->auskunftsanfragen();
    QTest::qWait(200);
    QCOMPARE(m_amp->auskunftsanfragen(), anfragen);
}

void TstAcomVerbindung::dieSystemauskunftWirdFuenfmalVersuchtUndDannAufgegeben()
{
    // Manche Firmware kennt die Abfrage nicht. Dann darf nicht endlos
    // gefragt werden -- und erst recht darf nichts darauf warten: die
    // Selbstskalierung deckt den Fall ohnehin ab.
    m_amp->antwortetAufSystemauskunft = false;
    QVERIFY(verbinden());

    QTRY_VERIFY_WITH_TIMEOUT(m_verb->systemConfigAttemptsForTesting() >= 5, 3000);
    QCOMPARE(m_verb->systemConfigAttemptsForTesting(), 5);

    // Und dann ist Ruhe -- kein sechster Versuch.
    QTest::qWait(300);
    QCOMPARE(m_verb->systemConfigAttemptsForTesting(), 5);
    QVERIFY2(m_amp->auskunftsanfragen() <= 5,
             qPrintable(QStringLiteral("%1 Anfragen hinausgegangen, hoechstens "
                                       "5 erlaubt").arg(m_amp->auskunftsanfragen())));

    // Die Telemetrie laeuft trotzdem -- das Aufgeben blockiert nichts.
    QSignalSpy tele(m_verb, &AcomConnection::telemetryUpdated);
    QVERIFY2(tele.wait(2000), "Nach dem Aufgeben kam keine Telemetrie mehr");
}

void TstAcomVerbindung::dieSelbstskalierungSpringtNurNachZweiRahmen()
{
    // DIE ENTPRELLUNG. Die Stufe geht nur nach oben und faellt nie
    // zurueck -- ein einzelner verfaelschter Rahmen, dessen
    // 8-Bit-Pruefsumme zufaellig aufgeht (etwa jeder 256.), wuerde sonst
    // alle Balken fuer die ganze Sitzung verstellen.
    m_amp->antwortetAufSystemauskunft = false;   // Auskunft aus dem Weg
    m_amp->forwardW = 450;                       // sauber in der 600S-Stufe
    QSignalSpy modell(m_verb, &AcomConnection::modelChanged);
    QVERIFY(verbinden());
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->schiebtGerade(), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->currentModel(), QStringLiteral("600S"), 2000);
    const int vorher = modell.size();

    // EIN einzelner Ausreisser: 1100 W wuerde 1200S-Klasse verlangen.
    m_amp->einmaligSchieben(1100);
    QTest::qWait(200);
    QVERIFY2(m_verb->currentModel() == QStringLiteral("600S"),
             qPrintable(QStringLiteral("Ein EINZELNER Rahmen hat die Stufe auf "
                                       "%1 gezogen -- die Entprellung greift "
                                       "nicht").arg(m_verb->currentModel())));
    QCOMPARE(modell.size(), vorher);

    // Dauerhaft 1100 W: jetzt soll sie springen, und zwar DIREKT auf
    // 1200S, nicht schrittweise ueber 700S.
    m_amp->forwardW = 1100;
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->currentModel(), QStringLiteral("1200S"), 3000);
    QVERIFY(modell.size() > vorher);
    QCOMPARE(modell.last().at(0).toString(), QStringLiteral("1200S"));
    QCOMPARE(modell.last().at(1).toString(), QStringLiteral("auto-scaled"));
}

void TstAcomVerbindung::dieSelbstskalierungFaelltNieZurueck()
{
    m_amp->antwortetAufSystemauskunft = false;
    m_amp->forwardW = 1100;
    QVERIFY(verbinden());
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->currentModel(), QStringLiteral("1200S"), 3000);

    // Zurueck auf kleine Leistung -- ein Einbruch ist Betrieb, kein
    // kleinerer Verstaerker.
    m_amp->forwardW = 100;
    QTest::qWait(300);
    QCOMPARE(m_verb->currentModel(), QStringLiteral("1200S"));

    // Erst beim Wiederverbinden steht sie wieder auf dem Vorgabewert.
    m_verb->disconnect();
    m_amp->forwardW = 450;
    QVERIFY(verbinden());
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->currentModel(), QStringLiteral("600S"), 2000);
}

void TstAcomVerbindung::dieBetriebsartGehtHinausUndKommtZurueck()
{
    QVERIFY(verbinden());
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->schiebtGerade(), 2000);
    m_amp->empfangenLeeren();

    m_verb->setOperate(false);
    QTRY_VERIFY_WITH_TIMEOUT(
        m_amp->empfangen().contains(Acom::buildModeCommand(Acom::ModeCommand::Standby)),
        2000);
    // Die Anzeige folgt der geschobenen Telemetrie, nicht dem Knopfdruck.
    QTRY_VERIFY_WITH_TIMEOUT(m_verb->lastTelemetry().mode == Acom::Mode::Standby, 2000);

    m_amp->empfangenLeeren();
    m_verb->setOperate(true);
    QTRY_VERIFY_WITH_TIMEOUT(m_verb->lastTelemetry().mode == Acom::Mode::OperateRx, 2000);

    m_amp->empfangenLeeren();
    m_verb->powerOff();
    QTRY_VERIFY_WITH_TIMEOUT(
        m_amp->empfangen().contains(Acom::buildModeCommand(Acom::ModeCommand::PowerOff)),
        2000);
}

void TstAcomVerbindung::einFehlerLaesstSichLoeschen()
{
    QVERIFY(verbinden());
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->schiebtGerade(), 2000);

    m_amp->errorCode = 0x39;   // „Excessive PAM current"
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->lastTelemetry().errorCode, quint8(0x39), 2000);
    QCOMPARE(Acom::errorCodeName(m_verb->lastTelemetry().errorCode),
             QStringLiteral("Excessive PAM current"));

    m_verb->clearFaults();
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->lastTelemetry().errorCode, quint8(0xFF), 2000);
}

void TstAcomVerbindung::keinRueckrufAusDemAbbau()
{
    // Dieselbe Falle wie bei SpeConnection und Kpa500Connection, und beim
    // Vorbild fehlt der Destruktor: m_socket ist VOR den drei Zeitgebern
    // erklaert, ~QTcpSocket loest disconnected() aus, und
    // onTransportDown() greift dann nach zwei schon zerstoerten
    // Zeitgebern.
    static QStringList warnungen;
    static QtMessageHandler voriger = nullptr;
    warnungen.clear();

    auto* verb = new AcomConnection();
    verb->setKeepaliveIntervalMs(30);
    verb->setSystemConfigRetryMs(25);
    verb->setAutoReconnect(true);
    QSignalSpy auf(verb, &AcomConnection::connected);
    verb->connectNetwork(QStringLiteral("127.0.0.1"), m_amp->port());
    QVERIFY(auf.wait(3000));
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->schiebtGerade(), 2000);

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

QTEST_MAIN(TstAcomVerbindung)
#include "tst_acom_verbindung.moc"
