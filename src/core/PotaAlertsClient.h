// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - POTA (Parks On The Air) scheduled-activations ("Alerts")
// client
//
// NereusSDR-native, no upstream equivalent (operator-requested
// follow-up to the SpotHub POTA improvement pass, 2026-08-27).
// Sibling of PotaClient/PotaParkInfoClient: where PotaClient polls
// live activator spots and PotaParkInfoClient looks up one park on
// demand, this class fetches the full list of *future* scheduled
// activations from https://api.pota.app/activation -- POTA's
// counterpart to SOTA's "Announcements" / SOTAwatch3's
// "ANKÜNDIGUNGEN" tab. Endpoint and field names verified live before
// writing this (same diligence as the other two POTA clients).
//
// Unlike PotaClient, this does not poll continuously: activations are
// scheduled days to weeks ahead, so sub-minute freshness has no
// value, and nothing else in the app consumes this data (no
// panadapter overlay, no shared table) -- a manual "Refresh" plus a
// fetch on tab-open is enough. See SpotHubDialog::buildAlertsTab().
//
// Modification history (NereusSDR):
//   2026-08-27  AI (Anthropic Claude Code)  Initial version.

#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QString>
#include <QVector>

namespace Longpath {

// One row of https://api.pota.app/activation.
struct PotaAlert {
    QString activator;      // callsign, may carry a /P-style suffix
    QString reference;
    QString name;            // park name
    QString locationDesc;    // e.g. "US-KY"
    QString startDate;       // "YYYY-MM-DD"
    QString endDate;
    QString startTime;       // "HH:MM" (UTC, per the web UI's "... UTC" suffix)
    QString endTime;
    QString frequencies;     // free text, e.g. "20 meter", "all", "" (unset)
    QString comments;
};

class PotaAlertsClient : public QObject {
    Q_OBJECT

public:
    explicit PotaAlertsClient(QObject* parent = nullptr);

    void fetchAlerts();

    // Public test seam, mirrors PotaClient::parseJsonForTest /
    // PotaParkInfoClient::parseParkInfoForTest.
    static QVector<PotaAlert> parseAlertsForTest(const QByteArray& data) {
        return parseAlerts(data);
    }

signals:
    void alertsReceived(const QVector<PotaAlert>& alerts);
    void alertsError(const QString& error);

private:
    static QVector<PotaAlert> parseAlerts(const QByteArray& data);

    QNetworkAccessManager* m_nam;

    static constexpr const char* ApiUrl = "https://api.pota.app/activation";
};

} // namespace Longpath
