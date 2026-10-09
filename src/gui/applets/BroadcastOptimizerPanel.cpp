// =================================================================
// src/gui/applets/BroadcastOptimizerPanel.cpp  (Longpath)
// =================================================================
//
// Longpath-original. See BroadcastOptimizerPanel.h for why this is a
// panel of its own and which house rules its shape comes from.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/applets/BroadcastOptimizerPanel.h"

#include "gui/StyleConstants.h"
#include "gui/widgets/CommandBar.h"

#include <QCheckBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace Longpath {

namespace {

// Rule 7: unknown is a dash, not a zero. One spelling of it, so the
// empty state cannot drift between cells.
const QString& dash()
{
    static const QString kDash = QStringLiteral("——");
    return kDash;
}

// Rule 1: every group of controls wears a versal heading. 9 px,
// wide tracking, the group-heading grey — the measurements come from
// HAUSSTIL §Token and §"Die acht Regeln".
QLabel* groupHeading(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text.toUpper(), parent);
    l->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 9px; letter-spacing: 0.18em; }")
        .arg(QString::fromLatin1(Style::kTextGroupHead)));
    return l;
}

// Rule 5: state is an outlined capsule, always top right.
QLabel* stateCapsule(QWidget* parent)
{
    auto* l = new QLabel(parent);
    l->setAlignment(Qt::AlignCenter);
    l->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; border: 1px solid %2; border-radius: 6px; "
        "padding: 2px 9px; font-size: 9px; letter-spacing: 0.14em; }")
        .arg(QString::fromLatin1(Style::kTextScale),
             QString::fromLatin1(Style::kBorder)));
    return l;
}

// One cell of the reading grid: small versal heading, monospaced
// value under it. Returns the value label; the heading needs no
// handle because it never changes.
QLabel* gridCell(QGridLayout* grid, int row, int col,
                 const QString& heading, QWidget* parent)
{
    auto* box = new QVBoxLayout;
    box->setSpacing(2);
    box->addWidget(groupHeading(heading, parent));

    auto* value = new QLabel(dash(), parent);
    value->setFont(Style::monoFontPt(11));
    value->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
        .arg(QString::fromLatin1(Style::kAmberText)));
    box->addWidget(value);

    grid->addLayout(box, row, col);
    return value;
}

} // namespace

BroadcastOptimizerPanel::BroadcastOptimizerPanel(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    refresh();
}

