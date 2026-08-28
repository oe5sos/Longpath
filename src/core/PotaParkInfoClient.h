// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - POTA (Parks On The Air) park-info lookup client
//
// NereusSDR-native, no upstream equivalent (neither Thetis nor
// AetherSDR have a POTA integration at all -- see PotaClient.h). This
// is a sibling of PotaClient: where PotaClient polls
// https://api.pota.app/spot/activator continuously for live
// activations, this class does a single on-demand GET to
// https://api.pota.app/park/{reference} when the operator asks "what
// is this park?" from the Spot List's right-click menu (2026-08-27,
// operator-requested follow-up to the SpotHub POTA improvement pass).
// Endpoint and field names verified live against a real park
// reference (US-4558) before writing this, same diligence as
// PotaClient's spot endpoint.
//
// Modification history (NereusSDR):
//   2026-08-27  AI (Anthropic Claude Code)  Initial version.

#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QString>
#include <optional>

namespace Longpath {

// Fields present in the api.pota.app/park/{reference} response.
// Optional/nullable API fields not needed by ParkInfoDialog (parkId,
// parktypeId, accessibility, sensitivity, parkURLs, createdByAdmin,
// entityId, entityDeleted) are intentionally not carried here.
struct PotaParkInfo {
    QString reference;
    QString name;
    double  latitude{0.0};
    double  longitude{0.0};
    QString grid6;
    QString parktypeDesc;    // e.g. "National Scenic Trail"
    QString parkComments;
    QString accessMethods;      // comma-separated, e.g. "Automobile,Foot,Other"
    QString activationMethods;  // comma-separated
    QString agencies;
    QString agencyURLs;
    QString website;
    QString locationDesc;    // comma-separated state/province codes, e.g. "US-CO,US-ID"
    QString locationName;    // comma-separated full names
    QString entityName;      // country, e.g. "United States of America"
    QString referencePrefix; // e.g. "US"
    QString firstActivator;
    QString firstActivationDate;
    bool    active{true};
};

// One-shot fetcher: fetchParkInfo() issues a single GET and emits
// exactly one of parkInfoReceived() / parkInfoError() per call. No
// polling, no state carried between calls (unlike PotaClient).
class PotaParkInfoClient : public QObject {
    Q_OBJECT

public:
    explicit PotaParkInfoClient(QObject* parent = nullptr);

    void fetchParkInfo(const QString& reference);

    // Public test seam, mirrors PotaClient::parseJsonForTest. Pure
    // data transformation: no network, no signals. Returns
    // std::nullopt if `data` isn't a JSON object or has no
    // `reference` field (park not found).
    static std::optional<PotaParkInfo> parseParkInfoForTest(const QByteArray& data) {
        return parseParkInfo(data);
    }

signals:
    void parkInfoReceived(const PotaParkInfo& info);
    void parkInfoError(const QString& reference, const QString& error);

private:
    static std::optional<PotaParkInfo> parseParkInfo(const QByteArray& data);

    QNetworkAccessManager* m_nam;

    static constexpr const char* ApiUrlTemplate = "https://api.pota.app/park/%1";
};

} // namespace Longpath
