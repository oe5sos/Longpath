// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/widgets/GibsTileLayer.cpp  (Longpath)
// =================================================================
//
// Longpath-original. No Thetis port. See GibsTileLayer.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "GibsTileLayer.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

Q_LOGGING_CATEGORY(lcGibs, "nereus.map.gibs")

// Ein Bildpunkt auf Stufe 0: 288° Kachel / 512 px. Aus den
// GIBS-Capabilities (ScaleDenominator 223632905.6 bei 0,28 mm/px).
constexpr double kDegPerPixelLevel0 = 0.5625;

constexpr const char* kBase = "https://gibs.earthdata.nasa.gov/wmts/epsg4326/best/";
constexpr const char* kBlueMarble = "BlueMarble_ShadedRelief_Bathymetry";
constexpr const char* kLandsat =
    "Landsat_WELD_CorrectedReflectance_TrueColor_Global_Annual";
// Das juengste Jahreskomposit, das GIBS fuehrt (Default der Time-Dimension).
constexpr const char* kLandsatTime = "2000-12-01";

} // namespace

// ── Raster ──────────────────────────────────────────────────────────

double GibsTileLayer::tileSpanDeg(int level)
{
    return kDegPerPixelLevel0 * kTilePx / std::ldexp(1.0, level);
}

double GibsTileLayer::degPerPixel(int level)
{
    return kDegPerPixelLevel0 / std::ldexp(1.0, level);
}

int GibsTileLayer::levelForDegPerPixel(double screenDegPerPx)
{
    if (!(screenDegPerPx > 0.0)) { return 0; }
    // Kleinste Stufe, deren Bildpunkt <= Bildschirmpunkt ist: dann wird
    // eine Kachel hoechstens verkleinert, nie aufgeblasen.
    const double raw = std::log2(kDegPerPixelLevel0 / screenDegPerPx);
    const int level = static_cast<int>(std::ceil(raw - 1e-9));
    return std::clamp(level, 0, kLandsatMax);
}

QVector<GibsTileLayer::TileId>
GibsTileLayer::tilesFor(int level, double lonMin, double lonMax,
                        double latMin, double latMax)
{
    QVector<TileId> out;
    level = std::clamp(level, 0, kLandsatMax);
    lonMin = std::clamp(lonMin, -180.0, 180.0);
    lonMax = std::clamp(lonMax, -180.0, 180.0);
    latMin = std::clamp(latMin, -90.0, 90.0);
    latMax = std::clamp(latMax, -90.0, 90.0);
    if (lonMax <= lonMin || latMax <= latMin) { return out; }

    const double span = tileSpanDeg(level);
    const int cols = static_cast<int>(std::ceil(360.0 / span));
    const int rows = static_cast<int>(std::ceil(180.0 / span));

    // Ein Ausschnitt, der genau auf einer Kachelkante endet, braucht
    // die Kachel dahinter nicht — daher das Epsilon nach innen.
    const double eps = span * 1e-9;
    const int c0 = std::clamp(int(std::floor((lonMin + 180.0) / span)), 0, cols - 1);
    const int c1 = std::clamp(int(std::floor((lonMax + 180.0 - eps) / span)), 0, cols - 1);
    const int r0 = std::clamp(int(std::floor((90.0 - latMax) / span)), 0, rows - 1);
    const int r1 = std::clamp(int(std::floor((90.0 - latMin - eps) / span)), 0, rows - 1);

    const bool landsat = usesLandsat(level);
    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            out.append(TileId{level, c, r, landsat});
        }
    }
    return out;
}

QRectF GibsTileLayer::tileBoundsDeg(const TileId& id)
{
    const double span = tileSpanDeg(id.level);
    const double left = -180.0 + id.col * span;
    const double top  =   90.0 - id.row * span;
    // y waechst nach Sueden: top ist die Nordkante, Hoehe positiv.
    return QRectF(left, top, span, span);
}

