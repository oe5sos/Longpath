// =================================================================
// src/gui/widgets/SwrChartWidget.cpp  (NereusSDR)
// =================================================================
//
// See SwrChartWidget.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-13 — Created by Ralph Martin Fischer (OE5SOS),
//                 AI-assisted implementation via Anthropic Claude
//                 (Cowork).
// =================================================================

#include "gui/styles/ThemeQss.h"
#include "SwrChartWidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>

#include <algorithm>
#include <cmath>
#include "gui/StyleConstants.h"

namespace Longpath {

namespace {

// ── NOTBEHELF, NICHT DIE HAUSFARBEN ─────────────────────────────────
//
// Die echten Werte stehen in themes/oe5sos.json unter den Rollen
// chart-trace-1 bis chart-trace-6. Dieser Block greift NUR, wenn gar
// keine Theme-Datei vorhanden ist -- also auf einer frischen
// Installation, bevor der Betreiber seine eigene hinterlegt hat.
//
// Er ist ausdruecklich kein Gestaltungsvorrat und wird nicht gepflegt.
// Wer die Diagrammfarben aendern will, aendert die Theme-Datei.
//
// Warum ueberhaupt eine Reihe und nicht ein Ton: das Diagramm legt
// mehrere Sweeps uebereinander, und die muessen unterscheidbar bleiben.
// In die Palette gezwungen saehen sie alle gleich aus, und dann ist das
// Bild huebsch und unlesbar. Gedeckt duerfen sie sein -- auseinander
// muessen sie liegen.
//
// Durchnummeriert, weil die Spuren hier wirklich beliebig sind: eine je
// Sweep-Durchlauf, in der Reihenfolge des Messens. Was eine Bedeutung
// haette, bekaeme sie auch als Namen.
const QColor kTraceFallback[] = {
    QColor(0xc2, 0x92, 0x4f),   // 1
    QColor(0x6f, 0xa3, 0x84),   // 2
    QColor(Style::kAccent),   // 3
    QColor(Style::kLabelMid),   // 4
    QColor(Style::kTxFilterOverlayBorder),   // 5
    QColor(Style::kAmberWarn),   // 6
};
constexpr int kTraceColorCount =
    int(sizeof(kTraceFallback) / sizeof(kTraceFallback[0]));

// Die Rollennamen, an EINER Stelle -- der Test in
// tst_chart_trace_roles liest sie hier und prueft, dass jede davon in
// docs/design/oe5sos.example.json steht. Eine frische Installation
// bekommt die Vorlage ausgeliefert und darf deshalb nicht auf den
// Notbehelf darueber fallen.
const char* const kTraceRoles[] = {
    "chart-trace-1", "chart-trace-2", "chart-trace-3",
    "chart-trace-4", "chart-trace-5", "chart-trace-6",
};

constexpr int kMarginL = 44;
constexpr int kMarginR = 12;
constexpr int kMarginT = 10;
constexpr int kMarginB = 26;

// Zone edges — the operator's question is "where is it good": below
// 1.5 every rig is happy, below 2.0 fine in practice, above is the
// red zone. Display convention only; nothing here feeds protection.
constexpr double kZoneGood = 1.5;
constexpr double kZoneOk   = 2.0;

} // namespace

SwrChartWidget::SwrChartWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setAutoFillBackground(false);
}

int SwrChartWidget::addTrace(const QString& name,
                             const QVector<SwrSweepPoint>& pts)
{
    Trace t;
    t.name   = name;
    t.color  = nextColor();
    t.points = pts;
    m_traces.append(t);
    update();
    return m_traces.size() - 1;
}

void SwrChartWidget::removeTrace(int index)
{
    if (index < 0 || index >= m_traces.size()) {
        return;
    }
    m_traces.removeAt(index);
    update();
}

void SwrChartWidget::setTraceVisible(int index, bool visible)
{
    if (index < 0 || index >= m_traces.size()) {
        return;
    }
    m_traces[index].visible = visible;
    update();
}

void SwrChartWidget::renameTrace(int index, const QString& name)
{
    if (index < 0 || index >= m_traces.size()) {
        return;
    }
    m_traces[index].name = name;
    update();
}

void SwrChartWidget::clearTraces()
{
    m_traces.clear();
    m_colorCursor = 0;
    update();
}

void SwrChartWidget::beginLiveTrace(const QString& name,
                                    quint64 startHz, quint64 stopHz)
{
    m_live = Trace{};
    m_live.name  = name;
    m_live.color = nextColor();
    m_liveActive  = true;
    m_liveStartHz = startHz;
    m_liveStopHz  = stopHz;
    update();
}

void SwrChartWidget::appendLivePoint(quint64 freqHz, double swr)
{
    if (!m_liveActive) {
        return;
    }
    m_live.points.append(SwrSweepPoint{freqHz, swr, 0.0});
    update();
}

