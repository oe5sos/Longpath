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
//   2026-10-09 — Vom Profilgenerator zum Messbericht: die
//                 Herleitung von Ketten ist entfallen (Entscheidung
//                 des Betreibers nach einem Tag Gegenproben), es
//                 bleiben Kanal, Vergleich und das Ablegen der von
//                 Hand eingestellten Kette.
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

    // Die Messung. Ungueltig oder uebersteuert raeumt zurueck auf
    // Striche, statt die Werte der vorigen Aufnahme stehen zu lassen.
    void setAnalysis(const VoiceAnalysis& a);

    int selectedIndex() const { return m_selected; }
    const QVector<BroadcastOptimizer::Plan>& plans() const { return m_plans; }

    void setAuditioning(bool on);

    // Pegelabgleich beim Vergleich. Aus: man hoert, was die
    // Einstellung an Lautheit bringt. An: beide Seiten gleich laut,
    // dann vergleicht man den Klang -- ein A/B ohne Abgleich gewinnt
    // immer das Lautere.
    bool levelMatched() const;

signals:
    // Den rohen Durchgang abspielen, durch den Kanal dieser Breite.
    void beforeRequested(const Longpath::BroadcastOptimizer::Plan& plan);

    // Und denselben durch die EIGENE Kette des Bedieners, ebenfalls
    // durch diesen Kanal. "Nachher" heisst hier nicht "was ein
    // Optimierer daraus macht", sondern "was deine Einstellung in
    // diesem Kanal macht".
    void afterRequested(const Longpath::BroadcastOptimizer::Plan& plan);

    // Die Kette, wie sie gerade eingestellt ist, unter dem Namen
    // dieser Breite ablegen.
    void saveProfileRequested(const Longpath::BroadcastOptimizer::Plan& plan);

    // Zur Kette, um dort weiterzudrehen.
    void openStripRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildUi();
    void select(int index);
    void refresh();

    VoiceAnalysis m_analysis;
    QVector<BroadcastOptimizer::Plan> m_plans;
    int  m_selected{-1};
    bool m_auditioning{false};

    QLabel* m_state{nullptr};
    QVector<QPushButton*> m_widthPills;

    QLabel* m_cellPitch{nullptr};
    QLabel* m_cellLoudness{nullptr};
    QLabel* m_cellRoom{nullptr};
    QLabel* m_cellSibilance{nullptr};
    QLabel* m_cellTop{nullptr};
    QLabel* m_cellChannel{nullptr};

    QLabel* m_headline{nullptr};
    QLabel* m_notes{nullptr};
    QLabel* m_caveat{nullptr};

    QPushButton* m_beforeBtn{nullptr};
    QPushButton* m_afterBtn{nullptr};
    QPushButton* m_saveBtn{nullptr};
    QPushButton* m_stripBtn{nullptr};
    QCheckBox*   m_matchBox{nullptr};
};

} // namespace Longpath
