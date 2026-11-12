// no-port-check: Longpath-original test file.
//
// Der TS-590 als eigenes Funkgeraet im Verbinden-Fenster (Betreiber,
// 2026-11-07: "ich möchte den 590 normal auch auswählen können und
// verbinden").
//
// Festgehalten wird, was daran eine Entscheidung ist:
//
//   1. Longpath tastet das Geraet nicht. Auf keinem Weg: die Verbindung
//      kann es nicht, RigctldClient hat keinen Befehl dafuer, und MOX
//      wird mit einem Grund abgewiesen. Das Doppel unten zaehlt jeden
//      Befehl, der ankommt.
//   2. Der Eintrag entsteht aus den Einstellungen, nur mit Anschluss.
//   3. Geraet und Scheibe bleiben gleich, ohne Kreis und ohne dass eine
//      alte Meldung den VFO zurueckzieht.
//   4. Kein Text wird zum zweiten Befehl an rigctld.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-07 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QtTest>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QPointer>
#include <QSignalSpy>
#include <QSlider>
#include <QTcpServer>
#include <QTcpSocket>

#include <cmath>
#include <complex>

#include "core/AppSettings.h"
#include "core/AudioZuIq.h"
#include "core/BoardCapabilities.h"
#include "core/CatRigBruecke.h"
#include "core/CatRigConnection.h"
#include "core/CompanionRigController.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/RigctldClient.h"
#include "core/RigctldServer.h"
#include "core/RxChannel.h"
#include "core/Ts590Cat.h"
#include "core/TransverterSetup.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "core/FFTRouter.h"
#include "gui/MainWindow.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/CatRigApplet.h"
#include "gui/SpectrumWidget.h"
#include "gui/WindowPlacement.h"

using namespace Longpath;

namespace {

// Ein rigctld zum Anfassen: antwortet wie das echte und merkt sich jeden
// Befehl, der kein reines Fragen ist.
class RigctldDoppel : public QObject {
    Q_OBJECT
public:
    explicit RigctldDoppel(QObject* parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket* s = m_server.nextPendingConnection()) {
                connect(s, &QTcpSocket::readyRead, this, [this, s]() { lies(s); });
            }
        });
        m_server.listen(QHostAddress::LocalHost, 0);
    }

    quint16 port() const { return m_server.serverPort(); }

    qint64     frequenz{28078400};
    QString    modus{QStringLiteral("USB")};
    QByteArrayList befehle;
    QByteArrayList rohFragen;
    bool gesendet{false};
    bool nullGetrennt{false};   // die Null einer Rohantwort als eigenes Paket
    QHash<QByteArray, QByteArray> roh{
        {"SM0;", "SM00015;"}, {"RM;", "RM10007;"}, {"AN;", "AN100;"}, {"PA;", "PA10;"},
        {"RA;", "RA0000;"}, {"RG;", "RG200;"}, {"GC;", "GC2;"}, {"NB;", "NB0;"},
        {"NR;", "NR1;"}, {"SL;", "SL02;"}, {"SH;", "SH09;"}, {"PC;", "PC005;"},
        {"IF;", "IF00028078400      000000000020000080;"}, {"NT;", "NT00;"},
        {"NL;", "?;"}, {"RL;", "RL05;"}};

    bool hatGetastet() const
    {
        for (const QByteArray& b : befehle) {
            const QByteArray t = b.trimmed();
            if (t.startsWith("T ") || t.startsWith("\\set_ptt") || t.startsWith("b ")) {
                return true;
            }
            if (t.startsWith("W ")) {
                bool erlaubt = false;
                for (const char* k : {"W GC", "W SH", "W SL", "W FW", "W FL", "W RT", "W RU",
                                      "W RD", "W RC", "W NT", "W NL", "W RL"}) {
                    if (t.startsWith(k)) { erlaubt = true; }
                }
                if (!erlaubt) { return true; }
            }
            for (const char* verboten : {"w TX", "w KY", "w PB", "w PS", "w EX", "w PC1", "l RFPOWER"}) {
                if (t.startsWith(verboten)) { return true; }
            }
        }
        return false;
    }

private:
    void lies(QTcpSocket* s)
    {
        while (s->canReadLine()) {
            const QByteArray zeile = s->readLine();
            const QByteArray t = zeile.trimmed();
            if (t == "f") {
                s->write(QByteArray::number(frequenz) + '\n');
            } else if (t == "m") {
                s->write(modus.toLatin1() + "\n2200\n");
            } else if (t == "t") {
                s->write(gesendet ? "1\n" : "0\n");
            } else if (t.startsWith("w ")) {
                // Wie rigctld: die Antwort des Geraets, kein Zeilenumbruch.
                const QByteArray frage = t.mid(2);
                if (roh.contains(frage) && nullGetrennt) {
                    s->write(roh.value(frage));
                    s->flush();
                    QPointer<QTcpSocket> spaeter(s);
                    QTimer::singleShot(15, s, [spaeter]() {
                        if (spaeter) { spaeter->write(QByteArray(1, '\0')); }
                    });
                } else if (roh.contains(frage)) { s->write(roh.value(frage) + '\0'); }
                else { s->write("RPRT -1\n"); }
                rohFragen.append(frage);
            } else if (t.startsWith("W ")) {
                // Wie rigctld: kein RPRT, nur ein Nullbyte.
                befehle.append(zeile);
                s->write(QByteArray(1, '\0'));
            } else {
                befehle.append(zeile);
                if (t.startsWith("F ")) { frequenz = t.mid(2).toLongLong(); }
                if (t.startsWith("M ")) { modus = QString::fromLatin1(t.split(' ').value(1)); }
                s->write("RPRT 0\n");
            }
        }
    }

    QTcpServer m_server;
};

// Eine verbundene CAT-Verbindung ohne Ton und ohne Hochlauf.
class VerbundeneCatRig : public CatRigConnection
{
public:
    explicit VerbundeneCatRig(const RadioInfo& info)
    {
        m_radioInfo = info;
        setState(ConnectionState::Connected);
    }
};

// Energie des Signals `hz` ueber dem Traeger, negativ darunter. Im
// Empfangsweg liegt das bei der umgekehrten Frequenz, siehe AudioZuIq.h.
double energieBei(const QVector<float>& iq, double hz, int ab)
{
    hz = -hz;
    std::complex<double> summe(0.0, 0.0);
    const int n = static_cast<int>(iq.size()) / 2;
    for (int i = ab; i < n; ++i) {
        const double w = -2.0 * M_PI * hz * i / 48000.0;
        summe += std::complex<double>(iq[2 * i], iq[2 * i + 1])
                 * std::complex<double>(std::cos(w), std::sin(w));
    }
    return std::abs(summe) / (n - ab);
}

QVector<float> sinus(double hz, int n)
{
    QVector<float> ton(n);
    for (int i = 0; i < n; ++i) {
        ton[i] = 0.5f * static_cast<float>(std::sin(2.0 * M_PI * hz * i / 48000.0));
    }
    return ton;
}

void stelleEin(const QString& schluessel, const QString& wert)
{
    AppSettings::instance().setValue(schluessel, wert);
}

void zielAuf(quint16 port)
{
    stelleEin(QStringLiteral("CompanionRigEnabled"), QStringLiteral("True"));
    stelleEin(QStringLiteral("CompanionRigStartDaemon"), QStringLiteral("False"));
    stelleEin(QStringLiteral("CompanionRigHost"), QStringLiteral("127.0.0.1"));
    stelleEin(QStringLiteral("CompanionRigPort"), QString::number(port));
    stelleEin(QStringLiteral("CompanionRigModel"), QStringLiteral("2031"));
    stelleEin(QStringLiteral("CompanionRigDevice"), QStringLiteral("/dev/cu.fake"));
    stelleEin(QStringLiteral("CompanionRigName"), QStringLiteral("TS-590S"));
}

} // namespace

class TstCatFunkgeraet : public QObject {
    Q_OBJECT

private slots:
    void dieFabrikBautDieCatVerbindung()
    {
        RadioInfo info;
        info.protocol = ProtocolVersion::CatRig;
        auto conn = RadioConnection::create(info);
        QVERIFY(conn != nullptr);
        QVERIFY(qobject_cast<CatRigConnection*>(conn.get()) != nullptr);
        QCOMPARE(conn->protocolVersion(), 6);
    }

    void dieTabelleKenntDasGeraetAlsNurEmpfang()
    {
        const BoardCapabilities& caps = BoardCapsTable::forBoard(HPSDRHW::CatRig);
        QCOMPARE(caps.board, HPSDRHW::CatRig);
        QVERIFY(caps.isRxOnlySku);
        // Ohne Strom bindet sich die Scheibe an nichts: Ton ja, Bild nein.
        QCOMPARE(caps.userDdcCount, 1);
        QVERIFY(caps.maxSlices >= 1);
    }

    // Der Ton muss dort liegen, wo ein SDR ihn zeigen wuerde: in USB ueber
    // dem Traeger, in LSB darunter. Sonst stuende im Wasserfall die falsche
    // Seite, und WDSP haette nichts zu demodulieren.
    void einTonLiegtAufDerRichtigenSeiteDesTraegers()
    {
        const QVector<float> ton = sinus(1000.0, 24000);

        AudioZuIq usb;
        const QVector<float> oben = usb.verarbeite(ton.constData(), ton.size());
        const double echt = energieBei(oben, 1000.0, 1000);
        const double spiegel = energieBei(oben, -1000.0, 1000);
        QVERIFY2(echt > 0.2, qPrintable(QString::number(echt)));
        QVERIFY2(spiegel < echt * 0.01,   // -40 dB
                 qPrintable(QStringLiteral("%1 gegen %2").arg(spiegel).arg(echt)));

        AudioZuIq lsb;
        lsb.setGespiegelt(true);
        const QVector<float> unten = lsb.verarbeite(ton.constData(), ton.size());
        QVERIFY(energieBei(unten, -1000.0, 1000) > 0.2);
        QVERIFY(energieBei(unten, 1000.0, 1000) < energieBei(unten, -1000.0, 1000) * 0.01);
    }

