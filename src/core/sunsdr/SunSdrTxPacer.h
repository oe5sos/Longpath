#pragma once

// no-port-check: NereusSDR/Longpath-original. Cites facts from
// ArtemisSDR's sunsdr.c (TX pacing cadence, empty-ring-repeats-last-
// packet behavior) the same way SunSdrRadioConnection.{h,cpp} already
// cite ArtemisSDR facts elsewhere in this driver — no ArtemisSDR C code
// is copied here, only the wire/timing FACTS the design doc's own "IQ
// stream" section already documents. See that section
// (docs/architecture/2026-08-24-sunsdr-native-driver-design.md,
// "TX packet sequence numbers reset to 0...") for the citations this
// file's own comments point back to.

// =================================================================
// src/core/sunsdr/SunSdrTxPacer.h  (NereusSDR/Longpath)
// =================================================================
//
// Step 3 of the SunSDR2 QRP TX-chain plan (operator-approved, 6 steps
// total — see SunSdrRadioConnection.cpp's setMox() comment for Step 2's
// own scope quote, and this step's own scope, quoted verbatim from the
// design synthesis that authorized it:
//
//   "SunSdrTxPacer skeleton (QTimer::PreciseTimer, no new thread) —
//   still zero wire reachability. Ring buffer, seq-reset-on-arm,
//   empty-ring-repeats-last-frame (byte-identical) + m_pacerUnderruns.
//   Ticks build real frames via step 1's encoders into
//   lastTxFrameForTest() — no socket send yet."
//
// A QTimer(Qt::PreciseTimer)-based QObject, NOT a QThread — the design
// synthesis explicitly rejected a new dedicated OS thread for this step,
// citing P1RadioConnection's own m_ep2PacerTimer (P1RadioConnection.cpp,
// kEp2PacerIntervalMs = 2ms, Qt::PreciseTimer) as real local precedent
// for exactly this kind of tight-interval pacing without a new OS
// thread. Escalating to a dedicated thread is Step 5's decision (a live
// bench jitter measurement with the operator present), not pre-empted
// here.
//
// THE TICK NEVER SENDS ANYTHING TO A SOCKET. onTick() only ever updates
// this object's own m_lastFrame (exposed via lastTxFrameForTest()). No
// QUdpSocket, no networking header, nothing socket-shaped appears
// anywhere in this file or its .cpp — that wiring is Step 6, explicitly
// out of scope here, same as Step 2 left setMox() with zero wire effect.
//
// ── IQ-frame-shape design call ──────────────────────────────────────
//
// The design doc's "IQ stream" section documents the TX-active frame
// shape as byte-identical in HEADER LAYOUT to the RX-idle frame this
// driver already sends every 2s from onKeepaliveTimeout() — only the
// opcode (0xFD kOpIqTxActive vs. 0xFE kOpIqRxIdle) and the payload
// content (200 real 6-byte I/Q samples vs. 1200 zero bytes) differ:
//
//   [0] magic0  [1] 0xFF  [2] opcode  [3] 0xFF
//   [4:5] payload size u16 LE (1200)  [6:7] seq u16 LE  [8:9] state bytes
//   [10:] 200 x 6-byte I/Q samples
//
// SunSdrProtocol::buildIqHeader() ALREADY builds exactly this 10-byte
// header (it's what onKeepaliveTimeout() already calls) — so
// buildTxIqFrame() below reuses that existing, already-tested builder
// rather than re-deriving the header layout by hand a second time here.
// This is narrower than it may look: no new opcode or byte-layout FACT
// is introduced (the header shape and both opcodes were already
// confirmed facts in SunSdrProtocol.h before this file existed) — the
// only thing genuinely new in this file is the ring-buffer plumbing
// that decides what payload bytes go after that existing header, and
// that plumbing deliberately stays here rather than expanding
// SunSdrProtocol.h's own scope for a Step-3 skeleton. See
// buildTxIqFrame()'s own comment in the .cpp for the state-byte values
// (0x02/0x01) reused from SunSdrProtocol.h's own citation of
// ArtemisSDR's TX-active path (sunsdr.c:1699-1701).
//
// Still deliberately NOT here: sendTxIq()'s real wiring into the ring
// (a later step — this step's own push interface exists so a test can
// push synthetic sample bytes directly, per the design synthesis quote
// above), any bench-measured real achievable pacing interval (Step 5),
// and anything socket-shaped (Step 6).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-02 — Original for NereusSDR/Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork). Step 3 of
//                 the operator-approved 6-step SunSDR2 QRP TX-chain
//                 plan.
// =================================================================

