#pragma once

// =================================================================
// src/gui/widgets/FlatMapWidget.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Equirectangular world map: every contact visible at once, which is
// the one thing the globe cannot do. Half the Earth is always facing
// away from a sphere, and "where have I worked" is a question about all
// of it.
//
// The projection is deliberately the naive one — longitude across,
// latitude down — because that is what the operator's world image is,
// and re-projecting a 5400 x 2700 photograph to something prettier
// would cost more than it buys.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-08-10 — Maidenhead grid overlay (fields, then squares as the
//                 zoom allows, click to identify), and contact markers
//                 became clickable — pointClicked carries the callsign
//                 so the map window can show the station's data. A
//                 click is distinguished from a drag by movement, not
//                 by timing. AI-assisted via Anthropic Claude (Cowork),
//                 operator Martin Fischer.
//   2026-09-21 — Luftbild-Kacheln (GibsTileLayer) unter der Weltkarte,
//                 sobald hineingezoomt ist; der Zoom reicht dann bis zur
//                 Kachelaufloesung statt bis 12x. flyTo() fliegt animiert
//                 auf einen Ort, setFocusStation() zeichnet die Zielstation
//                 mit Grosskreis vom eigenen Standort. Martin Fischer,
//                 AI-assisted via Anthropic Claude.
//   2026-11-11 — Locator-Jagd: das Mosaik (Felder je Band aus dem Logbuch)
//                 als Teil der Grid-Ueberlagerung, gehoerte FT8-Stationen
//                 als Punkte mit Hinweis; ein Klick darauf meldet
//                 livePunktGeklickt. Martin Fischer (OE5SOS), AI-assisted
//                 via Anthropic Claude (Claude Code).
// =================================================================

#include "MapPoint.h"
#include "core/LocatorJagd.h"

#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QWidget>

class QVariantAnimation;

namespace Longpath {

class GibsTileLayer;

class FlatMapWidget : public QWidget {
    Q_OBJECT
public:
    explicit FlatMapWidget(QWidget* parent = nullptr);

    void setHome(double lat, double lon);
    void clearHome();
    void setPoints(const QVector<MapPoint>& points);

    // ── Der eigene Standort als Bild statt als Punkt ─────────────────
    //
    // 2026-08-15, nach einer Vorlage des Betreibers: in der Vorlage ist der
    // eigene Standort kein Punkt und keine Nadel, sondern ein rundes
    // Foto der Station mit einem Ring und dem Rufzeichen darunter.
    //
    // Das ist mehr als Zierde. Auf einer Weltkarte voller
    // Kontaktmarker ist der eigene Standort der einzige, den man sofort
    // finden muss — und ein Foto findet das Auge, bevor es liest.
    //
    // `photo` darf leer sein; dann bleibt der bisherige Punkt, nur mit
    // dem Rufzeichen statt „HOME". Das Bild wird auf die gezeichnete
    // Größe vorskaliert und behalten, weil sonst bei jedem Neuzeichnen
    // — und die Karte zeichnet beim Ziehen dauernd neu — ein volles
    // Bild skaliert würde.
    void setStationMarker(const QString& callsign, const QImage& photo);

    /// Das fertig geschnittene Markerbild. Für Tests: ob ein Foto rund
    /// und mittig beschnitten ankommt, ist mit Arithmetik prüfbar —
    /// ob es hübsch aussieht, nicht.
    QPixmap stationMarkerPixmap() const { return m_stationPhoto; }
    QString stationCallsign() const { return m_stationCall; }

    // Draw a great circle from home to each point. Off for very large
    // sets, where the lines become a single smear and the dots alone
    // read better.
    void setShowPaths(bool on);
    bool showPaths() const { return m_showPaths; }

    // Day/night shading. The terminator is the reason to look at a
    // world map while operating, not decoration.
    void setShowTerminator(bool on);

    // Maidenhead overlay: fields (AA-RR) always, squares once the zoom
    // gives them room. Clicking an empty spot with the grid on
    // identifies the square under the cursor and highlights it.
    void setShowGrid(bool on);
    bool showGrid() const { return m_showGrid; }

    // ── Locator-Jagd (2026-11-11) ─────────────────────────────────────
    //
    // Das Mosaik gehoert zur Grid-Ueberlagerung, nicht daneben: es ist
    // dieselbe Frage („welches Feld ist das?"), nur mit der Antwort aus
    // dem Logbuch. Gezeichnet wird es nur bei eingeschaltetem Grid.
    void setMosaik(const QHash<QString, FeldStand>& felder);
    int  mosaikAnzahl() const { return m_mosaik.size(); }

    // Jagdmodus: das Mosaik deckt, statt zu tuschen. Der Schalter sitzt
    // im Kartenfenster ("Locator hunt"), nicht hier -- dieses Widget
    // weiss nur, wie kraeftig es malen soll.
    void setJagdModus(bool an);
    bool jagdModus() const { return m_jagdModus; }

