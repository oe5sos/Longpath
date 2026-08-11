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
class QTableWidget;

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
    // The cable currently selected, with the custom fields folded in
    // when "Custom" is chosen. One place, so the de-embedding and the
    // label under the curve cannot disagree about which cable it is.
    Feedline::Cable currentCable() const;
    // Rebuild the per-band table. Separate from refresh() only because
    // it is thirty lines of table filling and refresh() is already the
    // longest function here.
    void refreshBandTable();
    // Fill the length box with a half-wave estimate for the target.
    // Labelled as an estimate wherever it lands, because the velocity
    // factor of the operator's actual wire is not known here.
    void estimateLength();

    // The span the readouts describe: whatever was typed into the two
    // range boxes, or the band the sweep is on when they are empty.
    // Invalid when there is neither.
    AmateurBands::Band readoutSpan() const;

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
    // Only shown for the "Custom" catalogue entry, which was otherwise
    // a menu item that did nothing. Real cable varies by make and by
    // age, so somebody who has measured their own should be able to
    // enter it.
    QDoubleSpinBox* m_vfBox{nullptr};
    QDoubleSpinBox* m_lossBox{nullptr};
    // The coax controls are refinement, not the question, so they stay
    // folded away until asked for. Somebody who calibrated at the
    // antenna — which is the right way — never needs them at all.
    QPushButton* m_coaxToggle{nullptr};
    QWidget*     m_coaxGroup{nullptr};
    QDoubleSpinBox* m_limitBox{nullptr};
    // ── The exact span to read off ───────────────────────────────────
    //
    // A band is a reasonable default and not always the question. A CW
    // operator cares about 7.020 to 7.040, not about the whole of 40 m,
    // and an FT8 user cares about 14.074 give or take a couple of
    // kilohertz. Both zero means "use the band".
    QDoubleSpinBox* m_fromBox{nullptr};
    QDoubleSpinBox* m_toBox{nullptr};

    QLabel* m_action{nullptr};      // "+ 22 cm", large
    QLabel* m_actionSub{nullptr};   // "pro Schenkel · Linked Dipol 40 m"
    QLabel* m_caution{nullptr};     // amber, hidden when there is none

    QLabel* m_startVal{nullptr};
    QLabel* m_midVal{nullptr};
    QLabel* m_endVal{nullptr};
    // How wide the antenna actually is at the chosen SWR limit. The
    // other half of "bandwidth": not the span you asked about, but the
    // span you have.
    QLabel* m_spanVal{nullptr};
    QLabel* m_spanCap{nullptr};
    QLabel* m_startCap{nullptr};
    QLabel* m_midCap{nullptr};
    QLabel* m_endCap{nullptr};

    SwrCurveWidget* m_curve{nullptr};
    QLabel* m_explain{nullptr};     // the sentence under the curve
    QLabel* m_source{nullptr};
    QLabel* m_learned{nullptr};    // what the last change actually did
    // One row per band the sweep touches. Hidden for a single-band
    // sweep, where the three tiles at the top already say it and a
    // one-row table is furniture.
    QTableWidget* m_bandTable{nullptr};
    QPushButton* m_forgetBtn{nullptr};

    // Set once the operator has typed a target, so a newly loaded sweep
    // stops overwriting it with the band centre. Choosing 7.030 and
    // having it silently become 7.100 on the next measurement is the
    // kind of helpfulness that costs an afternoon.
    bool m_targetChosen{false};
};

} // namespace NereusSDR
