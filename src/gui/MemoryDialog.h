#pragma once
// =================================================================
// src/gui/MemoryDialog.h  (Longpath)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/Memory/MemoryForm.cs, original licence
//   from Thetis source is included below
//   Project Files/Source/Console/Memory/MemoryForm.Designer.cs (button
//   texts and tooltips), original licence from Thetis source is included
//   in MemoryDialog.cpp
//
// The memory window: the memory list as an editable grid, with Add
// (capture the active slice), Copy, Delete (after asking), Select
// (recall), "Close after selection", and a strip under the grid that
// names the selected row (group, name, frequency, comments -- Thetis's
// MemGroup / MemName / MemFreq / MemComments). Closing hides the window
// and saves the list, as MemoryForm_FormClosing does; every Add / Copy /
// Delete saves as well, as upstream does after each of them.
//
// Not ported: the ke9ns schedule strip (Longpath's RecordingScheduler
// owns scheduled recording), drag-and-drop of a browser URL onto Add
// (ke9ns "ReadURL"), "Open Rec Folder", MP3 conversion, Always-on-top.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Ported for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude, from Thetis
//                 v2.10.3.15-5-g852bf0e. WinForms DataGridView over a
//                 BindingList became QTableView over MemoryList
//                 (QAbstractTableModel); combo-box columns became
//                 delegates; MessageBox became an operator hook so
//                 tests can answer it.
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

#include <QDialog>
#include <QStyledItemDelegate>

#include <functional>

class QCheckBox;
class QCloseEvent;
class QLabel;
class QPushButton;
class QTableView;

namespace Longpath {

class RadioModel;
class MemoryList;

/// Combo-box cells for the enum columns (DSP Mode, Tune Step, RPTR, RX
/// Filter, AGC Mode) and check boxes for the bool ones -- what Thetis
/// builds as DataGridViewComboBoxColumn / DataGridViewCheckBoxColumn.
class MemoryCellDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit MemoryCellDelegate(QObject* parent = nullptr);
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override;
};

class MemoryDialog : public QDialog {
    Q_OBJECT
public:
    explicit MemoryDialog(RadioModel* radio, QWidget* parent = nullptr);

    // Tests: answer the delete confirmation without a modal box.
    void setOperatorHooks(std::function<bool(const QString&)> ask)
    { m_ask = std::move(ask); }

    QTableView* tableForTest() const { return m_table; }
    int currentRow() const;

public slots:
    // From Thetis Memory/MemoryForm.cs:502-546 [@852bf0e] MemoryRecordAdd_Click
    //   string mem_name = Convert.ToString(console.VFOAFreq);   //W4TME
    void addCurrent();
    // From Thetis Memory/MemoryForm.cs:548-556 [@852bf0e] btnMemoryRecordCopy_Click
    void copySelected();
    // From Thetis Memory/MemoryForm.cs:564-591 [@852bf0e] btnMemoryRecordDelete_Click
    void deleteSelected();
    // From Thetis Memory/MemoryForm.cs:605-627 [@852bf0e] btnSelect_Click
    void selectCurrent();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void refreshSelectionStrip();
    void saveList();
    bool askOperator(const QString& question);

    RadioModel*  m_radio{nullptr};
    MemoryList*  m_list{nullptr};
    QTableView*  m_table{nullptr};
    QPushButton* m_addBtn{nullptr};
    QPushButton* m_copyBtn{nullptr};
    QPushButton* m_deleteBtn{nullptr};
    QPushButton* m_selectBtn{nullptr};
    QCheckBox*   m_closeAfterSelect{nullptr};
    QLabel*      m_memGroup{nullptr};
    QLabel*      m_memName{nullptr};
    QLabel*      m_memFreq{nullptr};
    QLabel*      m_memComments{nullptr};
    std::function<bool(const QString&)> m_ask;
};

} // namespace Longpath
