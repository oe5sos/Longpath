// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/widgets/GibsTileLayer.h  (Longpath)
// =================================================================
//
// Longpath-original. No Thetis port.
//
// Satellitenbild-Kacheln von NASA GIBS (Global Imagery Browse Services)
// fuer die flache Karte: Blue Marble (500 m) fuer die Uebersicht und
// Landsat WELD (31 m, Jahreskomposit) fuer die Naehe. Beide sind frei,
// ohne Schluessel, und verlangen nur den Vermerk (attribution()).
//
// Warum EPSG:4326 und nicht das uebliche Web-Mercator-Raster: die
// flache Karte projiziert naiv — Laenge nach rechts, Breite nach unten
// (FlatMapWidget::project). GIBS liefert genau dieses Raster ebenfalls,
// also faellt jede Kachel ohne Umprojektion auf ihren Platz. Ein
// Mercator-Raster haette entweder die ganze Karte oder jede Kachel
// verbogen.
//
// Das Raster (GIBS-WMTS-Capabilities, TileMatrixSets „500m"/„31.25m"):
// Kacheln sind 512 px, Ursprung ist -180/+90, und auf Stufe z deckt eine
// Kachel 288° / 2^z ab — Stufe 0 also zwei Kacheln fuer die Welt, Stufe
// 11 (Landsat) 2560 x 1280 Kacheln zu 0,14°. Die Kacheln am rechten und
// unteren Rand ragen ueber die Karte hinaus; das ist im Raster so.
//
// Netz: eine Warteschlange mit wenigen gleichzeitigen Abrufen, ein
// Plattencache (QNetworkDiskCache), damit ein zweiter Blick auf dieselbe
// Gegend offline geht, und ein kurzes Gedaechtnis fuer Fehlschlaege,
// damit ein fehlender Kachelserver nicht bei jedem Neuzeichnen neu
// angefragt wird.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#pragma once

#include <QCache>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QUrl>
#include <QVector>

#include <deque>

class QNetworkAccessManager;
class QNetworkDiskCache;
class QNetworkReply;

namespace Longpath {

class GibsTileLayer : public QObject {
    Q_OBJECT
public:
    explicit GibsTileLayer(QObject* parent = nullptr);
    ~GibsTileLayer() override;

    // ── Raster (statisch, ohne Netz, fuer Tests) ─────────────────────
    static constexpr int kTilePx        = 512;
    static constexpr int kBlueMarbleMax = 7;    // TileMatrixSet „500m"
    static constexpr int kLandsatMax    = 11;   // TileMatrixSet „31.25m"

    struct TileId {
        int  level{0};
        int  col{0};
        int  row{0};
        bool landsat{false};
        bool operator==(const TileId& o) const {
            return level == o.level && col == o.col && row == o.row
                && landsat == o.landsat;
        }
    };

    /// Grad je Kachelkante auf dieser Stufe: 288 / 2^level.
    static double tileSpanDeg(int level);
    /// Grad je Bildpunkt auf dieser Stufe: 0.5625 / 2^level.
    static double degPerPixel(int level);
    /// Die groebste Stufe, deren Bildpunkt nicht groesser ist als der
    /// Bildschirmpunkt — feiner als noetig kostet nur Kacheln.
    static int levelForDegPerPixel(double screenDegPerPx);
    /// Ob auf dieser Stufe Landsat (statt Blue Marble) gezeichnet wird.
    static bool usesLandsat(int level) { return level > kBlueMarbleMax; }

    /// Alle Kacheln, die den Ausschnitt beruehren. Laengen in [-180, 180],
    /// Breiten in [-90, 90]; ausserhalb wird auf die Karte begrenzt.
    static QVector<TileId> tilesFor(int level, double lonMin, double lonMax,
                                    double latMin, double latMax);
    /// Ausdehnung einer Kachel in Grad: x = Laenge, y = Breite, top = Nord.
    /// Rechts und unten ueber die Karte hinaus, wenn das Raster es tut.
    static QRectF tileBoundsDeg(const TileId& id);
    static QUrl   urlFor(const TileId& id);
    static QString cacheKey(const TileId& id);

    /// Der Vermerk, den GIBS verlangt. Nicht abschaltbar, solange Kacheln
    /// gezeichnet werden.
    static QString attribution();

    // ── Bilder ───────────────────────────────────────────────────────
    /// Die Kachel, wenn sie da ist — sonst ein leeres Bild, und der Abruf
    /// laeuft an (falls das Netz erlaubt ist). tileReady() meldet, wenn
    /// sich Neuzeichnen lohnt.
    QImage tile(const TileId& id);
    /// Nur nachsehen, nichts anstossen: was im Speicher liegt.
    QImage cached(const TileId& id) const;

    /// Netz aus: nur, was im Speicher oder auf der Platte liegt.
    void setNetworkEnabled(bool on) { m_network = on; }
    bool networkEnabled() const { return m_network; }

    /// Fuer Tests: eine Kachel direkt hinterlegen.
    void putForTest(const TileId& id, const QImage& img);
    int  pendingForTest() const { return m_inFlight.size() + int(m_queue.size()); }

signals:
    /// Eine Kachel ist angekommen — die Karte darf neu zeichnen.
    void tileReady();

private:
    void pump();
    void onReplyFinished(QNetworkReply* reply, const TileId& id);

    QNetworkAccessManager* m_nam{nullptr};
    QNetworkDiskCache*     m_disk{nullptr};
    QCache<QString, QImage> m_memory;
    QSet<QString>          m_inFlight;
    std::deque<TileId>     m_queue;
    QHash<QString, qint64> m_failedAtMs;   // Kachel -> Zeitpunkt des Fehlschlags
    bool m_network{true};

    static constexpr int    kMaxInFlight   = 6;
    static constexpr int    kMemoryTiles   = 600;
    static constexpr qint64 kRetryAfterMs  = 60'000;
    static constexpr qint64 kDiskCacheBytes = 256LL * 1024 * 1024;
};

} // namespace Longpath
