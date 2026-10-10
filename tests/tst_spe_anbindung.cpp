// no-port-check: Longpath-eigener Pruefstand. Die Verweise auf AetherSDR
// in den Kommentaren nennen die Herkunft der GEPRUEFTEN Klassen, hier ist
// nichts portiert.
// =================================================================
// tests/tst_spe_anbindung.cpp  (Longpath)
// =================================================================
//
// SPE Expert: die Kette vom Draht bis ins Fenster.
//
// Schritt 1 (tst_spe_protokoll) prueft die Bytes, Schritt 2
// (tst_spe_verbindung) den Transport. Hier wird beides an das ECHTE
// Hauptfenster gehaengt und gefragt, ob die Zahlen des Verstaerkers
// dort ankommen und ob ein Knopfdruck zurueck auf den Draht geht.
//
// Warum das ein eigener Pruefstand ist: die Verdrahtung liegt in
// MainWindow.cpp, nicht im Applet. Ein Pruefstand, der Applet und
// Treiber selbst zusammensteckt, prueft seine eigene Verdrahtung und
// nicht die der Anwendung -- genau so koennte ein vergessenes connect()
// unbemerkt bleiben. Darum wird hier ein MainWindow gebaut und das
// Applet darin GESUCHT.
//
// Der Verstaerker ist ein eigener QTcpServer auf 127.0.0.1, der die
// Rahmen nachspricht. Kein Geraet, keine Leitung nach draussen.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 -- Neu, Zeus-Punkt 7. Martin Fischer, AI-assisted via
//                 Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include <QByteArray>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

#include "core/RadioDiscovery.h"
#include "core/SpeConnection.h"
#include "core/SpeProtocol.h"
#include "gui/MainWindow.h"
#include "gui/applets/SpeApplet.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

// Ein sendender 1.5K-FA mit Warnung -- jedes Feld traegt etwas, damit
// eine vergessene Leitung auffaellt und nicht in einer Null untergeht.
const char* kSendet15K =
    "C,15K,O,T,A,2,05,2b,0r,H,1350, 1.10, 1.25, 47.5, 32.0, 45, 40, 38,K,N";
// Derselbe Verstaerker im Alarm (SWR ueber der Grenze).
const char* kAlarm15K =
    "C,15K,O,R,A,2,05,2b,0r,M,0000, 0.00, 0.00, 0.0, 0.0, 45, 40, 38,N,S";

QByteArray zustandsRahmen(const QByteArray& p)
{
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

    bool hinstellen() { return m_server.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return m_server.serverPort(); }

    QString    zustand{QString::fromLatin1(kSendet15K)};
    bool       antwortet{true};
    QByteArray empfangen() const { return m_empfangen; }
    void       empfangenLeeren() { m_empfangen.clear(); }
    int        abfragen() const { return m_abfragen; }

private slots:
    void lesen()
    {
        if (!m_peer) { return; }
        const QByteArray stueck = m_peer->readAll();
        m_empfangen.append(stueck);
        for (int i = 0; i + 5 <= stueck.size(); ++i) {
            if (static_cast<quint8>(stueck.at(i))     != 0x55) { continue; }
            if (static_cast<quint8>(stueck.at(i + 1)) != 0x55) { continue; }
            if (static_cast<quint8>(stueck.at(i + 2)) != 0x55) { continue; }
            if (static_cast<quint8>(stueck.at(i + 3)) != 0x01) { continue; }
            const quint8 befehl = static_cast<quint8>(stueck.at(i + 4));
            if (!antwortet) { continue; }
            if (befehl == 0x90) {
                ++m_abfragen;
                m_peer->write(zustandsRahmen(zustand.toLatin1()));
            } else {
                QByteArray q;
                q.append(3, static_cast<char>(0xAA));
                q.append(static_cast<char>(0x01));
                q.append(static_cast<char>(befehl));
                q.append(static_cast<char>(befehl));
                m_peer->write(q);
            }
        }
    }

private:
    QTcpServer  m_server;
    QTcpSocket* m_peer{nullptr};
    QByteArray  m_empfangen;
    int         m_abfragen{0};
};

}  // namespace

