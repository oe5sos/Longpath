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
#include <QSignalSpy>
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
#include "core/RxChannel.h"
#include "core/Ts590Cat.h"
#include "core/TransverterSetup.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "core/FFTRouter.h"
#include "gui/MainWindow.h"
#include "gui/SpectrumWidget.h"

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
    QHash<QByteArray, QByteArray> roh{
        {"SM0;", "SM00015;"}, {"RM;", "RM10007;"}, {"AN;", "AN100;"}, {"PA;", "PA10;"},
        {"RA;", "RA0000;"}, {"RG;", "RG200;"}, {"GC;", "GC2;"}, {"NB;", "NB0;"},
        {"NR;", "NR1;"}, {"SL;", "SL02;"}, {"SH;", "SH09;"}, {"PC;", "PC005;"}};

    bool hatGetastet() const
    {
        for (const QByteArray& b : befehle) {
            const QByteArray t = b.trimmed();
            if (t.startsWith("T ") || t.startsWith("\\set_ptt") || t.startsWith("b ")) {
                return true;
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
                if (roh.contains(frage)) { s->write(roh.value(frage) + '\0'); }
                else { s->write("RPRT -1\n"); }
                rohFragen.append(frage);
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
            Ts590Cat::stelleRfGain(128), Ts590Cat::stelleAgc(Ts590Cat::Agc::Schnell),
            Ts590Cat::stelleNb(true), Ts590Cat::stelleNr(true),
            Ts590Cat::stelleLowCut(100), Ts590Cat::stelleHighCut(2800)};
        for (const QByteArray& z : zeilen) {
            QVERIFY2(z.startsWith("L ") || z.startsWith("U "), z.constData());
            QVERIFY2(!z.contains("RFPOWER") && !z.contains("PTT"), z.constData());
        }
        QCOMPARE(Ts590Cat::stelleRfGain(255), QByteArray("L RF 1.000\n"));
        QCOMPARE(Ts590Cat::punkteZuDbm(15), -73.0);
        QCOMPARE(Ts590Cat::naechsterHighCut(2850), 2800);
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
        QTRY_VERIFY_WITH_TIMEOUT(rig.verbunden(), 3000);
        rig.client()->setPollIntervalMs(20);

        QTRY_COMPARE_WITH_TIMEOUT(rig.catZustand().leistungW, 5, 5000);
        QCOMPARE(rig.catZustand().antenne, 2);
        QCOMPARE(rig.catZustand().rxAnt, 0);
        QCOMPARE(rig.catZustand().rfGain, 200);
        QCOMPARE(rig.catZustand().meterPunkte, 15);
        QTRY_VERIFY_WITH_TIMEOUT(!warnung.isEmpty(), 3000);
        QVERIFY(warnung.first().first().toString().contains(QStringLiteral("RX ANT")));
        const int anzahl = warnung.size();

        // Stehen beide richtig, schweigt die Warnung und bleibt still.
        doppel.roh["AN;"] = "AN111;";
        QTRY_COMPARE_WITH_TIMEOUT(rig.catZustand().drive, 1, 5000);
        QCOMPARE(warnung.size(), anzahl);

        // Der Sendezustand kommt vom Geraet, das SWR nur dann.
        QVERIFY(!rig.sendet());
        doppel.gesendet = true;
        QTRY_VERIFY_WITH_TIMEOUT(rig.sendet(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(rig.catZustand().swrPunkte, 7, 3000);

        // Stellen geht ueber die geschlossene Liste.
        rig.stelle(Ts590Cat::stelleDaempfung(true));
        QTRY_VERIFY_WITH_TIMEOUT(doppel.befehle.contains("L ATT 12\n"), 3000);
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
        QTRY_VERIFY_WITH_TIMEOUT(rig.verbunden(), 3000);
        rig.client()->setPollIntervalMs(20);

        SliceModel scheibe;
        CatRigBruecke bruecke(&rig, &scheibe);
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.dspMode(), DSPMode::USB, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.filterHigh(), 2800, 5000);
        QCOMPARE(scheibe.filterLow(), 100);

        // Das Geraet steht in LSB: dieselben Grenzen, unter dem Traeger.
        doppel.modus = QStringLiteral("LSB");
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.dspMode(), DSPMode::LSB, 10000);
        doppel.roh["SH;"] = "SH10;";
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.filterLow(), -3000, 5000);
        QCOMPARE(scheibe.filterHigh(), -100);

        // Von der Scheibe zum Geraet, auf die Tabelle gerundet.
        scheibe.setFilter(-2600, -200);
        QTRY_VERIFY_WITH_TIMEOUT(doppel.befehle.contains("L SLOPE_HIGH 2600\n"), 3000);
        QVERIFY(doppel.befehle.contains("L SLOPE_LOW 200\n"));
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
        QTRY_VERIFY_WITH_TIMEOUT(rig.verbunden(), 3000);
        QCOMPARE(rig.client()->pollIntervalMs(), CompanionRigController::kKatAbfrageMs);
        rig.client()->setPollIntervalMs(20);

        SliceModel scheibe;
        scheibe.setDspMode(DSPMode::LSB);
        CatRigBruecke bruecke(&rig, &scheibe);

        // Das Geraet gibt den Stand vor.
        QTRY_COMPARE_WITH_TIMEOUT(qRound64(scheibe.frequency()), qint64(28078400), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.dspMode(), DSPMode::USB, 10000);

        // Ohne das antwortet rigctld eine Sekunde lang aus dem Speicher.
        QTRY_VERIFY_WITH_TIMEOUT(
            doppel.befehle.contains(QByteArray("\\set_cache 0\n")), 3000);
        for (const QByteArray& b : std::as_const(doppel.befehle)) {
            QVERIFY2(b.startsWith("\\set_cache"),
                     "eine Meldung vom Geraet ging als Befehl zurueck");
        }

        // Die Scheibe stellt das Geraet.
        scheibe.setFrequency(28100000.0);
        QTRY_COMPARE_WITH_TIMEOUT(doppel.frequenz, qint64(28100000), 3000);

        // Eine abweichende Meldung in der Frist zieht den VFO nicht zurueck.
        // Erst warten, bis der Client die neue Zahl kennt: sonst aendert
        // sich aus seiner Sicht nichts, und die Pruefung sagt nichts aus.
        QTRY_COMPARE_WITH_TIMEOUT(rig.zfFrequenzHz(), qint64(28100000), 3000);
        doppel.frequenz = 28078400;
        QTRY_COMPARE_WITH_TIMEOUT(rig.zfFrequenzHz(), qint64(28078400), 3000);
        QCOMPARE(qRound64(scheibe.frequency()), qint64(28100000));

        // Nach der Frist gilt das Geraet wieder.
        bruecke.fristAufhebenForTest();
        doppel.frequenz = 28200000;
        QTRY_COMPARE_WITH_TIMEOUT(qRound64(scheibe.frequency()), qint64(28200000), 3000);

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
        QTRY_VERIFY_WITH_TIMEOUT(radio.companionRig()->verbunden(), 5000);
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
        QTRY_VERIFY_WITH_TIMEOUT(radio.companionRig()->verbunden(), 5000);
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
