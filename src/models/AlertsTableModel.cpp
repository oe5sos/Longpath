// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - AlertsTableModel implementation. See AlertsTableModel.h.
//
// NereusSDR-native, no upstream equivalent.

#include "AlertsTableModel.h"

#include <QColor>

namespace Longpath {

QVariant AlertsTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_alerts.size()) {
        return {};
    }
    const auto& a = m_alerts[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColActivator:   return a.activator;
        case ColReference:   return a.reference;
        case ColName:        return a.name;
        case ColLocation:    return a.locationDesc;
        case ColStart:       return QString("%1 %2 UTC").arg(a.startDate, a.startTime);
        case ColEnd:         return QString("%1 %2 UTC").arg(a.endDate, a.endTime);
        case ColFrequencies: return a.frequencies;
        case ColComments:    return a.comments;
        }
    }
    if (role == Qt::ForegroundRole && index.column() == ColActivator) {
        return QColor(0x00, 0xb4, 0xd8);  // same accent as SpotTableModel::ColDxCall
    }
    return {};
}

QVariant AlertsTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
    case ColActivator:   return "Activator";
    case ColReference:   return "Ref";
    case ColName:        return "Park";
    case ColLocation:    return "Location";
    case ColStart:       return "Start";
    case ColEnd:         return "End";
    case ColFrequencies: return "Frequencies";
    case ColComments:    return "Comments";
    }
    return {};
}

void AlertsTableModel::setAlerts(const QVector<PotaAlert>& alerts)
{
    beginResetModel();
    m_alerts = alerts;
    endResetModel();
}

} // namespace Longpath
