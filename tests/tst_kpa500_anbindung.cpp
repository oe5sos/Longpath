// no-port-check: Longpath-eigener Pruefstand, kein Port.
// =================================================================
// tests/tst_kpa500_anbindung.cpp  (Longpath)
// =================================================================
//
// Elecraft KPA500: die Kette vom Draht bis ins Fenster.
//
// Wie bei tst_spe_anbindung wird ein ECHTES MainWindow gebaut und das
// Applet darin GESUCHT -- ein Pruefstand, der Treiber und Feld selbst
// zusammensteckt, prueft seine eigene Verdrahtung.
//
// Der Fall, auf den es hier ankommt und den es beim SPE nicht gibt:
// `derBootZustandMachtDenEinschaltknopfSinnvoll`. Der KPA500 kann
// melden „aus, aber am Draht erreichbar", und genau dann -- und NUR
// dann -- soll der ON-Knopf bedienbar sein. Laeuft er, ist er schon an;
// antwortet er auf nichts, erreicht ihn auch das 'P' nicht. Diese
// Dreiteilung ist die ganze Begruendung dafuer, dass Kpa500Applet
// nicht SpeApplet mit anderen Namen ist.
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
#include "gui/MainWindow.h"
#include "gui/applets/Kpa500Applet.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

// Derselbe Kunstverstaerker wie in tst_kpa500_verbindung, auf das
// Noetige gekuerzt.
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

    bool laeuft{true};
    int  watt{250};
    int  swrMalZehn{15};
    int  voltMalZehn{475};
    int  ampMalZehn{320};
    int  tempC{42};
    bool operate{true};
    int  band{5};
    int  fehler{0};

    int abfragen() const { return m_abfragen; }
    QByteArray empfangen() const { return m_empfangen; }
    void empfangenLeeren() { m_empfangen.clear(); }

private slots:
    void lesen()
    {
        if (!m_peer) { return; }
        const QByteArray stueck = m_peer->readAll();
        m_empfangen.append(stueck);

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
        for (char c : uebrig) {
            if (c == 'I' && !laeuft) { m_peer->write(QByteArrayLiteral("KPA500")); }
            else if (c == 'P') { laeuft = true; }
        }
        if (!laeuft) { return; }
        while (true) {
            const int anf = gerahmt.indexOf('^');
            if (anf < 0) { break; }
            const int ende = gerahmt.indexOf(';', anf);
            if (ende < 0) { break; }
            antworten(QString::fromLatin1(gerahmt.mid(anf + 1, ende - anf - 1)));
            gerahmt.remove(0, ende + 1);
        }
    }

private:
    void antworten(const QString& befehl)
    {
        ++m_abfragen;
        static const QStringList drei = {QStringLiteral("RVM"), QStringLiteral("BRP"),
                                         QStringLiteral("BRX"), QStringLiteral("DMO")};
        QString verb = befehl.left(3);
        if (!drei.contains(verb)) { verb = befehl.left(2); }
        const QString daten = befehl.mid(verb.size());
        auto schick = [this](const QString& s) { m_peer->write(s.toLatin1()); };

        if (verb == QStringLiteral("WS")) {
            schick(QStringLiteral("^WS%1 %2;").arg(watt, 3, 10, QLatin1Char('0'))
                                              .arg(swrMalZehn, 3, 10, QLatin1Char('0')));
        } else if (verb == QStringLiteral("VI")) {
            schick(QStringLiteral("^VI%1 %2;").arg(voltMalZehn, 3, 10, QLatin1Char('0'))
                                              .arg(ampMalZehn, 3, 10, QLatin1Char('0')));
        } else if (verb == QStringLiteral("TM")) {
            schick(QStringLiteral("^TM%1;").arg(tempC, 3, 10, QLatin1Char('0')));
        } else if (verb == QStringLiteral("OS")) {
            if (!daten.isEmpty()) { operate = (daten == QStringLiteral("1")); }
            schick(QStringLiteral("^OS%1;").arg(operate ? 1 : 0));
        } else if (verb == QStringLiteral("BN")) {
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
};

}  // namespace

