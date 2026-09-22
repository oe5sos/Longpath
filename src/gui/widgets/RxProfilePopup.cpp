// =================================================================
// src/gui/widgets/RxProfilePopup.cpp  (Longpath)
// =================================================================
//
// Longpath-original file. See RxProfilePopup.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "gui/widgets/RxProfilePopup.h"

#include "core/RxProfileManager.h"
#include "gui/StyleConstants.h"
#include "models/SliceModel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace Longpath {

RxProfilePopup::RxProfilePopup(RxProfileManager* manager, SliceModel* slice, QWidget* parent)
    : QWidget(parent, Qt::Popup)
    , m_manager(manager)
    , m_slice(slice)
{
    setObjectName(QStringLiteral("RxProfilePopup"));
    buildUi();
    if (m_manager) {
        connect(m_manager, &RxProfileManager::profileListChanged, this, &RxProfilePopup::refresh);
        connect(m_manager, &RxProfileManager::activeProfileChanged, this, &RxProfilePopup::refresh);
    }
    refresh();
}

void RxProfilePopup::setSlice(SliceModel* slice)
{
    m_slice = slice;
    updateButtons();
}

void RxProfilePopup::buildUi()
{
    setStyleSheet(QStringLiteral(
        "#RxProfilePopup { background: %1; border: 1px solid %2; border-radius: 8px; }"
        "#RxProfilePopup QLabel { color: %3; font-size: %4px; background: transparent; }"
        "#RxProfilePopup QListWidget { background: %5; color: %6; border: 1px solid %2;"
        "  border-radius: 6px; font-size: %4px; }"
        "#RxProfilePopup QListWidget::item { padding: 3px 6px; }"
        "#RxProfilePopup QListWidget::item:selected { background: %7; color: %6; }"
    ).arg(QLatin1String(Style::kPanelBg), QLatin1String(Style::kBorder),
          QLatin1String(Style::kTextSecondary))
     .arg(Style::kFontSmall)
     .arg(QLatin1String(Style::kInsetBg), QLatin1String(Style::kTextPrimary),
          QLatin1String(Style::kAccent)));
    setMinimumWidth(260);

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(10, 8, 10, 8);
    col->setSpacing(6);

    auto* title = new QLabel(QStringLiteral("RX PROFILES"), this);
    title->setFont(Style::capsFont(font()));
    col->addWidget(title);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setFixedHeight(120);
    m_list->setToolTip(QStringLiteral(
        "Saved receive profiles: AGC, noise reduction, noise blankers, ANF, "
        "squelch, APF, binaural of one slice. Double-click loads."));
    col->addWidget(m_list);

    auto* row = new QHBoxLayout;
    row->setSpacing(6);
    m_loadBtn = new QPushButton(QStringLiteral("Load"), this);
    m_loadBtn->setToolTip(QStringLiteral("Apply the selected profile to this slice"));
    m_saveBtn = new QPushButton(QStringLiteral("Save"), this);
    m_saveBtn->setToolTip(QStringLiteral("Overwrite the selected profile with this slice's settings"));
    m_deleteBtn = new QPushButton(QStringLiteral("Delete"), this);
    m_deleteBtn->setToolTip(QStringLiteral("Remove the selected profile"));
    for (QPushButton* b : {m_loadBtn, m_saveBtn, m_deleteBtn}) {
        b->setStyleSheet(Style::buttonBaseStyle());
        row->addWidget(b);
    }
    row->addStretch(1);
    col->addLayout(row);

    // Inline confirmation for Delete (a modal box would close the popup).
    m_confirmRow = new QWidget(this);
    auto* crow = new QHBoxLayout(m_confirmRow);
    crow->setContentsMargins(0, 0, 0, 0);
    crow->setSpacing(6);
    m_confirmLabel = new QLabel(m_confirmRow);
    m_confirmYes = new QPushButton(QStringLiteral("Yes"), m_confirmRow);
    m_confirmNo = new QPushButton(QStringLiteral("No"), m_confirmRow);
    for (QPushButton* b : {m_confirmYes, m_confirmNo}) {
        b->setStyleSheet(Style::buttonBaseStyle());
    }
    crow->addWidget(m_confirmLabel, 1);
    crow->addWidget(m_confirmYes);
    crow->addWidget(m_confirmNo);
    m_confirmRow->hide();
    col->addWidget(m_confirmRow);

    auto* newRow = new QHBoxLayout;
    newRow->setSpacing(6);
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(QStringLiteral("New profile name"));
    m_nameEdit->setStyleSheet(Style::lineEditStyle());
    m_nameEdit->setToolTip(QStringLiteral("Type a name and press Enter or Save As"));
    m_saveAsBtn = new QPushButton(QStringLiteral("Save As"), this);
    m_saveAsBtn->setStyleSheet(Style::buttonBaseStyle());
    m_saveAsBtn->setToolTip(QStringLiteral("Save this slice's settings under the new name"));
    newRow->addWidget(m_nameEdit, 1);
    newRow->addWidget(m_saveAsBtn);
    col->addLayout(newRow);

    m_status = new QLabel(this);
    m_status->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: %2px; }")
                                .arg(QLatin1String(Style::kTextScale)).arg(Style::kFontCaption));
    col->addWidget(m_status);

    connect(m_list, &QListWidget::itemSelectionChanged, this, &RxProfilePopup::updateButtons);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { loadSelected(); });
    connect(m_loadBtn, &QPushButton::clicked, this, &RxProfilePopup::loadSelected);
    connect(m_saveBtn, &QPushButton::clicked, this, &RxProfilePopup::saveSelected);
    connect(m_deleteBtn, &QPushButton::clicked, this, &RxProfilePopup::deleteSelected);
    connect(m_saveAsBtn, &QPushButton::clicked, this, &RxProfilePopup::saveAsNew);
    connect(m_nameEdit, &QLineEdit::returnPressed, this, &RxProfilePopup::saveAsNew);
    connect(m_nameEdit, &QLineEdit::textChanged, this, [this](const QString&) { updateButtons(); });
    connect(m_confirmYes, &QPushButton::clicked, this, [this]() {
        if (m_manager && !m_pendingDelete.isEmpty()) {
            const QString name = m_pendingDelete;
            hideConfirm();
            if (m_manager->deleteProfile(name)) {
                m_status->setText(QStringLiteral("Deleted %1").arg(name));
            }
        }
    });
    connect(m_confirmNo, &QPushButton::clicked, this, [this]() { hideConfirm(); });
}

