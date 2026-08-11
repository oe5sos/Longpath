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

#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

#include <algorithm>
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
    m_band = m_forcedBand.isValid()
                 ? m_forcedBand
                 : AmateurBands::bestOverlap(m_sweep.startHz(),
                                             m_sweep.stopHz(), m_region);

    if (m_sweep.isEmpty()) {
        m_viewLoHz = m_viewHiHz = 0.0;
        m_resonance = {};
        m_best = {};
        return;
    }

    // A little air either side so the curve does not run into the
    // frame, and so a band edge sitting exactly at the sweep end is
    // still visible as a line rather than as part of the border.
    const double span = std::max(1.0, m_sweep.stopHz() - m_sweep.startHz());
    m_viewLoHz = m_sweep.startHz() - span * 0.02;
    m_viewHiHz = m_sweep.stopHz()  + span * 0.02;

    // Resonance nearest the target if one was given, otherwise nearest
    // the middle of the band — which is what somebody without a
    // specific frequency in mind actually means.
    const double near = m_targetHz > 0.0 ? m_targetHz
                      : m_band.isValid() ? m_band.centreHz()
                                         : 0.0;
    m_resonance = AntennaSweep::nearestResonance(m_sweep, near);
    m_best      = AntennaSweep::bestMatch(m_sweep);

    // ── The vertical scale ───────────────────────────────────────────
    //
    // Fitted to what is inside the band rather than to the whole sweep.
    // The skirts outside a band routinely reach SWR 20, and scaling to
    // them squashes everything the operator cares about into the bottom
    // two pixels.
    double worst = 1.0;
    for (const SweepPoint& p : m_sweep.points) {
        if (m_band.isValid() && !m_band.contains(p.freqHz)) { continue; }
        worst = std::max(worst, AntennaSweep::swr(p.gamma));
    }
    if (worst <= 1.0) {   // no points in the band at all
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
    m_plotB = h - 40.0;
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

    // ── The band ─────────────────────────────────────────────────────
    if (m_band.isValid()) {
        const double bl = std::max(xFor(m_band.lowHz),  m_plotL);
        const double br = std::min(xFor(m_band.highHz), m_plotR);
        if (br > bl) {
            QColor fill = kBandFill;
            fill.setAlpha(20);
            p.fillRect(QRectF(bl, m_plotT, br - bl, plot.height()), fill);
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
        QPolygonF prev;
        prev.reserve(m_reference.points.size());
        for (const SweepPoint& pt : m_reference.points) {
            prev << QPointF(xFor(pt.freqHz),
                            yFor(AntennaSweep::swr(pt.gamma)));
        }
        QColor faint = kCurve;
        faint.setAlpha(70);
        p.setClipRect(plot.adjusted(1, 1, -1, -1));
        p.setPen(QPen(faint, 1.6, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(prev);
        p.setClipping(false);
    }

    // ── The curve ────────────────────────────────────────────────────
    QPolygonF line;
    line.reserve(m_sweep.points.size());
    for (const SweepPoint& pt : m_sweep.points) {
        line << QPointF(xFor(pt.freqHz),
                        yFor(AntennaSweep::swr(pt.gamma)));
    }
    p.setClipRect(plot.adjusted(1, 1, -1, -1));
    p.setPen(QPen(kCurve, 2.2));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(line);
    p.setClipping(false);

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

    // ── Axis labels and the provenance line ──────────────────────────
    p.setPen(kDim);
    p.drawText(QPointF(m_plotL, m_plotB + 14.0), mhz(m_sweep.startHz()));
    const QString hi = mhz(m_sweep.stopHz());
    p.drawText(QPointF(m_plotR - sfm.horizontalAdvance(hi), m_plotB + 14.0),
               hi);

    if (m_band.isValid()) {
        const QString mid = QStringLiteral("%1 · mid %2 MHz")
                                .arg(m_band.name, mhz(m_band.centreHz()));
        p.setPen(kCentre);
        p.drawText(QPointF((m_plotL + m_plotR) / 2.0
                               - sfm.horizontalAdvance(mid) / 2.0,
                           m_plotB + 14.0), mid);
    }

    // Which band plan drew those edges. A green bar is not a licence,
    // and the only defence against it being read as one is saying where
    // it came from.
    p.setPen(kDim);
    const QString prov = m_band.isValid()
        ? QStringLiteral("Band edges: %1 — check your own licence")
              .arg(AmateurBands::regionName(m_region))
        : QStringLiteral("This sweep does not cover an amateur band (%1)")
              .arg(AmateurBands::regionName(m_region));
    p.drawText(QPointF(m_plotL, m_plotB + 28.0), prov);

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
}

} // namespace NereusSDR
