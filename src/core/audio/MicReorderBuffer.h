// =================================================================
// src/core/audio/MicReorderBuffer.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original (no Thetis equivalent: Thetis network.c:761-772
// [v2.10.3.13] discards the port-1026 mic sequence number entirely).
//
// Why this exists — network investigation 2026-08-11, remote bench:
// the socket-level mic sequence audit showed that on a routed/WLAN
// path the dominant defect is REORDERING, not loss. In almost every
// 5 s window "LOST n / out-of-order n" cancelled out — frames arrive,
// just late — with true loss around 0.1-0.3% and occasional correlated
// bursts. TxMicSource::inbound() consumes frames in ARRIVAL order, so
// every late frame was audible twice: once as the gap where it
// belonged, once as stale audio spliced where it did not.
//
// Design — speculative zero-latency reordering:
//
//  * In-order frames pass straight through: ZERO added latency on a
//    clean link. This is deliberately NOT a classic jitter buffer
//    that delays everything by its depth.
//  * A future frame (gap ahead of it) is HELD in a small pending
//    store instead of being emitted early. When the missing frame
//    arrives, it is emitted and the held run drains after it —
//    catching the measured swap/late pattern exactly.
//  * If the gap does not fill by the time kDepth frames are pending,
//    the missing frames are declared lost and CONCEALED by repeating
//    the last emitted block (a 1.33 ms splice of similar spectrum
//    beats a hole of zeros), then the held run drains.
//  * A stale frame (seq already emitted/concealed past it) is
//    dropped — its slot has already been filled.
//  * A forward jump >= kResyncDelta is treated as a stream restart
//    (radio rebooted / SendStart): state resets, no phantom
//    concealment burst.
//
// Latency cost: zero steady-state; transient hold of at most kDepth
// frames (kDepth * 64 / 48000 s ≈ 5.3 ms at the default depth 4)
// while an actual reorder/loss event is in flight. The downstream
// TxMicSource ring (8 blocks) absorbs the catch-up burst.
//
// Threading: none. Single-caller class, invoked from the connection
// thread's port-1026 handler. All state is plain members.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-11 — Created for the remote-bench mic reordering fix by
//                 Ralph Martin Fischer (OE5SOS), AI-assisted
//                 implementation via Anthropic Claude (Cowork).
// =================================================================

#pragma once

#include <QtGlobal>

#include <array>
#include <cstdint>
#include <functional>

namespace Longpath {

class MicReorderBuffer {
public:
    /// Mono frames per mic block — mirrors TxMicSource::kBlockFrames
    /// (P2 mic.spp = 64, netInterface.c:1458 [v2.10.3.13]).
    static constexpr int kBlockFrames = 64;

    /// How many future frames may pend before the gap under them is
    /// declared lost and concealed. 4 frames ≈ 5.3 ms — an order of
    /// magnitude above the reorder distances seen on the 2026-08-11
    /// remote bench (1-2 frames), far below anything audible.
    static constexpr int kDepth = 4;

    /// Pending store capacity. Must be >= kDepth; sized with headroom
    /// so a burst arriving while a drain is due never evicts.
    static constexpr int kSlots = 8;

    /// Forward jump treated as a stream restart instead of loss.
    static constexpr quint32 kResyncDelta = 64;

    using Block  = std::array<float, kBlockFrames>;
    /// Emit callback: one 64-frame block, in sequence order.
    using EmitFn = std::function<void(const float*, int)>;

    /// Window statistics — reset by takeStats(). "rescued" counts
    /// frames that arrived late but were slotted into their correct
    /// position (the frames the pre-buffer audit booked as
    /// out-of-order); "concealed" counts repeats emitted for frames
    /// that never came; "staleDropped" counts arrivals for positions
    /// already emitted; "resyncs" counts stream restarts.
    struct Stats {
        quint64 rescued{0};
        quint64 concealed{0};
        quint64 staleDropped{0};
        quint64 resyncs{0};
        bool any() const noexcept
        {
            return rescued || concealed || staleDropped || resyncs;
        }
    };

    MicReorderBuffer() = default;

    /// Feed one decoded mic frame. Emits zero or more blocks in
    /// sequence order through `emit` (possibly several when a gap
    /// fills or is concealed).
    void push(quint32 seq, const Block& samples, const EmitFn& emitFn);

    /// Forget everything (fresh session / SendStart). Keeps stats.
    void reset();

    /// Read-and-clear the window statistics.
    Stats takeStats();

    /// Test seam: number of frames currently held back.
    int pendingCountForTest() const noexcept { return pendingCount(); }

private:
    struct Slot {
        bool    used{false};
        quint32 seq{0};
        Block   data{};
    };

    int  pendingCount() const noexcept;
    void clearPending() noexcept;
    bool minPendingSeq(quint32& out) const noexcept;
    void store(quint32 seq, const Block& samples) noexcept;
    void emitBlock(const Block& b, const EmitFn& emitFn);
    void drainPending(const EmitFn& emitFn);

    bool    m_valid{false};
    quint32 m_nextSeq{0};
    Block   m_lastEmitted{};   // zeros until the first emit — a
                               // concealment before any audio is
                               // silence, which is correct.
    std::array<Slot, kSlots> m_pending{};
    Stats   m_stats;
};

} // namespace Longpath
