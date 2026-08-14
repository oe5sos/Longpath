// =================================================================
// src/gui/widgets/SwrCurveWidget.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See SwrCurveWidget.h for why the three band
// verticals are the whole point.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/widgets/SwrCurveWidget.h"

#include "gui/StyleConstants.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

#include <algorithm>
#include <limits>
#include <cmath>

namespace NereusSDR {
namespace {

const QColor kCurve   (Style::kAccent);
const QColor kBandFill(Style::kAccent);
const QColor kEdge    (Style::kTextSecondary);
const QColor kCentre  (Style::kGreenText);
const QColor kResonant(Style::kAmberText);
const QColor kLimit   (Style::kAmberBorder);
const QColor kGrid    (Style::kBorderSubtle);
const QColor kText    (Style::kTextPrimary);
const QColor kDim     (Style::kTextScale);
const QColor kBad     (Style::kRedBorder);

QString mhz(double hz, int digits = 3)
{
    return QStringLiteral("%1").arg(hz / 1e6, 0, 'f', digits);
}

} // namespace

SwrCurveWidget::SwrCurveWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(260, 150);
    // Without this Qt only reports moves while a button is held, and a
    // readout you have to drag for is worse than no readout.
    setMouseTracking(true);
}

double SwrCurveWidget::tickStepHz(double spanHz, int wanted) const
{
    if (spanHz <= 0.0 || wanted < 1) { return 1e6; }
    const double rough = spanHz / wanted;
    const double mag   = std::pow(10.0, std::floor(std::log10(rough)));
    const double n     = rough / mag;          // 1 .. 10
    // 1, 2, 5, 10 — the steps a person reads without arithmetic.
    const double nice = (n <= 1.5) ? 1.0
                      : (n <= 3.5) ? 2.0
                      : (n <= 7.5) ? 5.0
                                   : 10.0;
    return nice * mag;
}

double SwrCurveWidget::gapThresholdHz(const Sweep& s)
{
    // See the header for what this decides and why the median.
    if (s.points.size() <= 2) {
        return std::numeric_limits<double>::max();
    }
    QVector<double> steps;
    steps.reserve(s.points.size() - 1);
    for (int i = 1; i < s.points.size(); ++i) {
        steps.append(s.points.at(i).freqHz - s.points.at(i - 1).freqHz);
    }
    std::sort(steps.begin(), steps.end());
    const double median = steps.at(steps.size() / 2);
    // Four times the usual spacing: far enough above the jitter in a
    // file sweep never to fire by accident, far enough below a real
    // band gap always to fire. The narrowest gap in the HF plan is
    // 10.150 to 14.000 MHz, thousands of times a band sweep's step.
    return median > 0.0 ? median * 4.0
                        : std::numeric_limits<double>::max();
}

void SwrCurveWidget::drawBrokenCurve(QPainter& p, const Sweep& s) const
{
    if (s.points.isEmpty()) { return; }
    const double gapHz = gapThresholdHz(s);

    // ── One line, two kinds of ink ───────────────────────────────────
    //
    // "das ist keine durchgehende Linie" — and "über das hatten wir
    // aber jetzt schon oft geschrieben", which is fair. I broke the
    // curve at the band gaps and defended it three times.
    //
    // The concern was real and my answer to it was too blunt. Drawing a
    // solid stroke from 7.200 to 14.000 MHz claims a measurement that
    // does not exist. But leaving a hole throws away the thing the
    // operator is actually looking at on a nine-band sweep: the SHAPE,
    // where the antenna rises and falls across the whole range.
    //
    // Both are available. Solid where there are measurements, thin and
    // dashed across the gaps. The eye follows one line; the ink says
    // which parts were measured and which are just the two ends joined
    // up. That is what a plot with holes in it should look like, and it
    // is what I should have built when he first asked.
    QPen solid = p.pen();
    QPen bridge = solid;
    QColor faded = solid.color();
    faded.setAlpha(std::min(110, faded.alpha()));
    bridge.setColor(faded);
    bridge.setWidthF(std::max(0.8, solid.widthF() * 0.45));
    bridge.setStyle(Qt::DotLine);

    QPolygonF line;
    line.reserve(s.points.size());
    QPointF lastDrawn;
    double lastHz = 0.0;

    for (const SweepPoint& pt : s.points) {
        const QPointF here(xFor(pt.freqHz),
                           yFor(AntennaSweep::swr(pt.gamma)));
        if (!line.isEmpty() && (pt.freqHz - lastHz) > gapHz) {
            p.setPen(solid);
            p.drawPath(smoothPath(line));
            p.setPen(bridge);
            p.drawLine(lastDrawn, here);      // the join, marked as one
            line.clear();
        }
        line << here;
        lastDrawn = here;
        lastHz = pt.freqHz;
    }
    p.setPen(solid);
    p.drawPath(smoothPath(line));
}

