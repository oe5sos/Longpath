// =================================================================
// src/gui/widgets/SwrChartWidget.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. SWR-over-frequency chart for the radio-as-
// antenna-analyzer sweep (design doc
// docs/architecture/2026-08-13-swr-sweep-analyzer-design.md).
//
// Plain QPainter — a few hundred points repainted at human rates
// needs no GPU. X is frequency across the sweep span, Y is SWR
// clamped to a [1..yMax] display window with colored quality zones
// (green < 1.5, yellow < 2.0, red above). Multiple named traces
// overlay for antenna-vs-antenna or before-vs-after comparison; the
// live sweep draws incrementally through appendLivePoint().
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-13 — Created by Ralph Martin Fischer (OE5SOS),
//                 AI-assisted implementation via Anthropic Claude
//                 (Cowork).
// =================================================================

#pragma once

#include <QWidget>
#include <QVector>
#include <QColor>

#include "core/SwrSweepController.h"

namespace Longpath {

class SwrChartWidget : public QWidget
{
    Q_OBJECT

public:
    struct Trace {
        QString                name;
        QColor                 color;
        QVector<SwrSweepPoint> points;
        bool                   visible{true};
    };

    explicit SwrChartWidget(QWidget* parent = nullptr);

    // ── Trace management ─────────────────────────────────────────────
    /// Adds a finished trace and returns its index. Colors cycle
    /// through a fixed palette.
    int  addTrace(const QString& name, const QVector<SwrSweepPoint>& pts);
    void removeTrace(int index);
    void setTraceVisible(int index, bool visible);
    void renameTrace(int index, const QString& name);
    void clearTraces();
    const QVector<Trace>& traces() const { return m_traces; }

    // ── Live sweep feed ──────────────────────────────────────────────
    /// Starts an in-progress trace covering [startHz..stopHz]; live
    /// points append until finishLiveTrace()/dropLiveTrace().
    void beginLiveTrace(const QString& name, quint64 startHz, quint64 stopHz);
    void appendLivePoint(quint64 freqHz, double swr);
    /// Promotes the live trace to a normal one (returns its index) or
    /// discards it.
    int  finishLiveTrace();
    void dropLiveTrace();

    /// Upper edge of the SWR axis (values clamp visually). Default 6.
    void setYMax(double yMax);

    QSize minimumSizeHint() const override { return {420, 220}; }

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void leaveEvent(QEvent* ev) override;

private:
    // Combined span over all visible traces + the live trace.
    bool span(quint64& lo, quint64& hi) const;
    QColor nextColor();

    QVector<Trace> m_traces;
    Trace          m_live;
    bool           m_liveActive{false};
    quint64        m_liveStartHz{0};
    quint64        m_liveStopHz{0};

    double m_yMax{6.0};
    int    m_colorCursor{0};

    // Hover crosshair (widget coords; -1 = no hover).
    int m_hoverX{-1};
};

} // namespace Longpath
