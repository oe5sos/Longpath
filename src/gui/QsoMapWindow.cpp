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
//   2026-08-10 — Band/mode filter pills, station card on marker click,
//                 grid overlay switch, Google Earth (KML) export; see
//                 QsoMapWindow.h. AI-assisted via Anthropic Claude
//                 (Cowork), operator Martin Fischer.
// =================================================================

#include "QsoMapWindow.h"

#include "core/AdifLog.h"
#include "core/AppSettings.h"
#include "core/KmlExport.h"
#include "core/Maidenhead.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/FlatMapWidget.h"
#include "gui/widgets/GlobeWidget.h"
#include "gui/widgets/StationPhoto.h"
#include "core/CallsignCache.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QDateEdit>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFileDialog>
#include <QImage>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTimeZone>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace NereusSDR {

QsoMapWindow::QsoMapWindow(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("QSO map"));
    setModal(false);

    // A plain QDialog on macOS gets a close button and nothing else —
    // no zoom, no full screen — so a map window could not be made
    // bigger, which is the first thing anyone wants from a map. Asking
    // for the window flags explicitly gets the whole title bar.
    setWindowFlags(Qt::Window
                   | Qt::WindowTitleHint
                   | Qt::WindowSystemMenuHint
                   | Qt::WindowMinimizeButtonHint
                   | Qt::WindowMaximizeButtonHint
                   | Qt::WindowCloseButtonHint);
    setSizeGripEnabled(true);

    // Remember how big it was. A window that opens small every time is
    // a window you resize every time.
    const QByteArray geom = AppSettings::instance()
        .value(QStringLiteral("QsoMapGeometry")).toByteArray();
    if (!geom.isEmpty()) {
        restoreGeometry(geom);
    } else {
        resize(1100, 660);
    }

    buildUi();
}

void QsoMapWindow::closeEvent(QCloseEvent* e)
{
    AppSettings::instance().setValue(QStringLiteral("QsoMapGeometry"),
                                     saveGeometry());
    QDialog::closeEvent(e);
}

void QsoMapWindow::keyPressEvent(QKeyEvent* e)
{
    // The usual keys. Someone who has zoomed anything else on this
    // machine will try them before finding the buttons.
    switch (e->key()) {
    case Qt::Key_Plus:
    case Qt::Key_Equal:  zoomActiveView(1.25);  return;
    case Qt::Key_Minus:  zoomActiveView(1 / 1.25); return;
    case Qt::Key_0:      resetActiveView();     return;
    default: break;
    }
    QDialog::keyPressEvent(e);
}

void QsoMapWindow::zoomActiveView(double factor)
{
    if (m_stack->currentIndex() == 0) { m_globe->zoomBy(factor); }
    else                              { m_flat->zoomBy(factor); }
}

