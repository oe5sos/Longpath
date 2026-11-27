// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port. Siehe KiwiReplayBuffer.h.
//
//   2026-11-27 — Neu angelegt ("Nachhoeren").

#include "core/KiwiReplayBuffer.h"

#include <algorithm>

namespace Longpath {

void KiwiReplayBuffer::begin()
{
    clear();
    m_recording = true;
}

void KiwiReplayBuffer::feed(const QByteArray& pcm24kStereoFloat)
{
    if (!m_recording) { return; }
    m_data.append(pcm24kStereoFloat);
    const qsizetype limit = qsizetype(kMaxMs) * kBytesPerMs;
    if (m_data.size() > limit) {
        // In ganzen Rahmen vorne abschneiden.
        qsizetype drop = m_data.size() - limit;
        drop -= drop % 8;
        m_data.remove(0, drop);
    }
}

void KiwiReplayBuffer::finish(int skipLeadingMs)
{
    if (!m_recording) { return; }
    m_recording = false;
    m_readPos = std::min<qsizetype>(
        qsizetype(std::max(0, skipLeadingMs)) * kBytesPerMs, m_data.size());
    m_playing = m_readPos < m_data.size();
}

QByteArray KiwiReplayBuffer::nextChunk(int ms)
{
    if (!m_playing || ms <= 0) { return {}; }
    const qsizetype n = std::min<qsizetype>(qsizetype(ms) * kBytesPerMs,
                                            m_data.size() - m_readPos);
    const QByteArray part = m_data.mid(m_readPos, n);
    m_readPos += n;
    if (m_readPos >= m_data.size()) { clear(); }
    return part;
}

void KiwiReplayBuffer::clear()
{
    m_data.clear();
    m_readPos = 0;
    m_recording = false;
    m_playing = false;
}

} // namespace Longpath
