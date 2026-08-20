// =================================================================
// src/gui/applets/StripWindow.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See StripWindow.h for the layout rule and for
// what is ported and what is not.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/applets/StripWindow.h"

#include "gui/StyleConstants.h"
#include "core/strip/StripSettings.h"
#include "core/AppSettings.h"
#include "gui/applets/eq/EqHost.h"
#include "gui/applets/eq/StripEqPanel.h"
#include "gui/applets/TxVoiceCheckDialog.h"
#include "core/strip/StripCharacters.h"
#include "core/AudioEngine.h"
#include "gui/widgets/TxSpectrumWidget.h"

#include <QInputDialog>
#include <QFileDialog>
#include <QScrollArea>
#include <QSizePolicy>
#include <QMessageBox>
#include "core/AudioEngine.h"
#include "core/TxChannel.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QHeaderView>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>

namespace Longpath {

namespace {

QString dimStyle()
{
    return QStringLiteral("QLabel { color: %1; font-size: 11px; }")
        .arg(QString::fromLatin1(Style::kTextSecondary));
}

// A slider with its name on the left and its value on the right, in the
// unit the operator thinks in.
//
// Sliders carry integers, so every control here has a scale factor.
// That factor is the one thing in this file that is easy to get wrong
// and impossible to see: a release time off by ten still moves, still
// looks plausible, and just behaves oddly. So the conversion lives in
// one place, both directions, per row.
struct Knob {
    QSlider* slider{nullptr};
    QLabel*  value{nullptr};
};

Knob addKnob(QFormLayout* form, const QString& name,
             double minVal, double maxVal, double step,
             double initial, const QString& suffix,
             std::function<void(double)> onChange,
             int decimals = 1,
             std::function<void()> after = {})
{
    auto* row = new QWidget(form->parentWidget());
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);

    Knob k;
    k.slider = new QSlider(Qt::Horizontal, row);
    const int steps = int(std::lround((maxVal - minVal) / step));
    k.slider->setRange(0, steps);
    k.slider->setValue(int(std::lround((initial - minVal) / step)));
    h->addWidget(k.slider, 1);

    k.value = new QLabel(row);
    k.value->setMinimumWidth(72);
    k.value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    k.value->setStyleSheet(dimStyle());
    h->addWidget(k.value);

    auto toValue = [minVal, step](int pos) { return minVal + pos * step; };
    auto show = [k, suffix, decimals, toValue](int pos) {
        k.value->setText(QStringLiteral("%1 %2")
                             .arg(toValue(pos), 0, 'f', decimals)
                             .arg(suffix));
    };
    show(k.slider->value());

    QObject::connect(k.slider, &QSlider::valueChanged, k.slider,
                     [show, toValue, onChange, after](int pos) {
        show(pos);
        if (onChange) { onChange(toValue(pos)); }
        // Saving on every slider step rather than on release: a crash
        // or a power cut mid-adjustment should not cost the operator
        // the twenty minutes before it. AppSettings coalesces the disk
        // writes, so this is a map insert per step.
        if (after) { after(); }
    });

    form->addRow(name, row);
    return k;
}

} // namespace

StripWindow::StripWindow(RadioModel* radio, QWidget* parent)
    : QDialog(parent)
    , m_radio(radio)
{
    setWindowTitle(QStringLiteral("Nereus Audio Channel Strip"));
    setModal(false);
    // Restore before the panels are built, so every control opens
    // showing what the chain actually holds rather than a default it
    // would then write back over the top.
    if (StripChain* c = chain()) {
        StripSettings::restore(*c);
        m_hadChain = true;
    }
    buildUi();
    refreshChainRow();
}

void StripWindow::adoptChainIfArrived()
{
    const bool have = chain() != nullptr;
    if (have == m_hadChain) { return; }
    m_hadChain = have;
    if (!have) { return; }

    // The radio turned up after this window opened. Restore, lay the
    // bands out, and rebuild every panel — a rebuild rather than a
    // refresh for the same reason as applying a preset: each control
    // reads its own value from the chain when it is built, so there is
    // no second copy to forget.
    StripChain* c = chain();
    StripSettings::restore(*c);
    reloadControls();
    refreshStagePictures();
    refreshChainRow();
    if (m_note) {
        m_note->setText(QStringLiteral(
            "Sits in front of the radio's own processing, on the "
            "microphone before WDSP sees it. Switched off, the audio is "
            "bit-for-bit what it was before this window existed — so "
            "switching off is a real comparison, not an approximate "
            "one."));
    }
    if (m_master) {
        const QSignalBlocker block(m_master);
        m_master->setChecked(c->isEnabled());
    }
}

// Read a stage value for a control's initial position.
//
// Without this the panels are a lie after a restart: StripSettings has
// already put the operator's values into the chain, but every slider
// would open at the literal default written in the call below — and the
// first touch of any slider would then write that default over the
// restored value. Persistence that silently undoes itself on the next
// nudge is worse than none, because it looks like it works.
double StripWindow::cur(const std::function<float()>& read,
                        double fallback) const
{
    return chain() ? double(read()) : fallback;
}

StripWindow::~StripWindow()
{
    // If the window is destroyed with the apply-path bypass engaged,
    // the radio would keep a bypass it never asked for.
    setRadioBypass(false);
}

void StripWindow::setRadioBypass(bool on)
{
    if (!m_radio) { return; }
    TransmitModel& tx = m_radio->transmitModel();

    if (on) {
        if (m_radioBypassed) { return; }
        m_hadTxEq    = tx.txEqEnabled();
        m_hadLeveler = tx.txLevelerOn();
        m_hadCfc     = tx.cfcEnabled();
        m_radioBypassed = true;
        tx.setTxEqEnabled(false);
        tx.setTxLevelerOn(false);
        tx.setCfcEnabled(false);
        return;
    }
    if (!m_radioBypassed) { return; }
    m_radioBypassed = false;
    // Exactly as they were, not "off". An operator who had the leveler
    // on before opening this window must have it on afterwards, and
    // finding out otherwise on the air is not acceptable.
    tx.setTxEqEnabled(m_hadTxEq);
    tx.setTxLevelerOn(m_hadLeveler);
    tx.setCfcEnabled(m_hadCfc);
}

// setSelfMonitor is gone (2026-08-11) — the live listen path left this
// window with the "Hear myself" checkbox; the record-then-listen
// monitor never engages the off-air chain or the MON mix. setRadioBypass
// stays: the voice-check's strip apply-path still uses it.

// ── PUDU monitor button surface (AetherSDR contract) ────────────────

void StripWindow::setMonitorRecording(bool on)
{
    if (!m_monRecordBtn) { return; }
    m_monRecordBtn->setText(on ? QStringLiteral("■ Stop")
                               : QStringLiteral("● Record"));
    if (m_monPlayBtn) {
        m_monPlayBtn->setEnabled(!on && m_monHasRecording);
    }
}

void StripWindow::setMonitorPlaying(bool on)
{
    if (!m_monPlayBtn) { return; }
    m_monPlayBtn->setText(on ? QStringLiteral("■ Stop")
                             : QStringLiteral("▶ Play"));
}

void StripWindow::setMonitorHasRecording(bool has)
{
    m_monHasRecording = has;
    if (m_monPlayBtn) { m_monPlayBtn->setEnabled(has); }
}

void StripWindow::persist()
{
    StripChain* c = chain();
    if (!c) { return; }
    StripSettings::save(*c);
}

StripChain* StripWindow::chain() const
{
    return m_radio ? m_radio->stripChain() : nullptr;
}

