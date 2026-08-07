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
// =================================================================

#include "MapPoint.h"

#include <QImage>
#include <QPointF>
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

    // Draw a great circle from home to each point. Off for very large
    // sets, where the lines become a single smear and the dots alone
    // read better.
    void setShowPaths(bool on);
    bool showPaths() const { return m_showPaths; }

    // Day/night shading. The terminator is the reason to look at a
    // world map while operating, not decoration.
    void setShowTerminator(bool on);

    // Re-read the shared world image, e.g. after a download.
    void refreshTexture();

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
    void buildNightOverlay();

    QVector<MapPoint> m_points;
    double m_homeLat{0.0}, m_homeLon{0.0};
    bool   m_hasHome{false};

    bool m_showPaths{true};
    bool m_showTerminator{true};

    QImage m_night;          // coarse shading, scaled up on draw
    bool   m_nightDirty{true};

    double  m_zoom{1.0};
    QPointF m_pan{0.0, 0.0};   // pixels
    bool    m_dragging{false};
    QPoint  m_dragFrom;
    QPointF m_panFrom;
};

} // namespace NereusSDR
