// =================================================================
// src/core/CloudlogUploader.cpp  (Longpath)
// =================================================================
//
// Longpath-original — see CloudlogUploader.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-11-17 — parseResponse against the servers' own source, see
//                 the header. AI-assisted via Anthropic Claude Code,
//                 operator Martin Fischer.
// =================================================================

#include "CloudlogUploader.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>

namespace Longpath {

namespace {
constexpr int kTimeoutMs = 20000;
}

CloudlogUploader::CloudlogUploader(QObject* parent) : QsoUploader(parent) {}

QString CloudlogUploader::serviceName() const
{
    // Wavelog first: it is the fork operators run today, and the one
    // this was last checked against.
    return QStringLiteral("Wavelog / Cloudlog");
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
    if (!entry.isValid()) {
        emit uploadFinished(call, false, false,
            QStringLiteral("Nothing to upload"));
        return;
    }
    if (!isConfigured()) {
        emit uploadFinished(call, false, false,
            QStringLiteral("Wavelog is not set up yet"));
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
    // Without one a server that accepts the connection and then says
    // nothing holds this contact — and everything queued behind it —
    // for as long as the program runs.
    req.setTransferTimeout(kTimeoutMs);

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

namespace {

// "Differing locator <b>JN68</b> while importing …: SKIPPED<br>" — the
// messages are written for a web page. Tags out, entities back.
QString plainText(QString html)
{
    static const QRegularExpression tag(QStringLiteral("<[^>]*>"));
    html.replace(tag, QString{});
    html.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    html.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    html.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    html.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    return html.simplified();
}

} // namespace

bool CloudlogUploader::isDuplicateMessage(const QString& message)
{
    // Wavelog, Logbook_model::import():
    //   "Date/Time: <t> Callsign: <c> Band: <b> " . __("Duplicate for") . " <own call><br>"
    // Only the frame is fixed; see the header.
    static const QRegularExpression frame(
        QStringLiteral("^\\s*Date/Time: .+ Callsign: .+ Band: "));
    return frame.match(message).hasMatch()
           || message.contains(QStringLiteral("dupl"), Qt::CaseInsensitive);
}

// GEGENPROBE: der alte Rumpf von main.
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
