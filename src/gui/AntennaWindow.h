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

class QCloseEvent;
class QComboBox;
class QDragEnterEvent;
class QDropEvent;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace Longpath {

class SwrCurveWidget;
class SwrSweepPanel;

class AntennaWindow : public QDialog {
    Q_OBJECT
public:
    explicit AntennaWindow(QWidget* parent = nullptr);

    // Load a sweep from disk. Public so a future analyser driver, or a
    // drop onto the main window, can feed the same path.
    void openFile(const QString& path);
    void setSweep(const Sweep& s);

    /// The "Sweep (Radio)" tab (2026-08-13 radio-as-analyzer feature).
    /// MainWindow injects the SwrSweepController backend through it;
    /// without injection the tab stays inert and the file half of the
    /// window keeps working with no radio at all.
    SwrSweepPanel* sweepPanel() const { return m_sweepPanel; }

protected:
    // Drop a .s1p straight onto the window. A sweep usually arrives as
    // a file just copied off an SD card, and this is the shortest path
    // from there.
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

    // 2026-08-27: like every plain QDialog window in this app (see
    // LogbookWindow's own note), this one never saved or restored its
    // own position/size -- it always reopened at the fixed 1000x760
    // resize() below. Same idiom as LogbookWindow/AmpViewWindow:
    // capture on close, apply on construction.
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void saveGeometryState();
    void restoreGeometryState();
    // Recompute everything shown from the sweep and the three inputs.
    // One function, called from every control, so no two readouts can
    // be describing different states.
    void refresh();

    void chooseFile();
    /// Load the bundled 40 m dipole sweep. Searches beside the app
    /// bundle and up the source tree, because where the samples end up
    /// depends on how the build was run — and a demo button that
    /// cannot find its demo is worse than none.
    void loadSample();
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

    // ── The third state: measured, real, and no longer about this band ─
    //
    // The window used to know two things — a sweep, or no sweep — and
    // could therefore not express the one that is true for the whole
    // length of a run: these numbers were measured, they are correct,
    // and they describe a band nobody is looking at any more.
    //
    // So the analysis carries the stretch of spectrum it is about, and
    // a flag for whether that is still the stretch under the needle.
    // Set when a sweep starts on a different band; cleared ONLY by a
    // real new measurement arriving through setSweep().
    //
    // That asymmetry is what makes a failed run permanent without a
    // rule saying so: a sweep that measures nothing never calls
    // setSweep(), so the mark it set on starting simply stays. And a
    // failed run on the SAME band never set one, which is the operator's
    // decision of 2026-08-15 — a good measurement of 20 m is not made
    // wrong by a later 20 m attempt that the coupler slept through.
    void onSweepStartedFor(const QString& bandName, double loHz,
                           double hiHz);

    /// True when two frequency ranges are the same band's worth of
    /// spectrum: they overlap by more than half the narrower one.
    ///
    /// Public and static because it is the entire decision, and a
    /// decision that can be checked with two numbers should not need a
    /// window, a radio and a rendered picture to check.
public:
    static bool sameBandSpan(double aLoHz, double aHiHz,
                             double bLoHz, double bHiHz);

private:

    // As it came out of the file, and as it is after the feedline has
    // been taken back out. Both kept: the operator can compare, and
    // reloading is not needed when the cable length changes.
    Sweep m_measured;
    Sweep m_sweep;        // what everything downstream reads
    // The sweep before this one, drawn faint behind the live curve.
    Sweep m_previous;
    QString m_previousLabel;

    // ── The continuous curve the radio may not draw ──────────────────
    //
    // 2026-08-15. "es muss eine durchgehende linie sein, es muss der
    // ganze bereich gemessen werden."
    //
    // The radio cannot measure the whole range. Not a limitation of
    // this program: between 2.000 and 3.500 MHz, and in seven more
    // stretches up to 30 MHz, an amateur licence does not permit
    // transmitting, and the sweep keys the transmitter. The holes in
    // the curve ARE the band plan. BandPlanGuard is what stops it, and
    // it stopped two real out-of-band sweeps this week — 80 m at
    // 3.500–4.000 and 60 m at 5.100–5.500.
    //
    // The instrument that CAN sweep 1.8 to 30 MHz without a gap is a
    // vector analyser: microwatts, a measurement instrument, and the
    // ordinary way antennas are swept. This program already reads its
    // file.
    //
    // So the continuous line comes from the VNA and stays pinned
    // behind every radio sweep that follows, instead of being replaced
    // by the next one the way m_previous is. One picture: the whole
    // range as measured by the analyser, and the radio's own numbers
    // sitting on top of it in the bands where the radio is allowed to
    // look — which is also the comparison he asked for on the bench.
    Sweep m_pinned;
    QString m_pinnedLabel;
    void choosePinned();
    void applyReference();

    // Two or more measurements either side of a known change let this
    // work out how much the antenna really moves per centimetre — which
    // in the worked case was half what the textbook says. See
    // TrimSession.h.
    TrimSession m_session;

    QPushButton*    m_openBtn{nullptr};
    QPushButton*    m_demoBtn{nullptr};
    QPushButton*    m_pinBtn{nullptr};
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
    /// The four headline tiles, as one widget. A sweep across several
    /// bands gets the table below instead — four numbers describing
    /// whichever band happened to overlap widest are not a summary of
    /// nine bands, they are a wrong answer with a confident face.
    QWidget* m_tileRow{nullptr};

    QTableWidget* m_bandTable{nullptr};
    QPushButton* m_forgetBtn{nullptr};

    // Set once the operator has typed a target, so a newly loaded sweep
    // stops overwriting it with the band centre. Choosing 7.030 and
    // having it silently become 7.100 on the next measurement is the
    // kind of helpfulness that costs an afternoon.
    bool m_targetChosen{false};

    // What the numbers on screen are about, and whether that is still
    // the band being measured. See onSweepStartedFor().
    double  m_analysisLoHz{0.0};
    double  m_analysisHiHz{0.0};
    QString m_analysisBandName;
    bool    m_analysisStale{false};

    // The radio-sweep tab (2026-08-13). Owned by the tab widget.
    SwrSweepPanel* m_sweepPanel{nullptr};
};

} // namespace Longpath
