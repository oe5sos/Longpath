#pragma once

// =================================================================
// src/gui/applets/BroadcastOptimizerPanel.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// The four profiles, side by side, with what each one costs.
//
// ── Why a separate widget and not another section in the dialog ──────
//
// Because the operator asked for it beside the existing path, not
// instead of it: "build it parallel to our current audio setup and we
// decide later, or we keep using the current one" (2026-10-09). A
// panel that can be hosted, moved or dropped is a decision that can
// still be reversed; three hundred lines woven into
// TxVoiceCheckDialog is not.
//
// So this knows nothing about recording, the radio or the strip. It
// is handed a measurement, it shows four plans, and it asks — by
// signal — for the three things it cannot do itself: play a plan,
// play the raw take to compare it against, and put a plan into the
// chain.
//
// ── The shape, and the house rules it follows ────────────────────────
//
// HAUSSTIL §"Die acht Regeln":
//
//   1. Every control sits in a named group — the versal heading over
//      the width row and over the readout grid.
//   2. Exactly one pill in the group is lit: one width is selected,
//      the other three are nearly invisible.
//   5. State is an outlined capsule at the top right, always in the
//      same place.
//   6. A module explains itself in two lines before any control.
//   7. Unknown is a dash, never a zero. Before a take there are no
//      numbers here, and the grid says so with "——" rather than
//      showing a chain of plausible-looking noughts.
//
// And the "Ablesegitter" from §Instrumente: equal cells, small
// versal heading, monospaced value underneath — plus the row that
// shows what the profile is NOT, so the operator can see at a glance
// that the narrow one has switched the colour stages off.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/VoiceAnalyzer.h"
#include "core/strip/BroadcastOptimizer.h"

#include <QVector>
#include <QWidget>

class QCheckBox;
class QLabel;
class QPushButton;

namespace Longpath {

class BroadcastOptimizerPanel : public QWidget {
    Q_OBJECT
public:
    explicit BroadcastOptimizerPanel(QWidget* parent = nullptr);

    // Hand it a measurement. An invalid or clipped one clears the
    // panel back to dashes rather than leaving the previous take's
    // four profiles on screen next to a new recording — which would
    // be the most quietly misleading thing this window could do.
    void setAnalysis(const VoiceAnalysis& a);

    // Which width is showing, or -1 before there is anything.
    int selectedIndex() const { return m_selected; }
    const QVector<BroadcastOptimizer::Plan>& plans() const { return m_plans; }

    // The audition buttons go back to their un-pressed wording when
    // playback stops. The host owns the audio, so the host says when.
    void setAuditioning(bool on);

    // ── Comparing honestly ───────────────────────────────────────────
    //
    // Whether the audition should be matched to the raw take's
    // loudness before it is played. ON by default, and that default
    // is the whole point.
    //
    // An A/B between processed and unprocessed audio is won by
    // whichever is louder, every time, by anybody — it is the oldest
    // and best-documented trap in audio, and it is exactly the trap a
    // transmit-audio optimiser walks the operator into: the profile
    // adds several dB of make-up gain, so of course it sounds better.
    // Matched, the comparison is about tone and density, which is the
    // only thing worth deciding by ear.
    //
    // It can be switched off, because the gain IS part of what the
    // profile buys and hearing it is sometimes the point — but it has
    // to be switched off deliberately.
    bool levelMatched() const;

signals:
    // Play this plan: the raw take rendered through it, off the
    // real-time path. The host does the rendering — it is the one
    // holding the recording.
    void auditionRequested(const Longpath::BroadcastOptimizer::Plan& plan);

    // Play the same take untouched, to compare against.
    void rawRequested();

    // Put this plan into the live channel strip.
    void loadRequested(const Longpath::BroadcastOptimizer::Plan& plan);

    // Write all four out as named presets.
    void saveAllRequested();

private:
    void buildUi();
    void select(int index);
    void refresh();

    VoiceAnalysis m_analysis;
    QVector<BroadcastOptimizer::Plan> m_plans;
    int  m_selected{-1};
    bool m_auditioning{false};

    QLabel* m_state{nullptr};
    QLabel* m_headline{nullptr};
    QVector<QPushButton*> m_widthPills;

    // The readout grid, in the order they are built.
    QLabel* m_cellDensity{nullptr};
    QLabel* m_cellHpf{nullptr};
    QLabel* m_cellTone{nullptr};
    QLabel* m_cellComp{nullptr};
    QLabel* m_cellChannel{nullptr};
    QLabel* m_cellGain{nullptr};
    QLabel* m_cellForgets{nullptr};
    QLabel* m_cellLoudness{nullptr};

    QLabel* m_notes{nullptr};
    QLabel* m_caveat{nullptr};

    QPushButton* m_optimizeBtn{nullptr};
    QPushButton* m_hearBtn{nullptr};
    QPushButton* m_rawBtn{nullptr};
    QPushButton* m_loadBtn{nullptr};
    QPushButton* m_saveAllBtn{nullptr};
    QCheckBox*   m_matchBox{nullptr};
};

} // namespace Longpath