    void derWandlerHaeltDenPegel()
    {
        const QVector<float> ton = sinus(1500.0, 12000);
        AudioZuIq w;
        const QVector<float> iq = w.verarbeite(ton.constData(), ton.size());
        double spitze = 0.0;
        for (int i = 1000; i < ton.size(); ++i) {
            spitze = std::max(spitze, std::hypot(double(iq[2 * i]), double(iq[2 * i + 1])));
        }
        QVERIFY2(std::abs(spitze - 0.5) < 0.02, qPrintable(QString::number(spitze)));
    }

    void derTonKommtInBloeckenBeiDerVerarbeitungAn()
    {
        CatRigConnection verbindung;
        QSignalSpy bloecke(&verbindung, &RadioConnection::iqDataReceived);

        // Zwei Kanaele, 1,5 Bloecke: einer kommt, der Rest wartet.
        const int proben = CatRigConnection::kBlockProben * 3 / 2;
        QByteArray pcm(proben * 4, Qt::Uninitialized);
        auto* roh = reinterpret_cast<qint16*>(pcm.data());
        for (int i = 0; i < proben; ++i) {
            roh[2 * i]     = 8192;
            roh[2 * i + 1] = 0;
        }
        verbindung.verarbeiteTon(pcm, 2);
        QCOMPARE(bloecke.size(), 1);
        QCOMPARE(bloecke.first().at(1).value<QVector<float>>().size(),
                 CatRigConnection::kBlockProben * 2);

        verbindung.verarbeiteTon(pcm, 2);
        QCOMPARE(bloecke.size(), 3);
    }

    // In CTUN steht der VFO nicht in der Mitte des Stroms. WDSP stimmt
    // dann um den Abstand ab, also muss der Ton auch dort liegen.
    void derTonFolgtDemTraegerImStrom()
    {
        CatRigConnection verbindung;
        verbindung.setReceiverFrequency(0, 28080000);
        verbindung.setTraegerHz(28078400.0);

        QVector<float> gesammelt;
        connect(&verbindung, &RadioConnection::iqDataReceived, this,
                [&gesammelt](int, const QVector<float>& block) { gesammelt += block; });

        const QVector<float> ton = sinus(1000.0, 24000);
        QByteArray pcm(ton.size() * 2, Qt::Uninitialized);
        auto* roh = reinterpret_cast<qint16*>(pcm.data());
        for (int i = 0; i < ton.size(); ++i) {
            roh[i] = static_cast<qint16>(ton[i] * 32767.0f);
        }
        verbindung.verarbeiteTon(pcm, 1);

        // 1000 Hz ueber einem Traeger, der 1600 Hz unter der Mitte steht.
        QVERIFY(gesammelt.size() > 20000);
        const double dort = energieBei(gesammelt, -600.0, 1000);
        const double amTraeger = energieBei(gesammelt, 1000.0, 1000);
        QVERIFY2(dort > 0.2, qPrintable(QString::number(dort)));
        QVERIFY2(amTraeger < dort * 0.01, qPrintable(QString::number(amTraeger)));
    }

    void dieAntwortenDesGeraetsWerdenGelesen()
    {
        Ts590Cat::Zustand z;
        QVERIFY(Ts590Cat::leseAntwort(QByteArray("AN101;\0", 7), z));
        QCOMPARE(z.antenne, 2);
        QCOMPARE(z.rxAnt, 0);
        QCOMPARE(z.drive, 1);
        QVERIFY(Ts590Cat::leseAntwort("PA10;", z));
        QCOMPARE(z.vorverstaerker, 1);
        QVERIFY(Ts590Cat::leseAntwort("RA0100;", z));
        QCOMPARE(z.daempfung, 1);
        QVERIFY(Ts590Cat::leseAntwort("RG128;", z));
        QCOMPARE(z.rfGain, 128);
        QVERIFY(Ts590Cat::leseAntwort("SM00030;", z));
        QCOMPARE(z.meterPunkte, 30);
        QVERIFY(Ts590Cat::leseAntwort("RM10012;", z));
        QCOMPARE(z.swrPunkte, 12);
        QVERIFY(Ts590Cat::leseAntwort("SL02;", z));
        QCOMPARE(z.lowCutHz, 100);
        QVERIFY(Ts590Cat::leseAntwort("SH09;", z));
        QCOMPARE(z.highCutHz, 2800);
        QVERIFY(Ts590Cat::leseAntwort("FW0250;", z));
        QCOMPARE(z.cwBreiteHz, 250);
        QVERIFY(Ts590Cat::leseAntwort("FL2;", z));
        QCOMPARE(z.filter, 2);
        // So am TS-590S gelesen (2026-11-12): -120 Hz, RIT an.
        QVERIFY(Ts590Cat::leseAntwort("IF00014090000     -012010000020000080;", z));
        QCOMPARE(z.ritHz, -120);
        QCOMPARE(z.ritAn, 1);
        QVERIFY(Ts590Cat::leseAntwort("IF00014090000      020010000020000080;", z));
        QCOMPARE(z.ritHz, 200);
        QVERIFY(Ts590Cat::leseAntwort("NT10;", z));
        QCOMPARE(z.notch, 1);
        QVERIFY(Ts590Cat::leseAntwort("NL006;", z));
        QCOMPARE(z.nbPegel, 6);
        QVERIFY(Ts590Cat::leseAntwort("RL05;", z));
        QCOMPARE(z.nrPegel, 5);
        QVERIFY(Ts590Cat::leseAntwort("RM30012;", z));
        QCOMPARE(z.alcPunkte, 12);
        QVERIFY(!Ts590Cat::leseAntwort("?;", z));
        QVERIFY(Ts590Cat::leseAntwort("PC005;", z));
        QCOMPARE(z.leistungW, 5);
        QVERIFY(!Ts590Cat::leseAntwort("XX1;", z));
        QVERIFY(!Ts590Cat::leseAntwort("RPRT -1", z));
    }

    void dieBefehlslisteKannNichtSenden()
    {
        for (const QByteArray& f : Ts590Cat::abfragen()) {
            QVERIFY2(f.endsWith(';') && f.size() == 3, f.constData());
            for (const char* v : {"TX", "RX", "KY", "PB", "PS", "EX"}) {
                QVERIFY2(!f.startsWith(v), f.constData());
            }
        }
        const QByteArrayList zeilen = {
            Ts590Cat::stelleVorverstaerker(true), Ts590Cat::stelleDaempfung(true),
            Ts590Cat::stelleRfGain(128),
            Ts590Cat::stelleNb(true), Ts590Cat::stelleNr(true)};
        for (const QByteArray& z : zeilen) {
            QVERIFY2(z.startsWith("L ") || z.startsWith("U "), z.constData());
            QVERIFY2(!z.contains("RFPOWER") && !z.contains("PTT"), z.constData());
        }
        QCOMPARE(Ts590Cat::stelleRfGain(255), QByteArray("L RF 1.000\n"));
        QCOMPARE(Ts590Cat::stelleLowCut(100), QByteArray("W SL02; 0\n"));
        QCOMPARE(Ts590Cat::stelleHighCut(2800), QByteArray("W SH09; 0\n"));
        QCOMPARE(Ts590Cat::stelleHighCut(2850), QByteArray("W SH09; 0\n"));
        QCOMPARE(Ts590Cat::stelleCwBreite(480), QByteArray("W FW0500; 0\n"));
        QCOMPARE(Ts590Cat::stelleFilter(1), QByteArray("W FL1; 0\n"));
        QCOMPARE(Ts590Cat::stelleRit(true), QByteArray("W RT1; 0\n"));
        QCOMPARE(Ts590Cat::stelleRitSchritt(10), QByteArray("W RU00010; 0\n"));
        QCOMPARE(Ts590Cat::stelleRitSchritt(-10), QByteArray("W RD00010; 0\n"));
        QCOMPARE(Ts590Cat::stelleRitNull(), QByteArray("W RC; 0\n"));
        QCOMPARE(Ts590Cat::stelleNotch(true), QByteArray("W NT10; 0\n"));
        QCOMPARE(Ts590Cat::stelleNotch(false), QByteArray("W NT00; 0\n"));
        QCOMPARE(Ts590Cat::stelleNbPegel(5), QByteArray("W NL005; 0\n"));
        QCOMPARE(Ts590Cat::stelleNrPegel(12), QByteArray("W RL10; 0\n"));
        QCOMPARE(Ts590Cat::stelleFilter(2), QByteArray("W FL2; 0\n"));
        QCOMPARE(Ts590Cat::stelleAgc(Ts590Cat::Agc::Aus), QByteArray("W GC0; 0\n"));
        QCOMPARE(Ts590Cat::stelleAgc(Ts590Cat::Agc::Langsam), QByteArray("W GC1; 0\n"));
        QCOMPARE(Ts590Cat::stelleAgc(Ts590Cat::Agc::Schnell), QByteArray("W GC2; 0\n"));
        QCOMPARE(Ts590Cat::punkteZuDbm(15), -73.0);
        QCOMPARE(Ts590Cat::naechsterHighCut(2850), 2800);
    }

