// no-port-check: Longpath-eigener Pruefstand, kein Port.
// =================================================================
// tests/tst_acom_anbindung.cpp  (Longpath)
// =================================================================
//
// ACOM: die Kette vom Draht bis ins Fenster.
//
// Wie bei SPE und KPA500 wird ein ECHTES MainWindow gebaut und das
// Applet darin GESUCHT -- ein Pruefstand, der Treiber und Feld selbst
// zusammensteckt, prueft seine eigene Verdrahtung.
//
// Zwei Faelle tragen diesen Pruefstand, und keiner von ihnen hat beim
// SPE oder KPA500 ein Gegenstueck:
//
//   `dieSelbstskalierungZiehtDieBalkenMit` -- die Modellstufe kann sich
//   MITTEN in der Sitzung anheben, und dann muessen die Achsen von
//   Vorlauf UND Ruecklauf zusammen nachgezogen werden. Beim SPE haengt
//   die Achse an der gewaehlten Leistungsstufe, beim KPA500 steht sie
//   fest; nur hier bewegt sie sich von selbst.
//
//   `derModellversatzWirdAbgezogen` -- die Temperatur kommt in ROHEN
//   Sensoreinheiten. Der Abzug haengt an der erkannten Stufe, steht
//   also nicht in der Protokollschicht (die kennt kein Modell), sondern
//   in der Verdrahtung. Ohne ihn stehen dreihundert Grad im Feld.
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
#include <QTimer>

#include "core/AcomConnection.h"
#include "core/AcomProtocol.h"
#include "gui/MainWindow.h"
#include "gui/applets/AcomApplet.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

void setLe16(QByteArray& d, int offset, quint16 value)
{
    d[offset]     = static_cast<char>(value & 0xFF);
    d[offset + 1] = static_cast<char>((value >> 8) & 0xFF);
}

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
        m_push.setInterval(10);
        connect(&m_push, &QTimer::timeout, this, &KunstAcom::schieben);
        m_push.start();
    }
    bool hinstellen() { return m_server.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return m_server.serverPort(); }

    quint16 forwardW{450};
    quint16 reflectedW{80};
    quint16 swrMalHundert{130};
    quint16 tempRaw{305};        // roh! 305 - 273 (600S-Versatz) = 32 °C
    quint8  modeNibble{0x6};     // OperateRx
    quint8  errorCode{0xFF};
    quint32 betriebsSek{459797}; // 5 d 7 h -- die am echten 600S belegte Zahl
    bool    antwortetAufSystemauskunft{true};

    bool schiebtGerade() const { return m_an; }
    QByteArray empfangen() const { return m_empfangen; }
    void empfangenLeeren() { m_empfangen.clear(); }

private slots:
    void lesen()
    {
        if (!m_peer) { return; }
        const QByteArray stueck = m_peer->readAll();
        m_empfangen.append(stueck);
        Acom::FrameParser p;
        p.setFrameCallback([this](const Acom::Frame& f) {
            switch (f.address) {
                case 0x92: m_an = true; break;
                case 0x91: m_an = false; break;
                case 0x02:
                    if (!f.data.isEmpty()
                        && static_cast<quint8>(f.data.at(0)) == 0x11
                        && antwortetAufSystemauskunft) {
                        schickeAuskunft();
                    }
                    break;
                case 0x81:
                    if (f.data.size() >= 3
                        && static_cast<quint8>(f.data.at(0)) == 0x02) {
                        const quint8 ziel = static_cast<quint8>(f.data.at(2));
                        if (ziel == 0x05) { modeNibble = 0x5; }
                        else if (ziel == 0x06) { modeNibble = 0x6; }
                        else if (ziel == 0x0A) { modeNibble = 0xA; }
                    } else if (!f.data.isEmpty()
                               && static_cast<quint8>(f.data.at(0)) == 0x08) {
                        errorCode = 0xFF;
                    }
                    break;
                default: break;
            }
        });
        p.feed(stueck);
    }

    void schieben()
    {
        if (!m_peer || !m_an) { return; }
        QByteArray n(68, '\0');
        n[0] = static_cast<char>(modeNibble << 4);
        setLe16(n, 9,  static_cast<quint16>((betriebsSek >> 16) & 0xFFFF));
        setLe16(n, 11, static_cast<quint16>(betriebsSek & 0xFFFF));
        setLe16(n, 13, tempRaw);
        setLe16(n, 19, forwardW);
        setLe16(n, 21, reflectedW);
        setLe16(n, 23, swrMalHundert);
        setLe16(n, 37, 502);      // HV1 = 50,2 V
        setLe16(n, 41, 9400);     // Id = 9,4 A
        n[63] = static_cast<char>(errorCode);
        n[66] = static_cast<char>((2 << 4) | 5);   // Band 5 = 20 m
        m_peer->write(Acom::buildFrame(0x2F, n));
    }

    void schickeAuskunft()
    {
        if (!m_peer) { return; }
        QByteArray n(26, '\0');
        n[0] = static_cast<char>(1);   // Typ 1 = A600S
        n[1] = static_cast<char>(2);
        n[2] = static_cast<char>(9);
        m_peer->write(Acom::buildFrame(0x11, n));
    }

