// =================================================================
// tests/tst_mic_reorder_buffer.cpp  (NereusSDR)
// =================================================================
//
// Pins the MicReorderBuffer contract (remote-bench fix 2026-08-11):
// speculative zero-latency reordering between the P2 port-1026
// decoder and TxMicSource. The scenarios mirror the defect classes
// the socket-level sequence audit measured on the routed/WLAN path:
// clean runs, single swaps, short late runs, true loss, stale
// arrivals, stream restarts, and the 2^32 wrap.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-11 — Created by Ralph Martin Fischer (OE5SOS),
//                 AI-assisted implementation via Anthropic Claude
//                 (Cowork).
// =================================================================

#include <QtTest>

#include "core/audio/MicReorderBuffer.h"

#include <vector>

using NereusSDR::MicReorderBuffer;

namespace {

// A block whose every sample encodes its sequence number, so emission
// order is directly checkable at the output.
MicReorderBuffer::Block taggedBlock(quint32 seq)
{
    MicReorderBuffer::Block b{};
    b.fill(static_cast<float>(seq));
    return b;
}

struct Sink {
    std::vector<float> tags;   // first sample of each emitted block
    MicReorderBuffer::EmitFn fn()
    {
        return [this](const float* s, int n) {
            QCOMPARE(n, MicReorderBuffer::kBlockFrames);
            tags.push_back(s[0]);
        };
    }
};

} // namespace

class TstMicReorderBuffer : public QObject {
    Q_OBJECT

private slots:
    void inOrderPassesThroughImmediately();
    void singleSwapIsRescued();
    void lateRunDrainsInOrder();
    void trueLossIsConcealedAtDepth();
    void staleFrameIsDropped();
    void duplicatePendingIsHarmless();
    void forwardJumpResyncs();
    void wrapAroundIsSeamless();
    void resetForgetsState();
};

// A clean in-order stream must add zero latency: every push emits
// its own block before returning.
void TstMicReorderBuffer::inOrderPassesThroughImmediately()
{
    MicReorderBuffer rb;
    Sink sink;
    for (quint32 s = 100; s < 110; ++s) {
        rb.push(s, taggedBlock(s), sink.fn());
        QCOMPARE(sink.tags.size(), size_t(s - 99));
        QCOMPARE(sink.tags.back(), float(s));
    }
    QCOMPARE(rb.pendingCountForTest(), 0);
    const auto st = rb.takeStats();
    QVERIFY(!st.any());
}

// The dominant bench pattern: two adjacent frames swapped on the
// wire. The early frame is held, the late one slots into place, both
// come out in sequence order.
void TstMicReorderBuffer::singleSwapIsRescued()
{
    MicReorderBuffer rb;
    Sink sink;
    rb.push(1, taggedBlock(1), sink.fn());
    rb.push(3, taggedBlock(3), sink.fn());   // early — held
    QCOMPARE(sink.tags, (std::vector<float>{1.0f}));
    QCOMPARE(rb.pendingCountForTest(), 1);
    rb.push(2, taggedBlock(2), sink.fn());   // the late one
    QCOMPARE(sink.tags, (std::vector<float>{1.0f, 2.0f, 3.0f}));
    QCOMPARE(rb.pendingCountForTest(), 0);
    const auto st = rb.takeStats();
    QCOMPARE(st.rescued, quint64(1));
    QCOMPARE(st.concealed, quint64(0));
}

// A missing frame with a short run of successors held behind it:
// when it arrives, the whole run drains in order.
void TstMicReorderBuffer::lateRunDrainsInOrder()
{
    MicReorderBuffer rb;
    Sink sink;
    rb.push(10, taggedBlock(10), sink.fn());
    rb.push(12, taggedBlock(12), sink.fn());
    rb.push(13, taggedBlock(13), sink.fn());
    rb.push(14, taggedBlock(14), sink.fn());
    QCOMPARE(sink.tags, (std::vector<float>{10.0f}));
    rb.push(11, taggedBlock(11), sink.fn());
    QCOMPARE(sink.tags,
             (std::vector<float>{10.0f, 11.0f, 12.0f, 13.0f, 14.0f}));
}

// The frame never arrives: at kDepth pending, the hole is concealed
// with a REPEAT of the last emitted block (not zeros) and the run
// drains behind it.
void TstMicReorderBuffer::trueLossIsConcealedAtDepth()
{
    MicReorderBuffer rb;
    Sink sink;
    rb.push(20, taggedBlock(20), sink.fn());
    // 21 lost; 22..25 arrive.
    rb.push(22, taggedBlock(22), sink.fn());
    rb.push(23, taggedBlock(23), sink.fn());
    rb.push(24, taggedBlock(24), sink.fn());
    QCOMPARE(sink.tags, (std::vector<float>{20.0f}));   // still waiting
    rb.push(25, taggedBlock(25), sink.fn());            // kDepth reached
    // Concealment repeats block 20 in 21's slot, then 22..25 drain.
    QCOMPARE(sink.tags,
             (std::vector<float>{20.0f, 20.0f, 22.0f, 23.0f, 24.0f, 25.0f}));
    const auto st = rb.takeStats();
    QCOMPARE(st.concealed, quint64(1));
    QCOMPARE(st.rescued, quint64(0));
    // 26 continues normally afterwards.
    rb.push(26, taggedBlock(26), sink.fn());
    QCOMPARE(sink.tags.back(), 26.0f);
}