    // Die gehoerten FT8-Stationen. Unabhaengig vom Grid zu sehen: wer
    // FT8 hoert, will die Punkte auch ohne Gitter finden.
    void setLivePunkte(const QVector<LivePunkt>& punkte);
    const QVector<LivePunkt>& livePunkte() const { return m_live; }
    /// Wo der i-te Punkt gerade gezeichnet wird (Widget-Koordinaten).
    QPointF livePunktBildpunkt(int i) const;
    /// Der Punkt unter `pos` (Trefferradius), oder -1.
    int livePunktAt(const QPointF& pos) const;
    /// Felder, die im letzten Bild als „neu gehoert" umrandet wurden.
    int neueFelderGemaltForTest() const { return m_neueFelderGemalt; }

    // Re-read the shared world image, e.g. after a download.
    void refreshTexture();

    // Magnify about the centre. The wheel does this too, but a wheel is
    // not an affordance — nothing on screen says the map zooms.
    void zoomBy(double factor);
    double zoom() const { return m_zoom; }
    // Back to the whole world, centred.
    void resetView();

    // Auf einen Ort schwenken, ohne den Zoom anzufassen. Gebraucht fuer
    // den Uebergang von der Kugel: wer dort bis zum Anschlag
    // hineingezoomt hat, will hier denselben Ort sehen und nicht
    // Nullmeridian und Aequator.
    void centreOn(double lat, double lon);

    // ── Luftbild ─────────────────────────────────────────────────────
    //
    // Die Weltkarte ist ein 5400 px breites Bild; ab etwa 6x Zoom wird
    // sie weich. Mit einer Kachelschicht geht es weiter: Blue Marble bis
    // 500 m, Landsat bis 31 m je Bildpunkt. Die Kacheln liegen im
    // selben naiven Raster wie die Karte (siehe GibsTileLayer.h) und
    // werden ab kImageryFromZoom ueber das Weltbild gezeichnet.
    void setImagery(GibsTileLayer* layer);
    void setShowImagery(bool on);
    bool showImagery() const { return m_showImagery; }
    /// Ob im letzten Bild Kacheln gezeichnet wurden (fuer den Vermerk).
    bool imageryPainted() const { return m_imageryPainted; }

    // Der Zoom endet, wo das Bild endet: bei 12x ohne Luftbild, bei der
    // Landsat-Aufloesung mit. Ein Zoom ist relativ zur Einpassung, also
    // haengt die Zahl von der Fensterbreite ab — daher die Umrechnung.
    double maxZoom() const;
    double zoomForDegPerPixel(double degPerPx) const;
    double degPerPixel() const;
    static constexpr double kImageryFromZoom = 2.0;

    // ── Hinflug ──────────────────────────────────────────────────────
    //
    // Nicht springen, fliegen: Mitte und Zoom laufen weich zum Ziel, und
    // unterwegs geht der Zoom ein Stueck heraus, damit man sieht, wohin
    // es geht. durationMs <= 0 setzt die Ansicht sofort (Tests).
    void flyTo(double lat, double lon, double targetZoom, int durationMs = 1600);
    bool isFlying() const;
    /// Wo die Fenstermitte gerade liegt. false, wenn neben der Karte.
    bool viewCentre(double& lat, double& lon) const;
    /// Der Zoom unterwegs, fuer t in [0, 1]: Logarithmisch zwischen z0
    /// und z1, mit einem Bogen nach aussen in der Mitte. Statisch, damit
    /// ein Test die Form pruefen kann, ohne ein Fenster zu zeichnen.
    static double flightZoomAt(double t, double z0, double z1);

    // ── Zielstation ──────────────────────────────────────────────────
    //
    // Der Ort, den der Betreiber gerade sucht: ein Ring mit Rufzeichen
    // und der Grosskreis vom eigenen Standort. Unabhaengig von den
    // Logbuchpunkten, denn die Station muss nicht im Log stehen.
    void setFocusStation(const QString& call, double lat, double lon);
    void clearFocusStation();
    QString focusStation() const { return m_focusCall; }

    // ── Satelliten ueber dem Horizont ────────────────────────────────
    //
    // Subsatellitenpunkte der Satelliten, die der Betreiber gerade sehen
    // kann: ein Dreieck mit Namen und Elevation. Kommt aus dem
    // SatelliteService ueber das Kartenfenster; die Karte zeichnet nur.
    struct SatelliteMarker {
        QString name;
        double  lat{0.0};
        double  lon{0.0};
        double  elevationDeg{0.0};
    };
    void setSatellites(const QVector<SatelliteMarker>& sats);
    void setShowSatellites(bool on);
    bool showSatellites() const { return m_showSatellites; }
    int  satellitesPainted() const { return m_satellitesPainted; }

