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
#include "core/sunsdr/SunSdrProtocol.h"
#include "core/sunsdr/SunSdrTxPacer.h"
#include "core/safety/BandPlanGuard.h"
#include "core/WdspTypes.h"
#include "models/Band.h"

#include <QElapsedTimer>
#include <QHostAddress>
#include <QMutex>
#include <QString>
#include <QTimer>
#include <QUdpSocket>
#include <QVector>

#include <array>
#include <atomic>
#include <cstdint>

namespace Longpath {

// ── Step 2 (SunSDR2 QRP TX-chain plan): bench-only TX gate scaffolding ──
//
// Still zero wire reachability. These three free types back
// SunSdrRadioConnection's bench-only arm gate, MOX-check context, and
// 50-entry TX trace ring (see that class's own m_txArmed/m_mox/
// m_txTrace comments). Free (not nested) so a test can name them
// without a `SunSdrRadioConnection::` prefix — same shape as
// FaultLog.h's free `FaultEvent` struct.

/// Bench/test-only input to BandPlanGuard::checkMoxAllowed()
/// (src/core/safety/BandPlanGuard.h). This class has no SliceModel/
/// RadioModel of its own to source region/mode/band/frequency from the
/// way RadioModel::installBandPlanMoxCheck()'s real lambda does
/// (RadioModel.cpp:9318-9374 — the real call-site shape this mirrors:
/// region + a requested TX frequency + mode + rx/tx band +
/// preventDifferentBand + extended, exactly checkMoxAllowed()'s own
/// parameter list). Until a later step wires real integration with
/// MoxController's single-authority MOX flow (MoxController.h's own K.2
/// setMoxCheck() callback), this is how a test (or, later, a bench UI)
/// tells setMox() what to check against. Defaults are placeholders, not
/// meaningful production values — they're inert unless a test also
/// arms the gate (setTxArmedForTest(true)), since setMox() checks
/// m_txArmed first and never reaches this context otherwise.
struct TxCheckContext {
    safety::Region region{safety::Region::Europe};
    std::int64_t   txFreqHz{0};
    DSPMode        mode{DSPMode::USB};
    Band           rxBand{Band::GEN};
    Band           txBand{Band::GEN};
    bool           preventDifferentBand{false};
    bool           extended{false};
};

/// What kind of event a TxTraceEntry records. Deliberately narrow — only
/// the events Step 2 can actually produce (no pacer/antenna/PA code
/// exists yet to generate anything else).
enum class TxTraceKind : quint8 {
    Armed,        ///< setTxArmedForTest(true)
    Disarmed,     ///< setTxArmedForTest(false), or a teardown path's reset
    MoxRefused,   ///< setMox() refused — see TxTraceEntry::reason for why
    MoxAccepted,  ///< setMox() accepted (m_mox transitioned) — still zero
                  ///< wire effect at this step; see setMox()'s own comment
};

/// One entry in the 50-entry TX trace ring. Timestamp-free by design —
/// nothing here needs wall-clock ordering, only relative ordering, which
/// `seq` (monotonically increasing for the lifetime of the connection
/// object, never reset by disconnect()'s ring clear) already provides.
struct TxTraceEntry {
    quint32     seq{0};
    TxTraceKind kind{TxTraceKind::Armed};
    QString     reason;  ///< empty for Armed/Disarmed; the real
                         ///< BandPlanGuard reason for MoxRefused (or
                         ///< "refused: not armed"); "accepted: ..." for
                         ///< MoxAccepted.
};

/// Ring capacity — small and fixed, per the design synthesis ("keep it
/// genuinely small and simple, this is not a general-purpose logging
/// system").
inline constexpr int kTxTraceRingSize = 50;

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

    // True once a beacon reply has set m_radioAddr and not since cleared
    // by disconnect()/onConnectTimeout()/onDataWatchdogTick(). Tests use
    // this to confirm every teardown path actually clears it — a real
    // gap found in review, 2026-08-28: two of the three teardown paths
    // didn't, which let setReceiverFrequency()/setAttenuator() keep
    // sending to a torn-down session's address (m_radioAddr.isNull() is
    // their only "is there an open session" gate).
    bool hasRadioAddrForTest() const { return !m_radioAddr.isNull(); }

    // Exposes the private data-watchdog silence threshold, same
    // rationale as connectTimeoutMsForTest() above.
    static constexpr int dataSilenceTimeoutMsForTest() { return kDataSilenceTimeoutMs; }