private:
    QTcpServer  m_server;
    QTcpSocket* m_peer{nullptr};
    QTimer      m_push;
    QByteArray  m_empfangen;
    bool        m_an{false};
};

}  // namespace

class TstAcomAnbindung : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void dasFensterKenntDasFeld();
    void dieZahlenKommenImFeldAn();
    void derModellversatzWirdAbgezogen();
    void dieSelbstskalierungZiehtDieBalkenMit();
    void dieDreiZustandsknoepfeGehenHinaus();
    void einFehlerSchaltetDenLoeschknopfFrei();

private:
    bool verbinden();

    MainWindow*     m_mw{nullptr};
    AcomApplet*     m_applet{nullptr};
    KunstAcom*      m_amp{nullptr};
    AcomConnection* m_verb{nullptr};
};

void TstAcomAnbindung::initTestCase()
{
    m_amp = new KunstAcom(this);
    QVERIFY(m_amp->hinstellen());

    m_mw = new MainWindow();
    m_mw->resize(1200, 800);
    m_mw->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_mw, 20000));

    m_applet = m_mw->findChild<AcomApplet*>();
    QVERIFY2(m_applet, "Kein AcomApplet im Fenster gefunden");

    RadioModel* model = m_mw->findChild<RadioModel*>();
    QVERIFY(model);
    m_verb = model->acomConnection();
    QVERIFY2(m_verb, "RadioModel hat keine ACOM-Verbindung angelegt");

    m_verb->setKeepaliveIntervalMs(30);
    m_verb->setSystemConfigRetryMs(25);
    m_verb->setReconnectIntervalMs(50);
}

void TstAcomAnbindung::cleanupTestCase()
{
    if (m_verb) { m_verb->disconnect(); }
    delete m_mw;
    m_mw = nullptr;
}

bool TstAcomAnbindung::verbinden()
{
    if (m_verb->isConnected()) { return true; }
    QSignalSpy auf(m_verb, &AcomConnection::connected);
    m_verb->connectNetwork(QStringLiteral("127.0.0.1"), m_amp->port());
    return auf.wait(3000);
}

// ─────────────────────────────────────────────────────────────────────

void TstAcomAnbindung::dasFensterKenntDasFeld()
{
    QCOMPARE(m_applet->appletId(), QStringLiteral("Acom"));
    QVERIFY2(m_mw->appletIsRegisteredForTest(m_applet),
             "Ohne Eintrag im Auswaehler laesst es sich weder ein- noch "
             "ausblenden");
}

void TstAcomAnbindung::dieZahlenKommenImFeldAn()
{
    m_amp->forwardW = 450;
    m_amp->modeNibble = 0x6;
    QVERIFY(verbinden());
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->schiebtGerade(), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(m_applet->statusPillTextForTesting(),
                              QStringLiteral("OPR/RX"), 3000);

    QTRY_VERIFY_WITH_TIMEOUT(
        m_applet->infoTextForTesting().contains(QStringLiteral("50.2V")), 3000);
    const QString info = m_applet->infoTextForTesting();
    QVERIFY2(info.contains(QStringLiteral("9.4A")),
             qPrintable(QStringLiteral("Drainstrom fehlt: %1").arg(info)));
    QVERIFY2(info.contains(QStringLiteral("20m")),
             qPrintable(QStringLiteral("Band fehlt: %1").arg(info)));
    // Die GESAMTbetriebszeit, nicht die Dauer seit dem Einschalten:
    // 459.797 s sind 5 Tage 7 Stunden -- die am echten 600S belegte Zahl.
    QVERIFY2(info.contains(QStringLiteral("5 d 7 h")),
             qPrintable(QStringLiteral("Betriebszeit falsch gerechnet: %1").arg(info)));
    QVERIFY(m_applet->commandsEnabledForTesting());
}