void QsoMapWindow::resetActiveView()
{
    if (m_stack->currentIndex() == 0) { m_globe->resetView(); }
    else                              { m_flat->resetView(); }
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

    m_grid = new QCheckBox(QStringLiteral("Grid"), this);
    m_grid->setToolTip(QStringLiteral(
        "Maidenhead locator overlay on the flat map. Click a square to "
        "see its name and what the log has there."));
    m_grid->setStyleSheet(QStringLiteral("QCheckBox { color: %1; }")
                              .arg(QString::fromLatin1(Style::kTextPrimary)));
    bar->addWidget(m_grid);

    auto* earthBtn = new QPushButton(QStringLiteral("Google Earth…"), this);
    earthBtn->setStyleSheet(Style::buttonBaseStyle());
    earthBtn->setToolTip(QStringLiteral(
        "Export what the filters currently show as a KML file and open "
        "it in Google Earth — pins with the OM's data, lines from home, "
        "one folder per band, time slider ready"));
    bar->addWidget(earthBtn);
    connect(earthBtn, &QPushButton::clicked,
            this, &QsoMapWindow::exportKml);

    // Zoom controls. The wheel already did this; nothing said so.
    auto* zoomOut = new QPushButton(QStringLiteral("−"), this);
    auto* zoomIn  = new QPushButton(QStringLiteral("+"), this);
    auto* fit     = new QPushButton(QStringLiteral("Fit"), this);
    zoomOut->setToolTip(QStringLiteral("Zoom out  (− key, or the wheel)"));
    zoomIn->setToolTip(QStringLiteral("Zoom in  (+ key, or the wheel)"));
    fit->setToolTip(QStringLiteral("Back to the whole view  (0 key, or "
                                   "double-click)"));
    for (QPushButton* b : {zoomOut, zoomIn, fit}) {
        b->setStyleSheet(Style::buttonBaseStyle());
        b->setFocusPolicy(Qt::NoFocus);   // keep the +/- keys working
        bar->addWidget(b);
    }
    connect(zoomOut, &QPushButton::clicked,
            this, [this]() { zoomActiveView(1 / 1.25); });
    connect(zoomIn, &QPushButton::clicked,
            this, [this]() { zoomActiveView(1.25); });
    connect(fit, &QPushButton::clicked,
            this, [this]() { resetActiveView(); });

    m_viewBtn = new QPushButton(QStringLiteral("Flat map"), this);
    m_viewBtn->setStyleSheet(Style::buttonBaseStyle());
    m_viewBtn->setToolTip(QStringLiteral(
        "Switch between the globe and the whole world at once"));
    bar->addWidget(m_viewBtn);
    col->addLayout(bar);

    // Second row: one pill per band and mode the log actually has,
    // filled in by rebuildFilterPills() once entries arrive.
    m_pillRow = new QHBoxLayout;
    m_pillRow->setSpacing(4);
    col->addLayout(m_pillRow);

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

    // The answer to a click: a station's data, or a grid square's
    // contents. A label rather than a popup — a popup over a map hides
    // the very thing that was clicked.
    m_info = new QLabel(QString{}, this);
    m_info->setWordWrap(true);
    m_info->setTextFormat(Qt::RichText);
    m_info->setVisible(false);
    m_info->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; background: %2; "
        "border: 1px solid %3; border-radius: 3px; padding: 6px; }")
        .arg(QString::fromLatin1(Style::kTextPrimary),
             QString::fromLatin1(Style::kInsetBg),
             QString::fromLatin1(Style::kBorderSubtle)));
    col->addWidget(m_info);

    connect(m_grid, &QCheckBox::toggled, this, [this](bool on) {
        m_flat->setShowGrid(on);
        // The overlay lives on the flat map; looking at the globe with
        // Grid ticked would show nothing and look broken.
        if (on && m_stack->currentIndex() == 0) { m_viewBtn->click(); }
    });
    connect(m_flat, &FlatMapWidget::pointClicked, this,
            [this](const QString& label, double, double) {
        showStationInfo(label);
    });
    connect(m_flat, &FlatMapWidget::gridClicked,
            this, &QsoMapWindow::showGridInfo);

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

void QsoMapWindow::setPositionFallback(PositionFallback fn)
{
    m_fallback = std::move(fn);
    rebuild();
}

void QsoMapWindow::setHomeGrid(const QString& grid)
{
    m_homeGrid = grid.trimmed().toUpper();

    // Fall back to the operator's own locator. A log written before
    // MY_GRIDSQUARE was recorded, or imported from a logger that omits
    // it, has no home in it at all — and with no home there is nothing
    // to draw a line from, so the map came up with dots and no paths
    // and no obvious reason why.
    if (!isValidGridSquare(m_homeGrid)) {
        m_homeGrid = AppSettings::instance()
                         .value(QStringLiteral("StationGridSquare"), QString{})
                         .toString().trimmed().toUpper();
    }

    if (isValidGridSquare(m_homeGrid)) {
        double lat = 0, lon = 0;
        calculateLatLonFromGridSquare(m_homeGrid, lat, lon);
        m_globe->setHome(lat, lon);
        m_globe->resetView();
        m_flat->setHome(lat, lon);
        applyStationMarker();
    } else {
        m_flat->clearHome();
    }
}

