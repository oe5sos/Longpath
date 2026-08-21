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

#include "core/AppSettings.h"
#include "gui/StyleConstants.h"

#include <QMouseEvent>
#include <QMenu>
#include <QContextMenuEvent>
#include <QActionGroup>
#include <QPainter>
#include <QStringList>
#include <QRadialGradient>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

// Palette taken from Style:: so the dial matches the rest of the app
// rather than carrying its own look. The four needle states reuse the
// existing semantic colours: accent for the target, amber for motion,
// green for arrival.
// ── Dieselbe Handschrift wie die uebrigen Instrumente ────────────────
//
// Der Betreiber, 2026-08-20: „weiters sieht die grafik des rotors
// nicht im stil der anderen grafiken aus, bitte aendern."
//
// Er hat recht, und der Unterschied ist benennbar: die Zeiger- und
// Balkeninstrumente fuehren jeden GEMESSENEN Wert in Bernstein
// (Instrument::measured(), Rolle „measured"), die Teilung in
// kTextScale und die Beschriftung in der Schmalschrift
// Style::monoFont. Der Rotor zeichnete seinen Zeiger in kTextPrimary —
// dasselbe Grau wie ein Beschriftungstext. Neben einem
// Stehwellenzeiger in Bernstein sieht das aus wie ein Bauteil aus
// einem anderen Programm.
//
// Die Richtung, in die die Antenne zeigt, IST eine Messung. Sie
// bekommt dieselbe Farbe wie jede andere.
//
// Das Ziel bleibt im Akzentblau: es ist keine Messung, sondern eine
// Vorgabe, und der Unterschied zwischen „wo sie steht" und „wo sie
// hin soll" ist genau der, den man auf einen Blick lesen will.
const QColor kBackground(Style::kPanelBg);      // #0a0a18
const QColor kActual    (Style::kAmberText);    // where it is — gemessen
const QColor kTarget    (Style::kAccent);       // where it should go
const QColor kTurning   (Style::kAmberText);    // in motion
const QColor kArrived   (Style::kGreenText);    // on target
// ── Ring und Teilung eine Stufe heller ───────────────────────────────
//
// Der Betreiber, 2026-08-20: „grafik rotor auch leicht aufhellen."
//
// kBorder und kBorderSubtle sind RANDfarben — sie trennen Flaechen und
// duerfen dabei leise sein. Auf einem Zifferblatt sind dieselben
// Striche aber die TEILUNG, also das, was man ablesen soll. Eine
// Randfarbe als Skala ist zu zurueckhaltend fuer ihre Aufgabe.
//
// Eine Stufe hoeher in derselben Leiter: kTextScale fuer den aeusseren
// Ring, kBorder fuer den inneren. Die Abstufung zwischen beiden
// bleibt, sie liegt nur hoeher.
const QColor kRing      (Style::kTextScale);
const QColor kRingInner (Style::kBorder);
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
    // Through setHint rather than setToolTip: the simulated warning
    // also wants the tooltip, and refreshTooltip() is what keeps the
    // two from overwriting each other.
    setHint(QStringLiteral("Click inside the rose to aim the antenna"));

    // Die gemerkte Form. Vorgabe Vollkreis — so hat es der Betreiber
    // am 2026-08-21 entschieden („beide zur auswahl, standard
    // vollkreis").
    m_shape = AppSettings::instance()
                  .value(QStringLiteral("RotorDialShape"),
                         QStringLiteral("Rose")).toString()
                      == QStringLiteral("Tape")
                  ? Shape::Tape : Shape::Rose;

    // Die Wahl gehoert an das Ding selbst, nicht in einen Dialog: man
    // entscheidet sie, waehrend man es ansieht.
    setContextMenuPolicy(Qt::DefaultContextMenu);
}

void RotorDialWidget::contextMenuEvent(QContextMenuEvent* ev)
{
    QMenu menu(this);
    auto* group = new QActionGroup(&menu);
    group->setExclusive(true);

    const struct { const char* label; Shape shape; const char* tip; } kForms[] = {
        {QT_TR_NOOP("Vollkreis"), Shape::Rose,
         QT_TR_NOOP("Ganze Windrose, Norden oben. Zeigt auch, was hinter "
                    "der Antenne liegt.")},
        {QT_TR_NOOP("Peilband"), Shape::Tape,
         QT_TR_NOOP("Ein Band um die Antenne statt eines Kreises. Nutzt "
                    "die volle Breite — dafuer fehlt das Rundherum.")},
    };
    for (const auto& f : kForms) {
        QAction* a = menu.addAction(tr(f.label));
        a->setCheckable(true);
        a->setChecked(m_shape == f.shape);
        a->setToolTip(tr(f.tip));
        group->addAction(a);
        const Shape target = f.shape;
        connect(a, &QAction::triggered, this, [this, target]() {
            setShape(target);
        });
    }
    menu.exec(ev->globalPos());
    ev->accept();
}