void StripWindow::buildUi()
{
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(14, 14, 14, 14);
    col->setSpacing(10);

    // ── Master ───────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        m_master = new QCheckBox(QStringLiteral("Channel strip"), this);
        m_master->setChecked(chain() && chain()->isEnabled());
        row->addWidget(m_master);

        // ── The AetherSDR monitor buttons (2026-08-11) ───────────────
        //
        // The "Hear myself" live checkbox that stood here is gone —
        // asked for at the bench after a day of almost-right: no
        // direct listening anywhere, only AetherSDR's record-then-
        // listen loop. These are AetherialAudioStrip's two buttons,
        // verbatim in behaviour: Record toggles the capture (30 s cap,
        // auto-plays when it stops), Play replays the last take. The
        // monitor itself lives in MainWindow; this window only hosts
        // the buttons — same division of labour as upstream.
        m_monRecordBtn = new QPushButton(QStringLiteral("● Record"), this);
        m_monRecordBtn->setStyleSheet(Style::buttonBaseStyle());
        m_monRecordBtn->setToolTip(QStringLiteral(
            "Record your processed voice off the strip (up to 30 s) — "
            "it plays back by itself when you stop. Nothing is "
            "transmitted; the band is quiet while you record and "
            "listen."));
        connect(m_monRecordBtn, &QPushButton::clicked,
                this, &StripWindow::monitorRecordClicked);
        row->addWidget(m_monRecordBtn);

        m_monPlayBtn = new QPushButton(QStringLiteral("▶ Play"), this);
        m_monPlayBtn->setStyleSheet(Style::buttonBaseStyle());
        m_monPlayBtn->setEnabled(false);
        m_monPlayBtn->setToolTip(QStringLiteral(
            "Replay the last take."));
        connect(m_monPlayBtn, &QPushButton::clicked,
                this, &StripWindow::monitorPlayClicked);
        row->addWidget(m_monPlayBtn);

        row->addStretch(1);
        col->addLayout(row);

        m_note = new QLabel(this);
        m_note->setWordWrap(true);
        m_note->setStyleSheet(dimStyle());
        m_note->setText(chain()
            ? QStringLiteral(
                  "Sits in front of the radio's own processing, on the "
                  "microphone before WDSP sees it. Switched off, the audio "
                  "is bit-for-bit what it was before this window existed — "
                  "so switching off is a real comparison, not an "
                  "approximate one.")
            : QStringLiteral(
                  "Not connected to a radio. The strip is created with the "
                  "transmit pump, so there is nothing to set up yet."));
        col->addWidget(m_note);
    }

    // ── Starting points ──────────────────────────────────────────────
    //
    // First, above everything, because "what should I set all this to"
    // is the question an operator actually arrives with. Eight stages
    // at their defaults is not an answer.
    {
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel(QStringLiteral("Preset:"), this));
        m_presetBox = new QComboBox(this);
        m_presetBox->setMinimumWidth(200);
        row->addWidget(m_presetBox);

        m_presetSave = new QPushButton(QStringLiteral("Save as…"), this);
        m_presetSave->setStyleSheet(Style::buttonBaseStyle());
        row->addWidget(m_presetSave);
        m_presetDelete = new QPushButton(QStringLiteral("Delete"), this);
        m_presetDelete->setStyleSheet(Style::buttonBaseStyle());
        row->addWidget(m_presetDelete);
        row->addStretch(1);

        // A/B. Held down, not toggled: a comparison you have to keep
        // your finger on is one you cannot walk away from and forget,
        // and forgetting which state you left it in is the failure that
        // makes people distrust their own ears.
        m_compareBtn = new QPushButton(QStringLiteral("A/B — hold"), this);
        m_compareBtn->setStyleSheet(Style::buttonBaseStyle());
        m_compareBtn->setToolTip(QStringLiteral(
            "While held, the strip is bypassed so you hear the raw "
            "microphone. Releasing puts it back exactly as it was."));
        row->addWidget(m_compareBtn);
        col->addLayout(row);

        m_presetNote = new QLabel(this);
        m_presetNote->setWordWrap(true);
        m_presetNote->setStyleSheet(dimStyle());
        col->addWidget(m_presetNote);

        rebuildPresetBox();

        connect(m_presetBox, &QComboBox::currentIndexChanged, this,
                [this](int) {
            const QString n = m_presetBox->currentData().toString();
            if (!n.isEmpty()) { applyPreset(n); }
            m_presetDelete->setEnabled(
                m_presetBox->currentData(Qt::UserRole + 1).toBool());
        });
        connect(m_presetSave, &QPushButton::clicked,
                this, &StripWindow::saveUserPreset);
        connect(m_presetDelete, &QPushButton::clicked,
                this, &StripWindow::deleteUserPreset);

        connect(m_compareBtn, &QPushButton::pressed, this, [this]() {
            if (StripChain* c = chain()) {
                m_masterBeforeCompare = c->isEnabled();
                c->setEnabled(false);
                refreshChainRow();
            }
        });
        connect(m_compareBtn, &QPushButton::released, this, [this]() {
            if (StripChain* c = chain()) {
                c->setEnabled(m_masterBeforeCompare);
                refreshChainRow();
            }
        });
    }

    // ── Chain row ────────────────────────────────────────────────────
    //
    // Painted rather than eight labels: each tile carries its own
    // gain-reduction bar, and the whole chain's behaviour becomes one
    // glance instead of eight numbers to read while speaking.
    m_chainView = new StripChainView(this);
    m_chainView->setChain(chain());
    connect(m_chainView, &StripChainView::stageClicked, this, [this](int i) {
        if (m_tabs) { m_tabs->setCurrentIndex(i); }
    });
    col->addWidget(m_chainView);

    // In, out, and the difference. Directly under the chain, because
    // the chain is what changed the level and this is by how much.
    m_levels = new StripLevelBars(this);
    m_levels->setChain(chain());
    col->addWidget(m_levels);

    // ── One tab per stage ────────────────────────────────────────────
    m_tabs = new QTabWidget(this);
    // Built below; each panel registers its own enable box, and they are
    // synced to the chain immediately afterwards.
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        const auto s = static_cast<StripChain::Stage>(i);
        QWidget* page = nullptr;
        switch (s) {
        case StripChain::Stage::Gate:  page = buildGatePanel();  break;
        case StripChain::Stage::Eq:    page = buildEqPanel();    break;
        case StripChain::Stage::DeEss: page = buildDeEssPanel(); break;
        case StripChain::Stage::Comp:    page = buildCompPanel();    break;
        case StripChain::Stage::Tube:    page = buildTubePanel();    break;
        case StripChain::Stage::Pudu:    page = buildPuduPanel();    break;
        case StripChain::Stage::Reverb:  page = buildReverbPanel();  break;
        case StripChain::Stage::Limiter: page = buildLimiterPanel(); break;
        default:                         page = buildPlaceholder(s); break;
        }
        m_tabs->addTab(page,
                       QString::fromLatin1(StripChain::stageName(s)));
    }

    // After all the stages, because it measures the result of them.
    m_tabs->addTab(buildTxSpectrumPanel(), QStringLiteral("On air"));

    // ── Voice check, embedded (2026-08-11) ───────────────────────────
    // Asked for at the bench: one window for the whole loop — change a
    // stage, record, listen, compare. The dialog already knows how;
    // embedding it here retires the standalone window. Created once and
    // parented to the window (see the member note), then handed to the
    // tab bar.
    m_voiceCheck = new TxVoiceCheckDialog(m_radio, this, /*embedded=*/true);
    m_tabs->addTab(m_voiceCheck, QStringLiteral("Voice check"));

    col->addWidget(m_tabs, 1);

    // The enable boxes are created unchecked and the chain may already
    // have stages on from the saved settings. Sync with signals blocked
    // so this does not write the pre-sync state straight back out.
    if (StripChain* c = chain()) {
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            QCheckBox* box = m_stageBoxes[static_cast<size_t>(i)];
            if (!box) { continue; }
            const QSignalBlocker block(box);
            box->setChecked(c->stageEnabled(static_cast<StripChain::Stage>(i)));
        }
    }

    auto* closeRow = new QHBoxLayout;
    closeRow->addStretch(1);
    auto* closeBtn = new QPushButton(QStringLiteral("Close"), this);
    closeBtn->setStyleSheet(Style::buttonBaseStyle());
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    closeRow->addWidget(closeBtn);
    col->addLayout(closeRow);

    connect(m_master, &QCheckBox::toggled, this, [this](bool on) {
        // The master itself is not saved — it loads off every time, on
        // purpose — but toggling it is a good moment to flush the rest.
        if (StripChain* c = chain()) { c->setEnabled(on); }
        refreshChainRow();
        persist();
    });

    // Meters only. Parameters are pushed on change, not polled — a
    // window that writes settings on a timer fights the operator's
    // hand.
    m_meterTimer = new QTimer(this);
    m_meterTimer->setInterval(100);
    connect(m_meterTimer, &QTimer::timeout, this, &StripWindow::refreshMeters);
    m_meterTimer->start();

    setMinimumWidth(560);
}

// ── Gate ─────────────────────────────────────────────────────────────

