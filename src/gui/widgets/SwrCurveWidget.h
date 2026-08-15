#pragma once

// =================================================================
// src/gui/widgets/SwrCurveWidget.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// The SWR across a measured sweep, drawn against the band it is for.
//
// ── What it draws, and why those three lines ─────────────────────────
//
// The curve is SWR. Underneath it the band is shaded, and three
// verticals cross it: the low edge, the centre, and the high edge.
//
// Those three are the question an operator actually has. "Is the
// antenna good" is not answerable; "is it under 2 at the bottom, in the
// middle and at the top of the band I am allowed to use" is, and the
// answer is three numbers printed where the lines meet the curve.
//
// A fourth line, in amber, marks the resonance — where the reactance
// crosses zero. It is usually NOT at the SWR minimum, and having both
// visible at once is the only way that fact ever becomes obvious. See
// AntennaSweep.h.
//
// ── The band edges are a plan, not a permission ──────────────────────
//
// The shading comes from AmateurBands, which is the IARU plan for a
// chosen region. National allocations differ. The widget prints which
// region it drew so nobody reads the green bar as a licence.
//
// ── One band or many ─────────────────────────────────────────────────
//
// A dipole is swept across one band and the three verticals are the
// answer. An end-fed half-wave is swept across the whole of HF at once,
// and its owner's question is different: where are ALL the resonances,
// and do they land in bands?
//
// So a sweep touching more than one band shades and names every one of
// them and marks every series resonance. The three verticals stay, on
// the band nearest the target — the others get a name and their share
// of the shading, and the per-band numbers belong in a table beside the
// picture rather than crowded onto it.
//
// ── Why the vertical scale is not fixed ──────────────────────────────
//
// A fixed 1..10 axis flattens a good antenna into a straight line along
// the bottom, which is exactly when the shape matters most — the whole
// point is to see which way the curve leans. The scale therefore fits
// what is in the band, with a floor so a very flat antenna does not get
// magnified into noise.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/antenna/AmateurBands.h"
#include "core/antenna/AntennaSweep.h"
#include "core/antenna/Touchstone.h"

#include <QPainterPath>
#include <QPolygonF>
#include <QWidget>

class QPainter;
class QVariantAnimation;

namespace NereusSDR {

class SwrCurveWidget : public QWidget {
    Q_OBJECT
public:
    explicit SwrCurveWidget(QWidget* parent = nullptr);

    // Show this sweep. An empty one leaves the frame with its reason in
    // the middle rather than an empty box.
    void setSweep(const Sweep& s);

    // ── The sweep before this one ────────────────────────────────────
    //
    // Drawn faint, behind the live curve. Trimming an antenna is a loop
    // — measure, cut, measure — and the single most useful thing a tool
    // can show is whether the last change did what it was supposed to.
    // Two numbers cannot say that; two curves can.
    void setReference(const Sweep& s, const QString& label = {});
    void clearReference();
    const Sweep& sweep() const { return m_sweep; }

    // ── Numbers belong to a band, and a band can be left ─────────────
    //
    // The measurement stays on screen and stays real. What changes is
    // that it is no longer about the band being measured, and saying so
    // is the whole job: the curve steps back in opacity and carries its
    // own band name in a capsule, so a glance at the head of the window
    // and a glance here cannot be read as one statement any more.
    //
    // Not cleared, deliberately. An operator sweeping 80 m still wants
    // last hour's 20 m curve to compare against, and blanking it would
    // throw away a measurement to fix a labelling problem.
    void setSuperseded(bool on, const QString& bandName = {});
    bool isSuperseded() const { return m_superseded; }
    QString supersededBand() const { return m_supersededBand; }

    /// Where the fade is heading. Tests read this instead of waiting out
    /// the 150 ms the animation takes.
    double targetDim() const { return m_superseded ? kSupersededDim : 1.0; }

    /// How much opacity ink keeps once the analysis is superseded.
    static constexpr double kSupersededDim = 0.45;

    // ── The two colours that never step back ─────────────────────────
    //
    // Asked by theme role, and the single authority for it: paintEvent
    // routes EVERY colour through this, so there is no second place
    // where a colour could quietly be added to the fading set.
    //
    // `danger` is an SWR you should not transmit into. `measured-border`
    // is the limit rule it is judged against. Dimming a warning because
    // everything around it is dimmed abolishes it exactly as thoroughly
    // as dimming it because it is loud — which is the thing
    // docs/design/HAUSSTIL.md §Die Grenze, die kein Design überschreibt
    // forbids, and which BandPlanGuard exists to back up.
    //
    // Static and role-keyed so a test can state the rule without
    // rendering anything and counting pixels.
    static bool fadesWhenSuperseded(const char* role);

