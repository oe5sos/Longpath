// =================================================================
// src/core/CloudlogUploader.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — see CloudlogUploader.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "CloudlogUploader.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace Longpath {

CloudlogUploader::CloudlogUploader(QObject* parent) : QsoUploader(parent) {}

QString CloudlogUploader::serviceName() const
{
    return QStringLiteral("Cloudlog / Wavelog");
}

void CloudlogUploader::setBaseUrl(const QString& url)
{
    m_baseUrl = url.trimmed();
}

void CloudlogUploader::setApiKey(const QString& key)
{
    m_apiKey = key.trimmed();
}

void CloudlogUploader::setStationProfileId(const QString& id)
{
    m_stationId = id.trimmed();
}

QString CloudlogUploader::qsoEndpoint(const QString& base)
{
    QString b = base.trimmed();
    if (b.isEmpty()) { return {}; }

    while (b.endsWith(QLatin1Char('/'))) { b.chop(1); }

    // Operators paste what the browser shows, which may already include
    // any of these tails. Stripping them is cheaper than a support
    // conversation about which form is "correct".
    static const QStringList tails = {
        QStringLiteral("/index.php/api/qso"),
        QStringLiteral("/api/qso"),
        QStringLiteral("/index.php"),
    };
    for (const QString& t : tails) {
        if (b.endsWith(t, Qt::CaseInsensitive)) {
            b.chop(t.size());
            break;
        }
    }
    while (b.endsWith(QLatin1Char('/'))) { b.chop(1); }

    if (b.isEmpty()) { return {}; }

    // Default to https when no scheme was given. An API key sent in
    // clear over http would be handing it to the network.
    if (!b.contains(QStringLiteral("://"))) {
        b = QStringLiteral("https://") + b;
    }
    return b + QStringLiteral("/index.php/api/qso");
}

void CloudlogUploader::upload(const LogEntry& entry)
{
    const QString call = entry.call;
    if (!isConfigured()) {
        emit uploadFinished(call, false, false,
            QStringLiteral("Cloudlog is not set up yet"));
        return;
    }

    const QString endpoint = qsoEndpoint(m_baseUrl);
    const QUrl url(endpoint);
    if (!url.isValid()) {
        emit uploadFinished(call, false, false,
            QStringLiteral("That doesn't look like a valid address: %1")
                .arg(endpoint));
        return;
    }

    // The record is LogEntry's ADIF, not a second serialisation — a
    // contact must be spelled the same everywhere it is sent.
    QJsonObject body;
    body.insert(QStringLiteral("key"), m_apiKey);
    body.insert(QStringLiteral("station_profile_id"), m_stationId);
    body.insert(QStringLiteral("type"), QStringLiteral("adif"));
    body.insert(QStringLiteral("string"), entry.toAdifRecord());

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply =
        m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply, call]() {
        reply->deleteLater();
        const int status = reply
            ->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray payload = reply->readAll();

        if (reply->error() != QNetworkReply::NoError && status == 0) {
            // No HTTP status at all: the request never reached a server.
            emit uploadFinished(call, false, false, reply->errorString());
            return;
        }
        const Result r = parseResponse(status, payload);
        emit uploadFinished(call, r.ok, r.duplicate, r.message);
    });
}

CloudlogUploader::Result CloudlogUploader::parseResponse(int httpStatus,
                                                         const QByteArray& body)
{
    Result r;
    const QString text = QString::fromUtf8(body).trimmed();

    // Cloudlog answers with JSON on success and, depending on version and
    // on what the web server did to the request, HTML or plain text on
    // failure. So the status code decides, and the body only supplies
    // the explanation.
    QString message = text.left(300);

    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isObject()) {
        const QJsonObject o = doc.object();
        // Field names differ between Cloudlog and Wavelog releases;
        // take whichever is present rather than insisting on one.
        for (const char* k : {"reason", "message", "status", "error"}) {
            const QString v = o.value(QLatin1String(k)).toString();
            if (!v.isEmpty()) { message = v; break; }
        }
    }

    if (httpStatus >= 200 && httpStatus < 300) {
        r.ok = true;
        // "Duplicate" comes back as a successful post that says so.
        // Re-uploading a contact the log already has is the normal
        // result of a retry, not something to alarm the operator with.
        r.duplicate = message.contains(QStringLiteral("dupl"),
                                       Qt::CaseInsensitive);
        r.message = message.isEmpty() ? QStringLiteral("accepted") : message;
        return r;
    }

    if (httpStatus == 401 || httpStatus == 403) {
        r.message = QStringLiteral("Cloudlog rejected the API key");
    } else if (httpStatus == 404) {
        r.message = QStringLiteral("No API at that address — check the "
                                   "instance URL");
    } else if (message.isEmpty()) {
        r.message = QStringLiteral("HTTP %1").arg(httpStatus);
    } else {
        r.message = message;
    }
    return r;
}

} // namespace Longpath