QWidget* StripWindow::buildGatePanel()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    const int idx = static_cast<int>(StripChain::Stage::Gate);
    auto* on = new QCheckBox(QStringLiteral("Gate on"), page);
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    form->addRow(on);

    m_gateCurve = new StripDynamicsCurve(StripDynamicsCurve::Stage::Gate, page);
    m_gateCurve->setChain(chain());
    form->addRow(buildStageCard(page, m_gateCurve,
                                StripChain::Stage::Gate));
    connect(on, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) {
            c->setStageEnabled(StripChain::Stage::Gate, v);
        }
        refreshChainRow();
        persist();
    });

    // The two-entry "Character" combo that used to sit here is gone. It
    // wrote ratio and depth, and so does the character picker on the
    // card above — two controls owning one piece of state, which is the
    // drift this project has already paid for more than once. The
    // picker supersedes it and offers four more entries.

    addKnob(form, QStringLiteral("Threshold"),
        -80.0, 0.0, 1.0, cur([this]{ return chain()->gate().thresholdDb(); }, -40.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setThresholdDb(float(v)); }
        }, 0, [this]{ persist(); });
    addKnob(form, QStringLiteral("Attack"), 0.1, 100.0, 0.1, cur([this]{ return chain()->gate().attackMs(); }, 0.5),
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setAttackMs(float(v)); }
        }, 1, [this]{ persist(); });
    // The step used to be `cur(comp().attackMs(), 5.0)` — a copy-paste
    // from the compressor panel that made the gate's Hold slider step by
    // whatever the COMPRESSOR's attack happened to be. Invisible while
    // both defaulted to 5 ms, and a 5000-step slider the moment anyone
    // set a fast compressor attack.
    addKnob(form, QStringLiteral("Hold"), 0.0, 500.0, 5.0,
        cur([this]{ return chain()->gate().holdMs(); }, 20.0),
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setHoldMs(float(v)); }
        }, 0, [this]{ persist(); });
    addKnob(form, QStringLiteral("Release"), 5.0, 2000.0, 5.0, cur([this]{ return chain()->gate().releaseMs(); }, 100.0),
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setReleaseMs(float(v)); }
        }, 0);
    addKnob(form, QStringLiteral("Depth"), -80.0, 0.0, 1.0, cur([this]{ return chain()->gate().floorDb(); }, -15.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setFloorDb(float(v)); }
        }, 0);
    addKnob(form, QStringLiteral("Hysteresis"), 0.0, 20.0, 0.5, cur([this]{ return chain()->gate().returnDb(); }, 2.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setReturnDb(float(v)); }
        }, 1, [this]{ persist(); });

    auto* help = new QLabel(QStringLiteral(
        "Threshold is the level below which the gate starts working — set "
        "it between your voice and the room, not at either. Hysteresis is "
        "what stops it chattering when you sit right on the threshold: the "
        "gate stays open until the level drops this much further."), page);
    help->setWordWrap(true);
    help->setStyleSheet(dimStyle());
    form->addRow(help);

    m_gateMeter = new QLabel(page);
    m_gateMeter->setStyleSheet(dimStyle());
    form->addRow(m_gateMeter);

    return page;
}

// ── EQ ───────────────────────────────────────────────────────────────
//
// Sixteen bands is what the DSP offers and far more than a voice needs,
// so this panel does not expose sixteen sliders. It lays the bands out
// once, in a fixed order, and gives each group the control that group
// is actually for:
//
//   band 0      high-pass — the single most useful control on a
//               transmit EQ, and the one that fixes rumble and hum
//   bands 1-3   notches at the mains frequency and its first two
//               harmonics, switched as a group
//   bands 4-6   low, presence and high tone controls
//
// The fixed layout is also what makes the saved settings meaningful:
// band 2 is always the first mains harmonic, in the file as on screen.






QWidget* StripWindow::buildEqPanel()
{
    // ── AetherSDR's equaliser, whole ─────────────────────────────────
    //
    // Asked for at the bench: "delete what is here and take AetherSDR's".
    // What stood here before was roughly 700 lines of NereusSDR-original
    // interface — the fifteen-second take recorder, editable target
    // curves with A/B slots, loudness-matched bypass, match-EQ from a
    // WAV, band solo, the numeric table, the folding sections. All of it
    // is gone from the tab; all of it is still in the history, and this
    // commit is a single `git revert` away from bringing it back.
    //
    // The DSP underneath never changed: core/strip/ClientEq was already
    // a verbatim port of AetherSDR's at the same revision. This puts the
    // interface that was written for it back on top of it.
    //
    // EqHost is the seam. See gui/applets/eq/EqHost.h — the ported
    // widgets talk to an AudioEngine with five methods on it, and rather
    // than editing five borrowed files to reach into StripChain, the
    // five names are presented over NereusSDR's own parts. The borrowed
    // code stays byte-comparable against upstream.
    if (!m_eqHost) {
        m_eqHost = std::make_unique<EqHost>(chain());
    } else {
        m_eqHost->setChain(chain());
    }

    m_eqPanel = new StripEqPanel(m_eqHost.get(), this);
    m_eqPanel->showForPath(ClientEqApplet::Path::Tx);
    return m_eqPanel;
}

// ── What actually goes out ───────────────────────────────────────────

QWidget* StripWindow::buildTxSpectrumPanel()
{
    auto* page = new QWidget(this);
    auto* col = new QVBoxLayout(page);
    col->setContentsMargins(8, 8, 8, 8);
    col->setSpacing(7);

    m_txSpectrum = new TxSpectrumWidget(page);
    if (m_radio && m_radio->audioEngine()) {
        m_txSpectrum->setSource(&m_radio->audioEngine()->txSiphonSpectrum());
    }
    col->addWidget(m_txSpectrum, 1);

    auto* row = new QHBoxLayout;
    row->setSpacing(7);

    auto* hold = new QPushButton(QStringLiteral("Hold"), page);
    hold->setCheckable(true);
    hold->setStyleSheet(Style::buttonBaseStyle());
    hold->setToolTip(QStringLiteral(
        "Keep the highest level reached in every bin. A live curve "
        "during a call is unreadable; this is what you look at "
        "afterwards."));
    row->addWidget(hold);

    auto* reset = new QPushButton(QStringLiteral("Reset"), page);
    reset->setStyleSheet(Style::buttonBaseStyle());
    row->addWidget(reset);
    row->addStretch(1);
    col->addLayout(row);

    connect(hold, &QPushButton::toggled, this, [this](bool on) {
        if (m_txSpectrum) { m_txSpectrum->setHold(on); }
    });
    connect(reset, &QPushButton::clicked, this, [this]() {
        if (m_txSpectrum) { m_txSpectrum->resetHold(); }
    });

    // ── The sentence, which is the point ─────────────────────────────
    auto* advice = new QLabel(page);
    advice->setWordWrap(true);
    advice->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }")
            .arg(QString::fromLatin1(Style::kTextSecondary)));
    col->addWidget(advice);

    connect(m_txSpectrum, &TxSpectrumWidget::measured, this,
            [advice](const TxSpectrumAnalysis::Occupancy& occ) {
        // The filter width the advice is judged against. 2.7 kHz is the
        // usual SSB filter; reading the real one out of the radio would
        // be better and is a wire that does not exist yet, so the
        // number is stated rather than assumed silently.
        const QString a = TxSpectrumAnalysis::advice(occ, 2700.0);
        advice->setText(a.isEmpty()
            ? QStringLiteral("Within a normal 2.7 kHz SSB filter.")
            : a);
        advice->setVisible(true);
    });

    auto* note = new QLabel(QStringLiteral(
        "This is the post-modulator siphon: what the transmit chain "
        "produced, before the amplifier, the filters and the antenna. "
        "A clean reading here with a dirty signal on the air means the "
        "problem is downstream of this point."), page);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }")
            .arg(QString::fromLatin1(Style::kTextScale)));
    col->addWidget(note);

    return page;
}

// ── De-esser ─────────────────────────────────────────────────────────

