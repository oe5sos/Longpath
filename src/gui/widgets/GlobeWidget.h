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

#include "MapPoint.h"

#include <QImage>
#include <QPoint>
#include <QRectF>
#include <QVector>
#include <QWidget>

class QContextMenuEvent;
class QDateTime;
class QPainter;
class QTimer;

namespace Longpath {

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
    bool hasTexture() const;

    // Home station and the station being worked, in degrees.
    void setHome(double lat, double lon);
    void setTarget(double lat, double lon);
    void clearTarget();

    // Many places at once, for the QSO map. Drawn as dots with thin
    // paths, separately from the single live target — a log of 500
    // contacts and "the station I am working now" are different things
    // and should not look alike.
    void setPoints(const QVector<MapPoint>& points);
    void setShowPointPaths(bool on);

    // Turn the globe so the given bearing from home faces the viewer.
    // Animated — the point is to see which way the path swings.
    void lookAlongBearing(double deg);

    // Antenna main lobe, drawn as two more arcs this many degrees either
    // side of the path. Zero draws only the centre line. This is the
    // one thing on the globe that answers "will I hear them if the
    // rotator is a few degrees off".
    void setBeamSpread(double deg);
    double beamSpread() const { return m_beamSpread; }

    // Back to the default camera and magnification.
    void resetView();

    // Magnify. The wheel does this too, but a wheel is not an
    // affordance — nothing on screen says the globe zooms.
    void zoomBy(double factor);
    double zoom() const { return m_zoom; }

    // Wie weit hinein? Bis 2026-08-19 war die Decke fest bei 6x. Das war
    // die falsche Groesse: die Grenze ist nicht der Zoom, sondern die
    // TEXTUR. Bei 2048 Bildpunkten Breite ist ein Texturpixel etwa 0,18
    // Grad, bei 5400 etwa 0,067 — wer weiter hineinzoomt, vergroessert
    // Matsch und sieht nichts Neues.
    //
    // Also haengt die Decke an der geladenen Textur: ohne Textur bleibt
    // es bei 6x (mehr zeigt ein Gitternetz nicht), mit dem grossen Blue
    // Marble sind es rund 16x.
    double maxZoom() const;

    // Bildpunkt -> Ort. Umkehrung der orthographischen Projektion
    // (Snyder, Map Projections, orthographic inverse). Liefert false
    // ausserhalb der Scheibe — dort ist kein Ort, nicht der Rand.
    //
    // Gebraucht fuer „zoome dorthin, wo der Zeiger steht": ohne
    // Rueckrechnung zoomt ein Globus immer auf seine Mitte, und das ist
    // der Unterschied, den man zu Google Earth am deutlichsten spuert.
    bool unproject(const QPointF& pos, double& lat, double& lon) const;

    // Dorthin fliegen: Kamera auf den Ort, Zoom eine Stufe naeher,
    // animiert. Der Doppelklick auf die Kugel tut das (2026-08-19) —
    // bei Google Earth ist das die Geste, mit der man sich naeher holt.
    //
    // Der Doppelklick NEBEN die Kugel setzt weiter zurueck. Beides ist
    // gewuenscht: „naeher heran" ist die haeufige Geste, „zurueck" die
    // wichtige, und sie brauchen nicht dieselbe Flaeche.
    void flyTo(double lat, double lon, double zoomFactor = 1.8);

    // Testnaht fuer den Rundgang Ort -> Bildpunkt -> Ort. project() ist
    // privat, weil niemand von aussen zeichnet; der Rundgang ist aber
    // die einzige Pruefung, die einen vertauschten Sinus in der
    // Umkehrung sicher zeigt.
    bool projectForTest(double lat, double lon, QPointF& out) const
    {
        return project(lat, lon, out);
    }

    // Kameralage und Zoomziel, fuer Tests der Flug-Geste.
    void viewForTest(double& lat, double& lon) const
    {
        lat = m_viewLat;
        lon = m_viewLon;
    }
    double targetZoomForTest() const { return m_targetZoom; }

