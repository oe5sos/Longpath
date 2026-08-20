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

#include "core/strip/StripChain.h"

#include <QWidget>

#include <QElapsedTimer>
#include <QTimer>

#include <array>
#include <vector>

namespace Longpath {

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

// The equaliser's own curve widget used to live here. It went with the
// EQ tab when AetherSDR's equaliser replaced it — see StripWindow's
// buildEqPanel and docs/attribution/AETHERSDR-PORTS.md. Its replacement
// is gui/applets/eq/ClientEqCurveWidget.

// ── One picture per stage, each answering one question ───────────────
//
// Modelled on how Zeus lays out a channel strip, and on the reason it
// works: every stage gets a small picture that answers exactly one
// question about that stage, with a live dot showing where the signal
// is on it right now. A static curve is a manual page. The dot is what
// makes it an instrument.
//
// Three widgets cover seven stages, because the stages only ask three
// questions between them. Seven bespoke pictures would be six more
// things to keep in step with the DSP.
//
// Everything is read from the stage's own getters, and the tube curve
// comes from ClientTube::shapeAt — the function the audio thread runs.
// A picture computed from a second copy of the maths is a picture that
// will eventually disagree with the sound, and the operator will
// believe the picture.

// ── "What does it do to my level?" ───────────────────────────────────
//
// Input on one axis, output on the other, for the three stages that are
// transfer functions: the gate expanding downward, the compressor
// bending at its knee, the limiter flattening at its ceiling.
class StripDynamicsCurve : public QWidget {
    Q_OBJECT
public:
    enum class Stage { Gate, Compressor, Limiter };

    explicit StripDynamicsCurve(Stage s, QWidget* parent = nullptr);

    void setChain(StripChain* chain);
    // Called from the meter timer. Cheap: reads a handful of atomics.
    void refresh();

    // ── What am I looking at, right now ──────────────────────────────
    //
    // A sentence about the current state, in HTML, for the panel beside
    // the picture. Not a help text — a help text is read once and never
    // again, because it says the same thing every time. This changes
    // with the signal, so it is worth glancing at while turning a knob,
    // which is exactly when the operator needs it.
    //
    // It lives on the widget rather than in the window because the
    // widget is the thing that already knows what it drew. A second
    // copy of that knowledge in the panel would be a second thing to
    // keep in step.
    QString explain() const;

    // The colours, named. Three lines, drawn from the same constants
    // the picture uses, so a legend cannot describe a colour the
    // picture stopped using.
    QString legend() const;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;
    // Quiet while you look at it, precise when you point at it. The
    // permanent axis labels were the alternative and they cost about a
    // fifth of the curve area on a card this size; a crosshair costs
    // nothing until it is wanted. Same behaviour as the EQ curve, so
    // there is one gesture to learn rather than two.
    void mouseMoveEvent(QMouseEvent* ev) override;
    void leaveEvent(QEvent* ev) override;

private:
    QPoint m_cursor{-1, -1};
    bool   m_haveCursor{false};

    // A gate can attenuate far below anything a compressor does, so it
    // gets a taller scale. AetherSDR does the same and for the same
    // reason: on a -60 dB axis a gate's floor is off the bottom of the
    // picture and the curve looks like it just stops.
    double minDb() const { return m_stage == Stage::Gate ? -80.0 : -60.0; }
    static constexpr double kMaxDb = 0.0;

    // The curve itself, as a pure function of the stage's parameters.
    // Static so it can be tested without a widget, a chain or a screen.
    double outputDb(double inDb) const;

    double xFor(double db, const QRect& r) const;
    double yFor(double db, const QRect& r) const;

    Stage       m_stage;
    StripChain* m_chain{nullptr};

    // ── How the ball moves, taken from AetherSDR ─────────────────────
    //
    // Reported from the bench: "the dots are gone so fast you cannot see
    // them", with AetherSDR named as the thing that gets this right.
    // Reading AetherSDR's ClientCompCurveWidget showed that my first
    // attempt used the wrong mechanism entirely.
    //
    // I reached for peak-hold-with-decay, copying the level bars in this
    // same file. That jumps instantly to a new peak and ratchets down,
    // which suits a bar — a bar is asking "how loud was the loudest
    // thing" — and is wrong for a ball on a curve, which is asking
    // "where is the signal sitting". It still snaps on every syllable.
    //
    // AetherSDR uses a one-pole smoother on the input level, run from
    // the widget's own 30 Hz timer, and its comment says exactly why:
    // it "keeps the ball from twitching on silent frames where the peak
    // meter reads -120 dBFS". The ball glides in BOTH directions, so
    // there is never a jump to follow. Alpha 0.30 per tick, about eight
    // ticks to settle.
    //
    // Both the rate and the mechanism are AetherSDR's, and this is the
    // NereusSDR-side note recording where they came from.
    static constexpr double kBallSmoothAlpha = 0.30;
    static constexpr int    kBallTimerMs     = 33;   // ~30 Hz

    double m_liveIn{-120.0};    // smoothed input, what the ball follows
    double m_liveOut{-120.0};
    QTimer* m_ballTimer{nullptr};
};

// ── "What is it doing to the waveform?" ──────────────────────────────
//
// The waveshaper transfer curve, drawn from ClientTube::shapeAt with
// the stage's own drive and bias. The dashed diagonal is unity, so the
// distance between the two IS the distortion.
class StripShaperCurve : public QWidget {
    Q_OBJECT
public:
    explicit StripShaperCurve(QWidget* parent = nullptr);

    void setChain(StripChain* chain);
    void refresh();

    QString explain() const;
    QString legend() const;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    StripChain* m_chain{nullptr};
    double  m_livePeak{-120.0};   // one-pole smoothed, as the ball is
    QTimer* m_ballTimer{nullptr};
};

// ── "Which part of the voice is it listening to?" ────────────────────
//
// A band emphasis shape with its centre marked, for the stages that act
// on one region rather than on the whole signal: the de-esser's
// sidechain, and the exciter's two tunings.
class StripBandCurve : public QWidget {
    Q_OBJECT
public:
    enum class Stage { DeEsser, Exciter };

    explicit StripBandCurve(Stage s, QWidget* parent = nullptr);

    void setChain(StripChain* chain);
    void refresh();

    QString explain() const;
    QString legend() const;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    double xForHz(double hz, const QRect& r) const;

    Stage       m_stage;
    StripChain* m_chain{nullptr};
    double m_liveGr{0.0};
};

} // namespace Longpath