class TstKpa500Anbindung : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void dasFensterKenntDasFeld();
    void dieZahlenKommenImFeldAn();
    void derBootZustandMachtDenEinschaltknopfSinnvoll();
    void einFehlerKommtAlsZahlUndLaesstSichLoeschen();
    void derUmschalterSchicktDasGegenteil();

private:
    bool verbinden();

    MainWindow*       m_mw{nullptr};
    Kpa500Applet*     m_applet{nullptr};
    KunstKpa*         m_amp{nullptr};
    Kpa500Connection* m_verb{nullptr};
};

void TstKpa500Anbindung::initTestCase()
{
    m_amp = new KunstKpa(this);
    QVERIFY(m_amp->hinstellen());

    m_mw = new MainWindow();
    m_mw->resize(1200, 800);
    m_mw->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mw, 20000));

    m_applet = m_mw->findChild<Kpa500Applet*>();
    QVERIFY2(m_applet, "Kein Kpa500Applet im Fenster gefunden");

    RadioModel* model = m_mw->findChild<RadioModel*>();
    QVERIFY(model);
    m_verb = model->kpa500Connection();
    QVERIFY2(m_verb, "RadioModel hat keine KPA500-Verbindung angelegt");

    m_verb->setPollIntervalMs(20);
    m_verb->setSilentPollLimit(10);
    m_verb->setReconnectIntervalMs(50);
}

void TstKpa500Anbindung::cleanupTestCase()
{
    if (m_verb) { m_verb->disconnect(); }
    delete m_mw;
    m_mw = nullptr;
}

bool TstKpa500Anbindung::verbinden()
{
    if (m_verb->isConnected()) { return true; }
    QSignalSpy auf(m_verb, &Kpa500Connection::connected);
    m_verb->connectNetwork(QStringLiteral("127.0.0.1"), m_amp->port());
    return auf.wait(3000);
}

// ─────────────────────────────────────────────────────────────────────

void TstKpa500Anbindung::dasFensterKenntDasFeld()
{
    QCOMPARE(m_applet->appletId(), QStringLiteral("Kpa500"));
    QVERIFY2(m_mw->appletIsRegisteredForTest(m_applet),
             "Ohne Eintrag im Auswaehler laesst es sich weder ein- noch "
             "ausblenden");
}

void TstKpa500Anbindung::dieZahlenKommenImFeldAn()
{
    m_amp->laeuft = true;
    QVERIFY(verbinden());
    QTRY_COMPARE_WITH_TIMEOUT(m_applet->statusPillTextForTesting(),
                              QStringLiteral("OPERATE"), 3000);

    QTRY_VERIFY_WITH_TIMEOUT(
        m_applet->infoTextForTesting().contains(QStringLiteral("47.5V")), 3000);
    const QString info = m_applet->infoTextForTesting();
    QVERIFY2(info.contains(QStringLiteral("32.0A")),
             qPrintable(QStringLiteral("Strom fehlt: %1").arg(info)));
    QVERIFY2(info.contains(QStringLiteral("20m")),
             qPrintable(QStringLiteral("Band fehlt: %1").arg(info)));
    QVERIFY2(m_applet->commandsEnabledForTesting(),
             "Der Verstaerker antwortet -- die Tasten muessen bedienbar sein");
    QVERIFY2(!m_applet->powerOnEnabledForTesting(),
             "Er LAEUFT -- der Einschaltknopf hat keinen Sinn und muss grau "
             "sein");
}

