// =================================================================
// src/core/audio/MicReorderBuffer.cpp  (NereusSDR)
// =================================================================
//
// See MicReorderBuffer.h for the design rationale (speculative
// zero-latency reordering; remote-bench measurement 2026-08-11).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-11 — Created for the remote-bench mic reordering fix by
//                 Ralph Martin Fischer (OE5SOS), AI-assisted
//                 implementation via Anthropic Claude (Cowork).
// =================================================================

#include "MicReorderBuffer.h"

namespace NereusSDR {

void MicReorderBuffer::push(quint32 seq, const Block& samples,
                            const EmitFn& emitFn)
{
    if (!m_valid) {
        m_valid   = true;
        m_nextSeq = seq;
    }

    // Signed distance handles the 2^32 wrap transparently.
    const qint32 delta = static_cast<qint32>(seq - m_nextSeq);

    if (delta < 0) {
        // Position already emitted (or concealed). Nothing to splice
        // it into — its moment has passed.
        ++m_stats.staleDropped;
        return;
    }

    if (delta == 0) {
        // The expected frame. If pending frames were held for it,
        // this is the late arrival being rescued into place.
        if (pendingCount() > 0) {
            ++m_stats.rescued;
        }
        emitBlock(samples, emitFn);
        m_nextSeq = seq + 1;
        drainPending(emitFn);
        return;
    }

    if (delta >= static_cast<qint32>(kResyncDelta)) {
        // Stream restart (radio reboot / SendStart re-issue): resync
        // rather than concealing a phantom gap of thousands.
        ++m_stats.resyncs;
        clearPending();
        emitBlock(samples, emitFn);
        m_nextSeq = seq + 1;
        return;
    }

    // Future frame — hold it back instead of emitting early.
    store(seq, samples);

    if (pendingCount() >= kDepth) {
        // Waited long enough: everything below the earliest held
        // frame is declared lost and concealed with the last emitted
        // block, then the held run drains behind it.
        quint32 first = 0;
        if (minPendingSeq(first)) {
            while (static_cast<qint32>(first - m_nextSeq) > 0) {
                emitBlock(m_lastEmitted, emitFn);
                ++m_stats.concealed;
                ++m_nextSeq;
            }
            drainPending(emitFn);
        }
    }
}

void MicReorderBuffer::reset()
{
    m_valid   = false;
    m_nextSeq = 0;
    m_lastEmitted.fill(0.0f);
    clearPending();
}

MicReorderBuffer::Stats MicReorderBuffer::takeStats()
{
    const Stats out = m_stats;
    m_stats = Stats{};
    return out;
}

int MicReorderBuffer::pendingCount() const noexcept
{
    int n = 0;
    for (const Slot& s : m_pending) {
        if (s.used) {
            ++n;
        }
    }
    return n;
}

void MicReorderBuffer::clearPending() noexcept
{
    for (Slot& s : m_pending) {
        s.used = false;
    }
}

bool MicReorderBuffer::minPendingSeq(quint32& out) const noexcept
{
    bool found = false;
    for (const Slot& s : m_pending) {
        if (!s.used) {
            continue;
        }
        if (!found || static_cast<qint32>(s.seq - out) < 0) {
            out   = s.seq;
            found = true;
        }
    }
    return found;
}

void MicReorderBuffer::store(quint32 seq, const Block& samples) noexcept
{
    // Duplicate of an already-held frame: overwrite in place.
    for (Slot& s : m_pending) {
        if (s.used && s.seq == seq) {
            s.data = samples;
            return;
        }
    }
    for (Slot& s : m_pending) {
        if (!s.used) {
            s.used = true;
            s.seq  = seq;
            s.data = samples;
            return;
        }
    }
    // Store full (should not happen with kSlots > kDepth): evict the
    // OLDEST held frame after concealing up to it would be complex;
    // dropping the newcomer keeps the invariant simple and the case
    // is unreachable in practice because push() drains at kDepth.
}

void MicReorderBuffer::emitBlock(const Block& b, const EmitFn& emitFn)
{
    if (emitFn) {
        emitFn(b.data(), kBlockFrames);
    }
    m_lastEmitted = b;
}

void MicReorderBuffer::drainPending(const EmitFn& emitFn)
{
    // Emit consecutively while the next expected frame is held.
    for (;;) {
        bool advanced = false;
        for (Slot& s : m_pending) {
            if (s.used && s.seq == m_nextSeq) {
                emitBlock(s.data, emitFn);
                s.used = false;
                ++m_nextSeq;
                advanced = true;
                break;
            }
        }
        if (!advanced) {
            return;
        }
    }
}

} // namespace NereusSDR
