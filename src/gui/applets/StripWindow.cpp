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
#include "core/strip/StripTargets.h"

#include <QInputDialog>
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

#include <cmath>
#include <functional>

namespace NereusSDR {

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
    setWindowTitle(QStringLiteral("Aetherial Audio Channel Strip"));
    setModal(false);
    // Restore before the panels are built, so every control opens
    // showing what the chain actually holds rather than a default it
    // would then write back over the top.
    if (StripChain* c = chain()) {
        StripSettings::restore(*c);
        seedEqLayout();
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
    seedEqLayout();
    reloadControls();
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
    // Put the monitor back. Someone who opens this window to listen
    // while shaping a curve and then closes it should not be left with
    // a monitor they did not ask for and can no longer see.
    if (m_restoreMonitor) { setSelfMonitor(m_monitorWasOn); }
    // Belt and braces: setSelfMonitor(false) restores it, but if the
    // window is destroyed while listening with m_restoreMonitor unset
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

void StripWindow::setSelfMonitor(bool on)
{
    if (!m_radio) { return; }
    TxChannel* tx = m_radio->txChannel();
    if (!tx) { return; }

    // The radio's own chain steps aside while you listen, and comes
    // back when you stop. Without this you are hearing the strip and
    // WDSP in series and can judge neither.
    setRadioBypass(on);

    if (on && !m_restoreMonitor) {
        m_monitorWasOn = m_radio->transmitModel().monEnabled();
        m_restoreMonitor = true;
    }
    // Two switches, both needed: one runs the transmit chain off air,
    // the other mixes its output into the speakers. Neither writes to
    // the radio — TxChannel::writesToRadio() is gated on transmitting
    // alone, and tst_tx_offair_monitor holds it there.
    tx->setOffAirMonitor(on);
    m_radio->transmitModel().setMonEnabled(on);
    // And silence the band, or you are listening to your voice and the
    // noise floor at once and can judge neither.
    if (AudioEngine* ae = m_radio->audioEngine()) {
        ae->setRxMutedForMonitor(on);
    }
}

void StripWindow::persist()
{
    if (StripChain* c = chain()) { StripSettings::save(*c); }
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

        m_listen = new QCheckBox(QStringLiteral("Hear myself"), this);
        m_listen->setToolTip(QStringLiteral(
            "Run the transmit chain and listen to the result without "
            "going on the air. Nothing is transmitted."));
        row->addWidget(m_listen);
        connect(m_listen, &QCheckBox::toggled,
                this, &StripWindow::setSelfMonitor);

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
    connect(on, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) {
            c->setStageEnabled(StripChain::Stage::Gate, v);
        }
        refreshChainRow();
        persist();
    });

    auto* mode = new QComboBox(page);
    mode->addItem(QStringLiteral("Expander — gentle, keeps the room"),
                  int(ClientGate::Mode::Expander));
    mode->addItem(QStringLiteral("Gate — hard, removes it"),
                  int(ClientGate::Mode::Gate));
    {
        const int have = chain() ? int(chain()->gate().mode())
                                 : int(ClientGate::Mode::Expander);
        const int at = mode->findData(have);
        if (at >= 0) { mode->setCurrentIndex(at); }
    }
    form->addRow(QStringLiteral("Character"), mode);

    addKnob(form, QStringLiteral("Threshold"),
        -80.0, 0.0, 1.0, cur([this]{ return chain()->gate().thresholdDb(); }, -40.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setThresholdDb(float(v)); }
        }, 0);
    addKnob(form, QStringLiteral("Attack"), 0.1, 100.0, 0.1, cur([this]{ return chain()->gate().attackMs(); }, 0.5),
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setAttackMs(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Hold"), 0.0, 500.0, cur([this]{ return chain()->comp().attackMs(); }, 5.0), cur([this]{ return chain()->gate().holdMs(); }, 20.0),
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setHoldMs(float(v)); }
        }, 0);
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

    // Mode snaps ratio and depth, so the depth slider has to follow or
    // it will show one number while the gate uses another.
    connect(mode, &QComboBox::currentIndexChanged, this, [this, mode](int) {
        if (StripChain* c = chain()) {
            c->gate().setMode(
                static_cast<ClientGate::Mode>(mode->currentData().toInt()));
        }
        persist();
    });

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