class TstSpeAnbindung : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void dasFensterKenntDasFeld();
    void dieZahlenKommenImFeldAn();
    void einAlarmSchlaegtDieWarnung();
    void einKnopfdruckGehtAufDenDraht();
    void dieStilleLeertDasFeld();

private:
    bool verbinden();

    MainWindow*       m_mw{nullptr};
    SpeApplet*        m_applet{nullptr};
    KunstVerstaerker* m_amp{nullptr};
    SpeConnection*    m_verb{nullptr};
};

void TstSpeAnbindung::initTestCase()
{
    m_amp = new KunstVerstaerker(this);
    QVERIFY(m_amp->hinstellen());

    m_mw = new MainWindow();
    m_mw->resize(1200, 800);
    m_mw->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mw, 20000));

    // Das Applet wird GESUCHT, nicht gebaut -- sonst prueft der Pruefstand
    // seine eigene Verdrahtung.
    m_applet = m_mw->findChild<SpeApplet*>();
    QVERIFY2(m_applet, "Kein SpeApplet im Fenster gefunden");

    RadioModel* model = m_mw->findChild<RadioModel*>();
    QVERIFY2(model, "Kein RadioModel im Fenster gefunden");
    m_verb = model->speConnection();
    QVERIFY2(m_verb, "RadioModel hat keine SPE-Verbindung angelegt");

    // Gestauchte Zeiten, damit der Pruefstand nicht drei Sekunden auf die
    // Stilleerkennung wartet (siehe Kopf von SpeConnection.h). Nicht so
    // stark wie in tst_spe_verbindung (dort 10 ms / 5 Takte): hier laeuft
    // ein ganzes MainWindow im Ereignisring mit, und eine halbe Sekunde
    // Stille ist schnell genug fuer `dieStilleLeertDasFeld`, ohne dass
    // ein Flaechenneuaufbau sie ausloest.
    m_verb->setPollIntervalMs(20);
    m_verb->setSilentPollLimit(25);
    m_verb->setReconnectIntervalMs(50);
}

void TstSpeAnbindung::cleanupTestCase()
{
    if (m_verb) { m_verb->disconnect(); }
    delete m_mw;
    m_mw = nullptr;
}

bool TstSpeAnbindung::verbinden()
{
    if (m_verb->isConnected()) { return true; }
    QSignalSpy auf(m_verb, &SpeConnection::connected);
    m_verb->connectNetwork(QStringLiteral("127.0.0.1"), m_amp->port());
    return auf.wait(3000);
}

// ─────────────────────────────────────────────────────────────────────

void TstSpeAnbindung::dasFensterKenntDasFeld()
{
    // Nicht nur "es existiert": es muss ueber den Auswaehler auffindbar
    // sein, sonst laesst es sich weder ein- noch ausblenden und seine
    // Stelle ueberlebt keinen Neustart. (check-applet-ids.py und
    // tst_every_applet_is_reachable pruefen dasselbe von der anderen
    // Seite -- beide fielen, bis der Eintrag in m_appletsById stand.)
    QCOMPARE(m_applet->appletId(), QStringLiteral("Spe"));
    QVERIFY(m_mw->appletIsRegisteredForTest(m_applet));
}