    // Which band plan to draw. Region 1 by default — the operator this
    // was written for is in it, and a wrong default here paints
    // permission over spectrum.
    void setRegion(AmateurBands::Region r);

    // Draw against this band whatever the sweep covers. An invalid band
    // means "work it out from the sweep", which is the normal case.
    void setBand(const AmateurBands::Band& b);

    // The line the operator is aiming for, drawn dashed. Zero hides it.
    void setTargetHz(double hz);

    // SWR the antenna is expected to stay under. Drawn as a horizontal
    // rule; the three band readouts are coloured against it.
    void setSwrLimit(double limit);

    // The band actually being drawn, after working it out from the
    // sweep. Invalid when the sweep touches no band.
    AmateurBands::Band shownBand() const { return m_band; }

    // Every band the sweep touches, in frequency order. The window uses
    // it to build the per-band table under a wide sweep.
    QVector<AmateurBands::Band> shownBands() const { return m_bands; }

    // Every series resonance in the sweep. One for a dipole, several
    // for an end-fed.
    QVector<AntennaSweep::Crossing> resonances() const { return m_all; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    // ── Where a sweep stops being continuous ─────────────────────────
    //
    // A range sweep across several bands has holes: the spectrum
    // between the bands is not ours to transmit on, so no point exists
    // there. Joined into one polyline it becomes a straight stroke from
    // 7.200 to 10.100 MHz — full confidence across three megahertz
    // nobody measured, which is the shape of every mistake this window
    // has made.
    //
    // A gap is a step much larger than the run's own spacing. The
    // MEDIAN is the reference, not the mean: with the mean, one hole
    // widens the threshold until the next hole stops counting as one.
    //
    // Returns the step size above which the line must break, or
    // infinity when there are too few points to judge.
    //
    // Public and static because it is the whole decision, and a test
    // that renders the widget and counts pixels tests the painter's
    // grid lines as much as this — which is exactly how the first
    // version of that test failed.
    static double gapThresholdHz(const Sweep& s);

    /// The panels the segmented axis is currently divided into.
    ///
    /// Empty for an ordinary continuous sweep, which keeps the linear
    /// axis. One entry per measured stretch otherwise, in frequency
    /// order: {loHz, hiHz, left edge, right edge}, the edges as
    /// fractions of the plot width.
    ///
    /// Public because it is the layout, and the layout is the thing
    /// that was wrong four times running. It can be checked with
    /// arithmetic — does 40 m get a ninth of the width or a
    /// hundred-and-fortieth — where looking at the picture was the only
    /// check available before, and I cannot look at the picture.
    struct Panel {
        double loHz{0.0};      // drawn edge — the run plus 6 % of air
        double hiHz{0.0};
        double dataLoHz{0.0};  // first and last frequency measured in it
        double dataHiHz{0.0};
        double f0{0.0};
        double f1{0.0};
    };
    QVector<Panel> viewPanels() const;

private:
    /// True when this frequency has a place on the picture: inside the
    /// view on a linear axis, inside a panel on a segmented one.
    /// Anything else would be pinned to a panel edge by xFor, and a
    /// marker drawn there would point at the wrong frequency.
    bool inSomePanel(double hz) const;

public:

protected:
    void paintEvent(QPaintEvent*) override;
    // ── Reading a value off the curve ────────────────────────────────
    //
    // "wenn man mit der Maus auf dem Chart fährt, Frequenz und SWR
    //  ablesen können."
    //
    // Which is what every VNA does and what a picture of a curve is
    // otherwise bad at: the eye can see WHERE the dip is and cannot
    // read WHAT it is to two decimals. A crosshair with the two numbers
    // beside it turns the chart from an impression into an instrument.
    //
    // The SWR is interpolated from the sweep at the cursor's frequency,
    // not read off the nearest sample: between two points at 7 kHz
    // spacing the difference is visible, and a readout that jumps in
    // steps looks broken.
    void mouseMoveEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    /// Nice round frequency step for the ticks — 1, 2 or 5 × a power of
    /// ten, whichever gives roughly the wanted number of labels.
    double tickStepHz(double spanHz, int wanted) const;
    double xToHz(double x) const;

    /// Draw a sweep as one or more polylines, split wherever the step
    /// between neighbouring points jumps far above the run's own
    /// spacing. A range sweep across several bands has such jumps — the
    /// spectrum between the bands is not ours to transmit on, so no
    /// point exists there, and joining across it would draw a confident
    /// line through something nobody measured.
    void drawBrokenCurve(QPainter& p, const Sweep& s) const;

    /// A rounded path through every one of the given points.
    ///
    /// Catmull-Rom as cubic Béziers: the path passes exactly through
    /// each sample, only the stretch between two samples bends. Control
    /// points are clamped into the plot so a spline overshoot cannot
    /// draw an SWR below one.
    QPainterPath smoothPath(const QPolygonF& pts) const;

    // Recompute the band, the vertical scale and the cached features.
    // One place, called whenever any input changes, so the paint can
    // assume they agree.
    void recompute();

    double xFor(double hz) const;
    double yFor(double swr) const;

    Sweep m_sweep;
    Sweep m_reference;
    QString m_referenceLabel;
    AmateurBands::Region m_region{AmateurBands::Region::One};
    AmateurBands::Band   m_band;        // the one with the verticals
    QVector<AmateurBands::Band> m_bands;   // every one the sweep touches
    AmateurBands::Band   m_forcedBand;  // what the caller insisted on

    double m_targetHz{0.0};
    double m_limit{2.0};
    double m_swrTop{3.0};               // top of the vertical scale

    AntennaSweep::Crossing m_resonance;     // nearest the target
    QVector<AntennaSweep::Crossing> m_all;  // every rising crossing
    AntennaSweep::Minimum  m_best;

    // Plot rectangle, in widget coordinates. Recomputed per paint but
    // held here so xFor/yFor need no arguments.
    mutable double m_plotL{0.0}, m_plotR{0.0}, m_plotT{0.0}, m_plotB{0.0};
    // Frequency span drawn, which is the sweep padded a little so the
    // curve does not touch the frame.
    double m_viewLoHz{0.0}, m_viewHiHz{0.0};

    // ── One panel per measured stretch ───────────────────────────────
    //
    // 2026-08-15, 00:02. "der chart kann nicht stimmen, von 14 – 22 MHz
    // komplett gerade?" He was right to distrust it, and the fault was
    // mine twice over.
    //
    // Between 14.35 and 18.07 MHz there is not one measurement — it is
    // the spectrum between 20 m and 17 m. I drew a dashed bridge across
    // it, told myself the dashes said "interpolated", and they did not:
    // they read as curve. So the picture invited exactly the wrong
    // conclusion.
    //
    // And on a 1.8-to-30 MHz axis a band is a sliver. 40 m is 200 kHz,
    // seven thousandths of the width, about ten pixels. Eleven points
    // fit in ten pixels; a shape does not. No smoothing and no extra
    // points can fix that, which is why three attempts at both did not.
    //
    // So the axis stops being linear when the sweep has holes. Every
    // measured stretch gets an equal share of the width and its own
    // labels, with a visible break between panels. Nine bands, nine
    // readable curves, and nothing drawn where nothing was measured.
    //
    // Empty for an ordinary single-band sweep, which keeps the plain
    // linear axis it has always had.
    struct ViewSegment {
        double loHz{0.0};    // panel edge — the measured run plus margin
        double hiHz{0.0};
        // ── What was actually measured, for the labels ───────────────
        //
        // loHz/hiHz carry 6 % of air on each side so the curve does not
        // touch the break lines. Labelling the panel with THOSE prints
        // "1.799" under 160 m: a frequency below the band edge, that
        // nobody measured and nobody may transmit on. The scale has to
        // say where the data starts and stops, not where the drawing
        // does.
        double dataLoHz{0.0};
        double dataHiHz{0.0};
        double f0{0.0};      // left edge, fraction of the plot width
        double f1{0.0};      // right edge
    };
    QVector<ViewSegment> m_viewSegs;

    /// Rebuild m_viewSegs from the current sweep. Leaves it empty when
    /// the sweep is continuous.
    void rebuildViewSegments();

    /// True when a hole-free reference curve covers the whole of the
    /// live sweep — a VNA file, in practice. Then the axis stays
    /// linear: there is a continuous line to draw, so breaking the
    /// picture into panels would be hiding it.
    bool referenceSpansTheSweep() const;

    // Cursor position in widget coordinates while it is over the plot,
    // or a negative x when it is not.
    QPointF m_cursor{-1.0, -1.0};

    // ── Superseded ───────────────────────────────────────────────────
    //
    // m_dim is the live opacity multiplier, animated between 1.0 and
    // kSupersededDim. Animated rather than switched because the house
    // style has one rule about state changes and it is "nothing jumps":
    // a curve that halves in brightness between two frames reads as a
    // redraw, not as a step back.
    bool    m_superseded{false};
    QString m_supersededBand;
    double  m_dim{1.0};
    QVariantAnimation* m_dimAnim{nullptr};
};

} // namespace NereusSDR
