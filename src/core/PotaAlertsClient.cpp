// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - POTA scheduled-activations client (implementation)
//
// NereusSDR-native, no upstream equivalent. See PotaAlertsClient.h.

#include "PotaAlertsClient.h"
#include "LogCategories.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace Longpath {

PotaAlertsClient::PotaAlertsClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

// Pure data transformation, no network/signals.
QVector<PotaAlert> PotaAlertsClient::parseAlerts(const QByteArray& data)
{
    QVector<PotaAlert> alerts;
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        return alerts;
    }
    for (const auto& val : doc.array()) {
        QJsonObject obj = val.toObject();
        PotaAlert alert;
        alert.activator     = obj.value("activator").toString();
        alert.reference     = obj.value("reference").toString();
        alert.name          = obj.value("name").toString();
        alert.locationDesc  = obj.value("locationDesc").toString();
        alert.startDate     = obj.value("startDate").toString();
        alert.endDate       = obj.value("endDate").toString();
        alert.startTime     = obj.value("startTime").toString();
        alert.endTime       = obj.value("endTime").toString();
        alert.frequencies   = obj.value("frequencies").toString();
        alert.comments      = obj.value("comments").toString();

        if (alert.activator.isEmpty() || alert.reference.isEmpty()) {
            continue;
        }
        alerts.append(alert);
    }
    return alerts;
}

void PotaAlertsClient::fetchAlerts()
{
    QNetworkRequest req{QUrl(ApiUrl)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "Longpath");
    // Same keep-alive workaround as PotaClient/PotaParkInfoClient.
    req.setRawHeader("Connection", "close");
    auto* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcSpots) << "PotaAlertsClient: fetch failed:" << reply->errorString();
            emit alertsError(reply->errorString());
            return;
        }
        emit alertsReceived(parseAlerts(reply->readAll()));
    });
}

} // namespace Longpath
