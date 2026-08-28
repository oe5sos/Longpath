// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - POTA park-info lookup client (implementation)
//
// NereusSDR-native, no upstream equivalent. See PotaParkInfoClient.h.

#include "PotaParkInfoClient.h"
#include "LogCategories.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace Longpath {

PotaParkInfoClient::PotaParkInfoClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

// Pure data transformation, no network/signals -- see the header's
// parseParkInfoForTest() test seam, mirroring PotaClient's
// parseAndCollect()/parseJsonForTest() split.
std::optional<PotaParkInfo> PotaParkInfoClient::parseParkInfo(const QByteArray& data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return std::nullopt;
    }

    QJsonObject obj = doc.object();
    PotaParkInfo info;
    info.reference           = obj.value("reference").toString();
    info.name                = obj.value("name").toString();
    info.latitude            = obj.value("latitude").toDouble();
    info.longitude           = obj.value("longitude").toDouble();
    info.grid6               = obj.value("grid6").toString();
    info.parktypeDesc        = obj.value("parktypeDesc").toString();
    info.parkComments        = obj.value("parkComments").toString();
    info.accessMethods       = obj.value("accessMethods").toString();
    info.activationMethods   = obj.value("activationMethods").toString();
    info.agencies            = obj.value("agencies").toString();
    info.agencyURLs          = obj.value("agencyURLs").toString();
    info.website              = obj.value("website").toString();
    info.locationDesc        = obj.value("locationDesc").toString();
    info.locationName        = obj.value("locationName").toString();
    info.entityName          = obj.value("entityName").toString();
    info.referencePrefix     = obj.value("referencePrefix").toString();
    info.firstActivator      = obj.value("firstActivator").toString();
    info.firstActivationDate = obj.value("firstActivationDate").toString();
    info.active               = obj.value("active").toInt(1) != 0;

    if (info.reference.isEmpty()) {
        return std::nullopt;
    }
    return info;
}

void PotaParkInfoClient::fetchParkInfo(const QString& reference)
{
    QNetworkRequest req{QUrl(QString(ApiUrlTemplate).arg(reference))};
    req.setHeader(QNetworkRequest::UserAgentHeader, "Longpath");
    // Same keep-alive workaround as PotaClient::onPollTimer -- avoids
    // the known Qt6/macOS CFSocket-after-idle-close crash. This is a
    // one-shot request, so the extra TCP handshake cost is negligible.
    req.setRawHeader("Connection", "close");
    auto* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, reference] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcSpots) << "PotaParkInfoClient: lookup failed for" << reference
                               << ":" << reply->errorString();
            emit parkInfoError(reference, reply->errorString());
            return;
        }

        auto info = parseParkInfo(reply->readAll());
        if (!info) {
            emit parkInfoError(reference, QStringLiteral("Park not found"));
            return;
        }
        emit parkInfoReceived(*info);
    });
}

} // namespace Longpath
