// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - SOTA HTTPS spot poller (implementation). See SotaClient.h.
//
// Modification history (NereusSDR):
//   2026-09-03  AI (Anthropic Claude Code)  Original.

#include "SotaClient.h"
#include "AppSettings.h"
#include "LogCategories.h"
#include "core/Maidenhead.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>

namespace Longpath {

SotaClient::SotaClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setSingleShot(false);
    connect(m_pollTimer, &QTimer::timeout, this, &SotaClient::onPollTimer);
}

SotaClient::~SotaClient()
{
    stopPolling();
    m_logFile.close();
}

QString SotaClient::logFilePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
           + "/sota.log";
}

void SotaClient::startPolling(int intervalSec)
{
    if (m_polling) {
        return;
    }

    // SOTA's own published rule: no more than one request every 60
    // seconds. PotaClient leaves its interval floor to the UI spinbox;
    // this one is enforced here, because the request is being made in
    // this operator's name (the User-Agent below carries his callsign)
    // and a client that quietly ignores the limit is how a shared API
    // stops being offered to everyone else.
    if (intervalSec < kMinIntervalSec) {
        intervalSec = kMinIntervalSec;
    }

    qCDebug(lcSpots) << "SotaClient: starting polling every" << intervalSec << "sec";

    m_logFile.close();
    m_logFile.setFileName(logFilePath());
    QDir().mkpath(QFileInfo(m_logFile).absolutePath());
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        m_logFile.write(QString("--- SOTA polling started at %1 (every %2s) ---\n")
            .arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss UTC"))
            .arg(intervalSec).toUtf8());
        m_logFile.flush();
    }

    m_seenSpotIds.clear();
    m_lastEpoch.clear();
    m_polling = true;
    m_pollTimer->start(intervalSec * 1000);
    onPollTimer();  // immediate first poll
    emit started();
}

void SotaClient::stopPolling()
{
    if (!m_polling) {
        return;
    }
    m_pollTimer->stop();
    m_polling = false;
    emit stopped();
}

// From the SOTA API guidance: reference is either a dedicated ADIF-style
// field (this class only reads JSON, so that distinction is moot here)
// or, in the wire format, "summitCode" (e.g. "DL/MF-117"). entity is
// the association prefix before the '/', mirroring how PotaClient
// derives POTA's entity from the part before the '-' in "US-4558".
QVector<DxSpot> SotaClient::parseAndCollect(const QByteArray& data)
{
    QVector<DxSpot> newSpots;

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        return newSpots;
    }

    QJsonArray arr = doc.array();
    for (const auto& val : arr) {
        QJsonObject obj = val.toObject();

        const int spotId = obj.value("id").toInt();
        if (m_seenSpotIds.contains(spotId)) {
            continue;
        }
        m_seenSpotIds.insert(spotId);

        DxSpot spot;
        spot.dxCall      = obj.value("activatorCallsign").toString();
        spot.spotterCall = obj.value("callsign").toString();
        spot.source      = QStringLiteral("SOTA");

        // Already MHz on this API, unlike POTA's kHz string -- but
        // frequency can legally be null (no radio info given yet), so
        // this must not blindly call toDouble() on a JSON null.
        const QJsonValue freqVal = obj.value("frequency");
        spot.freqMhz = freqVal.isDouble() ? freqVal.toDouble() : 0.0;

        // SOTA spots carry no expiry field the way POTA's "expire"
        // does; lifetimeSec stays 0 so the caller falls back to its own
        // AppSettings default, same as every other source without one
        // (see DxSpot.h).

        QString sotaColor =
            AppSettings::instance().value("SotaSpotColor", "#c2924f").toString();
        if (sotaColor.length() == 7) {
            sotaColor = "#FF" + sotaColor.mid(1);
        }
        spot.color = sotaColor;

        const QString ref = obj.value("summitCode").toString();
        spot.reference = ref;
        spot.entity = ref.section('/', 0, 0);

        // The API gives lat/lon, not a locator directly -- POTA's grid6
        // needed no conversion, this does. Same helper the logbook and
        // FreeDV Reporter already use for exactly this (Maidenhead.h).
        const QJsonValue latVal = obj.value("latitude");
        const QJsonValue lonVal = obj.value("longitude");
        if (latVal.isDouble() && lonVal.isDouble()) {
            spot.grid = gridSquareFromLatLon(latVal.toDouble(), lonVal.toDouble());
        }

        // Build comment: the operator's real spot note when present,
        // else the summit name as a fallback so the column isn't blank
        // -- same rule PotaClient uses for its park-name fallback. Mode
        // is appended so SpotTableModel::extractMode (reads the first/
        // last word of `comment`) keeps working unchanged.
        const QString summit = obj.value("summitName").toString();
        const QString mode   = obj.value("mode").toString();
        const QString note   = obj.value("comments").toString().trimmed();
        spot.comment = note.isEmpty() ? summit : note;
        if (!mode.isEmpty()) {
            spot.comment += " " + mode;
        }

        const QString timeStr = obj.value("timeStamp").toString();
        QDateTime dt = QDateTime::fromString(timeStr, Qt::ISODateWithMs);
        if (!dt.isValid()) {
            dt = QDateTime::fromString(timeStr, Qt::ISODate);
        }
        spot.utcTime = dt.isValid() ? dt.toUTC().time()
                                     : QDateTime::currentDateTimeUtc().time();

        if (spot.freqMhz <= 0.0 || spot.dxCall.isEmpty()) {
            continue;
        }

        newSpots.append(spot);
    }

    return newSpots;
}

