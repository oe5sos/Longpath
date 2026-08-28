#pragma once

// no-port-check: NereusSDR/Longpath-original RadioConnection subclass.
// Wire framing comes from src/core/sunsdr/SunSdrProtocol.h (ported from
// ArtemisSDR, see that file's header and
// docs/attribution/ARTEMISSDR-PROVENANCE.md) — nothing in THIS file is
// copied from ArtemisSDR or Thetis; it's Longpath's own RadioConnection
// skeleton, following the shape P1RadioConnection/P2RadioConnection
// already established.

// =================================================================
// src/core/SunSdrRadioConnection.h  (NereusSDR/Longpath)
// =================================================================
//
// A third `RadioConnection` subclass, alongside P1RadioConnection and
// P2RadioConnection — for the SunSDR2 QRP's native wire protocol, not
// an OpenHPSDR protocol at all. NOT a variant of TciClient (that stays
// exactly what it is: a client of ExpertSDR2's companion protocol for
// operators who don't want, or can't yet use, the native path).
//
// Design doc: docs/architecture/2026-08-24-sunsdr-native-driver-design.md
// Plan doc:   docs/architecture/2026-08-26-sunsdr-connection-plan.md
//
// ── Scope: receive-only, and the minimal RX-start sequence ───────────
//
// This class binds its sockets, transitions through ConnectionState,
// and decodes/emits RX I/Q once a session is open. `connectToRadio()`
// does NOT send ArtemisSDR's ~30-step DX boot macro — the real QRP's
// boot sequence has eight opcodes still with no attributed meaning at
// all (design doc, updated through the evening of 2026-08-26), and
// porting the DX-documented macro verbatim would mean guessing at a
// QRP's control-channel handshake, which is exactly what CLAUDE.md's
// SOURCE-FIRST protocol exists to prevent.
//
// What IS here, bench-confirmed the same evening (design doc,
// "BREAKTHROUGH, 2026-08-26"): a broadcast discovery query (magic 0x03,
// opcode 0x00) gets a direct unicast "beacon" reply from the QRP
// (opcode 0x01, ExpertSDR2 never running); replaying one further
// control frame (opcode 0x01, `SUNSDR_OP_STATE_SYNC` in ArtemisSDR's
// naming) after the beacon starts a genuine, sustained I/Q stream —
// 15,336 real packets / 18.3 MB in an independent 8-second bench run,
// ExpertSDR2 never running that session. All three frames sent here
// (discovery, the state-sync frame, the frequency frame) are the exact
// bytes from that bench capture, not reconstructed via
// SunSdrProtocol::buildControlHeader() — a real discrepancy was found
// between what that builder assumes (bytes 14-17 always zero) and what
// these actual captured frames carry there (a checksum-or-sequence-
// shaped value that varies per capture and isn't understood yet), so
// building a *fresh* frame with those bytes zeroed is untested and not
// what got the QRP to answer. Exact-byte replay is the only path this
// evening's bench work actually confirmed.
//
// Updated 2026-08-27: `setReceiverFrequency()` is no longer a no-op.
// The frequency frame's 8-byte payload encoding was found (a candidate
// formula derived from ArtemisSDR's real DX/PRO code) and then
// confirmed the same day against a live, exact, known-frequency bench
// test — see SunSdrRadioConnection.cpp's implementation comment for the
// numbers. One caveat remains genuinely open: the 18-byte header's
// bytes 14-17 carry a varying, not-fully-understood value in every real
// captured frame; the implementation reuses the exact prefix from the
// one confirmed-accepted frame rather than guessing new bytes there.
//
// Every pure-virtual TX/PTT/mic-jack/PureSignal method on the base
// class is overridden here as a documented no-op (plan doc §Phase B.5)
// — this connection cannot transmit and does not claim to.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-26 — Original for NereusSDR/Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-08-26 — Minimal RX-start sequence wired into connectToRadio()
//                 (discovery broadcast + state-sync frame replay),
//                 following the same evening's bench-confirmed
//                 findings. AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "RadioConnection.h"
#include "BoardCapabilities.h"
#include "core/sunsdr/SunSdrProtocol.h"

#include <QElapsedTimer>
#include <QHostAddress>
#include <QTimer>
#include <QUdpSocket>

#include <atomic>

namespace Longpath {

class SunSdrRadioConnection : public RadioConnection {
    Q_OBJECT

public:
    explicit SunSdrRadioConnection(QObject* parent = nullptr);
    ~SunSdrRadioConnection() override;

    int protocolVersion() const override { return 3; }  // ProtocolVersion::SunSdr

    // Exposes the private connect-timeout constant for tests, same
    // pattern as P1RadioConnection::connectTimeoutMsForTest() /
    // P2RadioConnection::connectTimeoutMsForTest() — a test that waits
    // a hardcoded number of milliseconds for a timeout is measuring an
    // assumption, not the actual guard.
    static constexpr int connectTimeoutMsForTest() { return kConnectTimeoutMs; }