void TstAcomAnbindung::derModellversatzWirdAbgezogen()
{
    // Die Temperatur kommt in ROHEN Sensoreinheiten. Der Abzug haengt an
    // der erkannten Stufe (600S: 273) und steht darum in der
    // Verdrahtung, nicht in der Protokollschicht. Ohne ihn stuenden
    // 305 Grad im Feld.
    m_amp->tempRaw = 305;
    QVERIFY(verbinden());
    QTRY_VERIFY_WITH_TIMEOUT(
        m_applet->infoTextForTesting().contains(QStringLiteral("TEMP  32°C")), 3000);
    QVERIFY2(!m_applet->infoTextForTesting().contains(QStringLiteral("305")),
             qPrintable(QStringLiteral("Der Rohwert steht im Feld: %1")
                            .arg(m_applet->infoTextForTesting())));

    // Und er folgt dem Rohwert: 283 - 273 = 10.
    m_amp->tempRaw = 283;
    QTRY_VERIFY_WITH_TIMEOUT(
        m_applet->infoTextForTesting().contains(QStringLiteral("TEMP  10°C")), 3000);
    m_amp->tempRaw = 305;
}

void TstAcomAnbindung::dieSelbstskalierungZiehtDieBalkenMit()
{
    // DER FALL, DEN ES BEI SPE UND KPA500 NICHT GIBT: die Stufe kann
    // sich MITTEN in der Sitzung anheben, und dann muessen die Achsen
    // von Vorlauf UND Ruecklauf zusammen nachgezogen werden.
    m_amp->antwortetAufSystemauskunft = false;   // Auskunft aus dem Weg
    m_amp->forwardW = 450;
    QVERIFY(verbinden());
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->currentModel(), QStringLiteral("600S"), 3000);

    // Die Sprechblase sagt, WARUM die Skala so ist -- und zu Beginn ist
    // der Grund „geraten", nicht „bestaetigt".
    QTRY_VERIFY_WITH_TIMEOUT(
        !m_applet->diagnosticTooltipForTesting().isEmpty(), 3000);
    QVERIFY2(m_applet->diagnosticTooltipForTesting().contains(QStringLiteral("600S")),
             qPrintable(m_applet->diagnosticTooltipForTesting()));
    QVERIFY2(m_applet->diagnosticTooltipForTesting().contains(QStringLiteral("700")),
             qPrintable(QStringLiteral("Das Skalenende der 600S-Stufe (700 W) "
                                       "fehlt in der Sprechblase: %1")
                            .arg(m_applet->diagnosticTooltipForTesting())));

    // Die Achsen stehen jetzt auf der 600S-Stufe -- BEIDE, Vorlauf und
    // Ruecklauf.
    const auto& s600 = Acom::modelSpec(QStringLiteral("600S"));
    QTRY_COMPARE_WITH_TIMEOUT(m_applet->powerMaxForTesting(), s600.maxForwardW, 3000);
    QCOMPARE(m_applet->reflectedMaxForTesting(), s600.maxReflectedW);
    QCOMPARE(m_applet->reflectedNominalForTesting(), s600.nominalReflectedW);

    // Jetzt dauerhaft 1100 W -- das kann nur ein Geraet der 1200S-Klasse.
    m_amp->forwardW = 1100;
    QTRY_COMPARE_WITH_TIMEOUT(m_verb->currentModel(), QStringLiteral("1200S"), 4000);

    // Und BEIDE Achsen ziehen nach. Der Ruecklauf eigens geprueft: die
    // Sprechblase traegt nur die Vorlaufzahlen, eine Gegenprobe, die
    // allein die Ruecklauf-Achse entfernt, liefe sonst gruen durch --
    // genau so ist sie es beim ersten Anlauf.
    const auto& s1200 = Acom::modelSpec(QStringLiteral("1200S"));
    QTRY_COMPARE_WITH_TIMEOUT(m_applet->powerMaxForTesting(), s1200.maxForwardW, 3000);
    QVERIFY2(m_applet->reflectedMaxForTesting() == s1200.maxReflectedW,
             qPrintable(QStringLiteral("Die Ruecklauf-Achse steht noch auf %1 W, "
                                       "erwartet %2 W -- sie wurde nicht "
                                       "nachgezogen")
                            .arg(m_applet->reflectedMaxForTesting())
                            .arg(s1200.maxReflectedW)));
    QCOMPARE(m_applet->reflectedNominalForTesting(), s1200.nominalReflectedW);

    // Und das Feld zieht nach: Stufenname, Sprechblase mit der neuen
    // Achse, und der Grund ist jetzt „aus der Leistung geschlossen".
    QTRY_VERIFY_WITH_TIMEOUT(
        m_applet->diagnosticTooltipForTesting().contains(QStringLiteral("1200S")), 3000);
    QVERIFY2(m_applet->diagnosticTooltipForTesting().contains(QStringLiteral("auto-scaled")),
             qPrintable(QStringLiteral("Der Grund fehlt: %1")
                            .arg(m_applet->diagnosticTooltipForTesting())));
    QVERIFY2(m_applet->diagnosticTooltipForTesting().contains(QStringLiteral("1000")),
             qPrintable(QStringLiteral("Die Nennleistung der 1200S-Stufe "
                                       "(1000 W, NICHT 1200) fehlt: %1")
                            .arg(m_applet->diagnosticTooltipForTesting())));

    m_amp->forwardW = 450;
    m_amp->antwortetAufSystemauskunft = true;
}