int SwrChartWidget::finishLiveTrace()
{
    if (!m_liveActive) {
        return -1;
    }
    m_liveActive = false;
    m_traces.append(m_live);
    m_live = Trace{};
    update();
    return m_traces.size() - 1;
}

void SwrChartWidget::dropLiveTrace()
{
    m_liveActive = false;
    m_live = Trace{};
    update();
}

void SwrChartWidget::setYMax(double yMax)
{
    m_yMax = std::max(3.0, yMax);
    update();
}

QColor SwrChartWidget::nextColor()
{
    const int i = m_colorCursor % kTraceColorCount;
    // Rolle zuerst, Notbehelf nur wenn keine Theme-Datei etwas sagt.
    const QColor c(Style::role(kTraceRoles[i],
                               kTraceFallback[i].name().toLatin1().constData()));
    ++m_colorCursor;
    return c;
}

bool SwrChartWidget::span(quint64& lo, quint64& hi) const
{
    bool have = false;
    auto fold = [&](const Trace& t, quint64 s, quint64 e) {
        if (t.points.isEmpty() && s == 0) {
            return;
        }
        quint64 tLo = s != 0 ? s : t.points.first().freqHz;
        quint64 tHi = e != 0 ? e : t.points.last().freqHz;
        if (tLo > tHi) {
            std::swap(tLo, tHi);
        }
        if (!have) {
            lo = tLo;
            hi = tHi;
            have = true;
        } else {
            lo = std::min(lo, tLo);
            hi = std::max(hi, tHi);
        }
    };
    for (const Trace& t : m_traces) {
        if (t.visible) {
            fold(t, 0, 0);
        }
    }
    if (m_liveActive) {
        fold(m_live, m_liveStartHz, m_liveStopHz);
    }
    return have && hi > lo;
}

