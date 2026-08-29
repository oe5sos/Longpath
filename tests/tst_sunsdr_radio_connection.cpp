// no-port-check: NereusSDR/Longpath-original test file.

// =================================================================
// tests/tst_sunsdr_radio_connection.cpp  (NereusSDR/Longpath)
// =================================================================
//
// SunSdrRadioConnection's skeleton — deliberately NOT a "connect to a
// real QRP" test. connectToRadio() now DOES send a real minimal
// RX-start sequence (discovery broadcast, then a state-sync frame once
// a beacon replies — design doc "BREAKTHROUGH, 2026-08-26"), but every
// test here that calls connectToRadio() first calls
// setDiscoveryBroadcastEnabledForTest(false), so no real UDP packet
// ever leaves the machine and no real beacon can ever arrive — see
// that setter's comment in the header for why an undisabled test run
// would be a real hazard, not just noise, on the operator's own LAN.
// What IS tested here, all without any socket ever receiving a real
// reply:
//
//   - every connect attempt times out cleanly when no beacon replies
//     (correct behavior whether that's because discovery is suppressed
//     for the test, or because a real QRP genuinely isn't reachable)
//   - ConnectionState transitions match the same discipline P1/P2 use
//     (Connecting until a first frame, Disconnected on timeout, no
//     spurious connectFailed on an intentional disconnect)
//   - every TX-shaped pure virtual is a genuine no-op
//   - the rxWdspReady-equivalent gate actually gates: a synthetic,
//     protocol-correct IQ packet is silently dropped while closed and
//     decoded-and-emitted while open
//
// The beacon-detection/state-sync-replay path itself
// (onControlReadyRead()) is NOT covered here — it can only be reached
// by a real UDP datagram landing on the bound control socket, and
// synthesizing that would mean either sending a real loopback packet
// (reintroducing the same real-network-send concern this file goes out
// of its way to avoid) or adding a second ...ForTest() injection seam
// purely to reach it. Left as a gap for now; a bench run against the
// real QRP is what actually proves that path (see plan doc's pending
// tasks).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-26 — Original for NereusSDR/Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-08-26 — Updated for the real discovery+state-sync
//                 connectToRadio() sequence: every connect-invoking
//                 test now calls setDiscoveryBroadcastEnabledForTest(false)
//                 first. AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QHostAddress>

#include "core/SunSdrRadioConnection.h"
#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/HpsdrModel.h"
#include "core/sunsdr/SunSdrProtocol.h"

using namespace Longpath;

// See tst_radio_connection_failure.cpp for why this follows the
// watchdog constant rather than a hardcoded wait.
static constexpr int kWaitMs =
    SunSdrRadioConnection::connectTimeoutMsForTest() + 3000;

namespace {

RadioInfo someQrpInfo()
{
    RadioInfo info;
    info.address    = QHostAddress(QStringLiteral("192.0.2.1"));  // RFC 5737 — never a real reply
    info.port       = 50001;
    info.boardType  = HPSDRHW::SunSdr2Qrp;
    info.protocol   = ProtocolVersion::SunSdr;
    info.macAddress = QStringLiteral("00:00:00:00:00:00");
    info.name       = QStringLiteral("Test SunSDR2 QRP");
    return info;
}

// A protocol-correct 1210-byte RX-idle IQ packet: real header (built
// via the already-tested SunSdrProtocol::buildIqHeader), payload all
// zero (decodes to all-zero samples — see tst_sunsdr_protocol.cpp's
// decodesZeroAsZero for why that's a meaningful, not degenerate, check).
QByteArray silentIqPacket()
{
    QByteArray pkt = SunSdr::buildIqHeader(
        SunSdr::kProfileQrp, SunSdr::kOpIqRxIdle, /*seq=*/1,
        /*byte8=*/0, /*byte9=*/0);
    pkt.append(SunSdr::kIqPayloadSize, char(0));
    return pkt;
}

} // namespace

class TestSunSdrRadioConnection : public QObject
{
    Q_OBJECT

private slots:

    void protocolVersionIsDistinctFromP1AndP2()
    {
        SunSdrRadioConnection conn;
        QCOMPARE(conn.protocolVersion(), 3);
    }

