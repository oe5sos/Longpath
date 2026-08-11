#pragma once

// =================================================================
// src/gui/applets/eq/EqHistory.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Undo for the equaliser.
//
// ── Why it was missing, and why that is worse than it sounds ─────────
//
// Asked for at the bench in the plainest possible terms: "I want a back
// button, to remove the last setting." The equaliser had none. Every
// gesture on the curve wrote straight through to the DSP and straight
// on to disk, so a mis-aimed drag or a double-click that landed on the
// wrong handle was permanent the instant it happened.
//
// That is worse than an ordinary missing feature, because of what the
// panel is for. An operator shaping a voice is doing exactly one thing:
// trying something, listening, and deciding whether it was better. The
// deciding half needs a way back. Without one, people stop trying
// things — which is not a workflow problem, it is the panel failing at
// its whole purpose while appearing to work.
//
// ── Where the snapshots come from ────────────────────────────────────
//
// Every mutation in the equaliser — a drag on the canvas, a click on an
// icon, a typed value in the parameter row, the output fader, the
// context menu — ends by calling EqHost::saveClientEqSettings(). That
// is not a coincidence; it is how the ported widgets persist. It makes
// exactly one choke point, and one choke point is what an undo stack
// needs.
//
// The hook fires AFTER the change, which is the wrong end. So the stack
// keeps the last state it captured and, at each commit, pushes THAT and
// re-captures. The effect is the same and nothing in the ported widgets
// has to move.
//
// ── What counts as one step ──────────────────────────────────────────
//
// Whatever the widgets call one commit. A drag across the canvas
// persists once, on mouse release, so a drag is one undo step rather
// than two hundred — which is the behaviour anybody would want and is
// inherited rather than engineered.
//
// A commit that changes nothing is not recorded. Selecting a band, or
// re-typing the value that was already there, would otherwise fill the
// stack with steps that appear to do nothing when undone, and an undo
// button that sometimes does nothing is one nobody trusts.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-11 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/ClientEq.h"

#include <QVector>

namespace NereusSDR {

class EqHistory {
public:
    // Everything an equaliser edit can change. Deliberately the whole
    // state and not a description of the change: a diff has to be
    // inverted to be undone, and an inversion that is wrong for one
    // parameter is a bug that only appears after four undos.
    struct Snapshot {
        bool                          valid{false};
        int                           activeBandCount{0};
        float                         masterGain{1.0f};
        ClientEq::FilterFamily        family{ClientEq::FilterFamily::Butterworth};
        QVector<ClientEq::BandParams> bands;
    };

    // How far back it remembers. Sixty-four steps is several minutes of
    // shaping and about 20 kB — small enough not to think about, deep
    // enough that the limit is never the thing that stops you.
    static constexpr int kMaxDepth = 64;

    // Start again from where the equaliser is now. Called when the panel
    // binds to a chain: the state before that belongs to a different
    // radio session and undoing into it would be a surprise.
    void reset(const ClientEq& eq);

    // One commit happened. Records the state as it was before it.
    void noteCommit(const ClientEq& eq);

    bool canUndo() const noexcept { return !m_undo.isEmpty(); }
    bool canRedo() const noexcept { return !m_redo.isEmpty(); }
    int  undoDepth() const noexcept { return int(m_undo.size()); }
    int  redoDepth() const noexcept { return int(m_redo.size()); }

    // Step back / forward. Both write straight into the ClientEq and
    // return false when there was nothing to do, so a caller can leave
    // its button state to canUndo()/canRedo() and still be safe if the
    // two get out of step.
    bool undo(ClientEq& eq);
    bool redo(ClientEq& eq);

    void clear();

    static Snapshot capture(const ClientEq& eq);
    static void     apply(const Snapshot& s, ClientEq& eq);
    // Same to the DSP. Compared with exact equality on the floats and
    // not a tolerance: these are values that were copied, not computed,
    // so anything that differs at all differs because somebody changed
    // it.
    static bool     same(const Snapshot& a, const Snapshot& b);

private:
    Snapshot          m_current;
    QVector<Snapshot> m_undo;
    QVector<Snapshot> m_redo;
};

} // namespace NereusSDR