QWidget* StripWindow::buildDeEssPanel()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    const int idx = static_cast<int>(StripChain::Stage::DeEss);
    auto* on = new QCheckBox(QStringLiteral("De-esser on"), page);
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    form->addRow(on);

    m_deEssCurve = new StripBandCurve(StripBandCurve::Stage::DeEsser, page);
    m_deEssCurve->setChain(chain());
    form->addRow(buildStageCard(page, m_deEssCurve,
                                StripChain::Stage::DeEss));
    connect(on, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) {
            c->setStageEnabled(StripChain::Stage::DeEss, v);
        }
        refreshChainRow();
        persist();
    });

    addKnob(form, QStringLiteral("Frequency"), 1000.0, 12000.0, 100.0, cur([this]{ return chain()->deEss().frequencyHz(); }, 6000.0),
        QStringLiteral("Hz"), [this](double v) {
            if (StripChain* c = chain()) { c->deEss().setFrequencyHz(float(v)); }
        }, 0, [this]{ persist(); });
    addKnob(form, QStringLiteral("Width"), 0.5, 5.0, 0.1, cur([this]{ return chain()->deEss().q(); }, 2.0),
        QStringLiteral("Q"), [this](double v) {
            if (StripChain* c = chain()) { c->deEss().setQ(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Threshold"), -60.0, 0.0, 1.0, cur([this]{ return chain()->deEss().thresholdDb(); }, -25.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->deEss().setThresholdDb(float(v)); }
        }, 0, [this]{ persist(); });
    addKnob(form, QStringLiteral("Amount"), -24.0, 0.0, 0.5, cur([this]{ return chain()->deEss().amountDb(); }, -6.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->deEss().setAmountDb(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Attack"), 0.1, 30.0, 0.1, cur([this]{ return chain()->deEss().attackMs(); }, 1.0),
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->deEss().setAttackMs(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Release"), 10.0, 500.0, 5.0, cur([this]{ return chain()->deEss().releaseMs(); }, 80.0),
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->deEss().setReleaseMs(float(v)); }
        }, 0, [this]{ persist(); });

    auto* help = new QLabel(QStringLiteral(
        "Set the frequency by ear on the word \u201csix\u201d, not by the number: "
        "where sibilance lives depends on the voice and the microphone, "
        "and 6 kHz is only a place to start.\n\n"
        "Amount is a limit, not a setting — it is the most the de-esser "
        "may take off. Too much of it turns an s into a th, which is "
        "harder to listen to than the sibilance was."), page);
    help->setWordWrap(true);
    help->setStyleSheet(dimStyle());
    form->addRow(help);

    m_deEssMeter = new QLabel(page);
    m_deEssMeter->setStyleSheet(dimStyle());
    form->addRow(m_deEssMeter);

    return page;
}

// ── Compressor ───────────────────────────────────────────────────────

QWidget* StripWindow::buildCompPanel()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    const int idx = static_cast<int>(StripChain::Stage::Comp);
    auto* on = new QCheckBox(QStringLiteral("Compressor on"), page);
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    form->addRow(on);

    m_compCurve = new StripDynamicsCurve(StripDynamicsCurve::Stage::Compressor, page);
    m_compCurve->setChain(chain());
    form->addRow(buildStageCard(page, m_compCurve,
                                StripChain::Stage::Comp));
    connect(on, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) {
            c->setStageEnabled(StripChain::Stage::Comp, v);
        }
        refreshChainRow();
        persist();
    });

    addKnob(form, QStringLiteral("Threshold"), -60.0, 0.0, 1.0, cur([this]{ return chain()->comp().thresholdDb(); }, -20.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setThresholdDb(float(v)); }
        }, 0, [this]{ persist(); });
    addKnob(form, QStringLiteral("Ratio"), 1.0, 20.0, 0.5, cur([this]{ return chain()->comp().ratio(); }, 3.0),
        QStringLiteral(": 1"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setRatio(float(v)); }
        }, 1, [this]{ persist(); });
    // Opened at a literal 5.0 rather than at the chain's value, so the
    // compressor's attack was reset to 5 ms every time the panel was
    // built — and rebuilt is what the panel does on every preset and
    // every character.
    addKnob(form, QStringLiteral("Attack"), 0.1, 100.0, 0.1,
        cur([this]{ return chain()->comp().attackMs(); }, 5.0),
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setAttackMs(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Release"), 5.0, 2000.0, 5.0, cur([this]{ return chain()->comp().releaseMs(); }, 120.0),
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setReleaseMs(float(v)); }
        }, 0, [this]{ persist(); });
    addKnob(form, QStringLiteral("Knee"), 0.0, 24.0, 0.5, cur([this]{ return chain()->comp().kneeDb(); }, 6.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setKneeDb(float(v)); }
        }, 1, [this]{ persist(); });
    // The saved value was in the MINIMUM slot and the value was a
    // literal zero, so the knob's range started wherever make-up
    // happened to be and the knob itself always opened at the bottom of
    // it. Slots are (min, max, step, value).
    addKnob(form, QStringLiteral("Make-up"), 0.0, 24.0, 0.5,
        cur([this]{ return chain()->comp().makeupDb(); }, 0.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setMakeupDb(float(v)); }
        }, 1, [this]{ persist(); });

    // The phase rotator earns its own row and its own sentence: it is
    // the one control here that raises average power without making
    // anything louder, and nobody guesses that from the name.
    auto* rot = new QSpinBox(page);
    rot->setRange(0, 8);
    rot->setValue(chain() ? chain()->comp().phaseRotatorStages() : 0);
    form->addRow(QStringLiteral("Phase rotator"), rot);
    connect(rot, &QSpinBox::valueChanged, this, [this](int v) {
        if (StripChain* c = chain()) { c->comp().setPhaseRotatorStages(v); }
        persist();
    });

    // ── How to set it ────────────────────────────────────────────────
    //
    // The compressor had no setting guidance at all — the only help text
    // on this tab was about the phase rotator, which is the smallest
    // control on it. Asked for at the bench and correct: a compressor is
    // four interacting numbers and the order you set them in is most of
    // the skill.
    auto* order = new QLabel(QStringLiteral(
        "<b>Set them in this order.</b> Each one changes what the next "
        "one should be, so going round the panel left to right means "
        "doing it twice."
        "<ol style='margin-left:-22px'>"
        "<li><b>Ratio</b> first — it decides what kind of processing this "
        "is. 2:1 is levelling, 6:1 is talk power, 10:1 and up is an "
        "effect.</li>"
        "<li><b>Threshold</b> next, watching the gain reduction. Aim for "
        "6 to 8 dB on your loudest words and nothing at all on your "
        "quietest. If it never moves, the threshold is too high; if it "
        "never returns to zero, too low.</li>"
        "<li><b>Attack</b>. Fast catches the peak and dulls the "
        "consonant that made it; slow lets the consonant through and "
        "leaves a peak for the limiter. 5–10 ms keeps the words crisp; "
        "under 2 ms is for when the limiter is working too hard.</li>"
        "<li><b>Release</b> last, by ear on continuous speech. Too fast "
        "and the background pumps up between words; too slow and one "
        "loud word ducks the sentence after it. 100–200 ms suits most "
        "voices.</li>"
        "</ol>"), page);
    order->setWordWrap(true);
    order->setStyleSheet(dimStyle());
    form->addRow(order);

    auto* knee = new QLabel(QStringLiteral(
        "<b>Knee</b> is how abruptly the ratio arrives. A wide knee "
        "starts compressing below the threshold and reaches the full "
        "ratio above it, which is why a 3:1 setting with a 10 dB knee "
        "sounds gentler than a 3:1 with a hard one — the number is the "
        "same and the sound is not.<br><br>"
        "<b>Make-up</b> is not a tone control. Set it so the stage is as "
        "loud switched on as it is switched off, then judge. Everything "
        "sounds better louder, and a comparison that is not level-matched "
        "is not a comparison — it is the make-up gain winning."), page);
    knee->setWordWrap(true);
    knee->setStyleSheet(dimStyle());
    form->addRow(knee);

    auto* help = new QLabel(QStringLiteral(
        "The phase rotator makes speech more symmetrical without changing "
        "how it sounds. Speech has lopsided peaks — one polarity reaches "
        "the limit while the other still has room — so evening them out "
        "buys a few decibels of average power for free. Four stages is a "
        "typical setting; more is not better."), page);
    help->setWordWrap(true);
    help->setStyleSheet(dimStyle());
    form->addRow(help);

    m_compMeter = new QLabel(page);
    m_compMeter->setStyleSheet(dimStyle());
    form->addRow(m_compMeter);

    return page;
}

// ── Tube ─────────────────────────────────────────────────────────────

