// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath fragt ein fremdes Funkgeraet, wo es steht -- und tastet es
// niemals.
//
// Zwei Dinge belegt dieser Lauf, die man nicht am Quelltext sieht:
//
//  1. Eine EINZEILIGE Antwort auf die zweizeilige Modusabfrage laesst
//     den Client nicht haengen. Genau dort bliebe die Vorlage im
//     Contestprogramm stehen (ihr Kopf sagt es selbst: „that would
//     stall until the reply watchdog cuts the connection") -- vier
//     Sekunden Stillstand bei jedem Geraet, das `m` nicht beherrscht.
//
//  2. Dieser Client schickt NIE einen Tastbefehl. Der gefaelschte
//     rigctld unten schreibt jeden empfangenen Befehl mit, und der
//     Pruefstand sieht die Liste durch. Ein `T 1`, das sich einmal
//     einschleicht, tastet einen fremden Sender an einer fremden
//     Endstufe -- das soll nicht erst am Berg auffallen.

#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>

#include "core/RigctldClient.h"

using namespace Longpath;

namespace {

// Ein rigctld, das nur das Noetigste kann -- und auf Wunsch falsch
// antwortet.
class GefaelschtesRigctld : public QObject
{
    Q_OBJECT
public:
    explicit GefaelschtesRigctld(QObject* parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            m_client = m_server.nextPendingConnection();
            connect(m_client, &QTcpSocket::readyRead, this, [this]() {
                m_eingang += m_client->readAll();
                while (true) {
                    const int nl = m_eingang.indexOf('\n');
                    if (nl < 0) { break; }
                    const QByteArray befehl = m_eingang.left(nl).trimmed();
                    m_eingang.remove(0, nl + 1);
                    befehle << QString::fromLatin1(befehl);
                    antworte(befehl);
                }
            });
        });
    }

    bool starte()
    {
        return m_server.listen(QHostAddress::LocalHost, 0);
    }
    quint16 port() const { return m_server.serverPort(); }

    // Was der Client geschickt hat, in der Reihenfolge.
    QStringList befehle;

    qint64  frequenz{145300000};
    bool    getastet{false};
    // true: auf `m` einzeilig mit RPRT antworten -- ein Geraet, das die
    // Modusabfrage nicht beherrscht.
    bool    modusVerweigern{false};

private:
    void antworte(const QByteArray& befehl)
    {
        if (!m_client) { return; }
        if (befehl == QByteArrayLiteral("f")) {
            m_client->write(QByteArray::number(frequenz) + '\n');
        } else if (befehl == QByteArrayLiteral("m")) {
            if (modusVerweigern) {
                m_client->write(QByteArrayLiteral("RPRT -11\n"));
            } else {
                m_client->write(QByteArrayLiteral("USB\n2400\n"));
            }
        } else if (befehl == QByteArrayLiteral("t")) {
            m_client->write(getastet ? QByteArrayLiteral("1\n")
                                     : QByteArrayLiteral("0\n"));
        } else if (befehl.startsWith(QByteArrayLiteral("F "))) {
            frequenz = befehl.mid(2).trimmed().toLongLong();
            m_client->write(QByteArrayLiteral("RPRT 0\n"));
        } else {
            m_client->write(QByteArrayLiteral("RPRT -1\n"));
        }
        m_client->flush();
    }

    QTcpServer  m_server;
    QTcpSocket* m_client{nullptr};
    QByteArray  m_eingang;
};

} // namespace

class TstRigctldClient : public QObject
{
    Q_OBJECT

private slots:
    // ── Die Parser, ohne Netz ───────────────────────────────────────

    void frequenzLesen()
    {
        qint64 hz = 0;
        QVERIFY(RigctldClient::parseFrequency(
            QByteArrayLiteral("145300000\n"), hz));
        QCOMPARE(hz, 145300000);

        // Der Fall, der nie als Frequenz durchgehen darf.
        hz = 0;
        QVERIFY(!RigctldClient::parseFrequency(
            QByteArrayLiteral("RPRT -1\n"), hz));
        QCOMPARE(hz, 0);

        QVERIFY(!RigctldClient::parseFrequency(QByteArrayLiteral("\n"), hz));
        QVERIFY(!RigctldClient::parseFrequency(QByteArrayLiteral("abc\n"), hz));
        QVERIFY(!RigctldClient::parseFrequency(QByteArrayLiteral("0\n"), hz));
        QVERIFY(!RigctldClient::parseFrequency(QByteArray(), hz));
    }

