// =================================================================
// src/core/audio/AudioTapRing.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Begruendung steht im Header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "core/audio/AudioTapRing.h"

#include <algorithm>

namespace NereusSDR {

AudioTapRing::AudioTapRing(int capacityFloats)
{
    resize(capacityFloats);
}

void AudioTapRing::resize(int capacityFloats)
{
    // Ein Platz mehr als nutzbar: voll und leer waeren sonst derselbe
    // Zustand (beide Zaehler gleich), und der Leser koennte einen vollen
    // Speicher fuer einen leeren halten.
    m_buf.assign(capacityFloats > 0
                     ? static_cast<size_t>(capacityFloats) + 1u : 0u, 0.0f);
    reset();
}

void AudioTapRing::reset() noexcept
{
    m_writePos.store(0, std::memory_order_relaxed);
    m_readPos.store(0, std::memory_order_relaxed);
    m_dropped.store(0, std::memory_order_relaxed);
}

int AudioTapRing::write(const float* data, int count) noexcept
{
    const int took = tryWrite(data, count);
    if (took < count) {
        m_dropped.fetch_add(static_cast<long long>(count - took),
                            std::memory_order_relaxed);
    }
    return took;
}

int AudioTapRing::tryWrite(const float* data, int count) noexcept
{
    if (m_buf.empty() || data == nullptr || count <= 0) { return 0; }

    const size_t cap = m_buf.size();
    const size_t w   = m_writePos.load(std::memory_order_relaxed);
    // acquire: was der Leser freigegeben hat, muss hier sichtbar sein,
    // bevor daraufgeschrieben wird.
    const size_t r   = m_readPos.load(std::memory_order_acquire);

    const size_t free = (r + cap - w - 1u) % cap;
    const size_t take = std::min(free, static_cast<size_t>(count));

    for (size_t i = 0; i < take; ++i) {
        m_buf[(w + i) % cap] = data[i];
    }
    // release: erst die Werte, dann der Zaehler. Andersherum saehe der
    // Leser einen Stand, an dem noch nichts steht.
    m_writePos.store((w + take) % cap, std::memory_order_release);

    return static_cast<int>(take);
}

int AudioTapRing::read(float* out, int maxCount) noexcept
{
    if (m_buf.empty() || out == nullptr || maxCount <= 0) { return 0; }

    const size_t cap = m_buf.size();
    const size_t r   = m_readPos.load(std::memory_order_relaxed);
    const size_t w   = m_writePos.load(std::memory_order_acquire);

    const size_t have = (w + cap - r) % cap;
    const size_t take = std::min(have, static_cast<size_t>(maxCount));

    for (size_t i = 0; i < take; ++i) {
        out[i] = m_buf[(r + i) % cap];
    }
    m_readPos.store((r + take) % cap, std::memory_order_release);
    return static_cast<int>(take);
}

int AudioTapRing::available() const noexcept
{
    if (m_buf.empty()) { return 0; }
    const size_t cap = m_buf.size();
    const size_t r = m_readPos.load(std::memory_order_acquire);
    const size_t w = m_writePos.load(std::memory_order_acquire);
    return static_cast<int>((w + cap - r) % cap);
}

} // namespace NereusSDR
