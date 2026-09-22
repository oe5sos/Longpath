// =================================================================
// src/models/MemoryList.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/Memory/MemoryList.cs, original licence
//   from Thetis source is included in MemoryList.h
//   Project Files/Source/Console/Memory/MemoryForm.cs (the grid's
//   columns and their cell validation), original licence from Thetis
//   source is included in MemoryDialog.h
//
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Ported for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude, from Thetis
//                 v2.10.3.15-5-g852bf0e.
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
// --- From MemoryForm.cs ---
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

#include "models/MemoryList.h"

#include "core/LogCategories.h"
#include "models/SliceModel.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <algorithm>

namespace Longpath {

namespace {

// The element names XmlSerializer writes for MemoryRecord's properties
// (Memory/MemoryRecord.cs:181-478 [@852bf0e]) -- one per public property.
constexpr const char* kGroup       = "Group";
constexpr const char* kRXFreq      = "RXFreq";
constexpr const char* kName        = "Name";
constexpr const char* kDSPMode     = "DSPMode";
constexpr const char* kComments    = "Comments";
constexpr const char* kScan        = "Scan";
constexpr const char* kTuneStep    = "TuneStep";
constexpr const char* kRPTR        = "RPTR";
constexpr const char* kRPTROffset  = "RPTROffset";
constexpr const char* kCTCSSOn     = "CTCSSOn";
constexpr const char* kCTCSSFreq   = "CTCSSFreq";
constexpr const char* kDeviation   = "Deviation";
constexpr const char* kPower       = "Power";
constexpr const char* kSplit       = "Split";
constexpr const char* kTXFreq      = "TXFreq";
constexpr const char* kRXFilter    = "RXFilter";
constexpr const char* kRXFilterLow = "RXFilterLow";
constexpr const char* kRXFilterHigh = "RXFilterHigh";
constexpr const char* kAGCMode     = "AGCMode";
constexpr const char* kAGCT        = "AGCT";

QString boolText(bool v)
{
    // XmlSerializer writes booleans lower-case.
    return v ? QStringLiteral("true") : QStringLiteral("false");
}

bool boolFrom(const QString& s)
{
    const QString t = s.trimmed().toLower();
    return t == QLatin1String("true") || t == QLatin1String("1");
}

// Invariant-culture doubles, as XmlSerializer writes them ("0.78").
QString doubleText(double v)
{
    return QString::number(v, 'g', 15);
}

} // namespace

MemoryList::MemoryList(QObject* parent)
    : QAbstractTableModel(parent)
{
}

// ---------------------------------------------------------------------------
// QAbstractTableModel
// ---------------------------------------------------------------------------

int MemoryList::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_list.size();
}

int MemoryList::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant MemoryList::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) { return {}; }
    if (orientation == Qt::Vertical) { return section + 1; }
    // From Thetis Memory/MemoryForm.cs:86-200 [@852bf0e] -- the HeaderText
    // of each grid column.
    switch (section) {
    case Group:       return QStringLiteral("Group");
    case RxFreq:      return QStringLiteral("RX Freq");
    case Name:        return QStringLiteral("Name");
    case Mode:        return QStringLiteral("DSP Mode");
    case Scan:        return QStringLiteral("Scan");
    case TuneStep:    return QStringLiteral("Tune Step");
    case Rptr:        return QStringLiteral("RPTR");
    case RptrOffset:  return QStringLiteral("RPTR Offset");
    case CtcssOn:     return QStringLiteral("CTCSS On");
    case CtcssFreq:   return QStringLiteral("CTCSS Freq");
    case Deviation:   return QStringLiteral("Deviation");
    case Power:       return QStringLiteral("Power");
    case Split:       return QStringLiteral("Split");
    case TxFreq:      return QStringLiteral("TX Freq");
    case Filter:      return QStringLiteral("RX Filter");
    case FilterLow:   return QStringLiteral("Filter Low");
    case FilterHigh:  return QStringLiteral("Filter High");
    case Comments:    return QStringLiteral("Comments");
    case AgcMode:     return QStringLiteral("AGC Mode");
    case AgcT:        return QStringLiteral("AGC-T");
    default:          return {};
    }
}

namespace {
bool isCheckColumn(int column)
{
    // Thetis's DataGridViewCheckBoxColumns: Scan, CTCSSOn, Split.
    return column == MemoryList::Scan || column == MemoryList::CtcssOn
        || column == MemoryList::Split;
}
} // namespace

