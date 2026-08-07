#pragma once

// =================================================================
// src/gui/widgets/GlobeWidget.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. No Thetis or AetherSDR equivalent.
//
// Rendered in software with QPainter rather than through the QRhi
// pipeline the spectrum uses. At the sizes this runs (a 300 px globe is
// ~70k lit pixels) the CPU cost is well under a millisecond per frame,
// while a GPU path would need a new shader stage in the build system
// for no visible gain. The projection maths also stays unit-testable
// this way.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include <QImage>
#include <QWidget>

class QDateTime;
class QTimer;

namespace NereusSDR {

// Orthographic globe: the Earth as seen from far above one point.
//
// Shows where you are, where the station is, and the great-circle path
// between them — the path the signal actually takes, which on a flat
// map looks like a curve and here looks like what it is.
//
// The texture is supplied by the operator, not shipped: any
// equirectangular world image (2:1, longitude across, latitude down)
// works. Without one the globe still draws a lit sphere and graticule,
// so the geometry is usable before anyone downloads anything.
class GlobeWidget : public QWidget {
    Q_OBJECT
public:
    explicit GlobeWidget(QWidget* parent = nullptr);

    // Equirectangular world image. Returns false if it can't be read;
    // the globe then keeps whatever it had.
    bool loadTexture(const QString& path);
    bool hasTexture() const { return !m_texture.isNull(); }

    // Home station and the station being worked, in degrees.
    void setHome(double lat, double lon);
    void setTarget(double lat, double lon);
    void clearTarget();

    // Turn the globe so the given bearing from home faces the viewer.
    // Animated — the point is to see which way the path swings.
    void lookAlongBearing(double deg);

    // Spin slowly when idle. Off by default: motion in the corner of
    // the eye is a distraction while operating.
    void setAutoRotate(bool on);
    bool autoRotate() const { return m_autoRotate; }

    // Sun position drives the day/night terminator. Defaults to the
    // real subsolar point for the current UTC time.
    void setSubsolarPoint(double lat, double lon);
    void useCurrentSubsolarPoint();

    QSize sizeHint() const override { return {260, 260}; }
    QSize minimumSizeHint() const override { return {140, 140}; }

    // ── Pure geometry, exposed for tests ─────────────────────────────
    // Subsolar point for a UTC instant: where the sun is overhead.
    static void subsolarPoint(const QDateTime& utc, double& lat, double& lon);
    // Point `f` of the way (0..1) along the great circle from a to b.
    static void interpolateGreatCircle(double latA, double lonA,
                                       double latB, double lonB,
                                       double f, double& lat, double& lon);

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    // Render the lit sphere into m_frame at the current view angles.
    void renderSphere();
    // Screen position of a lat/lon, and whether it faces the viewer.
    bool project(double lat, double lon, QPointF& out) const;

    QImage m_texture;      // equirectangular, may be null
    QImage m_frame;        // rendered sphere, cached until something moves

    double m_viewLat{20.0};   // camera centre
    double m_viewLon{0.0};
    double m_targetViewLon{0.0};

    double m_homeLat{0.0},   m_homeLon{0.0};
    double m_targetLat{0.0}, m_targetLon{0.0};
    bool   m_hasHome{false};
    bool   m_hasTarget{false};

    double m_sunLat{0.0}, m_sunLon{0.0};

    bool     m_autoRotate{false};
    QTimer*  m_anim{nullptr};
    bool     m_frameDirty{true};
};

} // namespace NereusSDR