void StripWindow::seedEqLayout()
{
    StripChain* c = chain();
    if (!c) { return; }
    // Only seed a chain that has not been set up before. Overwriting a
    // restored layout would throw away the operator's settings every
    // time the window opened.
    if (c->eq().activeBandCount() >= kEqBandCount) { return; }

    auto put = [c](int idx, ClientEq::FilterType t, float hz, float gain,
                   float q, bool on, int slope) {
        ClientEq::BandParams p;
        p.type = t; p.freqHz = hz; p.gainDb = gain; p.q = q;
        p.enabled = on; p.slopeDbPerOct = slope;
        c->eq().setBand(idx, p);
    };

    put(0, ClientEq::FilterType::HighPass, 100.0f, 0.0f, 0.707f, true, 24);
    // Notches off until asked for: a notch at 50 Hz on a station that
    // runs at 60 removes nothing and costs phase.
    put(1, ClientEq::FilterType::Peak,  50.0f, -18.0f, 8.0f, false, 12);
    put(2, ClientEq::FilterType::Peak, 100.0f, -12.0f, 8.0f, false, 12);
    put(3, ClientEq::FilterType::Peak, 150.0f,  -9.0f, 8.0f, false, 12);

    // Six shaping bands rather than three, spread over the range a
    // voice actually occupies and log-spaced so each one covers about
    // the same musical distance. Three points can only tilt a curve;
    // six can put a dip where the problem is, which is what an operator
    // looking at their own spectrum wants to do.
    //
    // A shelf at each end and peaks between them: shelves are the right
    // shape for "everything below this" and peaks for "this bit here",
    // and offering a peak at 200 Hz where a shelf is wanted is how a
    // curve ends up with two bands fighting each other.
    put(4, ClientEq::FilterType::LowShelf,   180.0f, 0.0f, 0.707f, true, 12);
    put(5, ClientEq::FilterType::Peak,       350.0f, 0.0f, 1.0f,   true, 12);
    put(6, ClientEq::FilterType::Peak,       700.0f, 0.0f, 1.0f,   true, 12);
    put(7, ClientEq::FilterType::Peak,      1400.0f, 0.0f, 1.0f,   true, 12);
    put(8, ClientEq::FilterType::Peak,      2400.0f, 0.0f, 1.0f,   true, 12);
    put(9, ClientEq::FilterType::HighShelf, 3400.0f, 0.0f, 0.707f, true, 12);
    c->eq().setActiveBandCount(kEqBandCount);
}

void StripWindow::buildEqTable(QWidget* parent, QVBoxLayout* into)
{
    m_eqTable = new QTableWidget(0, 5, parent);
    m_eqTable->setHorizontalHeaderLabels({
        QStringLiteral("#"), QStringLiteral("Hz"), QStringLiteral("dB"),
        QStringLiteral("Q"), QStringLiteral("Shape")});
    m_eqTable->verticalHeader()->setVisible(false);
    m_eqTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_eqTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_eqTable->setMaximumHeight(190);
    m_eqTable->setStyleSheet(
        QStringLiteral("QTableWidget { font-size: 11px; }"));
    into->addWidget(m_eqTable);

    connect(m_eqTable, &QTableWidget::cellChanged,
            this, &StripWindow::onEqTableEdited);

    auto* note = new QLabel(QStringLiteral(
        "Type a number when you know it, drag the dot when you don't. "
        "Both are the same band — there is one setting, seen twice."),
        parent);
    note->setWordWrap(true);
    note->setStyleSheet(dimStyle());
    into->addWidget(note);

    refreshEqTable();
}

