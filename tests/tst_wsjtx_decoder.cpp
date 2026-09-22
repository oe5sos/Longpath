// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: This test contains fabricated WSJT-X message-text
// fixture strings (CQ / directed / RR73 forms with stand-in callsigns)
// that pre-commit's check-new-ports.py may flag. They are test
// fixtures, not ported callsigns. Precedent: B2 (commit f43582e),
// B3 (commit 9aa4202).
//
// Longpath - WsjtxClient binary protocol parser tests
//
// Phase 3J-2 Task B4. Pins the contract that WsjtxClient parses
// big-endian QDataStream Status (type 1) + Decode (type 2) messages
// per the WSJT-X UDP protocol, dispatches the right callsign from
// each WSJT-X message family, and computes spot frequency as
// dial + delta.

#include <QtTest>
#include <QDataStream>
#include <QSignalSpy>

#include "core/WsjtxClient.h"
#include "core/DxSpot.h"

using namespace Longpath;

// Write a QString in the WSJT-X wire format (the format
// WsjtxClient::readQString() expects): big-endian quint32 length
// followed by raw UTF-8 bytes. Qt's QDataStream operator<<(QString)
// emits UTF-16 with a different framing, so test packets must be
// hand-rolled to match the parser. See WsjtxClient.cpp readQString
// for the read side.
static void writeWsjtxString(QDataStream& ds, const QString& s) {
    QByteArray utf8 = s.toUtf8();
    ds << quint32(utf8.size());
    ds.writeRawData(utf8.constData(), utf8.size());
}

// A Status (type 1) datagram from instance `id` with `dialHz`.
static QByteArray statusPacket(const QString& id, quint64 dialHz,
                               const QString& mode = QStringLiteral("FT8")) {
    QByteArray pkt;
    QDataStream out(&pkt, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);
    out << quint32(0xADBCCBDA) << quint32(2) << quint32(1);
    writeWsjtxString(out, id);
    out << dialHz;
    writeWsjtxString(out, mode);
    return pkt;
}

// A Decode (type 2) datagram from instance `id` at audio offset `deltaHz`.
static QByteArray decodePacket(const QString& id, quint32 deltaHz,
                               const QString& message) {
    QByteArray pkt;
    QDataStream out(&pkt, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);
    out << quint32(0xADBCCBDA) << quint32(2) << quint32(2);
    writeWsjtxString(out, id);
    out << quint8(1);                        // isNew
    out << quint32(61680000) << qint32(-7) << double(0.1) << deltaHz;
    writeWsjtxString(out, "~");
    writeWsjtxString(out, message);
    out << quint8(0) << quint8(0);           // lowConfidence, offAir
    return pkt;
}

// A Close (type 6) datagram from instance `id`.
static QByteArray closePacket(const QString& id) {
    QByteArray pkt;
    QDataStream out(&pkt, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);
    out << quint32(0xADBCCBDA) << quint32(2) << quint32(6);
    writeWsjtxString(out, id);
    return pkt;
}

class TestWsjtxDecoder : public QObject {
    Q_OBJECT
private slots:
    void parsesStatusUpdatesDialFreq();
    void parsesDecodeEmitsSpot();
    void skipsNonNewDecodes();
    void skipsLowConfidence();
    void extractsCallsign_data();
    void extractsCallsign();
    // AetherSDR #3595 [@ae15dd7e]: the dial is per instance.
    void twoInstancesKeepTheirOwnBand();
    void decodeWithoutStatusIsDropped();
    void closeForgetsTheInstance();
    void zeroDialIsIgnored();
};

void TestWsjtxDecoder::parsesStatusUpdatesDialFreq() {
    QByteArray pkt;
    QDataStream out(&pkt, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);
    out << quint32(0xADBCCBDA);
    out << quint32(2);
    out << quint32(1);                       // type = Status
    writeWsjtxString(out, "WSJT-X");         // id
    out << quint64(14074000);                // dialFreqHz
    writeWsjtxString(out, "FT8");            // mode
    // Status has more fields after mode in upstream WSJT-X but the
    // first 3 (id/freq/mode) are what AetherSDR's parser reads.

    WsjtxClient c;
    QSignalSpy statusSpy(&c, &WsjtxClient::statusReceived);
    c.processPacketForTest(pkt);
    QCOMPARE(statusSpy.count(), 1);
    auto args = statusSpy.first();
    QCOMPARE(args[0].toString(), QStringLiteral("WSJT-X"));
    QCOMPARE(args[1].toDouble(), 14074000.0);
    QCOMPARE(args[2].toString(), QStringLiteral("FT8"));
}

