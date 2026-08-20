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

namespace Longpath {

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

void EqHost::setChain(StripChain* chain)
{
    if (m_chain == chain) { return; }
    m_chain = chain;
    // A different chain is a different session. Undoing across it would
    // restore a curve the operator shaped on another connection.
    resetEqHistory();
}

void EqHost::resetEqHistory()
{
    if (ClientEq* eq = clientEqTx()) { m_history.reset(*eq); }
    else                             { m_history.clear(); }
}

void EqHost::saveClientEqSettings()
{
    if (!m_chain) { return; }
    // Record first, save second. The order does not matter to either,
    // but reading it in this order makes it obvious that the history is
    // built from the same event the persistence is.
    if (!m_replaying) { m_history.noteCommit(m_chain->eq()); }
    StripSettings::save(*m_chain);
}

bool EqHost::undoEqEdit()
{
    ClientEq* eq = clientEqTx();
    if (!eq) { return false; }
    m_replaying = true;
    const bool did = m_history.undo(*eq);
    if (did && m_chain) { StripSettings::save(*m_chain); }
    m_replaying = false;
    return did;
}

bool EqHost::redoEqEdit()
{
    ClientEq* eq = clientEqTx();
    if (!eq) { return false; }
    m_replaying = true;
    const bool did = m_history.redo(*eq);
    if (did && m_chain) { StripSettings::save(*m_chain); }
    m_replaying = false;
    return did;
}

} // namespace Longpath