// ── Der eigene Standort als Bild ─────────────────────────────────────
//
// Rufzeichen aus den Einstellungen, Portraitadresse aus dem
// Rufzeichen-Cache, Bilddatei aus dem Portrait-Cache.
//
// Bewusst OHNE Netzwerk: ein Kartenfenster, das beim Öffnen eine
// Anfrage stellt, ist ein Kartenfenster, das ohne Internet später
// aufgeht. Was schon einmal geholt wurde, liegt auf der Platte; was
// nicht, bleibt ein Punkt mit Rufzeichen daneben. Beides ist brauchbar,
// nur eines ist hübscher.
//
// Jede Stufe darf fehlschlagen, ohne dass etwas kaputtgeht — kein
// Rufzeichen eingetragen, nie bei QRZ nachgeschlagen, kein Portrait
// hinterlegt: alle drei enden beim Punkt.
void QsoMapWindow::applyStationMarker()
{
    const QString call = AppSettings::instance()
                             .value(QStringLiteral("StationCallsign"), QString{})
                             .toString().trimmed().toUpper();
    if (call.isEmpty()) {
        m_flat->setStationMarker(QString{}, QImage{});
        return;
    }

    QImage photo;
    const CallsignInfo info = CallsignCache().get(call);
    if (!info.imageUrl.isEmpty()) {
        const QString file = StationPhoto::cachePath(info.imageUrl);
        if (QFileInfo::exists(file)) {
            photo.load(file);   // schlägt still fehl -> leeres Bild
        }
    }
    m_flat->setStationMarker(call, photo);
}

void QsoMapWindow::setEntries(const QVector<LogEntry>& entries)
{
    m_all = entries;
    rebuildFilterPills();
    rebuild();
}

// ── Filters ─────────────────────────────────────────────────────────

QString QsoMapWindow::bandKey(const LogEntry& e)
{
    const QString b = e.band.trimmed().toLower();
    return b.isEmpty() ? QStringLiteral("?") : b;
}

QString QsoMapWindow::modeKey(const LogEntry& e)
{
    const QString m = e.mode.trimmed().toUpper();
    return m.isEmpty() ? QStringLiteral("?") : m;
}

void QsoMapWindow::rebuildFilterPills()
{
    while (QLayoutItem* it = m_pillRow->takeAt(0)) {
        delete it->widget();
        delete it;
    }

    QSet<QString> bands, modes;
    for (const LogEntry& e : m_all) {
        bands.insert(bandKey(e));
        modes.insert(modeKey(e));
    }
    // A pill switched off for a band that later vanishes from the log
    // must not go on silently filtering from beyond the grave.
    m_offBands.intersect(bands);
    m_offModes.intersect(modes);

    QStringList bandList = bands.values();
    std::sort(bandList.begin(), bandList.end(),
              [](const QString& a, const QString& b) {
        return AdifLog::bandSortKeyMHz(a) < AdifLog::bandSortKeyMHz(b);
    });
    QStringList modeList = modes.values();
    std::sort(modeList.begin(), modeList.end());

    auto addCaption = [this](const QString& t) {
        auto* l = new QLabel(t, this);
        l->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }")
            .arg(QString::fromLatin1(Style::kTextScale)));
        m_pillRow->addWidget(l);
    };
    auto addPill = [this](const QString& key, const QString& label,
                          bool isBand) {
        auto* b = new QPushButton(label, this);
        b->setCheckable(true);
        b->setChecked(isBand ? !m_offBands.contains(key)
                             : !m_offModes.contains(key));
        b->setStyleSheet(Style::buttonBaseStyle()
                         + Style::greenCheckedStyle());
        b->setFocusPolicy(Qt::NoFocus);   // keep the +/- keys working
        connect(b, &QPushButton::toggled, this,
                [this, key, isBand](bool on) {
            QSet<QString>& off = isBand ? m_offBands : m_offModes;
            if (on) { off.remove(key); } else { off.insert(key); }
            rebuild();
        });
        m_pillRow->addWidget(b);
    };

    // Pills only earn their row when there is something to choose
    // between; a log entirely on one band in one mode gets no row.
    if (bandList.size() > 1) {
        addCaption(QStringLiteral("BAND"));
        for (const QString& b : bandList) { addPill(b, b, true); }
    }
    if (modeList.size() > 1) {
        addCaption(QStringLiteral("MODE"));
        for (const QString& m : modeList) { addPill(m, m, false); }
    }
    m_pillRow->addStretch(1);
}