QWidget* StripWindow::buildTubePanel()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    const int idx = static_cast<int>(StripChain::Stage::Tube);
    auto* on = new QCheckBox(QStringLiteral("Tube on"), page);
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    form->addRow(on);

    m_tubeCurve = new StripShaperCurve(page);
    m_tubeCurve->setChain(chain());
    form->addRow(buildStageCard(page, m_tubeCurve,
                                StripChain::Stage::Tube));
    connect(on, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) {
            c->setStageEnabled(StripChain::Stage::Tube, v);
        }
        refreshChainRow();
        persist();
    });

    auto* model = new QComboBox(page);
    model->addItem(QStringLiteral("A — soft"), int(ClientTube::Model::A));
    model->addItem(QStringLiteral("B — hard"), int(ClientTube::Model::B));
    model->addItem(QStringLiteral("C — asymmetric"), int(ClientTube::Model::C));
    {
        const int have = chain() ? int(chain()->tube().model())
                                 : int(ClientTube::Model::A);
        const int at = model->findData(have);
        if (at >= 0) { model->setCurrentIndex(at); }
    }
    // Labelled "Waveshaper", not "Character". The stage card above now
    // carries a Character picker like every other stage, and two combos
    // both called Character in one panel is a panel nobody can describe
    // to anybody else. This one is the genuine algorithm choice; the
    // character sets it along with drive, bias and mix.
    form->addRow(QStringLiteral("Waveshaper"), model);
    connect(model, &QComboBox::currentIndexChanged, this, [this, model](int) {
        if (StripChain* c = chain()) {
            c->tube().setModel(
                static_cast<ClientTube::Model>(model->currentData().toInt()));
        }
        persist();
    });

    addKnob(form, QStringLiteral("Drive"), 0.0, 24.0, 0.5,
        cur([this]{ return chain()->tube().driveDb(); }, 0.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->tube().setDriveDb(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Tone"), -1.0, 1.0, 0.05,
        cur([this]{ return chain()->tube().tone(); }, 0.0),
        QString(), [this](double v) {
            if (StripChain* c = chain()) { c->tube().setTone(float(v)); }
        }, 2, [this]{ persist(); });
    addKnob(form, QStringLiteral("Mix"), 0.0, 1.0, 0.05,
        cur([this]{ return chain()->tube().dryWet(); }, 1.0),
        QString(), [this](double v) {
            if (StripChain* c = chain()) { c->tube().setDryWet(float(v)); }
        }, 2, [this]{ persist(); });
    addKnob(form, QStringLiteral("Output"), -24.0, 24.0, 0.5,
        cur([this]{ return chain()->tube().outputGainDb(); }, 0.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->tube().setOutputGainDb(float(v)); }
        }, 1, [this]{ persist(); });

    auto* help = new QLabel(QStringLiteral(
        "Saturation adds harmonics that were not in your voice. On SSB "
        "that is not free: they land inside the transmitted bandwidth and "
        "some of them sound like distortion to the far end even when they "
        "sound flattering in headphones.\n\n"
        "Mix is the useful control. A little of the saturated signal "
        "under the clean one thickens a thin voice; all of it is an "
        "effect. Drive past about 6 dB with Mix at 1.0 is where operators "
        "start being told they sound rough."), page);
    help->setWordWrap(true);
    help->setStyleSheet(dimStyle());
    form->addRow(help);
    return page;
}

// ── PUDU — the AetherVoice exciter ───────────────────────────────────

QWidget* StripWindow::buildPuduPanel()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    const int idx = static_cast<int>(StripChain::Stage::Pudu);
    auto* on = new QCheckBox(QStringLiteral("Exciter on"), page);
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    form->addRow(on);

    m_puduCurve = new StripBandCurve(StripBandCurve::Stage::Exciter, page);
    m_puduCurve->setChain(chain());
    form->addRow(buildStageCard(page, m_puduCurve,
                                StripChain::Stage::Pudu));
    connect(on, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) {
            c->setStageEnabled(StripChain::Stage::Pudu, v);
        }
        refreshChainRow();
        persist();
    });

    auto* mode = new QComboBox(page);
    mode->addItem(QStringLiteral("Aural exciter + big bottom"),
                  int(ClientPudu::Mode::Aphex));
    mode->addItem(QStringLiteral("Sonic exciter"),
                  int(ClientPudu::Mode::Behringer));
    {
        const int have = chain() ? int(chain()->pudu().mode())
                                 : int(ClientPudu::Mode::Aphex);
        const int at = mode->findData(have);
        if (at >= 0) { mode->setCurrentIndex(at); }
    }
    form->addRow(QStringLiteral("Model"), mode);
    connect(mode, &QComboBox::currentIndexChanged, this, [this, mode](int) {
        if (StripChain* c = chain()) {
            c->pudu().setMode(
                static_cast<ClientPudu::Mode>(mode->currentData().toInt()));
        }
        persist();
    });

    auto* lowLbl = new QLabel(QStringLiteral("<b>Low end</b>"), page);
    form->addRow(lowLbl);
    addKnob(form, QStringLiteral("Tune"), 50.0, 160.0, 1.0,
        cur([this]{ return chain()->pudu().pooTuneHz(); }, 90.0),
        QStringLiteral("Hz"), [this](double v) {
            if (StripChain* c = chain()) { c->pudu().setPooTuneHz(float(v)); }
        }, 0, [this]{ persist(); });
    addKnob(form, QStringLiteral("Drive"), 0.0, 24.0, 0.5,
        cur([this]{ return chain()->pudu().pooDriveDb(); }, 0.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->pudu().setPooDriveDb(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Amount"), 0.0, 1.0, 0.05,
        cur([this]{ return chain()->pudu().pooMix(); }, 0.0),
        QString(), [this](double v) {
            if (StripChain* c = chain()) { c->pudu().setPooMix(float(v)); }
        }, 2, [this]{ persist(); });

    auto* hiLbl = new QLabel(QStringLiteral("<b>Top end</b>"), page);
    form->addRow(hiLbl);
    addKnob(form, QStringLiteral("Tune "), 1000.0, 10000.0, 100.0,
        cur([this]{ return chain()->pudu().dooTuneHz(); }, 3000.0),
        QStringLiteral("Hz"), [this](double v) {
            if (StripChain* c = chain()) { c->pudu().setDooTuneHz(float(v)); }
        }, 0, [this]{ persist(); });
    addKnob(form, QStringLiteral("Harmonics"), 0.0, 24.0, 0.5,
        cur([this]{ return chain()->pudu().dooHarmonicsDb(); }, 0.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) {
                c->pudu().setDooHarmonicsDb(float(v));
            }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Amount "), 0.0, 1.0, 0.05,
        cur([this]{ return chain()->pudu().dooMix(); }, 0.0),
        QString(), [this](double v) {
            if (StripChain* c = chain()) { c->pudu().setDooMix(float(v)); }
        }, 2, [this]{ persist(); });

    auto* help = new QLabel(QStringLiteral(
        "An exciter does not lift the top end — it invents harmonics "
        "above what is there and mixes them in, which the ear reads as "
        "detail that was always present. That is why a little sounds "
        "like a better microphone and a lot sounds artificial.\n\n"
        "On SSB the top-end tune is the control to watch. Set it above "
        "about 2.5 kHz and most of what it generates lands outside the "
        "transmit filter: you hear the effect in the monitor and the far "
        "end gets nothing but the intermodulation that came with it.\n\n"
        "The low end works the other way round — it synthesises an "
        "octave below the tune frequency, which on a 2.8 kHz SSB channel "
        "is largely inaudible at the far end too. Both are worth trying "
        "and neither is free."), page);
    help->setWordWrap(true);
    help->setStyleSheet(dimStyle());
    form->addRow(help);
    return page;
}

// ── Reverb ───────────────────────────────────────────────────────────

QWidget* StripWindow::buildReverbPanel()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    const int idx = static_cast<int>(StripChain::Stage::Reverb);
    auto* on = new QCheckBox(QStringLiteral("Reverb on"), page);
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    form->addRow(on);
    connect(on, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) {
            c->setStageEnabled(StripChain::Stage::Reverb, v);
        }
        refreshChainRow();
        persist();
    });

    addKnob(form, QStringLiteral("Size"), 0.0, 1.0, 0.05,
        cur([this]{ return chain()->reverb().size(); }, 0.3),
        QString(), [this](double v) {
            if (StripChain* c = chain()) { c->reverb().setSize(float(v)); }
        }, 2, [this]{ persist(); });
    addKnob(form, QStringLiteral("Decay"), 0.3, 5.0, 0.1,
        cur([this]{ return chain()->reverb().decayS(); }, 0.8),
        QStringLiteral("s"), [this](double v) {
            if (StripChain* c = chain()) { c->reverb().setDecayS(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Damping"), 0.0, 1.0, 0.05,
        cur([this]{ return chain()->reverb().damping(); }, 0.5),
        QString(), [this](double v) {
            if (StripChain* c = chain()) { c->reverb().setDamping(float(v)); }
        }, 2, [this]{ persist(); });
    addKnob(form, QStringLiteral("Pre-delay"), 0.0, 100.0, 1.0,
        cur([this]{ return chain()->reverb().preDelayMs(); }, 20.0),
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->reverb().setPreDelayMs(float(v)); }
        }, 0, [this]{ persist(); });
    addKnob(form, QStringLiteral("Mix"), 0.0, 1.0, 0.01,
        cur([this]{ return chain()->reverb().mix(); }, 0.0),
        QString(), [this](double v) {
            if (StripChain* c = chain()) { c->reverb().setMix(float(v)); }
        }, 2, [this]{ persist(); });

    auto* help = new QLabel(QStringLiteral(
        "Almost nobody should use this, and it is here because the DSP "
        "is and hiding a stage that is running would be worse.\n\n"
        "Reverb on transmit audio fills the gaps between words with a "
        "tail, and those gaps are what a listener uses to separate you "
        "from the noise. On a weak signal it costs readability directly. "
        "If you want it at all, Mix below 0.1 with a short decay is the "
        "only setting that will not draw comment."), page);
    help->setWordWrap(true);
    help->setStyleSheet(dimStyle());
    form->addRow(help);
    return page;
}

