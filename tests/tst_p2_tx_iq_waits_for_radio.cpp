// no-port-check: Longpath-original test. Cites Thetis only for the expected
// behaviour (when TX I/Q packets start); no Thetis logic is ported here.
//
// Der TX-I/Q-Takt (Port 1029) darf erst laufen, wenn das Geraet sich in
// dieser Sitzung gemeldet hat (2026-09-30).
//
// Vorher lief er ab SendStart: 4 x 1444 Byte alle 5 ms an ein Geraet, das
// noch nicht laeuft. Solange macOS die MAC-Adresse des Geraets per ARP
// aufloest, haelt es gezielte Pakete in einer Warteschlange von 16 und
// verwirft bei Ueberlauf die aeltesten -- General, Rx, Tx und
// HighPriority(run=1) fielen vorne heraus, das Geraet startete nie
// (erster Connect zur ANVELINA PRO 3 am 29.09., 2/2). Thetis sendet 1029
// erst, wenn Mikrofonrahmen des Geraets eintreffen (ChannelMaster
// network.c:772 -> cmaster.c:397 -> obbuffs.c:169 -> network.c:1388
// [@852bf0e]).
//
// Pruefaufbau: ein Schein-Geraet auf 127.0.0.1 hoert auf 1024 (General)
// und 1029 (TX I/Q) und antwortet von 1025 (High-Priority-Status).
#include <QtTest/QtTest>
#include <QHostAddress>
#include <QNetworkDatagram>
#include <QUdpSocket>

#include "core/HpsdrModel.h"
#include "core/P2RadioConnection.h"
#include "core/RadioDiscovery.h"

using namespace Longpath;

class TestP2TxIqWaitsForRadio : public QObject {
    Q_OBJECT

private:
    static RadioInfo loopbackRadio()
    {
        RadioInfo info;
        info.address         = QHostAddress(QHostAddress::LocalHost);
        info.port            = 1024;
        info.boardType       = HPSDRHW::OrionMKII;
        info.protocol        = ProtocolVersion::Protocol2;
        info.macAddress      = QStringLiteral("00:00:00:00:00:00");
        info.firmwareVersion = 22;
        info.name            = QStringLiteral("Loopback");
        return info;
    }

    static int drain(QUdpSocket& socket)
    {
        int n = 0;
        while (socket.hasPendingDatagrams()) {
            socket.receiveDatagram();
            ++n;
        }
        return n;
    }

private slots:
    void txIqStartsOnlyAfterFirstFrameFromRadio()
    {
        QUdpSocket general;
        QUdpSocket status;
        QUdpSocket txIq;
        if (!general.bind(QHostAddress::LocalHost, 1024)
            || !status.bind(QHostAddress::LocalHost, 1025)
            || !txIq.bind(QHostAddress::LocalHost, 1029)) {
            QSKIP("127.0.0.1:1024/1025/1029 belegt -- Schein-Geraet nicht moeglich");
        }

        P2RadioConnection conn;
        conn.init();
        conn.connectToRadio(loopbackRadio());

        // SendStart ist draussen: das General-Paket kommt an 1024 an und
        // verraet den Port, auf dem die Verbindung hoert.
        QTRY_VERIFY_WITH_TIMEOUT(general.hasPendingDatagrams(), 1000);
        const quint16 ourPort = general.receiveDatagram().senderPort();
        QVERIFY(ourPort != 0);

        // 200 ms sind 40 Takte des 5-ms-Timers. Ohne Rahmen vom Geraet
        // darf an 1029 nichts ankommen.
        QTest::qWait(200);
        QCOMPARE(drain(txIq), 0);

        // Das Geraet meldet sich mit einem Status von 1025 ...
        status.writeDatagram(QByteArray(60, '\0'),
                             QHostAddress(QHostAddress::LocalHost), ourPort);

        // ... ab jetzt laeuft der TX-I/Q-Takt wie bisher.
        QTRY_VERIFY_WITH_TIMEOUT(txIq.hasPendingDatagrams(), 1000);

        conn.disconnect();
    }
};

QTEST_MAIN(TestP2TxIqWaitsForRadio)
#include "tst_p2_tx_iq_waits_for_radio.moc"
