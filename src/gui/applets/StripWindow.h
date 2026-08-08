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

#include <QDialog>
#include <QPointer>

#include <array>

class QCheckBox;
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
    QWidget* buildCompPanel();
    QWidget* buildPlaceholder(StripChain::Stage s);

    // The chain the window edits, or null when there is no connection.
    // Every control checks: the chain lives with the connection, and
    // this window can outlive one.
    StripChain* chain() const;

    void refreshChainRow();
    void refreshMeters();

    QPointer<RadioModel> m_radio;

    QCheckBox*  m_master{nullptr};
    QLabel*     m_note{nullptr};
    QTabWidget* m_tabs{nullptr};
    QTimer*     m_meterTimer{nullptr};

    std::array<QLabel*, StripChain::kStageCount> m_chainTiles{};
    std::array<QCheckBox*, StripChain::kStageCount> m_stageBoxes{};

    QLabel* m_gateMeter{nullptr};
    QLabel* m_compMeter{nullptr};
};

} // namespace NereusSDR