void BroadcastOptimizerPanel::buildUi()
{
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(10);

    // ── Rule 6: name, one grey sentence, then controls ───────────────
    {
        auto* head = new QHBoxLayout;
        head->addWidget(groupHeading(
            QStringLiteral("Broadcast optimizer"), this));
        head->addStretch(1);
        m_state = stateCapsule(this);
        head->addWidget(m_state);
        col->addLayout(head);

        m_headline = new QLabel(QStringLiteral(
            "One take, four transmit bandwidths. Nothing here is "
            "applied until you say so."), this);
        m_headline->setWordWrap(true);
        m_headline->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
            .arg(QString::fromLatin1(Style::kTextSecondary)));
        col->addWidget(m_headline);
    }

    // ── The widths. Rule 2: exactly one of them is lit ───────────────
    {
        col->addWidget(groupHeading(QStringLiteral("Channel"), this));

        auto* row = new QHBoxLayout;
        row->setSpacing(5);
        for (int i = 0; i < BroadcastOptimizer::kWidthCount; ++i) {
            const double hz = BroadcastOptimizer::kWidthHz[i];
            auto* pill = new QPushButton(
                QStringLiteral("%1k").arg(hz / 1000.0, 0, 'f', 1), this);
            pill->setCheckable(true);
            pill->setMinimumHeight(27);
            // CommandBar::pillStyle() and not something that looks
            // like it. That is the lesson of 0.6.8: the profile rail
            // drifted away from BAND and MODE because it carried a
            // similar style instead of the same call, and the next
            // style sheet pulled them apart again. A selected width
            // here is a selection like any other — blue, because in
            // this house blue is what can be touched.
            pill->setStyleSheet(CommandBar::pillStyle());
            pill->setCursor(Qt::PointingHandCursor);
            connect(pill, &QPushButton::clicked, this,
                    [this, i] { select(i); });
            m_widthPills.push_back(pill);
            row->addWidget(pill);
        }
        row->addStretch(1);

        m_optimizeBtn = new QPushButton(QStringLiteral("Optimize"), this);
        m_optimizeBtn->setStyleSheet(Style::buttonBaseStyle());
        m_optimizeBtn->setToolTip(QStringLiteral(
            "Build all four profiles from the last recording. Reads "
            "the measurement only — your chain is not touched."));
        connect(m_optimizeBtn, &QPushButton::clicked, this,
                [this] { setAnalysis(m_analysis); });
        row->addWidget(m_optimizeBtn);
        col->addLayout(row);
    }

    // ── The reading grid ─────────────────────────────────────────────
    {
        auto* frame = new QFrame(this);
        frame->setStyleSheet(QStringLiteral(
            "QFrame { background: %1; border: 1px solid %2; "
            "border-radius: 6px; }")
            .arg(QString::fromLatin1(Style::kInsetBg),
                 QString::fromLatin1(Style::kInsetBorder)));

        auto* grid = new QGridLayout(frame);
        grid->setContentsMargins(12, 10, 12, 10);
        grid->setHorizontalSpacing(22);
        grid->setVerticalSpacing(10);

        m_cellDensity  = gridCell(grid, 0, 0, QStringLiteral("Density"), frame);
        m_cellHpf      = gridCell(grid, 0, 1, QStringLiteral("High-pass"), frame);
        m_cellTone     = gridCell(grid, 0, 2, QStringLiteral("Tone L/P/H"), frame);
        m_cellComp     = gridCell(grid, 0, 3, QStringLiteral("Compressor"), frame);
        m_cellChannel  = gridCell(grid, 1, 0, QStringLiteral("TX filter"), frame);
        m_cellGain     = gridCell(grid, 1, 1, QStringLiteral("Est. gain"), frame);
        m_cellLoudness = gridCell(grid, 1, 2, QStringLiteral("Take LUFS / LRA"), frame);
        m_cellForgets  = gridCell(grid, 1, 3, QStringLiteral("Forgets"), frame);

        col->addWidget(frame);
    }

    // ── What it decided, and what it costs ───────────────────────────
    {
        m_notes = new QLabel(this);
        m_notes->setWordWrap(true);
        m_notes->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_notes->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        m_notes->setMinimumHeight(96);
        m_notes->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; background: %2; border: 1px solid %3; "
            "padding: 10px; }")
            .arg(QString::fromLatin1(Style::kTextSecondary),
                 QString::fromLatin1(Style::kInsetBg),
                 QString::fromLatin1(Style::kInsetBorder)));
        col->addWidget(m_notes);

        // The one warm-warning line. HAUSSTIL keeps red for the band
        // edge and SWR; a profile that cannot deliver what its name
        // says is a caution, not a danger, so it takes the warning
        // amber and not the red.
        m_caveat = new QLabel(this);
        m_caveat->setWordWrap(true);
        m_caveat->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
            .arg(QString::fromLatin1(Style::kAmberWarn)));
        m_caveat->hide();
        col->addWidget(m_caveat);
    }

    // ── Hearing it ───────────────────────────────────────────────────
    //
    // The whole reason the four plans are worth anything: the same
    // fifteen seconds, through this profile and through none, one
    // click apart. Judging a transmit chain by reading its numbers is
    // how an operator ends up with a sound nobody can explain.
    {
        col->addWidget(groupHeading(QStringLiteral("Listen"), this));

        auto* row = new QHBoxLayout;
        row->setSpacing(5);

        m_hearBtn = new QPushButton(this);
        m_hearBtn->setStyleSheet(Style::buttonBaseStyle());
        m_hearBtn->setToolTip(QStringLiteral(
            "The last take rendered through this profile. From "
            "memory, off the real-time path — it cannot crackle, "
            "which is what makes it fit to judge a chain with."));
        connect(m_hearBtn, &QPushButton::clicked, this, [this] {
            if (m_selected >= 0 && m_selected < m_plans.size()) {
                emit auditionRequested(m_plans[m_selected]);
            }
        });
        row->addWidget(m_hearBtn);

        m_rawBtn = new QPushButton(QStringLiteral("▶ Without"), this);
        m_rawBtn->setStyleSheet(Style::buttonBaseStyle());
        m_rawBtn->setToolTip(QStringLiteral(
            "The same words with no processing at all, to compare "
            "against."));
        connect(m_rawBtn, &QPushButton::clicked, this,
                [this] { emit rawRequested(); });
        row->addWidget(m_rawBtn);

        // Default ON. See levelMatched() in the header for why this
        // particular default is not a preference but a correctness
        // matter.
        m_matchBox = new QCheckBox(QStringLiteral("Level-matched"), this);
        m_matchBox->setChecked(true);
        m_matchBox->setStyleSheet(QStringLiteral("QCheckBox { color: %1; }")
            .arg(QString::fromLatin1(Style::kTextSecondary)));
        m_matchBox->setToolTip(QStringLiteral(
            "Play the profile at the same loudness as the raw take, so "
            "the comparison is about tone and density rather than "
            "about which one is louder. Switch it off to hear the "
            "level the profile actually buys."));
        row->addWidget(m_matchBox);

        row->addStretch(1);

        m_loadBtn = new QPushButton(
            QStringLiteral("Load into strip"), this);
        m_loadBtn->setStyleSheet(Style::buttonBaseStyle());
        m_loadBtn->setToolTip(QStringLiteral(
            "Set the channel strip to this profile. The strip's own "
            "master switch is not touched — you decide when your "
            "transmit audio changes."));
        connect(m_loadBtn, &QPushButton::clicked, this, [this] {
            if (m_selected >= 0 && m_selected < m_plans.size()) {
                emit loadRequested(m_plans[m_selected]);
            }
        });
        row->addWidget(m_loadBtn);

        m_saveAllBtn = new QPushButton(
            QStringLiteral("Save all four"), this);
        m_saveAllBtn->setStyleSheet(Style::buttonBaseStyle());
        m_saveAllBtn->setToolTip(QStringLiteral(
            "Write all four as named presets, so they sit in the "
            "preset list beside your own and can be edited from "
            "there."));
        connect(m_saveAllBtn, &QPushButton::clicked, this,
                [this] { emit saveAllRequested(); });
        row->addWidget(m_saveAllBtn);

        col->addLayout(row);
    }
}