    // Test-only hook for the same reason: nothing wires the real
    // rxWdspReady-equivalent gate yet (that's Phase C/D's job, once the
    // downstream DSP-readiness signal exists to drive it from). Until
    // then this is the only way to open the gate at all, including for
    // tests.
    void setRxReadyForTest(bool ready) { setRxReady(ready); }

    // ── Step 2 (SunSDR2 QRP TX-chain plan): bench-only TX gate ─────────
    //
    // Still zero wire reachability — see setMox()'s own comment in the
    // .cpp for the full rationale. This block is the test/bench-only
    // surface for it: an arm gate that is never sticky and never
    // settable except through these hooks, a MOX-check context this
    // class has no other way to source (no SliceModel of its own), and
    // a read-only view of the trace ring setMox() writes to.

    // True once setTxArmedForTest(true) has been called and not since
    // cleared by disconnect()/onConnectTimeout()/onDataWatchdogTick() —
    // same *ForTest() naming and "every teardown path clears it"
    // discipline as isRxReadyForTest()/hasRadioAddrForTest() above.
    // m_txArmed is deliberately NEVER set true by any AppSettings-
    // persisted value, GUI control, or default: this is a bench-only
    // safety gate, not a preference, and arming is per-session by
    // design, not sticky across a teardown or a reconnect.
    bool isTxArmedForTest() const { return m_txArmed.load(std::memory_order_acquire); }
    void setTxArmedForTest(bool armed) { setTxArmed(armed); }

    // Reflects the CURRENT accepted MOX state (true only after a setMox()
    // call was actually accepted — armed AND BandPlanGuard allowed it),
    // not a pending request; a refused setMox() call never changes this.
    // Cleared the same way m_txArmed is, on every teardown path.
    bool isMoxForTest() const { return m_mox.load(std::memory_order_acquire); }

    // What setMox() checks an armed request against. See TxCheckContext's
    // own comment (top of this header) for why this exists at all.
    void setTxCheckContextForTest(const TxCheckContext& ctx) { m_txCheckContext = ctx; }
    TxCheckContext txCheckContextForTest() const { return m_txCheckContext; }

    // Read-only snapshot of the 50-entry TX trace ring, oldest entry
    // first. Lets a test inspect exactly what setMox()/setTxArmedForTest()
    // recorded without parsing qCWarning/qCInfo log output.
    QVector<TxTraceEntry> txTraceForTest() const;

    // ── Step 3 (SunSDR2 QRP TX-chain plan): pacer skeleton ──────────────
    //
    // Still zero wire reachability (see SunSdrTxPacer.h's own top-of-file
    // comment for this step's exact authorized scope). Forwards directly
    // to the owned SunSdrTxPacer instance's own pacerRunningForTest() —
    // same *ForTest() naming and "ask the real thing" discipline as this
    // class's other test accessors above. True only while setMox(true)
    // has been accepted and none of the four places that must stop the
    // pacer (setMox(false), disconnect(), onConnectTimeout(),
    // onDataWatchdogTick()) has since run.
    bool pacerRunningForTest() const {
        return m_txPacer && m_txPacer->pacerRunningForTest();
    }

    // ── Step 4 (SunSDR2 QRP TX-chain plan): read-only GUI accessors ─────
    //
    // Design synthesis quote that authorizes exactly this scope: "Network
    // Diagnostics wiring — still zero wire reachability. 'TX (SunSDR)'
    // row group: armed state, mox state, pace-repeat count, trace tail."
    // NetworkDiagnosticsDialog is a read-only status panel — it must
    // never call setTxArmedForTest()/setMox() itself — so it needs a
    // plain, non-test-named way to read the exact same state the
    // accessors above already expose for tests. Grep-verified 2026-09-02:
    // no other production .cpp file in this tree calls any class's own
    // *ForTest()-suffixed accessor, so a GUI-facing accessor gets its own
    // name here rather than reusing isTxArmedForTest()/isMoxForTest()/
    // txTraceForTest() directly, even though the underlying state is
    // identical.
    bool isTxArmed() const noexcept { return m_txArmed.load(std::memory_order_acquire); }
    bool isMoxOn() const noexcept { return m_mox.load(std::memory_order_acquire); }