// ── Round, not a chain of straight bits ──────────────────────────────
//
// "die Linie sollte aber wie eine Kurve aussehen, mit vielen Punkten,
//  rund!"
//
// A polyline through the samples is what a polyline looks like: on a
// band segment holding five points it is four visible straight strokes
// with corners. SWR does not have corners. The measurement is a
// smooth function sampled coarsely, and drawing it as a smooth curve
// through the samples is a better picture of it than joining the dots.
//
// Catmull-Rom, converted to cubic Béziers. It passes exactly THROUGH
// every measured point — nothing is moved, nothing is invented at a
// sample — and only the path between two samples is curved rather than
// straight.
//
// The one thing it can do wrong is overshoot: with a sharp dip the
// spline can bulge past the neighbouring values, and below SWR 1.0
// that would draw a physical impossibility. The control points are
// therefore clamped into the plot, so a bulge flattens instead of
// leaving the frame.
QPainterPath SwrCurveWidget::smoothPath(const QPolygonF& pts) const
{
    QPainterPath path;
    if (pts.isEmpty()) { return path; }
    path.moveTo(pts.first());
    if (pts.size() < 3) {
        for (int i = 1; i < pts.size(); ++i) { path.lineTo(pts.at(i)); }
        return path;
    }

    const double yTop = m_plotT;
    const double yBot = m_plotB;
    auto clampY = [yTop, yBot](QPointF q) {
        q.setY(std::clamp(q.y(), yTop, yBot));
        return q;
    };

    const int n = pts.size();
    for (int i = 0; i < n - 1; ++i) {
        const QPointF p0 = pts.at(i > 0 ? i - 1 : 0);
        const QPointF p1 = pts.at(i);
        const QPointF p2 = pts.at(i + 1);
        const QPointF p3 = pts.at(i + 2 < n ? i + 2 : n - 1);
        const QPointF c1 = clampY(p1 + (p2 - p0) / 6.0);
        const QPointF c2 = clampY(p2 - (p3 - p1) / 6.0);
        path.cubicTo(c1, c2, p2);
    }
    return path;
}

double SwrCurveWidget::xToHz(double x) const
{
    if (m_plotR <= m_plotL) { return m_viewLoHz; }
    const double t = (x - m_plotL) / (m_plotR - m_plotL);
    return m_viewLoHz + t * (m_viewHiHz - m_viewLoHz);
}

void SwrCurveWidget::mouseMoveEvent(QMouseEvent* e)
{
    const QPointF pos = e->position();
    // Only inside the frame. Off to the side there is no frequency to
    // report and a crosshair hanging in the margin invites reading one.
    const bool inside = pos.x() >= m_plotL && pos.x() <= m_plotR
                     && pos.y() >= m_plotT && pos.y() <= m_plotB;
    const QPointF next = inside ? pos : QPointF(-1.0, -1.0);
    if (next != m_cursor) {
        m_cursor = next;
        update();
    }
    QWidget::mouseMoveEvent(e);
}

void SwrCurveWidget::leaveEvent(QEvent* e)
{
    if (m_cursor.x() >= 0.0) {
        m_cursor = QPointF(-1.0, -1.0);
        update();
    }
    QWidget::leaveEvent(e);
}

