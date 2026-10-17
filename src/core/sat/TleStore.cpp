// SPDX-License-Identifier: GPL-3.0-or-later
// src/core/sat/TleStore.cpp  (Longpath) — see TleStore.h
//
// Longpath-original. No Thetis port.
#include "core/sat/TleStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimeZone>

namespace Longpath {

TleStore::TleStore(QObject* parent)
    : QObject(parent)
    , m_cachePath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                  + QStringLiteral("/tle/amateur.tle"))
    , m_source(defaultSource())
{
}

QUrl TleStore::defaultSource()
{
    return QUrl(QStringLiteral("https://celestrak.org/NORAD/elements/gp.php?GROUP=amateur&FORMAT=tle"));
}

QDateTime TleStore::lastAttemptAt() const
{
    const QFileInfo marker(m_cachePath + QStringLiteral(".attempt"));
    return marker.exists() ? marker.lastModified().toUTC() : QDateTime();
}

void TleStore::markAttempt() const
{
    QDir().mkpath(QFileInfo(m_cachePath).absolutePath());
    QFile marker(m_cachePath + QStringLiteral(".attempt"));
    if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        // Der Inhalt ist nur fuer Menschen; zaehlen tut die Dateizeit.
        marker.write(QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toUtf8());
        marker.close();
    }
}

bool TleStore::looksLikeTle(const QString& text)
{
    // Mindestens ein Zeilenpaar „1 …" / „2 …" — eine HTML-Fehlerseite
    // oder ein „No GP data found" darf nie die Datei ersetzen.
    return text.contains(QLatin1String("\n1 ")) && text.contains(QLatin1String("\n2 "));
}

bool TleStore::loadCached()
{
    QFile f(m_cachePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { return false; }
    const QString text = QString::fromUtf8(f.readAll());
    if (!looksLikeTle(QStringLiteral("\n") + text)) { return false; }
    m_text = text;
    m_fetchedAt = QFileInfo(f).lastModified().toUTC();
    return true;
}

bool TleStore::isStale(int maxAgeHours) const
{
    if (m_text.isEmpty() || !m_fetchedAt.isValid()) { return true; }
    return m_fetchedAt.secsTo(QDateTime::currentDateTimeUtc()) > qint64(maxAgeHours) * 3600;
}

void TleStore::refreshIfStale(int maxAgeHours)
{
    if (!isStale(maxAgeHours)) { return; }
    const QDateTime last = lastAttemptAt();
    if (last.isValid()
        && last.secsTo(QDateTime::currentDateTimeUtc()) < qint64(kRetryHours) * 3600) {
        return;
    }
    refresh();
}

void TleStore::refresh()
{
    if (m_reply) { return; }
    // Siehe TleStore.h: aus einem Pruefstand nie zu CelesTrak.
    if (QStandardPaths::isTestModeEnabled() && m_source == defaultSource()) { return; }
    markAttempt();
    QNetworkRequest req(m_source);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Longpath"));
    req.setTransferTimeout(20000);
    m_reply = m_nam.get(req);
    connect(m_reply, &QNetworkReply::finished, this, &TleStore::onFinished);
}

void TleStore::onFinished()
{
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    if (!reply) { return; }
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit failed(reply->errorString());
        return;
    }
    QString text = QString::fromUtf8(reply->readAll());
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    if (!looksLikeTle(QStringLiteral("\n") + text)) {
        emit failed(QStringLiteral("Die Antwort enthaelt keine Bahndaten"));
        return;
    }
    QDir().mkpath(QFileInfo(m_cachePath).absolutePath());
    QSaveFile f(m_cachePath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(text.toUtf8());
        f.commit();
    }
    m_text = text;
    m_fetchedAt = QDateTime::currentDateTimeUtc();
    emit updated(m_text);
}

} // namespace Longpath
