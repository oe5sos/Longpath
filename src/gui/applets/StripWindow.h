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
// The Nereus Audio Channel Strip, as a window.
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
#include "gui/applets/eq/ClientEqApplet.h"

#include <memory>

#include <QDialog>
#include <QPointer>

#include <array>
#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QTableWidget;
class QTabWidget;
class QVBoxLayout;
class QTimer;
class QWidget;

namespace NereusSDR {

class EqHost;
class RadioModel;
class StripEqPanel;
class TxVoiceCheckDialog;

class StripWindow : public QDialog {
    Q_OBJECT
public:
    explicit StripWindow(RadioModel* radio, QWidget* parent = nullptr);
    ~StripWindow() override;

    // Land on the embedded voice-check tab (menu entry + MainWindow
    // route here since 2026-08-11 — the standalone dialog is gone).
    void showVoiceCheckTab();

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

    // ── Not a stage ──────────────────────────────────────────────────
    //
    // Every other tab is one processing stage with its own knobs. This
    // one processes nothing: it measures what the whole chain and the
    // modulator produced, after everything else has had its say. It
    // sits last for that reason, and it is the only tab in the strip
    // whose number is about somebody other than the operator.
    QWidget* buildTxSpectrumPanel();

    // The EQ's bands are laid out once, in a fixed order, so that the
    // panel and the saved settings agree about which slot is which.
    // Band 0 is the high-pass; 1-3 are notches for the mains and its
    // first two harmonics; 4-6 are the tone controls.
    // The EQ layout, fixed so the panel, the drag handles and the saved
    // settings all agree about which slot is which. 0 is the high-pass,
    // 1-3 the mains notches, 4-13 the shaping bands.
    //
    // Ten shaping bands, asked for directly. Six could put a dip where
    // the problem was; ten can put a dip where the problem is and leave
    // its neighbours alone, which is the difference between correcting a
    // resonance and tilting the whole voice to hide it. Log-spaced from
    // 180 Hz to 3.4 kHz — 180, 250, 350, 500, 700, 1000, 1400, 1900,
    // 2400, 3400 — with a shelf at each end and peaks between: shelves
    // are the right shape for "everything below this" and peaks for
    // "this bit here".
    //
    // The four new ones are slots 10-13 rather than being inserted in
    // frequency order, so that 4, 8 and 9 still mean low shelf, presence
    // and high shelf and a settings file written by the previous version
    // still says what it said. Index order and frequency order have to
    // part company somewhere, and they part company the moment anyone
    // drags a handle anyway — so the picture and the table sort by
    // frequency for display and nothing else depends on the order.
    //
    // Slots 10-13 did not exist in the first shipped layout. seedEqLayout
    // therefore APPENDS them to an existing chain at 0 dB rather than
    // re-seeding, so an operator who had already shaped a curve gets four
    // more handles and not a reset.


    // The numbers under the picture, one row per draggable band.
    //
    // A graph is the right tool for "make this bit quieter" and the
    // wrong one for "put it at exactly 2200". Both belong, and they are
    // the same bands seen twice — the table is not a second setting to
    // keep in step, it reads and writes the chain like everything else.


    // Everything writes through here, so nothing can change the chain
    // without also being remembered.
    void persist();

    // ── Loudness matching ────────────────────────────────────────────
    //
    // Recompute the output trim so that switching the equaliser in and
    // out changes the shape and not the level. Called after anything
    // that can move a band: a drag, a table edit, a preset, a restore.
    //
    // On by default, and it should be: louder always sounds better, so
    // an unmatched comparison is not a comparison. It is a switch rather
    // than a rule because an operator who has set their gain staging by
    // hand around a particular curve is entitled to keep it.

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

    // ── The radio's own processing, while listening ──────────────────
    //
    // Asked for directly, and right: with the strip in front of WDSP's
    // equaliser, leveler and CFC, listening to yourself means listening
    // to two chains in series. Neither can then be judged — every
    // change you make to one is partly undone or doubled by the other,
    // and the operator ends up tuning the sum.
    //
    // So "Hear myself" bypasses the radio's own processing for as long
    // as it is on, and puts it back exactly as it was afterwards. That
    // is a change to settings the operator did not make by hand, which
    // is why it is announced on screen and why the restore is not
    // optional.
    void setRadioBypass(bool on);
    bool m_radioBypassed{false};
    bool m_hadTxEq{false};
    bool m_hadLeveler{false};
    bool m_hadCfc{false};

    void refreshChainRow();
    void refreshMeters();

    // ── The equaliser tab is AetherSDR's, whole ──────────────────────
    //
    // The ported panel and the adapter it talks through. The host is
    // owned here rather than by the panel because the panel is destroyed
    // and rebuilt with the tab, and the host must outlive that so the
    // panel is never handed a dangling engine.
    std::unique_ptr<EqHost> m_eqHost;
    StripEqPanel*           m_eqPanel{nullptr};
    class TxSpectrumWidget* m_txSpectrum{nullptr};

