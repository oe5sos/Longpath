// =================================================================
// src/gui/applets/eq/ClientEqEditorCanvas.h  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR (https://github.com/aethersdr/AetherSDR),
// GPLv3, primary author Jeremy [KK7GWY]:
//   src/gui/ClientEqEditorCanvas.h at 31b29583
//
// AetherSDR carries no per-file licence headers, so per
// docs/attribution/HOW-TO-PORT.md rule 6 the citation is at project
// level: there is no verbatim block to copy. Both projects are GPLv3,
// so the code carries forward under the same licence per GPLv3 §5.
//
// Ported at the bench's request to reproduce AetherSDR's equaliser
// exactly — its display and its behaviour — rather than to approximate
// it. The DSP it drives, core/strip/ClientEq, was already a verbatim
// port of the same upstream, so the pair are back together.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Ported to NereusSDR by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork). Namespace AetherSDR →
//                 NereusSDR; include paths rebased onto
//                 core/strip/ and gui/applets/eq/. Behaviour unchanged.
// =================================================================

#pragma once

#include "gui/applets/eq/ClientEqCurveWidget.h"

namespace NereusSDR {

class EqHost;

// Interactive version of the curve widget used inside the floating editor.
// Adds mouse handling on top of ClientEqCurveWidget's rendering:
//
//   - L-drag on a band handle: freq + gain
//   - Shift + L-drag on a handle: Q (vertical axis maps to Q)
//   - Double-click ON a handle: step the filter type round the ring
//   - Right-click on a handle: context menu (set type directly, slope,
//     toggle enable, reset)
//
// ── Why double-click and not something cleverer ──────────────────────
//
// Asked for at the bench: change what a point DOES without leaving the
// curve. Every other gesture on a handle was taken — press selects and
// starts a drag, shift-drag is Q, right-click is the menu — and
// double-click was a documented no-op, so it is the one that was free.
//
// It steps round a fixed ring rather than picking a type from where the
// handle sits. Position-aware would be cleverer and worse: this is a
// gesture people will repeat, and a repeated gesture has to land
// somewhere they can predict without looking. The right-click menu is
// still there for going straight to a type.
//
// Each mutation writes through to the ClientEq instance and calls
// AudioEngine::saveClientEqSettings() so the change persists across
// restarts.
class ClientEqEditorCanvas : public ClientEqCurveWidget {
    Q_OBJECT

public:
    explicit ClientEqEditorCanvas(QWidget* parent = nullptr);

    // Audio-engine pointer is needed for persistence callbacks after each
    // edit.  The ClientEq pointer itself is set via setEq() on the base.
    void setAudioEngine(EqHost* engine);

    // The ring itself lives in EqFilterRing.h, shared with the icon row
    // — see the note there for why it is not defined in this class.

signals:
    // Emitted live during a cutoff-line drag.  Audio-domain Hz values;
    // the editor / MainWindow translate to TX-filter or RX-slice writes.
    void cutoffsDragged(int audioLowHz, int audioHighHz);

protected:
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void mouseDoubleClickEvent(QMouseEvent* ev) override;
    void contextMenuEvent(QContextMenuEvent* ev) override;

private:
    // Hit-test pixel point against all active handles.  Returns band index
    // or -1 if no handle within kHandleHitRadius.
    int hitTestHandle(const QPointF& pos) const;

    enum class CutoffEdge { None, Low, High };
    // Hit-test against the dashed yellow cutoff guide lines.  Returns
    // which edge (if any) the cursor is within ~5 px of horizontally.
    // Excludes the band-plan strip area at the bottom.
    CutoffEdge hitTestCutoffEdge(const QPointF& pos) const;

    // Save current band state to settings (called after every edit so
    // the user doesn't lose work on crash / quit).
    void persist();

    EqHost* m_audio{nullptr};
    int  m_draggingBand{-1};
    bool m_dragShift{false};
    QPointF m_dragStart;
    float   m_dragStartFreqHz{0};
    float   m_dragStartGainDb{0};
    float   m_dragStartQ{0};

    CutoffEdge m_draggingCutoff{CutoffEdge::None};
};

} // namespace NereusSDR
