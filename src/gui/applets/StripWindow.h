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
    ~StripWindow() override;

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
    // The EQ layout, fixed so the panel, the drag handles and the saved
    // settings all agree about which slot is which. 0 is the high-pass,
    // 1-3 the mains notches, 4-9 the shaping bands.
    static constexpr int kEqBandCount   = 10;
    static constexpr int kBandLowShelf  = 4;
    static constexpr int kBandPresence  = 8;   // 2.4 kHz — where words live
    static constexpr int kBandHighShelf = 9;

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
    void rebuildPresetBox(const QString& select = QString());
    void saveUserPreset();
    void deleteUserPreset();
    // Push the chain's values back into every control after a preset
    // has changed them underneath. Without it the panel shows the old
    // settings and the first slider touch undoes the preset.
    void reloadControls();

    // Notice the radio arriving. The window can be opened before the
    // connection exists, and every control then binds to nothing: the
    // curve draws "Not connected", the combos open at their literal
    // defaults, and it all stays that way for the session. Exactly the
    // fault just fixed in the voice check, and worth writing down as a
    // pattern — a panel that asks "is X there" once, at construction,
    // is a panel that is wrong for the rest of the session in the one
    // case where the operator most needs it.
    void adoptChainIfArrived();
    bool m_hadChain{false};

    // Hearing yourself while shaping the curve. The voice check has the
    // same control, and putting it only there was a mistake: an
    // equaliser you cannot hear while dragging is an equaliser adjusted
    // by looking, which is how a curve that measures well and sounds
    // wrong gets made. Closing the voice check also restores the
    // monitor to whatever it was, which is correct of it and left this
    // window silent.
    void setSelfMonitor(bool on);

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
    QPushButton*    m_presetSave{nullptr};
    QPushButton*    m_presetDelete{nullptr};
    QPushButton*    m_compareBtn{nullptr};
    QPushButton*    m_holdBtn{nullptr};
    QCheckBox*      m_listen{nullptr};
    bool            m_monitorWasOn{false};
    bool            m_restoreMonitor{false};
    QLabel*         m_tips{nullptr};
    // What the master switch was before A/B was pressed, so releasing
    // it puts things back rather than leaving the strip in whichever
    // state the comparison happened to end on.
    bool            m_masterBeforeCompare{false};
    std::array<QCheckBox*, StripChain::kStageCount> m_stageBoxes{};

    QLabel* m_presetNote{nullptr};
    QLabel* m_gateMeter{nullptr};
    QLabel* m_deEssMeter{nullptr};
    QCheckBox* m_humBox{nullptr};
    QComboBox* m_humBase{nullptr};
    QLabel* m_compMeter{nullptr};
};

} // namespace NereusSDR