QUrl GibsTileLayer::urlFor(const TileId& id)
{
    // {layer}/default/[{time}/]{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.jpeg
    QString s = QString::fromLatin1(kBase);
    if (id.landsat) {
        s += QString::fromLatin1(kLandsat) + QStringLiteral("/default/")
           + QString::fromLatin1(kLandsatTime) + QStringLiteral("/31.25m/");
    } else {
        s += QString::fromLatin1(kBlueMarble) + QStringLiteral("/default/500m/");
    }
    s += QStringLiteral("%1/%2/%3.jpeg").arg(id.level).arg(id.row).arg(id.col);
    return QUrl(s);
}

QString GibsTileLayer::cacheKey(const TileId& id)
{
    return QStringLiteral("%1/%2/%3/%4")
        .arg(id.landsat ? QLatin1Char('L') : QLatin1Char('B'))
        .arg(id.level).arg(id.row).arg(id.col);
}

QString GibsTileLayer::attribution()
{
    return QStringLiteral("Imagery: NASA EOSDIS GIBS — Blue Marble, Landsat WELD");
}

// ── Bilder ──────────────────────────────────────────────────────────

GibsTileLayer::GibsTileLayer(QObject* parent)
    : QObject(parent), m_memory(kMemoryTiles)
{
    m_nam  = new QNetworkAccessManager(this);
    m_disk = new QNetworkDiskCache(this);
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                      + QStringLiteral("/gibs-tiles");
    QDir().mkpath(dir);
    m_disk->setCacheDirectory(dir);
    m_disk->setMaximumCacheSize(kDiskCacheBytes);
    m_nam->setCache(m_disk);
}

GibsTileLayer::~GibsTileLayer() = default;

QImage GibsTileLayer::tile(const TileId& id)
{
    const QString key = cacheKey(id);
    if (const QImage* hit = m_memory.object(key)) { return *hit; }

    if (!m_network || m_inFlight.contains(key)) { return {}; }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (const auto it = m_failedAtMs.constFind(key); it != m_failedAtMs.constEnd()) {
        if (now - it.value() < kRetryAfterMs) { return {}; }
    }
    // Schon in der Schlange? Dann nicht noch einmal.
    for (const TileId& q : m_queue) {
        if (q == id) { return {}; }
    }
    m_queue.push_back(id);
    pump();
    return {};
}

QImage GibsTileLayer::cached(const TileId& id) const
{
    if (const QImage* hit = m_memory.object(cacheKey(id))) { return *hit; }
    return {};
}

void GibsTileLayer::putForTest(const TileId& id, const QImage& img)
{
    m_memory.insert(cacheKey(id), new QImage(img));
}

void GibsTileLayer::pump()
{
    while (m_inFlight.size() < kMaxInFlight && !m_queue.empty()) {
        // Die juengste Anfrage zuerst: sie gehoert zum aktuellen
        // Ausschnitt. Was beim Fliegen liegen blieb, kommt danach.
        const TileId id = m_queue.back();
        m_queue.pop_back();
        const QString key = cacheKey(id);
        if (m_memory.contains(key) || m_inFlight.contains(key)) { continue; }

        QNetworkRequest req(urlFor(id));
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Longpath/%1")
                          .arg(QCoreApplication::applicationVersion()));
        req.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::PreferCache);
        req.setAttribute(QNetworkRequest::CacheSaveControlAttribute, true);
        req.setTransferTimeout(15'000);

        m_inFlight.insert(key);
        QNetworkReply* reply = m_nam->get(req);
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, id]() { onReplyFinished(reply, id); });
    }
}

void GibsTileLayer::onReplyFinished(QNetworkReply* reply, const TileId& id)
{
    reply->deleteLater();
    const QString key = cacheKey(id);
    m_inFlight.remove(key);

    bool ok = reply->error() == QNetworkReply::NoError;
    QImage img;
    if (ok) {
        img.loadFromData(reply->readAll());
        ok = !img.isNull();
    }
    if (ok) {
        m_memory.insert(key, new QImage(img));
        m_failedAtMs.remove(key);
        emit tileReady();
    } else {
        m_failedAtMs.insert(key, QDateTime::currentMSecsSinceEpoch());
        qCDebug(lcGibs) << "GIBS tile" << key << "failed:" << reply->errorString();
    }
    pump();
}

} // namespace Longpath
