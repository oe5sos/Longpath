// =================================================================
// src/gui/widgets/LogbookStatsWidget.cpp  (Longpath)
// =================================================================
//
// Longpath-original; see LogbookStatsWidget.h.
// no-port-check: Longpath-original.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-18 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "gui/widgets/LogbookStatsWidget.h"

#include "core/DxccFlag.h"
#include "gui/StyleConstants.h"

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QVBoxLayout>

#include <algorithm>

namespace Longpath {

namespace {

constexpr int kRowHeight   = 16;   // one horizontal bar row
constexpr int kLabelWidth  = 44;   // "160m", "DIGL"
constexpr int kValueWidth  = 44;
constexpr int kVerticalH   = 70;   // the weekly chart
constexpr int kTopCountries = 10;

QString num(int n)
{
    return QLocale(QLocale::German).toString(n);
}

QString percent(int part, int whole)
{
    if (whole <= 0) { return QStringLiteral("0 %"); }
    return QStringLiteral("%1 %").arg(part * 100 / whole);
}

} // namespace

// ---------------------------------------------------------------------------
// StatsBarChart
// ---------------------------------------------------------------------------

StatsBarChart::StatsBarChart(bool vertical, QWidget* parent)
    : QWidget(parent)
    , m_vertical(vertical)
{
    setMinimumWidth(160);
}

void StatsBarChart::setRows(const QVector<Row>& rows)
{
    m_rows = rows;
    updateGeometry();
    update();
}

QSize StatsBarChart::sizeHint() const
{
    if (m_vertical) { return QSize(220, kVerticalH); }
    return QSize(220, std::max(kRowHeight, kRowHeight * static_cast<int>(m_rows.size())));
}

void StatsBarChart::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    const QColor bar(Style::kAccent);
    const QColor confirmed(Style::kGreenText);
    const QColor text(Style::kTextPrimary);
    const QColor dim(Style::kTextScale);
    QFont f = Style::monoFont(font(), Style::kFontSmall);
    p.setFont(f);

    int maxValue = 1;
    for (const Row& r : m_rows) { maxValue = std::max(maxValue, r.value); }

    if (m_vertical) {
        // One bar per bucket across the width, baseline at the bottom.
        const int n = static_cast<int>(m_rows.size());
        if (n == 0) { return; }
        const int w = width();
        const int h = height() - 2;
        const double slot = static_cast<double>(w) / n;
        for (int i = 0; i < n; ++i) {
            const int x = static_cast<int>(i * slot) + 1;
            const int bw = std::max(1, static_cast<int>(slot) - 2);
            const int bh = m_rows[i].value <= 0 ? 0
                : std::max(1, static_cast<int>(static_cast<double>(m_rows[i].value) / maxValue * (h - 2)));
            p.fillRect(QRect(x, h - bh, bw, bh), bar);
            if (m_rows[i].value <= 0) {
                p.fillRect(QRect(x, h - 1, bw, 1), dim);
            }
        }
        return;
    }

    const int barX = kLabelWidth + 4;
    const int barW = std::max(10, width() - barX - kValueWidth - 4);
    int y = 0;
    for (const Row& r : m_rows) {
        p.setPen(dim);
        p.drawText(QRect(0, y, kLabelWidth, kRowHeight),
                   Qt::AlignLeft | Qt::AlignVCenter, r.label);
        const int len = r.value <= 0 ? 0
            : std::max(1, static_cast<int>(static_cast<double>(r.value) / maxValue * barW));
        const QRect full(barX, y + 3, len, kRowHeight - 6);
        p.fillRect(full, bar);
        if (r.confirmed > 0 && r.value > 0) {
            const int clen = std::max(1, static_cast<int>(
                static_cast<double>(r.confirmed) / r.value * len));
            p.fillRect(QRect(barX, y + 3, clen, kRowHeight - 6), confirmed);
        }
        p.setPen(text);
        p.drawText(QRect(barX + barW + 4, y, kValueWidth, kRowHeight),
                   Qt::AlignRight | Qt::AlignVCenter, num(r.value));
        y += kRowHeight;
    }
}

// ---------------------------------------------------------------------------
// LogbookStatsWidget
// ---------------------------------------------------------------------------

