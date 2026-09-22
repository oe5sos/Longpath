// =================================================================
// src/gui/MemoryDialog.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/Memory/MemoryForm.cs, original licence
//   from Thetis source is included in MemoryDialog.h
//
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Ported for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude, from Thetis
//                 v2.10.3.15-5-g852bf0e.
// =================================================================
//=================================================================
// MemoryForm.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2003-2013  FlexRadio Systems
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: gpl@flexradio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    4616 W. Howard Lane  Suite 1-150
//    Austin, TX 78728
//    USA
//=================================================================
// --- From MemoryForm.Designer.cs ---
//=================================================================
// MEmoryForm.Designer.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2003-2013  FlexRadio Systems
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: gpl@flexradio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    4616 W. Howard Lane  Suite 1-150
//    Austin, TX 78728
//    USA
//=================================================================

#include "gui/MemoryDialog.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "gui/StyleConstants.h"
#include "models/MemoryList.h"
#include "models/MemoryRecord.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace Longpath {

// ---------------------------------------------------------------------------
// MemoryCellDelegate
// ---------------------------------------------------------------------------

MemoryCellDelegate::MemoryCellDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

namespace {

// The choices Thetis's combo-box columns offer (MemoryForm.cs:86-200
// [@852bf0e]): every DSPMode, every tune step, the three FMTXModes,
// the Filter enum, the AGC modes.
QStringList choicesFor(int column)
{
    switch (column) {
    case MemoryList::Mode: {
        QStringList m;
        for (int i = static_cast<int>(DSPMode::LSB); i <= static_cast<int>(DSPMode::RADE_L); ++i) {
            m << SliceModel::modeName(static_cast<DSPMode>(i));
        }
        return m;
    }
    case MemoryList::TuneStep:
        return MemoryRecord::tuneStepNames();
    case MemoryList::Rptr:
        return {QStringLiteral("High"), QStringLiteral("Simplex"), QStringLiteral("Low")};
    case MemoryList::Filter:
        // From Thetis enums.cs:328-345 [@852bf0e] Filter: F1..F10, VAR1, VAR2, NONE
        return {QStringLiteral("F1"), QStringLiteral("F2"), QStringLiteral("F3"),
                QStringLiteral("F4"), QStringLiteral("F5"), QStringLiteral("F6"),
                QStringLiteral("F7"), QStringLiteral("F8"), QStringLiteral("F9"),
                QStringLiteral("F10"), QStringLiteral("VAR1"), QStringLiteral("VAR2"),
                QStringLiteral("NONE")};
    case MemoryList::AgcMode:
        return {QStringLiteral("FIXD"), QStringLiteral("LONG"), QStringLiteral("SLOW"),
                QStringLiteral("MED"), QStringLiteral("FAST"), QStringLiteral("CUSTOM")};
    default:
        return {};
    }
}

} // namespace

QWidget* MemoryCellDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                          const QModelIndex& index) const
{
    const QStringList choices = choicesFor(index.column());
    if (choices.isEmpty()) {
        return QStyledItemDelegate::createEditor(parent, option, index);
    }
    auto* combo = new QComboBox(parent);
    combo->addItems(choices);
    return combo;
}

void MemoryCellDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    if (auto* combo = qobject_cast<QComboBox*>(editor)) {
        const int i = combo->findText(index.data(Qt::EditRole).toString());
        combo->setCurrentIndex(std::max(0, i));
        return;
    }
    QStyledItemDelegate::setEditorData(editor, index);
}

void MemoryCellDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                      const QModelIndex& index) const
{
    if (auto* combo = qobject_cast<QComboBox*>(editor)) {
        model->setData(index, combo->currentText(), Qt::EditRole);
        return;
    }
    QStyledItemDelegate::setModelData(editor, model, index);
}

// ---------------------------------------------------------------------------
// MemoryDialog
// ---------------------------------------------------------------------------

MemoryDialog::MemoryDialog(RadioModel* radio, QWidget* parent)
    : QDialog(parent)
    , m_radio(radio)
    , m_list(radio ? radio->memories() : nullptr)
{
    setWindowTitle(QStringLiteral("Memories"));
    setModal(false);
    resize(1100, 480);
    buildUi();

    // From Thetis Memory/MemoryForm.cs:72 [@852bf0e]
    //   Common.RestoreForm(this, "MemoryForm", true); // ke9ns bring up
    //   memory window in place you left it last time
    const QByteArray st = AppSettings::instance()
        .value(QStringLiteral("MemoryDialogGeometryState")).toByteArray();
    if (!st.isEmpty()) { restoreGeometry(st); }
}