#include "core/sunsdr/SunSdrProtocol.h"

#include <QByteArray>
#include <QObject>
#include <QTimer>

#include <array>
#include <atomic>
#include <cstdint>

namespace Longpath {

// Fixed ring capacity, in 6-byte I/Q sample groups. The design doc pins
// the per-frame count (200 samples/frame — kIqComplexPerPkt) but not a
// ring capacity; this is a Step 3 implementation choice, not a cited
// fact: four frames' worth of headroom (800 samples) between now and
// whichever later step actually wires sendTxIq() to fill this ring.
inline constexpr int kTxPacerRingCapacitySamples = SunSdr::kIqComplexPerPkt * 4;

class SunSdrTxPacer : public QObject {
    Q_OBJECT

public:
    explicit SunSdrTxPacer(QObject* parent = nullptr);
    ~SunSdrTxPacer() override = default;

    // Design doc: "TX packets are paced on a dedicated high-resolution
    // timer thread at exactly 5.12 ms per packet (195.3125 pps)".
    // QTimer::setInterval(int) only accepts whole milliseconds — Qt6
    // has no fractional-millisecond timer interval API — so 5.12ms
    // cannot be represented exactly. Rounding to the nearest whole
    // millisecond (5ms, 200 pps) is the closest this timer can request,
    // NOT the bench-confirmed real interval; Qt::PreciseTimer itself is
    // only a *request* for OS-timer-resolution accuracy, not a
    // guarantee (P1RadioConnection's own kEp2PacerIntervalMs comment:
    // "PreciseTimer only delivers ~10 ms ticks on Windows"). The actual
    // achievable pacing rate against real hardware is Step 5's bench
    // jitter measurement, not this skeleton's job — this constant is a
    // documented approximation, not a claim of precision.
    static constexpr int kTxPaceIntervalMs = 5;

    // Sets which profile's magic byte buildTxIqFrame() encodes into
    // each tick's header. Defaults to SunSdr::kProfileQrp — the only
    // profile SunSdrRadioConnection::resolveProfile() currently ever
    // resolves to (its non-QRP branch also falls back to the QRP
    // profile — see that function's own comment), so a pacer
    // constructed before connectToRadio() has run anything still builds
    // a correct frame if ticked. SunSdrRadioConnection::connectToRadio()
    // calls this again once it resolves its own m_profile, so the two
    // never drift if a second profile is ever added later.
    void setProfile(const SunSdr::Profile& profile) { m_profile = &profile; }

    // Starts/stops the pacing timer. Idempotent either direction (Qt's
    // own QTimer::start()/stop() already are). Called from
    // SunSdrRadioConnection::setMox()'s accepted-true path (start(),
    // after resetSeq()) and unconditionally from setMox(false) plus all
    // three real teardown paths (stop()) — see that class's own setMox()
    // and teardown comments for why every one of those must reach
    // stop(), not just the "normal" ones.
    void start();
    void stop();

    // True only once has-actually-been-started AND not since stopped —
    // delegates directly to the underlying QTimer's own isActive(), the
    // same "ask the real thing, don't track a shadow bool" discipline
    // this class's start()/stop() already lean on.
    bool pacerRunningForTest() const;

    // Resets the TX packet sequence number to 0. Called externally on
    // every PTT-on (design doc: "TX packet sequence numbers reset to 0
    // on every PTT-on — carrying them monotonically across sessions
    // makes the radio silently drop packets as out-of-order, producing
    // a keyed, unmodulated carrier") — SunSdrRadioConnection::setMox()'s
    // already-existing accepted-true path is what actually calls this;
    // the "on every PTT-on" semantics live there, not in this class.
    void resetSeq() { m_seq = 0; }

    // Pushes one pre-encoded 6-byte I/Q sample group (SunSdr::
    // kIqBytesPerComplex) onto the ring. Returns false (and pushes
    // nothing) if `sixBytes` isn't exactly 6 bytes long or the ring is
    // already at kTxPacerRingCapacitySamples — a full ring silently
    // dropping the newest sample, rather than growing unbounded or
    // overwriting the oldest still-unsent one, is the right failure mode
    // for a LATER caller (sendTxIq()'s real wiring, not this step) to
    // detect and back off from; this step's own tests push synthetic
    // sample bytes directly, per the design synthesis's own instruction.
    bool pushSample(const QByteArray& sixBytes);

