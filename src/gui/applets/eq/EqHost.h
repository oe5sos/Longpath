#pragma once

// =================================================================
// src/gui/applets/eq/EqHost.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. The seam between AetherSDR's ported equaliser
// widgets and NereusSDR's channel strip.
//
// The ported widgets talk to an `AudioEngine`, and they use exactly
// five things from it:
//
//     clientEqTx()                    the ClientEq to edit
//     clientEqRx()                    the receive-side one
//     copyRecentClientEqTxSamples()   audio for the FFT behind the curve
//     copyRecentClientEqRxSamples()
//     saveClientEqSettings()          persistence
//
// NereusSDR has no AudioEngine of that shape, and its channel strip is
// transmit-only. Rather than editing five ported files to reach into
// StripChain — which would turn a verbatim port into a modified one and
// make the next upstream comparison useless — this presents the same
// five names over NereusSDR's own parts.
//
// An adapter is the honest way to port a widget: the borrowed code
// stays byte-comparable against upstream, and every adaptation lives in
// one file that is ours and can be read on its own.
//
// ── Receive side ─────────────────────────────────────────────────────
//
// There isn't one. NereusSDR's channel strip processes the microphone
// on the way out; the receive audio goes through WDSP and has its own
// equaliser applet. clientEqRx() therefore returns null, and the panel
// is only ever shown on the transmit path. Returning null rather than
// silently handing back the transmit equaliser is deliberate: an
// operator who somehow reached the RX view should get an empty panel,
// not a working one that edits the wrong thing.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

// ── Undo lives here, and only here ───────────────────────────────────
//
// saveClientEqSettings() is the one call every equaliser mutation ends
// with — canvas drag, icon click, typed value, output fader, context
// menu. That makes this class the only place that sees all of them, so
// it is where the history goes. Putting it in the panel would mean the
// panel intercepting five widgets it did not write.
//
// The suppression flag matters: undo() writes to the ClientEq and then
// persists, and persisting goes through the same door that records
// history. Without the flag, undoing would record the undo as an edit
// and the stack would never empty.

#include "gui/applets/eq/EqHistory.h"

#include <QPointer>

namespace NereusSDR {

class ClientEq;
class StripChain;

class EqHost {
public:
    // Takes the equaliser's current state as the undo baseline right
    // away. Without it the operator's FIRST edit of the session becomes
    // the baseline instead of a step, and the one edit people most want
    // back is the first one they were not sure about.
    explicit EqHost(StripChain* chain) : m_chain(chain) { resetEqHistory(); }

    void setChain(StripChain* chain);

    // The equaliser the widgets edit. Null when no radio is connected,
    // which every ported widget already handles — they were written
    // against an engine that can be between devices.
    ClientEq* clientEqTx();
    ClientEq* clientEqRx() { return nullptr; }   // see the header note

    // Recent microphone audio for the analyser behind the curve, oldest
    // first. Returns how many samples were written, which may be fewer
    // than asked for until the ring has filled.
    int copyRecentClientEqTxSamples(float* dst, int count);
    int copyRecentClientEqRxSamples(float*, int) { return 0; }

    // Called by every ported widget after it changes something. Saves
    // to disk, as upstream expects, and records an undo step.
    void saveClientEqSettings();

    // ── Stepping back ────────────────────────────────────────────────
    //
    // Both persist and both are safe to call when there is nothing to
    // do; they return false and change nothing, so a caller may leave
    // its button state to canUndo()/canRedo() without a race.
    bool undoEqEdit();
    bool redoEqEdit();
    bool canUndoEqEdit() const { return m_history.canUndo(); }
    bool canRedoEqEdit() const { return m_history.canRedo(); }
    int  undoDepth()     const { return m_history.undoDepth(); }

    // Forget everything and take the current state as the new baseline.
    // Called when the chain changes underneath — undoing across a
    // reconnection would restore a curve from a different session.
    void resetEqHistory();

private:
    StripChain* m_chain{nullptr};
    EqHistory   m_history;
    // True while undo/redo is writing. See the note above the class.
    bool        m_replaying{false};
};

} // namespace NereusSDR