void TstKpa500Anbindung::derBootZustandMachtDenEinschaltknopfSinnvoll()
{
    // DER FALL, DEN DER SPE NICHT HAT.
    QVERIFY(verbinden());
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->liveness(),
                              Kpa500Connection::Liveness::Running, 3000);

    // Ausschalten -- die Leitung bleibt, der Boot-Lader antwortet.
    m_verb->powerOff();
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->liveness(),
                              Kpa500Connection::Liveness::BootMode, 5000);

    QTRY_VERIFY_WITH_TIMEOUT(m_applet->powerOnEnabledForTesting(), 2000);
    QVERIFY2(!m_applet->commandsEnabledForTesting(),
             "Im Boot-Zustand nimmt er keine regulaeren Befehle -- die Tasten "
             "muessen grau sein");
    QVERIFY2(m_applet->statusPillTextForTesting().contains(QStringLiteral("AUS")),
             qPrintable(QStringLiteral("Die Pille sagt nicht, dass er aus ist: "
                                       "%1").arg(m_applet->statusPillTextForTesting())));
    // Und die Messwerte sind geleert, nicht eingefroren.
    QVERIFY2(!m_applet->infoTextForTesting().contains(QStringLiteral("47.5V")),
             qPrintable(QStringLiteral("Die Spannung steht noch da: %1")
                            .arg(m_applet->infoTextForTesting())));

    // Einschalten mit dem Knopf -- ueber das SIGNAL des Applets, damit die
    // Verdrahtung in MainWindow geprueft wird und nicht der Treiber.
    m_amp->empfangenLeeren();
    emit m_applet->powerOnClicked();
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->empfangen().contains('P'), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->liveness(),
                              Kpa500Connection::Liveness::Running, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(m_applet->commandsEnabledForTesting(), 2000);
    QVERIFY2(!m_applet->powerOnEnabledForTesting(),
             "Er laeuft wieder -- der Einschaltknopf gehoert wieder grau");
}

void TstKpa500Anbindung::einFehlerKommtAlsZahlUndLaesstSichLoeschen()
{
    m_amp->laeuft = true;
    QVERIFY(verbinden());
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->liveness(),
                              Kpa500Connection::Liveness::Running, 3000);
    QVERIFY(m_applet->faultTextForTesting().isEmpty());
    QVERIFY(!m_applet->faultClearVisibleForTesting());

    m_amp->fehler = 3;
    QTRY_VERIFY_WITH_TIMEOUT(!m_applet->faultTextForTesting().isEmpty(), 3000);
    const QString band = m_applet->faultTextForTesting();
    QVERIFY2(band.contains(QStringLiteral("03")),
             qPrintable(QStringLiteral("Die Nummer fehlt: %1").arg(band)));
    // KEIN erfundener Name. Das Dokument gibt die Zuordnung nicht her.
    QVERIFY2(band.contains(QStringLiteral("nicht in Elecrafts")),
             qPrintable(QStringLiteral("Der Hinweis, dass die Bedeutung nicht "
                                       "dokumentiert ist, fehlt: %1").arg(band)));
    QVERIFY2(m_applet->faultClearVisibleForTesting(),
             "Der Loeschknopf muss erscheinen, wenn ein Fehler anliegt");

    emit m_applet->clearFaultClicked();
    QTRY_VERIFY_WITH_TIMEOUT(m_applet->faultTextForTesting().isEmpty(), 3000);
    QVERIFY2(!m_applet->faultClearVisibleForTesting(),
             "Ohne Fehler hat der Loeschknopf keinen Anlass");
}

void TstKpa500Anbindung::derUmschalterSchicktDasGegenteil()
{
    m_amp->laeuft = true;
    QVERIFY(verbinden());
    QTRY_COMPARE_WITH_TIMEOUT(m_applet->statusPillTextForTesting(),
                              QStringLiteral("OPERATE"), 3000);

    m_amp->empfangenLeeren();
    emit m_applet->operateClicked();
    // Er steht auf OPERATE, also muss ^OS0; hinausgehen.
    QTRY_VERIFY_WITH_TIMEOUT(
        m_amp->empfangen().contains(QByteArrayLiteral("^OS0;")), 3000);
    // Und die Anzeige folgt der ANTWORT, nicht dem Knopfdruck.
    QTRY_COMPARE_WITH_TIMEOUT(m_applet->statusPillTextForTesting(),
                              QStringLiteral("STANDBY"), 3000);

    m_amp->empfangenLeeren();
    emit m_applet->operateClicked();
    QTRY_VERIFY_WITH_TIMEOUT(
        m_amp->empfangen().contains(QByteArrayLiteral("^OS1;")), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(m_applet->statusPillTextForTesting(),
                              QStringLiteral("OPERATE"), 3000);
}

QTEST_MAIN(TstKpa500Anbindung)
#include "tst_kpa500_anbindung.moc"