    // Number of samples currently queued — lets a test confirm a ring
    // is genuinely empty (or genuinely holds >=200) before ticking,
    // rather than inferring ring state indirectly from tick output.
    int ringSampleCountForTest() const { return m_ringCount; }

    // Forces one tick's worth of processing synchronously, without
    // waiting for the real QTimer — same rationale as this project's
    // other timer-driven classes exposing a synthetic-tick test hook
    // (e.g. Rf2ksConnection's onReconnectTimeout(), invoked directly via
    // QMetaObject::invokeMethod in its own tests) rather than sleeping
    // a real 5.12ms in a unit test.
    void tickForTest() { onTick(); }

    // Most recently built (or, on an underrun tick, most recently
    // repeated) frame — a full IQ-stream packet: 10-byte header + the
    // 1200-byte payload built from up to 200 queued samples. Empty
    // (default QByteArray) until the first tick that finds >=200
    // samples in the ring ever runs.
    QByteArray lastTxFrameForTest() const { return m_lastFrame; }

    // Increments every tick the ring held fewer than
    // SunSdr::kIqComplexPerPkt samples and the last frame had to be
    // repeated byte-identical instead of a fresh one being built —
    // design doc: "an empty pacing ring repeats the last packet rather
    // than emitting silence, to avoid an audible gap". std::atomic:
    // onTick() increments this on this object's own thread (parented to
    // SunSdrRadioConnection, which lives on RadioModel's connection
    // thread), but NetworkDiagnosticsDialog::refresh() reads it via
    // SunSdrRadioConnection::txPaceRepeatCount() synchronously from the
    // GUI thread — same cross-thread-read reasoning as this class's
    // sibling m_txArmed/m_mox atomics in SunSdrRadioConnection.h.
    quint32 pacerUnderrunsForTest() const
    {
        return m_pacerUnderruns.load(std::memory_order_acquire);
    }

    // The TX packet sequence number the NEXT built frame will carry —
    // lets a test confirm resetSeq()'s effect directly rather than only
    // indirectly via lastTxFrameForTest()'s raw header bytes.
    quint16 pacerSeqForTest() const { return m_seq; }

signals:
    // Step 5 instrumentation (2026-09-02): fired at the top of every
    // onTick() call, whatever triggered it — the real QTimer, or
    // tickForTest()'s synthetic path (existing unit tests call
    // tickForTest() directly and don't connect to this signal, so they
    // are unaffected either way). Exists solely so an external probe
    // (tools/sunsdr_tx_pacer_jitter_probe.cpp) — which only ever calls
    // start() and lets the real timer run — can timestamp each real tick
    // with QElapsedTimer and measure this timer's genuine cadence/jitter
    // under a real Qt event loop. That is the bench jitter measurement
    // the design synthesis reserved for Step 5: deciding whether
    // Qt::PreciseTimer holds 5.12ms closely enough, or a dedicated OS
    // thread is warranted (that escalation itself stays a
    // maintainer-review decision, not something this signal pre-empts).
    // Carries no data — the probe only needs the arrival time, not
    // anything about ring/frame state.
    void tickFiredForTest();

private slots:
    void onTick();

private:
    // Pops up to SunSdr::kIqComplexPerPkt samples off the ring and
    // builds a real 1210-byte IQ-active frame from them via
    // SunSdrProtocol::buildIqHeader() (see this header's own top-of-file
    // comment for the frame-shape design call). Only called from
    // onTick() when the ring actually holds a full frame's worth.
    QByteArray buildTxIqFrame();

    QTimer* m_timer{nullptr};

    const SunSdr::Profile* m_profile{&SunSdr::kProfileQrp};

    quint16 m_seq{0};

    QByteArray m_lastFrame;
    std::atomic<quint32> m_pacerUnderruns{0};

    // Fixed-capacity ring of pending 6-byte I/Q sample groups — a plain
    // array (no heap ring growth) sized by kTxPacerRingCapacitySamples,
    // with m_ringHead/m_ringCount tracking the live window the same
    // shape SunSdrRadioConnection's own m_txTrace ring already uses
    // (m_txTraceNext/m_txTraceCount there).
    std::array<QByteArray, kTxPacerRingCapacitySamples> m_ring{};
    int m_ringHead{0};
    int m_ringCount{0};
};

} // namespace Longpath
