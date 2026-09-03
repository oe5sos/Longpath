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
#include "core/sunsdr/SunSdrTxPacer.h"
#include "core/safety/BandPlanGuard.h"
#include "core/WdspTypes.h"
#include "models/Band.h"

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

// Step 3 (SunSDR2 QRP TX-chain plan): the exact same "armed, in-band,
// mode-allowed" TxCheckContext the Step 2 tests above already use
// (setMoxAcceptedWhenArmedAndInBandSendsZeroBytes()'s own ctx) — reused
// here rather than picked fresh, same rationale that comment gives:
// a regression in BandPlanGuard's own tables fails a Step 2 test first,
// not silently show up here as a mysterious refusal.
TxCheckContext armedInBandCtx()
{
    TxCheckContext ctx;
    ctx.region   = safety::Region::UnitedStates;
    ctx.txFreqHz = 14'200'000;  // US 20m, well in-band
    ctx.mode     = DSPMode::USB;
    ctx.rxBand   = Band::Band20m;
    ctx.txBand   = Band::Band20m;
    return ctx;
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

    // ── Step 2 (SunSDR2 QRP TX-chain plan): bench-only TX gate ─────────
    //
    // Still zero wire reachability — every one of these confirms setMox()
    // never touches a socket (txByteRate() stays 0.0, the same accessor
    // tst_radio_connection_byte_rates.cpp already uses to prove "nothing
    // was sent"), no matter which of the three outcomes it reaches.
    // Region::UnitedStates / 14,200,000 Hz / Band20m below are the exact
    // "definitely in-band" values tst_band_plan_guard_mode_allow_list.cpp
    // already pins for checkMoxAllowed()'s ok path (kRegion/kValidHz/
    // kBand20m there) — reused here rather than picked fresh, so a
    // regression in BandPlanGuard's own band tables would fail that
    // test first, not silently show up here as a mysterious refusal.

    // setMox(true) with the bench gate never armed: refused before
    // BandPlanGuard is even consulted (m_txCheckContext is left at its
    // default, meaningless-until-armed values — see that struct's own
    // comment), and traced with the literal "refused: not armed" reason.
    void setMoxRefusedWhenNotArmed()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();

        QVERIFY(!conn.isTxArmedForTest());
        conn.setMox(true);

        QVERIFY(!conn.isMoxForTest());
        QCOMPARE(conn.txByteRate(1000), 0.0);  // never reaches the wire

        const auto trace = conn.txTraceForTest();
        QCOMPARE(trace.size(), 1);
        QCOMPARE(trace.last().kind, TxTraceKind::MoxRefused);
        QCOMPARE(trace.last().reason, QStringLiteral("refused: not armed"));
    }

    // Armed, but the context describes a mode BandPlanGuard's own 3M-1b
    // allow-list rejects (CWL — "CW TX coming in Phase 3M-2"), at an
    // otherwise perfectly in-band frequency, so the mode check — not the
    // frequency/band check — is unambiguously what refuses this.
    void setMoxRefusedByBandPlanGuardWhenArmedButModeNotAllowed()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();

        conn.setTxArmedForTest(true);
        QVERIFY(conn.isTxArmedForTest());

        TxCheckContext ctx;
        ctx.region  = safety::Region::UnitedStates;
        ctx.txFreqHz = 14'200'000;   // valid US 20m -- not the blocker here
        ctx.mode    = DSPMode::CWL;  // CW TX not allowed yet (Phase 3M-2)
        ctx.rxBand  = Band::Band20m;
        ctx.txBand  = Band::Band20m;
        conn.setTxCheckContextForTest(ctx);

        conn.setMox(true);

        QVERIFY(!conn.isMoxForTest());
        QCOMPARE(conn.txByteRate(1000), 0.0);

        const auto trace = conn.txTraceForTest();
        // Armed (from setTxArmedForTest(true) above) + this refusal.
        QCOMPARE(trace.size(), 2);
        QCOMPARE(trace.at(0).kind, TxTraceKind::Armed);
        QCOMPARE(trace.at(1).kind, TxTraceKind::MoxRefused);
        QCOMPARE(trace.at(1).reason, QStringLiteral("CW TX coming in Phase 3M-2"));
    }

    // Armed, out-of-band frequency this time (above the US 20m edge) on
    // an otherwise-allowed mode — the freq/band half of checkMoxAllowed(),
    // not the mode half, is what refuses this one.
    void setMoxRefusedByBandPlanGuardWhenArmedButOutOfBand()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();

        conn.setTxArmedForTest(true);

        TxCheckContext ctx;
        ctx.region  = safety::Region::UnitedStates;
        ctx.txFreqHz = 14'500'000;  // above the US 20m edge (14.350 MHz)
        ctx.mode    = DSPMode::USB;
        ctx.rxBand  = Band::Band20m;
        ctx.txBand  = Band::Band20m;
        conn.setTxCheckContextForTest(ctx);

        conn.setMox(true);

        QVERIFY(!conn.isMoxForTest());
        QCOMPARE(conn.txByteRate(1000), 0.0);

        const auto trace = conn.txTraceForTest();
        QCOMPARE(trace.last().kind, TxTraceKind::MoxRefused);
        QCOMPARE(trace.last().reason,
                  QStringLiteral("Frequency outside TX-allowed range"));
    }

    // Armed AND in-band/in-mode: BandPlanGuard allows it, m_mox actually
    // transitions to true, an "accepted" transition is traced -- and,
    // the whole point of this step, txByteRate() stays exactly 0.0: no
    // frame is built or sent, even though Step 1's SunSdrProtocol
    // encoders (buildMoxFrame() etc.) exist and could in principle be
    // called here. That wiring is a later, separately-reviewed step.
    void setMoxAcceptedWhenArmedAndInBandSendsZeroBytes()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();

        conn.setTxArmedForTest(true);

        TxCheckContext ctx;
        ctx.region  = safety::Region::UnitedStates;
        ctx.txFreqHz = 14'200'000;  // US 20m, well in-band
        ctx.mode    = DSPMode::USB;
        ctx.rxBand  = Band::Band20m;
        ctx.txBand  = Band::Band20m;
        conn.setTxCheckContextForTest(ctx);

        conn.setMox(true);

        QVERIFY(conn.isMoxForTest());
        QCOMPARE(conn.txByteRate(1000), 0.0);  // accepted, but zero wire effect

        const auto trace = conn.txTraceForTest();
        QCOMPARE(trace.size(), 2);  // Armed + MoxAccepted
        QCOMPARE(trace.at(0).kind, TxTraceKind::Armed);
        QCOMPARE(trace.at(1).kind, TxTraceKind::MoxAccepted);
        QVERIFY(trace.at(1).reason.contains(QStringLiteral("accepted")));
    }

    // setMox(false) must be an unconditional release — the "armed" gate
    // and BandPlanGuard must apply ONLY to turning TX on, never to
    // turning it off, the same precedent MoxController::setMox() already
    // sets (K.2's BandPlanGuard check and the Task 87 TxInterlockPolicy
    // check are both written as `if (on && ...)`, gating only the
    // TX-on path). A kill switch that could itself be refused is not a
    // kill switch. Found and fixed 2026-09-02, before this class ever
    // shipped a caller that could hit it.
    void setMoxFalseIsUnconditionalEvenWhenNeverArmed()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();

        // Deliberately never armed, and no TxCheckContext set at all —
        // if setMox(false) routed through the same gate as setMox(true),
        // this would either refuse (not armed) or hit BandPlanGuard with
        // a default-constructed, meaningless context. It must do neither.
        conn.setMox(false);

        QVERIFY(!conn.isMoxForTest());
        const auto trace = conn.txTraceForTest();
        QCOMPARE(trace.size(), 1);
        QCOMPARE(trace.at(0).kind, TxTraceKind::MoxAccepted);
        QVERIFY(trace.at(0).reason.contains(QStringLiteral("unconditional")));
    }

    void setMoxFalseBypassesBandPlanGuardEvenWhenThatContextWouldRefuseEnable()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();

        conn.setTxArmedForTest(true);

        TxCheckContext ctx;
        ctx.region  = safety::Region::UnitedStates;
        ctx.txFreqHz = 14'200'000;  // in-band -- enable this once, cleanly
        ctx.mode    = DSPMode::USB;
        ctx.rxBand  = Band::Band20m;
        ctx.txBand  = Band::Band20m;
        conn.setTxCheckContextForTest(ctx);
        conn.setMox(true);
        QVERIFY(conn.isMoxForTest());

        // Now mutate the context to something BandPlanGuard would refuse
        // for an ENABLE call (out of band) -- and confirm setMox(false)
        // still succeeds, proving the release path truly does not
        // consult BandPlanGuard at all, not just that it happens to pass.
        ctx.txFreqHz = 14'500'000;  // out of the US 20m TX-allowed range
        conn.setTxCheckContextForTest(ctx);

        conn.setMox(false);

        QVERIFY(!conn.isMoxForTest());
        const auto trace = conn.txTraceForTest();
        QCOMPARE(trace.back().kind, TxTraceKind::MoxAccepted);
        QVERIFY(trace.back().reason.contains(QStringLiteral("mox -> false")));
        QVERIFY(trace.back().reason.contains(QStringLiteral("unconditional")));
    }

    // Disarming mid-transmission is itself a kill request, not merely a
    // block on future setMox(true) calls -- found during Step 3's review
    // (a running pacer survived setTxArmedForTest(false) alone, since
    // only the four teardown paths and setMox(false) itself stopped it)
    // and fixed by routing setTxArmed(false) through setMox(false)
    // whenever MOX is currently on. Same "a kill switch that could be
    // refused isn't one" reasoning as the two tests above, applied to the
    // arm gate itself.
    void disarmingMidTransmissionStopsThePacerAndClearsMox()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();

        conn.setTxArmedForTest(true);
        conn.setTxCheckContextForTest(armedInBandCtx());
        conn.setMox(true);
        QVERIFY(conn.isMoxForTest());
        QVERIFY(conn.pacerRunningForTest());

        conn.setTxArmedForTest(false);

        QVERIFY(!conn.isTxArmedForTest());
        QVERIFY(!conn.isMoxForTest());
        QVERIFY(!conn.pacerRunningForTest());

        const auto trace = conn.txTraceForTest();
        QCOMPARE(trace.back().kind, TxTraceKind::MoxAccepted);
        QVERIFY(trace.back().reason.contains(QStringLiteral("mox -> false")));
    }

    // ── disconnect()'s reset of the new TX gate state ───────────────────
    //
    // Mirrors hasRadioAddrForTest()'s own after-teardown pattern
    // (lateBeaconAfterDisconnectDoesNotReopenTheGate() above): arm, get
    // an accepted MOX transition, then disconnect() — both the arm gate
    // and the accepted MOX state must be false afterward, since bench
    // arming is deliberately per-session, not sticky across a teardown.
    void disconnectClearsTxArmedAndMoxAfterAnAcceptedTransition()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();

        conn.setTxArmedForTest(true);

        TxCheckContext ctx;
        ctx.region  = safety::Region::UnitedStates;
        ctx.txFreqHz = 14'200'000;
        ctx.mode    = DSPMode::USB;
        ctx.rxBand  = Band::Band20m;
        ctx.txBand  = Band::Band20m;
        conn.setTxCheckContextForTest(ctx);

        conn.setMox(true);
        QVERIFY(conn.isTxArmedForTest());
        QVERIFY(conn.isMoxForTest());

        conn.disconnect();

        QVERIFY(!conn.isTxArmedForTest());
        QVERIFY(!conn.isMoxForTest());
        // The trace ring is cleared on disconnect() specifically (see
        // that function's own comment) -- a fresh bench session starts
        // from an empty ring, not the prior session's history.
        QCOMPARE(conn.txTraceForTest().size(), 0);
    }

    // ── Step 3 (SunSDR2 QRP TX-chain plan): SunSdrTxPacer itself ────────
    //
    // These four instantiate SunSdrTxPacer directly (not through
    // SunSdrRadioConnection) — the pacer's own tick/ring/seq behavior is
    // independent of the connection that will eventually own one, and
    // testing it directly avoids needing connection-level plumbing just
    // to reach ring/seq accessors the connection itself never forwards
    // (only pacerRunningForTest() is forwarded — see that accessor's own
    // header comment). Synthetic tickForTest() calls throughout: never a
    // real 5.12ms wait.

    void txPacerStartsStoppedWithAnEmptyLastFrame()
    {
        SunSdrTxPacer pacer;
        QVERIFY(!pacer.pacerRunningForTest());
        QVERIFY(pacer.lastTxFrameForTest().isEmpty());
        QCOMPARE(pacer.pacerUnderrunsForTest(), 0u);
        QCOMPARE(pacer.pacerSeqForTest(), quint16(0));
        QCOMPARE(pacer.ringSampleCountForTest(), 0);

        pacer.start();
        QVERIFY(pacer.pacerRunningForTest());
        pacer.stop();
        QVERIFY(!pacer.pacerRunningForTest());
    }

    // A tick with >= kIqComplexPerPkt samples queued must build a real,
    // byte-exact 1210-byte frame: the same header SunSdrProtocol::
    // buildIqHeader() would build by hand (opcode kOpIqTxActive, seq=0,
    // state bytes 0x02/0x01 -- see SunSdrTxPacer.cpp's own comment for
    // that citation) followed by the exact 200 pushed samples in push
    // order, and the ring must be fully drained.
    void txPacerTickBuildsByteExactFrameFromRingSamples()
    {
        SunSdrTxPacer pacer;  // defaults to SunSdr::kProfileQrp

        QByteArray expectedPayload;
        for (int i = 0; i < SunSdr::kIqComplexPerPkt; ++i) {
            // Recognizable, non-degenerate sample content (index in the
            // low two bytes, a fixed marker in the last) -- a
            // wrong-slot or off-by-one copy bug would show up as a
            // content mismatch, not just a length one.
            QByteArray sample(SunSdr::kIqBytesPerComplex, char(0));
            sample[0] = static_cast<char>(i & 0xFF);
            sample[1] = static_cast<char>((i >> 8) & 0xFF);
            sample[5] = char(0xAB);
            QVERIFY(pacer.pushSample(sample));
            expectedPayload += sample;
        }
        QCOMPARE(pacer.ringSampleCountForTest(), SunSdr::kIqComplexPerPkt);

        pacer.tickForTest();

        QCOMPARE(pacer.ringSampleCountForTest(), 0);   // fully drained
        QCOMPARE(pacer.pacerUnderrunsForTest(), 0u);   // this tick built, didn't underrun
        QCOMPARE(pacer.pacerSeqForTest(), quint16(1)); // advanced past the built frame's seq=0

        const QByteArray expectedHeader = SunSdr::buildIqHeader(
            SunSdr::kProfileQrp, SunSdr::kOpIqTxActive, /*seq=*/0,
            /*byte8=*/0x02, /*byte9=*/0x01);
        QCOMPARE(pacer.lastTxFrameForTest(), expectedHeader + expectedPayload);
    }

    // The design doc's explicit rule: an empty (or merely partial) ring
    // must NOT build a new frame -- it must repeat the cached last frame
    // byte-identical and count an underrun. Also covers the "nothing has
    // ever been built yet" underrun case (lastTxFrameForTest() has
    // nothing to repeat, so it simply stays empty).
    void txPacerEmptyRingRepeatsLastFrameAndCountsUnderrun()
    {
        SunSdrTxPacer pacer;

        pacer.tickForTest();  // nothing queued, nothing ever built yet
        QCOMPARE(pacer.pacerUnderrunsForTest(), 1u);
        QVERIFY(pacer.lastTxFrameForTest().isEmpty());

        for (int i = 0; i < SunSdr::kIqComplexPerPkt; ++i) {
            QVERIFY(pacer.pushSample(QByteArray(SunSdr::kIqBytesPerComplex, char(i))));
        }
        pacer.tickForTest();
        const QByteArray built = pacer.lastTxFrameForTest();
        QVERIFY(!built.isEmpty());
        QCOMPARE(pacer.pacerUnderrunsForTest(), 1u);   // unchanged: that tick built
        QCOMPARE(pacer.pacerSeqForTest(), quint16(1));

        // Ring is empty again -- must repeat `built` byte-for-byte
        // (seq included) rather than building a new, different-seq frame.
        pacer.tickForTest();
        QCOMPARE(pacer.pacerUnderrunsForTest(), 2u);
        QCOMPARE(pacer.lastTxFrameForTest(), built);
        QCOMPARE(pacer.pacerSeqForTest(), quint16(1));  // unchanged: no new frame built
    }

    // "TX packet sequence numbers reset to 0 on every PTT-on" (design
    // doc) -- proven at the byte level: after resetSeq(), the very next
    // built frame's own header must carry seq=0, not a continuation of
    // whatever a prior "session" had already advanced to.
    void txPacerResetSeqZeroesSequenceAfterAdvancing()
    {
        SunSdrTxPacer pacer;

        for (int frame = 0; frame < 2; ++frame) {
            for (int i = 0; i < SunSdr::kIqComplexPerPkt; ++i) {
                QVERIFY(pacer.pushSample(QByteArray(SunSdr::kIqBytesPerComplex, char(i))));
            }
            pacer.tickForTest();
        }
        QVERIFY(pacer.pacerSeqForTest() > 0);  // advanced across two real builds

        pacer.resetSeq();
        QCOMPARE(pacer.pacerSeqForTest(), quint16(0));

        for (int i = 0; i < SunSdr::kIqComplexPerPkt; ++i) {
            QVERIFY(pacer.pushSample(QByteArray(SunSdr::kIqBytesPerComplex, char(i))));
        }
        pacer.tickForTest();

        const QByteArray header = pacer.lastTxFrameForTest().left(SunSdr::kIqHeaderSize);
        SunSdr::IqHeader parsed;
        QVERIFY(SunSdr::parseIqHeader(
            reinterpret_cast<const quint8*>(header.constData()), header.size(),
            SunSdr::kProfileQrp, &parsed));
        QCOMPARE(parsed.seq, quint16(0));
    }

    // ── Step 3: SunSdrRadioConnection's own pacer wiring ────────────────
    //
    // pacerRunningForTest() (delegating to the owned SunSdrTxPacer) is
    // the only pacer-shaped thing exposed at the connection level -- see
    // that accessor's own header comment. These confirm setMox(true)
    // starts it, and that all four places obligated to stop it actually
    // do, including while "mid-transmission" (mox accepted, pacer
    // genuinely running) rather than only when nothing was ever armed.

    void pacerRunsWhileMoxAcceptedAndStopsOnSetMoxFalse()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();

        conn.setTxArmedForTest(true);
        conn.setTxCheckContextForTest(armedInBandCtx());

        QVERIFY(!conn.pacerRunningForTest());
        conn.setMox(true);
        QVERIFY(conn.isMoxForTest());
        QVERIFY(conn.pacerRunningForTest());

        conn.setMox(false);
        QVERIFY(!conn.isMoxForTest());
        QVERIFY(!conn.pacerRunningForTest());
    }

    void pacerStopsAfterDisconnectMidTransmission()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();

        conn.setTxArmedForTest(true);
        conn.setTxCheckContextForTest(armedInBandCtx());
        conn.setMox(true);
        QVERIFY(conn.pacerRunningForTest());

        conn.disconnect();
        QVERIFY(!conn.pacerRunningForTest());
    }

    // Invokes the private onConnectTimeout() slot directly (same
    // QMetaObject::invokeMethod pattern tst_rf2ks_connection_reconnect.cpp
    // already uses for its own private timeout slot) rather than waiting
    // out a real kConnectTimeoutMs -- onConnectTimeout()'s own guard
    // requires state()==Connecting, which connectToRadio() alone already
    // reaches without ever needing a beacon.
    void pacerStopsAfterConnectTimeoutMidTransmission()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connecting, 500);

        conn.setTxArmedForTest(true);
        conn.setTxCheckContextForTest(armedInBandCtx());
        conn.setMox(true);
        QVERIFY(conn.pacerRunningForTest());

        QVERIFY(QMetaObject::invokeMethod(&conn, "onConnectTimeout",
                                          Qt::DirectConnection));
        QVERIFY(!conn.pacerRunningForTest());
    }

    // The data watchdog path: real beacon + one real decoded frame to
    // reach Connected, mox accepted mid-"session", then the real
    // kDataSilenceTimeoutMs wait for a genuine trip -- same shape
    // dataWatchdogTripTransitionsToLinkLostAndClearsRadioAddr() above
    // already uses, extended to also assert the pacer stopped.
    void pacerStopsAfterDataWatchdogTripMidTransmission()
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

        conn.feedStreamDatagramForTest(silentIqPacket());
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 500);

        conn.setTxArmedForTest(true);
        conn.setTxCheckContextForTest(armedInBandCtx());
        conn.setMox(true);
        QVERIFY(conn.pacerRunningForTest());

        QTRY_COMPARE_WITH_TIMEOUT(
            conn.state(), ConnectionState::LinkLost,
            SunSdrRadioConnection::dataSilenceTimeoutMsForTest() + 2000);
        QVERIFY(!conn.pacerRunningForTest());
    }
};

QTEST_MAIN(TestSunSdrRadioConnection)
#include "tst_sunsdr_radio_connection.moc"
