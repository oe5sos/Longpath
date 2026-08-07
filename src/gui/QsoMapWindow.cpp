// =================================================================
// src/gui/QsoMapWindow.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — see QsoMapWindow.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "QsoMapWindow.h"

#include "core/Maidenhead.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/FlatMapWidget.h"
#include "gui/widgets/GlobeWidget.h"

#include <QCheckBox>
#include <QDateEdit>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QStackedWidget>
#include <QTimeZone>
#include <QVBoxLayout>

namespace NereusSDR {

QsoMapWindow::QsoMapWindow(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("QSO map"));
    setModal(false);
    resize(1100, 660);
    buildUi();
}

void QsoMapWindow::buildUi()
{
    setStyleSheet(QStringLiteral("QDialog { background: %1; }")
                      .arg(QString::fromLatin1(Style::kAppBg)));

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(10, 10, 10, 10);
    col->setSpacing(8);

    auto* bar = new QHBoxLayout;
    bar->setSpacing(6);

    auto caption = [this](const QString& t) {
        auto* l = new QLabel(t, this);
        l->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 10px; }")
                             .arg(QString::fromLatin1(Style::kTextScale)));
        return l;
    };

    QLabel* fromCap = caption(QStringLiteral("FROM"));
    bar->addWidget(fromCap);
    m_from = new QDateEdit(QDate::currentDate().addDays(-30), this);
    m_from->setCalendarPopup(true);
    m_from->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    bar->addWidget(m_from);

    QLabel* toCap = caption(QStringLiteral("TO"));
    bar->addWidget(toCap);
    m_to = new QDateEdit(QDate::currentDate(), this);
    m_to->setCalendarPopup(true);
    m_to->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    bar->addWidget(m_to);
    m_rangeControls << fromCap << m_from << toCap << m_to;

    // Quick ranges, because typing two dates to answer "what did I work
    // this week" is three interactions too many.
    struct Quick { const char* label; int days; };
    for (const Quick& q : {Quick{"24 h", 1}, Quick{"7 d", 7},
                           Quick{"30 d", 30}, Quick{"1 y", 365},
                           Quick{"All", 0}}) {
        auto* b = new QPushButton(QString::fromLatin1(q.label), this);
        b->setStyleSheet(Style::buttonBaseStyle());
        bar->addWidget(b);
        m_rangeControls << b;
        const int days = q.days;
        connect(b, &QPushButton::clicked, this,
                [this, days]() { setQuickRange(days); });
    }

    bar->addStretch(1);

    m_onlySelected = new QCheckBox(QStringLiteral("Only marked"), this);
    m_onlySelected->setEnabled(false);
    m_onlySelected->setToolTip(QStringLiteral(
        "Show only the rows picked out in the logbook. Unticked, they "
        "stay marked in colour among the rest."));
    m_onlySelected->setStyleSheet(QStringLiteral("QCheckBox { color: %1; }")
                                      .arg(QString::fromLatin1(Style::kAmberText)));
    bar->addWidget(m_onlySelected);

    m_paths = new QCheckBox(QStringLiteral("Paths"), this);
    m_paths->setChecked(true);
    m_paths->setStyleSheet(QStringLiteral("QCheckBox { color: %1; }")
                               .arg(QString::fromLatin1(Style::kTextPrimary)));
    bar->addWidget(m_paths);

    m_viewBtn = new QPushButton(QStringLiteral("Flat map"), this);
    m_viewBtn->setStyleSheet(Style::buttonBaseStyle());
    m_viewBtn->setToolTip(QStringLiteral(
        "Switch between the globe and the whole world at once"));
    bar->addWidget(m_viewBtn);
    col->addLayout(bar);

    m_globe = new GlobeWidget(this);
    // No beam spread here: the flanking arcs answer a question about
    // aiming right now, and drawing them for every logged contact would
    // treble the clutter for nothing.
    m_globe->setBeamSpread(0.0);
    m_flat = new FlatMapWidget(this);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_globe);
    m_stack->addWidget(m_flat);
    col->addWidget(m_stack, 1);

    m_summary = new QLabel(QString{}, this);
    m_summary->setWordWrap(true);
    m_summary->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }")
        .arg(QString::fromLatin1(Style::kTextSecondary)));
    col->addWidget(m_summary);

    connect(m_viewBtn, &QPushButton::clicked, this, [this]() {
        const bool toFlat = m_stack->currentIndex() == 0;
        m_stack->setCurrentIndex(toFlat ? 1 : 0);
        m_viewBtn->setText(toFlat ? QStringLiteral("Globe")
                                  : QStringLiteral("Flat map"));
    });
    connect(m_paths, &QCheckBox::toggled, this, [this](bool on) {
        m_globe->setShowPointPaths(on);
        m_flat->setShowPaths(on);
    });
    connect(m_from, &QDateEdit::dateChanged, this, &QsoMapWindow::rebuild);
    connect(m_to,   &QDateEdit::dateChanged, this, &QsoMapWindow::rebuild);
    connect(m_onlySelected, &QCheckBox::toggled, this, [this](bool on) {
        // A live date field that changes nothing on screen is worse
        // than a disabled one.
        for (QWidget* w : m_rangeControls) { w->setEnabled(!on); }
        rebuild();
    });
}