QSize SwrCurveWidget::sizeHint()        const { return {620, 300}; }
QSize SwrCurveWidget::minimumSizeHint() const { return {260, 150}; }

void SwrCurveWidget::setSweep(const Sweep& s)
{
    m_sweep = s;
    recompute();
    update();
}

void SwrCurveWidget::setReference(const Sweep& s, const QString& label)
{
    m_reference = s;
    m_referenceLabel = label;
    update();
}

void SwrCurveWidget::clearReference()
{
    m_reference = Sweep{};
    m_referenceLabel.clear();
    update();
}

void SwrCurveWidget::setRegion(AmateurBands::Region r)
{
    if (m_region == r) { return; }
    m_region = r;
    recompute();
    update();
}

void SwrCurveWidget::setBand(const AmateurBands::Band& b)
{
    m_forcedBand = b;
    recompute();
    update();
}

void SwrCurveWidget::setTargetHz(double hz)
{
    if (qFuzzyCompare(m_targetHz + 1.0, hz + 1.0)) { return; }
    m_targetHz = hz;
    // ── The one setter that did not recompute ────────────────────────
    //
    // Every other one does. This did not, and the target feeds two
    // things recompute() decides:
    //
    //   * which band gets the three verticals, when the sweep spans
    //     several of them;
    //   * which resonance counts as "the" one, the nearest to it.
    //
    // So on an end-fed swept from 3 to 30 MHz, setting the target to
    // 7.100 left the band at 10 m — the widest overlap, which is what
    // bestOverlap() answers and precisely what the target exists to
    // override. It only ever took effect if a setSweep() or setRegion()
    // happened to follow, which in the window it usually did. Found by
    // a test written for something else.
    recompute();
    update();
}

void SwrCurveWidget::setSwrLimit(double limit)
{
    m_limit = std::max(1.05, limit);
    recompute();
    update();
}