void TstSpeAnbindung::dieZahlenKommenImFeldAn()
{
    m_amp->zustand = QString::fromLatin1(kSendet15K);
    QVERIFY(verbinden());
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->abfragen() >= 2, 3000);

    // Geraetekennung aus dem Zustandsstring -- der SPE sagt sie in JEDER
    // Antwort, darum braucht es hier keine Erkennung.
    QTRY_COMPARE_WITH_TIMEOUT(m_applet->statusPillTextForTesting(),
                              QStringLiteral("OPR · TX"), 2000);

    // ── Auf das Zahlenfeld muss EIGENS gewartet werden ────────────────
    //
    // Die Pille wird sofort gesetzt, die Zahlen nicht: setTemps() &
    // Co. merken sich nur den Wert und setzen eine Marke, geschrieben
    // wird im 10-Hz-Takt von m_labelTimer (Hausbremse, damit ein
    // schnellerer Abfragetakt nicht in Neuzeichnungen umschlaegt). Wer
    // nach der Pille sofort liest, liest in bis zu 100 ms Fenster den
    // LEEREN Anfangszustand.
    //
    // Dieser Pruefstand fiel daran in 3 von 8 Laeufen, mit einer
    // Fehlermeldung, die wie ein fehlendes connect() aussah
    // ("Temperatur fehlt: TEMP —"). Der erste Erklaerungsversuch -- die
    // Stilleerkennung habe mitten in der Messung geleert -- war falsch:
    // eine lockerere Schwelle aenderte nichts.
    QTRY_VERIFY_WITH_TIMEOUT(
        m_applet->infoTextForTesting().contains(QStringLiteral("45°")), 2000);

    const QString info = m_applet->infoTextForTesting();
    QVERIFY2(info.contains(QStringLiteral("47.5V")),
             qPrintable(QStringLiteral("Spannung fehlt: %1").arg(info)));
    QVERIFY2(info.contains(QStringLiteral("32.0A")),
             qPrintable(QStringLiteral("Strom fehlt: %1").arg(info)));
    QVERIFY2(info.contains(QStringLiteral("20m")),
             qPrintable(QStringLiteral("Band fehlt: %1").arg(info)));
    QVERIFY2(info.contains(QStringLiteral("ANT  2")),
             qPrintable(QStringLiteral("Antenne fehlt: %1").arg(info)));
    QVERIFY2(info.contains(QStringLiteral("BYP")),
             qPrintable(QStringLiteral("ATU-Zustand fehlt: %1").arg(info)));
    QVERIFY2(info.contains(QStringLiteral("IN  2")),
             qPrintable(QStringLiteral("Eingang fehlt: %1").arg(info)));
    QVERIFY2(info.contains(QStringLiteral("HIGH")),
             qPrintable(QStringLiteral("Leistungsstufe fehlt: %1").arg(info)));

    // Die Beschriftung des Stufenknopfs IST die Stufe.
    QCOMPARE(m_applet->powerLevelButtonTextForTesting(), QStringLiteral("HIGH"));

    // Die WARNUNG (K = ATU bypassed) ist keine Gefahr -- Bernstein, nicht
    // Rot. Hausstil.
    const QString band = m_applet->faultTextForTesting();
    QVERIFY2(band.contains(QStringLiteral("ATU bypassed")),
             qPrintable(QStringLiteral("Warnung fehlt: %1").arg(band)));
    QVERIFY2(!m_applet->faultIsAlarmForTesting(),
             "Eine Warnung darf nicht als Alarm gefaerbt werden");

    QVERIFY2(m_applet->commandsEnabledForTesting(),
             "Der Verstaerker antwortet -- die Tasten muessen bedienbar sein");
}

void TstSpeAnbindung::einAlarmSchlaegtDieWarnung()
{
    // Alarm und Warnung zugleich zu zeigen hiesse, die ernstere Meldung
    // neben einer harmlosen zu verstecken. Der Alarm gewinnt, und er
    // bekommt den Gefahrenton.
    QVERIFY(verbinden());
    m_amp->zustand = QString::fromLatin1(kAlarm15K);
    QTRY_VERIFY_WITH_TIMEOUT(
        m_applet->faultTextForTesting().contains(QStringLiteral("SWR exceeding")),
        3000);
    QVERIFY2(m_applet->faultIsAlarmForTesting(),
             "Ein Alarm muss den Gefahrenton bekommen");

    // Die Stufe hat gewechselt (H -> M) -- der Knopf zieht nach, und mit
    // ihm die Balkenachse (dass die Achse folgt, prueft
    // tst_spe_protokoll an der Rechnung; hier nur, dass die Stufe
    // ueberhaupt durchkommt).
    QTRY_COMPARE_WITH_TIMEOUT(m_applet->powerLevelButtonTextForTesting(),
                              QStringLiteral("MID"), 2000);
}