    // Die Antwort auf `W` ist ein Nullbyte. Bleibt das liegen, steht die
    // ganze Abfrage bis zur Antwortwache still.
    void agc_haelt_die_abfrage_nicht_auf()
    {
        RigctldDoppel doppel;
        zielAuf(doppel.port());
        CompanionRigController rig;
        rig.setCatBetrieb(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.verbunden(), 20000);
        rig.client()->setPollIntervalMs(20);
        // Langsam abfragen: reihum kaeme in der Frist unten nur eine Antwort.
        rig.client()->setPollIntervalMs(5000);
        doppel.rohFragen.clear();
        rig.stelle(Ts590Cat::stelleAgc(Ts590Cat::Agc::Langsam));
        QTRY_VERIFY_WITH_TIMEOUT(doppel.befehle.contains("W GC1; 0\n"), 2000);
        // Nachgelesen wird sofort, sonst springt die Taste bis zur naechsten
        // Runde zurueck.
        QTRY_VERIFY_WITH_TIMEOUT(doppel.rohFragen.size() >= Ts590Cat::abfragen().size(), 4000);
        rig.client()->setPollIntervalMs(20);
        doppel.frequenz = 28111000;
        QTRY_COMPARE_WITH_TIMEOUT(rig.client()->frequencyHz(), qint64(28111000), 20000);
        QVERIFY(rig.verbunden());
    }

    void derZustandKommtVomGeraetUndWarntAmTransverter()
    {
        RigctldDoppel doppel;
        zielAuf(doppel.port());

        TransverterSetup t;
        t.enabled = true;
        t.offsetHz = 116000000;
        t.save();

        CompanionRigController rig;
        QSignalSpy warnung(&rig, &CompanionRigController::problem);
        rig.setCatBetrieb(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.verbunden(), 20000);
        rig.client()->setPollIntervalMs(20);

        QTRY_COMPARE_WITH_TIMEOUT(rig.catZustand().leistungW, 5, 20000);
        QCOMPARE(rig.catZustand().antenne, 2);
        QCOMPARE(rig.catZustand().rxAnt, 0);
        QCOMPARE(rig.catZustand().rfGain, 200);
        QCOMPARE(rig.catZustand().meterPunkte, 15);
        QTRY_VERIFY_WITH_TIMEOUT(!warnung.isEmpty(), 20000);
        QVERIFY(warnung.first().first().toString().contains(QStringLiteral("RX ANT")));
        const int anzahl = warnung.size();

        // Stehen beide richtig, schweigt die Warnung und bleibt still.
        doppel.roh["AN;"] = "AN111;";
        QTRY_COMPARE_WITH_TIMEOUT(rig.catZustand().drive, 1, 20000);
        QCOMPARE(warnung.size(), anzahl);

        // Der Sendezustand kommt vom Geraet, das SWR nur dann.
        QVERIFY(!rig.sendet());
        doppel.gesendet = true;
        QTRY_VERIFY_WITH_TIMEOUT(rig.sendet(), 20000);
        QTRY_COMPARE_WITH_TIMEOUT(rig.catZustand().swrPunkte, 7, 20000);

        // Stellen geht ueber die geschlossene Liste.
        rig.stelle(Ts590Cat::stelleDaempfung(true));
        QTRY_VERIFY_WITH_TIMEOUT(doppel.befehle.contains("L ATT 12\n"), 20000);
        QVERIFY2(!doppel.hatGetastet(), "Longpath hat das fremde Geraet getastet");

        TransverterSetup{}.save();
        TransverterSetup::setCurrent(TransverterSetup{});
    }

    void dasFilterFolgtDemGeraetInBeidenSeitenbaendern()
    {
        RigctldDoppel doppel;
        zielAuf(doppel.port());
        CompanionRigController rig;
        rig.setCatBetrieb(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.verbunden(), 20000);
        rig.client()->setPollIntervalMs(20);

        SliceModel scheibe;
        CatRigBruecke bruecke(&rig, &scheibe);
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.dspMode(), DSPMode::USB, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.filterHigh(), 2800, 20000);
        QCOMPARE(scheibe.filterLow(), 100);