    // True once setRxReady(true) has been called and not since reset
    // by disconnect(). Tests use this instead of poking m_rxReady
    // directly (private, atomic, not meant for outside reads except
    // via this accessor).
    bool isRxReadyForTest() const { return m_rxReady.load(std::memory_order_acquire); }

    // Test-only hook for the same reason: nothing wires the real
    // rxWdspReady-equivalent gate yet (that's Phase C/D's job, once the
    // downstream DSP-readiness signal exists to drive it from). Until
    // then this is the only way to open the gate at all, including for
    // tests.
    void setRxReadyForTest(bool ready) { setRxReady(ready); }

    // Test-only hook: feed a raw stream-socket datagram through the
    // exact same decode path onStreamReadyRead() uses, without needing
    // a real bound UDP socket and a loopback send. Production code
    // never calls this — the real path is the readyRead signal.
    void feedStreamDatagramForTest(const QByteArray& datagram);

    // Same idea for the control channel — feeds a raw datagram (as if
    // from `sender`) through the exact same beacon-detection/state-sync-
    // replay path onControlReadyRead() uses. Added 2026-08-26 to close a
    // real coverage gap: before this existed, that whole path (every
    // byte-guard, the success path, the already-handshaken drain) had
    // zero test coverage, reachable only by a real UDP datagram landing
    // on a bound socket.
    void feedControlDatagramForTest(const QByteArray& datagram,
                                     const QHostAddress& sender);

    // Test-only kill switch for sendDiscoveryBroadcast()'s real network
    // send. Defaults true (production always broadcasts for real, no
    // call site needs to opt in). A test that calls connectToRadio()
    // must call this with false first, or the test run broadcasts a
    // real SunSDR2 discovery query onto whatever LAN the test machine
    // is on. On the operator's own dev machine that LAN can have a real
    // QRP listening — this feature exists because of exactly that
    // machine/radio pairing — so an undisabled test wouldn't just be
    // network noise, it would replay the bench-confirmed handshake in
    // onControlReadyRead() and start a real RX stream on real hardware
    // from a unit test.
    void setDiscoveryBroadcastEnabledForTest(bool enabled) {
        m_discoveryBroadcastEnabled = enabled;
    }

    // Test-only kill switch for init()'s fixed-port bind (see its own
    // comment). Defaults true (production always wants the fixed port —
    // that's the whole point of the fix it is). Must be called BEFORE
    // init(), since that is where the actual bind happens.
    //
    // Found necessary the hard way, 2026-08-27: even with
    // setDiscoveryBroadcastEnabledForTest(false) — which stops this
    // class from SENDING anything — init() still binds the real stream
    // socket to the real, well-known port 50002 with ShareAddress, so
    // it stays reachable by unsolicited real traffic. A live bench
    // session had left the QRP still streaming to that exact port after
    // the app closed (nothing sends it an explicit stop — see the
    // design doc's teardown gap), and a test that called QTest::qWait()
    // shortly after got 85 real leftover I/Q packets instead of the
    // zero it expected: real QRP traffic silently landing in a unit
    // test, discovered only because it happened to break an assertion.
    // Every test that calls init() must call this with false first,
    // unless it is specifically testing the fixed-port bind itself
    // (see controlSocketLocalPortForTest()'s test).
    void setFixedPortBindingEnabledForTest(bool enabled) {
        m_fixedPortBindingEnabled = enabled;
    }

    // Exact bytes, bench-confirmed 2026-08-26 (see the header's
    // top-of-file comment). Exposed so a test can assert the frames
    // this class actually sends match the bytes the bench run
    // confirmed work, byte for byte — not just "something non-empty
    // went out."
    static QByteArray discoveryFrameForTest();
    static QByteArray stateSyncFrameForTest();
    static QByteArray replayedFrequencyFrameForTest();

    // The bound local port each socket ended up on after init() — 0 if
    // not yet initialized. Exposed so a test can assert the fixed-port
    // bind (see init()'s comment) actually took the fixed port rather
    // than silently falling back to an ephemeral one, without needing
    // any real network round trip to observe it. Added 2026-08-26 after
    // exactly that fixed-vs-ephemeral distinction turned out to be the
    // root cause of the first live-test failure this evening.
    quint16 controlSocketLocalPortForTest() const {
        return m_controlSocket ? m_controlSocket->localPort() : 0;
    }
    quint16 streamSocketLocalPortForTest() const {
        return m_streamSocket ? m_streamSocket->localPort() : 0;
    }

public slots:
    // ── Real implementations (plan doc §Phase B.4) ──────────────────
    void init() override;
    void connectToRadio(const Longpath::RadioInfo& info) override;
    void disconnect() override;
    void setReceiverFrequency(int receiverIndex, quint64 frequencyHz) override;
    void setActiveReceiverCount(int count) override;
    void setSampleRate(int sampleRate) override;

    // Bench-confirmed 2026-08-27 (opcode 0x04) — narrowly scoped to
    // exactly the two real states this class has actual captured bytes
    // for (0 dB, -20 dB). Any other requested value is a logged no-op,
    // not an interpolation to the nearest known state. See the .cpp
    // implementation's comment for the exact provenance and why this
    // sits outside the no-op block below despite RX-side gain/atten
    // otherwise being an RX-not-TX control.
    void setAttenuator(int dB) override;

