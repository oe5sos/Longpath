// =================================================================
// src/gui/applets/BroadcastOptimizerPanel.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Siehe BroadcastOptimizerPanel.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-10-09 — Auf Messbericht und Kanalvergleich zurueckgebaut.
// =================================================================

#include "gui/applets/BroadcastOptimizerPanel.h"

#include "gui/StyleConstants.h"
#include "gui/widgets/CommandBar.h"

#include <QCheckBox>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace Longpath {

namespace {

const QString& dash()
{
    static const QString kDash = QStringLiteral("——");
    return kDash;
}

QLabel* groupHeading(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text.toUpper(), parent);
    l->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 9px; letter-spacing: 0.18em; }")
        .arg(QString::fromLatin1(Style::kTextGroupHead)));
    return l;
}

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

    {
        auto* head = new QHBoxLayout;
        head->addWidget(groupHeading(
            QStringLiteral("Voice and channel"), this));
        head->addStretch(1);
        m_state = new QLabel(this);
        m_state->setAlignment(Qt::AlignCenter);
        m_state->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; border: 1px solid %2; "
            "border-radius: 6px; padding: 2px 9px; font-size: 9px; "
            "letter-spacing: 0.14em; }")
            .arg(QString::fromLatin1(Style::kTextScale),
                 QString::fromLatin1(Style::kBorder)));
        head->addWidget(m_state);
        col->addLayout(head);

        auto* note = new QLabel(QStringLiteral(
            "What your take measured, and how it sounds through each "
            "transmit channel. Nothing in your strip is changed here — "
            "you set that by hand and save it under a name."), this);
        note->setWordWrap(true);
        note->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
            .arg(QString::fromLatin1(Style::kTextSecondary)));
        col->addWidget(note);
    }

    // ── Die vier Kanaele ─────────────────────────────────────────────
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
            pill->setCursor(Qt::PointingHandCursor);
            pill->setStyleSheet(CommandBar::pillStyle());
            pill->installEventFilter(this);
            connect(pill, &QPushButton::clicked, this,
                    [this, i] { select(i); });
            m_widthPills.push_back(pill);
            row->addWidget(pill);
        }
        row->addStretch(1);
        col->addLayout(row);
    }

    // ── Was gemessen wurde ───────────────────────────────────────────
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

        m_cellPitch     = gridCell(grid, 0, 0, QStringLiteral("Pitch"), frame);
        m_cellLoudness  = gridCell(grid, 0, 1, QStringLiteral("LUFS / LRA"), frame);
        m_cellRoom      = gridCell(grid, 0, 2, QStringLiteral("Room"), frame);
        m_cellSibilance = gridCell(grid, 1, 0, QStringLiteral("Sibilance"), frame);
        m_cellTop       = gridCell(grid, 1, 1, QStringLiteral("Mic ends"), frame);
        m_cellChannel   = gridCell(grid, 1, 2, QStringLiteral("Channel"), frame);

        col->addWidget(frame);
    }

    // ── Was das fuer diese Breite heisst ─────────────────────────────
    {
        m_headline = new QLabel(this);
        m_headline->setWordWrap(true);
        m_headline->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
            .arg(QString::fromLatin1(Style::kTextPrimary)));
        col->addWidget(m_headline);

        m_notes = new QLabel(this);
        m_notes->setWordWrap(true);
        m_notes->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_notes->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        m_notes->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; background: %2; border: 1px solid %3; "
            "padding: 10px; }")
            .arg(QString::fromLatin1(Style::kTextSecondary),
                 QString::fromLatin1(Style::kInsetBg),
                 QString::fromLatin1(Style::kInsetBorder)));
        col->addWidget(m_notes);

        m_caveat = new QLabel(this);
        m_caveat->setWordWrap(true);
        m_caveat->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
            .arg(QString::fromLatin1(Style::kAmberWarn)));
        m_caveat->hide();
        col->addWidget(m_caveat);
    }

    // ── Hoeren und ablegen ───────────────────────────────────────────
    {
        col->addWidget(groupHeading(QStringLiteral("Listen"), this));
        auto* row = new QHBoxLayout;
        row->setSpacing(5);

        m_beforeBtn = new QPushButton(QStringLiteral("▶ Raw"), this);
        m_beforeBtn->setStyleSheet(Style::buttonBaseStyle());
        m_beforeBtn->setToolTip(QStringLiteral(
            "Your take through this channel and nothing else — what "
            "the transmit filter alone does to it."));
        connect(m_beforeBtn, &QPushButton::clicked, this, [this] {
            if (m_selected >= 0 && m_selected < m_plans.size()) {
                emit beforeRequested(m_plans[m_selected]);
            }
        });
        row->addWidget(m_beforeBtn);

        m_afterBtn = new QPushButton(this);
        m_afterBtn->setStyleSheet(Style::buttonBaseStyle());
        m_afterBtn->setToolTip(QStringLiteral(
            "The same take through YOUR strip and then this channel. "
            "Set the strip by hand on the pages behind this one; this "
            "is how it will sound at this width."));
        connect(m_afterBtn, &QPushButton::clicked, this, [this] {
            if (m_selected >= 0 && m_selected < m_plans.size()) {
                emit afterRequested(m_plans[m_selected]);
            }
        });
        row->addWidget(m_afterBtn);

        m_matchBox = new QCheckBox(QStringLiteral("Level-matched"), this);
        m_matchBox->setChecked(false);
        m_matchBox->setStyleSheet(QStringLiteral("QCheckBox { color: %1; }")
            .arg(QString::fromLatin1(Style::kTextSecondary)));
        m_matchBox->setToolTip(QStringLiteral(
            "Off: you hear the level your strip buys. On: both sides "
            "at the same loudness, so the comparison is about tone — "
            "an unmatched A/B is always won by the louder side."));
        row->addWidget(m_matchBox);

        row->addStretch(1);

        m_stripBtn = new QPushButton(QStringLiteral("Set up the strip"), this);
        m_stripBtn->setStyleSheet(Style::buttonBaseStyle());
        m_stripBtn->setToolTip(QStringLiteral(
            "To the stage pages, where every control lives."));
        connect(m_stripBtn, &QPushButton::clicked, this,
                [this] { emit openStripRequested(); });
        row->addWidget(m_stripBtn);

        m_saveBtn = new QPushButton(QStringLiteral("Save as profile"), this);
        m_saveBtn->setStyleSheet(Style::buttonBaseStyle());
        m_saveBtn->setToolTip(QStringLiteral(
            "Store the strip exactly as it stands now under this "
            "width's name, so you can come back to it."));
        connect(m_saveBtn, &QPushButton::clicked, this, [this] {
            if (m_selected >= 0 && m_selected < m_plans.size()) {
                emit saveProfileRequested(m_plans[m_selected]);
            }
        });
        row->addWidget(m_saveBtn);

        col->addLayout(row);
    }
}