void MemoryDialog::buildUi()
{
    setStyleSheet(QStringLiteral("QDialog { background: %1; }")
                      .arg(QLatin1String(Style::kAppBg)));
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(10, 10, 10, 10);
    col->setSpacing(8);

    // ── Buttons, as on the form ──────────────────────────────────────
    auto* top = new QHBoxLayout;
    top->setSpacing(6);
    m_addBtn = new QPushButton(QStringLiteral("Add"), this);
    m_addBtn->setToolTip(QStringLiteral(
        "Add the current frequency, mode, filter and AGC as a new memory"));
    m_copyBtn = new QPushButton(QStringLiteral("Copy"), this);
    // From Thetis MemoryForm.Designer.cs:366 [@852bf0e]
    m_copyBtn->setToolTip(QStringLiteral(
        "Create a new row with the same values as the currently selected row"));
    m_deleteBtn = new QPushButton(QStringLiteral("Delete"), this);
    // From Thetis MemoryForm.Designer.cs:352 [@852bf0e]
    m_deleteBtn->setToolTip(QStringLiteral("Delete the current row"));
    m_selectBtn = new QPushButton(QStringLiteral("Select"), this);
    // From Thetis MemoryForm.Designer.cs:338 [@852bf0e]
    m_selectBtn->setToolTip(QStringLiteral("Make the selected memory active "));
    for (QPushButton* b : {m_addBtn, m_copyBtn, m_deleteBtn, m_selectBtn}) {
        b->setStyleSheet(Style::buttonBaseStyle());
        top->addWidget(b);
    }
    top->addStretch(1);
    // From Thetis MemoryForm.Designer.cs:325-326 [@852bf0e]
    m_closeAfterSelect = new QCheckBox(QStringLiteral("Close after selection"), this);
    m_closeAfterSelect->setToolTip(QStringLiteral(
        "Check to close the Memory window after an entry has been selected"));
    m_closeAfterSelect->setStyleSheet(QStringLiteral("QCheckBox { color: %1; }")
                                          .arg(QLatin1String(Style::kTextPrimary)));
    m_closeAfterSelect->setChecked(
        AppSettings::instance().value(QStringLiteral("MemoryDialogCloseAfterSelect"),
                                      QStringLiteral("False")).toString() == QLatin1String("True"));
    connect(m_closeAfterSelect, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("MemoryDialogCloseAfterSelect"),
                                         on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    top->addWidget(m_closeAfterSelect);
    col->addLayout(top);

    // ── The grid ─────────────────────────────────────────────────────
    m_table = new QTableView(this);
    m_table->setModel(m_list);
    m_table->setItemDelegate(new MemoryCellDelegate(m_table));
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    // No sort until a header is clicked: the list shows in file order, as
    // the upstream grid does.
    m_table->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    m_table->horizontalHeader()->setSectionsMovable(true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setDefaultSectionSize(22);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked
                             | QAbstractItemView::EditKeyPressed);
    m_table->setStyleSheet(QStringLiteral(
        "QTableView { background: %1; alternate-background-color: %4;"
        "  color: %2; border: 1px solid %3; gridline-color: %3;"
        "  font-size: 11px; }"
        "QTableView::item:selected { background: %6; color: %2; }"
        "QHeaderView::section { background: %4; color: %5; border: none;"
        "  border-bottom: 1px solid %3; padding: 3px 6px; font-size: 11px; }"
    ).arg(QString::fromLatin1(Style::kInsetBg),
          QString::fromLatin1(Style::kTextPrimary),
          QString::fromLatin1(Style::kBorderSubtle),
          QString::fromLatin1(Style::kButtonBg),
          QString::fromLatin1(Style::kTextSecondary),
          QString::fromLatin1(Style::kAccent)));
    col->addWidget(m_table, 1);

    // ── The selected row, spelled out (ke9ns MemGroup/MemName/MemFreq/
    //    MemComments, MemoryForm.cs:937-946 [@852bf0e]) ─────────────
    auto* strip = new QHBoxLayout;
    strip->setSpacing(12);
    auto makeField = [&](const QString& caption, QLabel*& value, int stretch) {
        auto* cap = new QLabel(caption, this);
        cap->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 9px; }")
                               .arg(QLatin1String(Style::kTextScale)));
        value = new QLabel(QStringLiteral("—"), this);
        value->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                                 .arg(QLatin1String(Style::kTextPrimary)));
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto* box = new QVBoxLayout;
        box->setSpacing(1);
        box->addWidget(cap);
        box->addWidget(value);
        strip->addLayout(box, stretch);
    };
    makeField(QStringLiteral("GROUP"), m_memGroup, 1);
    makeField(QStringLiteral("NAME"), m_memName, 1);
    makeField(QStringLiteral("FREQUENCY"), m_memFreq, 1);
    makeField(QStringLiteral("COMMENTS"), m_memComments, 3);
    col->addLayout(strip);

    connect(m_addBtn, &QPushButton::clicked, this, &MemoryDialog::addCurrent);
    connect(m_copyBtn, &QPushButton::clicked, this, &MemoryDialog::copySelected);
    connect(m_deleteBtn, &QPushButton::clicked, this, &MemoryDialog::deleteSelected);
    connect(m_selectBtn, &QPushButton::clicked, this, &MemoryDialog::selectCurrent);
    // A double-click on a row recalls it, as clicking Select would; the
    // cells edit on the F2 / Enter path instead.
    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex& idx) {
        Q_UNUSED(idx);
        selectCurrent();
    });
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex&, const QModelIndex&) { refreshSelectionStrip(); });
    if (m_list) {
        connect(m_list, &MemoryList::listChanged, this, &MemoryDialog::refreshSelectionStrip);
    }
    refreshSelectionStrip();
}