QString RxProfilePopup::selectedName() const
{
    const QList<QListWidgetItem*> sel = m_list->selectedItems();
    return sel.isEmpty() ? QString() : sel.first()->text();
}

void RxProfilePopup::refresh()
{
    if (!m_manager) { return; }
    const QString keep = selectedName();
    const QString active = m_manager->activeProfileName();
    m_list->clear();
    for (const QString& name : m_manager->profileNames()) {
        auto* it = new QListWidgetItem(name, m_list);
        if (name == active) {
            QFont f = it->font();
            f.setBold(true);
            it->setFont(f);
            it->setToolTip(QStringLiteral("The profile last loaded or saved"));
        }
    }
    const QString want = keep.isEmpty() ? active : keep;
    if (!want.isEmpty()) {
        const QList<QListWidgetItem*> hits = m_list->findItems(want, Qt::MatchExactly);
        if (!hits.isEmpty()) { m_list->setCurrentItem(hits.first()); }
    }
    hideConfirm();
    updateButtons();
}

void RxProfilePopup::updateButtons()
{
    const bool one = !selectedName().isEmpty();
    m_loadBtn->setEnabled(one && m_slice);
    m_saveBtn->setEnabled(one && m_slice);
    m_deleteBtn->setEnabled(one);
    m_saveAsBtn->setEnabled(m_slice && !m_nameEdit->text().trimmed().isEmpty());
}

void RxProfilePopup::loadSelected()
{
    const QString name = selectedName();
    if (!m_manager || !m_slice || name.isEmpty()) { return; }
    if (m_manager->applyProfile(name, m_slice)) {
        m_status->setText(QStringLiteral("Loaded %1").arg(name));
    }
}

void RxProfilePopup::saveSelected()
{
    const QString name = selectedName();
    if (!m_manager || !m_slice || name.isEmpty()) { return; }
    if (m_manager->saveProfile(name, m_slice)) {
        m_status->setText(QStringLiteral("Saved %1").arg(name));
    }
}

void RxProfilePopup::saveAsNew()
{
    const QString name = m_nameEdit->text().trimmed();
    if (!m_manager || !m_slice || name.isEmpty()) { return; }
    if (m_manager->saveProfile(name, m_slice)) {
        m_nameEdit->clear();
        m_status->setText(QStringLiteral("Saved %1").arg(name));
    }
}

void RxProfilePopup::deleteSelected()
{
    const QString name = selectedName();
    if (!m_manager || name.isEmpty()) { return; }
    showConfirm(name);
}

void RxProfilePopup::showConfirm(const QString& name)
{
    m_pendingDelete = name;
    m_confirmLabel->setText(QStringLiteral("Delete „%1“?").arg(name));
    m_confirmRow->show();
    adjustSize();
}

void RxProfilePopup::hideConfirm()
{
    m_pendingDelete.clear();
    if (m_confirmRow->isVisible()) {
        m_confirmRow->hide();
        adjustSize();
    }
}

void RxProfilePopup::showBelow(QWidget* anchor)
{
    refresh();
    adjustSize();
    // Under the gear, right-aligned with it -- as the TX fine sheet does,
    // so the sheet stays inside the window when the applet is docked at
    // the right edge.
    const QPoint at = anchor
        ? anchor->mapToGlobal(QPoint(anchor->width() - width(), anchor->height() + 4))
        : QPoint(0, 0);
    move(at);
    show();
    m_nameEdit->setFocus();
}

} // namespace Longpath
