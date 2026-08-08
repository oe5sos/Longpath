#pragma once

// =================================================================
// src/gui/applets/StripGraphics.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Draws over DSP ported from AetherSDR
// (https://github.com/aethersdr/AetherSDR, GPLv3, primary author
// Jeremy [KK7GWY]); the curve is computed from ClientEq's own
// analytic magnitude function, so what is drawn is what the filter
// does rather than a separate model of it that can drift from it.
//
// The two pictures the channel strip needs.
//
// Text is a poor instrument. "Gate: shut · -12.4 dB of attenuation" is
// accurate and tells an operator nothing they can act on while
// speaking, because reading it costs more attention than listening
// does. A bar that moves is understood without being read.
//
//   StripChainView   the eight stages as tiles, each with its own
//                    gain-reduction bar, so the whole chain's
//                    behaviour is one glance. Click a tile to open
//                    that stage.
//
//   StripEqCurve     the equaliser's actual response, on a log
//                    frequency axis, with the speech band marked.
//                    This is where the high-pass and the mains
//                    notches stop being numbers and become a shape.
//
// Both are read-only views over StripChain and hold no state of their
// own beyond what they are told. Neither can transmit.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/MicSpectrum.h"
#include "core/strip/StripTargets.h"
#include "core/strip/StripChain.h"

#include <QWidget>

#include <array>
#include <vector>

namespace NereusSDR {

// ── The chain, as tiles ──────────────────────────────────────────────

class StripChainView : public QWidget {
    Q_OBJECT
public:
    explicit StripChainView(QWidget* parent = nullptr);

    // Not owned. Null is legal and draws the row greyed out — the
    // window can be open with no radio connected.
    void setChain(StripChain* chain);

    // Called from the meter timer. Values are the stage's own
    // gain-reduction reading, ≤ 0 dB; stages that do not reduce gain
    // pass 0 and draw no bar.
    void setReduction(StripChain::Stage s, double db);

    QSize sizeHint() const override;

signals:
    void stageClicked(int stageIndex);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* ev) override;

private:
    QRect tileRect(int index) const;

    StripChain* m_chain{nullptr};
    std::array<double, StripChain::kStageCount> m_reduction{};

    // How much reduction fills the bar. 24 dB rather than the stage
    // maxima, because a common scale across all eight is what makes
    // the row readable as one picture — a bar that means -12 on one
    // tile and -40 on the next is decoration.
    static constexpr double kBarFullScaleDb = 24.0;
};

// ── In, out, and the difference ──────────────────────────────────────

class StripLevelBars : public QWidget {
    Q_OBJECT
public:
    explicit StripLevelBars(QWidget* parent = nullptr);

    void setChain(StripChain* chain);
    // Called from the meter timer.
    void tick();

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    static constexpr double kFloorDb = -60.0;

    StripChain* m_chain{nullptr};
    double m_in{-120.0};
    double m_out{-120.0};
    // Peak hold, so a transient that the eye would miss stays visible
    // long enough to be read. Decays rather than latching: a hold that
    // never falls stops being information after the first loud word.
    double m_inHold{-120.0};
    double m_outHold{-120.0};
};

// ── The equaliser's response ─────────────────────────────────────────

class StripEqCurve : public QWidget {
    Q_OBJECT
public:
    explicit StripEqCurve(QWidget* parent = nullptr);

    void setChain(StripChain* chain);

    // The live microphone, drawn behind the curve. Not owned; null
    // simply means no spectrum is shown.
    void setSpectrum(const MicSpectrum* spec);

    // Recompute the spectrum from the tap. Called from the meter timer.
    void tick();

    // Freeze what is on screen. The whole reason for this control: you
    // cannot aim an equaliser at a shape that is moving. Speak a
    // sentence, press Hold, and then shape the curve against the voice
    // that was actually there rather than against whatever the last
    // hundred milliseconds happened to contain.
    void setHeld(bool on);
    bool isHeld() const noexcept { return m_held; }

    // Smooth the held curve to a third of an octave. The raw average of
    // a voice is a comb of harmonics; the peaks and troughs between
    // them are real and are not something an equaliser should be aimed
    // at. Smoothing shows the shape underneath.
    void setSmoothing(bool on);
    bool smoothing() const noexcept { return m_smooth; }

    // Draw the target — where a voice that carries would sit — over the
    // held curve, offset onto it so the two can be compared by eye.
    // Only meaningful when something is held.
    void setShowTarget(bool on);

    // ── The result ───────────────────────────────────────────────────
    //
    // The measured voice with the equaliser applied: green, solid,
    // where you will actually end up.
    //
    // Without it the window shows the measurement and the intent and
    // leaves the operator to add two curves by eye across a log axis.
    // People are bad at that, and they are bad at it in a consistent
    // direction — the sum is read as flatter than it is, so the next
    // adjustment overshoots. Every other control here opens that loop;
    // this is the one that closes it.
    void setShowResult(bool on);
    bool showResult() const noexcept { return m_showResult; }