int MemoryDialog::currentRow() const
{
    if (!m_table || !m_table->selectionModel()) { return -1; }
    const QModelIndex cur = m_table->currentIndex();
    return cur.isValid() ? cur.row() : -1;
}

void MemoryDialog::refreshSelectionStrip()
{
    const int row = currentRow();
    if (!m_list || row < 0 || row >= m_list->count()) {
        for (QLabel* l : {m_memGroup, m_memName, m_memFreq, m_memComments}) {
            if (l) { l->setText(QStringLiteral("—")); }
        }
        return;
    }
    const MemoryRecord& r = m_list->at(row);
    m_memGroup->setText(r.group.isEmpty() ? QStringLiteral("—") : r.group);
    m_memName->setText(r.name.isEmpty() ? QStringLiteral("—") : r.name);
    // From Thetis Memory/MemoryForm.cs:946 [@852bf0e] MemFreq.Text = s.ToString("F6");
    m_memFreq->setText(QString::number(r.rxFreqMHz, 'f', 6));
    m_memComments->setText(r.comments.isEmpty() ? QStringLiteral("—") : r.comments);
}

void MemoryDialog::saveList()
{
    if (!m_radio) { return; }
    QString err;
    if (!m_radio->saveMemories(&err)) {
        qCWarning(lcMemories) << "memory.xml not saved:" << err;
    }
}

bool MemoryDialog::askOperator(const QString& question)
{
    if (m_ask) { return m_ask(question); }
    // From Thetis Memory/MemoryForm.cs:576-580 [@852bf0e]
    //   MessageBox.Show("Are you sure you want to remove the selected row(s)?",
    //       "Remove Row(s)?", MessageBoxButtons.YesNo, MessageBoxIcon.Question);
    return QMessageBox::question(this, QStringLiteral("Remove Row(s)?"), question,
                                 QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}

// From Thetis Memory/MemoryForm.cs:502-546 [@852bf0e]
//   string mem_name = Convert.ToString(console.VFOAFreq);   //W4TME
//   (the record itself is assembled in RadioModel::captureMemory)
void MemoryDialog::addCurrent()
{
    if (!m_radio || !m_list) { return; }
    const int row = m_list->add(m_radio->captureMemory());
    m_table->selectRow(row);
    m_table->scrollToBottom();
    //   Common.SaveForm(this, "MemoryForm");    // w4tme
    //   console.MemoryList.Save();              // w4tme
    saveList();
}

// From Thetis Memory/MemoryForm.cs:548-556 [@852bf0e]
//   if (console.MemoryList.List.Count == 0) return;
//   console.MemoryList.List.Add(new MemoryRecord(console.MemoryList.List[dataGridView1.CurrentCell.RowIndex]));
void MemoryDialog::copySelected()
{
    if (!m_list || m_list->count() == 0) { return; }
    const int row = currentRow();
    if (row < 0 || row >= m_list->count()) { return; }
    const int added = m_list->add(m_list->at(row));
    m_table->selectRow(added);
    saveList();
}

// From Thetis Memory/MemoryForm.cs:564-591 [@852bf0e]
void MemoryDialog::deleteSelected()
{
    if (!m_list || m_list->count() == 0) { return; }   // nothing in the list to copy, exit
    QList<int> rows;
    if (m_table->selectionModel()) {
        for (const QModelIndex& idx : m_table->selectionModel()->selectedRows()) {
            rows << idx.row();
        }
    }
    if (rows.isEmpty()) {   // no row selected -- use current cell
        const int cur = currentRow();
        if (cur < 0 || cur > m_list->count() - 1) { return; }
        rows << cur;
    }
    if (!askOperator(QStringLiteral("Are you sure you want to remove the selected row(s)?"))) {
        return;
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int r : rows) { m_list->removeAt(r); }
    saveList();
    refreshSelectionStrip();
}

// From Thetis Memory/MemoryForm.cs:605-627 [@852bf0e]
//   int index = dataGridView1.CurrentCell.RowIndex;
//   if (index < 0 || index > console.MemoryList.List.Count - 1) return;
//   console.changeComboFMMemory(index); // ke9ns this will call recallmemory in console
//   if (chkMemoryFormClose.Checked) { ...Save(); this.Close(); }
void MemoryDialog::selectCurrent()
{
    if (!m_radio || !m_list || m_list->count() == 0) { return; }   // nothing in the list, exit
    const int index = currentRow();
    if (index < 0 || index > m_list->count() - 1) { return; }      // index out of range
    m_radio->recallMemory(m_list->at(index));
    if (m_closeAfterSelect && m_closeAfterSelect->isChecked()) {
        saveList();
        close();
    }
}

// From Thetis Memory/MemoryForm.cs:642-648 [@852bf0e]
//   Don't actually close the form, just hide it and save the position/size.
void MemoryDialog::closeEvent(QCloseEvent* event)
{
    AppSettings::instance().setValue(QStringLiteral("MemoryDialogGeometryState"), saveGeometry());
    saveList();
    event->accept();
    hide();
}

} // namespace Longpath