void StripWindow::refreshEqTable()
{
    if (!m_eqTable) { return; }
    StripChain* c = chain();

    // Guarded, because filling the table emits cellChanged for every
    // cell, and each of those would be read back as an edit. Without
    // this the table writes its own contents into the chain on every
    // refresh — harmless while the values agree and quietly destructive
    // the moment rounding makes them differ.
    m_fillingTable = true;

    std::vector<int> rows;
    if (c) {
        const int n = c->eq().activeBandCount();
        if (n > 0) { rows.push_back(0); }
        for (int b = kBandLowShelf; b < n; ++b) { rows.push_back(b); }
    }
    m_eqTable->setRowCount(int(rows.size()));

    for (int r = 0; r < int(rows.size()); ++r) {
        const int band = rows[static_cast<size_t>(r)];
        const ClientEq::BandParams p = c->eq().band(band);

        auto put = [this, r](int col, const QString& text, bool editable) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignCenter);
            if (!editable) {
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
            m_eqTable->setItem(r, col, item);
        };
        put(0, QString::number(r + 1), false);
        put(1, QString::number(p.freqHz, 'f', 0), true);

        const bool isHp = p.type == ClientEq::FilterType::HighPass;
        // A high-pass has no gain and its width is a slope from a list
        // of four, so those two cells show what it has rather than a
        // number that would do nothing if typed into.
        put(2, isHp ? QStringLiteral("—")
                    : QString::number(p.gainDb, 'f', 1), !isHp);
        put(3, isHp ? QStringLiteral("%1 dB/oct").arg(p.slopeDbPerOct)
                    : QString::number(p.q, 'f', 2), !isHp);

        QString shape;
        switch (p.type) {
        case ClientEq::FilterType::HighPass:  shape = QStringLiteral("high-pass"); break;
        case ClientEq::FilterType::LowPass:   shape = QStringLiteral("low-pass");  break;
        case ClientEq::FilterType::LowShelf:  shape = QStringLiteral("low shelf"); break;
        case ClientEq::FilterType::HighShelf: shape = QStringLiteral("high shelf");break;
        case ClientEq::FilterType::Peak:      shape = QStringLiteral("peak");      break;
        }
        put(4, shape, false);

        // Row index carries the band, because the table only shows the
        // draggable ones and the two numberings would otherwise drift
        // apart the first time a band is added.
        m_eqTable->item(r, 0)->setData(Qt::UserRole, band);
    }
    m_fillingTable = false;
}

void StripWindow::onEqTableEdited(int row, int column)
{
    if (m_fillingTable || !m_eqTable) { return; }
    StripChain* c = chain();
    if (!c) { return; }
    QTableWidgetItem* idx = m_eqTable->item(row, 0);
    QTableWidgetItem* cell = m_eqTable->item(row, column);
    if (!idx || !cell) { return; }

    const int band = idx->data(Qt::UserRole).toInt();
    ClientEq::BandParams p = c->eq().band(band);

    bool ok = false;
    const double v = cell->text().trimmed().toDouble(&ok);
    if (!ok) {
        // Put the old value back rather than leaving the operator's
        // typo on screen looking like a setting.
        refreshEqTable();
        return;
    }

    switch (column) {
    case 1: p.freqHz = float(std::clamp(v, 20.0, 16000.0)); break;
    case 2: p.gainDb = float(std::clamp(v, -24.0, 24.0));   break;
    case 3: p.q      = float(std::clamp(v, 0.3, 12.0));     break;
    default: return;
    }
    p.enabled = true;
    c->eq().setBand(band, p);
    if (m_eqCurve) { m_eqCurve->refresh(); }
    refreshEqTable();     // shows the clamp, if one happened
    persist();
}

void StripWindow::applyHumNotches(int baseHz, bool on)
{
    StripChain* c = chain();
    if (!c) { return; }
    // Three of them, because mains hum is never just the fundamental —
    // a transformer produces the harmonics too, and removing only 50 Hz
    // leaves 100 and 150 sitting in the middle of the low end.
    for (int h = 1; h <= 3; ++h) {
        ClientEq::BandParams p = c->eq().band(h);
        p.freqHz  = float(baseHz * h);
        p.enabled = on;
        c->eq().setBand(h, p);
    }
    if (m_eqCurve) { m_eqCurve->refresh(); }
    persist();
}

