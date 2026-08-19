#pragma once

// =================================================================
// src/gui/widgets/FlatMapWidget.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
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
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-08-10 — Maidenhead grid overlay (fields, then squares as the
//                 zoom allows, click to identify), and contact markers
//                 became clickable — pointClicked carries the callsign
//                 so the map window can show the station's data. A
//                 click is distinguished from a drag by movement, not
//                 by timing. AI-assisted via Anthropic Claude (Cowork),
//                 operator Martin Fischer.
// =================================================================

#include "MapPoint.h"

#include <QImage>
#include <QPixmap>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QWidget>

namespace NereusSDR {

class FlatMapWidget : public QWidget {
    Q_OBJECT
public:
    explicit FlatMapWidget(QWidget* parent = nullptr);

    void setHome(double lat, double lon);
    void clearHome();
    void setPoints(const QVector<MapPoint>& points);

    // ── Der eigene Standort als Bild statt als Punkt ─────────────────
    //
    // 2026-08-15, nach einer Vorlage des Betreibers: bei Zeus ist der
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

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

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
    QString m_clickedGrid;   // highlighted square, empty for none

    QImage m_night;          // coarse shading, scaled up on draw
    bool   m_nightDirty{true};

    double  m_zoom{1.0};
    QPointF m_pan{0.0, 0.0};   // pixels
    bool    m_dragging{false};
    // A press is a click until it moves. Distinguishing by distance
    // rather than time: a slow deliberate click is still a click.
    bool    m_moved{false};
    QPoint  m_dragFrom;
    QPointF m_panFrom;
};

} // namespace NereusSDR
