// SPDX-License-Identifier: GPL-3.0-or-later
// src/core/sat/SatelliteService.cpp  (Longpath) — see SatelliteService.h
//
// Longpath-original. No Thetis port.
#include "core/sat/SatelliteService.h"

#include "core/AppSettings.h"
#include "core/Maidenhead.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcSat, "longpath.sat")

namespace Longpath {

SatelliteService::SatelliteService(QObject* parent)
    : QObject(parent)
{
    connect(&m_store, &TleStore::updated, this, [this](const QString& text) {
        adopt(text);
    });
    connect(&m_store, &TleStore::failed, this, [](const QString& why) {
        qCInfo(lcSat) << "TLE-Abruf fehlgeschlagen:" << why;
    });
    m_daily.setInterval(6 * 3600 * 1000);   // alle 6 h nachsehen, ob >24 h alt
    connect(&m_daily, &QTimer::timeout, this, [this]() { m_store.refreshIfStale(24); });
}

void SatelliteService::start()
{
    if (m_store.loadCached()) {
        adopt(m_store.text());
    }
    m_store.refreshIfStale(24);
    m_daily.start();
}

void SatelliteService::setTleText(const QString& text)
{
    adopt(text);
}

void SatelliteService::adopt(const QString& text)
{
    int rejected = 0;
    const int n = m_tracker.loadTle(text, &rejected);
    qCInfo(lcSat) << "TLE-Satz:" << n << "Satelliten," << rejected << "verworfen,"
                  << "aelteste Epoche" << m_tracker.oldestEpoch().toString(Qt::ISODate);
    emit catalogChanged();
}

bool SatelliteService::observerFor(const QString& gridIn, Observer* out) const
{
    QString grid = gridIn.trimmed().toUpper();
    if (!isValidGridSquare(grid)) {
        grid = AppSettings::instance()
                   .value(QStringLiteral("StationGridSquare"), QString{})
                   .toString().trimmed().toUpper();
    }
    if (!isValidGridSquare(grid)) { return false; }
    double lat = 0.0, lon = 0.0;
    calculateLatLonFromGridSquare(grid, lat, lon);
    if (out) { *out = Observer{lat, lon, 0.0}; }
    return true;
}

QVector<SatelliteSight> SatelliteService::inView(const QDateTime& utc, const QString& grid,
                                                 double minElevationDeg) const
{
    Observer obs;
    if (!observerFor(grid, &obs)) { return {}; }
    return m_tracker.inView(utc, obs, minElevationDeg);
}

QString SatelliteService::describe(const QVector<SatelliteSight>& sights, int maxCount)
{
    QStringList parts;
    for (const SatelliteSight& s : sights) {
        if (parts.size() >= maxCount) { break; }
        parts << QStringLiteral("%1 el %2° az %3°")
                     .arg(s.name)
                     .arg(qRound(s.elevationDeg))
                     .arg(qRound(s.azimuthDeg));
    }
    return parts.join(QStringLiteral(" · "));
}

QString SatelliteService::stamp(const QDateTime& utc, const QString& grid) const
{
    return describe(inView(utc, grid, 0.0));
}

} // namespace Longpath
