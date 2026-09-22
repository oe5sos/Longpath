#pragma once
// =================================================================
// src/models/MemoryList.h  (Longpath)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/Memory/MemoryList.cs, original licence
//   from Thetis source is included below
//
// The memory slots as a list with a file behind it. Thetis serialises
// the whole class to `memory.xml` next to its database, keeps a copy in
// `memory_bak.xml` after every successful load, and falls back to the
// copy when the main file is missing or unreadable. Longpath does the
// same, with the same element names (see MemoryRecord.h), so the file
// is interchangeable with Thetis's.
//
// Qt shape: the list is a QAbstractTableModel so the memory dialog's
// QTableView edits it in place, the way Thetis binds its DataGridView
// to the SortableBindingList. One column per MemoryRecord field, in
// the upstream grid's order.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Ported for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude, from Thetis
//                 v2.10.3.15-5-g852bf0e. XmlSerializer became
//                 QXmlStreamReader/Writer over the same element names;
//                 SortableBindingList became QAbstractTableModel.
// =================================================================
//=================================================================
// MemoryList.cs
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

#include "models/MemoryRecord.h"

#include <QAbstractTableModel>
#include <QString>
#include <QVector>

namespace Longpath {

class MemoryList : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit MemoryList(QObject* parent = nullptr);

    // The grid's columns, in the order Thetis's MemoryForm shows them
    // (MemoryForm.Designer.cs / MemoryForm.cs:86-200 [@852bf0e]).
    enum Column {
        Group = 0, RxFreq, Name, Mode, Scan, TuneStep, Rptr, RptrOffset,
        CtcssOn, CtcssFreq, Deviation, Power, Split, TxFreq, Filter,
        FilterLow, FilterHigh, Comments, AgcMode, AgcT,
        ColumnCount
    };

    // ── QAbstractTableModel ──────────────────────────────────────────
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    // ── The list ─────────────────────────────────────────────────────
    int count() const { return m_list.size(); }
    const MemoryRecord& at(int row) const { return m_list.at(row); }
    const QVector<MemoryRecord>& records() const { return m_list; }
    int  add(const MemoryRecord& record);       // appends, returns the row
    void removeAt(int row);
    void replace(int row, const MemoryRecord& record);
    void clear();

    // From Thetis Memory/MemoryList.cs:68-80 [@852bf0e]
    int majorVersion() const { return m_majorVersion; }
    int minorVersion() const { return m_minorVersion; }
    void setVersion(int major, int minor) { m_majorVersion = major; m_minorVersion = minor; }
    static constexpr int kCurrentMajorVersion = 1;   // current_major_version = 1
    static constexpr int kCurrentMinorVersion = 1;   // current_minor_version = 1
    // From Thetis Memory/MemoryList.cs:172-183 [@852bf0e] -- CheckVersion:
    // nothing to migrate between 1.0 and 1.1 upstream either.
    void checkVersion();

    // ── The file ─────────────────────────────────────────────────────
    // memory.xml in `dir`, memory_bak.xml written after every successful
    // read (Thetis Memory/MemoryList.cs:106-165 [@852bf0e]).
    static QString filePath(const QString& dir);
    static QString backupPath(const QString& dir);

    bool saveToFile(const QString& path, QString* error = nullptr) const;
    bool loadFromFile(const QString& path, QString* error = nullptr);

    // Save()/Restore() as Thetis does them, against one directory.
    bool save(const QString& dir, QString* error = nullptr) const;
    bool restore(const QString& dir, QString* error = nullptr);

    // The XML as text, for tests and for round-trip checks.
    QByteArray toXml() const;
    bool fromXml(const QByteArray& xml, QString* error = nullptr);

signals:
    void listChanged();

private:
    QVector<MemoryRecord> m_list;
    int m_majorVersion{kCurrentMajorVersion};
    int m_minorVersion{kCurrentMinorVersion};
};

} // namespace Longpath
