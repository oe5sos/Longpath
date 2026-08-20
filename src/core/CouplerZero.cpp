// =================================================================
// src/core/CouplerZero.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See CouplerZero.h for why the zero is measured
// rather than tabled, and why it is a low percentile of a window
// rather than a mean.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-14 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/CouplerZero.h"

#include <algorithm>

namespace Longpath {

void CouplerZero::observe(quint16 fwdRaw, quint16 revRaw, bool transmitting)
{
    if (transmitting) {
        // Not a zero, and the settle counter restarts so the residue
        // after the carrier drops is not taken for one either.
        m_sinceTx = 0;
        return;
    }
    if (m_sinceTx < kSettleSamples) {
        ++m_sinceTx;
        return;
    }

    m_fwd[static_cast<size_t>(m_head)] = fwdRaw;
    m_rev[static_cast<size_t>(m_head)] = revRaw;
    m_head = (m_head + 1) % kWindow;
    if (m_count < kWindow) { ++m_count; }
}

quint16 CouplerZero::percentileOf(
    const std::array<quint16, kWindow>& ring) const
{
    const int n = std::min(m_count, kWindow);
    // Copying up to 256 shorts to sort them. Called from the UI when a
    // reading is scaled, not per telemetry sample; a partial sort would
    // save microseconds nobody is waiting for.
    std::array<quint16, kWindow> tmp{};
    std::copy(ring.begin(), ring.begin() + n, tmp.begin());
    // nth_element rather than a full sort: only the rank matters.
    const int k = std::clamp((n * kPercentile) / 100, 0, n - 1);
    std::nth_element(tmp.begin(), tmp.begin() + k, tmp.begin() + n);
    return tmp[static_cast<size_t>(k)];
}

quint16 CouplerZero::forwardZero(quint16 fallback) const
{
    return known() ? percentileOf(m_fwd) : fallback;
}

quint16 CouplerZero::reverseZero(quint16 fallback) const
{
    return known() ? percentileOf(m_rev) : fallback;
}

void CouplerZero::reset() noexcept
{
    m_head = 0;
    m_count = 0;
    // Ready rather than mid-settle: a fresh connection has not just
    // stopped transmitting, so there is no residue to wait out.
    m_sinceTx = kSettleSamples;
}

} // namespace Longpath
