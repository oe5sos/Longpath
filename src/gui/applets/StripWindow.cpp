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

#include <QMessageBox>
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSignalBlocker>
#include <QSpinBox>
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
    }
    buildUi();
    refreshChainRow();
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
        row->addWidget(new QLabel(QStringLiteral("Start from:"), this));
        m_presetBox = new QComboBox(this);
        m_presetBox->addItem(QStringLiteral("— choose —"), QString());
        for (const auto& p : StripSettings::builtInPresets()) {
            m_presetBox->addItem(p.name, p.name);
        }
        row->addWidget(m_presetBox);
        row->addStretch(1);
        col->addLayout(row);

        m_presetNote = new QLabel(this);
        m_presetNote->setWordWrap(true);
        m_presetNote->setStyleSheet(dimStyle());
        col->addWidget(m_presetNote);

        connect(m_presetBox, &QComboBox::currentIndexChanged, this,
                [this](int) {
            const QString n = m_presetBox->currentData().toString();
            if (!n.isEmpty()) { applyPreset(n); }
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
        case StripChain::Stage::Comp:  page = buildCompPanel();  break;
        default:                       page = buildPlaceholder(s); break;
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
    if (c->eq().activeBandCount() >= 7) { return; }

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
    put(4, ClientEq::FilterType::LowShelf,   200.0f, 0.0f, 0.707f, true, 12);
    put(5, ClientEq::FilterType::Peak,      2000.0f, 0.0f, 1.0f,   true, 12);
    put(6, ClientEq::FilterType::HighShelf, 3000.0f, 0.0f, 0.707f, true, 12);
    c->eq().setActiveBandCount(7);
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
    m_eqCurve = new StripEqCurve(page);
    m_eqCurve->setChain(chain());
    outer->addWidget(m_eqCurve);

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
    slope->setCurrentIndex(1);
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
    addKnob(form, QStringLiteral("Low"), -12.0, 12.0, 0.5, cur([this]{ return chain()->eq().band(4).gainDb; }, 0.0),
        QStringLiteral("dB"), [setBandField](double v) {
            setBandField(4, &ClientEq::BandParams::gainDb, v);
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Presence"), -12.0, 12.0, 0.5, cur([this]{ return chain()->eq().band(5).gainDb; }, 0.0),
        QStringLiteral("dB"), [setBandField](double v) {
            setBandField(5, &ClientEq::BandParams::gainDb, v);
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("High"), -12.0, 12.0, 0.5, cur([this]{ return chain()->eq().band(6).gainDb; }, 0.0),
        QStringLiteral("dB"), [setBandField](double v) {
            setBandField(6, &ClientEq::BandParams::gainDb, v);
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

void StripWindow::applyPreset(const QString& name)
{
    StripChain* c = chain();
    if (!c) { return; }
    if (!StripSettings::applyBuiltIn(name, *c)) { return; }

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
    m_gateMeter = m_deEssMeter = m_compMeter = nullptr;
    m_stageBoxes.fill(nullptr);

    for (int i = 0; i < StripChain::kStageCount; ++i) {
        const auto s = static_cast<StripChain::Stage>(i);
        QWidget* page = nullptr;
        switch (s) {
        case StripChain::Stage::Gate:  page = buildGatePanel();  break;
        case StripChain::Stage::Eq:    page = buildEqPanel();    break;
        case StripChain::Stage::DeEss: page = buildDeEssPanel(); break;
        case StripChain::Stage::Comp:  page = buildCompPanel();  break;
        default:                       page = buildPlaceholder(s); break;
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
    if (m_eqCurve) { m_eqCurve->refresh(); }
}

void StripWindow::refreshMeters()
{
    StripChain* c = chain();
    if (!c) { return; }

    // The tiles are the primary reading; the text below each panel is
    // for someone who wants the number rather than the shape.
    if (m_levels) { m_levels->tick(); }
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