void SwrCurveWidget::recompute()
{
    m_bands = AmateurBands::allOverlapping(m_sweep.startHz(),
                                           m_sweep.stopHz(), m_region);

    // ── Only bands that were actually measured ───────────────────────
    //
    // allOverlapping() works from the first and last frequency, which
    // is right for a sweep that is continuous between them and wrong
    // for one with holes. A range sweep may only key inside the
    // allocations, so 7.0 to 29.7 MHz can hold points on 40 m and 10 m
    // and nothing at all on the five bands between — and those five
    // were being listed, each with a row of "not swept".
    //
    // Found by a test that put points on two bands and got seven rows.
    // Same shape as everything else here: describing spectrum nobody
    // looked at.
    if (!m_sweep.isEmpty()) {
        QVector<AmateurBands::Band> measured;
        measured.reserve(m_bands.size());
        for (const AmateurBands::Band& b : m_bands) {
            for (const SweepPoint& p : m_sweep.points) {
                if (b.contains(p.freqHz)) { measured.append(b); break; }
            }
        }
        m_bands = measured;
    }

    m_band = m_forcedBand.isValid()
                 ? m_forcedBand
                 : AmateurBands::bestOverlap(m_sweep.startHz(),
                                             m_sweep.stopHz(), m_region);

    if (m_sweep.isEmpty()) {
        m_viewLoHz = m_viewHiHz = 0.0;
        m_resonance = {};
        m_best = {};
        m_all.clear();
        m_bands.clear();
        return;
    }

    // ── Air either side of the measurement ───────────────────────────
    //
    // "sollte nicht beginnen bei 7 MHz und enden bei 7,2 MHz, sondern
    //  den Bereich ein bisschen größer machen, sodass man die Kurve
    //  besser sieht."
    //
    // Two percent was not air, it was a hairline. A radio sweep spans
    // exactly the band — it may not legally transmit outside it — so
    // the curve ran from the left frame to the right frame with its
    // ends clipped into the border, and the band edges, which are the
    // things being read off, sat underneath that border.
    //
    // Twelve percent each side. The measurement occupies the middle
    // three quarters and both band edges stand clear of the frame with
    // room for their labels. The empty margin is honest: nothing was
    // measured out there and nothing is drawn out there.
    //
    // This widens the VIEW, not the sweep. Transmitting past a band
    // edge to make a picture nicer is not a trade available to us.
    // Not below DC. Twelve percent of a wide sweep is a lot of hertz:
    // 1.8–30 MHz pads by 3.4 MHz and the axis would start at −1.58 MHz,
    // which is not a frequency. Nothing crashed — xFor is linear and
    // draws it happily — it just put a tick labelled "0" on the left and
    // wasted a tenth of the width on spectrum that cannot exist. Found
    // by working the tick spacing out on paper for a full-HF sweep.
    const double span = std::max(1.0, m_sweep.stopHz() - m_sweep.startHz());
    m_viewLoHz = std::max(0.0, m_sweep.startHz() - span * 0.12);
    m_viewHiHz = m_sweep.stopHz()  + span * 0.12;

    // Resonance nearest the target if one was given, otherwise nearest
    // the middle of the band — which is what somebody without a
    // specific frequency in mind actually means.
    const double near = m_targetHz > 0.0 ? m_targetHz
                      : m_band.isValid() ? m_band.centreHz()
                                         : 0.0;
    m_resonance = AntennaSweep::nearestResonance(m_sweep, near);
    m_best      = AntennaSweep::bestMatch(m_sweep);

    // Every SERIES resonance, for an end-fed swept across all of HF.
    // Falling crossings are dropped for the same reason nearestResonance
    // drops them: they are anti-resonances or feedline artefacts, and
    // marking one would invite somebody to trim towards it.
    m_all.clear();
    for (const auto& c : AntennaSweep::resonances(m_sweep)) {
        if (c.rising && c.resistanceOhms <= 400.0) { m_all.append(c); }
    }

    // With a target inside one band, put the verticals on that band
    // rather than on the widest overlap — on a 3-to-30 MHz end-fed
    // sweep the widest overlap is 10 m, which is unlikely to be what
    // was asked about.
    //
    // But only a target the sweep actually reaches. 2026-08-14, on the
    // bench: a 40 m sweep defaulted the target to 7.100, then a 20 m
    // sweep arrived. The target was still 7.100, so the band came out
    // as 40 m over a 14.000–14.350 curve — the three tiles read
    // "not swept" against 7.000 / 7.100 / 7.200 and the line under the
    // axis said "40 m · mid 7.100 MHz" beneath twenty metres of data.
    //
    // Worse, it could not recover: the window re-defaults the target to
    // the middle of the band it is shown, so 40 m wrote 7.100 back and
    // the next sweep found the same stale target waiting.
    //
    // The sweep decides which bands are in play. A target may only
    // choose among them; it may not nominate one that was not measured.
    if (!m_forcedBand.isValid() && m_targetHz > 0.0
        && m_targetHz >= m_sweep.startHz()
        && m_targetHz <= m_sweep.stopHz()) {
        const AmateurBands::Band inTarget =
            AmateurBands::containing(m_targetHz, m_region);
        if (inTarget.isValid()) { m_band = inTarget; }
    }

    // ── The vertical scale ───────────────────────────────────────────
    //
    // Fitted to what is inside the band rather than to the whole sweep.
    // The skirts outside a band routinely reach SWR 20, and scaling to
    // them squashes everything the operator cares about into the bottom
    // two pixels.
    double worst = 1.0;
    for (const SweepPoint& p : m_sweep.points) {
        // Inside ANY band the sweep touches. Scaling to one band on a
        // multiband sweep would clip the others off the top.
        bool inSome = m_bands.isEmpty();
        for (const AmateurBands::Band& b : m_bands) {
            if (b.contains(p.freqHz)) { inSome = true; break; }
        }
        if (!inSome) { continue; }
        worst = std::max(worst, AntennaSweep::swr(p.gamma));
    }
    if (worst <= 1.0) {   // no points in any band at all
        for (const SweepPoint& p : m_sweep.points) {
            worst = std::max(worst, AntennaSweep::swr(p.gamma));
        }
    }
    // Always room for the limit line, never less than 3 (so a flat
    // antenna is not magnified into noise), never more than 10 (past
    // which the shape stops carrying information).
    m_swrTop = std::clamp(std::ceil(std::max(worst, m_limit + 0.5)),
                          3.0, 10.0);
}