void TestWsjtxDecoder::parsesDecodeEmitsSpot() {
    QByteArray pkt;
    QDataStream out(&pkt, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);
    out << quint32(0xADBCCBDA);
    out << quint32(2);
    out << quint32(2);                       // type = Decode
    writeWsjtxString(out, "WSJT-X");
    out << quint8(1);                        // isNew = true (quint8)
    out << quint32(61680000);                // timeMs (17:08:00 UTC)
    out << qint32(5);                        // snr
    out << double(0.4);                      // deltaTime
    out << quint32(1320);                    // deltaFreqHz
    writeWsjtxString(out, "~");              // mode (FT8 in JT proto)
    writeWsjtxString(out, "CQ JA1MZK PM95");
    out << quint8(0);                        // lowConfidence
    out << quint8(0);                        // offAir

    WsjtxClient c;
    c.setDialFreqForTest(14074000.0, QStringLiteral("FT8"));
    QSignalSpy spotSpy(&c, &WsjtxClient::spotReceived);
    c.processPacketForTest(pkt);

    QCOMPARE(spotSpy.count(), 1);
    auto spot = spotSpy.first().first().value<DxSpot>();
    QCOMPARE(spot.dxCall, QStringLiteral("JA1MZK"));
    QCOMPARE(spot.freqMhz, 14.075320);  // (14074000 + 1320) / 1e6
    QCOMPARE(spot.snr, 5);
    QCOMPARE(spot.source, QStringLiteral("WSJT-X"));
}

void TestWsjtxDecoder::skipsNonNewDecodes() {
    QByteArray pkt;
    QDataStream out(&pkt, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);
    out << quint32(0xADBCCBDA);
    out << quint32(2);
    out << quint32(2);
    writeWsjtxString(out, "WSJT-X");
    out << quint8(0);                        // isNew = FALSE -> skip
    out << quint32(0);
    out << qint32(0);
    out << double(0);
    out << quint32(0);
    writeWsjtxString(out, "~");
    writeWsjtxString(out, "CQ JA1MZK PM95");
    out << quint8(0);
    out << quint8(0);

    WsjtxClient c;
    c.setDialFreqForTest(14074000.0, QStringLiteral("FT8"));
    QSignalSpy spotSpy(&c, &WsjtxClient::spotReceived);
    c.processPacketForTest(pkt);
    QCOMPARE(spotSpy.count(), 0);  // skipped
}

void TestWsjtxDecoder::skipsLowConfidence() {
    QByteArray pkt;
    QDataStream out(&pkt, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);
    out << quint32(0xADBCCBDA);
    out << quint32(2);
    out << quint32(2);
    writeWsjtxString(out, "WSJT-X");
    out << quint8(1);                        // isNew = true
    out << quint32(0);
    out << qint32(0);
    out << double(0);
    out << quint32(0);
    writeWsjtxString(out, "~");
    writeWsjtxString(out, "CQ JA1MZK PM95");
    out << quint8(1);                        // lowConfidence = TRUE -> skip
    out << quint8(0);

    WsjtxClient c;
    c.setDialFreqForTest(14074000.0, QStringLiteral("FT8"));
    QSignalSpy spotSpy(&c, &WsjtxClient::spotReceived);
    c.processPacketForTest(pkt);
    QCOMPARE(spotSpy.count(), 0);
}

// Two WSJT-X instances on one port, 40 m and 20 m, their Status
// datagrams interleaved: each decode lands on ITS instance's dial.
// With one "last dial seen" the 40 m decode (offset 1320 Hz) that
// arrived after the 20 m Status was painted at 14.075 MHz.
void TestWsjtxDecoder::twoInstancesKeepTheirOwnBand() {
    WsjtxClient c;
    QSignalSpy spotSpy(&c, &WsjtxClient::spotReceived);
    c.processPacketForTest(statusPacket(QStringLiteral("WSJT-X"), 7074000));
    c.processPacketForTest(statusPacket(QStringLiteral("WSJT-X - 2"), 14074000));
    QCOMPARE(c.dialTrackerForTest().instanceCount(), 2);

    c.processPacketForTest(decodePacket(QStringLiteral("WSJT-X"), 1320,
                                        QStringLiteral("CQ OE5SOS JN67")));
    c.processPacketForTest(decodePacket(QStringLiteral("WSJT-X - 2"), 2000,
                                        QStringLiteral("CQ JA1MZK PM95")));
    // The 40 m instance reports again AFTER the 20 m one: still its own band.
    c.processPacketForTest(statusPacket(QStringLiteral("WSJT-X - 2"), 14074000));
    c.processPacketForTest(decodePacket(QStringLiteral("WSJT-X"), 500,
                                        QStringLiteral("CQ DL1ABC JO31")));

    QCOMPARE(spotSpy.count(), 3);
    QCOMPARE(spotSpy.at(0).first().value<DxSpot>().freqMhz, 7.075320);
    QCOMPARE(spotSpy.at(1).first().value<DxSpot>().freqMhz, 14.076000);
    QCOMPARE(spotSpy.at(2).first().value<DxSpot>().freqMhz, 7.074500);
}