    void modusLesen()
    {
        QString m;
        int breite = -1;
        QVERIFY(RigctldClient::parseMode(
            QByteArrayLiteral("USB\n2400\n"), m, breite));
        QCOMPARE(m, QStringLiteral("USB"));
        QCOMPARE(breite, 2400);

        // Nur eine Zeile: der Modus gilt, die Breite bleibt unbekannt.
        m.clear(); breite = -1;
        QVERIFY(RigctldClient::parseMode(QByteArrayLiteral("LSB\n"), m, breite));
        QCOMPARE(m, QStringLiteral("LSB"));
        QCOMPARE(breite, 0);

        QVERIFY(!RigctldClient::parseMode(
            QByteArrayLiteral("RPRT -11\n"), m, breite));
        // Eine Zahl ist kein Modus -- hier waeren die Antworten vertauscht.
        QVERIFY(!RigctldClient::parseMode(
            QByteArrayLiteral("2400\nUSB\n"), m, breite));
        QVERIFY(!RigctldClient::parseMode(QByteArray(), m, breite));
    }

    void pttLesen()
    {
        bool getastet = true;
        QVERIFY(RigctldClient::parsePtt(QByteArrayLiteral("0\n"), getastet));
        QCOMPARE(getastet, false);

        QVERIFY(RigctldClient::parsePtt(QByteArrayLiteral("1\n"), getastet));
        QCOMPARE(getastet, true);

        QVERIFY(!RigctldClient::parsePtt(QByteArrayLiteral("RPRT 0\n"),
                                         getastet));
        QVERIFY(!RigctldClient::parsePtt(QByteArrayLiteral("huh\n"), getastet));
    }

    void quittungLesen()
    {
        int code = 99;
        QVERIFY(RigctldClient::parseReport(QByteArrayLiteral("RPRT 0\n"), code));
        QCOMPARE(code, 0);

        QVERIFY(RigctldClient::parseReport(
            QByteArrayLiteral("RPRT -11\n"), code));
        QCOMPARE(code, -11);

        QVERIFY(!RigctldClient::parseReport(
            QByteArrayLiteral("145300000\n"), code));
        QVERIFY(!RigctldClient::parseReport(QByteArrayLiteral("RPRT x\n"),
                                            code));
    }

    void fehlerNummernHabenWorte()
    {
        QCOMPARE(RigctldClient::describeReport(0), QStringLiteral("OK"));
        QVERIFY(RigctldClient::describeReport(-11).contains(
            QStringLiteral("cannot be asked")));
        QVERIFY(RigctldClient::describeReport(-6).contains(
            QStringLiteral("cable")));
        // Eine unbekannte Nummer verschwindet nicht, sie wird genannt.
        QVERIFY(RigctldClient::describeReport(-99).contains(
            QStringLiteral("-99")));
    }

    void abstimmbefehlOhneTrennzeichen()
    {
        QCOMPARE(RigctldClient::setFrequencyCommand(145300000),
                 QByteArrayLiteral("F 145300000\n"));
        QCOMPARE(RigctldClient::setFrequencyCommand(28300000),
                 QByteArrayLiteral("F 28300000\n"));
    }

    // ── Gegen einen gefaelschten rigctld ────────────────────────────