    // Forwards to the owned SunSdrTxPacer's own pacerUnderrunsForTest() —
    // same null-guarded forwarding shape pacerRunningForTest() above
    // already uses for the same pacer. "Pace repeats" per the design
    // synthesis quote: every tick whose ring held less than a full
    // frame's worth of samples repeats the last built frame byte-
    // identical instead of building a fresh one (SunSdrTxPacer.h's own
    // onTick() comment) — this is that running count.
    quint32 txPaceRepeatCount() const {
        return m_txPacer ? m_txPacer->pacerUnderrunsForTest() : 0;
    }

    // The last `maxEntries` entries of the 50-entry TX trace ring,
    // oldest-first (same order txTraceForTest() already returns) — a
    // bounded view sized for a status panel with room for only a
    // handful of lines, not the full ring. Reuses txTraceForTest()'s own
    // ring-walk rather than re-deriving it a second time.
    QVector<TxTraceEntry> txTraceTail(int maxEntries) const;

    // Test-only hook: feed a raw stream-socket datagram, as if from the
    // connected radio itself, through the exact same decode path
    // onStreamReadyRead() uses, without needing a real bound UDP socket
    // and a loopback send. Production code never calls this — the real
    // path is the readyRead signal.
    void feedStreamDatagramForTest(const QByteArray& datagram);

    // Same idea, but with an explicit sender address — for testing the
    // fixed-port-sharing foreign-sender rejection in
    // processStreamDatagram() (found in review, 2026-08-28: this class
    // shares its well-known stream port with any prior session's
    // still-streaming QRP, so a real defense against that had to exist
    // and had to be testable without a real second UDP source).
    void feedStreamDatagramFromSenderForTest(const QByteArray& datagram,
                                              const QHostAddress& sender);

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

    // Step 2 of the SunSDR2 QRP TX-chain plan (design synthesis quote:
    // "Gate scaffolding — still zero wire reachability... real setMox()
    // body that calls BandPlanGuard::checkMoxAllowed() and — since no
    // pacer/antenna/PA code exists yet — can only ever log-and-refuse or
    // log-and-accept-with-no-wire-effect"). Sits outside the no-op block
    // below for the same reason setAttenuator() does: it is no longer
    // unconditionally a no-op (m_mox now genuinely changes on an armed,
    // allowed request), even though — unlike setAttenuator() — nothing
    // it does ever reaches the wire yet. Full body + rationale in the
    // .cpp.
    void setMox(bool enabled) override;

    // ── Safe no-ops: this connection is receive-only (plan doc §Phase B.5) ──
    //
    // Every one of these exists only because RadioConnection declares it
    // pure virtual for the P1/P2 TX path. None of them touch the wire.
    // "No PTT/drive code written at all yet" is a direct quote from the
    // design doc's own phasing gate, not a paraphrase.
    void setTxFrequency(quint64 frequencyHz) override;
    void setPreamp(bool enabled) override;
    void setTxDrive(int level) override;
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
    void onDataWatchdogTick();

private:
    // The DX/PRO/QRP profile table this connection uses for magic byte
    // and default ports. Resolved from RadioInfo::boardType at
    // connectToRadio() time — see resolveProfile().
    static const SunSdr::Profile& resolveProfile(HPSDRHW board);

    // Shared by onStreamReadyRead() (real socket data) and
    // feedStreamDatagramForTest()/feedStreamDatagramFromSenderForTest()
    // (synthetic test data) — one decode path, three ways in. Takes the
    // sender address so the fixed-port-sharing foreign-sender rejection
    // (found in review, 2026-08-28) lives in one place, testable without
    // a real socket — same shape as processControlDatagram() below.
    void processStreamDatagram(const QByteArray& datagram, const QHostAddress& sender);

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

    // ── Step 2 (SunSDR2 QRP TX-chain plan): bench-only TX gate scaffolding ──
    //
    // Still zero wire reachability (design synthesis quote in setMox()'s
    // own .cpp comment). m_txArmed is the bench-only arm gate — the ONLY
    // way it becomes true is setTxArmedForTest() (see that accessor's
    // comment for why it must never be settable any other way). m_mox
    // reflects the current ACCEPTED MOX state as a plain bool, not a
    // multi-value enum — mirrors this class's existing m_rxReady
    // convention (a lone atomic<bool> gate) rather than introducing a new
    // MoxState-shaped type for a two-state need.
    void setTxArmed(bool armed);
    std::atomic<bool> m_txArmed{false};
    std::atomic<bool> m_mox{false};

    // Bench/test-only BandPlanGuard input — see TxCheckContext's own
    // comment (top of this header). Not persisted, not read from
    // AppSettings, not touched by any teardown path (only m_txArmed and
    // m_mox reset there; a stale context left over from a prior armed
    // session is harmless because it's inert until the gate is armed
    // again).
    TxCheckContext m_txCheckContext;