// A decode from an instance that has not reported its dial yet cannot be
// placed; it is dropped, not painted on another instance's band.
void TestWsjtxDecoder::decodeWithoutStatusIsDropped() {
    WsjtxClient c;
    QSignalSpy spotSpy(&c, &WsjtxClient::spotReceived);
    c.processPacketForTest(statusPacket(QStringLiteral("WSJT-X"), 7074000));
    c.processPacketForTest(decodePacket(QStringLiteral("WSJT-X - 2"), 1000,
                                        QStringLiteral("CQ JA1MZK PM95")));
    QCOMPARE(spotSpy.count(), 0);
    // Once its Status arrives, the next decode is placed.
    c.processPacketForTest(statusPacket(QStringLiteral("WSJT-X - 2"), 21074000));
    c.processPacketForTest(decodePacket(QStringLiteral("WSJT-X - 2"), 1000,
                                        QStringLiteral("CQ JA1MZK PM95")));
    QCOMPARE(spotSpy.count(), 1);
    QCOMPARE(spotSpy.at(0).first().value<DxSpot>().freqMhz, 21.075000);
}

// Close (type 6) forgets the instance: a relaunch does not inherit the
// old band until its first Status.
void TestWsjtxDecoder::closeForgetsTheInstance() {
    WsjtxClient c;
    QSignalSpy spotSpy(&c, &WsjtxClient::spotReceived);
    c.processPacketForTest(statusPacket(QStringLiteral("WSJT-X"), 7074000));
    c.processPacketForTest(closePacket(QStringLiteral("WSJT-X")));
    QCOMPARE(c.dialTrackerForTest().instanceCount(), 0);
    c.processPacketForTest(decodePacket(QStringLiteral("WSJT-X"), 1000,
                                        QStringLiteral("CQ JA1MZK PM95")));
    QCOMPARE(spotSpy.count(), 0);
}

// WSJT-X reports 0 Hz while it has no rig: that is not a dial.
void TestWsjtxDecoder::zeroDialIsIgnored() {
    WsjtxClient c;
    QSignalSpy spotSpy(&c, &WsjtxClient::spotReceived);
    c.processPacketForTest(statusPacket(QStringLiteral("WSJT-X"), 0));
    QCOMPARE(c.dialTrackerForTest().instanceCount(), 0);
    c.processPacketForTest(decodePacket(QStringLiteral("WSJT-X"), 1000,
                                        QStringLiteral("CQ JA1MZK PM95")));
    QCOMPARE(spotSpy.count(), 0);
    // An earlier good dial survives a later 0 Hz report.
    c.processPacketForTest(statusPacket(QStringLiteral("WSJT-X"), 7074000));
    c.processPacketForTest(statusPacket(QStringLiteral("WSJT-X"), 0));
    QCOMPARE(c.dialTrackerForTest().dialFreqHzFor(QStringLiteral("WSJT-X")).value_or(0.0),
             7074000.0);
}

void TestWsjtxDecoder::extractsCallsign_data() {
    QTest::addColumn<QString>("message");
    QTest::addColumn<QString>("expected");
    QTest::newRow("CQ basic")          << "CQ W1AW FN42"          << "W1AW";
    QTest::newRow("CQ DX")             << "CQ DX JA1ABC PM95"     << "JA1ABC";
    QTest::newRow("CQ POTA")           << "CQ POTA K1ABC FN42"    << "K1ABC";
    QTest::newRow("CQ continent NA")   << "CQ NA W1AW FN42"       << "W1AW";
    QTest::newRow("directed +report")  << "K3OTH W1AW +05"         << "W1AW";
    QTest::newRow("directed R-report") << "K3OTH W1AW R-10"        << "W1AW";
    QTest::newRow("directed RR73")     << "K3OTH W1AW RR73"        << "W1AW";
}

void TestWsjtxDecoder::extractsCallsign() {
    QFETCH(QString, message);
    QFETCH(QString, expected);
    WsjtxClient c;
    QCOMPARE(c.extractCallsignForTest(message), expected);
}

QTEST_GUILESS_MAIN(TestWsjtxDecoder)
#include "tst_wsjtx_decoder.moc"
