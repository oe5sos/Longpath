#pragma once

// =================================================================
// src/gui/widgets/RotorDialWidget.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Thetis has no rotator control. The project's
// existing `gui/meters/RotatorItem` is a meter-container *item* that
// draws one heading inside the MeterWidget scene graph; this is a
// standalone two-needle instrument (target + actual) with its own
// interaction, so it is a sibling rather than a reuse.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//                 Step 1 of the QRZ logbook work: display only, no
//                 rotator protocol behind it yet.
//   2026-08-10 — Elevation readout: shown in the top-right corner once
//                 a rotator has reported one. An azimuth-only rotator
//                 never triggers it, so nothing changes for the common
//                 case. AI-assisted via Anthropic Claude (Cowork),
//                 operator Martin Fischer.
// =================================================================

#include <QPointF>
class QContextMenuEvent;

#include <QImage>
#include <QWidget>

namespace Longpath {

// Compass rose showing where the antenna IS and where it SHOULD point.
//
// Two needles:
//   actual  — white, the rotator's reported heading
//   target  — accent, dashed, the bearing to the worked station
// plus the sector between them along the direction the rotator will
// actually travel.
//
// The widget holds no rotator connection. It reports what the operator
// asked for (rotateRequested / stopRequested / targetPicked) and shows
// what it is told (setActualBearing / setState). That split keeps the
// dial usable with a simulated rotator, a network one, or none at all.
class RotorDialWidget : public QWidget {
    Q_OBJECT
public:
    enum class State {
        Idle,      // no target — only the actual needle
        Targeted,  // target set, rotator not moving
        Turning,   // rotator in motion
        OnTarget,  // within the arrival tolerance
    };
    Q_ENUM(State)

    explicit RotorDialWidget(QWidget* parent = nullptr);

    // ── Live values (degrees true, 0-360) ────────────────────────────
    void setActualBearing(double deg);
    void setTargetBearing(double deg);
    void clearTarget();

    // Reported elevation, for az/el rotators. Drawn only after the
    // first call — an azimuth-only rotator never makes one, and a
    // permanent "EL 0°" would be noise on every dial.
    void setElevation(double deg);
    double actualBearing() const noexcept { return m_actual; }
    double targetBearing() const noexcept { return m_target; }
    bool   hasTarget()     const noexcept { return m_hasTarget; }

    State state() const noexcept { return m_state; }

    // ── Mechanics ────────────────────────────────────────────────────
    // Most rotators cannot pass a mechanical end stop, so the shorter
    // arc is not always the legal one. `stopDeg` is the heading the
    // rotator cannot turn through (commonly 180 for a south stop or 0
    // for a north stop); the travel path is computed the long way round
    // when the short way would cross it. A negative value means the
    // rotator turns freely.
    void setEndStop(double stopDeg);
    double endStop() const noexcept { return m_endStop; }

    // Antenna beam width, drawn as a wedge around the actual heading.
    // ── Als durchsichtiges Bild ──────────────────────────────────────
    //
    // Der Betreiber, 2026-08-20: „kann man den rotorzeiger alleine auch
    // im panadapter einblenden lassen? … sodass der hintergrund mit dem
    // panadapter eines ist, quasi transparent."
    //
    // Liefert die Windrose ohne Grundflaeche und ohne Ablesung — nur
    // Ring, Teilung, Zeiger und Ziel — auf durchsichtigem Grund. Der
    // Panadapter legt das Bild in eine Ecke; er bekommt dabei KEIN
    // Rotorwissen (siehe SpectrumWidget::setCompassOverlay).
    //
    // Ein Bild und kein Kindwidget, weil ein nicht-natives Kind auf
    // macOS hinter dem QRhi-Inhalt verschwindet und ein natives keine
    // Durchsichtigkeit ueber die GPU-Flaeche bekommt.
    QImage renderTransparent(int sidePx, qreal dpr = 2.0);

    // ── Zwei Formen ──────────────────────────────────────────────────
    //
    // Der Betreiber, 2026-08-21, nach dem Entwurfsblatt: „beide zur
    // auswahl, standard vollkreis."
    //
    // Der Grund steht im Entwurf (Longpath-Kompass-Entwurf.pdf): ein
    // Vollkreis ist so breit wie hoch. In einer Flaeche von 1180 x 330
    // begrenzt die HOEHE den Radius auf rund 150 — er nutzt damit 300
    // von 1180 Punkten Breite, und die restlichen 880 kann er nicht
    // fuellen, egal wie man rechnet. S-Meter und Stehwelle wirken
    // richtig proportioniert, weil sie Halbkreise sind.
    //
    // Das Band loest das, indem es die Rundform aufgibt: ein
    // Ausschnitt von +-120 Grad um die Antenne, die fest in der Mitte
    // steht. Was es dafuer verliert, ist das Rundherum — deshalb eine
    // WAHL und keine Ablösung.
    enum class Shape { Rose, Tape };

