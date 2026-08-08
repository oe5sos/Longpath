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

#include "core/strip/StripChain.h"

#include <QWidget>

#include <array>

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

// ── The equaliser's response ─────────────────────────────────────────

class StripEqCurve : public QWidget {
    Q_OBJECT
public:
    explicit StripEqCurve(QWidget* parent = nullptr);

    void setChain(StripChain* chain);

    // Repaint from the chain's current bands. Called after any EQ
    // control moves; the curve is computed from ClientEq's own static
    // magnitude function, so it cannot disagree with the filter.
    void refresh();

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    static constexpr double kMinHz  = 20.0;
    static constexpr double kMaxHz  = 16000.0;
    static constexpr double kRangeDb = 18.0;   // ± on the vertical axis

    double xForHz(double hz, const QRect& r) const;
    double yForDb(double db, const QRect& r) const;

    StripChain* m_chain{nullptr};
};

} // namespace NereusSDR