void SwrChartWidget::paintEvent(QPaintEvent* /*ev*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect plot(kMarginL, kMarginT,
                     width() - kMarginL - kMarginR,
                     height() - kMarginT - kMarginB);

    // Background.
    p.fillRect(rect(), QColor(0x0f, 0x0f, 0x1a));
    p.fillRect(plot, QColor(Style::kBadgeInfoBg));

    const double yMin = 1.0;
    auto yFor = [&](double swr) -> int {
        const double s = std::clamp(swr, yMin, m_yMax);
        const double t = (s - yMin) / (m_yMax - yMin);
        return plot.bottom() - static_cast<int>(t * plot.height());
    };

    // Quality zones (bottom-up: green, yellow, red).
    p.fillRect(QRect(plot.left(), yFor(kZoneGood), plot.width(),
                     plot.bottom() - yFor(kZoneGood)),
               QColor(Style::kGreenBg));
    p.fillRect(QRect(plot.left(), yFor(kZoneOk), plot.width(),
                     yFor(kZoneGood) - yFor(kZoneOk)),
               QColor(Style::kAmberBg));
    p.fillRect(QRect(plot.left(), plot.top(), plot.width(),
                     yFor(kZoneOk) - plot.top()),
               QColor(Style::kBadgeTxBg));

    // Y grid + labels at 1.5 / 2 / 3 / 5.
    p.setPen(QColor(Style::kBlueBg));
    p.setFont(QFont(font().family(), 9));
    for (double g : {1.0, kZoneGood, kZoneOk, 3.0, 5.0}) {
        if (g > m_yMax) {
            continue;
        }
        const int y = yFor(g);
        p.drawLine(plot.left(), y, plot.right(), y);
        p.setPen(QColor(0xc8, 0xd8, 0xe8));
        p.drawText(QRect(0, y - 8, kMarginL - 6, 16),
                   Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(g, 'g', 2));
        p.setPen(QColor(Style::kBlueBg));
    }

    quint64 lo = 0;
    quint64 hi = 0;
    if (!span(lo, hi)) {
        p.setPen(QColor(0x80, 0x90, 0xa0));
        p.drawText(plot, Qt::AlignCenter,
                   QStringLiteral("Kein Sweep — Band wählen und Start drücken"));
        return;
    }

    auto xFor = [&](quint64 f) -> int {
        const double t = static_cast<double>(f - lo)
                         / static_cast<double>(hi - lo);
        return plot.left() + static_cast<int>(t * plot.width());
    };

    // X labels: start / middle / stop in MHz.
    p.setPen(QColor(0xc8, 0xd8, 0xe8));
    auto mhz = [](quint64 f) {
        return QString::number(static_cast<double>(f) / 1e6, 'f', 3);
    };
    p.drawText(QRect(plot.left(), plot.bottom() + 4, 120, 18),
               Qt::AlignLeft, mhz(lo));
    p.drawText(QRect(plot.left(), plot.bottom() + 4, plot.width(), 18),
               Qt::AlignHCenter, mhz(lo + (hi - lo) / 2));
    p.drawText(QRect(plot.right() - 120, plot.bottom() + 4, 120, 18),
               Qt::AlignRight, mhz(hi));

    // Traces.
    auto drawTrace = [&](const Trace& t) {
        if (t.points.size() < 2) {
            // Single point: dot.
            if (t.points.size() == 1 && t.points[0].swr > 0.0) {
                p.setPen(Qt::NoPen);
                p.setBrush(t.color);
                p.drawEllipse(QPoint(xFor(t.points[0].freqHz),
                                     yFor(t.points[0].swr)), 2, 2);
            }
            return;
        }
        QPen pen(t.color, 2.0);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        QPolygonF poly;
        for (const SwrSweepPoint& pt : t.points) {
            if (pt.swr <= 0.0) {
                // Invalid sample: break the line.
                if (poly.size() >= 2) {
                    p.drawPolyline(poly);
                }
                poly.clear();
                continue;
            }
            poly << QPointF(xFor(pt.freqHz), yFor(pt.swr));
        }
        if (poly.size() >= 2) {
            p.drawPolyline(poly);
        }

        // Resonance marker: minimum valid SWR.
        double best = 0.0;
        quint64 bestF = 0;
        for (const SwrSweepPoint& pt : t.points) {
            if (pt.swr > 0.0 && (best <= 0.0 || pt.swr < best)) {
                best = pt.swr;
                bestF = pt.freqHz;
            }
        }
        if (best > 0.0) {
            const QPoint m(xFor(bestF), yFor(best));
            p.setBrush(t.color);
            p.drawEllipse(m, 4, 4);
            p.setPen(QColor(0xff, 0xff, 0xff));
            p.drawText(QPoint(m.x() + 6, m.y() - 6),
                       QStringLiteral("%1  SWR %2")
                           .arg(mhz(bestF))
                           .arg(best, 0, 'f', 2));
        }
    };

    for (const Trace& t : m_traces) {
        if (t.visible) {
            drawTrace(t);
        }
    }
    if (m_liveActive) {
        drawTrace(m_live);
    }

    // Legend, top-left inside the plot.
    int ly = plot.top() + 6;
    p.setFont(QFont(font().family(), 9));
    auto legendLine = [&](const Trace& t, bool live) {
        p.setPen(Qt::NoPen);
        p.setBrush(t.color);
        p.drawRect(plot.left() + 8, ly + 3, 14, 3);
        p.setPen(QColor(0xc8, 0xd8, 0xe8));
        p.drawText(plot.left() + 28, ly + 9,
                   live ? t.name + QStringLiteral(" …") : t.name);
        ly += 16;
    };
    for (const Trace& t : m_traces) {
        if (t.visible) {
            legendLine(t, false);
        }
    }
    if (m_liveActive) {
        legendLine(m_live, true);
    }

    // Hover crosshair with readout of the topmost visible trace.
    if (m_hoverX >= plot.left() && m_hoverX <= plot.right()) {
        p.setPen(QColor(0x50, 0x70, 0x90, 160));
        p.drawLine(m_hoverX, plot.top(), m_hoverX, plot.bottom());
        const auto fHover = static_cast<quint64>(
            lo + static_cast<double>(m_hoverX - plot.left())
                 / plot.width() * (hi - lo));
        // Nearest point of the last visible trace (or live).
        const Trace* src = m_liveActive ? &m_live : nullptr;
        if (!src) {
            for (int i = m_traces.size() - 1; i >= 0; --i) {
                if (m_traces[i].visible) {
                    src = &m_traces[i];
                    break;
                }
            }
        }
        if (src && !src->points.isEmpty()) {
            const SwrSweepPoint* nearest = nullptr;
            quint64 bestD = ~0ULL;
            for (const SwrSweepPoint& pt : src->points) {
                const quint64 d = pt.freqHz > fHover ? pt.freqHz - fHover
                                                     : fHover - pt.freqHz;
                if (d < bestD) {
                    bestD = d;
                    nearest = &pt;
                }
            }
            if (nearest && nearest->swr > 0.0) {
                const QString txt = QStringLiteral("%1 MHz — SWR %2")
                    .arg(mhz(nearest->freqHz))
                    .arg(nearest->swr, 0, 'f', 2);
                const QFontMetrics fm(p.font());
                const int w = fm.horizontalAdvance(txt) + 10;
                const int x = std::min(m_hoverX + 8, plot.right() - w);
                p.fillRect(QRect(x, plot.top() + 4, w, 18),
                           QColor(0x0f, 0x0f, 0x1a, 220));
                p.setPen(QColor(0xff, 0xff, 0xff));
                p.drawText(QRect(x + 5, plot.top() + 4, w, 18),
                           Qt::AlignLeft | Qt::AlignVCenter, txt);
            }
        }
    }
}

void SwrChartWidget::mouseMoveEvent(QMouseEvent* ev)
{
    m_hoverX = ev->pos().x();
    update();
}

void SwrChartWidget::leaveEvent(QEvent* /*ev*/)
{
    m_hoverX = -1;
    update();
}

} // namespace Longpath