    void  setShape(Shape s);
    Shape shape() const noexcept { return m_shape; }

    void setBeamWidth(double deg);

    // ── Is the actual needle reporting, or pretending? ───────────────
    //
    // With no rotator connected the panel drives the actual needle from
    // a timer so the dial is demonstrable. The needle then looks
    // exactly like a real reading, which is a display that says the
    // antenna is at 42° when nothing is at 42° — and an operator only
    // has to believe that once before they stop trusting the dial at
    // all.
    //
    // So a simulated needle is drawn hollow and dashed and the rose is
    // labelled. The status line already says it in words; words
    // scroll away and the needle does not. (2026-08-10)
    void setSimulated(bool on);
    bool isSimulated() const noexcept { return m_simulated; }

    // An extra line for the tooltip, set by whoever owns the dial —
    // the panel uses it to explain the double-click once it has hidden
    // the buttons. Kept separate from the simulated warning so the two
    // cannot overwrite each other, which is what happened when both
    // called setToolTip.
    void setHint(const QString& text);

    // How close counts as arrived.
    void setArrivalTolerance(double deg);

    // Signed travel from actual to target along the legal direction:
    // positive clockwise, negative counter-clockwise. Zero when there
    // is no target. Public because the surrounding card shows it as
    // text and the tests pin the end-stop behaviour.
    double travelDegrees() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setState(State s);

signals:
    // Operator clicked inside the rose to aim manually.
    void targetPicked(double deg);
    // Operator asked to start / stop turning.
    void rotateRequested(double targetDeg);
    void stopRequested();

protected:
    void paintEvent(QPaintEvent*) override;
    // Single click aims, double click aims and starts turning. Keeping
    // "commit" behind a second, deliberate action means a stray click
    // on the rose can never send the antenna somewhere.
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseDoubleClickEvent(QMouseEvent* ev) override;

private:
    // Screen angle for a compass bearing: 0° is up, clockwise.
    static double bearingToRadians(double deg);
    // Compass bearing under a widget-local point; negative inside the
    // hub's dead zone, where the angle is meaningless.
    double bearingAt(const QPointF& pos) const;
    void recomputeState();
    // Tooltip from both facts at once: the simulated warning first,
    // then whatever hint the owner set.
    void refreshTooltip();

    // ── Where the rose is, in one place ──────────────────────────────
    //
    // The rose normally sits high, with the readout under it; when the
    // widget is small it takes the whole face instead. Two functions
    // need to agree about that: the one that draws it and the one that
    // works out which bearing the mouse landed on.
    //
    // They did not. paintEvent moved the centre and bearingAt() kept
    // its own copy of the old formula, so on a small dial a click aimed
    // at a heading several degrees from the one under the cursor —
    // silently, and worse the smaller it got.
    // ── Querformat ───────────────────────────────────────────────────
    //
    // Die Leiste unten ist rund 1000 px breit und 300 hoch, und der
    // Rotor bekommt darin einen flachen Streifen. roseRadius() nahm
    // dort min(w*0.42, h*0.36) — die Hoehe gewinnt, und aus 160 px
    // Streifenhoehe werden 58 px Radius. Beim Rendern am 2026-08-20
    // gemessen: Zifferblatt rund 110 px in einer 1950 px breiten
    // Flaeche, und die Gradzahlen fielen ganz weg, weil sie erst ab
    // r > 84 gezeichnet werden.
    //
    // Genau das hat der Betreiber gemeldet: der Inhalt aendert sich
    // beim Verkleinern nicht im Massstab. Er tat es doch — nur an der
    // falschen Achse.
    //
    // Im Querformat steht die Rose links und nimmt die volle Hoehe;
    // die Ablesung rueckt nach rechts daneben, statt darunter zu
    // stehen, wo kein Platz ist.
    /// Waehrend renderTransparent(): kein Grund, keine Ablesung.
    bool    m_bare{false};
    Shape   m_shape{Shape::Rose};

    /// Das Band statt der Rose. Siehe Shape.
    void   paintTape(QPainter& p);
    void   contextMenuEvent(QContextMenuEvent* ev) override;
    double bearingAtTape(const QPointF& pos) const;
    /// Sichtbarer Ausschnitt des Bandes, in Grad (Vollbreite).
    static constexpr double kTapeSpanDeg = 240.0;

    bool    isLandscape()   const;
    bool    isCompassOnly() const;
    QPointF roseCentre()    const;
    double  roseRadius()    const;

    double m_actual{0.0};
    double m_target{0.0};
    bool   m_hasTarget{false};
    double m_elevation{0.0};
    bool   m_hasElevation{false};
    double m_endStop{-1.0};        // <0 = free rotation
    double m_beamWidth{40.0};
    double m_tolerance{3.0};
    bool   m_simulated{false};
    QString m_hint;
    State  m_state{State::Idle};
};

} // namespace Longpath