void QsoMapWindow::setSelection(const QVector<LogEntry>& selected)
{
    m_selected = selected;
    const bool any = !m_selected.isEmpty();

    QSignalBlocker block(m_onlySelected);
    m_onlySelected->setEnabled(any);
    m_onlySelected->setText(any
        ? QStringLiteral("Only marked (%1)").arg(m_selected.size())
        : QStringLiteral("Only marked"));
    // Opened from a selection, start by showing just that — it is what
    // the operator asked for. The tick is there to widen it back out.
    m_onlySelected->setChecked(any);
    for (QWidget* w : m_rangeControls) { w->setEnabled(!any); }

    rebuild();
}

void QsoMapWindow::setQuickRange(int days)
{
    QSignalBlocker b1(m_from);
    QSignalBlocker b2(m_to);
    m_to->setDate(QDate::currentDate());
    if (days <= 0) {
        // "All" means all, including a log imported from thirty years
        // ago — so the earliest contact, not an arbitrary floor.
        QDate earliest = QDate::currentDate();
        for (const LogEntry& e : m_all) {
            const QDate d = e.timeOn.toUTC().date();
            if (d.isValid() && d < earliest) { earliest = d; }
        }
        m_from->setDate(earliest);
    } else {
        m_from->setDate(QDate::currentDate().addDays(-days + 1));
    }
    rebuild();
}

void QsoMapWindow::setHomeGrid(const QString& grid)
{
    m_homeGrid = grid.trimmed().toUpper();
    if (isValidGridSquare(m_homeGrid)) {
        double lat = 0, lon = 0;
        calculateLatLonFromGridSquare(m_homeGrid, lat, lon);
        m_globe->setHome(lat, lon);
        m_globe->resetView();
        m_flat->setHome(lat, lon);
    } else {
        m_flat->clearHome();
    }
}

void QsoMapWindow::setEntries(const QVector<LogEntry>& entries)
{
    m_all = entries;
    rebuild();
}

void QsoMapWindow::rebuild()
{
    const bool onlyMarked = m_onlySelected->isChecked()
                            && !m_selected.isEmpty();

    // Which places are marked. Keyed by grid square rather than by
    // callsign: the map is a map of places, and two different stations
    // in one square share a dot.
    QSet<QString> markedKeys;
    for (const LogEntry& e : m_selected) {
        if (isValidGridSquare(e.gridSquare)) {
            markedKeys.insert(e.gridSquare.left(6).toUpper());
        }
    }

    const QVector<LogEntry>& source = onlyMarked ? m_selected : m_all;
    const QDate from = m_from->date();
    const QDate to   = m_to->date();

    QVector<MapPoint> points;
    QSet<QString> seen;
    int considered = 0;
    int noGrid     = 0;

    for (const LogEntry& e : source) {
        if (!onlyMarked) {
            // A contact with no timestamp is kept rather than dropped:
            // an imported log full of dateless records would otherwise
            // produce an empty map for every range the operator tries.
            const QDate d = e.timeOn.toUTC().date();
            if (d.isValid() && (d < from || d > to)) { continue; }
        }
        ++considered;

        if (!isValidGridSquare(e.gridSquare)) { ++noGrid; continue; }

        // One dot per place, not per contact. Working the same station
        // forty times should not stack forty arcs on one pixel and make
        // that direction look busier than it is.
        const QString key = e.gridSquare.left(6).toUpper();
        if (seen.contains(key)) { continue; }
        seen.insert(key);

        double lat = 0, lon = 0;
        calculateLatLonFromGridSquare(e.gridSquare, lat, lon);
        points.append(MapPoint{lat, lon, e.call, markedKeys.contains(key)});
    }

    m_globe->setPoints(points);
    m_flat->setPoints(points);

    // Say what could not be placed. A map quietly showing a third of the
    // log looks exactly like a map of the whole log, and the operator
    // has no way to tell the difference.
    QStringList bits;
    bits << (onlyMarked
        ? QStringLiteral("%1 marked contacts").arg(considered)
        : QStringLiteral("%1 contacts in range").arg(considered));
    bits << QStringLiteral("%1 places shown").arg(points.size());

    if (!onlyMarked && !markedKeys.isEmpty()) {
        int shownMarked = 0;
        for (const MapPoint& p : points) {
            if (p.highlight) { ++shownMarked; }
        }
        bits << QStringLiteral("%1 marked, in amber").arg(shownMarked);
        // A row marked outside the date range simply is not there. Say
        // so, or the operator counts the amber dots and concludes the
        // marking did not work.
        const int outside = markedKeys.size() - shownMarked;
        if (outside > 0) {
            bits << QStringLiteral("%1 marked outside the dates")
                        .arg(outside);
        }
    }
    if (noGrid > 0) {
        bits << QStringLiteral("%1 without a locator, so not on the map")
                    .arg(noGrid);
    }
    if (!isValidGridSquare(m_homeGrid)) {
        bits << QStringLiteral("no home locator — paths need one");
    }
    m_summary->setText(bits.join(QStringLiteral("  ·  ")));
}

} // namespace NereusSDR
