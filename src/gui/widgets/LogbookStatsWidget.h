#pragma once
// =================================================================
// src/gui/widgets/LogbookStatsWidget.h  (Longpath)
// =================================================================
//
// Longpath-original. The logbook's Kennzahlen view: six tiles over one
// LogbookStats -- Log, Bands, Modes, Activity (26 weeks), Top countries,
// Awards (DXCC · WAS · Grids · continents · CQ zones). Replaces the
// monospace text dump behind the logbook's "Stats…" button.
//
// Benchmark: Zeus's logbook workspace shows the same six tiles; the
// numbers here come from core/LogbookStats, nothing is ported. The
// tiles follow the QsoDetailPane idiom (panel background, caption in
// the scale colour, monospace for numbers); the bar charts are two
// small QPainter widgets, no charting dependency.
//
// The widget is a plain QWidget so it can sit in the Stats dialog now
// and in a docked panel later (benchmark item 2, "Standardlayout
// füllen", is the operator's layout decision, not taken here).
// no-port-check: Longpath-original.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-18 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "core/LogbookStats.h"

#include <QVector>
#include <QWidget>

class QGridLayout;
class QLabel;
class QVBoxLayout;

namespace Longpath {

/// Horizontal rows: label, bar (count) with the confirmed share drawn
/// on top, value. Or vertical bars when built with `vertical`.
class StatsBarChart : public QWidget {
    Q_OBJECT
public:
    struct Row {
        QString label;
        int     value{0};
        int     confirmed{0};
    };
    explicit StatsBarChart(bool vertical, QWidget* parent = nullptr);
    void setRows(const QVector<Row>& rows);
    const QVector<Row>& rows() const { return m_rows; }
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QVector<Row> m_rows;
    bool m_vertical{false};
};

class LogbookStatsWidget : public QWidget {
    Q_OBJECT
public:
    explicit LogbookStatsWidget(QWidget* parent = nullptr);

    void setStats(const LogbookStats& stats);
    const LogbookStats& stats() const { return m_stats; }

    // For tests: the charts and the tile captions, in build order.
    StatsBarChart* bandChart() const { return m_bandChart; }
    StatsBarChart* modeChart() const { return m_modeChart; }
    StatsBarChart* weeklyChart() const { return m_weeklyChart; }
    QLabel* totalLabel() const { return m_total; }
    QLabel* awardsLabel() const { return m_awards; }
    QLabel* countriesLabel() const { return m_countries; }

private:
    QWidget* makeTile(const QString& caption, QVBoxLayout*& body);
    QLabel* valueLine(QWidget* parent);
    void rebuild();

    LogbookStats   m_stats;
    QGridLayout*   m_grid{nullptr};
    QLabel*        m_total{nullptr};
    QLabel*        m_logLines{nullptr};
    StatsBarChart* m_bandChart{nullptr};
    StatsBarChart* m_modeChart{nullptr};
    StatsBarChart* m_weeklyChart{nullptr};
    QLabel*        m_weeklyCaption{nullptr};
    QLabel*        m_countries{nullptr};
    QLabel*        m_awards{nullptr};
};

} // namespace Longpath
