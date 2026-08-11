#pragma once

// =================================================================
// src/gui/AntennaWindow.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Thetis has no antenna analyser support.
//
// A measured sweep, turned into a length of wire.
//
// ── Layout A: the answer first ───────────────────────────────────────
//
// The instruction sits at the top in large type and the curve is
// underneath it as the evidence. That order was chosen deliberately:
// on a summit the operator has one question — how many centimetres and
// which way — and making them read it off a graph is making them do
// arithmetic in the wind.
//
// The curve is not decoration. It is where you check that the answer is
// believable: that the resonance found is the one you meant, that the
// band is where you think it is, and that the antenna is not doing
// something the single number cannot express.
//
// ── Nothing here transmits ───────────────────────────────────────────
//
// The window reads a file. It does not touch the radio, does not key
// anything, and cannot ask a VNA to do anything either — there is no
// analyser driver behind it yet. When one arrives it will need an
// interlock, because a VNA port is destroyed by a transmitter and the
// two share a coax. That is a problem for the driver, not for this.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/antenna/AmateurBands.h"
#include "core/antenna/AntennaTrim.h"
#include "core/antenna/Feedline.h"
#include "core/antenna/TrimSession.h"
#include "core/antenna/Touchstone.h"

#include <QDialog>

class QComboBox;
class QDragEnterEvent;
class QDropEvent;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace NereusSDR {

class SwrCurveWidget;

class AntennaWindow : public QDialog {
    Q_OBJECT
public:
    explicit AntennaWindow(QWidget* parent = nullptr);

    // Load a sweep from disk. Public so a future analyser driver, or a
    // drop onto the main window, can feed the same path.
    void openFile(const QString& path);
    void setSweep(const Sweep& s);

protected:
    // Drop a .s1p straight onto the window. A sweep usually arrives as
    // a file just copied off an SD card, and this is the shortest path
    // from there.
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    void buildUi();
    // Recompute everything shown from the sweep and the three inputs.
    // One function, called from every control, so no two readouts can
    // be describing different states.
    void refresh();

    void chooseFile();
    // Take the coax back out, if one was entered. Sets m_sweep from
    // m_measured — one place, so no view can be looking at the raw
    // sweep while another looks at the corrected one.
    void applyFeedline();
    // Fill the length box with a half-wave estimate for the target.
    // Labelled as an estimate wherever it lands, because the velocity
    // factor of the operator's actual wire is not known here.
    void estimateLength();

    AntennaTrim::Kind    currentKind() const;
    AmateurBands::Region currentRegion() const;

    // As it came out of the file, and as it is after the feedline has
    // been taken back out. Both kept: the operator can compare, and
    // reloading is not needed when the cable length changes.
    Sweep m_measured;
    Sweep m_sweep;        // what everything downstream reads
    // The sweep before this one, drawn faint behind the live curve.
    Sweep m_previous;
    QString m_previousLabel;

    // Two or more measurements either side of a known change let this
    // work out how much the antenna really moves per centimetre — which
    // in the worked case was half what the textbook says. See
    // TrimSession.h.
    TrimSession m_session;

    QPushButton*    m_openBtn{nullptr};
    QComboBox*      m_kindBox{nullptr};
    QDoubleSpinBox* m_lengthBox{nullptr};
    QPushButton*    m_estimateBtn{nullptr};
    QDoubleSpinBox* m_targetBox{nullptr};
    QComboBox*      m_regionBox{nullptr};
    QComboBox*      m_cableBox{nullptr};
    QDoubleSpinBox* m_cableLenBox{nullptr};
    QDoubleSpinBox* m_limitBox{nullptr};

    QLabel* m_action{nullptr};      // "+ 22 cm", large
    QLabel* m_actionSub{nullptr};   // "pro Schenkel · Linked Dipol 40 m"
    QLabel* m_caution{nullptr};     // amber, hidden when there is none

    QLabel* m_startVal{nullptr};
    QLabel* m_midVal{nullptr};
    QLabel* m_endVal{nullptr};
    QLabel* m_startCap{nullptr};
    QLabel* m_midCap{nullptr};
    QLabel* m_endCap{nullptr};

    SwrCurveWidget* m_curve{nullptr};
    QLabel* m_explain{nullptr};     // the sentence under the curve
    QLabel* m_source{nullptr};
    QLabel* m_learned{nullptr};    // what the last change actually did
    QPushButton* m_forgetBtn{nullptr};

    // Set once the operator has typed a target, so a newly loaded sweep
    // stops overwriting it with the band centre. Choosing 7.030 and
    // having it silently become 7.100 on the next measurement is the
    // kind of helpfulness that costs an afternoon.
    bool m_targetChosen{false};
};

} // namespace NereusSDR