    // ── Safe no-ops: this connection is receive-only (plan doc §Phase B.5) ──
    //
    // Every one of these exists only because RadioConnection declares it
    // pure virtual for the P1/P2 TX path. None of them touch the wire.
    // "No PTT/drive code written at all yet" is a direct quote from the
    // design doc's own phasing gate, not a paraphrase.
    void setTxFrequency(quint64 frequencyHz) override;
    void setPreamp(bool enabled) override;
    void setTxDrive(int level) override;
    void setMox(bool enabled) override;
    void setAntennaRouting(AntennaRouting routing) override;
    void sendTxIq(const float* iq, int n) override;
    void setTrxRelay(bool enabled) override;
    void setMicBoost(bool on) override;
    void setLineIn(bool on) override;
    void setMicTipRing(bool tipHot) override;
    void setMicBias(bool on) override;
    void setLineInGain(int gain) override;
    void setUserDigOut(quint8 dig) override;
    void setPuresignalRun(bool run) override;
    void setMicPTTDisabled(bool disabled) override;
    void setMicXlr(bool xlrJack) override;
    void setWatchdogEnabled(bool enabled) override;

private slots:
    void onControlReadyRead();
    void onStreamReadyRead();
    void onConnectTimeout();
    void onKeepaliveTimeout();

private:
    // The DX/PRO/QRP profile table this connection uses for magic byte
    // and default ports. Resolved from RadioInfo::boardType at
    // connectToRadio() time — see resolveProfile().
    static const SunSdr::Profile& resolveProfile(HPSDRHW board);

    // Shared by onStreamReadyRead() (real socket data) and
    // feedStreamDatagramForTest() (synthetic test data) — one decode
    // path, two ways in.
    void processStreamDatagram(const QByteArray& datagram);

    // Shared by onControlReadyRead() (real socket data) and
    // feedControlDatagramForTest() (synthetic test data) — same split as
    // the stream side above.
    void processControlDatagram(const QByteArray& data, const QHostAddress& sender);

    // Sends discoveryFrame() to the broadcast address of every local,
    // up IPv4 interface — mirrors what a live capture showed ExpertSDR2
    // itself doing on its own cold launch (design doc, "the
    // reachability gate is a broadcast discovery packet"), not a guess
    // at a single subnet's broadcast address.
    void sendDiscoveryBroadcast();

    // True from connectToRadio() until the first beacon reply is seen
    // (or the connect watchdog fires). Once false, further control-
    // channel replies are drained but not re-acted on — this class
    // sends its two confirmed frames exactly once per session.
    bool m_awaitingBeacon{false};
    QHostAddress m_radioAddr;

    // See setDiscoveryBroadcastEnabledForTest()'s comment.
    bool m_discoveryBroadcastEnabled{true};

    // See setFixedPortBindingEnabledForTest()'s comment.
    bool m_fixedPortBindingEnabled{true};

    // rxWdspReady-equivalent gate (plan doc §Phase C.3). Not started by
    // anything in this file yet — Phase D wires the downstream DSP
    // readiness signal into setRxReady(). Default false: no I/Q is fed
    // downstream until something explicitly arms it. Packets arriving
    // while false are discarded, not buffered — same rationale
    // ArtemisSDR itself documents (avoids a stale/garbage burst once
    // the DSP chain is ready).
    void setRxReady(bool ready);
    std::atomic<bool> m_rxReady{false};

    // Periodic silent RX-idle keepalive — SunSdrProtocol.h's own citation
    // (ArtemisSDR sunsdr.c) documents that the host must keep sending
    // these (opcode 0xFE, kOpIqRxIdle, zero payload) or the radio drops
    // the I/Q stream after roughly 8 seconds. Found missing entirely,
    // 2026-08-26, during a from-scratch review of this class the same
    // evening it first connected live — m_txSeq existed for exactly this
    // purpose already but nothing ever incremented or sent it. Started
    // once the RX gate opens (setRxReady(true) in onControlReadyRead()),
    // stopped on disconnect()/onConnectTimeout(). kKeepaliveIntervalMs is
    // a safety-margin guess, not a bench-measured number — "roughly 8
    // seconds" is the only cited figure; 2s leaves ample margin without
    // being so frequent it could plausibly matter for bandwidth.
    QTimer* m_keepaliveTimer{nullptr};
    static constexpr int kKeepaliveIntervalMs = 2000;

    QUdpSocket* m_controlSocket{nullptr};
    QUdpSocket* m_streamSocket{nullptr};
    QTimer*     m_connectWatchdog{nullptr};

    static constexpr int kConnectTimeoutMs = 3000;

    const SunSdr::Profile* m_profile{nullptr};
    const BoardCapabilities* m_caps{nullptr};

    bool m_running{false};
    bool m_intentionalDisconnect{false};
    quint16 m_txSeq{0};   // control-channel-independent; used only for the
                          // periodic silent IQ-stream keepalive (design doc:
                          // "the host must keep sending silent 0xFE packets")
};

} // namespace Longpath