QSize RotorDialWidget::sizeHint()        const { return {190, 210}; }

// Small enough that the panel can be dragged down to a compass and
// nothing else. Below about this the ticks stop being distinguishable
// and the needles overlap the hub, so it is a floor rather than a
// preference. (2026-08-10 — was {130, 150}, which set the panel's
// minimum height high enough that the dial was the thing squeezed.)
QSize RotorDialWidget::minimumSizeHint() const { return {84, 84}; }

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
    refreshTooltip();
    update();
}

void RotorDialWidget::setHint(const QString& text)
{
    if (m_hint == text) { return; }
    m_hint = text;
    refreshTooltip();
}

void RotorDialWidget::refreshTooltip()
{
    QStringList parts;
    // The full sentence lives here, where it fits at every size. The
    // dashed amber ring and the three letters on the face are the
    // summary.
    if (m_simulated) {
        parts << QStringLiteral(
            "No rotator is connected. This needle is driven by a timer, "
            "not by the mast — nothing has moved.");
    }
    if (!m_hint.isEmpty()) { parts << m_hint; }
    setToolTip(parts.join(QStringLiteral("\n\n")));
}

// ── Geometry, shared by drawing and hit-testing ─────────────────────

bool RotorDialWidget::isLandscape() const
{
    // Deutlich breiter als hoch, und hoch genug, dass eine Rose ueber
    // die volle Hoehe noch lesbar ist. Unterhalb davon greift weiter
    // isCompassOnly: dort ist fuer eine Ablesung ohnehin kein Platz,
    // egal auf welcher Seite.
    return width() > height() * 1.7 && height() >= 120;
}

bool RotorDialWidget::isCompassOnly() const
{
    // The rose normally sits in the top 84% and the two readout lines
    // take the rest. Dragged small, that split leaves a thumbnail rose
    // above text nobody can read — the worst of both. Below this size
    // the readout goes and the rose takes the whole face.
    return height() < 150 || width() < 118;
}

QPointF RotorDialWidget::roseCentre() const
{
    if (isLandscape()) {
        // ── Die GRUPPE mittig, nicht die Rose links ──────────────────
        //
        // Zuerst stand die Rose am linken Rand und die Ablesung
        // daneben; in einem breiten Fenster blieb rechts die halbe
        // Flaeche leer, und es sah aus, als klebte das Zifferblatt
        // fest. Der Betreiber: „rotor log veraendert sich nicht
        // proportional mit dem fenster."
        //
        // Rose und Ablesung gehoeren zusammen. Die BEIDEN werden
        // mittig gesetzt, dann sitzt das Paar in der Flaeche statt an
        // ihrem Rand.
        //
        // Die Breite der Ablesung ist beim Rechnen der Geometrie noch
        // nicht bekannt (sie haengt an Zustand und Schriftgroesse).
        // Geschaetzt wird sie mit dem Radius: die laengste Zeile ist
        // „-> 310° NW", und die passt in etwa einen Radius.
        const double r = roseRadius();
        const double groupW = 2.0 * r + 22.0 + r;   // Rose + Luft + Text
        const double left = qMax(6.0, (width() - groupW) / 2.0);
        return {left + r, height() * 0.5};
    }
    return {width() * 0.5, height() * (isCompassOnly() ? 0.5 : 0.42)};
}

