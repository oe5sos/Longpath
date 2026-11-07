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
#include "core/WdspEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "gui/MainWindow.h"

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
        QVERIFY2(sMeter > sMeterFalsch + 20.0,
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