QVariant MemoryList::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_list.size()) { return {}; }
    const MemoryRecord& r = m_list.at(index.row());
    if (isCheckColumn(index.column())) {
        // A check box, not the word "true": what the upstream grid shows.
        const bool on = index.column() == Scan ? r.scan
                      : index.column() == CtcssOn ? r.ctcssOn : r.split;
        if (role == Qt::CheckStateRole) { return on ? Qt::Checked : Qt::Unchecked; }
        if (role == Qt::EditRole) { return on; }
        return {};
    }
    if (role != Qt::DisplayRole && role != Qt::EditRole) { return {}; }
    switch (index.column()) {
    case Group:       return r.group;
    case RxFreq:      return role == Qt::EditRole ? QVariant(r.rxFreqMHz)
                                                  : QVariant(QString::number(r.rxFreqMHz, 'f', 6));
    case Name:        return r.name;
    case Mode:        return SliceModel::modeName(r.dspMode);
    case Scan:        return r.scan;
    case TuneStep:    return r.tuneStep;
    case Rptr:        return MemoryRecord::fmTxModeName(r.rptr);
    case RptrOffset:  return role == Qt::EditRole ? QVariant(r.rptrOffsetMHz)
                                                  : QVariant(QString::number(r.rptrOffsetMHz, 'f', 3));
    case CtcssOn:     return r.ctcssOn;
    case CtcssFreq:   return role == Qt::EditRole ? QVariant(r.ctcssFreq)
                                                  : QVariant(QString::number(r.ctcssFreq, 'f', 1));
    case Deviation:   return r.deviation;
    case Power:       return r.power;
    case Split:       return r.split;
    case TxFreq:      return role == Qt::EditRole ? QVariant(r.txFreqMHz)
                                                  : QVariant(QString::number(r.txFreqMHz, 'f', 6));
    case Filter:      return r.rxFilter;
    case FilterLow:   return r.rxFilterLow;
    case FilterHigh:  return r.rxFilterHigh;
    case Comments:    return r.comments;
    case AgcMode:     return MemoryRecord::agcModeName(r.agcMode);
    case AgcT:        return r.agcT;
    default:          return {};
    }
}

bool MemoryList::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_list.size()) {
        return false;
    }
    if (role == Qt::CheckStateRole && isCheckColumn(index.column())) {
        return setData(index, value.toInt() == Qt::Checked, Qt::EditRole);
    }
    if (role != Qt::EditRole) { return false; }
    MemoryRecord& r = m_list[index.row()];
    // From Thetis Memory/MemoryForm.cs:387-410 [@852bf0e]
    //   dataGridView1_CellValidating: floating-point and integer cells that
    //   do not parse are set to 0.0 / 0 rather than refused.
    auto dbl = [&](double& target) {
        bool ok = false;
        const double v = value.toDouble(&ok);
        target = ok ? v : 0.0;
    };
    auto integer = [&](int& target) {
        bool ok = false;
        const int v = value.toInt(&ok);
        target = ok ? v : 0;
    };
    switch (index.column()) {
    case Group:       r.group = value.toString(); break;
    case RxFreq:      dbl(r.rxFreqMHz); break;
    case Name:        r.name = value.toString(); break;
    case Mode:        r.dspMode = SliceModel::modeFromName(value.toString()); break;
    case Scan:        r.scan = value.toBool(); break;
    case TuneStep:    r.tuneStep = value.toString(); break;
    case Rptr:        r.rptr = MemoryRecord::fmTxModeFromName(value.toString()); break;
    case RptrOffset:  dbl(r.rptrOffsetMHz); break;
    case CtcssOn:     r.ctcssOn = value.toBool(); break;
    case CtcssFreq:   dbl(r.ctcssFreq); break;
    case Deviation:   integer(r.deviation); break;
    case Power:       integer(r.power); break;
    case Split:       r.split = value.toBool(); break;
    case TxFreq:      dbl(r.txFreqMHz); break;
    case Filter:      r.rxFilter = value.toString(); break;
    case FilterLow:   integer(r.rxFilterLow); break;
    case FilterHigh:  integer(r.rxFilterHigh); break;
    case Comments:    r.comments = value.toString(); break;
    case AgcMode:     r.agcMode = MemoryRecord::agcModeFromName(value.toString()); break;
    case AgcT:        integer(r.agcT); break;
    default:          return false;
    }
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole, Qt::CheckStateRole});
    emit listChanged();
    return true;
}

