// no-port-check: NereusSDR/Longpath-original. See header for scope and
// citations.

// =================================================================
// src/core/sunsdr/SunSdrTxPacer.cpp  (NereusSDR/Longpath)
// =================================================================
//
// NereusSDR/Longpath-original. Scope and rationale in the header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-02 — Original for NereusSDR/Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "SunSdrTxPacer.h"

namespace Longpath {

SunSdrTxPacer::SunSdrTxPacer(QObject* parent)
    : QObject(parent)
{
    // Same construction shape as SunSdrRadioConnection's own
    // m_keepaliveTimer/m_dataWatchdog (init()'s `new QTimer(this)` +
    // connect(...timeout...)): parented to this object (Qt-parented, no
    // raw new/delete anywhere this pointer is later torn down by hand),
    // constructed but not started here — start()/stop() are called
    // externally, matching how those two timers are armed/disarmed by
    // SunSdrRadioConnection rather than started at construction time.
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(kTxPaceIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &SunSdrTxPacer::onTick);
}

void SunSdrTxPacer::start()
{
    if (m_timer) {
        m_timer->start();
    }
}

void SunSdrTxPacer::stop()
{
    if (m_timer) {
        m_timer->stop();
    }
}

bool SunSdrTxPacer::pacerRunningForTest() const
{
    return m_timer && m_timer->isActive();
}

bool SunSdrTxPacer::pushSample(const QByteArray& sixBytes)
{
    if (sixBytes.size() != SunSdr::kIqBytesPerComplex) {
        return false;
    }
    if (m_ringCount >= kTxPacerRingCapacitySamples) {
        return false;  // full — see this method's own header comment
    }

    const int tail = (m_ringHead + m_ringCount) % kTxPacerRingCapacitySamples;
    m_ring[static_cast<std::size_t>(tail)] = sixBytes;
    ++m_ringCount;
    return true;
}

QByteArray SunSdrTxPacer::buildTxIqFrame()
{
    // Consume exactly kIqComplexPerPkt samples off the ring (the caller,
    // onTick(), only calls this once it has confirmed the ring holds at
    // least that many) and concatenate them into the 1200-byte payload.
    QByteArray payload;
    payload.reserve(SunSdr::kIqPayloadSize);
    for (int i = 0; i < SunSdr::kIqComplexPerPkt; ++i) {
        payload += m_ring[static_cast<std::size_t>(m_ringHead)];
        m_ringHead = (m_ringHead + 1) % kTxPacerRingCapacitySamples;
        --m_ringCount;
    }

    // Header: SunSdrProtocol::buildIqHeader() already builds exactly the
    // 10-byte layout the design doc's "IQ stream" section documents —
    // see this class's own header-file comment for why that existing
    // builder is reused rather than a new one written by hand.
    //
    // State bytes (8:9): reuses the exact 0x02/0x01 pair
    // SunSdrProtocol.h's own IqHeader comment already cites for
    // ArtemisSDR's TX-active path ("ArtemisSDR's TX-active path uses
    // byte8=0x02/byte9=0x01, sunsdr.c:1699-1701 [@f8b01d25c5]") — not a
    // new fact introduced here, the same citation SunSdrProtocol.h
    // already carries, reused at its one currently-relevant call site.
    const SunSdr::Profile& profile = m_profile ? *m_profile : SunSdr::kProfileQrp;
    QByteArray frame = SunSdr::buildIqHeader(profile, SunSdr::kOpIqTxActive,
                                              m_seq, /*byte8=*/0x02,
                                              /*byte9=*/0x01);
    frame += payload;

    // The sequence number just consumed belongs to the frame just built;
    // advance for the NEXT frame. An underrun tick (see onTick()) must
    // NOT reach this function at all, so it never advances m_seq or
    // touches m_lastFrame — repeating the cached frame means repeating
    // its embedded sequence number too, which is the point (design doc:
    // "an empty pacing ring repeats the last packet ... to avoid an
    // audible gap" — a genuine byte-for-byte repeat, seq included, not a
    // new packet that merely resembles the old one).
    ++m_seq;
    return frame;
}

void SunSdrTxPacer::onTick()
{
    emit tickFiredForTest();

    if (m_ringCount < SunSdr::kIqComplexPerPkt) {
        // Empty (or merely partial) ring: resend the cached last frame
        // byte-identical rather than building anything new — design
        // doc's explicit rule. m_lastFrame is simply left untouched
        // (still holds whatever the previous real build produced, or
        // stays the default-constructed empty QByteArray if no frame has
        // ever been built yet). THE TICK NEVER SENDS ANYTHING TO A
        // SOCKET either way — see this file's own header comment.
        m_pacerUnderruns.fetch_add(1, std::memory_order_release);
        return;
    }

    m_lastFrame = buildTxIqFrame();
}

} // namespace Longpath
