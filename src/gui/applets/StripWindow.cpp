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
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
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
             int decimals = 1)
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
                     [show, toValue, onChange](int pos) {
        show(pos);
        if (onChange) { onChange(toValue(pos)); }
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
    buildUi();
    refreshChainRow();
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

    // ── Chain row ────────────────────────────────────────────────────
    //
    // Read-only on purpose. It answers "what is running" at a glance
    // while a stage is being edited; a row that also accepts clicks is
    // a row people change by accident while reaching for a tab.
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            auto* tile = new QLabel(
                QString::fromLatin1(StripChain::stageName(
                    static_cast<StripChain::Stage>(i))), this);
            tile->setAlignment(Qt::AlignCenter);
            tile->setMinimumWidth(72);
            m_chainTiles[static_cast<size_t>(i)] = tile;
            row->addWidget(tile);
            if (i < StripChain::kStageCount - 1) {
                auto* arrow = new QLabel(QStringLiteral("▸"), this);
                arrow->setStyleSheet(dimStyle());
                row->addWidget(arrow);
            }
        }
        row->addStretch(1);
        col->addLayout(row);
    }

    // ── One tab per stage ────────────────────────────────────────────
    m_tabs = new QTabWidget(this);
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        const auto s = static_cast<StripChain::Stage>(i);
        QWidget* page = nullptr;
        switch (s) {
        case StripChain::Stage::Gate: page = buildGatePanel(); break;
        case StripChain::Stage::Comp: page = buildCompPanel(); break;
        default:                      page = buildPlaceholder(s); break;
        }
        m_tabs->addTab(page,
                       QString::fromLatin1(StripChain::stageName(s)));
    }
    col->addWidget(m_tabs, 1);

    auto* closeRow = new QHBoxLayout;
    closeRow->addStretch(1);
    auto* closeBtn = new QPushButton(QStringLiteral("Close"), this);
    closeBtn->setStyleSheet(Style::buttonBaseStyle());
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    closeRow->addWidget(closeBtn);
    col->addLayout(closeRow);

    connect(m_master, &QCheckBox::toggled, this, [this](bool on) {
        if (StripChain* c = chain()) { c->setEnabled(on); }
        refreshChainRow();
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
    });

    auto* mode = new QComboBox(page);
    mode->addItem(QStringLiteral("Expander — gentle, keeps the room"),
                  int(ClientGate::Mode::Expander));
    mode->addItem(QStringLiteral("Gate — hard, removes it"),
                  int(ClientGate::Mode::Gate));
    form->addRow(QStringLiteral("Character"), mode);

    addKnob(form, QStringLiteral("Threshold"),
        -80.0, 0.0, 1.0, -40.0, QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setThresholdDb(float(v)); }
        }, 0);
    addKnob(form, QStringLiteral("Attack"), 0.1, 100.0, 0.1, 0.5,
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setAttackMs(float(v)); }
        });
    addKnob(form, QStringLiteral("Hold"), 0.0, 500.0, 5.0, 20.0,
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setHoldMs(float(v)); }
        }, 0);
    addKnob(form, QStringLiteral("Release"), 5.0, 2000.0, 5.0, 100.0,
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setReleaseMs(float(v)); }
        }, 0);
    addKnob(form, QStringLiteral("Depth"), -80.0, 0.0, 1.0, -15.0,
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setFloorDb(float(v)); }
        }, 0);
    addKnob(form, QStringLiteral("Hysteresis"), 0.0, 20.0, 0.5, 2.0,
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setReturnDb(float(v)); }
        });

    // Mode snaps ratio and depth, so the depth slider has to follow or
    // it will show one number while the gate uses another.
    connect(mode, &QComboBox::currentIndexChanged, this, [this, mode](int) {
        if (StripChain* c = chain()) {
            c->gate().setMode(
                static_cast<ClientGate::Mode>(mode->currentData().toInt()));
        }
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
    });

    addKnob(form, QStringLiteral("Threshold"), -60.0, 0.0, 1.0, -20.0,
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setThresholdDb(float(v)); }
        }, 0);
    addKnob(form, QStringLiteral("Ratio"), 1.0, 20.0, 0.5, 3.0,
        QStringLiteral(": 1"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setRatio(float(v)); }
        });
    addKnob(form, QStringLiteral("Attack"), 0.1, 100.0, 0.1, 5.0,
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setAttackMs(float(v)); }
        });
    addKnob(form, QStringLiteral("Release"), 5.0, 2000.0, 5.0, 120.0,
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setReleaseMs(float(v)); }
        }, 0);
    addKnob(form, QStringLiteral("Knee"), 0.0, 24.0, 0.5, 6.0,
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setKneeDb(float(v)); }
        });
    addKnob(form, QStringLiteral("Make-up"), 0.0, 24.0, 0.5, 0.0,
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setMakeupDb(float(v)); }
        });

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

void StripWindow::refreshChainRow()
{
    StripChain* c = chain();
    const bool master = c && c->isEnabled();

    for (int i = 0; i < StripChain::kStageCount; ++i) {
        QLabel* tile = m_chainTiles[static_cast<size_t>(i)];
        if (!tile) { continue; }
        const bool on = c
            && c->stageEnabled(static_cast<StripChain::Stage>(i));
        // Three states, not two: a stage can be switched on and still
        // be doing nothing because the master is off, and showing that
        // as "on" is how an operator concludes the strip is broken.
        const QString bg  = (on && master)
            ? QString::fromLatin1(Style::kButtonBg)
            : QString::fromLatin1(Style::kInsetBg);
        const QString fg  = !on
            ? QString::fromLatin1(Style::kTextInactive)
            : (master ? QString::fromLatin1(Style::kAccent)
                      : QString::fromLatin1(Style::kTextSecondary));
        tile->setStyleSheet(
            QStringLiteral("QLabel { background: %1; color: %2; border: "
                           "1px solid %3; padding: 3px; font-size: 10px; }")
                .arg(bg, fg, QString::fromLatin1(Style::kInsetBorder)));
    }
}

void StripWindow::refreshMeters()
{
    StripChain* c = chain();
    if (!c) { return; }

    if (m_gateMeter) {
        m_gateMeter->setText(QStringLiteral(
            "Gate: %1 · %2 dB of attenuation · in %3 dBFS")
                .arg(c->gate().gateOpen() ? QStringLiteral("open")
                                          : QStringLiteral("shut"))
                .arg(c->gate().gainReductionDb(), 0, 'f', 1)
                .arg(c->gate().inputPeakDb(), 0, 'f', 1));
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