void TstAcomAnbindung::dieDreiZustandsknoepfeGehenHinaus()
{
    // Drei Zielzustaende, drei Knoepfe -- kein Umschalter. „Aus" ist
    // keine Gegenseite der anderen zwei.
    m_amp->modeNibble = 0x6;
    QVERIFY(verbinden());
    QTRY_COMPARE_WITH_TIMEOUT(m_applet->statusPillTextForTesting(),
                              QStringLiteral("OPR/RX"), 3000);
    // Wer auf OPERATE steht, braucht den OPERATE-Knopf nicht.
    QVERIFY2(!m_applet->findChildren<QPushButton*>().isEmpty(), "keine Knoepfe");

    m_amp->empfangenLeeren();
    emit m_applet->standbyClicked();
    QTRY_VERIFY_WITH_TIMEOUT(
        m_amp->empfangen().contains(Acom::buildModeCommand(Acom::ModeCommand::Standby)),
        3000);
    QTRY_COMPARE_WITH_TIMEOUT(m_applet->statusPillTextForTesting(),
                              QStringLiteral("STANDBY"), 3000);

    m_amp->empfangenLeeren();
    emit m_applet->operateClicked();
    QTRY_COMPARE_WITH_TIMEOUT(m_applet->statusPillTextForTesting(),
                              QStringLiteral("OPR/RX"), 3000);

    m_amp->empfangenLeeren();
    emit m_applet->offClicked();
    QTRY_VERIFY_WITH_TIMEOUT(
        m_amp->empfangen().contains(Acom::buildModeCommand(Acom::ModeCommand::PowerOff)),
        3000);
    m_amp->modeNibble = 0x6;
}

void TstAcomAnbindung::einFehlerSchaltetDenLoeschknopfFrei()
{
    m_amp->errorCode = 0xFF;
    QVERIFY(verbinden());
    QTRY_VERIFY_WITH_TIMEOUT(m_amp->schiebtGerade(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(!m_applet->clearFaultEnabledForTesting(), 3000);
    QVERIFY(m_applet->faultTextForTesting().isEmpty());

    m_amp->errorCode = 0x39;   // „Excessive PAM current"
    QTRY_VERIFY_WITH_TIMEOUT(
        m_applet->faultTextForTesting().contains(QStringLiteral("Excessive PAM")), 3000);
    QVERIFY2(m_applet->clearFaultEnabledForTesting(),
             "Bei anliegendem Fehler muss der Loeschknopf bedienbar werden");

    emit m_applet->clearFaultClicked();
    QTRY_VERIFY_WITH_TIMEOUT(m_applet->faultTextForTesting().isEmpty(), 3000);
    QVERIFY2(!m_applet->clearFaultEnabledForTesting(),
             "Ohne Fehler hat der Loeschknopf keinen Anlass -- er bleibt "
             "sichtbar, aber grau");
}

QTEST_MAIN(TstAcomAnbindung)
#include "tst_acom_anbindung.moc"