    // With the real discovery broadcast suppressed for the test, no
    // beacon can ever arrive, so the connect watchdog is the only thing
    // that can end this connection attempt — exactly the same outcome
    // a real QRP that never replies would produce.
    void everyConnectAttemptTimesOutForNow()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);

        QSignalSpy spy(&conn, &RadioConnection::connectFailed);
        conn.connectToRadio(someQrpInfo());

        QVERIFY(spy.wait(kWaitMs));
        QCOMPARE(spy.count(), 1);
        const auto reason = spy.takeFirst().at(0).value<ConnectFailure>();
        QCOMPARE(reason, ConnectFailure::Timeout);
    }

    // Same discipline as P1/P2 (issue #239 precedent): Connecting until
    // a first frame actually arrives, never Connected on faith.
    void stateStaysConnectingUntilFirstFrame()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);

        QSignalSpy stateSpy(&conn, &RadioConnection::connectionStateChanged);
        conn.connectToRadio(someQrpInfo());

        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connecting, 500);
        QTest::qWait(200);
        QCOMPARE(conn.state(), ConnectionState::Connecting);

        for (int i = 0; i < stateSpy.count(); ++i) {
            const auto s = stateSpy.at(i).at(0).value<ConnectionState>();
            QVERIFY2(s != ConnectionState::Connected,
                     "Connected must never be emitted before a real frame arrives");
        }
    }

    void stateBecomesDisconnectedOnConnectTimeout()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);

        QSignalSpy failSpy(&conn, &RadioConnection::connectFailed);
        conn.connectToRadio(someQrpInfo());

        QVERIFY(failSpy.wait(kWaitMs));
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Disconnected, 500);
    }

    void noFailureOnIntentionalDisconnect()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);

        QSignalSpy spy(&conn, &RadioConnection::connectFailed);
        conn.connectToRadio(someQrpInfo());
        conn.disconnect();

        // Found live, 2026-08-26: a 100ms wait here — far short of
        // kConnectTimeoutMs (3000ms) — cannot actually distinguish "the
        // connect watchdog was properly stopped by disconnect()" from
        // "the watchdog simply hadn't fired yet." Using the same kWaitMs
        // the genuine-timeout tests use proves the stop() call actually
        // did something, not just that nothing had happened yet.
        QTest::qWait(kWaitMs);
        QCOMPARE(spy.count(), 0);
    }

    // Every TX-shaped pure virtual: called, does nothing, doesn't crash.
    // This is the whole point of B.5 — a receive-only connection that
    // is honest about being receive-only, not one that silently accepts
    // TX calls and drops them somewhere less visible. setAttenuator()
    // is included here NOT because it's unconditionally a no-op anymore
    // (it isn't — see attenuatorSendsOnlyForBenchConfirmedValues() below,
    // 2026-08-27) but because this test never connects, so
    // m_radioAddr.isNull() makes it one in this specific scenario, same
    // as every real send in this class. dB=10 also isn't one of the two
    // bench-confirmed values (0, -20) either way.
    void everyTxShapedSetterIsANoOp()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();

        conn.setTxFrequency(14074000);
        conn.setAttenuator(10);
        conn.setPreamp(true);
        conn.setTxDrive(50);
        conn.setMox(true);
        conn.setAntennaRouting(AntennaRouting{});
        const float iq[4] = {0.1f, 0.1f, 0.1f, 0.1f};
        conn.sendTxIq(iq, 2);
        conn.setTrxRelay(true);
        conn.setMicBoost(true);
        conn.setLineIn(true);
        conn.setMicTipRing(true);
        conn.setMicBias(true);
        conn.setLineInGain(10);
        conn.setUserDigOut(0x0F);
        conn.setPuresignalRun(true);
        conn.setMicPTTDisabled(true);
        conn.setMicXlr(false);
        conn.setWatchdogEnabled(false);

        // Reaching here without a crash/assert IS the test. Also confirm
        // no TX call has a side effect that would make it Connected/Tx —
        // this connection was never even asked to connect in this test.
        QCOMPARE(conn.state(), ConnectionState::Disconnected);
    }

    // ── The rxWdspReady-equivalent gate ────────────────────────────────

    void closedGateDropsAValidPacketSilently()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        QVERIFY(!conn.isRxReadyForTest());

        QSignalSpy iqSpy(&conn, &RadioConnection::iqDataReceived);
        conn.feedStreamDatagramForTest(silentIqPacket());

        QTest::qWait(50);
        QCOMPARE(iqSpy.count(), 0);
    }

    void openGateDecodesAndEmits()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        // connectToRadio() must come BEFORE setRxReadyForTest(): it
        // resolves m_profile (processStreamDatagram() bails out with no
        // profile to parse against) and it also unconditionally calls
        // setRxReady(false) as part of a fresh connection's reset -- a
        // real bug this test caught on its first-ever real run
        // (2026-08-26): setRxReadyForTest(true) called first was
        // silently clobbered by that reset.
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        conn.setRxReadyForTest(true);

        QSignalSpy iqSpy(&conn, &RadioConnection::iqDataReceived);
        conn.feedStreamDatagramForTest(silentIqPacket());

        QCOMPARE(iqSpy.count(), 1);
        const int index = iqSpy.at(0).at(0).toInt();
        const auto samples = iqSpy.at(0).at(1).value<QVector<float>>();
        QCOMPARE(index, 0);
        QCOMPARE(samples.size(), SunSdr::kIqComplexPerPkt * 2);
        for (float v : samples) {
            QCOMPARE(v, 0.0f);  // all-zero payload decodes to all-zero samples
        }
    }

    // D.3: the first successfully-decoded frame promotes Connecting to
    // Connected and cancels the connect watchdog — the ONE way this
    // connection can currently reach Connected at all, since no boot
    // macro exists to earn it through the control channel.
    void firstDecodedFramePromotesToConnected()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        // Order matters: connectToRadio() calls setRxReady(false) as
        // part of its own reset, so setRxReadyForTest(true) must come
        // AFTER it, not before -- see openGateDecodesAndEmits()'s
        // comment for the real bug this ordering used to hide.
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        conn.setRxReadyForTest(true);

        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connecting, 500);

        QSignalSpy failSpy(&conn, &RadioConnection::connectFailed);
        conn.feedStreamDatagramForTest(silentIqPacket());

        QCOMPARE(conn.state(), ConnectionState::Connected);

        // And the connect watchdog must actually be cancelled — wait
        // past its normal timeout and confirm no belated connectFailed.
        QTest::qWait(kWaitMs);
        QCOMPARE(failSpy.count(), 0);
    }

    // A malformed/wrong-magic packet must not be treated as data —
    // this is the same discipline tst_sunsdr_protocol.cpp already
    // proves at the codec level; here it's proven at the connection
    // level, through the real dispatch path.
    void wrongMagicPacketIsIgnored()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        // Found live, 2026-08-26: this test used to call
        // setRxReadyForTest(true) WITHOUT connectToRadio() first, which
        // leaves m_profile null — processStreamDatagram()'s own
        // `!m_profile` guard then rejects the packet before
        // SunSdr::parseIqHeader()'s magic-byte check is ever reached.
        // The test passed either way, so it was vacuous: it never
        // actually exercised the magic-byte discrimination it's named
        // for. connectToRadio() (discovery suppressed) is what sets
        // m_profile to &SunSdr::kProfileQrp, the same ordering
        // openGateDecodesAndEmits()/firstDecodedFramePromotesToConnected()
        // already use.
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        conn.setRxReadyForTest(true);

        QByteArray wrong = SunSdr::buildIqHeader(
            SunSdr::kProfileDx, SunSdr::kOpIqRxIdle, 1, 0, 0);  // DX magic, not QRP
        wrong.append(SunSdr::kIqPayloadSize, char(0));

        QSignalSpy iqSpy(&conn, &RadioConnection::iqDataReceived);
        conn.feedStreamDatagramForTest(wrong);

        QTest::qWait(50);
        QCOMPARE(iqSpy.count(), 0);
    }

    // ── Frame byte-exactness ─────────────────────────────────────────
    //
    // discoveryFrameForTest()/stateSyncFrameForTest()/
    // replayedFrequencyFrameForTest() exist specifically so a test can
    // assert this class sends the exact bench-confirmed bytes (header
    // comment) — found live, 2026-08-26, that nothing actually did.
    // These pin them against the documented hex strings so a future
    // accidental one-byte change (merge conflict, refactor, typo) fails
    // CI instead of silently sending a frame the 2026-08-26 bench run
    // never actually confirmed.

    void discoveryFrameMatchesBenchConfirmedBytes()
    {
        QCOMPARE(SunSdrRadioConnection::discoveryFrameForTest(),
                  QByteArray::fromHex(
                      "03ff001a000000000000000000000000000000000000fbe6"));
    }

    void stateSyncFrameMatchesBenchConfirmedBytes()
    {
        QCOMPARE(SunSdrRadioConnection::stateSyncFrameForTest(),
                  QByteArray::fromHex(
                      "03ff01000c0000000000010000007648ea9e010000000c08040302020202"));
    }

    void replayedFrequencyFrameMatchesBenchConfirmedBytes()
    {
        QCOMPARE(SunSdrRadioConnection::replayedFrequencyFrameForTest(),
                  QByteArray::fromHex(
                      "03ff0800080000000000010000008ca31dd76ce0780800000000"));
    }

    // ── The control-channel beacon-detection/state-sync-replay path ───
    //
    // Added 2026-08-26 alongside feedControlDatagramForTest(): before
    // this, onControlReadyRead()'s entire logic — every byte-guard, the
    // success path, the already-handshaken drain — had zero test
    // coverage, reachable only by a real UDP datagram landing on a
    // bound socket. This is also the exact class of gap that let the
    // m_awaitingBeacon-not-reset-on-timeout bug ship undetected the same
    // evening: a control-path regression would have shipped with every
    // one of these tests still green.

    void realBeaconReplyOpensGateAndRepliesWithStateSync()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        QVERIFY(!conn.isRxReadyForTest());

        const QHostAddress radio(QStringLiteral("192.0.2.200"));  // RFC 5737, synthetic
        conn.feedControlDatagramForTest(
            QByteArray::fromHex("03ff011a7c0000004119c0a810c8c0a810c851c300004928"),
            radio);

        QVERIFY(conn.isRxReadyForTest());
    }

    void echoOfOwnDiscoveryBroadcastIsIgnoredNotTreatedAsABeacon()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());

        // Opcode 0x00, not 0x01 — the operator's own discovery broadcast
        // reflected back over the LAN (design doc, "another LAN host's
        // echo, expected"), the exact same bytes discoveryFrameForTest()
        // sends. This must NOT be mistaken for the radio's real beacon.
        conn.feedControlDatagramForTest(
            SunSdrRadioConnection::discoveryFrameForTest(),
            QHostAddress(QStringLiteral("192.0.2.100")));

        QVERIFY(!conn.isRxReadyForTest());
    }

    void wrongMagicControlDatagramIsIgnored()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());

        // DX's magic0 (0x32), not QRP's (0x03) — otherwise a byte-exact
        // beacon-reply shape.
        QByteArray wrongMagic = QByteArray::fromHex(
            "32ff011a7c0000004119c0a810c8c0a810c851c300004928");
        conn.feedControlDatagramForTest(
            wrongMagic, QHostAddress(QStringLiteral("192.0.2.200")));

        QVERIFY(!conn.isRxReadyForTest());
    }

    void secondBeaconReplyAfterHandshakeIsDrainedNotReprocessed()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());

        const QByteArray beacon = QByteArray::fromHex(
            "03ff011a7c0000004119c0a810c8c0a810c851c300004928");
        conn.feedControlDatagramForTest(
            beacon, QHostAddress(QStringLiteral("192.0.2.200")));
        QVERIFY(conn.isRxReadyForTest());

        // A duplicate/late second beacon (e.g. a retransmit) must not
        // re-run the handshake — m_awaitingBeacon is already false, so
        // this must be drained silently, not double-processed (which
        // would mean sending the state-sync frame a second time and
        // could, with a different sender address, reassign m_radioAddr
        // mid-session).
        conn.setRxReadyForTest(false);  // close the gate to observe whether this reopens it
        conn.feedControlDatagramForTest(
            beacon, QHostAddress(QStringLiteral("192.0.2.201")));
        QVERIFY(!conn.isRxReadyForTest());
    }

    // ── disconnect()'s reset of the beacon-wait state ──────────────────
    //
    // Found live, 2026-08-26: disconnect()'s m_awaitingBeacon/m_radioAddr
    // reset had no direct test — a regression dropping either line would
    // have shipped with every existing test green, since nothing
    // re-checked the gate after a disconnect() that followed an opened
    // session.

    void lateBeaconAfterDisconnectDoesNotReopenTheGate()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());

        const QByteArray beacon = QByteArray::fromHex(
            "03ff011a7c0000004119c0a810c8c0a810c851c300004928");
        conn.feedControlDatagramForTest(
            beacon, QHostAddress(QStringLiteral("192.0.2.200")));
        QVERIFY(conn.isRxReadyForTest());

        conn.disconnect();
        QVERIFY(!conn.isRxReadyForTest());

        // A beacon landing after disconnect() must not reopen the gate —
        // if disconnect() ever stopped resetting m_awaitingBeacon, this
        // would silently pass again.
        conn.feedControlDatagramForTest(
            beacon, QHostAddress(QStringLiteral("192.0.2.201")));
        QVERIFY(!conn.isRxReadyForTest());
    }

    // ── init()'s fixed-port binding ─────────────────────────────────────
    //
    // The root cause of the first live-test bug this evening (an
    // ephemeral bind meant the radio's reply had nowhere bound to land)
    // had zero test coverage even though the port CHOICE — unlike the
    // round-trip symptom — needs no networking at all to check.

    void controlAndStreamSocketsBindToTheFixedProfilePorts()
    {
        SunSdrRadioConnection conn;
        // Deliberately does NOT call setFixedPortBindingEnabledForTest(false)
        // — this test exists specifically to check the real, un-overridden
        // default (true) actually binds the fixed profile ports.
        conn.init();

        QCOMPARE(conn.controlSocketLocalPortForTest(),
                  SunSdr::kProfileQrp.defaultCtrlPort);
        QCOMPARE(conn.streamSocketLocalPortForTest(),
                  SunSdr::kProfileQrp.defaultStreamPort);
    }

    // ── Reentrant connect / unrecognized board fallback ─────────────────

    void connectToRadioCalledTwiceDisconnectsTheFirstAttempt()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);

        QSignalSpy stateSpy(&conn, &RadioConnection::connectionStateChanged);
        conn.connectToRadio(someQrpInfo());
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connecting, 500);

        // Second call must tear the first attempt down (disconnect())
        // before starting a fresh one, not race two watchdogs.
        conn.connectToRadio(someQrpInfo());
        QCOMPARE(conn.state(), ConnectionState::Connecting);

        bool sawDisconnectedInBetween = false;
        for (int i = 0; i < stateSpy.count(); ++i) {
            if (stateSpy.at(i).at(0).value<ConnectionState>()
                == ConnectionState::Disconnected) {
                sawDisconnectedInBetween = true;
            }
        }
        QVERIFY2(sawDisconnectedInBetween,
                 "second connectToRadio() must disconnect() the first attempt");
    }

    void unrecognizedBoardFallsBackToQrpProfileInsteadOfCrashing()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);

        RadioInfo info = someQrpInfo();
        info.boardType = HPSDRHW::Hermes;  // not SunSdr2Qrp — the fallback path

        // Reaching here without a crash/assert IS most of the test —
        // resolveProfile()'s fallback (qCWarning + kProfileQrp) has no
        // other externally-observable effect than "doesn't misbehave."
        conn.connectToRadio(info);
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connecting, 500);
    }

    // ── setAttenuator() — bench-confirmed for exactly two dB values ────
    //
    // Added 2026-08-27 alongside the real implementation (opcode 0x04,
    // two exact captured frames — see the .cpp comment for provenance).
    // Without a real socket to receive on, this can't assert the exact
    // bytes sent (no other test in this file does that for the control
    // channel either), but it does prove the class doesn't crash for
    // any of the three cases (confirmed value while disconnected,
    // unconfirmed value while disconnected) and that an unconfirmed
    // value never reaches the point of trying to send at all — the
    // guard at the top of setAttenuator() short-circuits before the
    // dB-value branch either way while m_radioAddr is unset, so this
    // mainly documents the current contract rather than exercising the
    // send path itself (that needs a live radio or a loopback listener,
    // neither of which exists in this test file yet).
    void attenuatorSendsOnlyForBenchConfirmedValues()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();

        // No connection ever made — m_radioAddr stays null, so all
        // three calls below must be safe no-ops regardless of dB value.
        conn.setAttenuator(0);
        conn.setAttenuator(-20);
        conn.setAttenuator(-10);  // not one of the two confirmed values

        QCOMPARE(conn.state(), ConnectionState::Disconnected);
    }

    // ── The dead-link watchdog (added 2026-08-28) ───────────────────────
    //
    // Found missing entirely in the first self-review: nothing in this
    // class re-armed after the initial connect watchdog stopped, so a
    // QRP powered off or unplugged mid-session left ConnectionState
    // stuck at Connected forever. The three tests below pin the fix and
    // two further bugs a second review pass found in it the same
    // evening: the stream socket has no sender check (foreign traffic
    // on the shared, ShareAddress-bound well-known port both gets
    // decoded and keeps the watchdog's silence clock alive), and two of
    // the three teardown paths never cleared m_radioAddr, leaving
    // setReceiverFrequency()/setAttenuator() free to keep sending to a
    // torn-down session.

    // A still-streaming prior session's QRP (or any other sender on the
    // shared stream port) must not be decoded as this session's I/Q —
    // found in review, 2026-08-28; see processStreamDatagram()'s comment.
    void foreignSenderStreamPacketIsIgnored()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());

        const QHostAddress radio(QStringLiteral("192.0.2.200"));
        conn.feedControlDatagramForTest(
            QByteArray::fromHex("03ff011a7c0000004119c0a810c8c0a810c851c300004928"),
            radio);
        QVERIFY(conn.isRxReadyForTest());

        const QHostAddress stranger(QStringLiteral("192.0.2.201"));
        QSignalSpy iqSpy(&conn, &RadioConnection::iqDataReceived);
        conn.feedStreamDatagramFromSenderForTest(silentIqPacket(), stranger);

        QTest::qWait(50);
        QCOMPARE(iqSpy.count(), 0);

        // The real radio's own packet, identical content, must still
        // decode — this isn't a content problem, only a sender one.
        conn.feedStreamDatagramFromSenderForTest(silentIqPacket(), radio);
        QTRY_COMPARE_WITH_TIMEOUT(iqSpy.count(), 1, 500);
    }

    // onConnectTimeout()'s gotBeacon=true branch (a beacon replied, the
    // stream never started) left m_radioAddr set — found in review,
    // 2026-08-28. disconnect() already cleared it; this path didn't.
    void connectTimeoutAfterBeaconClearsRadioAddr()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());

        conn.feedControlDatagramForTest(
            QByteArray::fromHex("03ff011a7c0000004119c0a810c8c0a810c851c300004928"),
            QHostAddress(QStringLiteral("192.0.2.200")));
        QVERIFY(conn.hasRadioAddrForTest());

        // No stream packet ever follows — the connect watchdog is still
        // running (D.3's promotion to Connected only happens on a real
        // decoded I/Q frame) and fires at kConnectTimeoutMs.
        QSignalSpy failSpy(&conn, &RadioConnection::connectFailed);
        QVERIFY(failSpy.wait(kWaitMs));
        QVERIFY(!conn.hasRadioAddrForTest());
    }

    // The data watchdog itself: connect, real beacon, no stream data
    // ever follows (same shape SunSDR2 mid-session power-off would
    // produce once a stream had actually started) — must transition to
    // LinkLost and clear m_radioAddr, not stay stuck Connected.
    void dataWatchdogTripTransitionsToLinkLostAndClearsRadioAddr()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());

        conn.feedControlDatagramForTest(
            QByteArray::fromHex("03ff011a7c0000004119c0a810c8c0a810c851c300004928"),
            QHostAddress(QStringLiteral("192.0.2.200")));
        QVERIFY(conn.isRxReadyForTest());
        QVERIFY(conn.hasRadioAddrForTest());

        // One real decoded frame promotes to Connected and stops the
        // connect watchdog — after this, only the data watchdog is
        // still running, so a real trip is unambiguous.
        conn.feedStreamDatagramForTest(silentIqPacket());
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 500);

        QTRY_COMPARE_WITH_TIMEOUT(
            conn.state(), ConnectionState::LinkLost,
            SunSdrRadioConnection::dataSilenceTimeoutMsForTest() + 2000);
        QVERIFY(!conn.hasRadioAddrForTest());
        QVERIFY(!conn.isRxReadyForTest());
    }
};

QTEST_MAIN(TestSunSdrRadioConnection)
#include "tst_sunsdr_radio_connection.moc"