LogbookStatsWidget::LogbookStatsWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("logbookStats"));
    setStyleSheet(QStringLiteral("#logbookStats { background: %1; }")
                      .arg(QLatin1String(Style::kAppBg)));

    m_grid = new QGridLayout(this);
    m_grid->setContentsMargins(10, 10, 10, 10);
    m_grid->setHorizontalSpacing(10);
    m_grid->setVerticalSpacing(10);

    QVBoxLayout* body = nullptr;

    // 1 · Log
    QWidget* log = makeTile(QStringLiteral("Log"), body);
    m_total = new QLabel(QStringLiteral("0"), log);
    m_total->setFont(Style::monoFont(font(), Style::kFontDisplay, QFont::DemiBold));
    m_total->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                               .arg(QLatin1String(Style::kTextPrimary)));
    body->addWidget(m_total);
    m_logLines = valueLine(log);
    body->addWidget(m_logLines);
    body->addStretch(1);
    m_grid->addWidget(log, 0, 0);

    // 2 · Bands
    QWidget* bands = makeTile(QStringLiteral("Bands"), body);
    m_bandChart = new StatsBarChart(false, bands);
    body->addWidget(m_bandChart);
    body->addStretch(1);
    m_grid->addWidget(bands, 0, 1);

    // 3 · Modes
    QWidget* modes = makeTile(QStringLiteral("Modes"), body);
    m_modeChart = new StatsBarChart(false, modes);
    body->addWidget(m_modeChart);
    body->addStretch(1);
    m_grid->addWidget(modes, 0, 2);

    // 4 · Activity
    QWidget* activity = makeTile(QStringLiteral("Activity · 26 weeks"), body);
    m_weeklyChart = new StatsBarChart(true, activity);
    m_weeklyChart->setFixedHeight(kVerticalH);
    body->addWidget(m_weeklyChart);
    m_weeklyCaption = valueLine(activity);
    body->addWidget(m_weeklyCaption);
    body->addStretch(1);
    m_grid->addWidget(activity, 1, 0);

    // 5 · Top countries
    QWidget* countries = makeTile(QStringLiteral("Top countries"), body);
    m_countries = valueLine(countries);
    body->addWidget(m_countries);
    body->addStretch(1);
    m_grid->addWidget(countries, 1, 1);

    // 6 · Awards
    QWidget* awards = makeTile(QStringLiteral("Awards"), body);
    m_awards = valueLine(awards);
    body->addWidget(m_awards);
    body->addStretch(1);
    m_grid->addWidget(awards, 1, 2);

    for (int c = 0; c < 3; ++c) { m_grid->setColumnStretch(c, 1); }
    rebuild();
}

QWidget* LogbookStatsWidget::makeTile(const QString& caption, QVBoxLayout*& body)
{
    auto* tile = new QFrame(this);
    tile->setObjectName(QStringLiteral("statsTile"));
    tile->setStyleSheet(QStringLiteral(
        "#statsTile { background: %1; border: 1px solid %2; border-radius: 6px; }")
        .arg(QLatin1String(Style::kPanelBg), QLatin1String(Style::kBorderSubtle)));
    body = new QVBoxLayout(tile);
    body->setContentsMargins(12, 10, 12, 10);
    body->setSpacing(6);

    auto* cap = new QLabel(caption, tile);
    cap->setFont(Style::capsFont(cap->font(), Style::kFontCaption));
    cap->setStyleSheet(QStringLiteral("QLabel { color: %1; border: none; }")
                           .arg(QLatin1String(Style::kTextScale)));
    body->addWidget(cap);
    return tile;
}

