// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - AlertsTableModel: QAbstractTableModel wrapping a
// QVector<PotaAlert> for the SpotHub Alerts tab.
//
// NereusSDR-native, no upstream equivalent (operator-requested
// follow-up to the SpotHub POTA improvement pass, 2026-08-27).
// Unlike SpotTableModel (a bounded, ever-growing ring fed by live
// spot signals), this model is replaced wholesale on every
// PotaAlertsClient::alertsReceived() -- the fetched list already IS
// the complete current state of scheduled activations, there is
// nothing to dedup or age out incrementally.
//
// Modification history (NereusSDR):
//   2026-08-27  AI (Anthropic Claude Code)  Initial version.

#pragma once

#include <QAbstractTableModel>
#include <QVector>

#include "core/PotaAlertsClient.h"

namespace Longpath {

class AlertsTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { ColActivator, ColReference, ColName, ColLocation, ColStart, ColEnd, ColFrequencies, ColComments, ColCount };

    explicit AlertsTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex& = {}) const override { return m_alerts.size(); }
    int columnCount(const QModelIndex& = {}) const override { return ColCount; }
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    // Wholesale replace -- see class comment above.
    void setAlerts(const QVector<PotaAlert>& alerts);

private:
    QVector<PotaAlert> m_alerts;
};

} // namespace Longpath
