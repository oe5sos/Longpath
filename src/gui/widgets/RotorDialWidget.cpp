// =================================================================
// src/gui/widgets/RotorDialWidget.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — see RotorDialWidget.h for provenance.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "RotorDialWidget.h"
#include "gui/StyleConstants.h"

#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

// Palette taken from Style:: so the dial matches the rest of the app
// rather than carrying its own look. The four needle states reuse the
// existing semantic colours: accent for the target, amber for motion,
// green for arrival.
const QColor kBackground(Style::kPanelBg);      // #0a0a18
const QColor kActual    (Style::kTextPrimary);  // where it is
const QColor kTarget    (Style::kAccent);       // where it should go
const QColor kTurning   (Style::kAmberText);    // in motion
const QColor kArrived   (Style::kGreenText);    // on target
const QColor kRing      (Style::kBorder);
const QColor kRingInner (Style::kBorderSubtle);
const QColor kCardinal  (Style::kTextScale);
const QColor kMuted     (Style::kTextSecondary);

double norm360(double deg)
{
    double d = std::fmod(deg, 360.0);
    if (d < 0.0) { d += 360.0; }
    return d;
}

// Compass point for a bearing — an operator turning a rotator thinks in
// "NW", and a bare number is easy to misread.
QString compassPoint(double deg)
{
    static const char* kPoints[] = {"N", "NNE", "NE", "ENE", "E", "ESE",
                                    "SE", "SSE", "S", "SSW", "SW", "WSW",
                                    "W", "WNW", "NW", "NNW"};
    const int idx = static_cast<int>(norm360(deg) / 22.5 + 0.5) % 16;
    return QString::fromLatin1(kPoints[idx]);
}

QString degText(double deg)
{
    return QStringLiteral("%1°").arg(qRound(norm360(deg)), 3, 10,
                                     QLatin1Char('0'));
}

} // namespace

RotorDialWidget::RotorDialWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setCursor(Qt::CrossCursor);
    // User-visible tooltip — plain English, no source cites.
    setToolTip(QStringLiteral("Click inside the rose to aim the antenna"));
}

QSize RotorDialWidget::sizeHint()        const { return {190, 210}; }
QSize RotorDialWidget::minimumSizeHint() const { return {130, 150}; }

double RotorDialWidget::bearingToRadians(double deg)
{
    // Compass 0° is up and increases clockwise; Qt's maths angle is 0°
    // to the right and increases counter-clockwise.
    return qDegreesToRadians(90.0 - deg);
}

void RotorDialWidget::setActualBearing(double deg)
{
    const double v = norm360(deg);
    if (qFuzzyCompare(m_actual + 1.0, v + 1.0)) { return; }
    m_actual = v;
    recomputeState();
    update();
}

void RotorDialWidget::setTargetBearing(double deg)
{
    m_target = norm360(deg);
    m_hasTarget = true;
    recomputeState();
    update();
}

void RotorDialWidget::clearTarget()
{
    if (!m_hasTarget) { return; }
    m_hasTarget = false;
    m_state = State::Idle;
    update();
}

void RotorDialWidget::setElevation(double deg)
{
    m_elevation = std::clamp(deg, 0.0, 90.0);
    m_hasElevation = true;
    update();
}

void RotorDialWidget::setEndStop(double stopDeg)
{
    m_endStop = stopDeg < 0.0 ? -1.0 : norm360(stopDeg);
    update();
}

void RotorDialWidget::setBeamWidth(double deg)
{
    m_beamWidth = std::clamp(deg, 0.0, 180.0);
    update();
}

void RotorDialWidget::setSimulated(bool on)
{
    if (m_simulated == on) { return; }
    m_simulated = on;
    update();
}

void RotorDialWidget::setArrivalTolerance(double deg)
{
    m_tolerance = std::max(0.5, deg);
    recomputeState();
    update();
}

void RotorDialWidget::setState(State s)
{
    if (m_state == s) { return; }
    m_state = s;
    update();
}