void BroadcastOptimizerPanel::setAnalysis(const VoiceAnalysis& a)
{
    const double previousWidth =
        (m_selected >= 0 && m_selected < m_plans.size())
            ? m_plans[m_selected].widthHz
            : 0.0;

    m_analysis = a;
    m_plans    = BroadcastOptimizer::plan(a);

    if (m_plans.isEmpty()) {
        m_selected = -1;
    } else {
        m_selected = BroadcastOptimizer::bestIndex(
            m_plans, previousWidth > 0.0 ? previousWidth : 2700.0);
    }
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

bool BroadcastOptimizerPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        for (int i = 0; i < m_widthPills.size(); ++i) {
            if (m_widthPills[i] != watched) { continue; }
            select(i);
            emit openStripRequested();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void BroadcastOptimizerPanel::refresh()
{
    const bool have = m_selected >= 0 && m_selected < m_plans.size();

    for (int i = 0; i < m_widthPills.size(); ++i) {
        m_widthPills[i]->setChecked(have && i == m_selected);
        m_widthPills[i]->setEnabled(!m_plans.isEmpty());
    }

    m_afterBtn->setText(m_auditioning
        ? QStringLiteral("■ Stop")
        : QStringLiteral("▶ Through your strip"));

    m_beforeBtn->setEnabled(have);
    m_afterBtn->setEnabled(have);
    m_saveBtn->setEnabled(have);

    if (!have) {
        m_state->setText(m_analysis.clippedSamples > 0
            ? QStringLiteral("CLIPPED")
            : QStringLiteral("NO TAKE"));
        for (QLabel* cell : {m_cellPitch, m_cellLoudness, m_cellRoom,
                             m_cellSibilance, m_cellTop, m_cellChannel}) {
            cell->setText(dash());
        }
        m_headline->clear();
        m_notes->setText(m_analysis.clippedSamples > 0
            ? QStringLiteral(
                "The take was clipped, so nothing can be measured from "
                "it. A clipped recording's spectrum is the spectrum of "
                "its own flat tops. Turn the microphone gain down and "
                "record again.")
            : QStringLiteral(
                "Record fifteen seconds above, then pick a channel."));
        m_caveat->hide();
        return;
    }

    const BroadcastOptimizer::Plan& p = m_plans[m_selected];
    m_state->setText(QStringLiteral("MEASURED"));

    m_cellPitch->setText(m_analysis.pitchHz > 0.0
        ? QStringLiteral("%1 Hz").arg(m_analysis.pitchHz, 0, 'f', 0)
        : dash());
    m_cellLoudness->setText(QStringLiteral("%1 / %2")
        .arg(QStringLiteral("%1").arg(m_analysis.loudnessLufs, 0, 'f', 1),
             m_analysis.loudnessRangeValid
                 ? QStringLiteral("%1 LU").arg(m_analysis.loudnessRangeLu,
                                               0, 'f', 1)
                 : dash()));
    m_cellRoom->setText(QStringLiteral("%1 dB down")
        .arg(m_analysis.speechDbFs - m_analysis.noiseFloorDbFs, 0, 'f', 0));
    m_cellSibilance->setText(QStringLiteral("%1 Hz  %2 dB")
        .arg(m_analysis.sibilanceHz, 0, 'f', 0)
        .arg(m_analysis.sibilanceDb, 0, 'f', 1));
    m_cellTop->setText(m_analysis.rolloffHz > 0.0
        ? QStringLiteral("%1 Hz").arg(m_analysis.rolloffHz, 0, 'f', 0)
        : dash());
    m_cellChannel->setText(QStringLiteral("%1 – %2 Hz")
        .arg(p.lowCutHz).arg(p.highCutHz));

    m_headline->setText(p.headline);
    m_notes->setText(p.notes.join(QStringLiteral("\n\n")));

    if (p.caveat.isEmpty()) {
        m_caveat->hide();
    } else {
        m_caveat->setText(p.caveat);
        m_caveat->show();
    }
}

} // namespace Longpath
