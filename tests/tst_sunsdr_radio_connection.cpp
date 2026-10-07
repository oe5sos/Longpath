// no-port-check: Longpath/Longpath-original test file.

// =================================================================
// tests/tst_sunsdr_radio_connection.cpp  (Longpath)
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
// Modification history (Longpath):
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

// Ein QRP-Block: I (Bytes 3-5 je Paar) traegt immer Daten, Q (Bytes 0-2)
// nur mit withQ -- nach dem Einschalten bleibt Q exakt 0.
QByteArray qrpBlock(quint16 seq, bool withQ)
{
    QByteArray pkt = SunSdr::buildIqHeader(
        SunSdr::kProfileQrp, SunSdr::kOpIqRxIdle, seq, 0x01, 0x00);
    QByteArray payload(SunSdr::kIqPayloadSize, char(0));
    for (int k = 0; k < SunSdr::kIqPayloadSize; k += 6) {
        payload[k + 3] = char(7);
        if (withQ) { payload[k] = char(5); }
    }
    pkt.append(payload);
    return pkt;
}

double peakOf(const QList<QVariant>& emission)
{
    double peak = 0.0;
    for (const float v : emission.at(1).value<QVector<float>>()) {
        peak = std::max(peak, double(std::abs(v)));
    }
    return peak;
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
        // Diese Linie prueft den FEHLSCHLAG. Seit dem 2026-10-04
        // wiederholt die Verbindung die Suche von selbst (das Geraet
        // sperrt nach einem abrupten Ende rund eine Minute) -- hier
        // soll sie das nicht, sonst wartet der Pruefstand 90 s.
        SunSdrRadioConnection conn;
        conn.setSucheWiederholungEnabledForTest(false);
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
        // Diese Linie prueft den FEHLSCHLAG. Seit dem 2026-10-04
        // wiederholt die Verbindung die Suche von selbst (das Geraet
        // sperrt nach einem abrupten Ende rund eine Minute) -- hier
        // soll sie das nicht, sonst wartet der Pruefstand 90 s.
        SunSdrRadioConnection conn;
        conn.setSucheWiederholungEnabledForTest(false);
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

    // 2026-09-24: die QRP wiederholt jeden Block bis zu achtmal, bis der
    // Host ihn mit derselben Folgenummer beantwortet (ExpertSDR2-Mitschnitt;
    // am Geraet bestaetigt: danach 240 Pakete/s, eine Kopie je Nummer).
    // Acht verschraenkte Kopien von zwei Bloecken muessen genau zwei
    // Antworten ausloesen, jede mit der Nummer ihres Blocks.
    void everyNewBlockIsAnsweredOnceWithItsOwnSequence()
    {
        qunsetenv("LONGPATH_SUNSDR_BLOCKANTWORT");
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

        auto block = [](quint16 seq) {
            QByteArray pkt = SunSdr::buildIqHeader(
                SunSdr::kProfileQrp, SunSdr::kOpIqRxIdle, seq, 0x01, 0x00);
            pkt.append(SunSdr::kIqPayloadSize, char(0));
            return pkt;
        };
        for (int copy = 0; copy < 8; ++copy) {
            conn.feedStreamDatagramFromSenderForTest(block(7), radio);
            conn.feedStreamDatagramFromSenderForTest(block(8), radio);
        }

        QTRY_COMPARE_WITH_TIMEOUT(conn.blockRepliesSentForTest(), quint64(2), 500);
        QCOMPARE(conn.lastBlockReplySeqForTest(), quint16(8));
    }

    void blockReplyCanBeSwitchedOff()
    {
        qputenv("LONGPATH_SUNSDR_BLOCKANTWORT", "0");
        auto restore = qScopeGuard([] { qunsetenv("LONGPATH_SUNSDR_BLOCKANTWORT"); });
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());

        const QHostAddress radio(QStringLiteral("192.0.2.200"));
        conn.feedControlDatagramForTest(
            QByteArray::fromHex("03ff011a7c0000004119c0a810c8c0a810c851c300004928"),
            radio);
        QSignalSpy iqSpy(&conn, &RadioConnection::iqDataReceived);
        conn.feedStreamDatagramFromSenderForTest(silentIqPacket(), radio);

        QTRY_COMPARE_WITH_TIMEOUT(iqSpy.count(), 1, 500);
        QCOMPARE(conn.blockRepliesSentForTest(), quint64(0));
    }

    // 2026-10-05, aus Martins Mitschnitten: ExpertSDR2 beantwortet JEDEN
    // Block, aber abwechselnd -- ungerade Nummer voll (1210 Byte, Laengenfeld
    // 1200), gerade Nummer nur der KOPF (10 Byte, Laengenfeld 0). Gemessen
    // als 240/s + 240/s gegen 480/s vom Geraet, in allen drei Mitschnitten
    // gleich. Longpath schickt bisher immer den vollen Block.
    //
    // Geprueft wird die GROESSE, nicht die Zahl: die Zahl der Antworten
    // aendert sich nicht, nur ihr Gewicht. Eine Pruefung auf
    // blockRepliesSentForTest() allein waere in beiden Fassungen gruen.
    void jedeZweiteAntwortIstNurDerKopf()
    {
        qunsetenv("LONGPATH_SUNSDR_BLOCKANTWORT");
        qputenv("LONGPATH_SUNSDR_KOPFANTWORT", "1");
        auto restore = qScopeGuard([] { qunsetenv("LONGPATH_SUNSDR_KOPFANTWORT"); });
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

        auto block = [](quint16 seq) {
            QByteArray pkt = SunSdr::buildIqHeader(
                SunSdr::kProfileQrp, SunSdr::kOpIqRxIdle, seq, 0x01, 0x00);
            pkt.append(SunSdr::kIqPayloadSize, char(0));
            return pkt;
        };
        for (quint16 seq = 0; seq < 4; ++seq) {
            conn.feedStreamDatagramFromSenderForTest(block(seq), radio);
        }

        QTRY_COMPARE_WITH_TIMEOUT(conn.blockRepliesSentForTest(), quint64(4), 500);
        // 0 und 2 sind gerade -> blosser Kopf; 1 und 3 ungerade -> voll.
        QCOMPARE(conn.bareBlockRepliesSentForTest(), quint64(2));
        // Die letzte Nummer war 3, also ungerade, also die volle Antwort.
        QCOMPARE(conn.lastBlockReplySeqForTest(), quint16(3));
        QCOMPARE(conn.lastBlockReplyBytesForTest(),
                 SunSdr::kIqHeaderSize + SunSdr::kIqPayloadSize);
    }

    // ── Der 77-Byte-Messwertrahmen (2026-10-07) ──────────────────────
    //
    // Bis heute fiel er an der Laengenpruefung heraus: alles unter 1210
    // Byte galt als unbrauchbar. Darin stehen die EINZIGEN Messwerte,
    // die die QRP im Empfang liefert -- deshalb zeigte ExpertSDR2 eine
    // Temperatur und Longpath nichts.
    void messwertrahmenWirdNichtMehrWeggeworfen()
    {
        qunsetenv("LONGPATH_SUNSDR_BLOCKANTWORT");
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

        // Genau der Rahmen vom Draht (expert-telemetrie.pcap, 2026-10-07),
        // mit 38,0 an [15] und 28,5 an [19].
        const QByteArray rahmen = QByteArray::fromHex(
            "03ff001f00002b0f38930000001d8300"   // Kopf + Zaehler
            "0018420000e441"                     // 38,0 bei [15], 28,5 bei [19]
            "00000000000000000000803f"
            "00000010010020130500802406002013050080"
            "380b0020130500804c1000201305008060150020130500");
        QCOMPARE(rahmen.size(), 77);

        conn.feedStreamDatagramFromSenderForTest(rahmen, radio);
        QTRY_COMPARE_WITH_TIMEOUT(conn.messwertRahmenForTest(), quint64(1), 500);
        QCOMPARE(conn.messwertAForTest(), 38.0f);
        QCOMPARE(conn.messwertBForTest(), 28.5f);
    }

    // Der Weg nach OBEN, nicht nur der Entschluessler.
    //
    // Beim Gegenlesen am 2026-10-07 gefunden: die beiden vorhandenen
    // Pruefungen sehen nur in den Treiber hinein (messwertAForTest).
    // Nimmt man den emit heraus, bleiben beide gruen -- und der Betreiber
    // saehe in der Statusseite weiter nichts. Also auch das Signal selbst
    // pruefen, und zwar dass es NUR bei Aenderung kommt.
    void dieMesswerteGehenAuchHinaus()
    {
        qunsetenv("LONGPATH_SUNSDR_BLOCKANTWORT");
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

        QSignalSpy spy(&conn, &RadioConnection::deviceTemperaturesUpdated);
        QVERIFY(spy.isValid());

        auto rahmenMit = [](float a, float b) {
            QByteArray r = QByteArray::fromHex(
                "03ff001f00002b0f38930000001d8300"
                "00000000000000"
                "00000000000000000000803f"
                "00000010010020130500802406002013050080"
                "380b0020130500804c1000201305008060150020130500");
            std::memcpy(r.data() + 15, &a, sizeof(float));
            std::memcpy(r.data() + 19, &b, sizeof(float));
            return r;
        };

        conn.feedStreamDatagramFromSenderForTest(rahmenMit(43.0f, 33.0f), radio);
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 500);
        QCOMPARE(spy.at(0).at(0).toDouble(), 43.0);
        QCOMPARE(spy.at(0).at(1).toDouble(), 33.0);

        // Derselbe Wert noch dreimal: KEIN weiteres Signal. Bei 20
        // Rahmen je Sekunde waere alles andere Last ohne Inhalt.
        for (int i = 0; i < 3; ++i) {
            conn.feedStreamDatagramFromSenderForTest(rahmenMit(43.0f, 33.0f), radio);
        }
        QTest::qWait(60);
        QCOMPARE(spy.count(), 1);

        // Ein neuer Wert meldet sich.
        conn.feedStreamDatagramFromSenderForTest(rahmenMit(43.5f, 33.0f), radio);
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 500);
        QCOMPARE(spy.at(1).at(0).toDouble(), 43.5);
    }

    // -200 ist kein Messwert, sondern "gerade keiner". Am 2026-10-07 in
    // beiden Laeufen aufgetaucht, einzeln je Wert. Wer das durchreicht,
    // zeigt minus zweihundert Grad an.
    void minusZweihundertIstKeinMesswert()
    {
        qunsetenv("LONGPATH_SUNSDR_BLOCKANTWORT");
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

        auto rahmenMit = [](float a, float b) {
            QByteArray r = QByteArray::fromHex(
                "03ff001f00002b0f38930000001d8300"
                "00000000000000"
                "00000000000000000000803f"
                "00000010010020130500802406002013050080"
                "380b0020130500804c1000201305008060150020130500");
            std::memcpy(r.data() + 15, &a, sizeof(float));
            std::memcpy(r.data() + 19, &b, sizeof(float));
            return r;
        };
        // Erst ein gueltiges Paar.
        conn.feedStreamDatagramFromSenderForTest(rahmenMit(43.0f, 33.0f), radio);
        QTRY_COMPARE_WITH_TIMEOUT(conn.messwertAForTest(), 43.0f, 500);
        QCOMPARE(conn.messwertBForTest(), 33.0f);

        // Dann einer mit -200 bei A: B wird uebernommen, A bleibt stehen.
        conn.feedStreamDatagramFromSenderForTest(rahmenMit(-200.0f, 32.5f), radio);
        QTRY_COMPARE_WITH_TIMEOUT(conn.messwertBForTest(), 32.5f, 500);
        QCOMPARE(conn.messwertAForTest(), 43.0f);

        // Und einer, in dem BEIDE fehlen: nichts aendert sich.
        conn.feedStreamDatagramFromSenderForTest(rahmenMit(-200.0f, -200.0f), radio);
        QTest::qWait(50);
        QCOMPARE(conn.messwertAForTest(), 43.0f);
        QCOMPARE(conn.messwertBForTest(), 32.5f);
    }

    // Der Zustand VOR dem ersten gueltigen Wert.
    //
    // Traegt der allererste Rahmen bei A ein -200, stand A noch auf dem
    // Anfangswert 0,0 -- und der ging als "0 Grad" hinaus. Null Grad ist
    // ein plausibler Messwert und faellt niemandem auf. Am 2026-10-07
    // vom Lueckenkritiker gefunden: die -200-Behandlung war geprueft,
    // der Zustand davor nicht.
    void vorDemErstenGueltigenWertWirdNichtsGemeldet()
    {
        qunsetenv("LONGPATH_SUNSDR_BLOCKANTWORT");
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

        QSignalSpy spy(&conn, &RadioConnection::deviceTemperaturesUpdated);
        auto rahmenMit = [](float a, float b) {
            QByteArray r = QByteArray::fromHex(
                "03ff001f00002b0f38930000001d8300"
                "00000000000000"
                "00000000000000000000803f"
                "00000010010020130500802406002013050080"
                "380b0020130500804c1000201305008060150020130500");
            std::memcpy(r.data() + 15, &a, sizeof(float));
            std::memcpy(r.data() + 19, &b, sizeof(float));
            return r;
        };

        // Erster Rahmen: A fehlt. Es darf NICHTS hinausgehen -- weder
        // der Anfangswert 0,0 noch sonst etwas.
        conn.feedStreamDatagramFromSenderForTest(rahmenMit(-200.0f, 33.0f), radio);
        QTest::qWait(60);
        QCOMPARE(spy.count(), 0);

        // Sobald A da ist, geht das Paar hinaus.
        conn.feedStreamDatagramFromSenderForTest(rahmenMit(43.0f, 33.0f), radio);
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 500);
        QCOMPARE(spy.at(0).at(0).toDouble(), 43.0);
        QCOMPARE(spy.at(0).at(1).toDouble(), 33.0);
    }

    // Seit der Messung am 2026-10-05 ist die Kopfantwort die VORGABE: an
    // den Wiederholungen aendert sie nichts (207/s gegen 214/s, also
    // nichts), sie halbiert aber den Rueckweg. Diese Pruefung haelt die
    // Vorgabe fest -- ohne sie koennte sie jemand unbemerkt zurueckdrehen.
    void ohneSchalterIstDieKopfantwortAn()
    {
        qunsetenv("LONGPATH_SUNSDR_BLOCKANTWORT");
        qunsetenv("LONGPATH_SUNSDR_KOPFANTWORT");
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

        auto block = [](quint16 seq) {
            QByteArray pkt = SunSdr::buildIqHeader(
                SunSdr::kProfileQrp, SunSdr::kOpIqRxIdle, seq, 0x01, 0x00);
            pkt.append(SunSdr::kIqPayloadSize, char(0));
            return pkt;
        };
        for (quint16 seq = 0; seq < 4; ++seq) {
            conn.feedStreamDatagramFromSenderForTest(block(seq), radio);
        }

        QTRY_COMPARE_WITH_TIMEOUT(conn.blockRepliesSentForTest(), quint64(4), 500);
        QCOMPARE(conn.bareBlockRepliesSentForTest(), quint64(2));
    }

    // Und sie laesst sich abschalten -- der Rueckweg ist dann wieder
    // durchgehend der volle Block.
    void mitNullBleibtJedeAntwortDerVolleBlock()
    {
        qunsetenv("LONGPATH_SUNSDR_BLOCKANTWORT");
        qputenv("LONGPATH_SUNSDR_KOPFANTWORT", "0");
        auto restore = qScopeGuard([] { qunsetenv("LONGPATH_SUNSDR_KOPFANTWORT"); });
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

        auto block = [](quint16 seq) {
            QByteArray pkt = SunSdr::buildIqHeader(
                SunSdr::kProfileQrp, SunSdr::kOpIqRxIdle, seq, 0x01, 0x00);
            pkt.append(SunSdr::kIqPayloadSize, char(0));
            return pkt;
        };
        for (quint16 seq = 0; seq < 4; ++seq) {
            conn.feedStreamDatagramFromSenderForTest(block(seq), radio);
        }

        QTRY_COMPARE_WITH_TIMEOUT(conn.blockRepliesSentForTest(), quint64(4), 500);
        QCOMPARE(conn.bareBlockRepliesSentForTest(), quint64(0));
        QCOMPARE(conn.lastBlockReplyBytesForTest(),
                 SunSdr::kIqHeaderSize + SunSdr::kIqPayloadSize);
    }

    // ── Pegelabgleich QRP, neu gemessen am 2026-10-04 ──────────────
    //
    // Bis dahin standen hier +20,0 dB -- am 2026-09-25 gegen ExpertSDR2
    // gemessen, aber ueber den TCI-Weg (rx_sensors). Der NATIVE Treiber
    // ist ein anderer Weg mit anderer Skalierung, und dort reichten die
    // 20 dB bei Weitem nicht: "ich muss voll aufdrehen, dass ich etwas
    // hoere" / "bei der haelfte, sprich 50 % faengt man an, etwas zu
    // hoeren", waehrend ExpertSDR2 am SELBEN Geraet ohne Antenne
    // "perfekt" laut war -- es lag also nicht an der fehlenden Antenne.
    //
    // +40,0 dB (Faktor 100) hat der Betreiber am 2026-10-04 am echten
    // Geraet selbst eingestellt und bestaetigt: "die lautstaerke passt".
    // Im Log seines Laufs steht "SunSdr: Pegelabgleich 40.0 dB
    // (eingestellt)". Seine Anforderung dazu: "rauschen muss immer zu
    // hoeren sein".
    void qrpSamplesAreRaisedByTheMeasuredFortyDb()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        QCOMPARE(conn.rxLevelGainForTest(), 100.0f);   // 10^(40/20)

        const QHostAddress radio(QStringLiteral("192.0.2.200"));
        conn.feedControlDatagramForTest(
            QByteArray::fromHex("03ff011a7c0000004119c0a810c8c0a810c851c300004928"),
            radio);
        QSignalSpy iqSpy(&conn, &RadioConnection::iqDataReceived);

        // Erster Slot: Q = 500 (Bytes 0-2), I = 1000 (Bytes 3-5), 24 Bit LE.
        QByteArray pkt = SunSdr::buildIqHeader(
            SunSdr::kProfileQrp, SunSdr::kOpIqRxIdle, /*seq=*/1, 0x01, 0x00);
        QByteArray payload(SunSdr::kIqPayloadSize, char(0));
        payload[0] = char(500 & 0xff); payload[1] = char((500 >> 8) & 0xff);
        payload[3] = char(1000 & 0xff); payload[4] = char((1000 >> 8) & 0xff);
        pkt.append(payload);
        conn.feedStreamDatagramFromSenderForTest(pkt, radio);

        QTRY_COMPARE_WITH_TIMEOUT(iqSpy.count(), 1, 500);
        const auto samples = iqSpy.first().at(1).value<QVector<float>>();
        QVERIFY(samples.size() >= 2);
        const float raw = 1.0f / 8388608.0f;   // 1 / 2^23
        QVERIFY(qFuzzyCompare(samples[0], 1000.0f * raw * 100.0f));   // I
        QVERIFY(qFuzzyCompare(samples[1],  500.0f * raw * 100.0f));   // Q
    }

    // Preamp/Abschwaecher 0x04, am 2026-09-25 in ExpertSDR2 der Reihe
    // nach durchgeschaltet: 00/01/02/03 = -20/-10/0/+10 dB. Die alte
    // Zuordnung schickte fuer "0 dB" die -20-dB-Stufe.
    void attenuatorFramesFollowTheMeasuredOrder()
    {
        QVERIFY(SunSdrRadioConnection::attenuatorFrameFor(0).toHex().endsWith("02000000"));
        QVERIFY(SunSdrRadioConnection::attenuatorFrameFor(-10).toHex().endsWith("01000000"));
        QVERIFY(SunSdrRadioConnection::attenuatorFrameFor(-20).toHex().endsWith("00000000"));
        QVERIFY(SunSdrRadioConnection::attenuatorFrameFor(10).isEmpty());
        QVERIFY(SunSdrRadioConnection::attenuatorFrameFor(-30).isEmpty());
        // ExpertSDR2s eigener Startrahmen, byte-genau.
        QCOMPARE(SunSdrRadioConnection::attenuatorFrameFor(0),
                 QByteArray::fromHex("03ff04000400000000000100000053ccd3b302000000"));
    }

    // Preamp-Schalter (PreampMode-Index -> Opcode 0x04): die vier Rahmen
    // entstehen aus Kopf + Stufe + CRC-32 und muessen byte-genau die
    // mitgeschnittenen ExpertSDR2-Rahmen vom 2026-09-25 sein.
    void preampFramesMatchExpertSdr2Bytes()
    {
        // 7 = Plus10, 1 = On (0 dB), 2 = Minus10, 3 = Minus20
        QCOMPARE(SunSdrRadioConnection::preampFrameFor(7).toHex(),
                 QByteArray("03ff04000400000000000100000036ab6f0b03000000"));
        QCOMPARE(SunSdrRadioConnection::preampFrameFor(1).toHex(),
                 QByteArray("03ff04000400000000000100000053ccd3b302000000"));
        QCOMPARE(SunSdrRadioConnection::preampFrameFor(2).toHex(),
                 QByteArray("03ff040004000000000001000000bd6366a101000000"));
        QCOMPARE(SunSdrRadioConnection::preampFrameFor(3).toHex(),
                 QByteArray("03ff040004000000000001000000d804da1900000000"));
        // Off heisst bei Thetis -20 dB (HPSDR_OFF) -> derselbe Rahmen.
        QCOMPARE(SunSdrRadioConnection::preampFrameFor(0),
                 SunSdrRadioConnection::preampFrameFor(3));
        // -30..-50 dB und Unsinn gibt es an der QRP nicht.
        QVERIFY(SunSdrRadioConnection::preampFrameFor(4).isEmpty());
        QVERIFY(SunSdrRadioConnection::preampFrameFor(6).isEmpty());
        QVERIFY(SunSdrRadioConnection::preampFrameFor(-1).isEmpty());
        QVERIFY(SunSdrRadioConnection::preampFrameFor(8).isEmpty());
        // Die Abschwaecher-Rahmen sind dieselben Bytes.
        QCOMPARE(SunSdrRadioConnection::preampFrameFor(1),
                 SunSdrRadioConnection::attenuatorFrameFor(0));
        QCOMPARE(SunSdrRadioConnection::preampFrameFor(2),
                 SunSdrRadioConnection::attenuatorFrameFor(-10));
    }

    // Ohne offene Sitzung schickt der Schalter nichts (und stuerzt nicht).
    void preampWithoutSessionSendsNothing()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        QVERIFY(!conn.hasRadioAddrForTest());
        const double before = conn.txByteRate(5000);
        conn.setPreampModeIndex(7);
        QCOMPARE(conn.txByteRate(5000), before);
    }

    // 0x07 = DDC-Frequenz je Unterempfaenger; der RX1-Rahmen schaltet die
    // QRP auf echtes I/Q (am Geraet eingegrenzt 2026-09-25). Byte-genau
    // gegen ExpertSDR2s eigene Rahmen von diesem Tag.
    void ddcFrameMatchesExpertSdr2Bytes()
    {
        QCOMPARE(SunSdrRadioConnection::ddcFrequencyFrame(0, 14224010).toHex(),
                 QByteArray("03ff070008000000000001000000dabdabb764697a0800000000"));
        QCOMPARE(SunSdrRadioConnection::ddcFrequencyFrame(1, 1905000).toHex(),
                 QByteArray("03ff0700080001000000010000001850a11e10ae220100000000"));
    }

    // Andere Frequenz -> andere Pruefsumme (CRC-32 ueber den Rahmen). Mit
    // dem alten Ende verwarf die QRP den Rahmen (Versuch "07x").
    void ddcFrameCarriesTheCrcOfItsOwnFrequency()
    {
        QCOMPARE(SunSdrRadioConnection::ddcFrequencyFrame(0, 7328400).toHex(),
                 QByteArray("03ff07000800000000000100000081b75f33a0395e0400000000"));
    }

    // Nach dem Einschalten liefert die QRP nur einen Kanal (Q = 0). Die
    // DIAG-Sekunde erkennt das.
    void singleChannelStateIsRecognised()
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

        auto onlyI = [](quint16 seq) {
            QByteArray pkt = SunSdr::buildIqHeader(
                SunSdr::kProfileQrp, SunSdr::kOpIqRxIdle, seq, 0x01, 0x00);
            QByteArray payload(SunSdr::kIqPayloadSize, char(0));
            for (int k = 0; k < SunSdr::kIqPayloadSize; k += 6) {
                payload[k + 3] = char(7);   // I traegt Daten, Q (Bytes 0-2) bleibt 0
            }
            pkt.append(payload);
            return pkt;
        };
        conn.feedStreamDatagramFromSenderForTest(onlyI(1), radio);
        QTest::qWait(1100);
        conn.feedStreamDatagramFromSenderForTest(onlyI(2), radio);
        QVERIFY(conn.singleChannelSeenForTest());
    }

    // Frisch eingeschaltet (nur I) war der Ton rund eine Sekunde laut, bis
    // 0x07 echtes I/Q einschaltete (Betreiber 2026-09-26: "war kurz ganz
    // laut"). Bis zum ersten Block mit Q geht Stille weiter; danach laeuft
    // alles durch, auch ein spaeterer Block ohne Q.
    void startupSingleChannelIsSilentUntilIqArrives()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        conn.setRxReadyForTest(true);
        QSignalSpy iqSpy(&conn, &RadioConnection::iqDataReceived);

        conn.feedStreamDatagramForTest(qrpBlock(1, /*withQ=*/false));
        QCOMPARE(iqSpy.count(), 1);
        QCOMPARE(peakOf(iqSpy.last()), 0.0);        // nur I -> Stille
        conn.feedStreamDatagramForTest(qrpBlock(2, /*withQ=*/true));
        QVERIFY(peakOf(iqSpy.last()) > 0.0);        // I/Q -> durch
        conn.feedStreamDatagramForTest(qrpBlock(3, /*withQ=*/false));
        QVERIFY(peakOf(iqSpy.last()) > 0.0);        // danach nie mehr gesperrt
    }

    // Bleibt die QRP einkanalig (0x07 kam nicht an), laeuft das Signal nach
    // der Frist wie frueher durch -- lieber falsch als gar nichts, die
    // Einkanal-Warnung sagt dann warum.
    void singleChannelPassesAfterTheHold()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        conn.setRxReadyForTest(true);
        conn.setSingleChannelHoldMsForTest(100);
        QSignalSpy iqSpy(&conn, &RadioConnection::iqDataReceived);

        conn.feedStreamDatagramForTest(qrpBlock(1, /*withQ=*/false));
        QCOMPARE(iqSpy.count(), 1);
        QCOMPARE(peakOf(iqSpy.last()), 0.0);
        QTest::qWait(150);
        conn.feedStreamDatagramForTest(qrpBlock(2, /*withQ=*/false));
        QVERIFY(peakOf(iqSpy.last()) > 0.0);
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
        // Diese Linie prueft den FEHLSCHLAG. Seit dem 2026-10-04
        // wiederholt die Verbindung die Suche von selbst (das Geraet
        // sperrt nach einem abrupten Ende rund eine Minute) -- hier
        // soll sie das nicht, sonst wartet der Pruefstand 90 s.
        SunSdrRadioConnection conn;
        conn.setSucheWiederholungEnabledForTest(false);
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

    // ── Mithoeren: was das Geraet von sich aus meldet ──────────────────
    //
    // Bis zum 2026-10-02 war dieser Treiber auf der Meldeseite taub:
    // processControlDatagram() stieg nach dem Handschlag mit "drain only"
    // aus, und im Stromkopf blieben Opcode und Zustandsbytes ungelesen.
    // Diese Pruefungen halten beides fest -- einschliesslich der zwei
    // Faelle, die beim Nachbauen am leichtesten verloren gehen: der
    // fremde Absender und der TX-aktive Rahmen, der fuer das I/Q
    // verworfen, fuers Inventar aber gezaehlt wird.

    // Ein gueltiger Steuerrahmen, wie das Geraet ihn im Betrieb schickt:
    // 18-Byte-Kopf plus Nutzlast, mit dem Magic der QRP.
    static QByteArray qrpControlFrame(quint8 opcode, quint16 sub,
                                      const QByteArray& payload)
    {
        QByteArray frame = SunSdr::buildControlHeader(
            SunSdr::kProfileQrp, opcode, sub, quint16(payload.size()));
        frame.append(payload);
        return frame;
    }

    // Bringt die Verbindung in denselben Zustand wie
    // realBeaconReplyOpensGateAndRepliesWithStateSync(): Handschlag durch,
    // m_radioAddr gesetzt, RX-Tor offen.
    static QHostAddress handshake(SunSdrRadioConnection& conn)
    {
        const QHostAddress radio(QStringLiteral("192.0.2.200"));  // RFC 5737
        conn.feedControlDatagramForTest(
            QByteArray::fromHex("03ff011a7c0000004119c0a810c8c0a810c851c300004928"),
            radio);
        return radio;
    }

    void steuerrahmenNachDemHandschlagKommenInsInventar()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        const QHostAddress radio = handshake(conn);
        QVERIFY(conn.isRxReadyForTest());

        // Der Beacon selbst gehoert nicht ins Inventar — er lief durch den
        // Handschlag-Zweig, nicht durch das Mithoeren.
        QCOMPARE(conn.controlFrameKindsForTest(), 0);

        conn.feedControlDatagramForTest(
            qrpControlFrame(0x0d, 0, QByteArray::fromHex("01000000")), radio);

        QCOMPARE(conn.controlFramesSeenForTest(), quint64(1));
        QCOMPARE(conn.controlFrameKindsForTest(), 1);
        const QString bericht = conn.frameInventoryReport();
        QVERIFY2(bericht.contains(QStringLiteral("op=0x0d")), qPrintable(bericht));
        QVERIFY2(bericht.contains(QStringLiteral("01000000")), qPrintable(bericht));
    }

    // Derselbe Opcode mit anderer Nutzlast ist KEINE neue Sorte, sondern
    // eine Aenderung — das ist die Unterscheidung, an der sich ein
    // Messwert von einer Ausstattungsmeldung erkennen laesst.
    void geaenderteNutzlastZaehltAlsAenderungNichtAlsNeueSorte()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        const QHostAddress radio = handshake(conn);

        conn.feedControlDatagramForTest(
            qrpControlFrame(0x0d, 0, QByteArray::fromHex("01000000")), radio);
        conn.feedControlDatagramForTest(
            qrpControlFrame(0x0d, 0, QByteArray::fromHex("02000000")), radio);

        QCOMPARE(conn.controlFrameKindsForTest(), 1);
        QCOMPARE(conn.controlFramesSeenForTest(), quint64(2));
        const QString bericht = conn.frameInventoryReport();
        QVERIFY2(bericht.contains(QStringLiteral("Aenderungen 1")), qPrintable(bericht));
        QVERIFY2(bericht.contains(QStringLiteral("erste 01000000 letzte 02000000")),
                 qPrintable(bericht));
    }

    // Der Fall, fuer den die Werteliste da ist: vier Stufen am
    // Vorverstaerker, am Ende steht wieder der Anfangswert. Erste und
    // letzte Nutzlast allein wuerden "keine Aenderung" suggerieren.
    void alleVerschiedenenWerteStehenImBericht()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        const QHostAddress radio = handshake(conn);

        for (const char* hex : {"00000000", "01000000", "02000000",
                                "03000000", "00000000"}) {
            conn.feedControlDatagramForTest(
                qrpControlFrame(0x05, 0, QByteArray::fromHex(hex)), radio);
        }

        const QString bericht = conn.frameInventoryReport();
        QCOMPARE(conn.controlFrameKindsForTest(), 1);
        QVERIFY2(bericht.contains(QStringLiteral(
                     "Werte: 00000000, 01000000, 02000000, 03000000")),
                 qPrintable(bericht));
        // Vier Stufen hin und eine zurueck sind vier Aenderungen.
        QVERIFY2(bericht.contains(QStringLiteral("Aenderungen 4")),
                 qPrintable(bericht));
        // Und der Beleg, dass erste/letzte allein getaeuscht haetten:
        QVERIFY2(bericht.contains(QStringLiteral("erste 00000000 letzte 00000000")),
                 qPrintable(bericht));
    }

    // Mehr verschiedene Werte als die Liste traegt: dann steht "..." dahinter
    // und die Zahl der Aenderungen traegt die Aussage.
    void zuVieleWerteWerdenAbgekuerzt()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        const QHostAddress radio = handshake(conn);

        for (int i = 0; i < 12; ++i) {
            QByteArray p(4, char(0));
            p[0] = char(i);
            conn.feedControlDatagramForTest(qrpControlFrame(0x0f, 0, p), radio);
        }

        const QString bericht = conn.frameInventoryReport();
        QVERIFY2(bericht.contains(QStringLiteral("...")), qPrintable(bericht));
        QVERIFY2(bericht.contains(QStringLiteral("Aenderungen 11")), qPrintable(bericht));
    }

    // Gleiche Begruendung wie bei processStreamDatagram()s Absenderpruefung:
    // der Steuerport wird mit ShareAddress gebunden, eine noch laufende
    // Vorsitzung derselben QRP darf das Inventar nicht mit fuellen.
    void fremderAbsenderKommtNichtInsInventar()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        conn.feedControlDatagramForTest(
            qrpControlFrame(0x0d, 0, QByteArray::fromHex("01000000")),
            QHostAddress(QStringLiteral("192.0.2.111")));

        QCOMPARE(conn.controlFramesSeenForTest(), quint64(0));
        QCOMPARE(conn.controlFrameKindsForTest(), 0);
    }

    // Die Zustandsbytes [8:9] des Stromkopfs. Zwei Bloecke mit
    // verschiedenen Bytes sind eine Sorte mit einer Aenderung.
    void zustandsbytesAusDemStromKommenInsInventar()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        conn.feedStreamDatagramForTest(qrpBlock(1, true));
        QCOMPARE(conn.streamStateKindsForTest(), 1);

        QByteArray andererZustand = SunSdr::buildIqHeader(
            SunSdr::kProfileQrp, SunSdr::kOpIqRxIdle, 2, 0x02, 0x01);
        andererZustand.append(QByteArray(SunSdr::kIqPayloadSize, char(0)));
        conn.feedStreamDatagramForTest(andererZustand);

        QCOMPARE(conn.streamStateKindsForTest(), 1);
        const QString bericht = conn.frameInventoryReport();
        QVERIFY2(bericht.contains(QStringLiteral("Strom op=0xfe")), qPrintable(bericht));
        QVERIFY2(bericht.contains(QStringLiteral("erste 0100 letzte 0201")),
                 qPrintable(bericht));
    }

    // Ein TX-aktiver Rahmen (0xFD) wird fuer das I/Q verworfen — er traegt
    // kein Empfangssignal. Fuers Inventar zaehlt er trotzdem: dass das
    // Geraet ueberhaupt in den Sendezustand gegangen ist, ist genau die
    // Meldung, die dieser Treiber bisher nicht gesehen hat.
    void txAktiverRahmenZaehltObwohlErFuersIqVerworfenWird()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        QSignalSpy iq(&conn, &RadioConnection::iqDataReceived);
        QByteArray txRahmen = SunSdr::buildIqHeader(
            SunSdr::kProfileQrp, SunSdr::kOpIqTxActive, 1, 0x02, 0x01);
        txRahmen.append(QByteArray(SunSdr::kIqPayloadSize, char(0)));
        conn.feedStreamDatagramForTest(txRahmen);

        QCOMPARE(iq.count(), 0);
        QCOMPARE(conn.streamStateKindsForTest(), 1);
        QVERIFY2(conn.frameInventoryReport().contains(QStringLiteral("Strom op=0xfd")),
                 qPrintable(conn.frameInventoryReport()));
    }

    // Das Inventar gehoert zur Sitzung, gleiche Begruendung wie bei
    // m_radioAddr: eine Sorte aus der vorigen Verbindung darf im Bericht
    // der neuen nicht als "schon gesehen" dastehen.
    void inventarBeginntMitJederVerbindungNeu()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        const QHostAddress radio = handshake(conn);
        conn.feedControlDatagramForTest(
            qrpControlFrame(0x0d, 0, QByteArray::fromHex("01000000")), radio);
        QCOMPARE(conn.controlFrameKindsForTest(), 1);

        conn.disconnect();
        conn.connectToRadio(someQrpInfo());

        QCOMPARE(conn.controlFrameKindsForTest(), 0);
        QCOMPARE(conn.controlFramesSeenForTest(), quint64(0));
        QVERIFY(conn.frameInventoryReport().contains(
            QStringLiteral("nichts aufgenommen")));
    }

    // Der Fall, fuer den das Mithoeren gebaut ist: das Geraet schaltet
    // sich ab (Akku leer, Stecker weg). Das endet NICHT ueber disconnect(),
    // sondern ueber den Stillstands-Wachhund -- und die Uebersicht muss
    // trotzdem ins Log, sonst ist die ganze Sammelarbeit genau in dem Lauf
    // verloren, fuer den sie gedacht war.
    void geraeteausfallSchreibtDieUebersichtTrotzdem()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        const QHostAddress radio = handshake(conn);
        conn.feedControlDatagramForTest(
            qrpControlFrame(0x0d, 0, QByteArray::fromHex("01000000")), radio);
        QCOMPARE(conn.controlFrameKindsForTest(), 1);

        // Erst ein Strompaket: ohne das kommt die Verbindung nie bis
        // Connected, und dann greift der Stillstands-Wachhund gar nicht --
        // es feuert der Verbindungs-Wachhund. Zwei verschiedene Abbrueche,
        // und nur der erste ist "das Geraet war da und ist weg".
        conn.feedStreamDatagramForTest(silentIqPacket());
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 500);

        // Keine Pakete mehr -- der Wachhund laeuft ab und bricht ab.
        QTRY_COMPARE_WITH_TIMEOUT(
            conn.state(), ConnectionState::LinkLost,
            SunSdrRadioConnection::dataSilenceTimeoutMsForTest() + 2000);

        // Der Beleg: die Uebersicht war fertig, BEVOR die Sitzung geraeumt
        // wurde -- das Inventar steht also noch, und der Bericht ist
        // geschrieben. Zweimal darf er nicht kommen, auch wenn der
        // Betreiber danach noch disconnect() nachschiebt.
        QVERIFY(conn.inventoryReportedForTest());
        conn.disconnect();
        QVERIFY(conn.inventoryReportedForTest());
    }

    // ── Werkbank-Rahmen: was hinausgeht, muss das sein, was dastand ────
    //
    // Der Versuch mit dem Verbindungsablauf (siehe
    // docs/architecture/2026-10-02-sunsdr-verbindungsablauf.md) haengt an
    // dieser Schnittstelle. QByteArray::fromHex() ueberspringt ungueltige
    // Zeichen STILL -- ein verrutschtes Zeichen ergaebe einen anderen,
    // kuerzeren Rahmen, und der ginge ans Funkgeraet, ohne dass es
    // irgendwo steht.

    void werkbankRahmenMitKaputtemHexGehenNichtHinaus()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());

        // "xx" ist kein Hex, "03ff0" hat ungerade Laenge -- beide muessen
        // wortlos liegenbleiben, nicht halb hinausgehen.
        qputenv("LONGPATH_SUNSDR_PRE", "03ffxx0004000000000001000000a444f1b700000000,03ff0");
        handshake(conn);
        qunsetenv("LONGPATH_SUNSDR_PRE");

        QCOMPARE(conn.benchFramesSentForTest(), 0u);
        QCOMPARE(conn.benchFramesRejectedForTest(), 2u);
    }

    void werkbankRahmenMitSauberemHexGehtHinaus()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());

        // Einer der dreizehn echten ExpertSDR2-Rahmen (0x10), mit
        // richtiger Pruefsumme -- siehe tst_sunsdr_protocol.cpp.
        qputenv("LONGPATH_SUNSDR_PRE",
                "03ff100004000000000001000000a444f1b700000000");
        handshake(conn);
        qunsetenv("LONGPATH_SUNSDR_PRE");

        QCOMPARE(conn.benchFramesSentForTest(), 1u);
        QCOMPARE(conn.benchFramesRejectedForTest(), 0u);
    }

    // ── Quittungen ─────────────────────────────────────────────────────
    //
    // Am 2026-10-03 am Geraet gemessen: die QRP quittiert jeden Rahmen, den
    // sie annimmt, mit demselben Opcode und leerer Nutzlast, binnen 15 bis
    // 50 ms. Bis dahin schickte dieser Treiber jeden Befehl ins Blaue.

    void quittungWirdDemGesendetenRahmenZugeordnet()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        const QHostAddress radio = handshake(conn);

        // Der Handschlag hat den Zustandsrahmen hinausgeschickt, und der
        // traegt Opcode 0x01 (SUNSDR_OP_STATE_SYNC) -- nicht 0x08. Diese
        // Verwechslung hat mich am 2026-10-03 eine falsche Behauptung
        // gekostet ("Longpath schickt den Stromstart-Rahmen gar nicht"):
        // stateSyncFrameForTest() ist bitgleich mit dem 0x01-Rahmen aus
        // dem ExpertSDR2-Mitschnitt, Longpath schickt ihn also laengst.
        QVERIFY(conn.offeneRahmenForTest() >= 1);
        QCOMPARE(conn.quittungenGesehenForTest(), quint64(0));

        conn.feedControlDatagramForTest(
            qrpControlFrame(0x01, 0, QByteArray()), radio);

        QCOMPARE(conn.quittungenGesehenForTest(), quint64(1));
        QCOMPARE(conn.rahmenOhneQuittungForTest(), quint64(0));
    }

    // Eine Quittung mit anderem Opcode darf den offenen Rahmen nicht
    // schliessen -- sonst zaehlt der Zaehler irgendetwas, nicht die Sache.
    void fremdeQuittungSchliesstDenOffenenRahmenNicht()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        const QHostAddress radio = handshake(conn);
        const int offenVorher = conn.offeneRahmenForTest();
        QVERIFY(offenVorher >= 1);

        conn.feedControlDatagramForTest(
            qrpControlFrame(0x16, 0, QByteArray()), radio);

        QCOMPARE(conn.quittungenGesehenForTest(), quint64(0));
        QCOMPARE(conn.offeneRahmenForTest(), offenVorher);
    }

    // Beim Trennen geht ein Stopp hinaus -- bis zum 2026-10-03 schickte
    // dieser Treiber GAR NICHTS, und die QRP streamte danach unbegrenzt
    // weiter (1940 Pakete/s ins Leere, bis zum Ausschalten). Mit dem Stopp
    // am Geraet gemessen: 0 Pakete/s.
    void trennenSchicktDenStopp()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);
        QCOMPARE(conn.stoppGeschicktForTest(), quint64(0));

        conn.disconnect();
        // Mindestens einer. Wie viele es werden, wenn niemand quittiert,
        // sagt bleibtDerStoppUnquittiertWirdErNachgeschickt().
        QVERIFY(conn.stoppGeschicktForTest() >= quint64(1));
    }

    // Der Vorfall vom 2026-10-04: der Betreiber konnte eine halbe Stunde
    // lang nicht mehr verbinden ("no beacon reply"), obwohl Geraet, Netz,
    // Einstellungen und Programmfassung einzeln geprueft in Ordnung waren.
    // Ursache: Pruefinstanzen waren hart beendet worden, und in einem Lauf
    // stand im Log
    //
    //   WRN: SunSdr: beim Verbindungsende noch unquittiert: 0x02
    //
    // Die QRP bedient EINEN Client und haelt die Sitzung fest. Kommt der
    // Stopp nicht an, bleibt sie an den Toten gebunden und antwortet auf
    // neue Suchmeldungen nicht mehr. Erst Aus- und Einschalten half.
    //
    // Der Stopp wird deshalb nachgeschickt, solange er unquittiert bleibt.
    // Er ist eine reine Abmeldung und mehrfach unschaedlich -- anders als
    // ein Rahmen, der etwas verstellt.
    void bleibtDerStoppUnquittiertWirdErNachgeschickt()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        // Im Pruefstand antwortet niemand -- genau der Fall, der das
        // Geraet haengen laesst.
        conn.disconnect();
        QVERIFY2(conn.stoppGeschicktForTest() >= quint64(2),
                 qPrintable(QStringLiteral("nur %1 Stopp-Rahmen geschickt")
                                .arg(conn.stoppGeschicktForTest())));
    }

    // Ohne stehende Verbindung gibt es keine Gegenstelle -- ein Stopp an
    // eine Adresse, die wir nicht kennen, waere ein Paket ins Nichts.
    void trennenOhneVerbindungSchicktKeinenStopp()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        // KEIN Handschlag -- m_awaitingBeacon bleibt true.
        conn.disconnect();
        QCOMPARE(conn.stoppGeschicktForTest(), quint64(0));
    }

    // Ein unquittierter Frequenzrahmen wird EINMAL nachgeschickt -- am
    // 2026-10-03 am Geraet beobachtet, dass einer verloren ging, und die
    // Folge ist nicht harmlos: das Geraet steht dann auf einer anderen
    // Frequenz als Longpath anzeigt.
    void unquittierterRahmenWirdEinmalNachgeschickt()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);
        // Der Zustandsrahmen 0x01 ist hinaus und wartet auf seine Quittung.
        QVERIFY(conn.offeneRahmenForTest() >= 1);

        // Keine Quittung. Nach der Frist muss genau EINE Wiederholung
        // kommen -- und danach eine Meldung, keine zweite Wiederholung.
        QTRY_VERIFY_WITH_TIMEOUT(conn.rahmenWiederholtForTest() >= 1, 8000);
        QCOMPARE(conn.rahmenWiederholtForTest(), quint64(1));

        // Und beim Verbindungsende wird gesagt, was offen blieb. Ohne das
        // fiel es stumm unter den Tisch, weil die Quittungspruefung am
        // Stillstands-Wachhund haengt und der beim Abbruch stoppt.
        conn.disconnect();
        QVERIFY(conn.rahmenOhneQuittungForTest() >= 1);
        QCOMPARE(conn.rahmenWiederholtForTest(), quint64(1));
    }

    // Werkbank-Rahmen werden NICHT nachgeschickt: was dort hinausgeht,
    // entscheidet der Mensch davor, und ein Treiber, der dessen Versuche
    // verdoppelt, faelscht das Ergebnis.
    void werkbankRahmenWirdNichtNachgeschickt()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());

        qputenv("LONGPATH_SUNSDR_PRE",
                "03ff100004000000000001000000a444f1b700000000");
        handshake(conn);
        qunsetenv("LONGPATH_SUNSDR_PRE");
        QCOMPARE(conn.benchFramesSentForTest(), 1u);

        // Warten, bis die Frist durch ist, dann die Sitzung beenden --
        // dabei wird gemeldet, was offen blieb.
        QTRY_VERIFY_WITH_TIMEOUT(conn.rahmenWiederholtForTest() >= 1, 8000);
        conn.disconnect();
        QVERIFY(conn.rahmenOhneQuittungForTest() >= 1);
        // Der Werkbank-Rahmen 0x10 darf nicht wiederholt worden sein; nur
        // der Zustandsrahmen 0x01 darf das, und auch der nur einmal.
        QVERIFY2(conn.rahmenWiederholtForTest() <= 1,
                 "Ein Werkbank-Rahmen wurde nachgeschickt");
    }


    // ── Zwei Stroeme und mehrere Pakete je Folgenummer ─────────────────
    //
    // Am 2026-10-03 aus einem ExpertSDR2-Mitschnitt gemessen: die QRP
    // schickt bei umgestelltem 0x01-Rahmen ZWEI Stroeme (byte8=2, byte9
    // als Index) mit verschiedenen Raten, und ein Strom ueber 48 kHz
    // traegt mehrere Pakete je Nummer -- mit VERSCHIEDENEN Proben.

    static QByteArray qrpBlockKanal(quint16 seq, int stroeme, int kanal,
                                    char fuellwert)
    {
        QByteArray pkt = SunSdr::buildIqHeader(
            SunSdr::kProfileQrp, SunSdr::kOpIqRxIdle, seq,
            quint8(stroeme), quint8(kanal));
        QByteArray payload(SunSdr::kIqPayloadSize, char(0));
        for (int k = 0; k < SunSdr::kIqPayloadSize; k += 6) {
            payload[k + 3] = fuellwert;      // I
            payload[k + 0] = char(5);        // Q, damit echtes I/Q gilt
        }
        pkt.append(payload);
        return pkt;
    }

    void zweiStroemeLandenAufVerschiedenenKanaelen()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        conn.setSingleChannelHoldMsForTest(0);
        handshake(conn);

        QSignalSpy iq(&conn, &RadioConnection::iqDataReceived);
        // So kommt es am Geraet wirklich (2026-10-03 gemessen): die Nummern
        // laufen GLOBAL fortlaufend, byte9 wechselt dabei den Strom.
        // Die erste Fassung dieses Tests nahm an, beide Kanaele traegen
        // dieselbe Nummer -- der Versuch am Geraet hat das widerlegt, und
        // der Zaehler meldete daraufhin 50 % Verlust bei gesundem Strom.
        // Erst mit EINEM Empfaenger: der zweite Strom wird verworfen, statt
        // nach oben zu gehen, wo er bestenfalls ignoriert und
        // schlimmstenfalls mit Kanal 0 vermischt wuerde.
        conn.feedStreamDatagramForTest(qrpBlockKanal(1, 2, 0, char(7)));
        conn.feedStreamDatagramForTest(qrpBlockKanal(2, 2, 1, char(9)));
        QCOMPARE(iq.count(), 1);
        QCOMPARE(iq.at(0).at(0).toInt(), 0);
        QCOMPARE(conn.kanalVerworfenForTest(1), quint64(1));

        // Und jetzt mit zwei: beide gehen durch, jeder auf seinen Kanal.
        conn.setActiveReceiverCount(2);
        conn.feedStreamDatagramForTest(qrpBlockKanal(3, 2, 0, char(7)));
        conn.feedStreamDatagramForTest(qrpBlockKanal(4, 2, 1, char(9)));

        QCOMPARE(iq.count(), 3);
        QCOMPARE(iq.at(1).at(0).toInt(), 0);
        QCOMPARE(iq.at(2).at(0).toInt(), 1);
        QCOMPARE(conn.kanalPaketeForTest(0), quint64(2));
        QCOMPARE(conn.kanalPaketeForTest(1), quint64(2));
        // Lueckenlos im globalen Nummernraum, und zwar EINSCHLIESSLICH der
        // Nummer des verworfenen Pakets: die Nummern laufen global, also
        // muss jede gezaehlt werden, auch wenn ihre Proben niemand braucht.
        // Andernfalls meldet der Zaehler Verlust, wo keiner ist -- am
        // 2026-10-03 im Messlauf zweimal passiert.
        QCOMPARE(conn.seqRepeatsForTest(), quint64(0));
        QCOMPARE(conn.seqLostForTest(), quint64(0));
        QCOMPARE(conn.seqFramesForTest(), quint64(4));
    }

    // Zwei Pakete mit derselben Nummer, aber VERSCHIEDENEM Inhalt: das ist
    // die zweite Haelfte der Proben (96 kHz), keine Wiederholung. Beide
    // muessen durchgehen.
    void zweitesPaketMitAnderemInhaltIstFortsetzung()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        conn.setSingleChannelHoldMsForTest(0);
        handshake(conn);

        QSignalSpy iq(&conn, &RadioConnection::iqDataReceived);
        conn.feedStreamDatagramForTest(qrpBlockKanal(5, 2, 0, char(7)));
        conn.feedStreamDatagramForTest(qrpBlockKanal(5, 2, 0, char(11)));

        QCOMPARE(iq.count(), 2);
        QCOMPARE(conn.kanalFortsetzungenForTest(0), quint64(1));
        QCOMPARE(conn.seqRepeatsForTest(), quint64(0));
        QCOMPARE(conn.seqLostForTest(), quint64(0));
    }

    // Gegenprobe, und der heutige Normalfall: zwei Pakete mit derselben
    // Nummer und BYTEGLEICHEM Inhalt sind eine Wiederholung (am
    // 2026-09-23 am Geraet belegt: 1683 von 1683 ganz bytegleich).
    void zweitesPaketMitGleichemInhaltBleibtWiederholung()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        conn.setSingleChannelHoldMsForTest(0);
        handshake(conn);

        conn.feedStreamDatagramForTest(qrpBlockKanal(5, 1, 0, char(7)));
        conn.feedStreamDatagramForTest(qrpBlockKanal(5, 1, 0, char(7)));

        QCOMPARE(conn.kanalFortsetzungenForTest(0), quint64(0));
        QCOMPARE(conn.seqRepeatsForTest(), quint64(1));
    }

    // Und das Wichtigste: am heutigen Betrieb aendert sich nichts. Mit
    // byte8 = 1 ist der Kanal immer 0, auch wenn byte9 etwas anderes sagt.
    void einStromBleibtImmerKanalNull()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        conn.setSingleChannelHoldMsForTest(0);
        handshake(conn);

        QSignalSpy iq(&conn, &RadioConnection::iqDataReceived);
        // byte8 = 1 (ein Strom), byte9 = 1 -- muss trotzdem Kanal 0 sein.
        conn.feedStreamDatagramForTest(qrpBlockKanal(1, 1, 1, char(7)));

        QCOMPARE(iq.count(), 1);
        QCOMPARE(iq.first().at(0).toInt(), 0);
        QCOMPARE(conn.kanalPaketeForTest(0), quint64(1));
        QCOMPARE(conn.kanalPaketeForTest(1), quint64(0));
    }

    // Die Rate geht jetzt ueber setSampleRate, nicht nur ueber die
    // Umgebung -- und nur fuer die zwei Raten, die am Geraet gemessen sind.
    void setSampleRateStelltDenStromstartRahmenUm()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        // Vorgabe: ein Strom, 48 kHz (= EinStrom48).
        QCOMPARE(conn.stromModusForTest(), 0);

        conn.setSampleRate(96000);
        // Mit EINEM Empfaenger ist 96 kHz seit dem 2026-10-07 EinStrom96
        // (= 3) und nicht mehr ZweiStroemeJe96: die vierte Nutzlast gibt
        // es, und ein zweiter Strom ohne Empfaenger ist halbe Datenmenge
        // umsonst.
        QCOMPARE(conn.stromModusForTest(), 3);   // EinStrom96

        conn.setSampleRate(48000);
        QCOMPARE(conn.stromModusForTest(), 0);

        // Eine Rate ohne gemessenen Rahmen aendert NICHTS -- raten geht
        // hier nicht, ein falscher Rahmen bedeutet Daten einer Rate in
        // einem Kanal einer anderen (am 2026-09-24 als "schlechtes
        // Rauschen" gehoert).
        conn.setSampleRate(192000);
        QCOMPARE(conn.stromModusForTest(), 0);
    }

    // Am 2026-10-04 an der echten QRP gefunden: die Oberflaeche stellt
    // 96 kHz ein, Longpath meldet "Connecting with sampleRate= 96000" --
    // und das Geraet streamt weiter mit 48 (Stromkopf 0100, 240
    // Nummern/s). WDSP lief also auf 96 kHz, die Daten kamen mit 48.
    //
    // Ursache: RadioModel schiebt setSampleRate AUSDRUECKLICH VOR
    // connectToRadio (eigener Kommentar dort: sonst liest composeEp2Frame
    // die Vorgaben) -- und der Sitzungs-Reset in connectToRadio hat die
    // Rate danach wieder auf die Umgebungsvorgabe zurueckgesetzt.
    //
    // Die Prueflinie oben (setSampleRateStelltDenStromstartRahmenUm) hat
    // das nicht gefangen, weil sie in der Reihenfolge prueft, die GEHT,
    // nicht in der, die die Anwendung nimmt.
    void rateVorDemVerbindenUeberlebtDenSitzungsReset()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);

        // Genau die Reihenfolge aus RadioModel::connectToRadio.
        conn.setSampleRate(96000);
        // Mit EINEM Empfaenger ist 96 kHz seit dem 2026-10-07 EinStrom96
        // (= 3) und nicht mehr ZweiStroemeJe96: die vierte Nutzlast gibt
        // es, und ein zweiter Strom ohne Empfaenger ist halbe Datenmenge
        // umsonst.
        QCOMPARE(conn.stromModusForTest(), 3);   // EinStrom96
        conn.connectToRadio(someQrpInfo());

        // Vor der Behebung stand hier wieder 0 -- und der Stromstart-Rahmen
        // ging mit 48 kHz hinaus, obwohl die App 96 angesagt hatte.
        QCOMPARE(conn.stromModusForTest(), 3);
    }

    // Dasselbe fuer die Zahl der Empfaenger: derselbe Reset setzt sie auf 1.
    void empfaengerzahlVorDemVerbindenUeberlebtDenSitzungsReset()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);

        conn.setActiveReceiverCount(2);
        QCOMPARE(conn.aktiveEmpfaengerForTest(), 2);
        conn.connectToRadio(someQrpInfo());
        QCOMPARE(conn.aktiveEmpfaengerForTest(), 2);
    }

    // ── Zwei Empfaenger brauchen zwei Stroeme ──────────────────────────
    //
    // Am 2026-10-04 belegt: der zweite Strom ist NICHT stumm. Im
    // Mitschnitt des Betreibers (ExpertSDR2 mit RX und RX2, seine eigene
    // Richtigstellung "es waren immer beide rx und rx2") traegt Kanal 1
    // echtes I/Q -- -127,9 dBFS bei 31,5 % Q ungleich null, also etwas
    // kraeftiger als Kanal 0. Longpath hat ihn weggeworfen, weil in den
    // Geraetefaehigkeiten EIN Empfaenger stand.
    //
    // Der Stromstart-Rahmen traegt beides: erstes Byte die Zahl der
    // Stroeme, zweites die Ratenstufe. Bis hierher waehlte nur die RATE
    // den Modus -- ein zweiter Empfaenger bei 48 kHz konnte also gar nie
    // Daten bekommen.
    void zweiterEmpfaengerStelltAufZweiStroemeUm()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);
        QCOMPARE(conn.stromModusForTest(), 0);   // ein Strom, 48 kHz

        conn.setActiveReceiverCount(2);
        QCOMPARE(conn.stromModusForTest(), 1);   // zwei Stroeme, je 48 kHz
        QCOMPARE(conn.aktiveEmpfaengerForTest(), 2);

        // Zurueck auf einen: wieder ein Strom, sonst laeuft die halbe
        // Datenmenge umsonst durchs Netz.
        conn.setActiveReceiverCount(1);
        QCOMPARE(conn.stromModusForTest(), 0);
    }

    // ~~Bei 96 kHz gibt es keinen Ein-Strom-Modus~~ -- am 2026-10-07
    // widerlegt. Die Annahme stammte daher, dass nur drei der vier
    // Nutzlasten gemessen waren; ExpertSDR2 schickt die vierte (ein
    // Strom, 96 kHz), und das Geraet nimmt sie an.
    //
    // Die Pruefung bleibt stehen, mit umgedrehter Erwartung: Rate und
    // Empfaengerzahl entscheiden GETRENNT, und genau das soll niemand
    // versehentlich wieder zusammenlegen.
    void beiSechsundneunzigEntscheidetDieEmpfaengerzahlMit()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        // Ein Empfaenger ist der Ausgangszustand -> ein Strom, 96 kHz.
        conn.setSampleRate(96000);
        QCOMPARE(conn.stromModusForTest(), 3);   // EinStrom96

        conn.setActiveReceiverCount(2);
        QCOMPARE(conn.stromModusForTest(), 2);   // zwei Stroeme, je 96 kHz

        conn.setActiveReceiverCount(1);
        QCOMPARE(conn.stromModusForTest(), 3);   // und wieder zurueck
    }

    // Die selbsttaetige Wiederholung (2026-10-04): bleibt KEIN Beacon
    // aus, gibt die Verbindung nicht mehr nach drei Sekunden auf, sondern
    // wartet und sucht von selbst erneut. Grund ist eine Messung am
    // Geraet: nach einem abrupten Programmende sperrt es rund eine
    // Minute und kommt dann von selbst zurueck -- der Betreiber hat an
    // einem Vormittag eine halbe Stunde verloren, weil jeder neue Klick
    // wieder in dasselbe Fenster fiel.
    void ohneBeaconWirdDieSucheWiederholtStattAufzugeben()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);

        QSignalSpy fail(&conn, &RadioConnection::connectFailed);
        conn.connectToRadio(someQrpInfo());
        // Kein Beacon einspeisen -- der Waechter laeuft ab.
        QTest::qWait(kWaitMs);

        // Frueher stand hier ein Fehlschlag. Jetzt laeuft es weiter.
        QCOMPARE(fail.count(), 0);
        QCOMPARE(conn.state(), ConnectionState::Connecting);
    }

    // Und mit abgeschalteter Wiederholung gibt es ihn sofort -- sonst
    // koennte die Prueflinie oben auch dann gruen sein, wenn gar kein
    // Fehlschlag mehr moeglich waere.
    void ohneWiederholungGibtEsDenFehlschlagSofort()
    {
        SunSdrRadioConnection conn;
        conn.setSucheWiederholungEnabledForTest(false);
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);

        QSignalSpy fail(&conn, &RadioConnection::connectFailed);
        conn.connectToRadio(someQrpInfo());
        QVERIFY(fail.wait(kWaitMs));
        QCOMPARE(conn.state(), ConnectionState::Disconnected);
    }

    // ── Mikrofon-PTT am Geraet ─────────────────────────────────────────
    //
    // Die zweite Empfangsluecke, geschlossen ohne Protokollwissen: der
    // Stromkopf traegt den Betriebszustand (0xFE Empfang, 0xFD Senden).
    // Drueckt jemand am Geraet die Mikrofontaste, wechselt der Opcode.

    static QByteArray qrpBlockTx(quint16 seq)
    {
        QByteArray pkt = SunSdr::buildIqHeader(
            SunSdr::kProfileQrp, SunSdr::kOpIqTxActive, seq, 0x02, 0x01);
        pkt.append(QByteArray(SunSdr::kIqPayloadSize, char(0)));
        return pkt;
    }

    void sendezustandAmGeraetMeldetPtt()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        QSignalSpy ptt(&conn, &RadioConnection::micPttFromRadio);
        conn.feedStreamDatagramForTest(qrpBlockSeq(1));
        QCOMPARE(ptt.count(), 0);

        conn.feedStreamDatagramForTest(qrpBlockTx(2));
        QCOMPARE(ptt.count(), 1);
        QCOMPARE(ptt.first().at(0).toBool(), true);
        QVERIFY(conn.geraetSendetForTest());

        // Nur die FLANKE: 240 Pakete je Sekunde duerfen nicht 240 Signale
        // ergeben.
        for (quint16 n = 3; n <= 30; ++n) {
            conn.feedStreamDatagramForTest(qrpBlockTx(n));
        }
        QCOMPARE(ptt.count(), 1);

        // Und zurueck.
        conn.feedStreamDatagramForTest(qrpBlockSeq(31));
        QCOMPARE(ptt.count(), 2);
        QCOMPARE(ptt.last().at(0).toBool(), false);
        QVERIFY(!conn.geraetSendetForTest());
        QCOMPARE(conn.mikrofonPttFlankenForTest(), quint64(2));
    }

    // Was Longpath selbst ausgeloest hat, ist kein PTT vom Geraet.
    void eigenesMoxGiltNichtAlsPttVomGeraet()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);
        conn.feedStreamDatagramForTest(qrpBlockSeq(1));

        conn.setTxArmedForTest(true);
        conn.setTxCheckContextForTest(armedInBandCtx());
        conn.setMox(true);
        QVERIFY(conn.isMoxForTest());

        QSignalSpy ptt(&conn, &RadioConnection::micPttFromRadio);
        conn.feedStreamDatagramForTest(qrpBlockTx(2));

        QCOMPARE(ptt.count(), 0);
        QCOMPARE(conn.mikrofonPttFlankenForTest(), quint64(0));
        // Der Zustand wird trotzdem mitgefuehrt -- nur nicht als PTT
        // gemeldet.
        QVERIFY(conn.geraetSendetForTest());
    }

    // Ein haengendes PTT darf eine Sitzung nicht ueberleben: das ist der
    // falsche Zustand, in dem man einen Sender in Erinnerung behaelt.
    void haengendesPttWirdBeimTrennenZurueckgenommen()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);
        conn.feedStreamDatagramForTest(qrpBlockSeq(1));
        conn.feedStreamDatagramForTest(qrpBlockTx(2));

        QSignalSpy ptt(&conn, &RadioConnection::micPttFromRadio);
        QVERIFY(conn.geraetSendetForTest());
        conn.disconnect();

        QCOMPARE(ptt.count(), 1);
        QCOMPARE(ptt.first().at(0).toBool(), false);
        QVERIFY(!conn.geraetSendetForTest());
    }

    // ── Uebersteuerung ─────────────────────────────────────────────────
    //
    // Eine der zwei echten Luecken im Empfang: P1/P2 melden adcOverflow aus
    // einem Statusbit, die QRP schickt keines (am 2026-10-03 gemessen:
    // zehn Minuten kein unaufgeforderter Rahmen, Zustandsbytes konstant).
    // Also aus dem Signal selbst -- eine Probe am Anschlag ist eine Probe
    // am Anschlag.

    // Ein Block mit Proben am Vollausschlag. Der Wandler liefert 24 Bit in
    // den oberen drei Byte eines 32-Bit-Worts; 0x7fffff ist der Anschlag.
    static QByteArray qrpBlockVollausschlag(quint16 seq, int wieViele)
    {
        QByteArray pkt = SunSdr::buildIqHeader(
            SunSdr::kProfileQrp, SunSdr::kOpIqRxIdle, seq, 0x01, 0x00);
        QByteArray payload(SunSdr::kIqPayloadSize, char(0));
        for (int i = 0; i < wieViele && i < SunSdr::kIqComplexPerPkt; ++i) {
            const int k = i * SunSdr::kIqBytesPerComplex;
            // I-Anteil (Byte 3..5) auf 0x7fffff
            payload[k + 3] = char(0xFF);
            payload[k + 4] = char(0xFF);
            payload[k + 5] = char(0x7F);
        }
        pkt.append(payload);
        return pkt;
    }

    void vollausschlagMeldetUebersteuerung()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        QSignalSpy ueber(&conn, &RadioConnection::adcOverflow);
        conn.feedStreamDatagramForTest(qrpBlockVollausschlag(1, 40));

        QCOMPARE(ueber.count(), 1);
        QCOMPARE(ueber.first().at(0).toInt(), 0);
        QCOMPARE(conn.anschlagMeldungenForTest(), quint64(1));
        QCOMPARE(conn.anschlagProbenForTest(), quint64(40));
    }

    // Der Rauschflur ohne Antenne liegt bei etwa 2e-05 -- sechs
    // Zehnerpotenzen unter der Schwelle. Es darf nichts anschlagen.
    void rauschenMeldetKeineUebersteuerung()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        QSignalSpy ueber(&conn, &RadioConnection::adcOverflow);
        for (quint16 n = 1; n <= 20; ++n) {
            conn.feedStreamDatagramForTest(qrpBlockSeq(n));
        }

        QCOMPARE(ueber.count(), 0);
        QCOMPARE(conn.anschlagProbenForTest(), quint64(0));
    }

    // Eine einzelne Probe am Anschlag kann ein Zufall sein und darf keine
    // Meldung ausloesen -- gezaehlt wird sie trotzdem.
    void einzelneProbeAmAnschlagMeldetNichts()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        QSignalSpy ueber(&conn, &RadioConnection::adcOverflow);
        conn.feedStreamDatagramForTest(qrpBlockVollausschlag(1, 1));

        QCOMPARE(ueber.count(), 0);
        QCOMPARE(conn.anschlagProbenForTest(), quint64(1));
    }

    // Und die Drosselung: 240 Pakete je Sekunde duerfen nicht 240
    // Meldungen ergeben.
    void uebersteuerungWirdGedrosselt()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        QSignalSpy ueber(&conn, &RadioConnection::adcOverflow);
        for (quint16 n = 1; n <= 50; ++n) {
            conn.feedStreamDatagramForTest(qrpBlockVollausschlag(n, 40));
        }

        QCOMPARE(ueber.count(), 1);
        QCOMPARE(conn.anschlagProbenForTest(), quint64(50 * 40));
    }

    // ── Folgenummern: Verlust, Luecken, Wiederholungen ────────────────
    //
    // P1 und P2 melden das seit langem, dieser Treiber bisher nicht. Die
    // QRP-eigene Regel ist die Wiederholung: dieselbe Nummer achtmal ist
    // kein Verlust (gemessen 2026-09-23), und sie darf den Nenner der
    // Verlustrechnung nicht aufblaehen.

    static QByteArray qrpBlockSeq(quint16 seq)
    {
        QByteArray pkt = SunSdr::buildIqHeader(
            SunSdr::kProfileQrp, SunSdr::kOpIqRxIdle, seq, 0x01, 0x00);
        pkt.append(QByteArray(SunSdr::kIqPayloadSize, char(0)));
        return pkt;
    }

    // Wie oben, aber mit waehlbarem Inhalt -- fuer die Frage, ob eine
    // wiederkehrende Nummer wirklich eine bytegleiche Kopie ist.
    static QByteArray qrpBlockSeqInhalt(quint16 seq, char fuellung)
    {
        QByteArray pkt = SunSdr::buildIqHeader(
            SunSdr::kProfileQrp, SunSdr::kOpIqRxIdle, seq, 0x01, 0x00);
        pkt.append(QByteArray(SunSdr::kIqPayloadSize, fuellung));
        return pkt;
    }

    // Am 2026-10-04 aus Martins Mitschnitt (118 550 Pakete, 0 vom Kern
    // verworfen) belegt: auf dem Draht sind NULL bytegleiche
    // Wiederholungen -- und Longpath meldete im selben Betrieb "1,41
    // Kopien je Nummer". Die Zahl kam aus der eigenen Buchfuehrung.
    //
    // Grund: der Ring haelt 128 Nummern, aber verglichen wurde nur die
    // Nummer, nicht der Inhalt. Die Fortsetzungs-Erkennung in
    // processStreamDatagram prueft den Inhalt zwar, aber nur gegen die
    // UNMITTELBAR vorige Nummer desselben Kanals. Kehrt eine Nummer mit
    // Abstand wieder (der Zaehler laeuft um), galt sie ungeprueft als
    // Kopie.
    //
    // Das ist nicht nur eine schiefe Zahl: an genau dieser Groesse
    // erkennen wir die Achtfachung (1,0 heisst, die Blockantwort wirkt;
    // 8,0 heisst, sie wirkt nicht). Eine Grundlast von 1,4 verdeckt eine
    // echte Verschlechterung.
    void gleicheNummerMitAnderemInhaltIstKeineKopie()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        for (quint16 n = 1; n <= 5; ++n) {
            conn.feedStreamDatagramForTest(qrpBlockSeqInhalt(n, char(0)));
        }
        // Dieselbe Nummer, ANDERER Inhalt: der Zaehler ist umgelaufen,
        // das ist ein neuer Block und keine Kopie.
        conn.feedStreamDatagramForTest(qrpBlockSeqInhalt(3, char(0x5A)));
        QCOMPARE(conn.seqRepeatsForTest(), quint64(0));

        // Dieselbe Nummer mit GLEICHEM Inhalt bleibt eine Kopie -- sonst
        // wuerde die Behebung die Achtfachungs-Erkennung abschalten.
        conn.feedStreamDatagramForTest(qrpBlockSeqInhalt(4, char(0)));
        QCOMPARE(conn.seqRepeatsForTest(), quint64(1));
    }

    void luekenloseFolgeMeldetKeinenVerlust()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        QSignalSpy gap(&conn, &RadioConnection::iqSequenceGap);
        for (quint16 n = 100; n < 140; ++n) {
            conn.feedStreamDatagramForTest(qrpBlockSeq(n));
        }

        QCOMPARE(conn.seqFramesForTest(), quint64(40));
        QCOMPARE(conn.seqLostForTest(), quint64(0));
        QCOMPARE(conn.seqRepeatsForTest(), quint64(0));
        QCOMPARE(gap.count(), 0);
    }

    // Die Eigenschaft, an der sich dieser Treiber von P1/P2 unterscheidet:
    // achtmal dieselbe Nummer ist der Normalzustand ohne Blockantwort.
    void achtfachWiederholungIstKeinVerlust()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        QSignalSpy gap(&conn, &RadioConnection::iqSequenceGap);
        for (quint16 n = 1; n <= 10; ++n) {
            for (int kopie = 0; kopie < 8; ++kopie) {
                conn.feedStreamDatagramForTest(qrpBlockSeq(n));
            }
        }

        QCOMPARE(conn.seqFramesForTest(), quint64(10));
        QCOMPARE(conn.seqRepeatsForTest(), quint64(70));
        QCOMPARE(conn.seqLostForTest(), quint64(0));
        QCOMPARE(gap.count(), 0);
    }

    // Am 2026-10-03 am echten Geraet gemessen: die bytegleichen
    // Wiederholungen kommen MIT ABSTAND, nicht direkt hintereinander --
    // "5 3 6 7 8 9 10 8 11 12 13 14 12". Gegen die letzte Nummer gerechnet
    // waere die 3 nach der 5 ein Rueckwaerts-Laeufer; sie ist aber eine
    // Wiederholung (das Messgeraet belegt es ueber die ganze Nutzlast:
    // 100 % bytegleich, verschieden 0). Deshalb der Ring.
    void wiederholungMitAbstandIstKeinSpaetling()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        for (const quint16 n : {quint16(1), quint16(2), quint16(3), quint16(4),
                                quint16(5), quint16(3), quint16(6), quint16(7),
                                quint16(5)}) {
            conn.feedStreamDatagramForTest(qrpBlockSeq(n));
        }

        // 1..7 sind sieben Nummern, die 3 und die 5 kamen je zweimal.
        QCOMPARE(conn.seqFramesForTest(), quint64(7));
        QCOMPARE(conn.seqRepeatsForTest(), quint64(2));
        QCOMPARE(conn.seqBackwardsForTest(), quint64(0));
        QCOMPARE(conn.seqLostForTest(), quint64(0));
    }

    // Der Fehler, der den Zaehler am Geraet voellig lahmgelegt hat: das
    // Geraet faengt die Folgenummer bei 0 NEU an, wenn der Strom neu
    // startet. Die erste Fassung hing danach auf der alten Nummer fest und
    // meldete "0 Nummern in 5 s, 1301 rueckwaerts".
    void stromneustartWirdErkanntUndNichtZumDauerzustand()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        for (quint16 n = 42520; n <= 42529; ++n) {
            conn.feedStreamDatagramForTest(qrpBlockSeq(n));
        }
        QCOMPARE(conn.seqFramesForTest(), quint64(10));

        // Jetzt faengt der Strom bei 0 an.
        for (quint16 n = 0; n <= 20; ++n) {
            conn.feedStreamDatagramForTest(qrpBlockSeq(n));
        }

        QCOMPARE(conn.seqRestartsForTest(), quint64(1));
        // 10 alte + 21 neue, minus die zwei, die bis zum Erkennen des
        // Neuanfangs als Spaetlinge gezaehlt wurden.
        QCOMPARE(conn.seqFramesForTest(), quint64(29));
        QCOMPARE(conn.seqBackwardsForTest(), quint64(2));
        QCOMPARE(conn.seqLostForTest(), quint64(0));
    }

    // Gegenprobe dazu: EIN Spaetling zwischen passenden Paketen darf nicht
    // als Neuanfang gelesen werden, sonst dreht ein einzelnes verirrtes
    // Paket den ganzen Zaehler um.
    void einzelnerSpaetlingIstKeinNeuanfang()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        conn.feedStreamDatagramForTest(qrpBlockSeq(5000));
        conn.feedStreamDatagramForTest(qrpBlockSeq(5001));
        conn.feedStreamDatagramForTest(qrpBlockSeq(1000));  // weit zurueck
        conn.feedStreamDatagramForTest(qrpBlockSeq(5002));  // passt wieder
        conn.feedStreamDatagramForTest(qrpBlockSeq(5003));

        QCOMPARE(conn.seqRestartsForTest(), quint64(0));
        QCOMPARE(conn.seqBackwardsForTest(), quint64(1));
        QCOMPARE(conn.seqFramesForTest(), quint64(4));
    }

    void echteLueckeWirdGezaehltUndGemeldet()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        QSignalSpy gap(&conn, &RadioConnection::iqSequenceGap);
        conn.feedStreamDatagramForTest(qrpBlockSeq(10));
        conn.feedStreamDatagramForTest(qrpBlockSeq(14));  // 11,12,13 fehlen

        QCOMPARE(conn.seqLostForTest(), quint64(3));
        QCOMPARE(gap.count(), 1);
    }

    // 16 Bit laufen um. 65535 -> 0 ist eine lueckenlose Folge, keine
    // Luecke von 65535 Nummern -- darum wird die Differenz als quint16
    // gebildet.
    // Gegenprobe zur Drosselung: viele Luecken in derselben Millisekunde
    // ergeben EINE Meldung, nicht zwanzig -- und nicht null. Der
    // Startwert -1 ist genau dafuer da (siehe m_lastGapSignalMs).
    void vieleLueckenKurzHintereinanderMeldenEinmal()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        QSignalSpy gap(&conn, &RadioConnection::iqSequenceGap);
        quint16 n = 1000;
        for (int i = 0; i < 20; ++i) {
            conn.feedStreamDatagramForTest(qrpBlockSeq(n));
            n = quint16(n + 3);  // je zwei Nummern fehlen
        }

        QCOMPARE(gap.count(), 1);
        QCOMPARE(conn.seqLostForTest(), quint64(38));  // 19 Luecken x 2
    }

    void umlaufDerSechzehnBitIstKeineLuecke()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        conn.feedStreamDatagramForTest(qrpBlockSeq(65534));
        conn.feedStreamDatagramForTest(qrpBlockSeq(65535));
        conn.feedStreamDatagramForTest(qrpBlockSeq(0));
        conn.feedStreamDatagramForTest(qrpBlockSeq(1));

        QCOMPARE(conn.seqFramesForTest(), quint64(4));
        QCOMPARE(conn.seqLostForTest(), quint64(0));
    }

    // Ein Spaetling zaehlt nicht als Verlust, und er darf den Stand nicht
    // zurueckdrehen -- sonst waere die naechste richtige Nummer eine
    // Riesenluecke.
    void spaetlingZaehltNichtAlsVerlustUndDrehtDenStandNichtZurueck()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);

        conn.feedStreamDatagramForTest(qrpBlockSeq(5000));
        conn.feedStreamDatagramForTest(qrpBlockSeq(4000));  // rueckwaerts
        conn.feedStreamDatagramForTest(qrpBlockSeq(5001));  // schliesst an 5000 an

        QCOMPARE(conn.seqBackwardsForTest(), quint64(1));
        QCOMPARE(conn.seqLostForTest(), quint64(0));
        QCOMPARE(conn.seqFramesForTest(), quint64(2));
    }

    void folgenummernBeginnenMitJederVerbindungNeu()
    {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();
        conn.setDiscoveryBroadcastEnabledForTest(false);
        conn.connectToRadio(someQrpInfo());
        handshake(conn);
        conn.feedStreamDatagramForTest(qrpBlockSeq(9000));
        QCOMPARE(conn.seqFramesForTest(), quint64(1));

        conn.disconnect();
        conn.connectToRadio(someQrpInfo());
        handshake(conn);
        // Nummer 10 nach 9000: ohne Ruecksetzen waere das eine Luecke.
        conn.feedStreamDatagramForTest(qrpBlockSeq(10));

        QCOMPARE(conn.seqFramesForTest(), quint64(1));
        QCOMPARE(conn.seqLostForTest(), quint64(0));
        QCOMPARE(conn.seqBackwardsForTest(), quint64(0));
    }
};

QTEST_MAIN(TestSunSdrRadioConnection)
#include "tst_sunsdr_radio_connection.moc"