    void dieFrequenzKommtAn()
    {
        GefaelschtesRigctld server;
        QVERIFY(server.starte());

        RigctldClient c;
        c.setPollIntervalMs(50);
        c.setTarget(QStringLiteral("127.0.0.1"), server.port());

        QSignalSpy freq(&c, &RigctldClient::frequencyChanged);
        c.connectToRig();

        QTRY_VERIFY_WITH_TIMEOUT(c.isConnected(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!freq.isEmpty(), 3000);
        QCOMPARE(c.frequencyHz(), 145300000);
        QVERIFY(c.hasFreshFrequency());

        // Dreht der Betreiber am Knopf, folgt die Anzeige.
        server.frequenz = 145212000;
        QTRY_COMPARE_WITH_TIMEOUT(c.frequencyHz(), 145212000, 3000);
    }

    void modusUndPttKommenAuchAn()
    {
        GefaelschtesRigctld server;
        QVERIFY(server.starte());
        server.getastet = true;

        RigctldClient c;
        c.setPollIntervalMs(30);
        c.setTarget(QStringLiteral("127.0.0.1"), server.port());
        c.connectToRig();

        QTRY_VERIFY_WITH_TIMEOUT(c.isConnected(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(c.mode(), QStringLiteral("USB"), 3000);
        QCOMPARE(c.passbandHz(), 2400);
        QTRY_VERIFY_WITH_TIMEOUT(c.ptt(), 3000);
    }

    // Der Fall, an dem die Vorlage haengen bliebe.
    void einzeiligeAntwortAufDieModusabfrageHaengtNicht()
    {
        GefaelschtesRigctld server;
        QVERIFY(server.starte());
        server.modusVerweigern = true;

        RigctldClient c;
        c.setPollIntervalMs(30);
        c.setTarget(QStringLiteral("127.0.0.1"), server.port());

        QSignalSpy wache(&c, &RigctldClient::replyTimedOut);
        c.connectToRig();
        QTRY_VERIFY_WITH_TIMEOUT(c.isConnected(), 3000);

        // Warten, bis die Modusabfrage sicher durch ist (jeder achte
        // Takt, also spaetestens nach acht Takten a 30 ms).
        QTRY_VERIFY_WITH_TIMEOUT(
            server.befehle.contains(QStringLiteral("m")), 3000);

        // Und jetzt der Beleg: es geht weiter. Die Frequenz aendert
        // sich noch, die Antwortwache hat nicht angeschlagen, die
        // Verbindung steht.
        server.frequenz = 144300000;
        QTRY_COMPARE_WITH_TIMEOUT(c.frequencyHz(), 144300000, 3000);
        QVERIFY(c.isConnected());
        QCOMPARE(wache.count(), 0);
    }

    void abstimmenGehtZumGeraet()
    {
        GefaelschtesRigctld server;
        QVERIFY(server.starte());

        RigctldClient c;
        c.setPollIntervalMs(200);
        c.setTarget(QStringLiteral("127.0.0.1"), server.port());
        c.connectToRig();
        QTRY_VERIFY_WITH_TIMEOUT(c.isConnected(), 3000);

        c.setFrequencyHz(144325000);
        QTRY_COMPARE_WITH_TIMEOUT(server.frequenz, 144325000, 3000);

        // Ohne Verbindung wird nichts geschickt und nichts geworfen.
        c.disconnectFromRig();
        const int vorher = server.befehle.size();
        c.setFrequencyHz(144400000);
        QTest::qWait(100);
        QCOMPARE(server.befehle.size(), vorher);
    }

    // Die Sicherheitszusage aus dem Kopf von RigctldClient.h.
    void niemalsEinTastbefehl()
    {
        GefaelschtesRigctld server;
        QVERIFY(server.starte());
        server.getastet = true;

        RigctldClient c;
        c.setPollIntervalMs(20);
        c.setTarget(QStringLiteral("127.0.0.1"), server.port());
        c.connectToRig();
        QTRY_VERIFY_WITH_TIMEOUT(c.isConnected(), 3000);

        // Lange genug laufen lassen, dass jede Abfrage mehrfach
        // durchgelaufen ist, und zwischendurch abstimmen.
        QTRY_VERIFY_WITH_TIMEOUT(
            server.befehle.count(QStringLiteral("t")) >= 2, 3000);
        c.setFrequencyHz(144300000);
        QTest::qWait(200);

        QVERIFY(!server.befehle.isEmpty());
        for (const QString& b : server.befehle) {
            QVERIFY2(b != QStringLiteral("T 1"), qPrintable(b));
            QVERIFY2(b != QStringLiteral("T 0"), qPrintable(b));
            QVERIFY2(!b.startsWith(QStringLiteral("T ")), qPrintable(b));
        }
    }

    void trennenHaeltAuchStill()
    {
        GefaelschtesRigctld server;
        QVERIFY(server.starte());

        RigctldClient c;
        c.setPollIntervalMs(20);
        c.setTarget(QStringLiteral("127.0.0.1"), server.port());
        c.connectToRig();
        QTRY_VERIFY_WITH_TIMEOUT(c.isConnected(), 3000);

        c.disconnectFromRig();
        QCOMPARE(c.state(), RigctldClient::State::Disconnected);
        QVERIFY(!c.hasFreshFrequency());

        // Kein Wiederanlauf hinter dem Ruecken: wer getrennt hat, hat
        // getrennt.
        const int vorher = server.befehle.size();
        QTest::qWait(300);
        QCOMPARE(server.befehle.size(), vorher);
    }

    void ohneAdresseWirdNichtVersucht()
    {
        RigctldClient c;
        QSignalSpy fehler(&c, &RigctldClient::errorOccurred);
        c.connectToRig();
        QCOMPARE(c.state(), RigctldClient::State::Disconnected);
        QCOMPARE(fehler.count(), 1);
    }
};

QTEST_MAIN(TstRigctldClient)
#include "tst_rigctld_client.moc"