void TstSpeAnbindung::einKnopfdruckGehtAufDenDraht()
{
    m_amp->zustand = QString::fromLatin1(kSendet15K);
    QVERIFY(verbinden());
    QTRY_VERIFY_WITH_TIMEOUT(m_applet->commandsEnabledForTesting(), 3000);

    m_amp->empfangenLeeren();
    // Ueber die SIGNALE des Applets, nicht ueber SpeConnection -- geprueft
    // wird die Verdrahtung in MainWindow, nicht der Treiber.
    emit m_applet->operateClicked();
    emit m_applet->tuneClicked();
    emit m_applet->powerLevelClicked();
    emit m_applet->inputClicked();
    emit m_applet->antennaClicked();
    emit m_applet->driveUpClicked();
    emit m_applet->driveDownClicked();
    emit m_applet->offClicked();

    QTRY_VERIFY_WITH_TIMEOUT(
        m_amp->empfangen().contains(QByteArray::fromHex("555555010d0d0d0a")), 3000);
    const QByteArray draht = m_amp->empfangen();
    QVERIFY2(draht.contains(QByteArray::fromHex("5555550109090d0a")), "TUNE");
    QVERIFY2(draht.contains(QByteArray::fromHex("555555010b0b0d0a")), "POWER");
    QVERIFY2(draht.contains(QByteArray::fromHex("5555550101010d0a")), "INPUT");
    QVERIFY2(draht.contains(QByteArray::fromHex("5555550104040d0a")), "ANTENNA");
    // Die Pfeile: 0x10 ist rechts (hoch), 0x0F ist links (runter).
    QVERIFY2(draht.contains(QByteArray::fromHex("5555550110100d0a")),
             "Pfeil hoch -- falscher Tastencode?");
    QVERIFY2(draht.contains(QByteArray::fromHex("555555010f0f0d0a")),
             "Pfeil runter -- falscher Tastencode?");
    QVERIFY2(draht.contains(QByteArray::fromHex("555555010a0a0d0a")), "SWITCH OFF");
}

void TstSpeAnbindung::dieStilleLeertDasFeld()
{
    // Der Fall, um den es beim SPE wirklich geht: der Betreiber schaltet
    // den Verstaerker aus, die TCP-Verbindung ueber ser2net bleibt oben.
    // Ohne das Leeren zeigt das Feld die Zahlen der letzten Antwort
    // weiter -- und sie SEHEN aus wie Messwerte.
    m_amp->zustand = QString::fromLatin1(kSendet15K);
    m_amp->antwortet = true;
    QVERIFY(verbinden());
    QTRY_VERIFY_WITH_TIMEOUT(
        m_applet->infoTextForTesting().contains(QStringLiteral("47.5V")), 3000);

    m_amp->antwortet = false;
    QTRY_VERIFY_WITH_TIMEOUT(!m_applet->commandsEnabledForTesting(), 3000);

    const QString info = m_applet->infoTextForTesting();
    QVERIFY2(!info.contains(QStringLiteral("47.5V")),
             qPrintable(QStringLiteral("Die Spannung steht noch da: %1").arg(info)));
    QVERIFY2(!info.contains(QStringLiteral("45°")),
             qPrintable(QStringLiteral("Die Temperatur steht noch da: %1").arg(info)));
    QVERIFY2(!info.contains(QStringLiteral("20m")),
             qPrintable(QStringLiteral("Das Band steht noch da: %1").arg(info)));
    QVERIFY2(m_applet->faultTextForTesting().isEmpty(),
             "Das Warnband steht noch da");
    QCOMPARE(m_applet->statusPillTextForTesting(), QStringLiteral("—"));
    QCOMPARE(m_applet->powerLevelButtonTextForTesting(), QStringLiteral("PWR"));

    // ON bleibt bedienbar -- das ist sein ganzer Zweck.
    QVERIFY2(m_applet->powerOnEnabledForTesting(),
             "ON muss bedienbar bleiben, solange die Leitung steht");

    // Und beim Einschalten kommt alles von selbst zurueck.
    m_amp->antwortet = true;
    QTRY_VERIFY_WITH_TIMEOUT(
        m_applet->infoTextForTesting().contains(QStringLiteral("47.5V")), 3000);
    QVERIFY(m_applet->commandsEnabledForTesting());
}

QTEST_MAIN(TstSpeAnbindung)
#include "tst_spe_anbindung.moc"
