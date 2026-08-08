#pragma once

// =================================================================
// src/gui/applets/StripWindow.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original, over DSP ported from AetherSDR
// (https://github.com/aethersdr/AetherSDR, GPLv3, primary author
// Jeremy [KK7GWY]). AetherSDR's own window is AetherialAudioStrip
// plus nine Strip*Panel classes wired through its AudioEngine;
// NereusSDR's transmit audio does not pass through an AudioEngine, so
// the window is written here against StripChain directly. The control
// sets and their ranges come from the ported stage headers.
//
// The Aetherial Audio Channel Strip, as a window.
//
// Layout follows the same rule as the voice check: the thing you need
// most often is largest and first, and the numbers are available but
// not in the way.
//
//   A master switch. One click restores exactly the audio the
//   operator had before the strip existed — see StripChain's bypass,
//   which returns before touching a sample.
//
//   A chain row. All eight stages at a glance, lit or dark, so the
//   state of the whole chain is visible while one stage is being
//   edited. Read-only: it reports, it does not accept clicks, because
//   a row that both shows state and changes it is a row people change
//   by accident.
//
//   One tab per stage, each with its own enable and its own controls.
//
// Stages whose panel is not built yet say so plainly rather than
// showing controls that do nothing. The DSP for all eight is ported
// and tested; the panels arrive one at a time, and an empty tab is
// more honest than a decorative one.
//
// Nothing here can transmit.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/StripChain.h"
#include "gui/applets/StripGraphics.h"

#include <QDialog>
#include <QPointer>

#include <array>
#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QTabWidget;
class QTimer;
class QWidget;

namespace NereusSDR {

class RadioModel;

class StripWindow : public QDialog {
    Q_OBJECT
public:
    explicit StripWindow(RadioModel* radio, QWidget* parent = nullptr);

private:
    void buildUi();
    QWidget* buildGatePanel();
    QWidget* buildEqPanel();
    QWidget* buildDeEssPanel();
    QWidget* buildCompPanel();
    QWidget* buildTubePanel();
    QWidget* buildPuduPanel();
    QWidget* buildReverbPanel();
    QWidget* buildLimiterPanel();
    QWidget* buildPlaceholder(StripChain::Stage s);

    // The EQ's bands are laid out once, in a fixed order, so that the
    // panel and the saved settings agree about which slot is which.
    // Band 0 is the high-pass; 1-3 are notches for the mains and its
    // first two harmonics; 4-6 are the tone controls.
    void seedEqLayout();
    void applyHumNotches(int baseHz, bool on);

    // Everything writes through here, so nothing can change the chain
    // without also being remembered.
    void persist();

    // A control's opening position: the chain's value when there is a
    // chain, the literal default otherwise. See the note on the
    // definition — without it, persistence undoes itself on the first
    // nudge of any slider.
    double cur(const std::function<float()>& read, double fallback) const;

    // The chain the window edits, or null when there is no connection.
    // Every control checks: the chain lives with the connection, and
    // this window can outlive one.
    StripChain* chain() const;

    void applyPreset(const QString& name);
    // Push the chain's values back into every control after a preset
    // has changed them underneath. Without it the panel shows the old
    // settings and the first slider touch undoes the preset.
    void reloadControls();

    void refreshChainRow();
    void refreshMeters();

    QPointer<RadioModel> m_radio;

    QCheckBox*  m_master{nullptr};
    QLabel*     m_note{nullptr};
    QTabWidget* m_tabs{nullptr};
    QTimer*     m_meterTimer{nullptr};

    StripChainView* m_chainView{nullptr};
    StripEqCurve*   m_eqCurve{nullptr};
    StripLevelBars* m_levels{nullptr};
    QComboBox*      m_presetBox{nullptr};
    std::array<QCheckBox*, StripChain::kStageCount> m_stageBoxes{};

    QLabel* m_presetNote{nullptr};
    QLabel* m_gateMeter{nullptr};
    QLabel* m_deEssMeter{nullptr};
    QCheckBox* m_humBox{nullptr};
    QComboBox* m_humBase{nullptr};
    QLabel* m_compMeter{nullptr};
};

} // namespace NereusSDR
