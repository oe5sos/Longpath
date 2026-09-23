// no-port-check: Longpath-original unit-test file.
// =================================================================
// tests/tst_sunsdr_duplicate_blocks.cpp  (Longpath)
// =================================================================
//
// Die SunSDR2 QRP schickt JEDEN Datenblock achtmal.
//
// Am Geraet gemessen (2026-09-23, 30 000 Pakete in 15,46 s ueber einen
// eigenen Ethernet-Adapter mitgeschnitten): 29 683 IQ-Pakete, darin nur
// 3 718 verschiedene Nutzlasten -- 3 703 davon exakt achtmal. Die acht
// Kopien liegen ueber rund 32 ms verteilt (11,6 / 8,0 / 4,0 / 3,0 /
// 2,0 / 2,0 / 2,0 ms Abstand) und sind mit den Nachbarbloecken
// verschraenkt. Neue Bloecke kommen alle 4,17 ms, also 240 je Sekunde;
// mal 200 Probenpaare sind das 48 000 Proben je Sekunde -- und nicht
// die 1920 Pakete/s, die auf dem Draht liegen.
//
// Bis zum 2026-09-23 reichte der Treiber alle acht Kopien an die
// Signalverarbeitung weiter. Die Diagnosezeile im Treiber, die
// "byte-identical to the one before them" zaehlt, meldete dabei immer
// 0 -- weil die Kopien eben NICHT hintereinander kommen.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/SunSdrRadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/sunsdr/SunSdrProtocol.h"

#include <QHostAddress>

using namespace Longpath;

namespace {

// Ein echter 1210-Byte-Rahmen mit erkennbarem Inhalt.
QByteArray iqPacket(quint16 seq, char fill)
{
    QByteArray pkt = SunSdr::buildIqHeader(
        SunSdr::kProfileQrp, SunSdr::kOpIqRxIdle, seq, /*byte8=*/0, /*byte9=*/0);
    pkt.append(SunSdr::kIqPayloadSize, fill);
    return pkt;
}

RadioInfo qrpInfo()
{
    RadioInfo info;
    info.address    = QHostAddress(QStringLiteral("192.0.2.1"));  // RFC 5737
    info.port       = 50001;
    info.boardType  = HPSDRHW::SunSdr2Qrp;
    info.protocol   = ProtocolVersion::SunSdr;
    info.macAddress = QStringLiteral("00:00:00:00:00:00");
    info.name       = QStringLiteral("QRP");
    return info;
}

} // namespace

class TstSunSdrDuplicateBlocks : public QObject { Q_OBJECT
private slots:

    void eightCopiesOfABlockYieldOneFrame()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(qrpInfo());   // setzt das Profil
        conn.setRxReadyForTest(true);
        QSignalSpy iq(&conn, &RadioConnection::iqDataReceived);

        // Wie am Geraet: acht Kopien desselben Blocks.
        for (int i = 0; i < 8; ++i) {
            conn.feedStreamDatagramForTest(iqPacket(0x1234, '\x11'));
        }
        QCOMPARE(iq.count(), 1);
        QCOMPARE(conn.duplicateBlocksDroppedForTest(), quint64(7));
    }

    void interleavedCopiesAreCaughtToo()
    {
        // Der eigentliche Fall: die Kopien liegen NICHT hintereinander,
        // sondern zwischen den Nachbarbloecken. Genau so sieht es im
        // Mitschnitt aus (e36f, e370, e372, e36d, e36e, e375, e36f, ...).
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(qrpInfo());   // setzt das Profil
        conn.setRxReadyForTest(true);
        QSignalSpy iq(&conn, &RadioConnection::iqDataReceived);

        const quint16 order[] = {0xe36f, 0xe370, 0xe372, 0xe36d, 0xe36e,
                                 0xe375, 0xe36f, 0xe371, 0xe36e, 0xe370,
                                 0xe36f, 0xe373, 0xe36e, 0xe376, 0xe36f};
        for (const quint16 s : order) {
            conn.feedStreamDatagramForTest(iqPacket(s, '\x22'));
        }
        // Verschiedene Nummern in dieser Folge: 6d 6e 6f 70 71 72 73 75 76 = 9
        QCOMPARE(iq.count(), 9);
        QCOMPARE(conn.duplicateBlocksDroppedForTest(), quint64(6));
    }

    void aFreshSessionForgetsTheOldSequences()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(qrpInfo());   // setzt das Profil
        conn.setRxReadyForTest(true);
        QSignalSpy iq(&conn, &RadioConnection::iqDataReceived);

        conn.feedStreamDatagramForTest(iqPacket(0x0001, '\x33'));
        QCOMPARE(iq.count(), 1);

        conn.disconnect();
        conn.setRxReadyForTest(true);

        // Dieselbe Nummer in der neuen Sitzung ist ein ECHTER Block.
        conn.feedStreamDatagramForTest(iqPacket(0x0001, '\x44'));
        QCOMPARE(iq.count(), 2);
        QCOMPARE(conn.duplicateBlocksDroppedForTest(), quint64(0));
    }

    void thirtyTwoDistinctBlocksStillPassInOrder()
    {
        // Der Ring hat 32 Plaetze. Mehr verschiedene Bloecke hintereinander
        // duerfen trotzdem alle durchkommen -- die Wache wirft nur
        // Wiederholungen weg, nicht neue Nummern.
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(qrpInfo());   // setzt das Profil
        conn.setRxReadyForTest(true);
        QSignalSpy iq(&conn, &RadioConnection::iqDataReceived);
        for (int i = 0; i < 100; ++i) {
            conn.feedStreamDatagramForTest(iqPacket(static_cast<quint16>(i), '\x55'));
        }
        QCOMPARE(iq.count(), 100);
        QCOMPARE(conn.duplicateBlocksDroppedForTest(), quint64(0));
    }
};

QTEST_MAIN(TstSunSdrDuplicateBlocks)
#include "tst_sunsdr_duplicate_blocks.moc"