    QSize sizeHint() const override { return {900, 460}; }

    // ── Pure geometry, exposed for tests ─────────────────────────────
    //
    // A great circle sampled in (lon, lat) may cross the antimeridian,
    // where a polyline drawn straight would streak all the way back
    // across the map. This splits the samples into runs that each stay
    // on one side. Input and output points are (x = lon, y = lat).
    static QVector<QVector<QPointF>>
    splitAtAntimeridian(const QVector<QPointF>& lonLat);

    // Samples of the great circle between two places, in (lon, lat).
    static QVector<QPointF> greatCircleSamples(double lat1, double lon1,
                                               double lat2, double lon2,
                                               int steps);

    // The 4-character Maidenhead square containing a position. Static
    // and here rather than in core/Maidenhead: the overlay needs the
    // square's IDENTITY at arbitrary coarseness, not its centre.
    static QString gridSquare4(double lat, double lon);

signals:
    /// Der Hinflug ist am Ziel (auch bei durationMs <= 0).
    void flightFinished();
    // Der Betreiber zoomt am unteren Anschlag weiter heraus (2026-08-19).
    // Fuer die flache Karte ist bei 1x die ganze Welt zu sehen — weiter
    // heraus gibt es hier nichts, wohl aber auf der KUGEL. Das Fenster
    // schaltet daraufhin um.
    void zoomedOutPastFloor();

    // A contact marker was clicked. label is the MapPoint's label —
    // the callsign, prefixed with '~' when the position was a country
    // guess rather than a locator.
    void pointClicked(const QString& label, double lat, double lon);

    // With the grid overlay on, a click that hit no marker names the
    // square it landed in.
    void gridClicked(const QString& locator);

    // Ein gehoerter FT8-Punkt wurde angeklickt (2026-11-11). Er hat
    // Vorrang vor Logbuchpunkt und Gitter: er liegt obenauf.
    void livePunktGeklickt(const QString& rufzeichen);

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    bool event(QEvent*) override;

private:
    // Map area inside the widget at the current zoom and pan.
    QRectF mapRect() const;
    QPointF project(double lat, double lon) const;
    // Inverse of project(). False when the pixel lies off the map.
    bool unproject(const QPointF& pos, double& lat, double& lon) const;
    void buildNightOverlay();
    void paintGridOverlay(QPainter& p, const QRectF& r);
    // The marker under (or within a few pixels of) a click, or -1.
    int pointAt(const QPointF& pos) const;
    void setView(double lat, double lon, double zoom);
    bool paintImagery(QPainter& p, const QRectF& r);
    void paintFocusStation(QPainter& p);
    void paintSatellites(QPainter& p);
    void paintMosaik(QPainter& p);
    void paintLive(QPainter& p, const QRectF& r);

    QVector<MapPoint> m_points;
    double m_homeLat{0.0}, m_homeLon{0.0};

    // Der Standortmarker. m_stationPhoto ist bereits rund geschnitten
    // und auf kStationMarkerPx skaliert — beim Ziehen der Karte wird
    // dutzendfach pro Sekunde neu gezeichnet, und ein volles Foto pro
    // Bild zu skalieren würde man merken.
    QString m_stationCall;
    QPixmap m_stationPhoto;
    static constexpr int kStationMarkerPx = 52;
    bool   m_hasHome{false};

    bool m_showPaths{true};
    bool m_showTerminator{true};
    bool m_showGrid{false};
    bool m_jagdModus{false};
    QString m_clickedGrid;   // highlighted square, empty for none

    QHash<QString, FeldStand> m_mosaik;
    QVector<LivePunkt>        m_live;
    int                       m_neueFelderGemalt{0};

    QImage m_night;          // coarse shading, scaled up on draw
    bool   m_nightDirty{true};

    double  m_zoom{1.0};
    QPointF m_pan{0.0, 0.0};   // pixels

    GibsTileLayer* m_imagery{nullptr};
    bool m_showImagery{false};
    bool m_imageryPainted{false};

    QVariantAnimation* m_flight{nullptr};
    double m_flyLat0{0.0}, m_flyLon0{0.0}, m_flyZ0{1.0};
    double m_flyLat1{0.0}, m_flyLon1{0.0}, m_flyZ1{1.0};

    bool    m_hasFocus{false};
    QString m_focusCall;

    QVector<SatelliteMarker> m_satellites;
    bool m_showSatellites{true};
    int  m_satellitesPainted{0};
    double  m_focusLat{0.0}, m_focusLon{0.0};
    bool    m_dragging{false};
    // A press is a click until it moves. Distinguishing by distance
    // rather than time: a slow deliberate click is still a click.
    bool    m_moved{false};
    QPoint  m_dragFrom;
    QPointF m_panFrom;
};

} // namespace Longpath
