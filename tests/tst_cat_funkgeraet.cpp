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
#include "models/RadioModel.h"
#include "models/SliceModel.h"

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

    bool hatGetastet() const
    {
        for (const QByteArray& b : befehle) {
            const QByteArray t = b.trimmed();
            if (t.startsWith("T ") || t.startsWith("\\set_ptt") || t.startsWith("b ")) {
                return true;
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
                s->write("0\n");
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

// Energie des komplexen Signals bei `hz`; negativ heisst unter dem Traeger.
double energieBei(const QVector<float>& iq, double hz, int ab)
{
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
        rig.client()->setPollIntervalMs(20);

        SliceModel scheibe;
        scheibe.setDspMode(DSPMode::LSB);
        CatRigBruecke bruecke(&rig, &scheibe);

        // Das Geraet gibt den Stand vor.
        QTRY_COMPARE_WITH_TIMEOUT(qRound64(scheibe.frequency()), qint64(28078400), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.dspMode(), DSPMode::USB, 3000);
        QVERIFY2(doppel.befehle.isEmpty(), "eine Meldung vom Geraet ging als Befehl zurueck");

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
        QTRY_COMPARE_WITH_TIMEOUT(doppel.modus, QStringLiteral("LSB"), 3000);
        bruecke.fristAufhebenForTest();
        doppel.modus = QStringLiteral("CW");
        QTRY_COMPARE_WITH_TIMEOUT(scheibe.dspMode(), DSPMode::CWU, 3000);

        QVERIFY2(!doppel.hatGetastet(), "Longpath hat das fremde Geraet getastet");
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