    // The embedded voice check. Created ONCE in buildUi and owned by
    // this window, not by the tab bar: reloadControls() deletes every
    // stage page on a preset apply, and a recording must survive that.
    TxVoiceCheckDialog* m_voiceCheck{nullptr};

    QPointer<RadioModel> m_radio;

    QCheckBox*  m_master{nullptr};
    QLabel*     m_note{nullptr};
    QTabWidget* m_tabs{nullptr};
    QTimer*     m_meterTimer{nullptr};

    StripChainView* m_chainView{nullptr};
    StripLevelBars* m_levels{nullptr};
    QComboBox*      m_presetBox{nullptr};
    QPushButton*    m_presetSave{nullptr};
    QPushButton*    m_presetDelete{nullptr};
    QPushButton*    m_compareBtn{nullptr};
    // Measure, then shape. See the note at the button's construction
    // for why a take with a start and a stop replaced the rolling
    // average the bench could not interpret.
    QCheckBox*      m_listen{nullptr};
    bool            m_monitorWasOn{false};
    bool            m_restoreMonitor{false};

    // ── The operator's own targets, A and B ──────────────────────────
    //
    // Adjusting one curve and trying to remember the other is a memory
    // test, and the ear loses it in about two seconds. Two slots and an
    // instant switch turn "is this better?" into a question that can be
    // answered while still talking.
    //
    // Shown only when the target being shaped is the operator's own:
    // there is no A and B of a built-in profile, and a pair of buttons
    // that do nothing for four of the six entries in the picker is a
    // pair of buttons that teaches people to distrust the panel.

    // Match a recording. The one method here where nobody's opinion sits
    // between the operator's ear and the number: load a WAV of a signal
    // that sounded right and its long-term spectrum becomes the target.
    // What the master switch was before A/B was pressed, so releasing
    // it puts things back rather than leaving the strip in whichever
    // state the comparison happened to end on.
    bool            m_masterBeforeCompare{false};
    std::array<QCheckBox*, StripChain::kStageCount> m_stageBoxes{};

    // ── One picture per stage ────────────────────────────────────────
    //
    // Each answers one question about its stage and is driven entirely
    // from that stage's own getters, so a picture cannot disagree with
    // the DSP. Refreshed from the same meter timer as the level bars.
    StripDynamicsCurve* m_gateCurve{nullptr};
    StripDynamicsCurve* m_compCurve{nullptr};
    StripDynamicsCurve* m_limiterCurve{nullptr};
    StripShaperCurve*   m_tubeCurve{nullptr};
    StripBandCurve*     m_deEssCurve{nullptr};
    StripBandCurve*     m_puduCurve{nullptr};
    // Point every picture at the chain. Called when the radio arrives
    // after the window opened — the same late-binding trap the curve
    // and the voice check both fell into once already.
    void refreshStagePictures();

    // ── The 50/50 stage card ─────────────────────────────────────────
    //
    // Picture on the left, words on the right, half each. Asked for at
    // the bench and right for a reason worth writing down: the picture
    // and the sentence explaining it are the same size because they are
    // equally important. A caption under a graph is read once; a panel
    // beside it, the same height, that changes as you talk, is read
    // every time.
    //
    // The picture is kept SQUARE. A transfer curve has the same unit on
    // both axes, and stretching it wide distorts the 45° diagonal that
    // the whole reading rests on — "here nothing happens" stops being a
    // recognisable angle.
    //
    // `explain` and `legend` are pulled from the widget each tick; the
    // widget is the thing that already knows what it drew, so there is
    // no second copy of that knowledge to keep in step.
    QWidget* buildStageCard(QWidget* page, QWidget* picture,
                            StripChain::Stage stage);
    void refreshStageText();

    // Which character a stage is showing. Kept in settings rather than
    // in the chain because it is a fact about the UI, not about the
    // DSP — the chain holds the numbers the character produced, and
    // those are what actually process audio. See the note at the
    // picker's construction for the bug this fixes and the one it
    // does not.
    static QString characterSettingsKey(StripChain::Stage s);
    static QString characterKeyFor(StripChain::Stage s);
    static void    rememberCharacter(StripChain::Stage s, const QString& name);

    struct StageText {
        QWidget* picture{nullptr};
        QLabel*  live{nullptr};
        QLabel*  legend{nullptr};
        QLabel*  charNote{nullptr};
    };
    std::array<StageText, StripChain::kStageCount> m_stageText{};

    QLabel* m_presetNote{nullptr};
    QLabel* m_gateMeter{nullptr};
    QLabel* m_deEssMeter{nullptr};
    QLabel* m_compMeter{nullptr};
};

} // namespace NereusSDR