Qt::ItemFlags MemoryList::flags(const QModelIndex& index) const
{
    if (!index.isValid()) { return Qt::NoItemFlags; }
    if (isCheckColumn(index.column())) {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
    }
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

namespace {

// The value one grid column shows for one record, as the sort and the
// EditRole read it.
QVariant fieldValue(const MemoryRecord& r, int column)
{
    switch (column) {
    case MemoryList::Group:       return r.group;
    case MemoryList::RxFreq:      return r.rxFreqMHz;
    case MemoryList::Name:        return r.name;
    case MemoryList::Mode:        return SliceModel::modeName(r.dspMode);
    case MemoryList::Scan:        return r.scan;
    case MemoryList::TuneStep:    return r.tuneStep;
    case MemoryList::Rptr:        return MemoryRecord::fmTxModeName(r.rptr);
    case MemoryList::RptrOffset:  return r.rptrOffsetMHz;
    case MemoryList::CtcssOn:     return r.ctcssOn;
    case MemoryList::CtcssFreq:   return r.ctcssFreq;
    case MemoryList::Deviation:   return r.deviation;
    case MemoryList::Power:       return r.power;
    case MemoryList::Split:       return r.split;
    case MemoryList::TxFreq:      return r.txFreqMHz;
    case MemoryList::Filter:      return r.rxFilter;
    case MemoryList::FilterLow:   return r.rxFilterLow;
    case MemoryList::FilterHigh:  return r.rxFilterHigh;
    case MemoryList::Comments:    return r.comments;
    case MemoryList::AgcMode:     return MemoryRecord::agcModeName(r.agcMode);
    case MemoryList::AgcT:        return r.agcT;
    default:                      return {};
    }
}

bool fieldLess(const QVariant& va, const QVariant& vb)
{
    if (va.typeId() == QMetaType::Double || va.typeId() == QMetaType::Int) {
        return va.toDouble() < vb.toDouble();
    }
    if (va.typeId() == QMetaType::Bool) {
        return !va.toBool() && vb.toBool();
    }
    return va.toString().localeAwareCompare(vb.toString()) < 0;
}

} // namespace

void MemoryList::sort(int column, Qt::SortOrder order)
{
    if (m_list.isEmpty()) { return; }
    beginResetModel();
    std::stable_sort(m_list.begin(), m_list.end(),
                     [column, order](const MemoryRecord& a, const MemoryRecord& b) {
        const QVariant va = fieldValue(a, column);
        const QVariant vb = fieldValue(b, column);
        return order == Qt::AscendingOrder ? fieldLess(va, vb) : fieldLess(vb, va);
    });
    endResetModel();
    emit listChanged();
}

// ---------------------------------------------------------------------------
// The list
// ---------------------------------------------------------------------------

int MemoryList::add(const MemoryRecord& record)
{
    const int row = m_list.size();
    beginInsertRows(QModelIndex(), row, row);
    m_list.push_back(record);
    endInsertRows();
    emit listChanged();
    return row;
}

void MemoryList::removeAt(int row)
{
    if (row < 0 || row >= m_list.size()) { return; }
    beginRemoveRows(QModelIndex(), row, row);
    m_list.remove(row);
    endRemoveRows();
    emit listChanged();
}

void MemoryList::replace(int row, const MemoryRecord& record)
{
    if (row < 0 || row >= m_list.size()) { return; }
    m_list[row] = record;
    emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
    emit listChanged();
}

void MemoryList::clear()
{
    if (m_list.isEmpty()) { return; }
    beginResetModel();
    m_list.clear();
    endResetModel();
    emit listChanged();
}

// From Thetis Memory/MemoryList.cs:172-183 [@852bf0e]
void MemoryList::checkVersion()
{
    if (m_majorVersion == kCurrentMajorVersion && m_minorVersion == kCurrentMinorVersion) { return; }
    if (m_majorVersion == 1 && m_minorVersion == 0) {
        // go modify the data as appropriate
    }
}

// ---------------------------------------------------------------------------
// The file
// ---------------------------------------------------------------------------

// From Thetis Memory/MemoryList.cs:107-109 [@852bf0e]
//   string file_name = path + "memory.xml";
QString MemoryList::filePath(const QString& dir)
{
    return dir + QStringLiteral("/memory.xml");
}

// From Thetis Memory/MemoryList.cs:118 [@852bf0e]
//   string bak_file_name = path + "memory_bak.xml";
QString MemoryList::backupPath(const QString& dir)
{
    return dir + QStringLiteral("/memory_bak.xml");
}

QByteArray MemoryList::toXml() const
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.setAutoFormattingIndent(2);
    w.writeStartDocument(QStringLiteral("1.0"));
    // The root XmlSerializer writes for `MemoryList`, with its two
    // namespace declarations; Thetis reads the file back through the
    // same serialiser, which does not need them but always writes them.
    w.writeStartElement(QStringLiteral("MemoryList"));
    w.writeNamespace(QStringLiteral("http://www.w3.org/2001/XMLSchema-instance"), QStringLiteral("xsi"));
    w.writeNamespace(QStringLiteral("http://www.w3.org/2001/XMLSchema"), QStringLiteral("xsd"));
    w.writeStartElement(QStringLiteral("List"));
    for (const MemoryRecord& r : m_list) {
        w.writeStartElement(QStringLiteral("MemoryRecord"));
        w.writeTextElement(QLatin1String(kGroup), r.group);
        w.writeTextElement(QLatin1String(kRXFreq), doubleText(r.rxFreqMHz));
        w.writeTextElement(QLatin1String(kName), r.name);
        w.writeTextElement(QLatin1String(kDSPMode), SliceModel::modeName(r.dspMode));
        w.writeTextElement(QLatin1String(kComments), r.comments);
        w.writeTextElement(QLatin1String(kScan), boolText(r.scan));
        w.writeTextElement(QLatin1String(kTuneStep), r.tuneStep);
        w.writeTextElement(QLatin1String(kRPTR), MemoryRecord::fmTxModeName(r.rptr));
        w.writeTextElement(QLatin1String(kRPTROffset), doubleText(r.rptrOffsetMHz));
        w.writeTextElement(QLatin1String(kCTCSSOn), boolText(r.ctcssOn));
        w.writeTextElement(QLatin1String(kCTCSSFreq), doubleText(r.ctcssFreq));
        w.writeTextElement(QLatin1String(kDeviation), QString::number(r.deviation));
        w.writeTextElement(QLatin1String(kPower), QString::number(r.power));
        w.writeTextElement(QLatin1String(kSplit), boolText(r.split));
        w.writeTextElement(QLatin1String(kTXFreq), doubleText(r.txFreqMHz));
        w.writeTextElement(QLatin1String(kRXFilter), r.rxFilter);
        w.writeTextElement(QLatin1String(kRXFilterLow), QString::number(r.rxFilterLow));
        w.writeTextElement(QLatin1String(kRXFilterHigh), QString::number(r.rxFilterHigh));
        w.writeTextElement(QLatin1String(kAGCMode), MemoryRecord::agcModeName(r.agcMode));
        w.writeTextElement(QLatin1String(kAGCT), QString::number(r.agcT));
        // Longpath addition: whatever the file carried that this program
        // does not model (the ke9ns schedule set), written back as read.
        for (const auto& kv : r.extras) {
            w.writeTextElement(kv.first, kv.second);
        }
        w.writeEndElement();   // MemoryRecord
    }
    w.writeEndElement();       // List
    w.writeTextElement(QStringLiteral("MajorVersion"), QString::number(m_majorVersion));
    w.writeTextElement(QStringLiteral("MinorVersion"), QString::number(m_minorVersion));
    w.writeEndElement();       // MemoryList
    w.writeEndDocument();
    return out;
}