void RotorDialWidget::recomputeState()
{
    if (!m_hasTarget) { m_state = State::Idle; return; }
    if (m_state == State::Turning) { return; }   // the mover owns this
    const double delta = std::abs(travelDegrees());
    m_state = (delta <= m_tolerance) ? State::OnTarget : State::Targeted;
}

double RotorDialWidget::travelDegrees() const
{
    if (!m_hasTarget) { return 0.0; }

    // Shortest signed arc, clockwise positive.
    double diff = norm360(m_target - m_actual);
    double shortWay = (diff <= 180.0) ? diff : diff - 360.0;

    if (m_endStop < 0.0) { return shortWay; }

    // The rotator cannot turn *through* its end stop. If the short way
    // would cross it, the only legal path is the long way round — this
    // is the difference between aiming the antenna and winding it into
    // the stop.
    auto crossesStop = [this](double from, double travel) {
        const int steps = static_cast<int>(std::ceil(std::abs(travel)));
        const double dir = travel >= 0.0 ? 1.0 : -1.0;
        for (int i = 1; i <= steps; ++i) {
            const double prev = norm360(from + dir * (i - 1));
            const double cur  = norm360(from + dir * std::min<double>(i, std::abs(travel)));
            // Did this one-degree step step over the stop heading?
            const double a = norm360(m_endStop - prev);
            const double b = norm360(m_endStop - cur);
            if (dir > 0.0 && a <= 1.0 && b > 359.0) { return true; }
            if (dir < 0.0 && b <= 1.0 && a > 359.0) { return true; }
            if (std::abs(norm360(cur) - m_endStop) < 1e-9) { return true; }
        }
        return false;
    };

    if (!crossesStop(m_actual, shortWay)) { return shortWay; }
    return shortWay >= 0.0 ? shortWay - 360.0 : shortWay + 360.0;
}

// Bearing under the cursor, or a negative value inside the hub's dead
// zone (where the angle is meaningless and a stray pixel would swing
// the target wildly).
double RotorDialWidget::bearingAt(const QPointF& pos) const
{
    const QPointF c(width() * 0.5, height() * 0.42);
    const QPointF p = pos - c;
    if (std::hypot(p.x(), p.y()) < 8.0) { return -1.0; }
    return norm360(qRadiansToDegrees(std::atan2(p.x(), -p.y())));
}

void RotorDialWidget::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton) { QWidget::mousePressEvent(ev); return; }
    const double deg = bearingAt(ev->position());
    if (deg < 0.0) { return; }
    setTargetBearing(deg);
    emit targetPicked(deg);
}

void RotorDialWidget::mouseDoubleClickEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(ev);
        return;
    }
    const double deg = bearingAt(ev->position());
    if (deg < 0.0) { return; }

    // Aim at where the second click landed, not at the target the first
    // click set: the two can differ by a pixel or two, and the antenna
    // should go where the operator last pointed.
    setTargetBearing(deg);
    emit targetPicked(deg);
    emit rotateRequested(deg);
}

void RotorDialWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const double w = width();
    const double h = height();
    p.fillRect(rect(), kBackground);

    // Faint accent wash from below so the face is not dead flat. Kept
    // subtle and in the app's accent hue rather than a warm lamp — the
    // surrounding panels are cool-toned.
    QRadialGradient glow(QPointF(w * 0.5, h * 1.10), w * 0.85);
    QColor glowInner = QColor(Style::kAccent);
    glowInner.setAlpha(26);
    glow.setColorAt(0.0, glowInner);
    glow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.fillRect(rect(), glow);

    const QPointF c(w * 0.5, h * 0.42);
    const double r = std::min(w * 0.42, h * 0.36);

    // Rings
    p.setPen(QPen(kRing, 1));
    p.drawEllipse(c, r, r);
    p.setPen(QPen(kRingInner, 1));
    p.drawEllipse(c, r * 0.83, r * 0.83);

    // Ticks every 30°, longer on the cardinals
    for (int deg = 0; deg < 360; deg += 30) {
        const double a = bearingToRadians(deg);
        const bool cardinal = (deg % 90) == 0;
        const double r0 = r * (cardinal ? 0.86 : 0.92);
        p.setPen(QPen(cardinal ? kMuted : kRing, cardinal ? 1.5 : 1.0));
        p.drawLine(QPointF(c.x() + r0 * std::cos(a), c.y() - r0 * std::sin(a)),
                   QPointF(c.x() + r  * std::cos(a), c.y() - r  * std::sin(a)));
    }

    // Cardinal letters
    QFont cf = p.font();
    cf.setPixelSize(10);
    p.setFont(cf);
    p.setPen(kCardinal);
    const QFontMetrics cfm(cf);
    const struct { const char* s; int deg; } kCards[] = {
        {"N", 0}, {"E", 90}, {"S", 180}, {"W", 270}};
    for (const auto& card : kCards) {
        const double a = bearingToRadians(card.deg);
        const double rr = r + 11;
        const QString s = QString::fromLatin1(card.s);
        p.drawText(QPointF(c.x() + rr * std::cos(a) - cfm.horizontalAdvance(s) / 2.0,
                           c.y() - rr * std::sin(a) + 4),
                   s);
    }

    // Travel sector: from actual towards target along the legal path.
    if (m_hasTarget) {
        const double travel = travelDegrees();
        const QRectF box(c.x() - r, c.y() - r, r * 2, r * 2);
        // Qt angles: 0 at 3 o'clock, counter-clockwise positive.
        const int startQt = static_cast<int>((90.0 - m_actual) * 16);
        const int spanQt  = static_cast<int>(-travel * 16);
        QColor sector = (m_state == State::Turning) ? kTurning
                      : (m_state == State::OnTarget) ? kArrived : kTarget;
        sector.setAlpha(m_state == State::OnTarget ? 26 : 30);
        p.setPen(Qt::NoPen);
        p.setBrush(sector);
        p.drawPie(box, startQt, spanQt);
        p.setBrush(Qt::NoBrush);
    }

    // Beam-width wedge around the actual heading
    if (m_beamWidth > 0.5) {
        const QRectF box(c.x() - r * 0.95, c.y() - r * 0.95, r * 1.9, r * 1.9);
        const int startQt = static_cast<int>((90.0 - m_actual - m_beamWidth / 2.0) * 16);
        const int spanQt  = static_cast<int>(-m_beamWidth * 16);
        QColor wedge = kActual;
        wedge.setAlpha(16);
        p.setPen(Qt::NoPen);
        p.setBrush(wedge);
        p.drawPie(box, startQt, spanQt);
        p.setBrush(Qt::NoBrush);
    }

    auto drawNeedle = [&](double deg, const QColor& col, double len,
                          bool dashed, double width) {
        const double a = bearingToRadians(deg);
        const QPointF tip(c.x() + r * len * std::cos(a),
                          c.y() - r * len * std::sin(a));
        QColor halo = col;
        halo.setAlpha(70);
        p.setPen(QPen(halo, width + 2.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(c, tip);
        QPen pen(col, width, dashed ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap);
        if (dashed) { pen.setDashPattern({2.2, 1.6}); }
        p.setPen(pen);
        p.drawLine(c, tip);
    };

    // Target first so the actual needle reads on top of it.
    if (m_hasTarget && m_state != State::OnTarget) {
        drawNeedle(m_target, kTarget, 0.90, /*dashed=*/true, 2.4);
    }

    // A simulated needle is drawn dashed and dim. It is the same shape
    // as a real reading otherwise, and an operator who mistakes one for
    // the other turns the wrong way at three in the morning — or,
    // worse, believes the antenna moved when it did not. (2026-08-10)
    const QColor actualCol = m_simulated                  ? kMuted
                           : (m_state == State::Turning)  ? QColor(0xff, 0xfb, 0xe8)
                           : (m_state == State::OnTarget) ? QColor(0xea, 0xff, 0xf4)
                                                          : kActual;
    drawNeedle(m_actual, actualCol, 0.90, m_simulated, m_simulated ? 2.0 : 2.6);

    // Hub
    const QColor hubCol = (m_state == State::Turning)  ? kTurning
                        : (m_state == State::OnTarget) ? kArrived
                                                       : QColor(0xc9, 0xcc, 0xd4);
    QRadialGradient hub(c, 9);
    hub.setColorAt(0.0, hubCol);
    QColor hubEdge = hubCol;
    hubEdge.setAlpha(0);
    hub.setColorAt(1.0, hubEdge);
    p.setPen(Qt::NoPen);
    p.setBrush(hub);
    p.drawEllipse(c, 9, 9);
    p.setBrush(hubCol);
    p.drawEllipse(c, 3.6, 3.6);
    p.setBrush(Qt::NoBrush);

    // Elevation, top-right, only for rotators that report one.
    // Top-left, opposite the elevation readout, so the two never
    // collide on an az/el rotator.
    if (m_simulated) {
        QFont sf = p.font();
        sf.setPixelSize(10);
        sf.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
        p.setFont(sf);
        p.setPen(QColor(Style::kAmberWarn));
        p.drawText(QPointF(6.0, 14.0), QStringLiteral("NO ROTATOR"));
    }

    if (m_hasElevation) {
        QFont ef = p.font();
        ef.setPixelSize(10);
        p.setFont(ef);
        p.setPen(kMuted);
        const QString etext =
            QStringLiteral("EL %1°").arg(qRound(m_elevation));
        p.drawText(QPointF(w - QFontMetrics(ef).horizontalAdvance(etext)
                               - 6.0, 14.0), etext);
    }

    // ── Readout under the rose ───────────────────────────────────────
    QFont big = p.font();
    big.setPixelSize(std::max(13, static_cast<int>(h * 0.075)));
    big.setBold(true);
    p.setFont(big);
    const QFontMetrics bfm(big);

    QString line1;
    QColor line1Col = kMuted;
    switch (m_state) {
    case State::Idle:
        line1 = degText(m_actual);
        line1Col = kActual;
        break;
    case State::Targeted:
        line1 = QStringLiteral("→ %1 %2").arg(degText(m_target),
                                              compassPoint(m_target));
        line1Col = kTarget;
        break;
    case State::Turning:
        line1 = QStringLiteral("%1 …").arg(degText(m_actual));
        line1Col = kTurning;
        break;
    case State::OnTarget:
        line1 = QStringLiteral("%1 %2").arg(degText(m_actual),
                                            compassPoint(m_actual));
        line1Col = kArrived;
        break;
    }
    p.setPen(line1Col);
    p.drawText(QPointF((w - bfm.horizontalAdvance(line1)) / 2.0, h * 0.84),
               line1);

    QFont small = p.font();
    small.setPixelSize(10);
    small.setBold(false);
    p.setFont(small);
    const QFontMetrics sfm(small);

    QString line2;
    switch (m_state) {
    case State::Idle:
        // Say what to do rather than leaving a blank strip.
        line2 = QStringLiteral("no target");
        break;
    case State::Targeted: {
        const double t = travelDegrees();
        line2 = QStringLiteral("now %1 · turn %2%3°")
                    .arg(degText(m_actual),
                         t >= 0 ? QStringLiteral("CW ") : QStringLiteral("CCW "))
                    .arg(qRound(std::abs(t)));
        break;
    }
    case State::Turning:
        line2 = QStringLiteral("%1° to go").arg(qRound(std::abs(travelDegrees())));
        break;
    case State::OnTarget:
        line2 = QStringLiteral("on target");
        break;
    }
    p.setPen(kMuted);
    p.drawText(QPointF((w - sfm.horizontalAdvance(line2)) / 2.0, h * 0.95),
               line2);
}

} // namespace NereusSDR