namespace {

// Identity of one logged contact, for telling a marked row from an
// identical-looking neighbour. Callsign plus timestamp: the same
// station worked twice is two rows, and marking one must not light the
// other.
QString entryKey(const LogEntry& e)
{
    return e.call.trimmed().toUpper() + QLatin1Char('|')
           + e.timeOn.toUTC().toString(Qt::ISODate);
}

} // namespace

void QsoMapWindow::rebuild()
{
    const bool onlyMarked = m_onlySelected->isChecked()
                            && !m_selected.isEmpty();

    // Marked rows, identified per contact rather than per grid square.
    // Keying on the square meant marking one QSO lit every other QSO
    // from the same square — which for a station worked forty times is
    // forty lines the operator did not ask for.
    QSet<QString> markedRows;
    for (const LogEntry& e : m_selected) { markedRows.insert(entryKey(e)); }

    const QVector<LogEntry>& source = onlyMarked ? m_selected : m_all;
    const QDate from = m_from->date();
    const QDate to   = m_to->date();

    QVector<MapPoint> points;
    QSet<QString> seenPlaces;
    int considered   = 0;
    int noGrid       = 0;
    int markedShown  = 0;
    int markedCut    = 0;
    int markedNoGrid = 0;
    int approxCount  = 0;

    m_lastShown.clear();

    for (const LogEntry& e : source) {
        const bool marked = markedRows.contains(entryKey(e));

        if (!onlyMarked) {
            // A contact with no timestamp is kept rather than dropped:
            // an imported log full of dateless records would otherwise
            // produce an empty map for every range the operator tries.
            const QDate d = e.timeOn.toUTC().date();
            if (d.isValid() && (d < from || d > to)) { continue; }
        }

        // Band and mode pills. Applied to the marked view too — a pill
        // switched off means "I do not want to see 40 m right now", and
        // that intent does not flip with the Only-marked tick.
        if (m_offBands.contains(bandKey(e))) { continue; }
        if (m_offModes.contains(modeKey(e))) { continue; }

        ++considered;
        // What the KML export sends: exactly what the filters let
        // through, placed or not — the exporter has its own fallback.
        m_lastShown.append(e);

        double lat = 0, lon = 0;
        QString place;
        bool approximate = false;

        if (isValidGridSquare(e.gridSquare)) {
            calculateLatLonFromGridSquare(e.gridSquare, lat, lon);
            place = e.gridSquare.left(6).toUpper();
        } else if (m_fallback && m_fallback(e.call, lat, lon)) {
            // The middle of the country, which is a guess — but a
            // contact drawn approximately in Japan tells the operator
            // more than a contact not drawn at all.
            approximate = true;
            ++approxCount;
            place = QStringLiteral("~") + e.call.trimmed().toUpper();
        } else {
            ++noGrid;
            if (marked) { ++markedNoGrid; }
            continue;
        }

        if (marked) {
            // Every marked contact gets its own line, not one per
            // place. Two marked QSOs in the same square draw the same
            // path twice, which is what "show them all" means.
            if (markedShown >= kMaxMarkedPaths) { ++markedCut; continue; }
            ++markedShown;
            points.append(MapPoint{lat, lon, e.call, true, approximate});
            continue;
        }

        // Unmarked contacts stay one dot per place. Working the same
        // station forty times should not stack forty arcs on one pixel
        // and make that direction look busier than it is.
        if (seenPlaces.contains(place)) { continue; }
        seenPlaces.insert(place);
        points.append(MapPoint{lat, lon, e.call, false, approximate});
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

    if (markedShown > 0) {
        bits << QStringLiteral("%1 marked lines").arg(markedShown);
    }
    if (markedCut > 0) {
        bits << QStringLiteral("%1 more marked not drawn (limit %2)")
                    .arg(markedCut).arg(kMaxMarkedPaths);
    }
    if (markedNoGrid > 0) {
        bits << QStringLiteral("%1 marked without a locator")
                    .arg(markedNoGrid);
    }
    if (!onlyMarked) {
        bits << QStringLiteral("%1 other places").arg(seenPlaces.size());
        // A row marked outside the date range simply is not there. Say
        // so, or the operator counts the amber lines, gets fewer than
        // they marked, and concludes the marking did not work.
        const int outside = m_selected.size() - markedShown - markedCut
                            - markedNoGrid;
        if (outside > 0) {
            bits << QStringLiteral("%1 marked outside the dates")
                        .arg(outside);
        }
    }
    if (approxCount > 0) {
        bits << QStringLiteral("%1 placed from the country only (rings)")
                    .arg(approxCount);
    }
    if (noGrid - markedNoGrid > 0) {
        bits << QStringLiteral("%1 could not be placed at all")
                    .arg(noGrid - markedNoGrid);
    }
    m_summary->setText(bits.join(QStringLiteral("  ·  ")));

    // The one condition under which nothing can be drawn, said on its
    // own line and in amber rather than buried at the end of a summary.
    // Without a home there is no line to draw, and the map otherwise
    // shows dots with no explanation of the missing paths.
    if (!isValidGridSquare(m_homeGrid)) {
        m_summary->setText(
            QStringLiteral("No home locator, so no paths can be drawn. "
                           "Set MY in the Rotor / Log panel.  ·  ")
            + m_summary->text());
        m_summary->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; }")
            .arg(QString::fromLatin1(Style::kAmberText)));
    } else {
        m_summary->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; }")
            .arg(QString::fromLatin1(Style::kTextSecondary)));
    }
}