// ── Limiter ──────────────────────────────────────────────────────────

QWidget* StripWindow::buildLimiterPanel()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    const int idx = static_cast<int>(StripChain::Stage::Limiter);
    auto* on = new QCheckBox(QStringLiteral("Limiter on"), page);
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    form->addRow(on);

    m_limiterCurve =
        new StripDynamicsCurve(StripDynamicsCurve::Stage::Limiter, page);
    m_limiterCurve->setChain(chain());
    form->addRow(buildStageCard(page, m_limiterCurve,
                                StripChain::Stage::Limiter));
    connect(on, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) {
            c->setStageEnabled(StripChain::Stage::Limiter, v);
        }
        refreshChainRow();
        persist();
    });

    addKnob(form, QStringLiteral("Ceiling"), -12.0, 0.0, 0.1,
        cur([this]{ return chain()->limiter().ceilingDb(); }, -1.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->limiter().setCeilingDb(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Output trim"), -12.0, 12.0, 0.5,
        cur([this]{ return chain()->limiter().outputTrimDb(); }, 0.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) {
                c->limiter().setOutputTrimDb(float(v));
            }
        }, 1, [this]{ persist(); });

    auto* dc = new QCheckBox(QStringLiteral("Remove DC offset"), page);
    dc->setChecked(chain() ? chain()->limiter().dcBlockEnabled() : true);
    form->addRow(dc);
    connect(dc, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) { c->limiter().setDcBlockEnabled(v); }
        persist();
    });

    auto* help = new QLabel(QStringLiteral(
        "The last thing in the chain, and it should stay that way. "
        "Everything above it can add gain — the tube, the exciter, the "
        "compressor's make-up — and this is what stops the sum arriving "
        "at the modulator hotter than it should.\n\n"
        "Leave it on. A ceiling of -1 dB gives the transmit chain a "
        "little room without costing anything audible; the reason not to "
        "use 0 is that everything downstream — resampling, the "
        "modulator — can overshoot slightly, and a signal already at "
        "full scale has nowhere to put the overshoot.\n\n"
        "DC offset removal matters more than it sounds: a microphone "
        "with a small offset spends part of the transmitter's headroom "
        "on a signal nobody can hear."), page);
    help->setWordWrap(true);
    help->setStyleSheet(dimStyle());
    form->addRow(help);
    return page;
}

// Every stage now has a real panel, so this is only reached if a stage
// is added and its panel is not. Kept for exactly that: a new stage
// should appear with an honest empty tab rather than crash or silently
// vanish from the window.
QWidget* StripWindow::buildPlaceholder(StripChain::Stage s)
{
    auto* page = new QWidget(this);
    auto* col = new QVBoxLayout(page);

    const int idx = static_cast<int>(s);
    auto* on = new QCheckBox(
        QStringLiteral("%1 on")
            .arg(QString::fromLatin1(StripChain::stageName(s))), page);
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    col->addWidget(on);
    connect(on, &QCheckBox::toggled, this, [this, s](bool v) {
        if (StripChain* c = chain()) { c->setStageEnabled(s, v); }
        refreshChainRow();
        persist();
    });

    auto* note = new QLabel(QStringLiteral(
        "The DSP for this stage is in place and tested; its controls are "
        "not built yet, so it runs at its defaults. Switching it on here "
        "does exactly that and nothing more.\n\n"
        "An empty tab is more honest than one full of controls that go "
        "nowhere."), page);
    note->setWordWrap(true);
    note->setStyleSheet(dimStyle());
    col->addWidget(note);
    col->addStretch(1);
    return page;
}

// ── Refresh ──────────────────────────────────────────────────────────

void StripWindow::rebuildPresetBox(const QString& select)
{
    if (!m_presetBox) { return; }
    const QSignalBlocker block(m_presetBox);
    m_presetBox->clear();
    m_presetBox->addItem(QStringLiteral("— choose —"), QString());

    // Built-ins first and the operator's own below, separated, so it is
    // always obvious which of the two a name is. A flat merged list is
    // where someone deletes a built-in they cannot delete and concludes
    // the button is broken.
    for (const auto& p : StripSettings::builtInPresets()) {
        m_presetBox->addItem(p.name, p.name);
        m_presetBox->setItemData(m_presetBox->count() - 1, false,
                                 Qt::UserRole + 1);
    }
    const QStringList mine = StripSettings::userPresetNames();
    if (!mine.isEmpty()) {
        m_presetBox->insertSeparator(m_presetBox->count());
        for (const QString& n : mine) {
            m_presetBox->addItem(n, n);
            m_presetBox->setItemData(m_presetBox->count() - 1, true,
                                     Qt::UserRole + 1);
        }
    }
    if (!select.isEmpty()) {
        const int i = m_presetBox->findData(select);
        if (i >= 0) { m_presetBox->setCurrentIndex(i); }
    }
    if (m_presetDelete) {
        m_presetDelete->setEnabled(
            m_presetBox->currentData(Qt::UserRole + 1).toBool());
    }
}

void StripWindow::saveUserPreset()
{
    StripChain* c = chain();
    if (!c) { return; }

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("Save preset"),
        QStringLiteral("Name:"), QLineEdit::Normal,
        m_presetBox ? m_presetBox->currentData().toString() : QString(), &ok);
    if (!ok) { return; }

    if (!StripSettings::saveUserPreset(name, *c)) {
        QMessageBox::warning(this, QStringLiteral("Save preset"),
            QStringLiteral("That name can't be used. It needs at least one "
                           "character and no slashes."));
        return;
    }
    rebuildPresetBox(name.trimmed());
    m_presetNote->setText(QStringLiteral("Saved as \u201c%1\u201d.")
                              .arg(name.trimmed()));
}