// A frame for a position already emitted (or concealed past) has no
// slot left — it is dropped, not spliced somewhere wrong.
void TstMicReorderBuffer::staleFrameIsDropped()
{
    MicReorderBuffer rb;
    Sink sink;
    rb.push(5, taggedBlock(5), sink.fn());
    rb.push(6, taggedBlock(6), sink.fn());
    rb.push(5, taggedBlock(5), sink.fn());   // stale duplicate
    QCOMPARE(sink.tags, (std::vector<float>{5.0f, 6.0f}));
    const auto st = rb.takeStats();
    QCOMPARE(st.staleDropped, quint64(1));
}

// The same future frame arriving twice while held must not occupy two
// slots or emit twice.
void TstMicReorderBuffer::duplicatePendingIsHarmless()
{
    MicReorderBuffer rb;
    Sink sink;
    rb.push(1, taggedBlock(1), sink.fn());
    rb.push(3, taggedBlock(3), sink.fn());
    rb.push(3, taggedBlock(3), sink.fn());   // duplicate of the held one
    QCOMPARE(rb.pendingCountForTest(), 1);
    rb.push(2, taggedBlock(2), sink.fn());
    QCOMPARE(sink.tags, (std::vector<float>{1.0f, 2.0f, 3.0f}));
}

// A forward jump >= kResyncDelta is a stream restart, not a gap of
// thousands to conceal.
void TstMicReorderBuffer::forwardJumpResyncs()
{
    MicReorderBuffer rb;
    Sink sink;
    rb.push(100, taggedBlock(100), sink.fn());
    // Delta is measured against the NEXT expected seq (101), so the
    // smallest resyncing jump from here is 101 + kResyncDelta.
    const quint32 jump = 101 + MicReorderBuffer::kResyncDelta;
    rb.push(jump, taggedBlock(jump), sink.fn());
    QCOMPARE(sink.tags.size(), size_t(2));
    const auto st = rb.takeStats();
    QCOMPARE(st.resyncs, quint64(1));
    QCOMPARE(st.concealed, quint64(0));
    // Stream continues from the new position.
    rb.push(jump + 1, taggedBlock(jump + 1), sink.fn());
    QCOMPARE(sink.tags.size(), size_t(3));
}

// Sequence 0xFFFFFFFF -> 0 must behave exactly like n -> n+1,
// including a swap straddling the wrap.
void TstMicReorderBuffer::wrapAroundIsSeamless()
{
    MicReorderBuffer rb;
    Sink sink;
    rb.push(0xFFFFFFFEu, taggedBlock(0xFFFFFFFEu), sink.fn());
    rb.push(0u, taggedBlock(0u), sink.fn());          // early (wrap) — held
    QCOMPARE(sink.tags.size(), size_t(1));
    rb.push(0xFFFFFFFFu, taggedBlock(0xFFFFFFFFu), sink.fn());
    QCOMPARE(sink.tags.size(), size_t(3));
    QCOMPARE(sink.tags[1], float(0xFFFFFFFFu));
    QCOMPARE(sink.tags[2], 0.0f);
    rb.push(1u, taggedBlock(1u), sink.fn());
    QCOMPARE(sink.tags.back(), 1.0f);
    const auto st = rb.takeStats();
    QCOMPARE(st.rescued, quint64(1));
}

// reset() (SendStart) forgets sequence state and pending frames; the
// next frame re-anchors without loss accounting.
void TstMicReorderBuffer::resetForgetsState()
{
    MicReorderBuffer rb;
    Sink sink;
    rb.push(50, taggedBlock(50), sink.fn());
    rb.push(52, taggedBlock(52), sink.fn());   // held
    QCOMPARE(rb.pendingCountForTest(), 1);
    rb.takeStats();
    rb.reset();
    QCOMPARE(rb.pendingCountForTest(), 0);
    rb.push(7, taggedBlock(7), sink.fn());     // fresh anchor
    QCOMPARE(sink.tags.back(), 7.0f);
    const auto st = rb.takeStats();
    QVERIFY(!st.any());
}

QTEST_GUILESS_MAIN(TstMicReorderBuffer)
#include "tst_mic_reorder_buffer.moc"