bool MemoryList::fromXml(const QByteArray& xml, QString* error)
{
    QXmlStreamReader x(xml);
    QVector<MemoryRecord> parsed;
    int major = kCurrentMajorVersion;
    int minor = kCurrentMinorVersion;
    bool sawRoot = false;

    while (!x.atEnd()) {
        x.readNext();
        if (!x.isStartElement()) { continue; }
        const QString tag = x.name().toString();
        if (tag == QLatin1String("MemoryList")) { sawRoot = true; continue; }
        if (tag == QLatin1String("MajorVersion")) { major = x.readElementText().toInt(); continue; }
        if (tag == QLatin1String("MinorVersion")) { minor = x.readElementText().toInt(); continue; }
        if (tag != QLatin1String("MemoryRecord")) { continue; }

        MemoryRecord r;
        while (!(x.isEndElement() && x.name() == QLatin1String("MemoryRecord")) && !x.atEnd()) {
            x.readNext();
            if (!x.isStartElement()) { continue; }
            const QString field = x.name().toString();
            const QString text = x.readElementText();
            if      (field == QLatin1String(kGroup))        { r.group = text; }
            else if (field == QLatin1String(kRXFreq))       { r.rxFreqMHz = text.toDouble(); }
            else if (field == QLatin1String(kName))         { r.name = text; }
            else if (field == QLatin1String(kDSPMode))      { r.dspMode = SliceModel::modeFromName(text); }
            else if (field == QLatin1String(kComments))     { r.comments = text; }
            else if (field == QLatin1String(kScan))         { r.scan = boolFrom(text); }
            else if (field == QLatin1String(kTuneStep))     { r.tuneStep = text; }
            else if (field == QLatin1String(kRPTR))         { r.rptr = MemoryRecord::fmTxModeFromName(text); }
            else if (field == QLatin1String(kRPTROffset))   { r.rptrOffsetMHz = text.toDouble(); }
            else if (field == QLatin1String(kCTCSSOn))      { r.ctcssOn = boolFrom(text); }
            else if (field == QLatin1String(kCTCSSFreq))    { r.ctcssFreq = text.toDouble(); }
            else if (field == QLatin1String(kDeviation))    { r.deviation = text.toInt(); }
            else if (field == QLatin1String(kPower))        { r.power = text.toInt(); }
            else if (field == QLatin1String(kSplit))        { r.split = boolFrom(text); }
            else if (field == QLatin1String(kTXFreq))       { r.txFreqMHz = text.toDouble(); }
            else if (field == QLatin1String(kRXFilter))     { r.rxFilter = text; }
            else if (field == QLatin1String(kRXFilterLow))  { r.rxFilterLow = text.toInt(); }
            else if (field == QLatin1String(kRXFilterHigh)) { r.rxFilterHigh = text.toInt(); }
            else if (field == QLatin1String(kAGCMode))      { r.agcMode = MemoryRecord::agcModeFromName(text); }
            else if (field == QLatin1String(kAGCT))         { r.agcT = text.toInt(); }
            else                                            { r.extras.push_back({field, text}); }
        }
        parsed.push_back(r);
    }

    if (x.hasError()) {
        if (error) { *error = x.errorString(); }
        return false;
    }
    if (!sawRoot) {
        if (error) { *error = QStringLiteral("no <MemoryList> root"); }
        return false;
    }

    beginResetModel();
    m_list = parsed;
    m_majorVersion = major;
    m_minorVersion = minor;
    endResetModel();
    emit listChanged();
    return true;
}