void SotaClient::fetchSpots()
{
    QNetworkRequest req{QUrl{QString::fromLatin1(SpotsUrl)}};
    const QString callsign =
        AppSettings::instance().value("StationCallsign", QString()).toString();
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  callsign.isEmpty() ? QStringLiteral("Longpath")
                                     : QStringLiteral("Longpath (%1)").arg(callsign));
    req.setRawHeader("Connection", "close");   // see PotaClient.cpp for why
    auto* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString();
            qCWarning(lcSpots) << "SotaClient: spot fetch failed:" << err;
            emit pollError(err);
            return;
        }

        const QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isArray()) {
            emit pollError(QStringLiteral("Invalid JSON response"));
            return;
        }

        const int total = doc.array().size();
        const QVector<DxSpot> newSpots = parseAndCollect(data);

        for (const DxSpot& spot : newSpots) {
            const QString logLine = QString("%1  %2  %3 MHz  %4  %5")
                .arg(spot.utcTime.toString("HH:mm"),
                     spot.dxCall,
                     QString::number(spot.freqMhz, 'f', 4),
                     spot.reference,
                     spot.comment);
            if (m_logFile.isOpen()) {
                m_logFile.write((logLine + "\n").toUtf8());
                m_logFile.flush();
            }
            emit rawLineReceived(logLine);
            emit spotReceived(spot);
        }

        emit pollComplete(total, newSpots.size());
    });
}

void SotaClient::onPollTimer()
{
    // The epoch check the maintainer asked every client to do: a bare
    // GUID string, cheap next to the full spot list, so a poll that
    // finds nothing new costs one small request instead of a real
    // fetch every time.
    QNetworkRequest req{QUrl{QString::fromLatin1(EpochUrl)}};
    const QString callsign =
        AppSettings::instance().value("StationCallsign", QString()).toString();
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  callsign.isEmpty() ? QStringLiteral("Longpath")
                                     : QStringLiteral("Longpath (%1)").arg(callsign));
    req.setRawHeader("Connection", "close");
    auto* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString();
            qCWarning(lcSpots) << "SotaClient: epoch check failed:" << err;
            emit pollError(err);
            return;
        }

        const QString epoch = QString::fromUtf8(reply->readAll()).trimmed();
        if (!epoch.isEmpty() && epoch == m_lastEpoch) {
            // Nothing changed since the last fetch -- exactly the case
            // this check exists to catch. Still counts as a completed,
            // empty poll so a status display doesn't read as stuck.
            emit pollComplete(0, 0);
            return;
        }
        m_lastEpoch = epoch;
        fetchSpots();
    });
}

} // namespace Longpath