double RotorDialWidget::roseRadius() const
{
    if (isLandscape()) {
        // Die volle Hoehe, nicht 36 % davon. Nach oben begrenzt, damit
        // die Rose in einer sehr hohen, sehr breiten Flaeche nicht die
        // Ablesung erdrueckt.
        return std::min(height() * 0.46 - 4.0, width() * 0.34);
    }
    return isCompassOnly()
               ? std::min(width(), height()) * 0.46 - 3.0
               : std::min(width() * 0.42, height() * 0.36);
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
    if (m_shape == Shape::Tape) { return bearingAtTape(pos); }
    const QPointF p = pos - roseCentre();
    // The dead zone scales with the rose. A fixed 8 px hub was most of
    // a compass-only dial, so on a small one the middle third of the
    // face quietly ignored clicks.
    const double dead = std::max(5.0, roseRadius() * 0.16);
    if (std::hypot(p.x(), p.y()) < dead) { return -1.0; }
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

QImage RotorDialWidget::renderTransparent(int sidePx, qreal dpr)
{
    if (sidePx < 24) { return {}; }
    QImage img(QSize(sidePx, sidePx) * dpr, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::transparent);

    // Groesse und Zustand vorruebergehend umstellen, dann zurueck. Das
    // Widget bleibt dabei sichtbar oder unsichtbar wie es war — es
    // wird nur einmal in ein Bild gemalt.
    const QSize was = size();
    m_bare = true;
    resize(sidePx, sidePx);
    render(&img, QPoint(), QRegion(), QWidget::DrawChildren);
    m_bare = false;
    resize(was);
    return img;
}

// ── Form waehlen ─────────────────────────────────────────────────────
//
// Gemerkt, weil es eine Entscheidung ist und keine Geste: wer auf das
// Band umstellt, tut das nicht fuer eine Sitzung.
void RotorDialWidget::setShape(Shape s)
{
    if (m_shape == s) { return; }
    m_shape = s;
    AppSettings::instance().setValue(
        QStringLiteral("RotorDialShape"),
        s == Shape::Tape ? QStringLiteral("Tape") : QStringLiteral("Rose"));
    update();
}

// ── Das Peilband ─────────────────────────────────────────────────────
//
// Ein Ausschnitt von 240 Grad, die Antenne fest in der Mitte. Es liest
// sich wie ein Massband, das unter einem festen Zeiger durchlaeuft —
// beim Drehen wandert die Skala, nicht die Marke.
//
// Der Gewinn ist die Flaeche: das Band nutzt die volle Breite, die ein
// Kreis nicht fuellen kann. Der Preis ist das Rundherum — was hinter
// der Antenne liegt, steht ausserhalb des Ausschnitts. Deshalb eine
// Wahl und keine Ablösung.
void RotorDialWidget::paintTape(QPainter& p)
{
    const double w = width();
    const double h = height();
    const double pad = 14.0;
    const double bx = pad, bw = w - 2.0 * pad;
    if (bw < 80.0) { return; }

    const double mid = bx + bw / 2.0;
    const double ppd = bw / kTapeSpanDeg;          // Punkte je Grad

    // ── Die Teilung waechst mit der Hoehe ────────────────────────────
    //
    // Erst standen hier feste Laengen (16 / 11 / 6 Punkte). In einer
    // Flaeche von 1170 x 330 klebte das ganze Band dann auf einem
    // duennen Streifen in der Mitte, und ringsum war Platz, den es
    // nicht nutzte — genau der Vorwurf, den das Band eigentlich
    // aufloesen soll.
    //
    // `unit` ist die Laenge des laengsten Strichs; alles andere haengt
    // daran. Nach unten begrenzt, damit es in einem flachen Streifen
    // nicht verschwindet, nach oben, damit es in einem hohen Fenster
    // nicht zum Zaun wird.
    const double unit = qBound(14.0, h * 0.22, 64.0);
    const double yb   = m_bare ? h * 0.62 : h * 0.56 + unit * 0.25;

    auto px = [&](double deg) {
        return mid + std::fmod(deg - m_actual + 540.0, 360.0) - 180.0 <= 0.0
                   ? mid + (std::fmod(deg - m_actual + 540.0, 360.0) - 180.0) * ppd
                   : mid + (std::fmod(deg - m_actual + 540.0, 360.0) - 180.0) * ppd;
    };

    // Keule als warmes Feld um die Mitte
    if (m_beamWidth > 0.5) {
        QColor wedge = kActual;
        wedge.setAlpha(34);
        p.fillRect(QRectF(px(m_actual - m_beamWidth / 2.0), yb - unit * 1.15,
                          m_beamWidth * ppd, unit * 1.15), wedge);
    }

    // Teilung: 5 Grad fein, 30 mittel, 90 lang
    QFont f = Style::monoFont(p.font(), 10);
    for (int d = 0; d < 360; d += 5) {
        const double x = px(d);
        if (x < bx - 6.0 || x > bx + bw + 6.0) { continue; }
        const bool cardinal = (d % 90) == 0;
        const bool major    = (d % 30) == 0;
        const double len = unit * (cardinal ? 1.0 : major ? 0.68 : 0.34);
        p.setPen(QPen(cardinal ? kCardinal : major ? kRing : kRingInner,
                      cardinal ? 1.8 : major ? 1.1 : 0.7));
        p.drawLine(QPointF(x, yb - len), QPointF(x, yb));
        if (!major) { continue; }
        f.setPixelSize(qBound(9, int(unit * 0.30), 15)
                       + (cardinal ? 2 : 0));
        f.setBold(cardinal);
        p.setFont(f);
        // Die vier Himmelsrichtungen in der Textfarbe: sie sind die
        // Orientierung, nicht Beiwerk. Die Zahlen dazwischen bleiben
        // leiser.
        p.setPen(cardinal ? QColor(Style::kTextPrimary) : kMuted);
        const QString lab = (d == 0)   ? QStringLiteral("N")
                          : (d == 90)  ? QStringLiteral("E")
                          : (d == 180) ? QStringLiteral("S")
                          : (d == 270) ? QStringLiteral("W")
                                       : QString::number(d);
        p.drawText(QPointF(x - QFontMetrics(f).horizontalAdvance(lab) / 2.0,
                           yb + QFontMetrics(f).ascent() + 5.0), lab);
    }

    // Ziel
    if (m_hasTarget) {
        const double tx = px(m_target);
        if (tx > bx - 8.0 && tx < bx + bw + 8.0) {
            QPen pen(m_state == State::OnTarget ? kArrived : kTarget, 1.6);
            pen.setStyle(Qt::DashLine);
            pen.setDashPattern({2.6, 2.2});
            p.setPen(pen);
            p.drawLine(QPointF(tx, yb - unit * 1.15), QPointF(tx, yb));
            QPolygonF mark;
            mark << QPointF(tx, yb) << QPointF(tx - 5.0, yb + 9.0)
                 << QPointF(tx + 5.0, yb + 9.0);
            p.setPen(Qt::NoPen);
            p.setBrush(m_state == State::OnTarget ? kArrived : kTarget);
            p.drawPolygon(mark);
            p.setBrush(Qt::NoBrush);
        }
    }

    // Die feste Marke in der Mitte: SIE ist die Antenne.
    const QColor actualCol = m_simulated                 ? kMuted
                           : (m_state == State::Turning) ? kTurning
                           : (m_state == State::OnTarget)? kArrived
                                                         : kActual;
    const double headY = yb - unit * 1.45;
    p.setPen(QPen(actualCol, 2.6));
    p.drawLine(QPointF(mid, headY), QPointF(mid, yb + 4.0));
    QPolygonF head;
    head << QPointF(mid, headY)
         << QPointF(mid - 7.0, headY - 13.0)
         << QPointF(mid + 7.0, headY - 13.0);
    p.setPen(Qt::NoPen);
    p.setBrush(actualCol);
    p.drawPolygon(head);
    p.setBrush(Qt::NoBrush);

    if (m_bare) { return; }

    QFont big = Style::monoFont(p.font(), 13);
    big.setPixelSize(std::max(15, static_cast<int>(h * 0.13)));
    big.setBold(true);
    p.setFont(big);
    p.setPen(actualCol);
    p.drawText(QPointF(bx, h - 10.0), degText(m_actual));

    QFont small = Style::monoFont(p.font(), 11);
    small.setPixelSize(11);
    p.setFont(small);
    p.setPen(kMuted);
    QString line2 = m_hasTarget
        ? QStringLiteral("Ziel %1 · %2%3°")
              .arg(degText(m_target),
                   travelDegrees() >= 0 ? QStringLiteral("CW ")
                                        : QStringLiteral("CCW "))
              .arg(qRound(std::abs(travelDegrees())))
        : QStringLiteral("kein Ziel");
    p.drawText(QPointF(bx + QFontMetrics(big).horizontalAdvance("000°") + 14.0,
                       h - 12.0), line2);
}

double RotorDialWidget::bearingAtTape(const QPointF& pos) const
{
    const double pad = 14.0;
    const double bw = width() - 2.0 * pad;
    if (bw < 80.0) { return -1.0; }
    const double mid = pad + bw / 2.0;
    const double ppd = bw / kTapeSpanDeg;
    return norm360(m_actual + (pos.x() - mid) / ppd);
}

void RotorDialWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (m_shape == Shape::Tape) {
        if (!m_bare) { p.fillRect(rect(), kBackground); }
        paintTape(p);
        return;
    }

    const double w = width();
    const double h = height();
    // m_bare: die Rose soll mit dem Panadapter EINS sein, nicht als
    // Kasten davor liegen. Also kein Grund und weiter unten keine
    // Ablesung — nur Ring, Teilung, Zeiger und Ziel.
    if (!m_bare) { p.fillRect(rect(), kBackground); }

    // Faint accent wash from below so the face is not dead flat. Kept
    // subtle and in the app's accent hue rather than a warm lamp — the
    // surrounding panels are cool-toned.
    // Der Schimmer gehoert zur Flaeche, nicht zur Rose. Ohne Grund
    // haette er nichts, worauf er faellt — also im durchsichtigen
    // Fall ueberspringen.
    //
    // Als Block, nicht als Sprung: STYLEGUIDE.md verbietet goto, und
    // hier zu Recht — der Sprung haette Initialisierungen uebersprungen,
    // was der Uebersetzer ohnehin zurueckgewiesen hat.
    if (!m_bare) {
    QRadialGradient glow(QPointF(w * 0.5, h * 1.10), w * 0.85);
    QColor glowInner = QColor(Style::kAccent);
    glowInner.setAlpha(26);
    glow.setColorAt(0.0, glowInner);
    glow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.fillRect(rect(), glow);
    }

    // ── Und eine Lampe hinter der Rose ───────────────────────────────
    //
    // Dasselbe Mittel wie bei den Zeigerinstrumenten (siehe
    // NeedleInstrument::paintEvent): ein sehr schwacher Verlauf aus der
    // Mitte, in der Messfarbe. Er beleuchtet die ganze Scheibe, statt
    // nur den Zeiger heller zu machen — „von hinten leicht
    // beleuchten", wie es hiess.
    //
    // Auch im durchsichtigen Fall: gerade dort, ueber dem Spektrum,
    // braucht die Rose etwas, das sie vom Untergrund abhebt.
    {
        const QPointF lc = roseCentre();
        const double  lr = roseRadius();
        QRadialGradient lamp(lc, lr * 1.05);
        QColor warm(Style::kAmberText);
        warm.setAlphaF(m_bare ? 0.10 : 0.12);
        lamp.setColorAt(0.0, warm);
        warm.setAlphaF(0.0);
        lamp.setColorAt(1.0, warm);
        p.setPen(Qt::NoPen);
        p.setBrush(lamp);
        p.drawEllipse(lc, lr * 1.05, lr * 1.05);
        p.setBrush(Qt::NoBrush);
    }

    // Compass-only mode (2026-08-10). The geometry lives in
    // isCompassOnly/roseCentre/roseRadius so that bearingAt() cannot
    // drift away from it — see the note on those declarations.
    const bool compassOnly = isCompassOnly();
    const QPointF c = roseCentre();
    const double  r = roseRadius();

    // Rings. The outer one turns amber and dashed while the needle is
    // invented — a mark that costs no space, so it survives at any size
    // the dial can be dragged to. The words below may not fit; this
    // always does.
    if (m_simulated) {
        QPen warn(QColor(Style::kAmberWarn), 1.2);
        warn.setStyle(Qt::DashLine);
        p.setPen(warn);
    } else {
        p.setPen(QPen(kRing, 1));
    }
    p.drawEllipse(c, r, r);
    p.setPen(QPen(kRing, 1));
    p.setPen(QPen(kRingInner, 1));
    p.drawEllipse(c, r * 0.83, r * 0.83);

    // ── Die Teilung: drei Stufen statt einer ─────────────────────────
    //
    // Hier stand eine Teilung alle 30° und sonst nichts. Der Betreiber
    // hat am 2026-08-20 um „neue, passende und aufwendigere Grafiken"
    // gebeten, und eine Windrose mit zwoelf Strichen ist fuer ein
    // Instrument, an dem man Grade ablesen soll, zu grob.
    //
    // Drei Stufen, wie an jedem Kompass: alle 10° ein kurzer Strich,
    // alle 30° ein laengerer, auf den Haupthimmelsrichtungen der
    // laengste. Die Abstufung macht das Zaehlen ueberfluessig — man
    // sieht die Zehner, ohne sie abzuzaehlen.
    for (int deg = 0; deg < 360; deg += 10) {
        const double a = bearingToRadians(deg);
        const bool cardinal = (deg % 90) == 0;
        const bool major    = (deg % 30) == 0;
        const double r0 = r * (cardinal ? 0.86 : major ? 0.90 : 0.94);
        p.setPen(QPen(cardinal ? kMuted : kRing,
                      cardinal ? 1.5 : major ? 1.1 : 0.7));
        p.drawLine(QPointF(c.x() + r0 * std::cos(a), c.y() - r0 * std::sin(a)),
                   QPointF(c.x() + r  * std::cos(a), c.y() - r  * std::sin(a)));
    }

    // ── Gradzahlen alle 30°, wenn Platz ist ──────────────────────────
    //
    // Nur ab einem Radius, bei dem sie sich nicht beruehren, und ohne
    // die vier Himmelsrichtungen: dort stehen schon N/E/S/W, und eine
    // „0" unter dem N waere doppelt gemoppelt.
    if (r > 84.0) {
        QFont df = Style::monoFont(p.font(), 9);
        df.setPixelSize(9);
        p.setFont(df);
        const QFontMetrics dfm(df);
        QColor degInk(Style::kTextScale);
        degInk.setAlpha(150);
        p.setPen(degInk);
        for (int deg = 30; deg < 360; deg += 30) {
            if (deg % 90 == 0) { continue; }
            const double a  = bearingToRadians(deg);
            const double rr = r * 0.755;
            const QString t = QStringLiteral("%1").arg(deg);
            p.drawText(QPointF(c.x() + rr * std::cos(a)
                                   - dfm.horizontalAdvance(t) / 2.0,
                               c.y() - rr * std::sin(a) + dfm.ascent() / 2.0),
                       t);
        }
    }

    // Cardinal letters
    //
    // In derselben Schmalschrift wie die Teilung der uebrigen
    // Instrumente (InstrumentPainter benutzt Style::monoFont fuer die
    // Skalenbeschriftung). Eine Windrose in der Fliesstextschrift neben
    // Zifferblaettern in Schmalschrift war der zweite Teil dessen, was
    // der Betreiber am 2026-08-20 als „nicht im stil der anderen
    // grafiken" gesehen hat — der erste war die Farbe des Zeigers.
    QFont cf = Style::monoFont(p.font(), 11);
    cf.setPixelSize(11);
    p.setFont(cf);
    p.setPen(kCardinal);
    const QFontMetrics cfm(cf);
    const struct { const char* s; int deg; } kCards[] = {
        {"N", 0}, {"E", 90}, {"S", 180}, {"W", 270}};
    // ── Die Himmelsrichtungen INNEN, im Ring der Gradzahlen ─────────
    //
    // Sie standen AUSSERHALB der Rose (r + 11). Damit lag das „S"
    // unter dem unteren Ringrand — genau dort, wo die Ablesung
    // beginnt, und auf dem Bild vom 2026-08-20 steckte es in der
    // „120°".
    //
    // Auf demselben Radius wie die Gradzahlen ergeben die vier
    // Buchstaben und die acht Zahlen EINEN Beschriftungsring statt
    // zweier, die Rose gewinnt aussen 11 px, und unter ihr bleibt die
    // Flaeche frei fuer das, was dort hingehoert.
    for (const auto& card : kCards) {
        const double a = bearingToRadians(card.deg);
        const double rr = r * 0.755;
        const QString s = QString::fromLatin1(card.s);
        p.drawText(QPointF(c.x() + rr * std::cos(a) - cfm.horizontalAdvance(s) / 2.0,
                           c.y() - rr * std::sin(a) + cfm.ascent() / 2.0),
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
        // ── Die Keule sitzt auf der Antennenrichtung ────────────────
        //
        // Hier stand `- m_beamWidth / 2.0`. Qt zaehlt von 3 Uhr gegen
        // den Uhrzeigersinn, eine Peilung von Nord im Uhrzeigersinn;
        // die Umrechnung ist Qt = 90 - Peilung. Der Sektor soll um
        // diesen Wert HERUM liegen, also bei Qt+Haelfte anfangen und
        // mit negativer Spanne darueber hinweglaufen.
        //
        // Mit dem Minus fing er eine halbe Keulenbreite zu frueh an und
        // lief eine halbe zu frueh aus — die ganze Keule stand um ihre
        // eigene Breite neben dem Zeiger. Beim Rendern am 2026-08-20
        // sofort zu sehen: Zeiger auf 45°, Keule nach Osten.
        const int startQt = static_cast<int>((90.0 - m_actual + m_beamWidth / 2.0) * 16);
        const int spanQt  = static_cast<int>(-m_beamWidth * 16);
        QColor wedge = kActual;
        wedge.setAlpha(16);
        p.setPen(Qt::NoPen);
        p.setBrush(wedge);
        p.drawPie(box, startQt, spanQt);
        p.setBrush(Qt::NoBrush);
    }

    // ── Der Zeiger ───────────────────────────────────────────────────
    //
    // Bisher ein Strich von der Mitte nach aussen. Ein Instrumenten-
    // zeiger ist etwas anderes: er verjuengt sich zur Spitze, und er
    // hat hinter der Achse ein kurzes Gegengewicht. Beides hat einen
    // Zweck, nicht nur ein Aussehen — die Verjuengung sagt, welches
    // Ende die Ablesung ist, und das Gegengewicht macht die Drehachse
    // als Achse kenntlich statt als Anfangspunkt eines Strichs.
    //
    // Der gestrichelte Zielzeiger bleibt ein Strich: er ist eine
    // Vorgabe, kein Messwerk, und soll auch so aussehen.
    auto drawNeedle = [&](double deg, const QColor& col, double len,
                          bool dashed, double width) {
        const double a = bearingToRadians(deg);
        const QPointF dir(std::cos(a), -std::sin(a));
        const QPointF nrm(-dir.y(), dir.x());
        const QPointF tip(c.x() + r * len * dir.x(),
                          c.y() + r * len * dir.y());

        if (dashed) {
            QColor halo = col;
            halo.setAlpha(70);
            p.setPen(QPen(halo, width + 2.6, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(c, tip);
            QPen pen(col, width, Qt::DashLine, Qt::RoundCap);
            pen.setDashPattern({2.2, 1.6});
            p.setPen(pen);
            p.drawLine(c, tip);
            return;
        }

        const double halfW = width * 0.9;
        const double tailL = r * 0.16;
        const QPointF tail(c.x() - tailL * dir.x(), c.y() - tailL * dir.y());

        QPolygonF body;
        body << tip
             << QPointF(c.x() + nrm.x() * halfW, c.y() + nrm.y() * halfW)
             << QPointF(tail.x() + nrm.x() * halfW * 0.7,
                        tail.y() + nrm.y() * halfW * 0.7)
             << QPointF(tail.x() - nrm.x() * halfW * 0.7,
                        tail.y() - nrm.y() * halfW * 0.7)
             << QPointF(c.x() - nrm.x() * halfW, c.y() - nrm.y() * halfW);

        QColor halo = col;
        halo.setAlpha(60);
        // Vierter Parameter ist die KAPPE, nicht die Ecke — der
        // Eckenstil kommt danach.
        QPen haloPen(halo, 3.0, Qt::SolidLine, Qt::RoundCap);
        haloPen.setJoinStyle(Qt::RoundJoin);
        p.setPen(haloPen);
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(body);

        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawPolygon(body);
        p.setBrush(Qt::NoBrush);
    };

    // ── Die Zielmarke am Rand ────────────────────────────────────────
    //
    // Ein gestrichelter Strich sagt die Richtung, aber nicht genau, wo
    // sie den Rand trifft. Ein kleines Dreieck auf dem Ring tut das —
    // und bleibt lesbar, wenn Zeiger und Ziel dicht beieinander
    // stehen, wo sich zwei Striche sonst zu einem verwischen.
    if (m_hasTarget) {
        const double a = bearingToRadians(m_target);
        const QPointF dir(std::cos(a), -std::sin(a));
        const QPointF nrm(-dir.y(), dir.x());
        const QPointF onRim(c.x() + r * dir.x(), c.y() + r * dir.y());
        const QPointF inner(c.x() + (r - 9.0) * dir.x(),
                            c.y() + (r - 9.0) * dir.y());
        QPolygonF mark;
        mark << onRim
             << QPointF(inner.x() + nrm.x() * 4.5, inner.y() + nrm.y() * 4.5)
             << QPointF(inner.x() - nrm.x() * 4.5, inner.y() - nrm.y() * 4.5);
        p.setPen(Qt::NoPen);
        p.setBrush(m_state == State::OnTarget ? kArrived : kTarget);
        p.drawPolygon(mark);
        p.setBrush(Qt::NoBrush);
    }

    // Target first so the actual needle reads on top of it.
    if (m_hasTarget && m_state != State::OnTarget) {
        drawNeedle(m_target, kTarget, 0.90, /*dashed=*/true, 2.4);
    }

    // A simulated needle is drawn dashed and dim. It is the same shape
    // as a real reading otherwise, and an operator who mistakes one for
    // the other turns the wrong way at three in the morning — or,
    // worse, believes the antenna moved when it did not. (2026-08-10)
    // ── Der Zeiger spricht dieselbe Sprache wie Nabe und Ablesung ────
    //
    // Hier stand fuer BEIDE Zustaende — dreht und am Ziel — dasselbe
    // Rot, waehrend die Nabe darunter schon amber (dreht) und gruen
    // (am Ziel) faerbte und die Ablesung ebenso. Drei Stellen
    // desselben Instruments sagten damit zwei verschiedene Dinge, und
    // das Rot behauptete oben Gefahr, wo unten „angekommen" stand.
    //
    // Jetzt einheitlich: bernsteinfarben in Ruhe (gemessen), waehrend
    // der Fahrt dasselbe Amber wie die Nabe, am Ziel gruen.
    const QColor actualCol = m_simulated                  ? kMuted
                           : (m_state == State::Turning)  ? kTurning
                           : (m_state == State::OnTarget) ? kArrived
                                                          : kActual;
    drawNeedle(m_actual, actualCol, 0.90, m_simulated, m_simulated ? 2.0 : 2.6);

    // Hub
    const QColor hubCol = (m_state == State::Turning)  ? kTurning
                        : (m_state == State::OnTarget) ? kArrived
                                                       : QColor(Style::kAmberText);
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
        sf.setPixelSize(11);
        sf.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
        p.setFont(sf);
        p.setPen(QColor(Style::kAmberWarn));
        // Three letters when there is no room for eleven. The dashed
        // amber ring carries the meaning either way; this only names
        // it.
        p.drawText(QPointF(6.0, 14.0),
                   w < 150.0 ? QStringLiteral("SIM")
                             : QStringLiteral("NO ROTATOR"));
    }

    if (m_hasElevation) {
        QFont ef = p.font();
        ef.setPixelSize(11);
        p.setFont(ef);
        p.setPen(kMuted);
        const QString etext =
            QStringLiteral("EL %1°").arg(qRound(m_elevation));
        p.drawText(QPointF(w - QFontMetrics(ef).horizontalAdvance(etext)
                               - 6.0, 14.0), etext);
    }

    // ── Readout under the rose ───────────────────────────────────────
    //
    // Skipped entirely when the widget is small: there is no room under
    // the rose because the rose now fills the widget, and drawing over
    // it would be worse than saying nothing.
    if (compassOnly || m_bare) { return; }

    // Die Ablesung in Schmalschrift — wie die Zahlenfelder der
    // Zeigerinstrumente. Ziffern, die untereinander stehen sollen,
    // gehoeren in eine Schrift mit gleichen Ziffernbreiten; sonst
    // wandert die Zahl bei jeder Aenderung seitlich.
    QFont big = Style::monoFont(p.font(), 13);
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
    // ── Die Ablesung gehoert UNTER die Rose ──────────────────────────
    //
    // Hier stand h * 0.84 — eine feste Zahl, die nichts von der Rose
    // weiss. Bei 360x400 landete die Grundlinie damit genau auf dem
    // „S" der Windrose; auf dem Bild vom 2026-08-20 steckte die 045°
    // im S.
    //
    // Jetzt aus der Geometrie: unterer Rand der Rose, plus die
    // Oberlaenge der Schrift, plus Luft. Damit stimmt es bei jeder
    // Groesse, statt bei einer.
    // Im Querformat NEBEN die Rose, sonst darunter.
    //
    // Unter einer Rose, die die volle Hoehe nimmt, ist kein Platz mehr
    // — und rechts davon liegt die Flaeche brach, die den flachen
    // Streifen ueberhaupt erst breit macht.
    const bool land = isLandscape();
    const double readoutX = land
        ? c.x() + r + 22.0
        : (w - bfm.horizontalAdvance(line1)) / 2.0;
    const double readoutY = land
        ? c.y() - 4.0
        : qMin(h - 6.0, c.y() + r + bfm.ascent() + 8.0);
    p.setPen(line1Col);
    p.drawText(QPointF(readoutX, readoutY), line1);

    QFont small = Style::monoFont(p.font(), 11);
    small.setPixelSize(11);
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
    // Zweite Zeile immer unter der ersten, mit derselben Rechnung
    // statt einer zweiten festen Zahl.
    p.drawText(QPointF(land ? readoutX
                            : (w - sfm.horizontalAdvance(line2)) / 2.0,
                       land ? readoutY + sfm.height() + 8.0
                            : qMin(h - 4.0, readoutY + sfm.height() + 6.0)),
               line2);
}

} // namespace Longpath
