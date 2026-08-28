// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - Park Info dialog: shows the fields fetched by
// PotaParkInfoClient for one POTA park reference.
//
// NereusSDR-native, no upstream equivalent (operator-requested
// follow-up to the SpotHub POTA improvement pass, 2026-08-27).
// Triggered from the Spot List tab's right-click menu ("Park Info:
// <ref>") whenever the selected row carries a non-empty
// DxSpot::reference. A single instance is reused across lookups
// (SpotHubDialog owns it) rather than spawning a new dialog per
// click; showLoading()/showInfo()/showError() swap its content in
// place.
//
// Modification history (NereusSDR):
//   2026-08-27  AI (Anthropic Claude Code)  Initial version.

#pragma once

#include <QDialog>

#include "core/PotaParkInfoClient.h"

class QLabel;

namespace Longpath {

class ParkInfoDialog : public QDialog {
    Q_OBJECT

public:
    explicit ParkInfoDialog(QWidget* parent = nullptr);

    // Clears the dialog to a "Fetching <reference>..." placeholder and
    // shows it. Call before firing the PotaParkInfoClient request.
    void showLoading(const QString& reference);
    void showInfo(const PotaParkInfo& info);
    void showError(const QString& reference, const QString& error);

private:
    QLabel* m_titleLabel{nullptr};
    QLabel* m_bodyLabel{nullptr};
};

} // namespace Longpath
