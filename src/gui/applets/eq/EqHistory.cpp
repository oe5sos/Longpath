// =================================================================
// src/gui/applets/eq/EqHistory.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See EqHistory.h for why the snapshots are whole
// states rather than diffs, and why the hook fires at the wrong end.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-11 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/applets/eq/EqHistory.h"

namespace NereusSDR {

EqHistory::Snapshot EqHistory::capture(const ClientEq& eq)
{
    Snapshot s;
    s.valid           = true;
    s.activeBandCount = eq.activeBandCount();
    s.masterGain      = eq.masterGain();
    s.family          = eq.filterFamily();
    s.bands.reserve(s.activeBandCount);
    for (int i = 0; i < s.activeBandCount; ++i) {
        s.bands.append(eq.band(i));
    }
    return s;
}

void EqHistory::apply(const Snapshot& s, ClientEq& eq)
{
    if (!s.valid) { return; }
    // Bands first, then the count. The other order would briefly expose
    // slots that still hold the state being undone, and the analyser
    // repaints off the audio thread.
    for (int i = 0; i < s.bands.size(); ++i) {
        eq.setBand(i, s.bands.at(i));
    }
    eq.setActiveBandCount(s.activeBandCount);
    eq.setMasterGain(s.masterGain);
    eq.setFilterFamily(s.family);
}

bool EqHistory::same(const Snapshot& a, const Snapshot& b)
{
    if (!a.valid || !b.valid)                     { return false; }
    if (a.activeBandCount != b.activeBandCount)   { return false; }
    if (a.masterGain      != b.masterGain)        { return false; }
    if (a.family          != b.family)            { return false; }
    if (a.bands.size()    != b.bands.size())      { return false; }
    for (int i = 0; i < a.bands.size(); ++i) {
        const auto& x = a.bands.at(i);
        const auto& y = b.bands.at(i);
        if (x.freqHz        != y.freqHz)        { return false; }
        if (x.gainDb        != y.gainDb)        { return false; }
        if (x.q             != y.q)             { return false; }
        if (x.type          != y.type)          { return false; }
        if (x.enabled       != y.enabled)       { return false; }
        if (x.slopeDbPerOct != y.slopeDbPerOct) { return false; }
    }
    return true;
}

void EqHistory::reset(const ClientEq& eq)
{
    clear();
    m_current = capture(eq);
}

void EqHistory::clear()
{
    m_undo.clear();
    m_redo.clear();
    m_current = Snapshot{};
}

void EqHistory::noteCommit(const ClientEq& eq)
{
    const Snapshot now = capture(eq);

    // First commit after binding, before reset() had a baseline. Take
    // this one as the baseline rather than recording a step out of
    // nothing — undoing into a state that was never on screen is worse
    // than having one fewer step.
    if (!m_current.valid) { m_current = now; return; }

    // Nothing actually moved. Selecting a band and re-typing the value
    // that was already there both land here.
    if (same(m_current, now)) { return; }

    m_undo.append(m_current);
    if (m_undo.size() > kMaxDepth) { m_undo.removeFirst(); }

    // A new edit after undoing abandons the redo branch. Standard, and
    // the alternative — keeping it — means the redo button restores
    // something the operator has since edited past.
    m_redo.clear();

    m_current = now;
}

bool EqHistory::undo(ClientEq& eq)
{
    if (m_undo.isEmpty()) { return false; }
    m_redo.append(m_current);
    if (m_redo.size() > kMaxDepth) { m_redo.removeFirst(); }
    m_current = m_undo.takeLast();
    apply(m_current, eq);
    return true;
}

bool EqHistory::redo(ClientEq& eq)
{
    if (m_redo.isEmpty()) { return false; }
    m_undo.append(m_current);
    if (m_undo.size() > kMaxDepth) { m_undo.removeFirst(); }
    m_current = m_redo.takeLast();
    apply(m_current, eq);
    return true;
}

} // namespace NereusSDR
