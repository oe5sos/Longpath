// =================================================================
// src/gui/applets/eq/EqHost.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See EqHost.h for why an adapter rather than five
// edited ports.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/applets/eq/EqHost.h"

#include "core/strip/MicSpectrum.h"
#include "core/strip/StripChain.h"
#include "core/strip/StripSettings.h"

namespace NereusSDR {

ClientEq* EqHost::clientEqTx()
{
    return m_chain ? &m_chain->eq() : nullptr;
}

int EqHost::copyRecentClientEqTxSamples(float* dst, int count)
{
    if (!m_chain || !dst || count <= 0) { return 0; }

    // The strip's own microphone ring, which is fed unconditionally by
    // the worker thread whether or not the chain is enabled — so the
    // analyser draws a spectrum even with the strip switched off, which
    // is what an operator comparing the two wants.
    //
    // Upstream's engine hands back post-EQ audio. This is PRE-EQ, and
    // that difference is deliberate rather than an oversight: the curve
    // is drawn over the spectrum so the operator can aim it, and aiming
    // a filter at a spectrum the filter has already changed is chasing
    // your own tail. Aether's own curve sits over its post-EQ analyser
    // and has the same quirk; NereusSDR does not have to inherit it.
    return m_chain->micSpectrum().snapshot(dst, count);
}

void EqHost::saveClientEqSettings()
{
    if (m_chain) { StripSettings::save(*m_chain); }
}

} // namespace NereusSDR