        // Das Geraet steht in LSB: dieselben Grenzen, unter dem Traeger.
        doppel.modus = QStringLiteral("LSB");
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.dspMode(), DSPMode::LSB, 10000);
        doppel.roh["SH;"] = "SH10;";
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.filterLow(), -3000, 20000);
        QCOMPARE(scheibe.filterHigh(), -100);

        // Von der Scheibe zum Geraet, auf die Tabelle gerundet.
        scheibe.setFilter(-2600, -200);
        QTRY_VERIFY_WITH_TIMEOUT(doppel.befehle.contains("W SH08; 0\n"), 3000);
        QVERIFY(doppel.befehle.contains("W SL03; 0\n"));
    }

    // Beim Ziehen kommt jede Bewegung einzeln. Zum Geraet geht nur die letzte,
    // sonst stapeln sich die Befehle hinter dem Mauszeiger.
    void ziehen_an_der_bandbreite_schickt_nur_das_letzte()
    {
        RigctldDoppel doppel;
        zielAuf(doppel.port());
        CompanionRigController rig;
        rig.setCatBetrieb(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.verbunden(), 20000);
        rig.client()->setPollIntervalMs(20);
        SliceModel scheibe;
        CatRigBruecke bruecke(&rig, &scheibe);
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.filterHigh(), 2800, 20000);

        doppel.befehle.clear();
        for (int hz = 2000; hz <= 3000; hz += 50) {
            scheibe.setFilter(100, hz);
            QTest::qWait(2);
        }
        QTRY_VERIFY_WITH_TIMEOUT(doppel.befehle.contains("W SH10; 0\n"), 2000);
        QCOMPARE(doppel.befehle.size(), 1);

        // Das Geraet rundet auf seine Tabelle: die Anzeige folgt ihm, auch
        // wenn sich dort nichts mehr aendert.
        scheibe.setFilter(100, 2850);
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.filterHigh(), 2800, 20000);
    }

    // Die Filtertasten halten die Mitte fest; eine Kante landet dabei jenseits
    // des Traegers. Am 590 wurde aus 2.9k so 2400 Hz.
    void filtertaste_ueber_den_traeger_behaelt_die_breite()
    {
        RigctldDoppel doppel;
        doppel.modus = QStringLiteral("LSB");
        doppel.roh["SL;"] = "SL01;";
        doppel.roh["SH;"] = "SH07;";
        zielAuf(doppel.port());
        CompanionRigController rig;
        rig.setCatBetrieb(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.verbunden(), 20000);
        rig.client()->setPollIntervalMs(20);
        SliceModel scheibe;
        CatRigBruecke bruecke(&rig, &scheibe);
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.dspMode(), DSPMode::LSB, 20000);
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.filterLow(), -2400, 20000);

        doppel.befehle.clear();
        scheibe.setFilter(-2675, 225);   // Mitte -1225, 2900 Hz breit
        QTRY_VERIFY_WITH_TIMEOUT(doppel.befehle.contains("W SH10; 0\n"), 20000);
        QCOMPARE(doppel.befehle.size(), 1);   // die untere Kante bleibt 50 Hz

        // Dasselbe in USB, auf der anderen Seite des Traegers.
        doppel.roh["SH;"] = "SH10;";
        doppel.modus = QStringLiteral("USB");
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.dspMode(), DSPMode::USB, 20000);
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.filterHigh(), 3000, 20000);
        doppel.befehle.clear();
        scheibe.setFilter(-175, 2225);   // 2400 Hz breit
        QTRY_VERIFY_WITH_TIMEOUT(doppel.befehle.contains("W SH07; 0\n"), 20000);
    }

    // Am Geraet gedreht, kurz nachdem Longpath etwas geschickt hat: die
    // Meldung faellt in die Frist und darf trotzdem nicht verloren gehen.
    // Gefunden auf dem langsamen CI-Laeufer, der genau dieses Fenster traf.
    void eine_meldung_in_der_frist_geht_nicht_verloren()
    {
        RigctldDoppel doppel;
        zielAuf(doppel.port());
        CompanionRigController rig;
        rig.setCatBetrieb(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.verbunden(), 20000);
        rig.client()->setPollIntervalMs(20);
        SliceModel scheibe;
        CatRigBruecke bruecke(&rig, &scheibe);
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.dspMode(), DSPMode::USB, 20000);
        QTRY_COMPARE_WITH_TIMEOUT(qRound64(scheibe.frequency()), qint64(28078400), 20000);

        bruecke.fristSetzenForTest(1500);
        scheibe.setFrequency(28100000.0);
        QTRY_COMPARE_WITH_TIMEOUT(doppel.frequenz, qint64(28100000), 20000);
        doppel.modus = QStringLiteral("LSB");
        doppel.frequenz = 28120000;
        QTRY_COMPARE_WITH_TIMEOUT(rig.client()->mode(), QStringLiteral("LSB"), 20000);

        QTRY_COMPARE_WITH_TIMEOUT(scheibe.dspMode(), DSPMode::LSB, 20000);
        QTRY_COMPARE_WITH_TIMEOUT(qRound64(scheibe.frequency()), qint64(28120000), 20000);
        bruecke.fristSetzenForTest(CatRigBruecke::kFristNachBefehlMs);
    }

    // Ein anderes Geraet als der TS-590S bekommt keine Kenwood-Zeilen, auch
    // nicht ueber die Bruecke.
    void ein_anderes_geraet_bekommt_keine_kenwood_zeilen()
    {
        RigctldDoppel doppel;
        zielAuf(doppel.port());
        stelleEin(QStringLiteral("CompanionRigModel"), QStringLiteral("2037"));
        CompanionRigController rig;
        rig.setCatBetrieb(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.verbunden(), 20000);
        rig.client()->setPollIntervalMs(20);
        SliceModel scheibe;
        CatRigBruecke bruecke(&rig, &scheibe);
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.dspMode(), DSPMode::USB, 20000);

        rig.stelle(Ts590Cat::stelleAgc(Ts590Cat::Agc::Langsam));
        rig.stelle(Ts590Cat::stelleVorverstaerker(false));
        scheibe.setFilter(100, 2000);
        QTest::qWait(800);
        for (const QByteArray& b : std::as_const(doppel.befehle)) {
            QVERIFY2(!b.startsWith("W ") && !b.startsWith("L ") && !b.startsWith("U "), b.constData());
        }
        stelleEin(QStringLiteral("CompanionRigModel"), QStringLiteral("2031"));
    }

    // Eine Betriebsart, die das Geraet nicht kennt, kippt nach der Frist nicht
    // auf die des Geraets zurueck.
    void sam_bleibt_sam()
    {
        RigctldDoppel doppel;
        doppel.modus = QStringLiteral("AM");
        zielAuf(doppel.port());
        CompanionRigController rig;
        rig.setCatBetrieb(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.verbunden(), 20000);
        rig.client()->setPollIntervalMs(20);
        SliceModel scheibe;
        CatRigBruecke bruecke(&rig, &scheibe);
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.dspMode(), DSPMode::AM, 20000);

        scheibe.setDspMode(DSPMode::SAM);
        scheibe.setFrequency(28090000.0);
        QTRY_COMPARE_WITH_TIMEOUT(doppel.frequenz, qint64(28090000), 20000);
        QTest::qWait(1500);
        QCOMPARE(scheibe.dspMode(), DSPMode::SAM);

        // Schaltet man am Geraet wirklich um, gilt das weiter.
        doppel.modus = QStringLiteral("USB");
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.dspMode(), DSPMode::USB, 20000);
    }

    // Die neuen Regler im Feld schicken, was am Geraet gemessen wurde.
    void rit_notch_und_pegel_gehen_zum_geraet()
    {
        RigctldDoppel doppel;
        doppel.roh["NB;"] = "NB1;";
        doppel.roh["NL;"] = "NL006;";
        zielAuf(doppel.port());
        const auto info = CompanionRigController::radioInfoAusEinstellungen();
        QVERIFY(info.has_value());
        RadioModel radio;
        radio.setLastRadioInfoForTest(*info);
        radio.onConnectionStateChangedForTest(ConnectionState::Connected);
        QTRY_VERIFY_WITH_TIMEOUT(radio.companionRig()->verbunden(), 20000);
        CatRigApplet feld(&radio);
        QTRY_VERIFY_WITH_TIMEOUT(radio.companionRig()->catZustand().nbPegel == 6, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(radio.companionRig()->catZustand().ritAn == 0, 20000);

        const auto drueck = [&](const char* name) {
            auto* t = feld.findChild<QPushButton*>(QLatin1String(name));
            QVERIFY2(t, name);
            t->click();
        };
        doppel.befehle.clear();
        drueck("catRigRit");
        drueck("catRigRitHoch");
        drueck("catRigRitRunter");
        drueck("catRigRitNull");
        drueck("catRigNotch");
        for (const char* b : {"W RT1; 0\n", "W RU00010; 0\n", "W RD00010; 0\n", "W RC; 0\n",
                              "W NT10; 0\n"}) {
            QTRY_VERIFY2_WITH_TIMEOUT(doppel.befehle.contains(QByteArray(b)), b, 20000);
        }
        auto* nb = feld.findChild<QSlider*>(QStringLiteral("catRigNbPegel"));
        QVERIFY(nb);
        QTRY_VERIFY_WITH_TIMEOUT(nb->isEnabled() && nb->value() == 6, 20000);
        nb->triggerAction(QAbstractSlider::SliderSingleStepAdd);
        QTRY_VERIFY_WITH_TIMEOUT(doppel.befehle.contains("W NL007; 0\n"), 20000);
        // NR steht im Doppel auf NR1, der Pegel ist bedienbar; bei NR2 nicht.
        auto* nr = feld.findChild<QSlider*>(QStringLiteral("catRigNrPegel"));
        QTRY_VERIFY_WITH_TIMEOUT(nr->isEnabled() && nr->value() == 5, 20000);
        doppel.roh["NR;"] = "NR2;";
        QTRY_VERIFY_WITH_TIMEOUT(!nr->isEnabled(), 20000);
        QVERIFY(!doppel.hatGetastet());

        // Der Versatz steht im Feld.
        doppel.roh["IF;"] = "IF00028078400     -012010000020000080;";
        auto* wert = feld.findChild<QLabel*>(QStringLiteral("catRigRitWert"));
        QVERIFY(wert);
        QTRY_COMPARE_WITH_TIMEOUT(wert->text(), QStringLiteral("-120 Hz"), 20000);
    }

    // Beim Senden zeigt das Feld ALC, wenn das Geraet ALC als Instrument hat.
    void beim_senden_steht_alc_im_feld()
    {
        RigctldDoppel doppel;
        doppel.gesendet = true;
        doppel.roh["RM;"] = "RM30007;";
        zielAuf(doppel.port());
        const auto info = CompanionRigController::radioInfoAusEinstellungen();
        RadioModel radio;
        radio.setLastRadioInfoForTest(*info);
        radio.onConnectionStateChangedForTest(ConnectionState::Connected);
        QTRY_VERIFY_WITH_TIMEOUT(radio.companionRig()->verbunden(), 20000);
        CatRigApplet feld(&radio);
        auto* mess = feld.findChild<QLabel*>(QStringLiteral("catRigMesswerte"));
        QVERIFY(mess);
        QTRY_VERIFY2_WITH_TIMEOUT(mess->text().contains(QStringLiteral("ALC 7")),
                                  qPrintable(mess->text()), 20000);
    }

    // Mausrad und Tasten loesen kein sliderReleased aus; RF Gain muss trotzdem
    // beim Geraet ankommen.
    void rf_gain_mit_dem_mausrad()
    {
        RigctldDoppel doppel;
        zielAuf(doppel.port());
        const auto info = CompanionRigController::radioInfoAusEinstellungen();
        QVERIFY(info.has_value());
        RadioModel radio;
        radio.setLastRadioInfoForTest(*info);
        radio.onConnectionStateChangedForTest(ConnectionState::Connected);
        QTRY_VERIFY_WITH_TIMEOUT(radio.companionRig()->verbunden(), 20000);
        CatRigApplet feld(&radio);
        QTRY_VERIFY_WITH_TIMEOUT(radio.companionRig()->catZustand().rfGain >= 0, 20000);
        auto* rf = feld.findChild<QSlider*>(QStringLiteral("catRigRfGain"));
        QVERIFY(rf);
        doppel.befehle.clear();
        rf->triggerAction(QAbstractSlider::SliderPageStepAdd);
        const QByteArray erwartet = Ts590Cat::stelleRfGain(rf->value());
        QTRY_VERIFY_WITH_TIMEOUT(doppel.befehle.contains(erwartet), 20000);
    }

    // Bandtaste oder Eingabe auf 2 m: am Geraet muss die ZF ankommen.
    void eine_eingabe_auf_2m_landet_auf_der_zf()
    {
        RigctldDoppel doppel;
        zielAuf(doppel.port());
        TransverterSetup t;
        t.enabled = true;
        t.offsetHz = 116000000;
        t.save();
        TransverterSetup::setCurrent(t);
        CompanionRigController rig;
        rig.setCatBetrieb(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.verbunden(), 20000);
        rig.client()->setPollIntervalMs(20);
        SliceModel scheibe;
        CatRigBruecke bruecke(&rig, &scheibe);
        QTRY_COMPARE_WITH_TIMEOUT(qRound64(scheibe.frequency()), qint64(28078400), 20000);

        scheibe.setFrequency(144300000.0);
        QTRY_COMPARE_WITH_TIMEOUT(doppel.frequenz, qint64(28300000), 20000);
        QTRY_COMPARE_WITH_TIMEOUT(qRound64(scheibe.frequency()), qint64(28300000), 20000);

        t.enabled = false;
        t.save();
        TransverterSetup::setCurrent(t);
    }

    // In CW stellt das Geraet nur die Breite. Die Lage bleibt der Scheibe.
    void in_cw_wird_die_breite_gestellt()
    {
        RigctldDoppel doppel;
        doppel.modus = QStringLiteral("CW");
        doppel.roh["FW;"] = "FW0500;";
        doppel.roh.remove("SL;");
        doppel.roh.remove("SH;");
        zielAuf(doppel.port());
        CompanionRigController rig;
        rig.setCatBetrieb(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.verbunden(), 20000);
        rig.client()->setPollIntervalMs(20);
        SliceModel scheibe;
        CatRigBruecke bruecke(&rig, &scheibe);
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.dspMode(), DSPMode::CWU, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.filterHigh() - scheibe.filterLow(), 500, 20000);
        const int mitte = (scheibe.filterHigh() + scheibe.filterLow()) / 2;

        doppel.roh["FW;"] = "FW0250;";
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.filterHigh() - scheibe.filterLow(), 250, 20000);
        QVERIFY(std::abs((scheibe.filterHigh() + scheibe.filterLow()) / 2 - mitte) <= 1);

        doppel.befehle.clear();
        scheibe.setFilter(mitte - 300, mitte + 300);
        QTRY_VERIFY_WITH_TIMEOUT(doppel.befehle.contains("W FW0600; 0\n"), 2000);
    }

    // Nur von Hand, mit dem Geraet am USB-Kabel. Liest nur den Ton.
    void derEchteTonEingangLiefertBloecke()
    {
        const QByteArray name = qgetenv("LONGPATH_CAT_TON");
        if (name.isEmpty()) { QSKIP("Nur von Hand: LONGPATH_CAT_TON=<Geraetename>"); }

        CatRigConnection verbindung;
        verbindung.setTonEingang(QString::fromUtf8(name));
        QSignalSpy bloecke(&verbindung, &RadioConnection::iqDataReceived);
        RadioInfo info;
        info.protocol = ProtocolVersion::CatRig;
        verbindung.connectToRadio(info);
        QVERIFY2(verbindung.tonLaeuft(), "der Toneingang ist nicht aufgegangen");
        QTest::qWait(3000);
        verbindung.disconnect();

        double summe = 0.0;
        qint64 proben = 0;
        for (const QList<QVariant>& b : std::as_const(bloecke)) {
            const auto iq = b.at(1).value<QVector<float>>();
            for (float v : iq) { summe += double(v) * v; ++proben; }
        }
        const double rms = proben ? std::sqrt(summe / proben) : 0.0;
        qInfo() << "Bloecke" << bloecke.size() << "in 3 s, Pegel"
                << 20.0 * std::log10(rms + 1e-12) << "dBFS";
        QVERIFY(bloecke.size() > 200);
    }

    // Die leere Seite liess Skala und Wasserfall am 2026-11-07 auf -190 dB
    // fallen, waehrend die Daten bei -120 lagen.
    void dieGegenseiteDesTonsIstNichtLeer()
    {
        CatRigConnection verbindung;
        QVector<float> gesammelt;
        connect(&verbindung, &RadioConnection::iqDataReceived, this,
                [&gesammelt](int, const QVector<float>& block) { gesammelt += block; });

        // Weisses Rauschen als Ton: auf der Tonseite breitbandig.
        constexpr int kProben = 48000;
        QByteArray pcm(kProben * 2, Qt::Uninitialized);
        auto* roh = reinterpret_cast<qint16*>(pcm.data());
        quint32 z = 12345u;
        for (int i = 0; i < kProben; ++i) {
            z = z * 1664525u + 1013904223u;
            roh[i] = static_cast<qint16>(int(z >> 16) - 32768) / 8;
        }
        verbindung.verarbeiteTon(pcm, 1);
        QVERIFY(gesammelt.size() > 80000);

        const auto band = [&gesammelt](double vonHz, double bisHz) {
            double summe = 0.0;
            int punkte = 0;
            for (double hz = vonHz; hz < bisHz; hz += 137.0) {
                const double e = energieBei(gesammelt, hz, 1000);
                summe += e * e;
                ++punkte;
            }
            return summe / punkte;
        };
        const double tonseite = band(1000.0, 8000.0);
        const double gegenseite = band(-8000.0, -1000.0);
        const double verhaeltnisDb = 10.0 * std::log10(gegenseite / tonseite);
        QVERIFY2(std::abs(verhaeltnisDb) < 3.0, qPrintable(QString::number(verhaeltnisDb)));
    }

    void ohneTonEingangSteigtDieVerbindungNichtAus()
    {
        CatRigConnection verbindung;
        verbindung.setTonEingang(QStringLiteral("gibt es nicht"));
        RadioInfo info;
        info.protocol = ProtocolVersion::CatRig;
        verbindung.connectToRadio(info);
        QCOMPARE(verbindung.state(), ConnectionState::Connected);
        QVERIFY(!verbindung.tonLaeuft());
        verbindung.disconnect();
        QCOMPARE(verbindung.state(), ConnectionState::Disconnected);
    }

    void ohneAnschlussGibtEsKeinenEintrag()
    {
        stelleEin(QStringLiteral("CompanionRigDevice"), QString());
        stelleEin(QStringLiteral("CompanionRigModel"), QStringLiteral("2031"));
        QVERIFY(!CompanionRigController::radioInfoAusEinstellungen().has_value());

        stelleEin(QStringLiteral("CompanionRigDevice"), QStringLiteral("/dev/cu.fake"));
        stelleEin(QStringLiteral("CompanionRigModel"), QStringLiteral("0"));
        QVERIFY(!CompanionRigController::radioInfoAusEinstellungen().has_value());
    }

    void derEintragTraegtNameUndProtokoll()
    {
        stelleEin(QStringLiteral("CompanionRigDevice"), QStringLiteral("/dev/cu.fake"));
        stelleEin(QStringLiteral("CompanionRigModel"), QStringLiteral("2031"));
        stelleEin(QStringLiteral("CompanionRigName"), QStringLiteral("TS-590S"));
        const auto info = CompanionRigController::radioInfoAusEinstellungen();
        QVERIFY(info.has_value());
        QCOMPARE(info->name, QStringLiteral("TS-590S"));
        QCOMPARE(info->protocol, ProtocolVersion::CatRig);
        QCOMPARE(info->boardType, HPSDRHW::CatRig);
        QVERIFY(!info->macAddress.isEmpty());
    }

    void dieBetriebsartenGehenHinUndZurueck()
    {
        for (const QString& rig : {QStringLiteral("USB"), QStringLiteral("LSB"),
                                   QStringLiteral("CW"), QStringLiteral("CWR"),
                                   QStringLiteral("AM"), QStringLiteral("FM"),
                                   QStringLiteral("PKTUSB"), QStringLiteral("PKTLSB")}) {
            DSPMode m = DSPMode::SPEC;
            QVERIFY2(CatRigBruecke::modusAusGeraet(rig, m), qPrintable(rig));
            QCOMPARE(CatRigBruecke::modusFuerGeraet(m), rig);
        }
        DSPMode m = DSPMode::USB;
        QVERIFY(!CatRigBruecke::modusAusGeraet(QStringLiteral("WFM"), m));
        QVERIFY(CatRigBruecke::modusFuerGeraet(DSPMode::SPEC).isEmpty());
    }

    void keinTextWirdZumZweitenBefehl()
    {
        QCOMPARE(RigctldClient::setModeCommand(QStringLiteral("usb")),
                 QByteArray("M USB 0\n"));
        QVERIFY(RigctldClient::setModeCommand(QStringLiteral("USB\nT 1")).isEmpty());
        QVERIFY(RigctldClient::setModeCommand(QStringLiteral("USB 0")).isEmpty());
        QVERIFY(RigctldClient::setModeCommand(QString()).isEmpty());
    }

    void geraetUndScheibeBleibenGleich()
    {
        RigctldDoppel doppel;
        zielAuf(doppel.port());

        CompanionRigController rig;
        rig.setCatBetrieb(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.verbunden(), 20000);
        QCOMPARE(rig.client()->pollIntervalMs(), CompanionRigController::kKatAbfrageMs);
        rig.client()->setPollIntervalMs(20);

        SliceModel scheibe;
        scheibe.setDspMode(DSPMode::LSB);
        CatRigBruecke bruecke(&rig, &scheibe);
        // Die Frist lang machen: auf einem langsamen CI-Rechner lief sie
        // zwischen Befehl und abweichender Meldung schon ab (macOS-CI,
        // rot ab 2026-11-08). Aufgehoben wird sie unten ausdruecklich.
        bruecke.setFristForTest(10000);

        // Das Geraet gibt den Stand vor.
        QTRY_COMPARE_WITH_TIMEOUT(qRound64(scheibe.frequency()), qint64(28078400), 20000);
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.dspMode(), DSPMode::USB, 10000);

        // Ohne das antwortet rigctld eine Sekunde lang aus dem Speicher.
        QTRY_VERIFY_WITH_TIMEOUT(
            doppel.befehle.contains(QByteArray("\\set_cache 0\n")), 20000);
        for (const QByteArray& b : std::as_const(doppel.befehle)) {
            QVERIFY2(b.startsWith("\\set_cache"),
                     "eine Meldung vom Geraet ging als Befehl zurueck");
        }

        // Die Scheibe stellt das Geraet.
        bruecke.fristSetzenForTest(600000);
        scheibe.setFrequency(28100000.0);
        QTRY_COMPARE_WITH_TIMEOUT(doppel.frequenz, qint64(28100000), 20000);

        // Eine abweichende Meldung in der Frist zieht den VFO nicht zurueck.
        // Erst warten, bis der Client die neue Zahl kennt: sonst aendert
        // sich aus seiner Sicht nichts, und die Pruefung sagt nichts aus.
        QTRY_COMPARE_WITH_TIMEOUT(rig.zfFrequenzHz(), qint64(28100000), 20000);
        doppel.frequenz = 28078400;
        QTRY_COMPARE_WITH_TIMEOUT(rig.zfFrequenzHz(), qint64(28078400), 20000);
        QCOMPARE(qRound64(scheibe.frequency()), qint64(28100000));

        // Nach der Frist gilt das Geraet wieder.
        bruecke.fristSetzenForTest(CatRigBruecke::kFristNachBefehlMs);
        bruecke.fristAufhebenForTest();
        doppel.frequenz = 28200000;
        QTRY_COMPARE_WITH_TIMEOUT(qRound64(scheibe.frequency()), qint64(28200000), 20000);

        // Betriebsart in beide Richtungen.
        scheibe.setDspMode(DSPMode::LSB);
        QTRY_COMPARE_WITH_TIMEOUT(doppel.modus, QStringLiteral("LSB"), 10000);
        bruecke.fristAufhebenForTest();
        doppel.modus = QStringLiteral("CW");
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.dspMode(), DSPMode::CWU, 10000);

        QVERIFY2(!doppel.hatGetastet(), "Longpath hat das fremde Geraet getastet");
    }

    // Die ganze Kette durch den echten Hochlauf, von Hand zu starten: in
    // einem frischen Konfigurationsordner rechnet WDSP erst seine Wisdom,
    // das sprengt die Zeitgrenze eines normalen Laufs.
    void dieGanzeKetteLiefertEinSpektrum()
    {
        if (!qEnvironmentVariableIsSet("LONGPATH_CAT_KETTE")) {
            QSKIP("Nur von Hand: LONGPATH_CAT_KETTE=1");
        }
        RigctldDoppel doppel;
        doppel.frequenz = 28078000;
        zielAuf(doppel.port());
        stelleEin(QStringLiteral("CompanionRigAudioInput"), QStringLiteral("gibt es nicht"));
        const auto info = CompanionRigController::radioInfoAusEinstellungen();
        QVERIFY(info.has_value());

        // So stand der Panadapter des Betreibers: eine gemerkte KIWI-Quelle
        // verwarf jeden Rahmen des Geraets.
        stelleEin(QStringLiteral("Pan_pan-0_KiwiDisplay"), QStringLiteral("True"));

        // Das echte Hauptfenster: die FFT haengt an ihm, nicht am Modell.
        auto* fenster = new MainWindow();   // bewusst nicht abgeraeumt
        fenster->resize(1280, 800);
        fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(fenster, 20000));
        RadioModel& radio = *fenster->radioModelForTest();
        QVector<float> letztes;
        int spanne = 0;
        qint64 mitte = 0;
        int rahmen = 0;
        connect(&radio, &RadioModel::spektrumBereit, this,
                [&](int, const QVector<float>& dbm, int s, qint64 m) {
                    letztes = dbm; spanne = s; mitte = m; ++rahmen;
                });

        radio.connectToRadio(*info);
        QTRY_VERIFY_WITH_TIMEOUT(radio.isConnected(), 300000);
        QTRY_VERIFY_WITH_TIMEOUT(radio.companionRig()->verbunden(), 20000);
        qInfo() << "Abtastrate" << radio.connectionSampleRateHz();
        QCOMPARE(radio.connectionSampleRateHz(), 48000);

        auto* cat = qobject_cast<CatRigConnection*>(
            const_cast<RadioConnection*>(radio.connection()));
        QVERIFY(cat);

        auto speise = [&]() {
            const QVector<float> ton = sinus(1000.0, 48000 * 2);
            constexpr int kStueck = 960;
            for (int ab = 0; ab + kStueck <= ton.size(); ab += kStueck) {
                QByteArray pcm(kStueck * 2, Qt::Uninitialized);
                auto* roh = reinterpret_cast<qint16*>(pcm.data());
                for (int i = 0; i < kStueck; ++i) {
                    roh[i] = static_cast<qint16>(ton[ab + i] * 32767.0f);
                }
                QMetaObject::invokeMethod(cat, [cat, pcm]() { cat->verarbeiteTon(pcm, 1); });
                QTest::qWait(20);
            }
        };
        auto spitzeHz = [&]() {
            int spitze = 0;
            for (int i = 1; i < letztes.size(); ++i) {
                if (letztes[i] > letztes[spitze]) { spitze = i; }
            }
            return -spanne / 2.0 + spitze * double(spanne) / letztes.size();
        };
        auto* kanal = radio.wdspEngine()->rxChannel(0);
        QVERIFY(kanal);

        // Die Scheibe steht in USB: der Ton gehoert ueber den Traeger, im
        // Wasserfall wie im Demodulator.
        cat->setGespiegelt(false);
        speise();
        const double sMeter = kanal->getMeter(RxMeterType::SignalAvg);
        qInfo() << "USB: Spitze" << spitzeHz() << "S" << sMeter << "Rahmen" << rahmen;
        for (SpectrumWidget* sw : fenster->findChildren<SpectrumWidget*>()) {
            const auto& pix = sw->undentedPixelsForTest();
            float lo = 999.f, hi = -999.f;
            for (float v : pix) { lo = std::min(lo, v); hi = std::max(hi, v); }
            qInfo() << "Flaeche" << sw->objectName() << "ddc" << sw->ddcCenterFrequency()
                    << "rate" << sw->sampleRate() << "mitte" << sw->centerFrequency()
                    << "bw" << sw->bandwidth() << "pix" << sw->renderedPixelCountForTest()
                    << "bins" << sw->visibleBinCountForTest() << "nf" << sw->nfFftBinAverageForTest()
                    << "lo" << lo << "hi" << hi << "sichtbar" << sw->isVisible();
        }
        qInfo() << "Scheibenstrom" << (radio.activeSlice() ? radio.activeSlice()->streamIndex() : -9)
                << "Pans" << radio.fftRouter()->pansForReceiver(0)
                << "Frequenz" << (radio.activeSlice() ? radio.activeSlice()->frequency() : 0.0);
        QVERIFY2(radio.activeSlice() && radio.activeSlice()->streamIndex() == 0,
                 "die Scheibe haengt an keinem Strom, der Panadapter bleibt leer");
        QVERIFY(!radio.fftRouter()->pansForReceiver(0).isEmpty());
        for (SpectrumWidget* sw : fenster->findChildren<SpectrumWidget*>()) {
            QVERIFY2(!sw->kiwiDisplaySource(), "der Panadapter steht noch auf KIWI");
            QVERIFY2(sw->renderedPixelCountForTest() > 0, "der Panadapter hat nichts gezeichnet");
        }
        const double spitzeUsb = spitzeHz();
        // Gegenrichtung: derselbe Ton im falschen Seitenband ist im
        // Durchlassbereich nicht zu hoeren.
        cat->setGespiegelt(true);
        speise();
        const double sMeterFalsch = kanal->getMeter(RxMeterType::SignalAvg);
        qInfo() << "falsch: Spitze" << spitzeHz() << "S" << sMeterFalsch;
        cat->setGespiegelt(false);

        // Der Traeger wandert im Fenster: Ton und Demodulator muessen
        // mitgehen, sonst waere ein Signal in CTUN nicht zu hoeren.
        doppel.frequenz = 28078000 + 1500;
        QTRY_COMPARE_WITH_TIMEOUT(radio.companionRig()->zfFrequenzHz(), doppel.frequenz, 4000);
        QTest::qWait(200);
        speise();
        const double sVerschoben = kanal->getMeter(RxMeterType::SignalAvg);
        qInfo() << "verschoben: Spitze" << spitzeHz() << "S" << sVerschoben;
        QVERIFY2(rahmen > 0, "kein Spektrum aus der Kette");
        QVERIFY2(std::abs(spitzeUsb - 1000.0) < 200.0, qPrintable(QString::number(spitzeUsb)));
        // Nicht mehr: die Gegenseite traegt jetzt das Fuellrauschen, und das
        // liegt hier im Durchlassbereich, weil der Ton falsch herum steht.
        QVERIFY2(sMeter > sMeterFalsch + 6.0,
                 qPrintable(QStringLiteral("%1 gegen %2").arg(sMeter).arg(sMeterFalsch)));
        QVERIFY2(sVerschoben > sMeter - 6.0, qPrintable(QString::number(sVerschoben)));

        radio.setMox(true);
        QVERIFY(!radio.mox());
        QVERIFY(!doppel.hatGetastet());
        radio.disconnectFromRadio();
        QTRY_VERIFY_WITH_TIMEOUT(!radio.isConnected(), 10000);
        QVERIFY(!radio.companionRig()->catBetrieb());
    }

    // Nur von Hand, am echten Geraet: die Tasten des Feldes druecken und am
    // Geraet nachlesen. Ohne Fenster; rigctld laeuft auf dem Port aus der
    // Umgebung. Am Ende steht alles wie vorher, nur RF Gain ist offen.
    void feld_am_echten_geraet()
    {
        const int port = qEnvironmentVariableIntValue("LONGPATH_CAT_FELD_PORT");
        if (port <= 0) { QSKIP("Nur von Hand: LONGPATH_CAT_FELD_PORT=<rigctld>"); }
        zielAuf(static_cast<quint16>(port));
        const auto info = CompanionRigController::radioInfoAusEinstellungen();
        QVERIFY(info.has_value());

        QTcpSocket leser;
        leser.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(port));
        QVERIFY(leser.waitForConnected(3000));
        const auto frage = [&leser](const QByteArray& zeile) {
            leser.readAll();
            leser.write(zeile + '\n');
            QByteArray antwort;
            for (int i = 0; i < 40 && !antwort.contains(';') && !antwort.contains('\n'); ++i) {
                leser.waitForReadyRead(50);
                antwort += leser.readAll();
            }
            return QByteArray(antwort).replace('\0', "").trimmed();
        };
        leser.write("\\set_cache 0\n");
        leser.waitForReadyRead(500);
        const QByteArray frequenzVorher = frage("f");

        RadioModel radio;
        radio.setLastRadioInfoForTest(*info);
        radio.installBandPlanMoxCheckForTest();
        radio.onConnectionStateChangedForTest(ConnectionState::Connected);
        QTRY_VERIFY_WITH_TIMEOUT(radio.companionRig()->verbunden(), 5000);

        CatRigApplet feld(&radio);
        // Eine ganze Runde der Abfrageliste, damit das Feld den Stand kennt.
        QTRY_VERIFY_WITH_TIMEOUT(radio.companionRig()->catZustand().rfGain >= 0, 5000);
        QTest::qWait(3000);

        const auto taste = [&](const char* name, const QByteArray& abfrage) {
            auto* knopf = feld.findChild<QPushButton*>(QLatin1String(name));
            QVERIFY(knopf);
            const QByteArray vorher = frage(abfrage);
            const bool stand = knopf->isChecked();
            knopf->click();
            QTRY_VERIFY2_WITH_TIMEOUT(frage(abfrage) != vorher, name, 3000);
            const QByteArray geschaltet = frage(abfrage);
            QTRY_COMPARE_WITH_TIMEOUT(knopf->isChecked(), !stand, 3000);
            knopf->click();
            QTRY_VERIFY2_WITH_TIMEOUT(frage(abfrage) == vorher, name, 3000);
            QTRY_COMPARE_WITH_TIMEOUT(knopf->isChecked(), stand, 3000);
            qInfo().noquote() << "FELD" << name << vorher << "->" << geschaltet << "->" << vorher;
        };
        taste("catRigAtt", "w RA;");
        taste("catRigNb", "w NB;");
        taste("catRigNr", "w NR;");
        taste("catRigPreamp", "w PA;");

        auto* agc = feld.findChild<QComboBox*>(QStringLiteral("catRigAgc"));
        QVERIFY(agc);
        const QByteArray agcVorher = frage("w GC;");
        // Steht das Geraet auf AGC aus (bei uns in CW), nimmt es weder GC1
        // noch GC2 an; gemessen 2026-11-07.
        if (agcVorher != "GC0;") {
            const int agcStand = agc->currentIndex();
            const int agcAnders = agcStand == 1 ? 2 : 1;
            agc->setCurrentIndex(agcAnders);
            emit agc->activated(agcAnders);
            QTRY_VERIFY_WITH_TIMEOUT(frage("w GC;") != agcVorher, 3000);
            const QByteArray agcGeschaltet = frage("w GC;");
            agc->setCurrentIndex(agcStand);
            emit agc->activated(agcStand);
            QTRY_COMPARE_WITH_TIMEOUT(frage("w GC;"), agcVorher, 3000);
            qInfo().noquote() << "FELD AGC" << agcVorher << "->" << agcGeschaltet << "->" << agcVorher;
        }

        auto* rf = feld.findChild<QSlider*>(QStringLiteral("catRigRfGain"));
        QVERIFY(rf);
        const QByteArray meterVorher = frage("w SM0;");
        const QByteArray rfVorher = frage("w RG;");
        rf->setValue(255);
        emit rf->sliderReleased();
        QTRY_COMPARE_WITH_TIMEOUT(frage("w RG;"), QByteArray("RG255;"), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(radio.companionRig()->catZustand().rfGain, 255, 500);
        QCOMPARE(rf->value(), 255);
        qInfo().noquote() << "FELD RF" << rfVorher << "->" << frage("w RG;")
                          << "S-Meter" << meterVorher << "->" << frage("w SM0;");

        // Die Filtertaste und die Bandbreite, in dem Seitenband, in dem das
        // Geraet gerade steht. Am Ende steht beides wie vorher.
        auto* filter = feld.findChild<QPushButton*>(QStringLiteral("catRigFilter"));
        QVERIFY(filter);
        const QByteArray filterVorher = frage("w FL;");
        filter->click();
        QTRY_VERIFY_WITH_TIMEOUT(frage("w FL;") != filterVorher, 3000);
        const QByteArray filterGeschaltet = frage("w FL;");
        QTRY_VERIFY_WITH_TIMEOUT(filter->text() == (filterGeschaltet == "FL1;"
                                     ? QStringLiteral("FIL A") : QStringLiteral("FIL B")), 3000);
        filter->click();
        QTRY_COMPARE_WITH_TIMEOUT(frage("w FL;"), filterVorher, 3000);
        qInfo().noquote() << "FELD FIL" << filterVorher << "->" << filterGeschaltet << "->" << filterVorher;

        const QByteArray modus = frage("w MD;");
        if (modus == "MD1;" || modus == "MD2;") {
            SliceModel scheibe;
            CatRigBruecke bruecke(radio.companionRig(), &scheibe);
            const DSPMode band = modus == "MD1;" ? DSPMode::LSB : DSPMode::USB;
            QTRY_COMPARE_WITH_TIMEOUT(scheibe.dspMode(), band, 5000);
            QTRY_VERIFY_WITH_TIMEOUT(scheibe.filterHigh() != scheibe.filterLow()
                                     && std::abs(scheibe.filterHigh()) + std::abs(scheibe.filterLow()) > 0, 5000);
            QTest::qWait(1000);
            const int vz = band == DSPMode::LSB ? -1 : 1;
            const auto setze = [&](int von, int bis) {
                band == DSPMode::LSB ? scheibe.setFilter(-bis, -von) : scheibe.setFilter(von, bis);
            };
            const QByteArray slVorher = frage("w SL;"), shVorher = frage("w SH;");
            const int hochVorher = std::max(std::abs(scheibe.filterLow()), std::abs(scheibe.filterHigh()));
            const int tiefVorher = std::min(std::abs(scheibe.filterLow()), std::abs(scheibe.filterHigh()));
            setze(300, 2000);
            QTRY_COMPARE_WITH_TIMEOUT(frage("w SL;"), QByteArray("SL04;"), 3000);
            QTRY_COMPARE_WITH_TIMEOUT(frage("w SH;"), QByteArray("SH05;"), 3000);
            setze(300, 2850);      // zwischen zwei Stufen: das Geraet rundet
            QTRY_COMPARE_WITH_TIMEOUT(frage("w SH;"), QByteArray("SH09;"), 3000);
            QTRY_COMPARE_WITH_TIMEOUT(band == DSPMode::LSB ? scheibe.filterLow() : scheibe.filterHigh(),
                                      vz * 2800, 3000);
            setze(tiefVorher, hochVorher);
            QTRY_COMPARE_WITH_TIMEOUT(frage("w SL;"), slVorher, 3000);
            QTRY_COMPARE_WITH_TIMEOUT(frage("w SH;"), shVorher, 3000);
            qInfo().noquote() << "FELD Bandbreite" << modus << slVorher << shVorher
                              << "-> SL04; SH05; -> SH09; -> zurueck";
        }

        if (modus == "MD3;") {
            SliceModel scheibe;
            CatRigBruecke bruecke(radio.companionRig(), &scheibe);
            QTRY_COMPARE_WITH_TIMEOUT(scheibe.dspMode(), DSPMode::CWU, 5000);
            const QByteArray breiteVorher = frage("w FW;");
            QTest::qWait(1500);
            const int mitte = (scheibe.filterLow() + scheibe.filterHigh()) / 2;
            scheibe.setFilter(mitte - 300, mitte + 300);
            QTRY_COMPARE_WITH_TIMEOUT(frage("w FW;"), QByteArray("FW0600;"), 3000);
            scheibe.setFilter(mitte - 240, mitte + 240);   // rundet auf 500
            QTRY_COMPARE_WITH_TIMEOUT(frage("w FW;"), QByteArray("FW0500;"), 3000);
            QTRY_COMPARE_WITH_TIMEOUT(scheibe.filterHigh() - scheibe.filterLow(), 500, 3000);
            leser.write("W " + breiteVorher + " 0\n");
            QTRY_COMPARE_WITH_TIMEOUT(frage("w FW;"), breiteVorher, 3000);
            qInfo().noquote() << "FELD CW" << breiteVorher << "-> FW0600; -> FW0500; -> zurueck";
        }

        QCOMPARE(frage("f"), frequenzVorher);
        QCOMPARE(frage("t"), QByteArray("0"));
    }

    // Nur von Hand, am echten Geraet und ohne Konverter: der 590 steht auf der
    // ZF, Longpath rechnet auf 2 m. Es wird nichts gesendet.
    void trockenlauf_mit_transverter()
    {
        const int port = qEnvironmentVariableIntValue("LONGPATH_CAT_TROCKEN");
        if (port <= 0) { QSKIP("Nur von Hand: LONGPATH_CAT_TROCKEN=<rigctld>"); }
        zielAuf(static_cast<quint16>(port));
        TransverterSetup t;
        t.enabled = true;
        t.offsetHz = 116000000;
        t.save();
        TransverterSetup::setCurrent(t);
        const auto info = CompanionRigController::radioInfoAusEinstellungen();
        QVERIFY(info.has_value());

        QTcpSocket leser;
        leser.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(port));
        QVERIFY(leser.waitForConnected(3000));
        const auto frage = [&leser](const QByteArray& zeile) {
            leser.readAll();
            leser.write(zeile + '\n');
            QByteArray antwort;
            for (int i = 0; i < 40 && !antwort.contains(';') && !antwort.contains('\n'); ++i) {
                leser.waitForReadyRead(50);
                antwort += leser.readAll();
            }
            return QByteArray(antwort).replace('\0', "").trimmed();
        };
        frage("\\set_cache 0");
        leser.write("W MD2; 0\n");
        leser.waitForReadyRead(500);
        QCOMPARE(frage("F 28144000"), QByteArray("RPRT 0"));

        auto* fenster = new MainWindow();   // bewusst nicht abgeraeumt
        fenster->resize(1280, 800);
        fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(fenster, 20000));
        VerbundeneCatRig verbindung(*info);
        RadioModel& radio = *fenster->radioModelForTest();
        radio.setLastRadioInfoForTest(*info);
        radio.injectConnectionForTest(&verbindung);
        if (radio.activeSlice() == nullptr) { radio.addSlice(); }
        radio.onConnectionStateChangedForTest(ConnectionState::Connected);
        CompanionRigController* rig = radio.companionRig();
        QTRY_VERIFY_WITH_TIMEOUT(rig->verbunden(), 10000);

        // Das Geraet gibt den Stand vor: Scheibe auf der ZF, Anzeige auf 2 m.
        QTRY_COMPARE_WITH_TIMEOUT(radio.vfoHz(0, 0), qint64(28144000), 10000);
        QCOMPARE(rig->rfFrequenzHz(), qint64(144144000));
        auto* flaeche = fenster->findChild<SpectrumWidget*>();
        QVERIFY(flaeche);
        QCOMPARE(qint64(flaeche->rfFuerAnzeigeForTest(28144000.0)), qint64(144144000));
        qInfo().noquote() << "TROCKEN Anzeige" << rig->beschreibung();

        // RX ANT und DRV sind aus: das Feld muss es rot sagen.
        auto* feld = fenster->findChild<CatRigApplet*>();
        QVERIFY(feld);
        auto* wege = feld->findChild<QLabel*>(QStringLiteral("catRigWege"));
        QVERIFY(wege);
        QTRY_VERIFY_WITH_TIMEOUT(wege->text().contains(QStringLiteral("off")), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!wege->styleSheet().isEmpty(), 10000);
        qInfo().noquote() << "TROCKEN Warnung" << wege->text() << wege->styleSheet();

        // Ein Klick im Logbuch: es rechnet 144,250 auf die ZF und schickt 28,250.
        RigctldServer server(&radio);
        QVERIFY(server.start(QHostAddress::LocalHost, 45321));
        QTcpSocket logbuch;
        logbuch.connectToHost(QHostAddress::LocalHost, 45321);
        QVERIFY(logbuch.waitForConnected(3000));
        logbuch.write("F 28250000\n");
        QTRY_COMPARE_WITH_TIMEOUT(frage("f"), QByteArray("28250000"), 5000);
        qInfo().noquote() << "TROCKEN Logbuch 144.250 -> Geraet" << frage("f");

        // Eine Eingabe auf 2 m, etwa die Bandtaste: am Geraet muss die ZF landen.
        radio.setVfoHz(0, 0, 144300000);
        QTRY_COMPARE_WITH_TIMEOUT(frage("f"), QByteArray("28300000"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(radio.vfoHz(0, 0), qint64(28300000), 5000);
        qInfo().noquote() << "TROCKEN Eingabe 144.300 -> Geraet" << frage("f");

        QCOMPARE(frage("t"), QByteArray("0"));
        radio.onConnectionStateChangedForTest(ConnectionState::Disconnected);
        radio.injectConnectionForTest(nullptr);
        t.enabled = false;
        t.save();
        TransverterSetup::setCurrent(t);
    }

    // Im Profil CONTEST ist die Appletspalte nicht zu sehen. Das Feld stand
    // dort „sichtbar" und war nicht zu erreichen; gesehen am Geraet.
    void verbunden_geht_das_feld_auch_ohne_spalte_auf()
    {
        RigctldDoppel doppel;
        zielAuf(doppel.port());
        const auto info = CompanionRigController::radioInfoAusEinstellungen();
        QVERIFY(info.has_value());
        // Der gemerkte Zustand sagt schon „sichtbar", wie beim Betreiber.
        stelleEin(QStringLiteral("AppletCatRigVisible"), QStringLiteral("True"));

        auto* fenster = new MainWindow();   // bewusst nicht abgeraeumt
        fenster->resize(1280, 800);
        fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(fenster, 20000));

        auto* feld = fenster->findChild<CatRigApplet*>();
        auto* spalte = fenster->findChild<AppletPanelWidget*>();
        QVERIFY(feld);
        QVERIFY(spalte);
        spalte->hide();
        QVERIFY(!feld->isVisible());

        VerbundeneCatRig verbindung(*info);
        RadioModel& radio = *fenster->radioModelForTest();
        radio.injectConnectionForTest(&verbindung);
        radio.onConnectionStateChangedForTest(ConnectionState::Connected);

        QTRY_VERIFY_WITH_TIMEOUT(feld->isVisible(), 20000);
        QVERIFY2(feld->window() != fenster, "das Feld blieb in der verdeckten Spalte");
        radio.injectConnectionForTest(nullptr);
    }

    // Waehrend die Maske steht, sind die Fenster versteckt und die Spalte
    // gilt als frei. Danach lagen sie wieder ueber dem Feld; gesehen am
    // Geraet im Profil N.
    void das_feld_wartet_auf_die_fenster_hinter_der_maske()
    {
        RigctldDoppel doppel;
        zielAuf(doppel.port());
        const auto info = CompanionRigController::radioInfoAusEinstellungen();
        QVERIFY(info.has_value());
        stelleEin(QStringLiteral("AppletCatRigVisible"), QStringLiteral("True"));

        auto* fenster = new MainWindow();   // bewusst nicht abgeraeumt
        fenster->resize(1280, 800);
        fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(fenster, 20000));

        auto* feld = fenster->findChild<CatRigApplet*>();
        QVERIFY(feld);
        QTRY_VERIFY_WITH_TIMEOUT(feld->isVisible(), 20000);
        QCOMPARE(feld->window(), static_cast<QWidget*>(fenster));

        // Ein schwebendes Fenster genau ueber dem Feld, hinter der Maske.
        QWidget deckel;
        deckel.setGeometry(QRect(feld->mapToGlobal(QPoint(0, 0)), feld->size()));
        deckel.show();
        fenster->rememberFloatingWindowBehindConnectMaskForTest(&deckel);

        VerbundeneCatRig verbindung(*info);
        RadioModel& radio = *fenster->radioModelForTest();
        radio.injectConnectionForTest(&verbindung);
        radio.onConnectionStateChangedForTest(ConnectionState::Connected);
        QTest::qWait(100);
        QCOMPARE(feld->window(), static_cast<QWidget*>(fenster));

        // Die Maske geht nach dem Verbinden von selbst zu.
        QTRY_VERIFY_WITH_TIMEOUT(deckel.isVisible(), 20000);
        QTRY_VERIFY2_WITH_TIMEOUT(feld->window() != fenster,
                                  "das Feld blieb unter dem Fenster liegen", 20000);
        QVERIFY(feld->isVisible());
        // Und es liegt nicht ueber dem Fenster, wenn daneben Platz ist.
        QTRY_VERIFY_WITH_TIMEOUT(
            !feld->window()->frameGeometry().intersects(deckel.frameGeometry()), 20000);
        radio.injectConnectionForTest(nullptr);
    }

    // Ein Klick im Contest-Teil geht ueber den Rig-Server auf die Scheibe.
    // Von dort muss er beim Geraet ankommen.
    void abstimmen_ueber_die_scheibe_erreicht_das_geraet()
    {
        RigctldDoppel doppel;
        zielAuf(doppel.port());
        const auto info = CompanionRigController::radioInfoAusEinstellungen();
        QVERIFY(info.has_value());

        auto* fenster = new MainWindow();   // bewusst nicht abgeraeumt
        fenster->resize(1280, 800);
        fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(fenster, 20000));

        VerbundeneCatRig verbindung(*info);
        RadioModel& radio = *fenster->radioModelForTest();
        radio.setLastRadioInfoForTest(*info);
        radio.injectConnectionForTest(&verbindung);
        if (radio.activeSlice() == nullptr) { radio.addSlice(); }
        radio.onConnectionStateChangedForTest(ConnectionState::Connected);
        QTRY_VERIFY_WITH_TIMEOUT(radio.companionRig()->verbunden(), 20000);
        QTRY_VERIFY_WITH_TIMEOUT(radio.vfoHz(0, 0) == doppel.frequenz, 20000);

        doppel.befehle.clear();
        radio.setVfoHz(0, 0, 28144000);
        QTRY_COMPARE_WITH_TIMEOUT(doppel.frequenz, qint64(28144000), 20000);
        QVERIFY(!doppel.hatGetastet());

        radio.onConnectionStateChangedForTest(ConnectionState::Disconnected);
        radio.injectConnectionForTest(nullptr);
    }

    void freierPlatz_meidet_was_belegt_ist()
    {
        const QRect flaeche(0, 0, 400, 300);
        QCOMPARE(freierPlatz(QSize(100, 100), flaeche, {}), QRect(0, 0, 100, 100));
        QCOMPARE(freierPlatz(QSize(100, 100), flaeche, {QRect(0, 0, 200, 300)}),
                 QRect(208, 0, 100, 100));
        QVERIFY(freierPlatz(QSize(100, 100), flaeche, {flaeche}).isEmpty());
        QVERIFY(freierPlatz(QSize(500, 100), flaeche, {}).isEmpty());
    }

    // Ueber die Pruef-Zugaenge und nicht ueber connectToRadio(): der volle
    // Hochlauf rechnet zuerst die WDSP-Wisdom und braucht Minuten.
    void verbunden_wird_MOX_abgewiesen_und_nichts_getastet()
    {
        RigctldDoppel doppel;
        zielAuf(doppel.port());
        const auto info = CompanionRigController::radioInfoAusEinstellungen();
        QVERIFY(info.has_value());

        RadioModel radio;
        radio.setLastRadioInfoForTest(*info);
        radio.installBandPlanMoxCheckForTest();
        radio.onConnectionStateChangedForTest(ConnectionState::Connected);
        QVERIFY(radio.companionRig()->catBetrieb());
        QTRY_VERIFY_WITH_TIMEOUT(radio.companionRig()->verbunden(), 20000);
        QVERIFY(radio.moxController());

        QSignalSpy abgewiesen(radio.moxController(), &MoxController::moxRejected);
        radio.setMox(true);
        QCOMPARE(abgewiesen.size(), 1);
        QVERIFY2(abgewiesen.first().first().toString().contains(QStringLiteral("TS-590S")),
                 qPrintable(abgewiesen.first().first().toString()));
        QVERIFY(!radio.mox());
        QTest::qWait(100);
        QVERIFY2(!doppel.hatGetastet(), "MOX kam beim fremden Geraet an");
    }
};

QTEST_MAIN(TstCatFunkgeraet)
#include "tst_cat_funkgeraet.moc"