double SwrCurveWidget::xFor(double hz) const
{
    if (m_viewHiHz <= m_viewLoHz) { return m_plotL; }
    const double t = (hz - m_viewLoHz) / (m_viewHiHz - m_viewLoHz);
    return m_plotL + t * (m_plotR - m_plotL);
}

double SwrCurveWidget::yFor(double swr) const
{
    const double v = std::clamp(swr, 1.0, m_swrTop);
    const double t = (v - 1.0) / (m_swrTop - 1.0);
    return m_plotB - t * (m_plotB - m_plotT);
}

void SwrCurveWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(Style::kPanelBg));

    const double w = width();
    const double h = height();

    m_plotL = 44.0;
    m_plotR = w - 12.0;
    m_plotT = 26.0;
    // Three rows below the frame now: the frequency ticks, the band
    // line, and the provenance. 40 px fitted two.
    m_plotB = h - 48.0;
    const QRectF plot(m_plotL, m_plotT, m_plotR - m_plotL, m_plotB - m_plotT);

    p.setPen(QPen(kGrid, 1));
    p.setBrush(QColor(Style::kInsetBg));
    p.drawRect(plot);

    if (m_sweep.isEmpty() || plot.width() < 20 || plot.height() < 20) {
        p.setPen(kDim);
        QFont f = p.font();
        f.setPixelSize(11);
        p.setFont(f);
        p.drawText(plot, Qt::AlignCenter | Qt::TextWordWrap,
                   m_sweep.note.isEmpty()
                       ? QStringLiteral("No sweep loaded.\nOpen a .s1p file "
                                        "from your analyser.")
                       : m_sweep.note);
        return;
    }

    QFont small = p.font();
    small.setPixelSize(10);

    // ── The bands ────────────────────────────────────────────────────
    //
    // All of them, not only the one with the verticals. On an end-fed
    // sweep across HF the picture IS the set of bands and where the
    // resonances fall relative to them.
    p.setFont(small);
    {
        const QFontMetrics bfm(small);
        for (const AmateurBands::Band& b : m_bands) {
            const double bl = std::max(xFor(b.lowHz),  m_plotL);
            const double br = std::min(xFor(b.highHz), m_plotR);
            if (br <= bl) { continue; }

            QColor fill = kBandFill;
            // The band the verticals are on is a shade stronger, so it
            // is findable among eight others.
            fill.setAlpha(b.name == m_band.name ? 30 : 14);
            p.fillRect(QRectF(bl, m_plotT, br - bl, plot.height()), fill);

            // Name it only where the name fits. A 30 m band on a 3-to-30
            // sweep is four pixels wide and a label there is a smear.
            const double tw = bfm.horizontalAdvance(b.name);
            if (br - bl > tw + 4.0) {
                p.setPen(QColor(Style::kAccent));
                p.drawText(QPointF((bl + br) / 2.0 - tw / 2.0,
                                   m_plotT + 12.0), b.name);
            }
        }
    }

    // ── Horizontal rules ─────────────────────────────────────────────
    p.setFont(small);
    const QFontMetrics sfm(small);
    for (double s = 2.0; s <= m_swrTop + 0.01; s += 1.0) {
        const double y = yFor(s);
        const bool isLimit = std::abs(s - m_limit) < 0.01;
        p.setPen(QPen(isLimit ? kLimit : kGrid, 1,
                      isLimit ? Qt::DashLine : Qt::DotLine));
        p.drawLine(QPointF(m_plotL, y), QPointF(m_plotR, y));
        p.setPen(isLimit ? kLimit : kDim);
        p.drawText(QPointF(m_plotL - 8.0 - sfm.horizontalAdvance(
                               QString::number(int(s))),
                           y + 3.0), QString::number(int(s)));
    }
    p.setPen(kDim);
    p.drawText(QPointF(m_plotL - 8.0 - sfm.horizontalAdvance(
                           QStringLiteral("1")), m_plotB + 3.0),
               QStringLiteral("1"));

    // ── The sweep before this one, faint and behind ──────────────────
    if (!m_reference.isEmpty()) {
        QColor faint = kCurve;
        faint.setAlpha(70);
        p.setClipRect(plot.adjusted(1, 1, -1, -1));
        p.setPen(QPen(faint, 1.6, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        // Same treatment as the live curve: the previous sweep can be a
        // range sweep too, and a dashed line across a band gap is no
        // more honest than a solid one.
        drawBrokenCurve(p, m_reference);
        p.setClipping(false);
    }

    // ── The curve, broken where nothing was measured ─────────────────
    p.setClipRect(plot.adjusted(1, 1, -1, -1));
    p.setPen(QPen(kCurve, 2.2));
    p.setBrush(Qt::NoBrush);
    drawBrokenCurve(p, m_sweep);
    p.setClipping(false);

    // ── Every other series resonance, thin ───────────────────────────
    //
    // An end-fed has one per harmonic band. They are drawn before the
    // main one so it reads on top, and without labels — the table under
    // the picture names them.
    if (m_all.size() > 1) {
        QColor thin = kResonant;
        thin.setAlpha(120);
        p.setPen(QPen(thin, 1.0, Qt::DotLine));
        for (const auto& c : m_all) {
            if (m_resonance.found
                && std::abs(c.freqHz - m_resonance.freqHz) < 1.0) {
                continue;   // the amber one, drawn below
            }
            if (c.freqHz < m_viewLoHz || c.freqHz > m_viewHiHz) { continue; }
            const double x = xFor(c.freqHz);
            p.drawLine(QPointF(x, m_plotT), QPointF(x, m_plotB));
        }
    }

    // ── Resonance, in amber ──────────────────────────────────────────
    //
    // Drawn before the band verticals so those read on top of it: the
    // three band numbers are what the operator is here for and the
    // resonance is the explanation.
    if (m_resonance.found
        && m_resonance.freqHz >= m_viewLoHz
        && m_resonance.freqHz <= m_viewHiHz) {
        const double x = xFor(m_resonance.freqHz);
        p.setPen(QPen(kResonant, 1.4));
        p.drawLine(QPointF(x, m_plotT), QPointF(x, m_plotB));
        const QString t = QStringLiteral("resonant %1")
                              .arg(mhz(m_resonance.freqHz));
        // Flipped to the left when it would run off the right edge.
        const double tw = sfm.horizontalAdvance(t);
        const bool leftOf = x + 6.0 + tw > m_plotR;
        p.drawText(QPointF(leftOf ? x - 6.0 - tw : x + 6.0, m_plotT + 12.0),
                   t);
    }

    // ── Low edge, centre, high edge ──────────────────────────────────
    if (m_band.isValid()) {
        struct Mark { double hz; QString label; QColor col; };
        const QVector<Mark> marks = {
            {m_band.lowHz,    QStringLiteral("start"),  kEdge},
            {m_band.centreHz(), QStringLiteral("mid"),  kCentre},
            {m_band.highHz,   QStringLiteral("end"),    kEdge},
        };

        for (const Mark& m : marks) {
            if (m.hz < m_viewLoHz || m.hz > m_viewHiHz) { continue; }
            const double x = xFor(m.hz);
            p.setPen(QPen(m.col, 1.2, Qt::DashLine));
            p.drawLine(QPointF(x, m_plotT), QPointF(x, m_plotB));

            const double s = AntennaSweep::swrAt(m_sweep, m.hz);
            if (s <= 0.0) {
                // The sweep does not reach this edge. Saying nothing
                // would let an operator assume it was measured.
                p.setPen(kDim);
                p.drawText(QPointF(x + 4.0, m_plotB - 6.0),
                           QStringLiteral("not swept"));
                continue;
            }
            const QPointF at(x, yFor(s));
            p.setPen(Qt::NoPen);
            p.setBrush(s > m_limit ? kBad : m.col);
            p.drawEllipse(at, 3.6, 3.6);

            const QString t = QStringLiteral("%1  %2")
                                  .arg(m.label).arg(s, 0, 'f', 2);
            const double tw = sfm.horizontalAdvance(t);
            const bool leftOf = x + 6.0 + tw > m_plotR;
            p.setPen(s > m_limit ? kBad : kText);
            p.drawText(QPointF(leftOf ? x - 6.0 - tw : x + 6.0,
                               at.y() - 7.0), t);
        }
    }

    // ── Target, if one was set ───────────────────────────────────────
    if (m_targetHz > m_viewLoHz && m_targetHz < m_viewHiHz) {
        const double x = xFor(m_targetHz);
        QPen pen(kCurve, 1.2, Qt::DashDotLine);
        p.setPen(pen);
        p.drawLine(QPointF(x, m_plotT), QPointF(x, m_plotB));
    }

    // ── Frequency scale ──────────────────────────────────────────────
    //
    // "der dargestellte x-Bereich sollte die Frequenz in Form von
    //  Marken haben, um leichter ablesen zu können."
    //
    // Two numbers at the ends is not a scale. Reading 7.130 off a curve
    // then meant measuring with a finger. Ticks on round steps — 1, 2
    // or 5 times a power of ten — so the labels are numbers a person
    // would say out loud rather than whatever the span divides into.
    {
        // How many labels fit, not how many look nice in the abstract:
        // a narrow panel with eight of them prints them on top of each
        // other, which reads as a smudge rather than as a scale.
        const int wanted = std::clamp(
            static_cast<int>((m_plotR - m_plotL) / 78.0), 3, 10);
        const double step  = tickStepHz(m_viewHiHz - m_viewLoHz, wanted);
        const double first = std::ceil(m_viewLoHz / step) * step;
        // Enough decimals for the step and no more. A 5 MHz step
        // labelled "15.000" is three zeros of noise.
        const int digits = (step >= 1e6)   ? 0
                         : (step >= 1e5)   ? 1
                         : (step >= 1e4)   ? 2
                                           : 3;
        p.setFont(small);
        for (double f = first; f <= m_viewHiHz + 1.0; f += step) {
            const double x = xFor(f);
            if (x < m_plotL - 0.5 || x > m_plotR + 0.5) { continue; }
            // Short tick outside the frame and a faint grid line inside
            // it: the line is what lets the eye carry a frequency up to
            // the curve, which is the whole point of having a scale.
            p.setPen(QPen(kGrid, 1.0, Qt::DotLine));
            p.drawLine(QPointF(x, m_plotT), QPointF(x, m_plotB));
            p.setPen(kDim);
            p.drawLine(QPointF(x, m_plotB), QPointF(x, m_plotB + 4.0));
            const QString t = mhz(f, digits);
            p.drawText(QPointF(x - sfm.horizontalAdvance(t) / 2.0,
                               m_plotB + 15.0), t);
        }
    }

    if (m_band.isValid()) {
        const QString mid = QStringLiteral("%1 · mid %2 MHz · %3 kHz wide")
                                .arg(m_band.name, mhz(m_band.centreHz()))
                                .arg(m_band.widthHz() / 1000.0, 0, 'f', 0);
        p.setPen(kCentre);
        p.drawText(QPointF((m_plotL + m_plotR) / 2.0
                               - sfm.horizontalAdvance(mid) / 2.0,
                           m_plotB + 29.0), mid);
    }

    // Which band plan drew those edges. A green bar is not a licence,
    // and the only defence against it being read as one is saying where
    // it came from.
    p.setPen(kDim);
    // Say where the three verticals came from. With a typed range they
    // are the operator's own and calling them band edges would be a
    // small lie in a place that matters.
    const QString prov =
        m_forcedBand.isValid()
            ? QStringLiteral("Verticals: your own range. Shaded bands: "
                             "%1 — check your own licence")
                  .arg(AmateurBands::regionName(m_region))
        : m_band.isValid()
            ? QStringLiteral("Band edges: %1 — check your own licence")
                  .arg(AmateurBands::regionName(m_region))
            : QStringLiteral("This sweep does not cover an amateur band (%1)")
                  .arg(AmateurBands::regionName(m_region));
    p.drawText(QPointF(m_plotL, m_plotB + 42.0), prov);

    // ── Title line ───────────────────────────────────────────────────
    p.setPen(kText);
    QFont title = p.font();
    title.setPixelSize(11);
    p.setFont(title);
    QString head = QStringLiteral("SWR");
    if (m_best.found) {
        head += QStringLiteral("   best %1 at %2 MHz")
                    .arg(m_best.swr, 0, 'f', 2).arg(mhz(m_best.freqHz));
    }
    if (m_resonance.found && m_best.found
        && std::abs(m_best.freqHz - m_resonance.freqHz) > 20e3) {
        p.setPen(kResonant);
        head += QStringLiteral("   — not the same as resonant");
    }
    p.drawText(QPointF(m_plotL, m_plotT - 9.0), head);

    // Name the dashed one, or it is just a second line nobody asked
    // for.
    if (!m_reference.isEmpty()) {
        QColor faint = kCurve;
        faint.setAlpha(150);
        p.setPen(faint);
        const QString ref = m_referenceLabel.isEmpty()
            ? QStringLiteral("- - -  previous sweep")
            : QStringLiteral("- - -  %1").arg(m_referenceLabel);
        p.drawText(QPointF(m_plotR - sfm.horizontalAdvance(ref),
                           m_plotT - 9.0), ref);
    }

    // ── Cursor readout ───────────────────────────────────────────────
    //
    // Drawn last so it sits over everything: it is the one thing on the
    // picture that answers a question being asked right now.
    if (m_cursor.x() >= 0.0 && !m_sweep.isEmpty()) {
        const double hz  = xToHz(m_cursor.x());
        const double swr = AntennaSweep::swrAt(m_sweep, hz);
        const bool   have = swr >= 1.0;   // 0 means outside the sweep

        p.setFont(small);
        p.setPen(QPen(kText, 1.0, Qt::DashLine));
        p.drawLine(QPointF(m_cursor.x(), m_plotT),
                   QPointF(m_cursor.x(), m_plotB));

        // A dot on the curve, not at the pointer. The pointer is where
        // the hand is; the dot is where the answer is, and putting the
        // marker anywhere else would misreport the reading by however
        // far the two differ vertically.
        double markerY = m_cursor.y();
        if (have) {
            markerY = yFor(swr);
            p.setPen(QPen(kText, 1.0, Qt::DashLine));
            p.drawLine(QPointF(m_plotL, markerY),
                       QPointF(m_plotR, markerY));
            p.setPen(Qt::NoPen);
            p.setBrush(swr > m_limit ? kBad : kCurve);
            p.drawEllipse(QPointF(m_cursor.x(), markerY), 3.5, 3.5);
            p.setBrush(Qt::NoBrush);
        }

        const QString txt = have
            ? QStringLiteral("%1 MHz   SWR %2")
                  .arg(mhz(hz, 4)).arg(swr, 0, 'f', 2)
            : QStringLiteral("%1 MHz   (nicht gemessen)").arg(mhz(hz, 4));

        const double tw = sfm.horizontalAdvance(txt) + 10.0;
        const double th = sfm.height() + 4.0;
        // Flip to the other side near the right edge so the box never
        // runs off the frame and truncates the number being read.
        double bx = m_cursor.x() + 8.0;
        if (bx + tw > m_plotR) { bx = m_cursor.x() - 8.0 - tw; }
        double by = markerY - th - 8.0;
        if (by < m_plotT) { by = markerY + 8.0; }

        QColor box(Style::kPanelBg);
        box.setAlpha(235);
        p.setPen(QPen(kGrid, 1));
        p.setBrush(box);
        p.drawRoundedRect(QRectF(bx, by, tw, th), 3.0, 3.0);
        p.setBrush(Qt::NoBrush);
        p.setPen(have && swr > m_limit ? kBad : kText);
        p.drawText(QPointF(bx + 5.0, by + sfm.ascent() + 2.0), txt);
    }
}

} // namespace NereusSDR
