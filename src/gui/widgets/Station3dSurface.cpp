// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/widgets/Station3dSurface.cpp  (Longpath)
// =================================================================
// Longpath-original. No Thetis port. Siehe Station3dSurface.h.
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "gui/widgets/Station3dSurface.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>

namespace Longpath {

namespace {
Station3dSurface::Factory& factory()
{
    static Station3dSurface::Factory f;
    return f;
}
} // namespace

void Station3dSurface::setFactory(Factory f) { factory() = std::move(f); }
bool Station3dSurface::isAvailable() { return static_cast<bool>(factory()); }

Station3dSurface* Station3dSurface::create(QWidget* parent)
{
    return factory() ? factory()(parent) : nullptr;
}

namespace Station3dPage {

QString cesiumBaseUrl()
{
    return QStringLiteral("https://cdn.jsdelivr.net/npm/cesium@%1/Build/Cesium/")
        .arg(QLatin1String(kCesiumVersion));
}

QString html(const QString& cesiumBase)
{
    QFile f(QStringLiteral(":/flug3d/flug3d.html"));
    if (!f.open(QIODevice::ReadOnly)) { return QString(); }
    QString page = QString::fromUtf8(f.readAll());
    page.replace(QStringLiteral("@@CESIUM_BASE@@"), cesiumBase);
    return page;
}

QString flightScript(const Station3dFlight& fl)
{
    QJsonObject o;
    o.insert(QStringLiteral("call"), fl.call);
    o.insert(QStringLiteral("caption"), fl.caption);
    o.insert(QStringLiteral("lat"), fl.lat);
    o.insert(QStringLiteral("lon"), fl.lon);
    if (fl.hasHome) {
        o.insert(QStringLiteral("home"), QJsonObject{{QStringLiteral("lat"), fl.homeLat},
                                                     {QStringLiteral("lon"), fl.homeLon}});
    }
    if (fl.km >= 0.0)         { o.insert(QStringLiteral("km"), fl.km); }
    if (fl.bearingDeg >= 0.0) { o.insert(QStringLiteral("bearing"), fl.bearingDeg); }
    o.insert(QStringLiteral("key"), fl.apiKey);
    const QString json = QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    return QStringLiteral("window.longpath && window.longpath.fly(%1);").arg(json);
}

QUrl googleEarthWebUrl(double lat, double lon, double headingDeg)
{
    double h = std::fmod(headingDeg, 360.0);
    if (h < 0.0) { h += 360.0; }
    // @Breite,Laenge,Hoehe a, Abstand d, Blickwinkel y, Richtung h,
    // Neigung t, Rollen r -- 600 m Abstand bei 60° Neigung zeigt Haus
    // und Garten, wo Google 3D hat.
    return QUrl(QStringLiteral("https://earth.google.com/web/@%1,%2,0a,600d,35y,%3h,60t,0r")
                    .arg(lat, 0, 'f', 6)
                    .arg(lon, 0, 'f', 6)
                    .arg(h, 0, 'f', 0));
}

} // namespace Station3dPage

} // namespace Longpath