    // Which target the rose line aims at. Changing it redraws; it does
    // not touch the equaliser.
    void setProfile(const QString& name);
    QString profile() const { return m_profile; }

    // Repaint from the chain's current bands. The curve is computed
    // from ClientEq's own static magnitude function, so it cannot
    // disagree with the filter.
    void refresh();

    // Three sentences naming the biggest differences between the held
    // spectrum and the target, worst first. Empty when nothing is held
    // — advice from a moving picture would change while it was read.
    QStringList tips() const;

    // The measured curve sampled at the target's twelve frequencies,
    // relative to 1 kHz. Empty until there is a fifteen-second average.
    // Used by "my voice → target": the most honest starting point there
    // is, because it is the only one that is actually about this
    // operator and this microphone.
    QVector<double> measuredAtTargetPoints() const;

    QSize sizeHint() const override;

signals:
    // A band was dragged. The window persists and redraws; this widget
    // deliberately does not save anything itself, so there is one place
    // that decides when settings are written.
    void bandChanged(int bandIndex);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void wheelEvent(QWheelEvent* ev) override;
    // Double-click a handle to change its shape — peak, low shelf, high
    // shelf, notch. Double-click empty space to add a band there, and
    // right-click a handle to take it away. Shape, position and count
    // are the three things an equaliser has, and until now only two of
    // them were reachable from the picture.
    void mouseDoubleClickEvent(QMouseEvent* ev) override;
    void contextMenuEvent(QContextMenuEvent* ev) override;
    void leaveEvent(QEvent* ev) override;

private:
    static constexpr double kMinHz  = 20.0;
    static constexpr double kMaxHz  = 16000.0;
    static constexpr double kRangeDb = 18.0;   // ± on the vertical axis

    // 4096 at 48 kHz is 12 Hz per bin — enough to separate a 50 Hz hum
    // from the voice above it, which is the one thing this picture
    // exists to make visible.
    static constexpr int kFft = 4096;

    // Which bands get a handle: the high-pass, and every tone band
    // from kFirstToneBand up. The three mains notches in between
    // deliberately get none — dragging a notch by hand is a worse way
    // to place it than choosing 50 or 60 from a list, and a handle
    // sitting 18 dB down would be off the bottom of the plot anyway.
    static constexpr int kFirstToneBand = 4;
    std::vector<int> handleBands() const;

    double xForHz(double hz, const QRect& r) const;
    double yForDb(double db, const QRect& r) const;
    double hzForX(double x, const QRect& r) const;
    double dbForY(double y, const QRect& r) const;
    QRect  plotRect() const;
    int    handleAt(const QPoint& p) const;
    QPointF handlePos(int band, const QRect& r) const;

    void recomputeSpectrum();
    // Average the last fifteen seconds into one curve, in the power
    // domain rather than in decibels — averaging logarithms weights a
    // quiet window the same as a loud one and flatters the pauses.
    void captureHold();
    void applySmoothing();

    StripChain*        m_chain{nullptr};
    const MicSpectrum* m_spec{nullptr};

    // Magnitude in dB per FFT bin, exponentially averaged. Averaged
    // rather than instantaneous because a single frame of speech is
    // mostly gaps between harmonics, and aiming an equaliser at those
    // gaps is aiming at nothing.
    std::vector<double> m_mag;
    std::vector<double> m_heldMag;       // as captured
    std::vector<double> m_heldShown;     // after smoothing, what is drawn
    // True once a fifteen-second average exists. The average is taken
    // continuously, not only on Hold — an operator should be able to
    // glance at the window and see their own standard curve without
    // having pressed anything.
    bool m_haveHold{false};
    int  m_sinceCapture{0};
    bool m_held{false};
    bool m_smooth{true};
    bool m_showTarget{true};
    bool m_showResult{true};
    bool m_haveMag{false};

    // Dragging the rose line itself, when the profile is the
    // operator's own. -1 when not dragging a target point.
    bool editingTarget() const;
    int  targetPointAt(const QPoint& p) const;
    QPointF targetPointPos(int idx, const QRect& r, double ref) const;
    double  m_targetRef{0.0};    // where the rose sits, from the last paint
    int m_dragTarget{-1};

    int m_dragBand{-1};
    int m_hoverBand{-1};

    // The level the spectrum is drawn relative to, kept from the last
    // time there was speech. Without this the picture vanishes between
    // words: in a pause the reference collapses toward the noise floor,
    // and a curve drawn relative to it either flies off the top or is
    // rejected as unusable. Holding the last good reference is what
    // makes the shape stay still while the level moves.
    double m_lastRef{-1000.0};
    QString m_profile{QStringLiteral("SSB 2.7 kHz")};
};

} // namespace NereusSDR