QWidget* StripWindow::buildEqPanel()
{
    auto* page = new QWidget(this);
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);

    // The curve first, and large. This is the whole reason the EQ tab
    // is worth opening: the high-pass corner and the mains notches stop
    // being three numbers and become a shape you can aim.
    // ── What are we shaping for? ─────────────────────────────────────
    //
    // Asked before shaping rather than discovered afterwards. A contest
    // signal and a ragchew signal want opposite things, and the
    // transmit bandwidth changes the answer again — aiming a 2.7 kHz
    // channel at a 3.3 kHz shape wastes the wider one and overdrives
    // the narrower.
    {
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel(QStringLiteral("Shaping for:"), page));
        m_profileBox = new QComboBox(page);
        for (const auto& pr : StripTargets::profiles()) {
            m_profileBox->addItem(pr.name, pr.name);
        }
        m_profileBox->setMinimumWidth(150);
        row->addWidget(m_profileBox);

        // ── Three ways to get the target you want ────────────────────
        //
        // The bench's verdict on all five built-ins was "not what I
        // want", which is the correct verdict on somebody else's
        // opinion about your voice. So: copy the nearest one and adjust
        // it, take your own current sound as the target, or start from
        // flat. All three write the same twelve points, and the rose
        // line grows handles once "Mine" is chosen.
        auto* copyBtn = new QPushButton(QStringLiteral("Copy to Mine"), page);
        copyBtn->setStyleSheet(Style::buttonBaseStyle());
        copyBtn->setToolTip(QStringLiteral(
            "Take the profile shown and make it your own starting point. "
            "Nothing is lost — the built-ins are still there."));
        row->addWidget(copyBtn);

        auto* fromVoiceBtn = new QPushButton(
            QStringLiteral("My voice → Mine"), page);
        fromVoiceBtn->setStyleSheet(Style::buttonBaseStyle());
        fromVoiceBtn->setToolTip(QStringLiteral(
            "Use the fifteen-second average as the target. The rose line "
            "then sits flat, and every change you make afterwards is "
            "measured against how you sound today rather than against "
            "somebody's idea of how a voice should sound."));
        row->addWidget(fromVoiceBtn);

        auto* flatBtn = new QPushButton(QStringLiteral("Flat"), page);
        flatBtn->setStyleSheet(Style::buttonBaseStyle());
        flatBtn->setToolTip(QStringLiteral(
            "Clear your curve. A flat target means the equaliser is "
            "asked to leave the voice alone."));
        row->addWidget(flatBtn);

        row->addStretch(1);
        outer->addLayout(row);

        connect(copyBtn, &QPushButton::clicked, this, [this]() {
            StripTargets::seedUserTargetFrom(
                m_profileBox->currentData().toString());
            const int at = m_profileBox->findData(
                QString::fromLatin1(StripTargets::kUserProfileName));
            if (at >= 0) { m_profileBox->setCurrentIndex(at); }
            if (m_eqCurve) { m_eqCurve->refresh(); }
        });
        connect(flatBtn, &QPushButton::clicked, this, [this]() {
            StripTargets::setUserTarget(
                QVector<double>(StripTargets::kUserPointCount, 0.0));
            const int at = m_profileBox->findData(
                QString::fromLatin1(StripTargets::kUserProfileName));
            if (at >= 0) { m_profileBox->setCurrentIndex(at); }
            if (m_eqCurve) { m_eqCurve->refresh(); }
        });
        connect(fromVoiceBtn, &QPushButton::clicked, this, [this]() {
            if (!m_eqCurve) { return; }
            const QVector<double> mine = m_eqCurve->measuredAtTargetPoints();
            if (mine.isEmpty()) {
                QMessageBox::information(this, QStringLiteral("Target"),
                    QStringLiteral("Speak for a few seconds first — there "
                                   "is no measured curve to copy yet."));
                return;
            }
            StripTargets::setUserTarget(mine);
            const int at = m_profileBox->findData(
                QString::fromLatin1(StripTargets::kUserProfileName));
            if (at >= 0) { m_profileBox->setCurrentIndex(at); }
            m_eqCurve->refresh();
        });

        m_profileNote = new QLabel(page);
        m_profileNote->setWordWrap(true);
        m_profileNote->setStyleSheet(dimStyle());
        outer->addWidget(m_profileNote);

        auto describe = [this]() {
            const QString n = m_profileBox->currentData().toString();
            for (const auto& pr : StripTargets::profiles()) {
                if (pr.name == n) { m_profileNote->setText(pr.description); }
            }
            if (m_eqCurve) { m_eqCurve->setProfile(n); }
        };
        connect(m_profileBox, &QComboBox::currentIndexChanged, this,
                [describe](int) { describe(); });
        // Called after the curve exists, below.
        QTimer::singleShot(0, this, describe);
    }

    m_eqCurve = new StripEqCurve(page);
    m_eqCurve->setChain(chain());
    if (StripChain* ch = chain()) { m_eqCurve->setSpectrum(&ch->micSpectrum()); }
    outer->addWidget(m_eqCurve);

    // Dragging the curve writes straight into the chain; the window
    // decides when that reaches the settings file, so there is one
    // place that saves rather than one per control.
    connect(m_eqCurve, &StripEqCurve::bandChanged, this, [this](int band) {
        // -1 means the target moved, not a band. It is already stored in
        // its own settings key, so there is nothing to persist and no
        // table row to refresh.
        if (band >= 0) { refreshEqTable(); persist(); }
    });

    {
        auto* row = new QHBoxLayout;
        m_holdBtn = new QPushButton(QStringLiteral("Hold"), page);
        m_holdBtn->setCheckable(true);
        m_holdBtn->setStyleSheet(Style::buttonBaseStyle());
        m_holdBtn->setToolTip(QStringLiteral(
            "Freeze the voice behind the curve. You cannot aim an "
            "equaliser at a shape that is moving — speak a sentence, "
            "press Hold, then drag the dots onto what you froze."));
        row->addWidget(m_holdBtn);

        auto* smooth = new QCheckBox(QStringLiteral("Smooth"), page);
        smooth->setChecked(true);
        smooth->setToolTip(QStringLiteral(
            "Average the held curve to a third of an octave. The raw "
            "spectrum of a voice is a comb of harmonics that move with "
            "every note you speak on; the smoothed one is the shape "
            "underneath, which is what an equaliser can actually act "
            "on."));
        row->addWidget(smooth);
        connect(smooth, &QCheckBox::toggled, this, [this](bool on) {
            if (m_eqCurve) { m_eqCurve->setSmoothing(on); }
        });

        auto* target = new QCheckBox(QStringLiteral("Target"), page);
        target->setChecked(true);
        target->setToolTip(QStringLiteral(
            "Draw where a voice that carries would sit, in rose, over "
            "what you froze. Lift the amber onto the rose and the blue "
            "curve is what you move to do it."));
        row->addWidget(target);
        connect(target, &QCheckBox::toggled, this, [this](bool on) {
            if (m_eqCurve) { m_eqCurve->setShowTarget(on); }
        });

        auto* hint = new QLabel(QStringLiteral(
            "Drag a dot for frequency and gain · wheel for width · "
            "double-click a dot to change its shape · double-click "
            "empty space to add one · right-click the last one to "
            "remove it."), page);
        hint->setWordWrap(true);
        hint->setStyleSheet(dimStyle());
        row->addWidget(hint, 1);
        outer->addLayout(row);

        m_tips = new QLabel(page);
        m_tips->setWordWrap(true);
        m_tips->setStyleSheet(dimStyle());
        outer->addWidget(m_tips);

        // Five lines, in order, because an operator opening this for the
        // first time has no way to know that the amber curve is not
        // something they set, or that the rose one is where the blue one
        // goes rather than where the voice goes.
        auto* guide = new QLabel(QStringLiteral(
            "<b>How to use this</b><br>"
            "1 &nbsp;Tick <i>Hear myself</i>. The radio's own EQ, leveler "
            "and CFC step aside while you listen, and come back when you "
            "stop.<br>"
            "2 &nbsp;Choose what you are shaping for, above.<br>"
            "3 &nbsp;Talk normally for a quarter of a minute. The amber "
            "curve is your own voice, averaged over the last 15 seconds — "
            "it builds itself, you do not have to press anything.<br>"
            "4 &nbsp;Press <i>Hold</i> to stop it moving.<br>"
            "5 &nbsp;Drag the blue dots onto the rose line. Rose is not "
            "your target voice — it is where the equaliser should sit to "
            "get you there, so when blue lies on rose you are done.<br>"
            "<br>The receiver is silenced while you listen, and comes "
            "back when you stop."), page);
        guide->setWordWrap(true);
        guide->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-size: 11px; "
                           "background: %2; border: 1px solid %3; "
                           "padding: 8px; }")
                .arg(QString::fromLatin1(Style::kTextSecondary),
                     QString::fromLatin1(Style::kInsetBg),
                     QString::fromLatin1(Style::kInsetBorder)));
        outer->addWidget(guide);

        connect(m_holdBtn, &QPushButton::toggled, this, [this](bool on) {
            if (!m_eqCurve) { return; }
            m_eqCurve->setHeld(on);
            if (!on) { m_tips->clear(); return; }

            const QStringList t = m_eqCurve->tips();
            if (t.isEmpty()) {
                m_tips->setText(QStringLiteral(
                    "Nothing stands out — what you froze is already close "
                    "to a shape that carries. Either speak a little longer "
                    "before holding, or leave it alone."));
                return;
            }
            QString text;
            for (int i = 0; i < t.size(); ++i) {
                text += QStringLiteral("%1. %2\n").arg(i + 1).arg(t.at(i));
            }
            m_tips->setText(text.trimmed());
        });
    }

    buildEqTable(page, outer);

    auto* body = new QWidget(page);
    outer->addWidget(body, 1);
    auto* form = new QFormLayout(body);

    const int idx = static_cast<int>(StripChain::Stage::Eq);
    auto* on = new QCheckBox(QStringLiteral("EQ on"), page);
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    form->addRow(on);
    connect(on, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) {
            c->setStageEnabled(StripChain::Stage::Eq, v);
        }
        refreshChainRow();
        persist();
    });

    auto setBandField = [this](int band, auto member, double v) {
        if (StripChain* c = chain()) {
            ClientEq::BandParams p = c->eq().band(band);
            p.*member = decltype(p.*member)(v);
            c->eq().setBand(band, p);
        }
    };

    // ── High-pass ────────────────────────────────────────────────────
    addKnob(form, QStringLiteral("High-pass"), 20.0, 400.0, 5.0, cur([this]{ return chain()->eq().band(0).freqHz; }, 100.0),
        QStringLiteral("Hz"), [setBandField](double v) {
            setBandField(0, &ClientEq::BandParams::freqHz, v);
        }, 0, [this]{ persist(); });

    auto* slope = new QComboBox(page);
    for (int db : {12, 24, 36, 48}) {
        slope->addItem(QStringLiteral("%1 dB/octave").arg(db), db);
    }
    // From the chain, not a hardcoded index. Same class of bug as the
    // sliders had: the panel would show 48 dB/octave while the filter
    // ran at 24, and the first touch of the combo would then make the
    // filter agree with the wrong label.
    {
        const int have = chain() ? chain()->eq().band(0).slopeDbPerOct : 24;
        const int at = slope->findData(have);
        slope->setCurrentIndex(at >= 0 ? at : 1);
    }
    form->addRow(QStringLiteral("High-pass slope"), slope);
    connect(slope, &QComboBox::currentIndexChanged, this,
            [this, slope, setBandField](int) {
        setBandField(0, &ClientEq::BandParams::slopeDbPerOct,
                     slope->currentData().toInt());
        persist();
    });

    auto* hpHelp = new QLabel(QStringLiteral(
        "Nothing below about 100 Hz survives an SSB transmitter, but it "
        "does reach the compressor first and eat its headroom. Cutting it "
        "here makes everything above it louder without touching a gain "
        "control."), page);
    hpHelp->setWordWrap(true);
    hpHelp->setStyleSheet(dimStyle());
    form->addRow(hpHelp);

    // ── Mains hum ────────────────────────────────────────────────────
    {
        auto* row = new QWidget(page);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        m_humBox = new QCheckBox(QStringLiteral("Remove mains hum"), row);
        h->addWidget(m_humBox);
        m_humBase = new QComboBox(row);
        m_humBase->addItem(QStringLiteral("50 Hz"), 50);
        m_humBase->addItem(QStringLiteral("60 Hz"), 60);
        h->addWidget(m_humBase);
        h->addStretch(1);
        form->addRow(row);

        if (StripChain* ch = chain()) {
            const ClientEq::BandParams p = ch->eq().band(1);
            const QSignalBlocker b1(m_humBox);
            const QSignalBlocker b2(m_humBase);
            m_humBox->setChecked(p.enabled);
            const int at = m_humBase->findData(
                int(std::lround(double(p.freqHz))) >= 55 ? 60 : 50);
            if (at >= 0) { m_humBase->setCurrentIndex(at); }
        }

        auto apply = [this]() {
            applyHumNotches(m_humBase->currentData().toInt(),
                            m_humBox->isChecked());
        };
        connect(m_humBox, &QCheckBox::toggled, this, [apply](bool) { apply(); });
        connect(m_humBase, &QComboBox::currentIndexChanged, this,
                [apply](int) { apply(); });

        auto* humHelp = new QLabel(QStringLiteral(
            "Three narrow notches, at the mains frequency and its first "
            "two harmonics — hum from a transformer is never just the "
            "fundamental, and removing only the first leaves the other "
            "two sitting in the low end.\n\n"
            "This is a repair, not a cure. If the voice check reports hum "
            "less than about 20 dB below your speech, the cause is a "
            "ground loop or a power supply, and the notches only hide it. "
            "Find it if you can; use these if you can't."), page);
        humHelp->setWordWrap(true);
        humHelp->setStyleSheet(dimStyle());
        form->addRow(humHelp);
    }

    // ── Tone ─────────────────────────────────────────────────────────
    addKnob(form, QStringLiteral("Low"), -12.0, 12.0, 0.5, cur([this]{ return chain()->eq().band(kBandLowShelf).gainDb; }, 0.0),
        QStringLiteral("dB"), [setBandField](double v) {
            setBandField(kBandLowShelf, &ClientEq::BandParams::gainDb, v);
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Presence"), -12.0, 12.0, 0.5, cur([this]{ return chain()->eq().band(kBandPresence).gainDb; }, 0.0),
        QStringLiteral("dB"), [setBandField](double v) {
            setBandField(kBandPresence, &ClientEq::BandParams::gainDb, v);
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("High"), -12.0, 12.0, 0.5, cur([this]{ return chain()->eq().band(kBandHighShelf).gainDb; }, 0.0),
        QStringLiteral("dB"), [setBandField](double v) {
            setBandField(kBandHighShelf, &ClientEq::BandParams::gainDb, v);
        }, 1, [this]{ persist(); });

    auto* toneHelp = new QLabel(QStringLiteral(
        "Presence sits at 2 kHz — the band that decides whether a signal "
        "is understood rather than whether it sounds pleasant. A little "
        "here is worth a lot anywhere else."), page);
    toneHelp->setWordWrap(true);
    toneHelp->setStyleSheet(dimStyle());
    form->addRow(toneHelp);

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
        }, 0);
    addKnob(form, QStringLiteral("Ratio"), 1.0, 20.0, 0.5, cur([this]{ return chain()->comp().ratio(); }, 3.0),
        QStringLiteral(": 1"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setRatio(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Attack"), 0.1, 100.0, 0.1, 5.0,
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setAttackMs(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Release"), 5.0, 2000.0, 5.0, cur([this]{ return chain()->comp().releaseMs(); }, 120.0),
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setReleaseMs(float(v)); }
        }, 0);
    addKnob(form, QStringLiteral("Knee"), 0.0, 24.0, 0.5, cur([this]{ return chain()->comp().kneeDb(); }, 6.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setKneeDb(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Make-up"), cur([this]{ return chain()->comp().makeupDb(); }, 0.0), 24.0, 0.5, 0.0,
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setMakeupDb(float(v)); }
        }, 1, [this]{ persist(); });

    // The phase rotator earns its own row and its own sentence: it is
    // the one control here that raises average power without making
    // anything louder, and nobody guesses that from the name.
    auto* rot = new QSpinBox(page);
    rot->setRange(0, 8);
    rot->setValue(0);
    form->addRow(QStringLiteral("Phase rotator"), rot);
    connect(rot, &QSpinBox::valueChanged, this, [this](int v) {
        if (StripChain* c = chain()) { c->comp().setPhaseRotatorStages(v); }
    });

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
    form->addRow(QStringLiteral("Character"), model);
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
    while (m_tabs->count() > 0) {
        QWidget* w = m_tabs->widget(0);
        m_tabs->removeTab(0);
        w->deleteLater();
    }
    m_eqCurve = nullptr;          // owned by the page just removed
    m_eqTable = nullptr;
    m_holdBtn = nullptr;
    m_tips    = nullptr;
    m_gateMeter = m_deEssMeter = m_compMeter = nullptr;
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

void StripWindow::refreshChainRow()
{
    if (m_chainView) {
        m_chainView->setChain(chain());
        m_chainView->update();
    }
    if (m_levels) { m_levels->setChain(chain()); }
    if (m_eqCurve) {
        // Re-bound here, not only at construction. Without this the
        // curve keeps whatever it was given when the tab was built —
        // which, for a window opened before the radio, is nothing.
        m_eqCurve->setChain(chain());
        if (StripChain* ch = chain()) {
            m_eqCurve->setSpectrum(&ch->micSpectrum());
        }
    }
    if (m_eqCurve) { m_eqCurve->refresh(); }
}

void StripWindow::refreshMeters()
{
    adoptChainIfArrived();

    StripChain* c = chain();
    if (!c) { return; }

    // The tiles are the primary reading; the text below each panel is
    // for someone who wants the number rather than the shape.
    if (m_levels) { m_levels->tick(); }
    if (m_eqCurve) { m_eqCurve->tick(); }
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

} // namespace NereusSDR