void BroadcastOptimizerPanel::setAnalysis(const VoiceAnalysis& a)
{
    m_analysis = a;
    m_plans    = BroadcastOptimizer::plan(a);

    // 2.7 kHz is where an operator who has not said otherwise is
    // transmitting, so that is the one that comes up selected. The
    // host overrides it by calling select() if it knows better.
    m_selected = m_plans.isEmpty()
        ? -1
        : BroadcastOptimizer::bestIndex(m_plans, 2700.0);
    refresh();
}

void BroadcastOptimizerPanel::select(int index)
{
    if (index < 0 || index >= m_plans.size()) { return; }
    m_selected = index;
    refresh();
}

bool BroadcastOptimizerPanel::levelMatched() const
{
    return m_matchBox != nullptr && m_matchBox->isChecked();
}

void BroadcastOptimizerPanel::setAuditioning(bool on)
{
    m_auditioning = on;
    refresh();
}

void BroadcastOptimizerPanel::refresh()
{
    const bool have = m_selected >= 0 && m_selected < m_plans.size();

    // Rule 2: one pill lit, the rest nearly invisible. The lit look
    // is the house's own :checked rule laid over the base style —
    // the same pair every other toggle row in the program uses, so a
    // selected width here reads identically to a selected anything
    // else.
    for (int i = 0; i < m_widthPills.size(); ++i) {
        m_widthPills[i]->setChecked(have && i == m_selected);
        m_widthPills[i]->setEnabled(!m_plans.isEmpty());
    }

    m_hearBtn->setText(m_auditioning
        ? QStringLiteral("■ Stop")
        : QStringLiteral("▶ With this profile"));

    m_hearBtn->setEnabled(have);
    m_rawBtn->setEnabled(have);
    m_loadBtn->setEnabled(have);
    m_saveAllBtn->setEnabled(!m_plans.isEmpty());
    m_optimizeBtn->setEnabled(m_analysis.valid
                              && m_analysis.clippedSamples == 0);

    if (!have) {
        m_state->setText(m_analysis.clippedSamples > 0
            ? QStringLiteral("CLIPPED")
            : QStringLiteral("NO TAKE"));
        for (QLabel* cell : {m_cellDensity, m_cellHpf, m_cellTone,
                             m_cellComp, m_cellChannel, m_cellGain,
                             m_cellLoudness, m_cellForgets}) {
            cell->setText(dash());
        }
        m_notes->setText(m_analysis.clippedSamples > 0
            ? QStringLiteral(
                "The take was clipped, so no profile was built. A "
                "clipped recording's spectrum is the spectrum of its "
                "own flat tops, and every stage set from it would be "
                "confidently wrong at once. Turn the microphone gain "
                "down and record again.")
            : QStringLiteral(
                "Record fifteen seconds with the strip switched off, "
                "then press Optimize. Everything below is built from "
                "that one measurement."));
        m_caveat->hide();
        return;
    }

    const BroadcastOptimizer::Plan& p = m_plans[m_selected];

    m_state->setText(QStringLiteral("%1 PROFILES").arg(m_plans.size()));

    m_cellDensity->setText(QStringLiteral("%1  %2")
        .arg(p.density).arg(BroadcastOptimizer::densityName(p.density)));
    m_cellHpf->setText(QStringLiteral("%1 Hz").arg(p.highPassHz, 0, 'f', 0));
    m_cellTone->setText(QStringLiteral("%1 / %2 / %3")
        .arg(p.lowShelfDb,  0, 'f', 1)
        .arg(p.presenceDb,  0, 'f', 1)
        .arg(p.highShelfDb, 0, 'f', 1));
    m_cellComp->setText(QStringLiteral("%1:1  %2 dB")
        .arg(p.compRatio, 0, 'f', 1)
        .arg(p.compThresholdDbFs, 0, 'f', 0));
    m_cellChannel->setText(QStringLiteral("%1 – %2 Hz")
        .arg(p.lowCutHz).arg(p.highCutHz));
    m_cellGain->setText(QStringLiteral("+%1 dB")
        .arg(p.densityGainDb, 0, 'f', 1));

    // Rule 7 again: a take with too few blocks for a loudness range
    // shows a dash for the range, not a zero that would read as "this
    // operator never varies".
    m_cellLoudness->setText(QStringLiteral("%1 / %2")
        .arg(QStringLiteral("%1").arg(m_analysis.loudnessLufs, 0, 'f', 1),
             m_analysis.loudnessRangeValid
                 ? QStringLiteral("%1 LU").arg(m_analysis.loudnessRangeLu,
                                               0, 'f', 1)
                 : dash()));

    m_cellForgets->setText(p.characterStages
        ? QStringLiteral("nothing")
        : QStringLiteral("valve · exciter · reverb"));

    m_notes->setText(p.notes.join(QStringLiteral("\n\n")));

    if (p.caveat.isEmpty()) {
        m_caveat->hide();
    } else {
        m_caveat->setText(p.caveat);
        m_caveat->show();
    }
}

} // namespace Longpath
