// SPDX-License-Identifier: GPL-3.0-or-later
// src/gui/widgets/BlueMarbleFetcher.cpp  (Longpath) — see BlueMarbleFetcher.h
//
// Longpath-original. No Thetis port.
#include "gui/widgets/BlueMarbleFetcher.h"

#include "gui/StyleConstants.h"
#include "gui/widgets/WorldMapCatalog.h"
#include "gui/widgets/WorldTexture.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QPainter>
#include <QCoreApplication>
#include <QPointer>
#include <QSaveFile>

Q_LOGGING_CATEGORY(lcBlueMarble, "longpath.bluemarble")

namespace Longpath {

BlueMarbleFetcher::BlueMarbleFetcher(QObject* parent)
    : QObject(parent)
{
}

QString BlueMarbleFetcher::fileName()
{
    return QStringLiteral("NASA Blue Marble (GIBS).jpg");
}

QString BlueMarbleFetcher::targetPath()
{
    return WorldMapCatalog::directory() + QLatin1Char('/') + fileName();
}

QString BlueMarbleFetcher::target() const
{
    return m_targetOverride.isEmpty() ? targetPath() : m_targetOverride;
}

QImage BlueMarbleFetcher::compose(GibsTileLayer& tiles, int level, bool* complete)
{
    const double degPerPx = GibsTileLayer::degPerPixel(level);
    const int width  = qRound(360.0 / degPerPx);
    const int height = qRound(180.0 / degPerPx);
    QImage out(width, height, QImage::Format_RGB32);
    out.fill(QColor(Style::kMapOceanDeep));   // Ozean, falls eine Kachel fehlt
    QPainter p(&out);
    bool all = true;
    const auto ids = GibsTileLayer::tilesFor(level, -180.0, 180.0, -90.0, 90.0);
    for (const GibsTileLayer::TileId& id : ids) {
        const QImage tile = tiles.cached(id);
        if (tile.isNull()) { all = false; continue; }
        const QRectF b = GibsTileLayer::tileBoundsDeg(id);
        // b: Laenge links/rechts, Breite oben/unten — in Bildpunkte, die
        // Kachel exakt auf ihr Feld skaliert (die unterste Reihe ragt
        // ueber den Suedpol hinaus und wird vom Bildrand beschnitten).
        const double x = (b.left() + 180.0) / degPerPx;
        const double y = (90.0 - b.top()) / degPerPx;
        const double w = b.width() / degPerPx;
        const double h = b.height() / degPerPx;
        p.drawImage(QRectF(x, y, w, h), tile);
    }
    p.end();
    if (complete) { *complete = all; }
    return out;
}

void BlueMarbleFetcher::start()
{
    if (m_running) { return; }
    // Der Betreiber hat schon ein Bild: nichts anfassen.
    if (!WorldTexture::currentPath().isEmpty() && !WorldTexture::image().isNull()) {
        emit finished(true, WorldTexture::currentPath());
        return;
    }
    const QString path = target();
    if (QFile::exists(path) && WorldTexture::setPath(path)) {
        qCInfo(lcBlueMarble) << "Weltbild liegt schon:" << path;
        emit finished(true, path);
        return;
    }
    if (!m_tiles) { m_tiles = new GibsTileLayer(this); }
    m_running = true;
    connect(m_tiles, &GibsTileLayer::tileReady, this, &BlueMarbleFetcher::onTileReady,
            Qt::UniqueConnection);
    // Alle Kacheln anfordern; was im Plattencache liegt, kommt sofort.
    for (const GibsTileLayer::TileId& id : GibsTileLayer::tilesFor(kLevel, -180, 180, -90, 90)) {
        m_tiles->tile(id);
    }
    onTileReady();
}

void BlueMarbleFetcher::onTileReady()
{
    if (!m_running || !m_tiles) { return; }
    bool complete = false;
    for (const GibsTileLayer::TileId& id : GibsTileLayer::tilesFor(kLevel, -180, 180, -90, 90)) {
        if (m_tiles->cached(id).isNull()) { return; }   // noch nicht alle da
    }
    const QImage img = compose(*m_tiles, kLevel, &complete);
    m_running = false;
    if (!complete || img.isNull()) {
        emit finished(false, QString());
        return;
    }
    const QString path = target();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || !img.save(&f, "JPEG", 88) || !f.commit()) {
        qCWarning(lcBlueMarble) << "Weltbild nicht geschrieben:" << path;
        emit finished(false, QString());
        return;
    }
    // Sidecar wie bei jedem Weltbild im Ordner: Name, Herkunft, Vermerk.
    {
        QJsonObject o;
        o.insert(QStringLiteral("name"), QStringLiteral("NASA Blue Marble (GIBS)"));
        o.insert(QStringLiteral("source"),
                 QStringLiteral("NASA EOSDIS Global Imagery Browse Services, "
                                "BlueMarble_ShadedRelief_Bathymetry, TileMatrixSet 500m Stufe %1")
                     .arg(kLevel));
        o.insert(QStringLiteral("attribution"), GibsTileLayer::attribution());
        o.insert(QStringLiteral("attributionRequired"), true);
        const QString sidecar = QFileInfo(path).absolutePath() + QLatin1Char('/')
                              + QFileInfo(path).completeBaseName() + QStringLiteral(".json");
        QSaveFile s(sidecar);
        if (s.open(QIODevice::WriteOnly)) {
            s.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
            s.commit();
        }
    }
    // Nur eintragen, wenn inzwischen niemand etwas anderes gewaehlt hat.
    if (WorldTexture::currentPath().isEmpty()) {
        WorldTexture::setPath(path);
    }
    qCInfo(lcBlueMarble) << "Weltbild zusammengesetzt:" << path << img.size();
    emit finished(true, path);
}

void BlueMarbleFetcher::ensureDefault()
{
    static bool asked = false;
    if (asked) { return; }
    asked = true;
    if (!WorldTexture::currentPath().isEmpty() && !WorldTexture::image().isNull()) { return; }
    static QPointer<BlueMarbleFetcher> fetcher;
    if (!fetcher) { fetcher = new BlueMarbleFetcher(qApp); }
    fetcher->start();
}

} // namespace Longpath