bool MemoryList::saveToFile(const QString& path, QString* error) const
{
    // Thetis writes straight into memory.xml (StreamWriter); a crash
    // mid-write would leave half a file. QSaveFile writes beside it and
    // renames, the same rule AdifLog applies to the logbook.
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) { *error = f.errorString(); }
        return false;
    }
    f.write(toXml());
    if (!f.commit()) {
        if (error) { *error = f.errorString(); }
        return false;
    }
    return true;
}

bool MemoryList::loadFromFile(const QString& path, QString* error)
{
    QFile f(path);
    if (!f.exists()) {
        if (error) { *error = QStringLiteral("not found: %1").arg(path); }
        return false;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) { *error = f.errorString(); }
        return false;
    }
    return fromXml(f.readAll(), error);
}

// From Thetis Memory/MemoryList.cs:104-110 [@852bf0e]
//   public void Save() { string file_name = path + "memory.xml"; Save(file_name); }
bool MemoryList::save(const QString& dir, QString* error) const
{
    return saveToFile(filePath(dir), error);
}

// From Thetis Memory/MemoryList.cs:113-165 [@852bf0e]
//   public static MemoryList Restore(): read memory.xml; on success write
//   memory_bak.xml; on any failure fall back to memory_bak.xml; no file
//   at all is an empty list, not an error.
bool MemoryList::restore(const QString& dir, QString* error)
{
    const QString main = filePath(dir);
    const QString bak = backupPath(dir);
    QString err;
    if (loadFromFile(main, &err)) {
        // save backup file
        QString bakErr;
        if (!saveToFile(bak, &bakErr)) {
            qCWarning(lcMemories) << "memory_bak.xml not written:" << bakErr;
        }
        return true;
    }
    if (!QFileInfo::exists(main) && !QFileInfo::exists(bak)) {
        clear();
        return true;   // no memory, no backup
    }
    qCWarning(lcMemories) << "memory.xml unreadable, trying the backup:" << err;
    // check to see if backup file exists
    // if so, try to deserialize it
    if (!QFileInfo::exists(bak)) {
        if (error) { *error = err; }
        return false;
    }
    QString bakErr;
    if (loadFromFile(bak, &bakErr)) { return true; }
    if (error) { *error = err + QStringLiteral(" / backup: ") + bakErr; }
    return false;
}

} // namespace Longpath