    // Spin slowly when idle. Off by default: motion in the corner of
    // the eye is a distraction while operating.
    void setAutoRotate(bool on);
    bool autoRotate() const { return m_autoRotate; }

    // The blue atmosphere rim around the disc. Off by default
    // (2026-08-10): in a narrow dock the ring reads as a strange
    // circular bar over the globe rather than as air, and the operator
    // asked for it gone. The code stays for anyone who wants it back.
    void setShowAtmosphere(bool on);
    bool showAtmosphere() const { return m_showAtmosphere; }

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
    // Where you arrive setting off from (lat,lon) on `bearingDeg` and
    // travelling `angularDistDeg` of arc. Used to spread the beam.
    static void destinationPoint(double lat, double lon, double bearingDeg,
                                 double angularDistDeg,
                                 double& outLat, double& outLon);
    // Initial bearing a→b, degrees true.
    static double initialBearing(double latA, double lonA,
                                 double latB, double lonB);
    // Angular separation a→b, in degrees of arc.
    static double angularDistance(double latA, double lonA,
                                  double latB, double lonB);

signals:
    // Siehe oben.
    void zoomedInPastCeiling(double lat, double lon);

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    // Render the lit sphere into m_frame at the current view angles.
    void renderSphere();
    // Screen position of a lat/lon, and whether it faces the viewer.
    bool project(double lat, double lon, QPointF& out) const;
    // As project(), but for a point lifted `alt` sphere-radii above the
    // surface. Arcs are drawn raised: a great circle lying flat on the
    // sphere projects to a straight line whenever the camera is in its
    // plane — which is exactly where lookAlongBearing puts it. Lifting
    // the path off the surface makes it read as the curve it is, and is
    // also what makes an orbital view legible.
    bool projectAlt(double lat, double lon, double alt, QPointF& out) const;
    // Radius of the disc in pixels, magnification included.
    double radiusPx() const;
    // Lage der gemalten Zoomknoepfe. Zeichnung und Trefferpruefung
    // teilen sie sich (2026-08-19).
    QRectF zoomButtonRect(bool plus) const;
    // One raised great-circle arc from home to the given end point.
    //
    // `steps` is the caller's, not the arc's: drawing five hundred of
    // these is a different budget from drawing one, and the caller is
    // the only one who knows how many are coming.
    void drawArc(QPainter& p, double endLat, double endLon,
                 const QColor& col, double width, double opacity,
                 int steps) const;

    QImage m_frame;        // rendered sphere, cached until something moves

    double m_viewLat{20.0};   // camera centre
    double m_viewLon{0.0};
    double m_targetViewLon{0.0};
    // Ziele der Animation. Bis 2026-08-19 wurde nur die Laenge geglättet;
    // Breite und Zoom sprangen. Beim Hinfliegen auf einen Ort sieht das
    // aus wie ein Bildfehler, nicht wie eine Bewegung.
    double m_targetViewLat{20.0};
    double m_targetZoom{1.0};

    double m_homeLat{0.0},   m_homeLon{0.0};
    double m_targetLat{0.0}, m_targetLon{0.0};
    bool   m_hasHome{false};
    bool   m_hasTarget{false};

    double m_sunLat{0.0}, m_sunLon{0.0};

    bool     m_autoRotate{false};
    QTimer*  m_anim{nullptr};
    bool     m_frameDirty{true};

    // Main lobe half-width for the flanking arcs, in degrees.
    double m_beamSpread{5.0};

    QVector<MapPoint> m_points;
    bool m_showPointPaths{true};
    bool m_showAtmosphere{false};

    // Magnification. 1.0 fits the disc in the widget.
    double m_zoom{1.0};

    // Drag state. The globe is a thing you turn with your hand, and an
    // operator will try that before finding any button.
    bool   m_dragging{false};
    QPoint m_dragFrom;
    double m_dragStartLat{0.0};
    double m_dragStartLon{0.0};
};

} // namespace Longpath
