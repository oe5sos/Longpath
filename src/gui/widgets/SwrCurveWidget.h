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

#include <QWidget>

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

protected:
    void paintEvent(QPaintEvent*) override;

private:
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
};

} // namespace NereusSDR
