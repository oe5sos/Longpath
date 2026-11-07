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

#include "core/AppSettings.h"
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