QLabel* LogbookStatsWidget::valueLine(QWidget* parent)
{
    auto* l = new QLabel(parent);
    l->setFont(Style::monoFont(font(), Style::kFontSmall));
    l->setStyleSheet(QStringLiteral("QLabel { color: %1; border: none; }")
                         .arg(QLatin1String(Style::kTextPrimary)));
    l->setTextFormat(Qt::RichText);
    l->setWordWrap(true);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

void LogbookStatsWidget::setStats(const LogbookStats& stats)
{
    m_stats = stats;
    rebuild();
}

void LogbookStatsWidget::rebuild()
{
    const LogbookStats& s = m_stats;
    const QString dimOpen = QStringLiteral("<span style='color:%1'>")
                                .arg(QLatin1String(Style::kTextScale));
    const QString okOpen = QStringLiteral("<span style='color:%1'>")
                               .arg(QLatin1String(Style::kGreenText));
    const QString close = QStringLiteral("</span>");
    auto row = [&](const QString& label, const QString& value) {
        return dimOpen + label.toHtmlEscaped() + close
             + QStringLiteral("&nbsp;") + value + QStringLiteral("<br>");
    };

    // ── Log ──────────────────────────────────────────────────────────
    m_total->setText(num(s.total));
    QString lines;
    lines += row(QStringLiteral("calls"), num(s.uniqueCalls));
    if (s.first.isValid()) {
        lines += row(QStringLiteral("first"),
                     s.first.toString(QStringLiteral("yyyy-MM-dd")));
        lines += row(QStringLiteral("last"),
                     s.last.toString(QStringLiteral("yyyy-MM-dd")));
    }
    lines += row(QStringLiteral("7 / 30 / 365 d"),
                 QStringLiteral("%1 / %2 / %3")
                     .arg(num(s.last7Days), num(s.last30Days), num(s.last365Days)));
    lines += row(QStringLiteral("confirmed"),
                 okOpen + num(s.confirmed) + close
                 + QStringLiteral(" (%1)").arg(percent(s.confirmed, s.total)));
    if (s.longestKm > 0.0) {
        lines += row(QStringLiteral("furthest"),
                     QStringLiteral("%1 km %2")
                         .arg(num(static_cast<int>(s.longestKm)),
                              s.longestCall.toHtmlEscaped()));
    }
    m_logLines->setText(lines);

    // ── Bands / Modes ────────────────────────────────────────────────
    QVector<StatsBarChart::Row> rows;
    for (const auto& b : s.byBand) { rows.push_back({b.key, b.count, b.confirmed}); }
    m_bandChart->setRows(rows);
    rows.clear();
    for (const auto& m : s.byMode) { rows.push_back({m.key, m.count, m.confirmed}); }
    m_modeChart->setRows(rows);

    // ── Activity ─────────────────────────────────────────────────────
    rows.clear();
    int weekMax = 0;
    int weekSum = 0;
    for (int n : s.weekly) {
        rows.push_back({QString(), n, 0});
        weekMax = std::max(weekMax, n);
        weekSum += n;
    }
    m_weeklyChart->setRows(rows);
    m_weeklyCaption->setText(
        row(QStringLiteral("total"), num(weekSum))
        + row(QStringLiteral("best week"), num(weekMax))
        + row(QStringLiteral("this week"), num(s.weekly.isEmpty() ? 0 : s.weekly.last())));

    // ── Top countries ────────────────────────────────────────────────
    QString countries;
    const int shown = std::min(kTopCountries, static_cast<int>(s.countries.size()));
    for (int i = 0; i < shown; ++i) {
        const auto& c = s.countries[i];
        const QString flag = c.prefix.startsWith(QLatin1Char('#'))
            ? QString() : dxccFlagEmoji(c.prefix);
        countries += QStringLiteral("%1 %2 %3%4%5")
            .arg(flag.isEmpty() ? QStringLiteral("&nbsp;&nbsp;") : flag,
                 c.name.toHtmlEscaped(),
                 dimOpen, num(c.count), close);
        if (c.confirmed > 0) {
            countries += QStringLiteral(" %1✓%2%3")
                .arg(okOpen, num(c.confirmed), close);
        }
        countries += QStringLiteral("<br>");
    }
    if (shown == 0) {
        countries = dimOpen + QStringLiteral("no entity resolved") + close;
    } else if (s.countries.size() > shown) {
        countries += dimOpen
            + QStringLiteral("+%1 more").arg(num(s.countries.size() - shown))
            + close;
    }
    m_countries->setText(countries);

    // ── Awards ───────────────────────────────────────────────────────
    auto award = [&](const QString& name, int worked, int of, int confirmed) {
        QString v = QStringLiteral("<b>%1</b>").arg(num(worked));
        if (of > 0) { v += dimOpen + QStringLiteral(" / %1").arg(num(of)) + close; }
        if (confirmed > 0 || worked > 0) {
            v += QStringLiteral(" %1%2 confirmed%3").arg(okOpen, num(confirmed), close);
        }
        return row(name, v);
    };
    QString awards;
    awards += award(QStringLiteral("DXCC"), s.dxccWorked, s.dxccTotal, s.dxccConfirmed);
    awards += award(QStringLiteral("WAS"), s.wasWorked, LogbookStats::kWasStates, s.wasConfirmed);
    awards += award(QStringLiteral("grids"), s.gridsWorked, 0, s.gridsConfirmed);
    awards += row(QStringLiteral("continents"),
                  QStringLiteral("<b>%1</b>%2 / %3%4")
                      .arg(num(s.continentsWorked), dimOpen,
                           num(LogbookStats::kContinents), close));
    awards += row(QStringLiteral("CQ zones"),
                  QStringLiteral("<b>%1</b>%2 / %3%4")
                      .arg(num(s.cqZonesWorked), dimOpen,
                           num(LogbookStats::kCqZones), close));
    if (!s.dxccByBand.isEmpty()) {
        awards += QStringLiteral("<br>") + dimOpen
                + QStringLiteral("DXCC per band (worked / confirmed)") + close
                + QStringLiteral("<br>");
        for (const auto& b : s.dxccByBand) {
            awards += QStringLiteral("%1%2%3 %4 / %5<br>")
                .arg(dimOpen, b.band.toHtmlEscaped(), close,
                     num(b.worked), num(b.confirmed));
        }
    }
    m_awards->setText(awards);
}

} // namespace Longpath
