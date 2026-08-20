// =================================================================
// src/core/strip/MicSpectrum.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See MicSpectrum.h for why the audio thread does
// nothing here but copy.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/MicSpectrum.h"

#include <algorithm>
#include <cstring>

namespace Longpath {

MicSpectrum::MicSpectrum(int sampleRate)
{
    setSampleRate(sampleRate);
}

void MicSpectrum::setSampleRate(int hz)
{
    if (hz <= 0) { return; }
    m_sampleRate = hz;
    m_ring.assign(static_cast<size_t>(hz) * kSeconds, 0.0f);
    m_write.store(0, std::memory_order_release);
    m_seen.store(0, std::memory_order_release);
}

void MicSpectrum::feed(const float* samples, int frames) noexcept
{
    if (!samples || frames <= 0 || m_ring.empty()) { return; }

    const size_t cap = m_ring.size();
    size_t w = m_write.load(std::memory_order_relaxed);

    // Wrapping rather than dropping the tail, unlike TxAudioRecorder.
    // The two want opposite things: a recording must not silently
    // replace its own beginning, and a live view must not stop being
    // live. Same ring, opposite policy, and the difference is worth
    // naming because copying one into the other would be an easy and
    // invisible mistake.
    int left = frames;
    const float* src = samples;
    while (left > 0) {
        const size_t room = cap - w;
        const size_t n = std::min(room, static_cast<size_t>(left));
        std::memcpy(m_ring.data() + w, src, n * sizeof(float));
        w = (w + n) % cap;
        src += n;
        left -= static_cast<int>(n);
    }

    m_write.store(w, std::memory_order_release);
    m_seen.fetch_add(static_cast<unsigned long long>(frames),
                     std::memory_order_acq_rel);
}

int MicSpectrum::snapshot(float* out, int count) const
{
    if (!out || count <= 0 || m_ring.empty()) { return 0; }

    const size_t cap = m_ring.size();
    const unsigned long long seen = m_seen.load(std::memory_order_acquire);
    const size_t have = static_cast<size_t>(
        std::min<unsigned long long>(seen, cap));
    const size_t n = std::min(have, static_cast<size_t>(count));
    if (n == 0) { return 0; }

    // Read backwards from the write cursor so `out` is oldest-first,
    // which is what an FFT wants and what a caller assumes.
    const size_t w = m_write.load(std::memory_order_acquire);
    const size_t start = (w + cap - n) % cap;
    const size_t first = std::min(n, cap - start);
    std::memcpy(out, m_ring.data() + start, first * sizeof(float));
    if (n > first) {
        std::memcpy(out + first, m_ring.data(), (n - first) * sizeof(float));
    }
    return static_cast<int>(n);
}

} // namespace Longpath