// ── Click answers ───────────────────────────────────────────────────

void QsoMapWindow::showStationInfo(const QString& label)
{
    // A '~' marks a country-guess position; the callsign follows it.
    const QString call = (label.startsWith(QLatin1Char('~'))
                              ? label.mid(1) : label).trimmed().toUpper();
    if (call.isEmpty()) { return; }

    QVector<const LogEntry*> mine;
    for (const LogEntry& e : m_all) {
        if (e.call.trimmed().toUpper() == call) { mine.append(&e); }
    }
    if (mine.isEmpty()) {
        m_info->setText(QStringLiteral("<b>%1</b> — not in the log")
                            .arg(call.toHtmlEscaped()));
        m_info->setVisible(true);
        return;
    }

    // Newest first; the newest record has the freshest name and QTH.
    std::sort(mine.begin(), mine.end(),
              [](const LogEntry* a, const LogEntry* b) {
        return a->timeOn > b->timeOn;
    });
    const LogEntry& last = *mine.first();

    QStringList who;
    if (!last.name.trimmed().isEmpty())    { who << last.name.toHtmlEscaped(); }
    if (!last.qth.trimmed().isEmpty())     { who << last.qth.toHtmlEscaped(); }
    if (!last.country.trimmed().isEmpty()) { who << last.country.toHtmlEscaped(); }

    QString text = QStringLiteral("<b>%1</b> — %2 QSO%3")
        .arg(call.toHtmlEscaped())
        .arg(mine.size())
        .arg(mine.size() == 1 ? QString{} : QStringLiteral("s"));
    if (!who.isEmpty()) {
        text += QStringLiteral(" · ") + who.join(QStringLiteral(" · "));
    }
    if (isValidGridSquare(last.gridSquare)) {
        text += QStringLiteral(" · %1").arg(last.gridSquare.toHtmlEscaped());
        if (isValidGridSquare(m_homeGrid)) {
            text += QStringLiteral(" · %1 km · %2°")
                .arg(calculateDistanceKm(m_homeGrid, last.gridSquare),
                     0, 'f', 0)
                .arg(calculateBearingInDegrees(m_homeGrid, last.gridSquare),
                     0, 'f', 0);
        }
    }

    // The most recent few, so "when did I last work him" is answered
    // without opening anything else.
    text += QStringLiteral("<br>");
    QStringList recent;
    for (int i = 0; i < mine.size() && i < 5; ++i) {
        const LogEntry& e = *mine.at(i);
        QStringList bits;
        const QDateTime u = e.timeOn.toUTC();
        if (u.isValid()) {
            bits << u.toString(QStringLiteral("yyyy-MM-dd hh:mm"));
        }
        if (!e.band.trimmed().isEmpty()) { bits << e.band.toHtmlEscaped(); }
        const QString m = e.submode.isEmpty() ? e.mode : e.submode;
        if (!m.trimmed().isEmpty()) { bits << m.toHtmlEscaped(); }
        recent << bits.join(QStringLiteral(" "));
    }
    text += recent.join(QStringLiteral(" &nbsp;·&nbsp; "));
    if (mine.size() > 5) {
        text += QStringLiteral(" &nbsp;·&nbsp; +%1 more").arg(mine.size() - 5);
    }

    m_info->setText(text);
    m_info->setVisible(true);
}