void StripWindow::deleteUserPreset()
{
    const QString name = m_presetBox
        ? m_presetBox->currentData().toString() : QString();
    if (name.isEmpty()) { return; }
    if (!m_presetBox->currentData(Qt::UserRole + 1).toBool()) { return; }

    if (QMessageBox::question(this, QStringLiteral("Delete preset"),
            QStringLiteral("Delete \u201c%1\u201d? The settings in use now "
                           "stay as they are.").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }
    StripSettings::removeUserPreset(name);
    rebuildPresetBox();
    m_presetNote->clear();
}

void StripWindow::applyPreset(const QString& name)
{
    StripChain* c = chain();
    if (!c) { return; }

    // A user preset of the same name wins: it is the one the operator
    // made, and shadowing a built-in with your own version of it is a
    // reasonable thing to want.
    bool applied = StripSettings::applyUserPreset(name, *c);
    if (!applied) { applied = StripSettings::applyBuiltIn(name, *c); }
    if (!applied) { return; }

    m_presetNote->setText(QStringLiteral("Your own setting: %1").arg(name));
    for (const auto& p : StripSettings::builtInPresets()) {
        if (p.name == name) { m_presetNote->setText(p.description); break; }
    }

    // A preset changes the chain underneath every control in the
    // window. Rebuilding the panels is the only version of "refresh"
    // that cannot drift: each control reads its own value from the
    // chain when it is built, so there is no second copy to forget.
    reloadControls();
    refreshChainRow();
    persist();
}

void StripWindow::reloadControls()
{
    if (!m_tabs) { return; }
    const int keep = m_tabs->currentIndex();
    // The voice check is not a stage panel and may hold a recording the
    // operator is comparing against — pull it out before the loop
    // deletes pages, put it back at the end. Parent goes back to the
    // window so the widget is owned even while off the tab bar.
    if (m_voiceCheck) {
        const int vcIdx = m_tabs->indexOf(m_voiceCheck);
        if (vcIdx >= 0) { m_tabs->removeTab(vcIdx); }
        m_voiceCheck->setParent(this);
        m_voiceCheck->hide();
    }
    while (m_tabs->count() > 0) {
        QWidget* w = m_tabs->widget(0);
        m_tabs->removeTab(0);
        w->deleteLater();
    }
    // Owned by the pages just removed. The equaliser panel goes with
    // them; the host it talks through is owned by this window and
    // survives, so the rebuilt panel gets a live engine rather than a
    // dangling one.
    m_eqPanel = nullptr;
    m_gateMeter = m_deEssMeter = m_compMeter = nullptr;
    // Owned by the pages just removed. Left dangling, the next
    // refreshMeters() would call isVisible() on freed memory — the same
    // class of fault as the late-binding one this window already had,
    // and one that only shows up under a rebuild.
    m_gateCurve = m_compCurve = m_limiterCurve = nullptr;
    m_tubeCurve = nullptr;
    m_deEssCurve = m_puduCurve = nullptr;
    // Same trap, missed once. The "On air" tab was added in buildUi()
    // and nowhere else, so the first preset or character applied made it
    // disappear for the rest of the session and left this pointer aimed
    // at a deleted widget. Both halves are fixed: nulled here, re-added
    // below. Anything buildUi() puts in the tab bar has to be put back
    // here too — the loop covers the eight stages and nothing else.
    m_txSpectrum = nullptr;
    m_stageText.fill(StageText{});
    m_stageBoxes.fill(nullptr);

    for (int i = 0; i < StripChain::kStageCount; ++i) {
        const auto s = static_cast<StripChain::Stage>(i);
        QWidget* page = nullptr;
        switch (s) {
        case StripChain::Stage::Gate:  page = buildGatePanel();  break;
        case StripChain::Stage::Eq:    page = buildEqPanel();    break;
        case StripChain::Stage::DeEss: page = buildDeEssPanel(); break;
        case StripChain::Stage::Comp:    page = buildCompPanel();    break;
        case StripChain::Stage::Tube:    page = buildTubePanel();    break;
        case StripChain::Stage::Pudu:    page = buildPuduPanel();    break;
        case StripChain::Stage::Reverb:  page = buildReverbPanel();  break;
        case StripChain::Stage::Limiter: page = buildLimiterPanel(); break;
        default:                         page = buildPlaceholder(s); break;
        }
        m_tabs->addTab(page, QString::fromLatin1(StripChain::stageName(s)));
    }
    m_tabs->addTab(buildTxSpectrumPanel(), QStringLiteral("On air"));
    // Back where buildUi() put it, same instance, recording intact.
    if (m_voiceCheck) {
        m_tabs->addTab(m_voiceCheck, QStringLiteral("Voice check"));
    }
    if (StripChain* c = chain()) {
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            QCheckBox* box = m_stageBoxes[static_cast<size_t>(i)];
            if (!box) { continue; }
            const QSignalBlocker block(box);
            box->setChecked(c->stageEnabled(static_cast<StripChain::Stage>(i)));
        }
    }
    if (keep >= 0 && keep < m_tabs->count()) { m_tabs->setCurrentIndex(keep); }
}

void StripWindow::showVoiceCheckTab()
{
    if (m_tabs && m_voiceCheck) {
        m_tabs->setCurrentWidget(m_voiceCheck);
    }
}





// Which character the operator last chose for a stage, and where it is
// kept. One settings key per stage, named after the stage, so a new
// stage needs no new key and a removed one leaves no orphan behind.
QString StripWindow::characterSettingsKey(StripChain::Stage s)
{
    return QStringLiteral("StripCharacter/")
           + QString::fromLatin1(StripChain::stageName(s));
}

QString StripWindow::characterKeyFor(StripChain::Stage s)
{
    return AppSettings::instance()
        .value(characterSettingsKey(s), QString()).toString();
}

void StripWindow::rememberCharacter(StripChain::Stage s, const QString& name)
{
    AppSettings::instance().setValue(characterSettingsKey(s), name);
    AppSettings::instance().save();
}

QWidget* StripWindow::buildStageCard(QWidget* page, QWidget* picture,
                                     StripChain::Stage stage)
{
    auto* card = new QWidget(page);
    auto* row  = new QHBoxLayout(card);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);

    // Square, and it stays square: a transfer curve has the same unit
    // on both axes and a stretched one lies about the 45° diagonal.
    picture->setParent(card);
    picture->setMinimumSize(260, 260);
    picture->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    row->addWidget(picture, 1);

    auto* words = new QWidget(card);
    auto* col   = new QVBoxLayout(words);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(7);

    const int idx = static_cast<int>(stage);
    StageText& st = m_stageText[static_cast<size_t>(idx)];
    st.picture = picture;

    st.live = new QLabel(words);
    st.live->setWordWrap(true);
    st.live->setTextFormat(Qt::RichText);
    st.live->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    st.live->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 13px; }")
            .arg(QString::fromLatin1(Style::kTextPrimary)));
    col->addWidget(st.live);

    st.legend = new QLabel(words);
    st.legend->setWordWrap(true);
    st.legend->setTextFormat(Qt::RichText);
    st.legend->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 11px; }")
            .arg(QString::fromLatin1(Style::kTextSecondary)));
    col->addWidget(st.legend);

    // ── The character picker ─────────────────────────────────────────
    //
    // Only where there is something to pick. An empty combo on the
    // stages that have no characters would be a control that teaches
    // people the controls are decorative.
    const QVector<StripCharacters::Character> chars =
        StripCharacters::forStage(stage);
    if (!chars.isEmpty()) {
        auto* pickRow = new QHBoxLayout;
        pickRow->addWidget(new QLabel(QStringLiteral("Character:"), words));
        auto* box = new QComboBox(words);
        for (const auto& ch : chars) { box->addItem(ch.name, ch.name); }
        box->setMinimumWidth(150);
        pickRow->addWidget(box, 1);

        // ── Show which one is actually in effect ─────────────────────
        //
        // Reported from the bench: change the tube's character and the
        // curve moves but the name does not. The combo was rebuilt from
        // scratch on every reloadControls() — which is what applying a
        // character triggers — so it always came back reading its first
        // entry while the chain held the values of whichever one had
        // just been picked. The picture and the label disagreed, and
        // the label was the one lying.
        //
        // The answer is not to remember what was clicked. It is to ask
        // the chain what it currently is: StripCharacters::inEffect()
        // compares the stage against every character and names the one
        // it matches, or none. That is true after a preset, after a
        // restore from disk, and after a knob has been moved — the
        // three cases a remembered name gets wrong.
        //
        // The remembered name is still kept, for one job only: when the
        // stage matches nothing, it says which character the operator
        // started from, so the note reads "Voodoo — edited" rather than
        // going blank and losing the explanation with it.
        //
        // Signals are blocked while setting the index. Otherwise
        // restoring it would re-apply the character and undo whatever
        // was edited.
        // Wired up here, kept current by refreshStageText() on the
        // meter timer. Doing it at build time only would mean the mark
        // appeared at the next panel rebuild rather than when the knob
        // moved — and for a stage nobody applies a preset to, that is
        // never.
        st.charBox = box;

        // A word after the combo when the stage no longer matches what
        // the combo names. Beside it rather than inside the entry, so
        // the list stays a list of characters and does not grow an
        // entry called "Voodoo — edited" that cannot be chosen.
        st.charMark = new QLabel(QStringLiteral("edited"), words);
        st.charMark->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-size: 11px; "
                           "font-style: italic; }")
                .arg(QString::fromLatin1(Style::kAmberText)));
        st.charMark->setToolTip(QStringLiteral(
            "The stage has been adjusted since this character was "
            "chosen. Picking it again puts the numbers back."));
        st.charMark->hide();
        pickRow->addWidget(st.charMark);
        col->addLayout(pickRow);

        st.charNote = new QLabel(words);
        st.charNote->setWordWrap(true);
        st.charNote->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-size: 11px; "
                           "background: %2; border: 1px solid %3; "
                           "padding: 6px; }")
                .arg(QString::fromLatin1(Style::kTextSecondary),
                     QString::fromLatin1(Style::kInsetBg),
                     QString::fromLatin1(Style::kInsetBorder)));
        col->addWidget(st.charNote);

        // `activated`, not `currentIndexChanged`. It fires on a choice
        // the operator made, including re-choosing the entry already
        // shown — which is what "picking it again puts the numbers
        // back" in the mark's tooltip depends on, and what
        // currentIndexChanged would swallow.
        connect(box, &QComboBox::activated, this,
                [this, box, stage](int) {
            // Applying a character moves several knobs at once, so the
            // panel is rebuilt rather than refreshed — every control
            // reads its own value from the chain when it is built, and
            // that is the only version of "refresh" that cannot drift.
            if (StripChain* c = chain()) {
                const QString name = box->currentData().toString();
                if (StripCharacters::apply(*c, stage, name)) {
                    // Remembered BEFORE the rebuild, because the rebuild
                    // destroys this combo and reads the name back.
                    rememberCharacter(stage, name);
                    persist();
                    reloadControls();
                    refreshChainRow();
                }
            }
        });

        // Fill the note now rather than leaving an empty box until the
        // first meter tick. lastSeen is left empty on purpose so the
        // comparison inside sees a change and does the work.
        if (StripChain* c = chain()) { refreshCharacter(st, stage, *c); }
    }

    col->addStretch(1);
    row->addWidget(words, 1);       // half and half
    return card;
}