    // The BandPlanGuard instance setMox() calls checkMoxAllowed() on —
    // pure data + pure functions, no Qt parent, default-constructed the
    // same way RadioModel's own m_bandPlan is (RadioModel.h:3119).
    safety::BandPlanGuard m_bandPlan;

    // 50-entry TX trace ring — see TxTraceEntry/TxTraceKind's own
    // comment (top of this header) for what it is and the events it
    // records. m_txTraceNext is the next slot to be OVERWRITTEN (so once
    // the ring has wrapped, it also points at the oldest surviving
    // entry); m_txTraceCount saturates at kTxTraceRingSize rather than
    // continuing to grow; m_txTraceSeq is the monotonic counter each new
    // entry gets, and — unlike m_txTraceCount/m_txTraceNext — is NOT
    // reset when disconnect() clears the ring, so sequence numbers stay
    // unique for the object's whole lifetime rather than restarting at 0
    // and potentially colliding with numbers a caller may have already
    // observed from a prior session.
    //
    // m_txTraceMutex guards all four fields below. Writers (pushTxTrace(),
    // disconnect()'s ring clear) run on this object's connection thread;
    // the reader (txTraceForTest(), and therefore txTraceTail()) is
    // called synchronously from NetworkDiagnosticsDialog::refresh() on
    // the GUI thread (QMutexLocker — same pattern this project already
    // uses for other cross-thread state, e.g. ReceiverManager's
    // m_routingMutex). TxTraceEntry carries a QString, so an unguarded
    // concurrent read/write here would be a real, not just theoretical,
    // data race.
    void pushTxTrace(TxTraceKind kind, const QString& reason);
    mutable QMutex m_txTraceMutex;
    std::array<TxTraceEntry, kTxTraceRingSize> m_txTrace{};
    int     m_txTraceCount{0};
    int     m_txTraceNext{0};
    quint32 m_txTraceSeq{0};

    // ── Step 3 (SunSDR2 QRP TX-chain plan): pacer skeleton ──────────────
    //
    // Still zero wire reachability — see SunSdrTxPacer.h's own top-of-
    // file comment for this step's exact scope. Same ownership pattern
    // as m_keepaliveTimer/m_dataWatchdog below: a raw pointer, Qt-
    // parented (`new SunSdrTxPacer(this)`), constructed in init(), never
    // explicitly deleted. setMox()'s accepted-true path calls
    // resetSeq()+start(); setMox()'s unconditional-false path and all
    // three real teardown paths (disconnect(), onConnectTimeout(),
    // onDataWatchdogTick()) call stop() — a pacer left running after any
    // of those four would be a "phantom pacer" ticking against a
    // disarmed or torn-down connection.
    SunSdrTxPacer* m_txPacer{nullptr};

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

    // Post-connect silence detection — found missing entirely during
    // review, 2026-08-28: nothing in this class re-armed after the
    // initial connect watchdog stopped (see processStreamDatagram()'s
    // Connecting->Connected promotion), so a QRP powered off or
    // unplugged mid-session left ConnectionState stuck at Connected
    // forever — the UI kept showing a live connection against dead
    // air (verification README Row 7's exact scenario). Mirrors
    // P1RadioConnection's own onWatchdogTick() silence-detection
    // pattern (P1RadioConnection.cpp, kWatchdogTickMs periodic tick
    // checking elapsed-since-last-frame), but simpler: this class has
    // no reconnect timer of its own (a fresh RadioConnection is always
    // created per connect attempt — see onConnectTimeout()'s own
    // comment), so a tripped watchdog here fully tears down to
    // LinkLost rather than arming a retry. kDataSilenceTimeoutMs
    // matches ConnectionState::LinkLost's own doc comment in
    // ConnectionState.h ("Was Connected; no frames for >5s") exactly.
    QTimer* m_dataWatchdog{nullptr};
    QElapsedTimer m_lastStreamPacketAt;
    static constexpr int kDataWatchdogTickMs = 1000;
    static constexpr int kDataSilenceTimeoutMs = 5000;

    const SunSdr::Profile* m_profile{nullptr};

    bool m_running{false};
    quint16 m_txSeq{0};   // control-channel-independent; used only for the
                          // periodic silent IQ-stream keepalive (design doc:
                          // "the host must keep sending silent 0xFE packets")
};

} // namespace Longpath