void QsoMapWindow::showGridInfo(const QString& locator)
{
    const QString loc = locator.trimmed().toUpper();
    if (loc.isEmpty()) { return; }

    QStringList calls;
    int count = 0;
    for (const LogEntry& e : m_all) {
        if (!e.gridSquare.trimmed().toUpper().startsWith(loc)) { continue; }
        ++count;
        const QString c = e.call.trimmed().toUpper();
        if (!calls.contains(c) && calls.size() < 8) { calls << c; }
    }

    if (count == 0) {
        m_info->setText(QStringLiteral(
            "<b>%1</b> — nothing in the log from this square")
            .arg(loc.toHtmlEscaped()));
    } else {
        QString text = QStringLiteral("<b>%1</b> — %2 QSO%3: %4")
            .arg(loc.toHtmlEscaped())
            .arg(count)
            .arg(count == 1 ? QString{} : QStringLiteral("s"))
            .arg(calls.join(QStringLiteral(", ")).toHtmlEscaped());
        if (count > calls.size()) {
            text += QStringLiteral(" …");
        }
        m_info->setText(text);
    }
    m_info->setVisible(true);
}

// ── Google Earth ────────────────────────────────────────────────────

void QsoMapWindow::exportKml()
{
    if (m_lastShown.isEmpty()) {
        m_info->setText(QStringLiteral(
            "Nothing to export — the current filters show no contacts."));
        m_info->setVisible(true);
        return;
    }

    const QString suggested =
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)
        + QStringLiteral("/NereusSDR-log.kml");
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export for Google Earth"), suggested,
        QStringLiteral("Google Earth KML (*.kml)"));
    if (path.isEmpty()) { return; }

    KmlExport::Options opt;
    opt.myGrid = m_homeGrid;
    opt.fallback = m_fallback;

    QString err;
    KmlExport::Result res;
    if (!KmlExport::writeKml(path, m_lastShown, opt, &err, &res)) {
        m_info->setText(QStringLiteral("KML export failed: %1")
                            .arg(err.toHtmlEscaped()));
        m_info->setVisible(true);
        return;
    }

    QString text = QStringLiteral(
        "<b>%1</b> contacts exported to %2 — opening it now. Bands are "
        "folders in Google Earth's sidebar; the time slider filters by "
        "date.").arg(res.placed).arg(path.toHtmlEscaped());
    if (res.skipped > 0) {
        text += QStringLiteral(" %1 without a locator were left out.")
                    .arg(res.skipped);
    }
    m_info->setText(text);
    m_info->setVisible(true);

    // Whatever handles .kml — Google Earth when installed, otherwise
    // the OS says so, which is a better error than any dialog here.
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

} // namespace NereusSDR