void StripWindow::refreshStageText()
{
    StripChain* c = chain();
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        StageText& st = m_stageText[static_cast<size_t>(i)];
        if (!st.picture || !st.picture->isVisible()) { continue; }
        QString live, legend;
        if (auto* d = qobject_cast<StripDynamicsCurve*>(st.picture)) {
            live = d->explain(); legend = d->legend();
        } else if (auto* sh = qobject_cast<StripShaperCurve*>(st.picture)) {
            live = sh->explain(); legend = sh->legend();
        } else if (auto* b = qobject_cast<StripBandCurve*>(st.picture)) {
            live = b->explain(); legend = b->legend();
        }
        if (st.live)   { st.live->setText(live); }
        if (st.legend) { st.legend->setText(legend); }

        if (c && st.charBox) {
            refreshCharacter(st, static_cast<StripChain::Stage>(i), *c);
        }
    }
}

// ── Keeping the character honest ─────────────────────────────────────
//
// Runs on the meter timer, for the visible tab only. Recognising a
// character means trying every one of them against the stage, which is
// cheap but not free — so the stage's parameters are compared against
// the previous tick's first, and the work only happens when a number
// has actually moved.
void StripWindow::refreshCharacter(StageText& st, StripChain::Stage stage,
                                   const StripChain& c)
{
    const QVector<float> now = StripCharacters::captureStage(c, stage);
    if (now == st.lastSeen) { return; }
    st.lastSeen = now;

    const QString active   = StripCharacters::inEffect(c, stage);
    const QString started  = characterKeyFor(stage);
    const bool    edited   = active.isEmpty() && !started.isEmpty();
    const QString show     = edited ? started : active;

    if (st.charMark) { st.charMark->setVisible(edited); }

    // Blocked: setting the index fires nothing here, but `activated`
    // aside, a stray signal would re-apply the character and undo the
    // very edit being reported.
    if (!show.isEmpty()) {
        const int at = st.charBox->findData(show);
        if (at >= 0 && at != st.charBox->currentIndex()) {
            const QSignalBlocker block(st.charBox);
            st.charBox->setCurrentIndex(at);
        }
    }

    if (!st.charNote) { return; }
    for (const auto& ch : StripCharacters::forStage(stage)) {
        if (ch.name != show) { continue; }
        // A character is a starting point and adjusting from it is the
        // intended use, so this is not a warning. It is the difference
        // between a label describing the STAGE and one describing a
        // button somebody pressed once — without it the description
        // below explains settings the stage no longer has.
        st.charNote->setText(edited
            ? QStringLiteral("Started from %1, adjusted since — what follows "
                             "is where you began, not where you are.\n\n%2")
                  .arg(ch.name, ch.description)
            : ch.description);
        return;
    }
    // Matches nothing and nothing was ever chosen: a hand-built stage,
    // or one restored from a preset. Say that rather than describing a
    // character it does not have.
    if (show.isEmpty()) {
        st.charNote->setText(QStringLiteral(
            "Set by hand. Pick a character above to start from a known "
            "point — it will say what it is for, and you can adjust from "
            "there."));
    }
}

void StripWindow::refreshStagePictures()
{
    StripChain* ch = chain();
    if (m_gateCurve)     { m_gateCurve->setChain(ch); }
    if (m_compCurve)     { m_compCurve->setChain(ch); }
    if (m_limiterCurve)  { m_limiterCurve->setChain(ch); }
    if (m_tubeCurve)     { m_tubeCurve->setChain(ch); }
    if (m_deEssCurve)    { m_deEssCurve->setChain(ch); }
    if (m_puduCurve)     { m_puduCurve->setChain(ch); }
}


void StripWindow::refreshChainRow()
{
    if (m_chainView) {
        m_chainView->setChain(chain());
        m_chainView->update();
    }
    if (m_levels) { m_levels->setChain(chain()); }
    // Same late-binding trap as before, now one line: the host is what
    // the ported panel reaches through, so pointing it at the current
    // chain is enough for every widget inside it.
    if (m_eqHost) { m_eqHost->setChain(chain()); }
    if (m_eqPanel) { m_eqPanel->refreshFromEngine(); }
}

void StripWindow::refreshMeters()
{
    adoptChainIfArrived();

    StripChain* c = chain();
    if (!c) { return; }

    // The tiles are the primary reading; the text below each panel is
    // for someone who wants the number rather than the shape.
    if (m_levels) { m_levels->tick(); }
    // The ported equaliser panel drives its own analyser on its own
    // timer, as AetherSDR does; nothing here needs to pump it.

    // Each stage's picture, from that stage's own atomics. Only the
    // visible tab is worth repainting — the others are hidden and a
    // repaint of a hidden widget is work nobody sees.
    for (QWidget* w : {static_cast<QWidget*>(m_gateCurve),
                       static_cast<QWidget*>(m_compCurve),
                       static_cast<QWidget*>(m_limiterCurve),
                       static_cast<QWidget*>(m_tubeCurve),
                       static_cast<QWidget*>(m_deEssCurve),
                       static_cast<QWidget*>(m_puduCurve)}) {
        if (!w || !w->isVisible()) { continue; }
        if (auto* d = qobject_cast<StripDynamicsCurve*>(w)) { d->refresh(); }
        else if (auto* sh = qobject_cast<StripShaperCurve*>(w)) { sh->refresh(); }
        else if (auto* b = qobject_cast<StripBandCurve*>(w))  { b->refresh(); }
    }
    refreshStageText();
    if (m_chainView) {
        m_chainView->setReduction(StripChain::Stage::Gate,
                                  c->gate().gainReductionDb());
        m_chainView->setReduction(StripChain::Stage::Comp,
                                  c->comp().gainReductionDb());
        m_chainView->setReduction(StripChain::Stage::DeEss,
                                  c->deEss().gainReductionDb());
    }

    if (m_gateMeter) {
        m_gateMeter->setText(QStringLiteral(
            "Gate: %1 · %2 dB of attenuation · in %3 dBFS")
                .arg(c->gate().gateOpen() ? QStringLiteral("open")
                                          : QStringLiteral("shut"))
                .arg(c->gate().gainReductionDb(), 0, 'f', 1)
                .arg(c->gate().inputPeakDb(), 0, 'f', 1));
    }
    if (m_deEssMeter) {
        m_deEssMeter->setText(QStringLiteral(
            "De-esser: %1 dB off the sibilance · sidechain %2 dBFS")
                .arg(c->deEss().gainReductionDb(), 0, 'f', 1)
                .arg(c->deEss().sidechainPeakDb(), 0, 'f', 1));
    }
    if (m_compMeter) {
        m_compMeter->setText(QStringLiteral(
            "Compressor: %1 dB of reduction · in %2 dBFS · out %3 dBFS")
                .arg(c->comp().gainReductionDb(), 0, 'f', 1)
                .arg(c->comp().inputPeakDb(), 0, 'f', 1)
                .arg(c->comp().outputPeakDb(), 0, 'f', 1));
    }
}

} // namespace Longpath
