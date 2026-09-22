// =================================================================
// src/gui/LogbookWindow.cpp  (Longpath)
// =================================================================
//
// Longpath-original — see LogbookWindow.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-09-21 — Sync QRZ: das QRZ-Logbuch abholen und wie einen Import
//                 zusammenfuehren; getroffene Kontakte gelten als bei QRZ
//                 vorhanden. Martin Fischer, AI-assisted via Anthropic Claude.
// =================================================================

#include "LogbookWindow.h"
#include "core/LogbookStats.h"
#include "gui/widgets/LogbookStatsWidget.h"

#include "core/AdifLog.h"
#include "core/BeamHeading.h"
#include "core/QsoConfirmation.h"
#include "core/AppSettings.h"
#include "core/CallsignInfo.h"
#include "core/QrzLogbookFetcher.h"
#include "core/QrzLogbookUploader.h"
#include "core/QsoUploader.h"
#include "gui/QsoMapWindow.h"
#include "gui/widgets/FlowLayout.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/QsoDetailPane.h"

#include <QSplitter>

#include <QAccessible>
#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTimeEdit>
#include <QHash>
#include <QMenu>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTextStream>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>

#include <algorithm>

namespace Longpath {

namespace {

enum Column {
    ColDate = 0, ColTime, ColCall, ColFreq, ColBand, ColMode,
    ColSent, ColRcvd, ColName, ColQth, ColCountry,
    ColGrid, ColDistance, ColQrz, ColConfirmed, ColComment, ColumnCount
};

QStringList headerLabels()
{
    return {QStringLiteral("Date"),  QStringLiteral("UTC"),
            QStringLiteral("Call"),  QStringLiteral("MHz"),
            QStringLiteral("Band"),
            QStringLiteral("Mode"),  QStringLiteral("Sent"),
            QStringLiteral("Rcvd"),  QStringLiteral("Name"),
            QStringLiteral("QTH"),   QStringLiteral("Country"),
            QStringLiteral("Grid"),  QStringLiteral("km"),
            QStringLiteral("QRZ"),   QStringLiteral("QSL"),
            QStringLiteral("Comment")};
}

// Newest first. Contacts with no timestamp sort last rather than being
// scattered through the list by a zero date.
bool newerFirst(const LogEntry& a, const LogEntry& b)
{
    if (a.timeOn.isValid() != b.timeOn.isValid()) {
        return a.timeOn.isValid();
    }
    return a.timeOn > b.timeOn;
}

} // namespace

LogbookWindow::LogbookWindow(const QString& adifPath, QWidget* parent)
    : QDialog(parent), m_path(adifPath)
{
    setWindowTitle(QStringLiteral("Logbook"));
    // Not modal: you look things up in the log *while* working a
    // station, which is exactly when a modal dialog would be in the way.
    setModal(false);
    resize(1240, 620);
    // Read before the pane is built so the first selection already has
    // whatever QRZ said last time — otherwise the first contact clicked
    // after every start looks like a station nobody has ever heard of.
    m_callCache.load();
    buildUi();
    restoreHeaderState();
    restoreSplitState();
    restoreGeometryState();
    reload();
}

void LogbookWindow::closeEvent(QCloseEvent* event)
{
    // Persist position/size on close so the next open restores it,
    // instead of Qt's default QDialog(parent) placement.
    saveGeometryState();
    QDialog::closeEvent(event);
}

void LogbookWindow::moveEvent(QMoveEvent* event)
{
    QDialog::moveEvent(event);
    saveGeometryState();
}

void LogbookWindow::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    saveGeometryState();
}

void LogbookWindow::saveGeometryState()
{
    AppSettings::instance().setValue(
        QStringLiteral("LogbookGeometryState"), saveGeometry());
}

void LogbookWindow::restoreGeometryState()
{
    const QByteArray st = AppSettings::instance()
        .value(QStringLiteral("LogbookGeometryState")).toByteArray();
    // Empty on a first run, or a saved state from before this window
    // had a screen of its own to remember -- the resize(1240, 620)
    // above already set a sane default in that case.
    if (!st.isEmpty()) { restoreGeometry(st); }
}

void LogbookWindow::restoreSplitState()
{
    const QByteArray st = AppSettings::instance()
        .value(QStringLiteral("LogbookSplitState")).toByteArray();
    if (!st.isEmpty() && m_split->restoreState(st)) { return; }
    // A first run, or a saved state from a build with a different
    // number of panes. Give the table the room and the pane enough to
    // read a callsign in.
    m_split->setSizes({860, 300});
}

void LogbookWindow::setQrzClient(QrzClient* qrz)
{
    m_qrz = qrz;
    if (m_detail) { m_detail->setQrzClient(qrz); }
    if (m_map)    { m_map->setQrzClient(qrz); }
}

// ── UI ──────────────────────────────────────────────────────────────

void LogbookWindow::buildUi()
{
    setStyleSheet(QStringLiteral("QDialog { background: %1; }")
                      .arg(QString::fromLatin1(Style::kAppBg)));

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(10, 10, 10, 10);
    col->setSpacing(8);

    // Search + Werkzeugleiste. Ein FlowLayout, kein QHBoxLayout: elf
    // Elemente nebeneinander machten das Fenster ueber 1000 px breit
    // und liessen es nicht schmaler ziehen — die Leiste bricht jetzt in
    // eine zweite Zeile um (Betreiber, 2026-09-21). Das Suchfeld nimmt,
    // was in seiner Zeile uebrig bleibt.
    auto* top = new FlowLayout(nullptr, 6, 6);
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(
        QStringLiteral("Search call, name, QTH, country, grid or comment"));
    m_search->setClearButtonEnabled(true);
    m_search->setStyleSheet(Style::lineEditStyle());
    m_search->setMinimumWidth(220);
    top->addWidget(m_search);

    m_editBtn   = new QPushButton(QStringLiteral("Edit…"), this);
    m_deleteBtn = new QPushButton(QStringLiteral("Delete"), this);
    m_uploadBtn = new QPushButton(QStringLiteral("Upload…"), this);
    m_uploadBtn->setToolTip(
        QStringLiteral("Send the selected contacts to a logging service"));
    auto* mapBtn  = new QPushButton(QStringLiteral("Map…"), this);
    mapBtn->setToolTip(
        QStringLiteral("See the contacts in a period on a globe or a "
                       "world map"));
    auto* importBtn = new QPushButton(QStringLiteral("Import…"), this);
    importBtn->setToolTip(QStringLiteral(
        "Merge an ADIF file into this log, skipping contacts it already has"));
    m_syncBtn = new QPushButton(QStringLiteral("Sync QRZ"), this);
    m_syncBtn->setToolTip(QStringLiteral(
        "Fetch your QRZ logbook and merge it into this log: contacts "
        "logged there are added, confirmations and QRZ log ids fill in, "
        "nothing you can see and edit here is overwritten. Needs the "
        "logbook key from Tools > QRZ."));
    auto* adifBtn = new QPushButton(QStringLiteral("Export ADIF…"), this);
    auto* csvBtn  = new QPushButton(QStringLiteral("Export CSV…"), this);
    auto* cabrBtn = new QPushButton(QStringLiteral("Cabrillo…"), this);
    cabrBtn->setToolTip(QStringLiteral(
        "Export the filtered view as a Cabrillo 3.0 skeleton — check "
        "the exchange column against the contest's rules before "
        "submitting"));
    auto* statsBtn = new QPushButton(QStringLiteral("Stats…"), this);
    statsBtn->setToolTip(QStringLiteral(
        "Contacts per band, mode and year, unique calls and squares, "
        "furthest DX — for whatever the filters currently show"));
    for (QPushButton* b : {m_editBtn, m_deleteBtn, m_uploadBtn, mapBtn,
                           statsBtn, importBtn, m_syncBtn, adifBtn, csvBtn, cabrBtn}) {
        b->setStyleSheet(Style::buttonBaseStyle());
        top->addWidget(b);
    }
    col->addLayout(top);
    connect(mapBtn, &QPushButton::clicked, this, &LogbookWindow::openMap);
    connect(statsBtn, &QPushButton::clicked,
            this, &LogbookWindow::showStatistics);
    connect(cabrBtn, &QPushButton::clicked,
            this, &LogbookWindow::exportCabrillo);
    connect(importBtn, &QPushButton::clicked,
            this, &LogbookWindow::importAdif);
    connect(m_syncBtn, &QPushButton::clicked,
            this, &LogbookWindow::syncFromQrz);

    connect(m_uploadBtn, &QPushButton::clicked, this, [this]() {
        if (m_uploaders.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("Upload"),
                QStringLiteral("No logging service is set up yet.\n\n"
                               "Add one under Tools."));
            return;
        }
        // Decide what is being sent before asking where. Marked rows
        // if there are any; otherwise everything not yet uploaded —
        // "what still needs to go" is the question after a session, and
        // making the operator select it by hand every time is the
        // reason people stop bothering.
        QList<int> rows = selectedSourceRows();
        const bool fromSelection = !rows.isEmpty();
        if (!fromSelection) { rows = outstandingRows(); }

        if (rows.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("Upload"),
                QStringLiteral("Everything is already uploaded."));
            return;
        }

        QMenu menu(this);
        auto* header = menu.addAction(fromSelection
            ? QStringLiteral("Send %1 marked contacts to…").arg(rows.size())
            : QStringLiteral("Send %1 not yet uploaded to…").arg(rows.size()));
        header->setEnabled(false);
        menu.addSeparator();

        QHash<QAction*, QsoUploader*> map;
        for (QsoUploader* u : m_uploaders) {
            QAction* a = menu.addAction(u->serviceName());
            // Listed but disabled rather than hidden: an operator who
            // set one up last week and sees it missing assumes the
            // feature is broken, not that a credential went astray.
            a->setEnabled(u->isConfigured());
            if (!u->isConfigured()) {
                a->setText(u->serviceName()
                           + QStringLiteral("  (not configured)"));
            }
            map.insert(a, u);
        }
        QAction* chosen = menu.exec(m_uploadBtn->mapToGlobal(
            QPoint(0, m_uploadBtn->height())));
        if (chosen && map.contains(chosen)) {
            uploadEntries(rows, map.value(chosen));
        }
    });

    buildFilterBar(col);

    // Table
    m_table = new QTableWidget(0, ColumnCount, this);
    m_table->setHorizontalHeaderLabels(headerLabels());
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    // Not QTableWidget's own sorting: it reorders the rows underneath
    // m_visible, and every row action here — edit, delete, upload, map —
    // maps a view row back to a log record through that index. Sorting
    // behind its back would silently act on the wrong contact. We sort
    // m_visible instead and rebuild.
    m_table->setSortingEnabled(false);
    m_table->horizontalHeader()->setSectionsClickable(true);
    m_table->horizontalHeader()->setSortIndicatorShown(true);
    m_table->horizontalHeader()->setSortIndicator(m_sortColumn, m_sortOrder);
    m_table->horizontalHeader()->setSectionsMovable(true);
    m_table->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    // Glas & Tiefe (2026-09-17): der zentrale Tabellenstil — versenkt,
    // Kopf als Versalzeile ohne Kaesten, Auswahl gedeckt. Schriften per
    // setFont, nicht im Stylesheet (Groesse kaskadiert sonst in den Kopf).
    m_table->setStyleSheet(Style::tableStyle());
    m_table->setFont([this] { QFont f = font(); f.setPixelSize(Style::kFontSmall); return f; }());
    m_table->horizontalHeader()->setFont(Style::capsFont(font(), 8));
    m_table->horizontalHeader()->setHighlightSections(false);

    // ── Table beside a detail pane (L1, 2026-08-10) ──────────────────
    //
    // The log keeps every ADIF field it does not model, and until now
    // kept them invisibly. A column each is not the answer — there is
    // no bound on how many a foreign logger writes — so they go in a
    // pane beside the table, which costs the table no width and shows
    // all of them for the row in hand.
    //
    // A splitter rather than a fixed width: on a laptop the pane is
    // most of the window, and an operator who wants the table back
    // should be able to drag it away rather than turn the feature off.
    m_split = new QSplitter(Qt::Horizontal, this);
    m_split->setChildrenCollapsible(true);
    m_split->addWidget(m_table);

    // Das Detailpaneel in einem Rollbereich: wird das Fenster niedriger
    // als Foto, Kopf, Peilung und Knoepfe zusammen, rollt es, statt dass
    // die Abschnitte uebereinanderrutschen (gesehen beim Verkleinern
    // auf 420 × 500, 2026-09-21). Waagrecht rollt nichts — das Paneel
    // nimmt die Breite des Rollbereichs.
    m_detail = new QsoDetailPane;
    m_detail->setCache(&m_callCache);
    auto* detailScroll = new QScrollArea(m_split);
    detailScroll->setWidgetResizable(true);
    detailScroll->setFrameShape(QFrame::NoFrame);
    detailScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detailScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    detailScroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
    detailScroll->setMinimumWidth(230);
    detailScroll->setWidget(m_detail);
    m_split->addWidget(detailScroll);
    m_split->setStretchFactor(0, 1);
    m_split->setStretchFactor(1, 0);
    col->addWidget(m_split, 1);

    // The pane can turn the antenna, but it must not know there is one
    // — same reason the window does not. Pass it on and let whoever
    // owns a rotor decide.
    connect(m_detail, &QsoDetailPane::turnRotorRequested,
            this, &LogbookWindow::turnRotorRequested);
    connect(m_detail, &QsoDetailPane::editRequested,
            this, &LogbookWindow::editSelected);

    m_stats = new QLabel(QString{}, this);
    m_stats->setWordWrap(true);
    m_stats->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }")
        .arg(QString::fromLatin1(Style::kTextSecondary)));
    col->addWidget(m_stats);

    connect(m_table->horizontalHeader(), &QHeaderView::sectionClicked,
            this, [this](int column) {
        // Second click on the same column turns it round; a new column
        // starts ascending, except the date, where the useful first
        // answer is the most recent contact.
        if (column == m_sortColumn) {
            m_sortOrder = (m_sortOrder == Qt::AscendingOrder)
                              ? Qt::DescendingOrder : Qt::AscendingOrder;
        } else {
            m_sortColumn = column;
            m_sortOrder = (column == ColDate || column == ColTime)
                              ? Qt::DescendingOrder : Qt::AscendingOrder;
        }
        m_table->horizontalHeader()->setSortIndicator(m_sortColumn, m_sortOrder);
        applySort();
        refreshTable();
    });

    // Right-click the header to hide a column. Fourteen columns is more
    // than most operators want at once, and which ones matter differs
    // per person — contest, DX chasing and casual logging each care
    // about a different half.
    // Right-click a contact to point the beam at it.
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) { showRowMenu(pos); });

    connect(m_table->horizontalHeader(), &QHeaderView::customContextMenuRequested,
            this, [this](const QPoint& pos) {
        QMenu menu(this);
        const QStringList labels = headerLabels();
        for (int c = 0; c < ColumnCount; ++c) {
            QAction* a = menu.addAction(labels.at(c));
            a->setCheckable(true);
            a->setChecked(!m_table->isColumnHidden(c));
            connect(a, &QAction::toggled, this, [this, c](bool on) {
                m_table->setColumnHidden(c, !on);
                m_headerRestored = true;   // stop auto-fitting over it
                saveHeaderState();
            });
        }
        menu.exec(m_table->horizontalHeader()->mapToGlobal(pos));
    });

    connect(m_table->horizontalHeader(), &QHeaderView::sectionResized,
            this, [this](int, int, int) {
        // Once a width has been chosen by hand, stop refitting on every
        // reload — a column that snaps back is worse than a narrow one.
        m_headerRestored = true;
        saveHeaderState();
    });

    // The pane follows the current row. currentCellChanged rather than
    // itemSelectionChanged: with several rows marked for an upload
    // there is no single contact to describe, and the one the operator
    // last touched is the one they are looking at.
    connect(m_table, &QTableWidget::currentCellChanged, this,
            [this](int row, int, int, int) {
        const int idx = sourceRow(row);
        if (idx < 0) { m_detail->clearEntry(); return; }
        m_detail->setEntry(m_all.at(idx));
    });

    connect(m_split, &QSplitter::splitterMoved, this, [this](int, int) {
        AppSettings::instance().setValue(QStringLiteral("LogbookSplitState"),
                                         m_split->saveState());
    });

    connect(m_search, &QLineEdit::textChanged, this, [this]() {
        applyFilter();
        applySort();
        refreshTable();
        updateStats();
    });

    // ── Return on a callsign means "tell me about this station" ──────
    //
    // Typing filters; Return is a decision. That distinction is what
    // makes it safe to spend a QRZ request here without asking: nobody
    // presses Return by scrolling past something.
    //
    // Three cases, and the third is the one that was missing. The call
    // is in the log — select the matching contact, which brings its
    // bearing, confirmations and ADIF fields with it. It is in the log
    // several times — the newest, because the sort already put it
    // first. It is not in the log at all — show what QRZ knows anyway,
    // because "who is this" is a fair question about a station you have
    // never worked, and an empty pane is a poor answer to it.
    connect(m_search, &QLineEdit::returnPressed, this, [this]() {
        const QString call = Callsigns::normalized(m_search->text());
        if (call.isEmpty()) { return; }

        for (int row = 0; row < m_visible.size(); ++row) {
            const int idx = sourceRow(row);
            if (idx < 0) { continue; }
            if (Callsigns::normalized(m_all.at(idx).call) != call) {
                continue;
            }
            m_table->setCurrentCell(row, ColCall);
            m_detail->setEntry(m_all.at(idx));
            m_detail->lookUpNow();
            return;
        }

        if (!Callsigns::isLikelyCallsign(call)) { return; }
        m_detail->showCallsign(call);
    });
    connect(m_editBtn,   &QPushButton::clicked, this, &LogbookWindow::editSelected);
    connect(m_deleteBtn, &QPushButton::clicked, this, &LogbookWindow::deleteSelected);
    connect(adifBtn,     &QPushButton::clicked, this, &LogbookWindow::exportAdif);
    connect(csvBtn,      &QPushButton::clicked, this, &LogbookWindow::exportCsv);
    connect(m_table, &QTableWidget::itemDoubleClicked,
            this, [this](QTableWidgetItem*) { editSelected(); });
}

void LogbookWindow::buildFilterBar(QVBoxLayout* col)
{
    // Dieselbe Umbruch-Leiste wie oben; „Clear" bleibt rechts, solange
    // die Zeile es hergibt.
    auto* row = new FlowLayout(nullptr, 6, 6);

    auto caption = [this](const QString& t) {
        auto* l = new QLabel(t, this);
        l->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                             .arg(QString::fromLatin1(Style::kTextScale)));
        return l;
    };
    // Beschriftung und Feld als EIN Element, damit der Umbruch sie nie
    // trennt — „BAND" allein am Zeilenende, das Feld in der naechsten,
    // liest niemand.
    auto pair = [this, &row](QLabel* label, QWidget* field, QWidget* second = nullptr) {
        auto* box = new QWidget(this);
        auto* h = new QHBoxLayout(box);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(4);
        h->addWidget(label);
        h->addWidget(field);
        if (second) { h->addWidget(second); }
        row->addWidget(box);
    };

    m_bandBox = new QComboBox(this);
    m_bandBox->setMinimumWidth(80);
    pair(caption(QStringLiteral("BAND")), m_bandBox);

    m_modeBox = new QComboBox(this);
    m_modeBox->setMinimumWidth(80);
    pair(caption(QStringLiteral("MODE")), m_modeBox);

    m_gridEdit = new QLineEdit(this);
    m_gridEdit->setPlaceholderText(QStringLiteral("JN, JN67, JN67VV"));
    m_gridEdit->setToolTip(QStringLiteral(
        "Matches from the start, so two characters find a whole field"));
    m_gridEdit->setFixedWidth(92);
    pair(caption(QStringLiteral("GRID")), m_gridEdit);

    m_countryEdit = new QLineEdit(this);
    m_countryEdit->setPlaceholderText(QStringLiteral("part of the name"));
    m_countryEdit->setFixedWidth(100);
    pair(caption(QStringLiteral("COUNTRY")), m_countryEdit);

    m_activationBox = new QComboBox(this);
    m_activationBox->setMinimumWidth(100);
    m_activationBox->setToolTip(QStringLiteral(
        "SOTA summit or POTA park this contact was logged from.\n\n"
        "Pick one to see just that activation — Export ADIF exports "
        "what is filtered, so this is also how one activation leaves "
        "as its own file."));
    pair(caption(QStringLiteral("ACTIVATION")), m_activationBox);

    // Off by default. A live date range would hide contacts the moment
    // the window opened, and an empty log reads as an empty log.
    // ── Only what is still outstanding ───────────────────────────────
    //
    // The question an operator actually asks of a logbook is not "which
    // contacts are confirmed" but "which are not yet" — that is the list
    // you chase, and the one you send to LoTW again.
    m_unconfirmedOnly = new QCheckBox(QStringLiteral("Unconfirmed only"),
                                      this);
    m_unconfirmedOnly->setStyleSheet(
        QStringLiteral("QCheckBox { color: %1; }")
            .arg(QString::fromLatin1(Style::kTextPrimary)));
    m_unconfirmedOnly->setToolTip(QStringLiteral(
        "Hide contacts confirmed by LoTW, a card or eQSL.\n\nA contact "
        "with a QSL merely REQUESTED still counts as unconfirmed, "
        "because it is."));
    row->addWidget(m_unconfirmedOnly);

    m_useDates = new QCheckBox(QStringLiteral("Dates"), this);
    m_useDates->setStyleSheet(QStringLiteral("QCheckBox { color: %1; }")
                                  .arg(QString::fromLatin1(Style::kTextPrimary)));

    m_fromDate = new QDateEdit(QDate::currentDate().addYears(-1), this);
    m_fromDate->setCalendarPopup(true);
    m_fromDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_fromDate->setEnabled(false);
    m_fromDate->setFixedWidth(118);

    m_toDate = new QDateEdit(QDate::currentDate(), this);
    m_toDate->setCalendarPopup(true);
    m_toDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_toDate->setEnabled(false);
    m_toDate->setFixedWidth(118);
    // Haken und beide Daten als ein Element: ein Datumsbereich, der auf
    // zwei Zeilen zerfaellt, ist keiner mehr.
    {
        auto* box = new QWidget(this);
        auto* h = new QHBoxLayout(box);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(6);
        h->addWidget(m_useDates);
        h->addWidget(m_fromDate);
        h->addWidget(m_toDate);
        row->addWidget(box);
    }

    row->addStretch();

    m_clearBtn = new QPushButton(QStringLiteral("Clear"), this);
    m_clearBtn->setStyleSheet(Style::buttonBaseStyle());
    m_clearBtn->setToolTip(QStringLiteral("Empty every filter box"));
    row->addWidget(m_clearBtn);

    for (QLineEdit* e : {m_gridEdit, m_countryEdit}) {
        e->setClearButtonEnabled(true);
        e->setStyleSheet(Style::lineEditStyle());
    }
    col->addLayout(row);

    auto reapply = [this]() {
        applyFilter();
        applySort();
        refreshTable();
        updateStats();
    };
    connect(m_bandBox, &QComboBox::currentTextChanged, this, reapply);
    connect(m_modeBox, &QComboBox::currentTextChanged, this, reapply);
    connect(m_activationBox, &QComboBox::currentTextChanged, this, reapply);
    connect(m_gridEdit, &QLineEdit::textChanged, this, reapply);
    connect(m_countryEdit, &QLineEdit::textChanged, this, reapply);
    connect(m_fromDate, &QDateEdit::dateChanged, this, reapply);
    connect(m_toDate, &QDateEdit::dateChanged, this, reapply);
    connect(m_unconfirmedOnly, &QCheckBox::toggled, this,
            [reapply](bool) { reapply(); });
    connect(m_useDates, &QCheckBox::toggled, this, [this, reapply](bool on) {
        m_fromDate->setEnabled(on);
        m_toDate->setEnabled(on);
        reapply();
    });

    connect(m_clearBtn, &QPushButton::clicked, this, [this, reapply]() {
        // Block each control rather than reapplying six times on the
        // way to an empty filter.
        QSignalBlocker b1(m_search), b2(m_bandBox), b3(m_modeBox);
        QSignalBlocker b4(m_gridEdit), b5(m_countryEdit), b6(m_useDates);
        QSignalBlocker b7(m_activationBox);
        m_search->clear();
        m_bandBox->setCurrentIndex(0);
        m_modeBox->setCurrentIndex(0);
        m_activationBox->setCurrentIndex(0);
        m_gridEdit->clear();
        m_countryEdit->clear();
        m_useDates->setChecked(false);
        m_fromDate->setEnabled(false);
        m_toDate->setEnabled(false);
        reapply();
    });
}

void LogbookWindow::refreshFilterChoices()
{
    // Only what the log actually contains. A list of every band in
    // existence is mostly a set of ways to get no results.
    auto fill = [](QComboBox* box, QStringList values) {
        const QString keep = box->currentText();
        values.removeDuplicates();
        std::sort(values.begin(), values.end());

        QSignalBlocker block(box);
        box->clear();
        box->addItem(QStringLiteral("any"));
        box->addItems(values);

        const int at = box->findText(keep);
        box->setCurrentIndex(at >= 0 ? at : 0);
    };

    QStringList bands;
    QStringList modes;
    QStringList activations;
    for (const LogEntry& e : m_all) {
        if (!e.band.trimmed().isEmpty())    { bands << e.band.trimmed(); }
        if (!e.mode.trimmed().isEmpty())    { modes << e.mode.trimmed(); }
        if (!e.submode.trimmed().isEmpty()) { modes << e.submode.trimmed(); }
        if (!e.mySotaRef.trimmed().isEmpty()) {
            activations << e.mySotaRef.trimmed();
        }
        if (!e.myPotaRef.trimmed().isEmpty()) {
            activations << e.myPotaRef.trimmed();
        }
    }
    fill(m_bandBox, bands);
    fill(m_modeBox, modes);
    fill(m_activationBox, activations);
}

LogFilter LogbookWindow::currentFilter() const
{
    LogFilter f;
    f.text    = m_search->text();
    f.grid    = m_gridEdit->text();
    f.country = m_countryEdit->text();

    // Index 0 is "any" and is not a value to match against.
    if (m_bandBox->currentIndex() > 0) { f.band = m_bandBox->currentText(); }
    if (m_modeBox->currentIndex() > 0) { f.mode = m_modeBox->currentText(); }
    if (m_activationBox->currentIndex() > 0) {
        f.activation = m_activationBox->currentText();
    }

    f.unconfirmedOnly = m_unconfirmedOnly->isChecked();
    f.useDates = m_useDates->isChecked();
    if (f.useDates) {
        f.from = m_fromDate->date();
        f.to   = m_toDate->date();
    }
    return f;
}

// ── Data ────────────────────────────────────────────────────────────

void LogbookWindow::reload()
{
    QString err;
    m_all = AdifLog::read(m_path, &err);
    if (!err.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Logbook"),
            QStringLiteral("Couldn't read the log:\n%1").arg(err));
    }
    std::stable_sort(m_all.begin(), m_all.end(), newerFirst);
    refreshFilterChoices();
    applyFilter();
    applySort();
    refreshTable();
    updateStats();
}

void LogbookWindow::applyFilter()
{
    const LogFilter f = currentFilter();
    m_visible.clear();
    m_visible.reserve(m_all.size());

    for (int i = 0; i < m_all.size(); ++i) {
        if (f.matches(m_all.at(i))) { m_visible.append(i); }
    }
}

void LogbookWindow::saveHeaderState()
{
    AppSettings::instance().setValue(
        QStringLiteral("LogbookHeaderState"),
        m_table->horizontalHeader()->saveState());
}

void LogbookWindow::restoreHeaderState()
{
    const QByteArray st = AppSettings::instance()
        .value(QStringLiteral("LogbookHeaderState")).toByteArray();
    if (st.isEmpty()) {
        // First run with the detail pane. Five columns say exactly what
        // the pane says, and with both on screen the table needs
        // horizontal scrolling to show the ones that are its own job —
        // date, callsign, band, mode, confirmed.
        //
        // Only when nothing was saved. An operator who has already
        // arranged their columns has said what they want, and quietly
        // hiding five of them because a new pane arrived would be the
        // program overruling them.
        for (int c : {ColName, ColQth, ColCountry, ColGrid, ColDistance}) {
            m_table->setColumnHidden(c, true);
        }
        return;
    }
    // A saved state from a build with fewer columns restores the old
    // count and leaves the new ones invisible with no way to find them.
    // Better to start over than to hide a column the operator never
    // hid.
    QHeaderView* h = m_table->horizontalHeader();
    if (!h->restoreState(st)) { return; }
    if (h->count() != ColumnCount) { return; }
    m_headerRestored = true;
    h->setSortIndicator(m_sortColumn, m_sortOrder);
}

void LogbookWindow::applySort()
{
    const int column = m_sortColumn;
    const bool asc = m_sortOrder == Qt::AscendingOrder;
    const QVector<LogEntry>& all = m_all;

    auto text = [&all, column](int i) -> QString {
        const LogEntry& e = all.at(i);
        switch (column) {
        case ColCall:    return e.call;
        case ColMode:    return e.submode.isEmpty() ? e.mode : e.submode;
        case ColSent:    return e.rstSent;
        case ColRcvd:    return e.rstRcvd;
        case ColName:    return e.name;
        case ColQth:     return e.qth;
        case ColCountry: return e.country;
        case ColGrid:    return e.gridSquare;
        case ColComment: return e.comment;
        default:         return {};
        }
    };

    std::stable_sort(m_visible.begin(), m_visible.end(),
                     [&](int lhs, int rhs) {
        const LogEntry& a = all.at(lhs);
        const LogEntry& b = all.at(rhs);
        bool less = false;
        switch (column) {
        case ColDate:
        case ColTime:
            // Undated contacts sort together at the end whichever way
            // the column is pointing, rather than jumping between the
            // top and the bottom as it is toggled.
            if (a.timeOn.isValid() != b.timeOn.isValid()) {
                return a.timeOn.isValid() == asc ? false : true;
            }
            less = a.timeOn < b.timeOn;
            break;
        case ColFreq:
            less = a.freqMHz < b.freqMHz;
            break;
        case ColBand:
            // By frequency, not by name. Sorted as text, "160m" comes
            // before "40m" and the band column becomes nonsense.
            less = AdifLog::bandSortKeyMHz(a.band)
                 < AdifLog::bandSortKeyMHz(b.band);
            break;
        case ColDistance:
            less = a.distanceKm < b.distanceKm;
            break;
        case ColQrz:
            // Not uploaded first when ascending: the useful question is
            // "what is still outstanding", not "what is done".
            less = a.uploadedToQrz < b.uploadedToQrz;
            break;
        case ColConfirmed:
            // Same reasoning: unconfirmed first, because that is the
            // list you act on.
            less = QsoConfirmation::isConfirmed(a)
                 < QsoConfirmation::isConfirmed(b);
            break;
        default:
            less = text(lhs).compare(text(rhs), Qt::CaseInsensitive) < 0;
            break;
        }
        return asc ? less : !less;
    });
}

void LogbookWindow::showRowMenu(const QPoint& pos)
{
    const int view = m_table->rowAt(pos.y());
    const int idx  = sourceRow(view);
    if (idx < 0) { return; }
    const LogEntry& e = m_all.at(idx);

    QMenu menu(this);

    // The bearing is only meaningful when both locators are known —
    // ours and theirs. Without it the entry has a bearing field holding
    // zero, and offering to turn the antenna due north because a grid
    // square is missing would be worse than offering nothing.
    const bool haveBearing = e.distanceKm > 0.0
                          && !e.gridSquare.trimmed().isEmpty()
                          && !e.myGridSquare.trimmed().isEmpty();

    if (haveBearing) {
        const double sp = BeamHeading::wrap360(e.bearingDeg);
        const double lp = BeamHeading::longPath(sp);
        QAction* a1 = menu.addAction(
            QStringLiteral("Turn rotor to %1 — short path %2°")
                .arg(e.call).arg(sp, 0, 'f', 0));
        QAction* a2 = menu.addAction(
            QStringLiteral("Turn rotor to %1 — long path %2°")
                .arg(e.call).arg(lp, 0, 'f', 0));
        a2->setToolTip(QStringLiteral(
            "The other way round the world. On the low bands and at grey "
            "line this is often the stronger signal."));
        connect(a1, &QAction::triggered, this,
                [this, sp, call = e.call]() {
            emit turnRotorRequested(sp, call);
        });
        connect(a2, &QAction::triggered, this,
                [this, lp, call = e.call]() {
            emit turnRotorRequested(lp, call);
        });
        menu.addSeparator();
    } else {
        QAction* none = menu.addAction(
            QStringLiteral("No bearing — needs both locators"));
        none->setEnabled(false);
        menu.addSeparator();
    }

    QAction* edit = menu.addAction(QStringLiteral("Edit…"));
    connect(edit, &QAction::triggered, this, &LogbookWindow::editSelected);

    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

void LogbookWindow::refreshTable()
{
    m_table->setRowCount(m_visible.size());

    // Bug fix 2026-09-07: this loop calls setItem()/setForeground()/
    // setFont()/setToolTip() up to ColumnCount times PER ROW, and each
    // one fires a dataChanged signal from the table's model. Qt's macOS
    // accessibility bridge does not coalesce those -- confirmed against
    // Qt's own source (qtbase src/widgets/accessible/itemviews.cpp,
    // src/plugins/platforms/cocoa/qcocoaaccessibilityelement.mm): every
    // dataChanged re-walks and reallocates the WHOLE table's
    // NSAccessibilityElement row/column arrays via
    // -[QMacAccessibilityElement populateTableArray:count:], which
    // always allocates fresh elements rather than reusing cached ones.
    // With any accessibility observer attached (VoiceOver, remote
    // control software, or a11y-based automation), that is O(rows x
    // cols) work PER CELL -- O((rows x cols)^2) total for one refresh.
    // Confirmed live: opening this window while an observer was attached
    // grew process RSS from under 1 GB to 2-5+ GB within seconds (up to
    // 164 million live QMacAccessibilityElement instances in one heap
    // snapshot), eventually forcing macOS to freeze or kill the app.
    //
    // setRowCount() above stays unblocked (the view still needs its
    // rowsInserted/rowsRemoved to keep geometry/scrollbars correct); only
    // the per-cell dataChanged flood from the loop below is suppressed,
    // and replaced with a single manual repaint + a single accessibility
    // DataChanged event covering the whole table after the loop. This
    // turns O(rows x cols) accessibility notifications into O(1) without
    // disabling accessibility for real VoiceOver users.
    {
    const QSignalBlocker modelBlocker(m_table->model());
    for (int row = 0; row < m_visible.size(); ++row) {
        const LogEntry& e = m_all.at(m_visible.at(row));
        const QDateTime u = e.timeOn.toUTC();

        auto put = [this, row](int c, const QString& s) {
            m_table->setItem(row, c, new QTableWidgetItem(s));
        };
        put(ColDate, u.isValid() ? u.toString(QStringLiteral("yyyy-MM-dd"))
                                 : QString{});
        put(ColTime, u.isValid() ? u.toString(QStringLiteral("hh:mm"))
                                 : QString{});
        put(ColCall, e.call);
        put(ColFreq, e.freqMHz > 0.0
                ? QString::number(e.freqMHz, 'f', 3) : QString{});
        put(ColBand, e.band);
        // Band coloured by band (2026-08-10): a mixed log is scanned by
        // this column, and colour reads faster than text.
        if (QTableWidgetItem* bi = m_table->item(row, ColBand)) {
            static const QHash<QString, QColor> kBandCol = {
                {QStringLiteral("160m"), QColor(Style::kTxRed)},
                {QStringLiteral("80m"),  QColor(Style::kTxRed)},
                {QStringLiteral("60m"),  QColor(Style::kAmberText)},
                {QStringLiteral("40m"),  QColor(Style::kAmberText)},
                {QStringLiteral("30m"),  QColor(Style::kAmberText)},
                {QStringLiteral("20m"),  QColor(Style::kGreenText)},
                {QStringLiteral("17m"),  QColor(Style::kDspToggleText)},
                {QStringLiteral("15m"),  QColor(Style::kBlueText)},
                {QStringLiteral("12m"),  QColor(Style::kAccent)},
                {QStringLiteral("10m"),  QColor(Style::kBlueHover)},
                {QStringLiteral("6m"),   QColor(Style::kTxRed)},
                {QStringLiteral("2m"),   QColor(Style::kTxRed)},
            };
            const QColor c = kBandCol.value(e.band.trimmed().toLower());
            if (c.isValid()) {
                bi->setForeground(c);
                QFont bf = bi->font();
                bf.setBold(true);
                bi->setFont(bf);
            }
        }
        put(ColMode, e.submode.isEmpty() ? e.mode : e.submode);
        put(ColSent, e.rstSent);
        put(ColRcvd, e.rstRcvd);
        put(ColName, e.name);
        put(ColQth, e.qth);
        put(ColCountry, e.country);
        put(ColGrid, e.gridSquare);
        put(ColDistance, e.distanceKm > 0.0
                ? QString::number(e.distanceKm, 'f', 0) : QString{});
        // A tick, not the word "yes": the column is scanned, not read.
        put(ColQrz, e.uploadedToQrz ? QStringLiteral("\u2713") : QString{});

        // ── Confirmations, at last visible ───────────────────────────
        //
        // These fields have survived an import since the ADIF
        // round-trip was fixed, and until now nothing showed them. A
        // confirmation you cannot see is not much better than one that
        // was deleted, which is what used to happen to it.
        //
        // Blank when nothing is confirmed, deliberately: an unconfirmed
        // contact should be quiet rather than compete with the
        // confirmed ones for attention.
        put(ColConfirmed, QsoConfirmation::badge(e));
        if (QTableWidgetItem* it = m_table->item(row, ColConfirmed)) {
            it->setToolTip(QsoConfirmation::describe(e));
            if (QsoConfirmation::isConfirmed(e)) {
                it->setForeground(QColor(0x00, 0xff, 0x88));
            }
        }
        put(ColComment, e.comment);
    }
    } // modelBlocker scope

    // Single manual repaint (paintEvent reads current model data directly,
    // so this is correct even though dataChanged was suppressed above) and
    // a single accessibility notification for the whole table, replacing
    // the O(rows x cols) per-cell ones that were just blocked.
    m_table->viewport()->update();
    {
        QAccessibleTableModelChangeEvent tableEvent(
            m_table, QAccessibleTableModelChangeEvent::DataChanged);
        tableEvent.setFirstRow(0);
        tableEvent.setLastRow(qMax(0, m_table->rowCount() - 1));
        tableEvent.setFirstColumn(0);
        tableEvent.setLastColumn(ColumnCount - 1);
        QAccessible::updateAccessibility(&tableEvent);
    }

    if (!m_headerRestored) { m_table->resizeColumnsToContents(); }

    // ── Something has to be selected for the pane to have a subject ──
    //
    // setRowCount() drops the current cell, so after every filter
    // keystroke and every reload there was no current row and the
    // detail pane read "Select a contact" — including the moment the
    // window opens, which is when an operator first looks at it and
    // concludes the pane is broken.
    //
    // The first row is the right default because the table is sorted
    // newest-first: the contact you want is nearly always the last one
    // you made. (2026-08-10)
    if (m_table->rowCount() > 0) {
        if (m_table->currentRow() < 0) {
            m_table->setCurrentCell(0, ColCall);
        } else {
            // The row count changed underneath the current row and the
            // signal does not fire for that, so the pane would keep
            // describing whichever contact used to be at this index.
            const int idx = sourceRow(m_table->currentRow());
            if (idx >= 0) { m_detail->setEntry(m_all.at(idx)); }
        }
    } else {
        m_detail->clearEntry();
    }
}

void LogbookWindow::updateStats()
{
    // The Kennzahlen dialog follows the same view as this status line.
    refreshStatsView();

    QSet<QString> calls;
    QSet<QString> bands;
    QSet<QString> modes;
    double longest = 0.0;
    QString longestCall;

    for (int i : m_visible) {
        const LogEntry& e = m_all.at(i);
        calls.insert(e.call.toUpper());
        if (!e.band.isEmpty()) { bands.insert(e.band.toLower()); }
        if (!e.mode.isEmpty()) { modes.insert(e.mode.toUpper()); }
        if (e.distanceKm > longest) {
            longest = e.distanceKm;
            longestCall = e.call;
        }
    }

    QStringList bits;
    bits << QStringLiteral("%1 of %2 contacts")
                .arg(m_visible.size()).arg(m_all.size());
    bits << QStringLiteral("%1 unique calls").arg(calls.size());
    if (!bands.isEmpty()) {
        bits << QStringLiteral("%1 bands").arg(bands.size());
    }
    if (!modes.isEmpty()) {
        bits << QStringLiteral("%1 modes").arg(modes.size());
    }
    if (longest > 0.0) {
        bits << QStringLiteral("furthest %1 km (%2)")
                    .arg(longest, 0, 'f', 0).arg(longestCall);
    }
    // What still has to go out, always visible rather than only on
    // opening the Upload menu. A prompt on every open would be nagging;
    // a number that is there when you look is not.
    const int outstanding = outstandingRows().size();
    if (outstanding > 0 && !m_uploaders.isEmpty()) {
        bits << QStringLiteral("%1 not uploaded").arg(outstanding);
    }

    m_stats->setText(bits.join(QStringLiteral("  ·  ")));
}

int LogbookWindow::sourceRow(int viewRow) const
{
    if (viewRow < 0 || viewRow >= m_visible.size()) { return -1; }
    return m_visible.at(viewRow);
}

QList<int> LogbookWindow::selectedSourceRows() const
{
    // Collect distinct rows first: selectedItems() yields one entry per
    // cell, so a three-row selection arrives as thirty-odd items.
    QSet<int> rows;
    for (QTableWidgetItem* it : m_table->selectedItems()) {
        rows.insert(it->row());
    }
    QList<int> viewRows(rows.begin(), rows.end());
    std::sort(viewRows.begin(), viewRows.end());

    QList<int> out;
    for (int r : viewRows) {
        const int idx = sourceRow(r);
        if (idx >= 0) { out.append(idx); }
    }
    return out;
}

void LogbookWindow::setUploaders(const QVector<QsoUploader*>& uploaders)
{
    m_uploaders = uploaders;
    for (QsoUploader* u : m_uploaders) {
        if (!u) { continue; }
        // Uploads from the panel use the same objects, so filter by
        // whether a batch of ours is outstanding. Without that, logging
        // a contact live would pop a summary box from this window.
        //
        // ── Echte Slot-Methode, kein Lambda ─────────────────────────
        //
        // Qt::UniqueConnection greift laut Dokumentation NUR bei Zeigern
        // auf Elementfunktionen und schweigend NICHT bei Lambdas. Hier
        // stand ein Lambda MIT UniqueConnection -- die Fahne tat also
        // nichts, und jeder Aufruf von setUploaders haengte eine weitere
        // Verbindung an. Zwei Aufrufe, zwei Meldungsfenster je Upload.
        connect(u, &QsoUploader::uploadFinished,
                this, &LogbookWindow::onUploadFinished,
                Qt::UniqueConnection);
    }
}

void LogbookWindow::onUploadFinished(const QString& call, bool ok,
                                     bool duplicate, const QString& message)
{
            if (m_pending <= 0) { return; }
            --m_pending;
            if (ok) {
                ++m_okCount;
                if (duplicate) { ++m_dupCount; }
                // A duplicate counts as uploaded: the service has it.
                // Leaving it unmarked would offer it again on every
                // future run, for ever.
                for (int idx : m_uploadBatch) {
                    if (idx >= 0 && idx < m_all.size()
                        && !m_all[idx].uploadedToQrz
                        && Callsigns::normalized(m_all.at(idx).call)
                               == Callsigns::normalized(call)) {
                        m_all[idx].uploadedToQrz = true;
                        break;
                    }
                }
            } else {
                m_failures << QStringLiteral("%1: %2").arg(call, message);
            }
            if (m_pending > 0) { return; }

            // One summary at the end, not a box per contact.
            QString text = QStringLiteral("%1 accepted").arg(m_okCount);
            if (m_dupCount > 0) {
                text += QStringLiteral(", of which %1 already present")
                            .arg(m_dupCount);
            }
            if (!m_failures.isEmpty()) {
                text += QStringLiteral("\n\n%1 failed:\n%2")
                            .arg(m_failures.size())
                            .arg(m_failures.mid(0, 10)
                                     .join(QLatin1Char('\n')));
                if (m_failures.size() > 10) {
                    text += QStringLiteral("\n…and %1 more")
                                .arg(m_failures.size() - 10);
                }
            }
            // Write the marks once for the whole batch, not once per
            // contact: a five-hundred-contact upload would otherwise
            // rewrite the log five hundred times.
            if (m_okCount > 0) { saveAll(); }

            const bool anyFailed = !m_failures.isEmpty();
            m_okCount = 0;
            m_dupCount = 0;
            m_failures.clear();
            m_uploadBatch.clear();
            m_uploadBtn->setEnabled(true);

            if (anyFailed) {
                QMessageBox::warning(this, QStringLiteral("Upload"), text);
            } else {
                QMessageBox::information(this, QStringLiteral("Upload"), text);
            }
}

QList<int> LogbookWindow::outstandingRows() const
{
    QList<int> out;
    for (int i = 0; i < m_all.size(); ++i) {
        if (!m_all.at(i).uploadedToQrz) { out.append(i); }
    }
    return out;
}

void LogbookWindow::uploadEntries(const QList<int>& sourceRows,
                                  QsoUploader* target)
{
    if (!target || sourceRows.isEmpty()) { return; }
    if (m_pending > 0) { return; }   // a batch is already running

    // Confirm the count. Sending 400 contacts because a stray Ctrl-A
    // selected the whole log is not recoverable at the far end.
    if (sourceRows.size() > 1) {
        if (QMessageBox::question(this, QStringLiteral("Upload"),
                QStringLiteral("Send %1 contacts to %2?")
                    .arg(sourceRows.size()).arg(target->serviceName()),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
    }

    m_pending     = sourceRows.size();
    m_okCount     = 0;
    m_dupCount    = 0;
    m_uploadBatch = sourceRows;
    m_failures.clear();
    m_uploadBtn->setEnabled(false);

    for (int idx : sourceRows) { target->upload(m_all.at(idx)); }
}

// ── Saving and correcting ───────────────────────────────────────────
//
// These three went missing in the one-click-logging commit: an edit
// script replaced a region and took the tail of the file with it.
// Nothing complained until the linker did, because a definition that
// is simply absent compiles perfectly well.

bool LogbookWindow::saveAll()
{
    // The table shows newest first, but the file stays chronological.
    // Writing m_all in display order would silently reverse the log on
    // the first correction, and every other reader of the file — the
    // dock's recent list, another logger's import — assumes oldest
    // first.
    QVector<LogEntry> chronological = m_all;
    std::stable_sort(chronological.begin(), chronological.end(),
                     [](const LogEntry& a, const LogEntry& b) {
        if (a.timeOn.isValid() != b.timeOn.isValid()) {
            return b.timeOn.isValid();
        }
        return a.timeOn < b.timeOn;
    });

    QString err;
    if (AdifLog::write(m_path, chronological, &err)) {
        emit logChanged();
        return true;
    }
    QMessageBox::critical(this, QStringLiteral("Logbook"),
        QStringLiteral("Couldn't save the log:\n%1\n\n"
                       "Your previous log file is untouched.").arg(err));
    return false;
}

void LogbookWindow::editSelected()
{
    const int view = m_table->currentRow();
    const int idx  = sourceRow(view);
    if (idx < 0) { return; }

    LogEntry e = m_all.at(idx);

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Edit %1").arg(e.call));
    auto* form = new QFormLayout(&dlg);

    auto* call    = new QLineEdit(e.call, &dlg);
    auto* when    = new QDateTimeEdit(e.timeOn.toUTC(), &dlg);
    when->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    when->setCalendarPopup(true);
    // The stored time is UTC and stays UTC. A widget that silently
    // showed local time would rewrite every edited contact by the
    // offset, and nothing would look wrong until someone compared logs.
    //
    // QDateTimeEdit::setTimeZone() is Qt 6.7+ (the ubuntu-24.04-arm
    // release runner's system package is Qt 6.4.2); pre-6.7 there is no
    // way to lock the edit's zone explicitly, so this falls back to the
    // UTC-constructed QDateTime passed to the constructor above, which
    // is the best available approximation on that Qt version.
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    when->setTimeZone(QTimeZone::UTC);
#endif

    auto* band    = new QLineEdit(e.band, &dlg);
    auto* mode    = new QLineEdit(e.mode, &dlg);
    auto* submode = new QLineEdit(e.submode, &dlg);
    auto* sent    = new QLineEdit(e.rstSent, &dlg);
    auto* rcvd    = new QLineEdit(e.rstRcvd, &dlg);
    auto* grid    = new QLineEdit(e.gridSquare, &dlg);
    auto* myGrid  = new QLineEdit(e.myGridSquare, &dlg);

    // One field for either scheme rather than four: SOTA references
    // always carry a '/' (W2/WE-003, G/LD-003), POTA park references
    // never do (US-0005, OE-1234) — that alone is enough to sort a
    // typed reference into the right ADIF field on save (see below).
    // Whichever of the pair is set shows here; both are cleared before
    // the save re-populates one of them, so switching schemes on an
    // existing contact does not leave the old one behind.
    auto* myActivation = new QLineEdit(
        e.mySotaRef.isEmpty() ? e.myPotaRef : e.mySotaRef, &dlg);
    myActivation->setPlaceholderText(
        QStringLiteral("summit or park you're activating, e.g. OE/OO-001 or OE-1234"));
    auto* theirActivation = new QLineEdit(
        e.sotaRef.isEmpty() ? e.potaRef : e.sotaRef, &dlg);
    theirActivation->setPlaceholderText(
        QStringLiteral("their summit/park, if they're activating too"));

    auto* name    = new QLineEdit(e.name, &dlg);
    auto* qth     = new QLineEdit(e.qth, &dlg);
    auto* country = new QLineEdit(e.country, &dlg);
    auto* comment = new QLineEdit(e.comment, &dlg);

    form->addRow(QStringLiteral("Call"),        call);
    form->addRow(QStringLiteral("UTC"),         when);
    form->addRow(QStringLiteral("Band"),        band);
    form->addRow(QStringLiteral("Mode"),        mode);
    form->addRow(QStringLiteral("Submode"),     submode);
    form->addRow(QStringLiteral("RST sent"),    sent);
    form->addRow(QStringLiteral("RST rcvd"),    rcvd);
    form->addRow(QStringLiteral("Their grid"),  grid);
    form->addRow(QStringLiteral("My grid"),     myGrid);
    form->addRow(QStringLiteral("My activation (SOTA/POTA)"), myActivation);
    form->addRow(QStringLiteral("Their activation"), theirActivation);
    form->addRow(QStringLiteral("Name"),        name);
    form->addRow(QStringLiteral("QTH"),         qth);
    form->addRow(QStringLiteral("Country"),     country);
    form->addRow(QStringLiteral("Comment"),     comment);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Save
                                     | QDialogButtonBox::Cancel, &dlg);
    form->addRow(box);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) { return; }

    if (call->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Logbook"),
            QStringLiteral("A contact needs a callsign."));
        return;
    }

    e.call         = call->text().trimmed().toUpper();
    e.timeOn       = when->dateTime();
    e.band         = band->text().trimmed();
    e.mode         = mode->text().trimmed().toUpper();
    e.submode      = submode->text().trimmed().toUpper();
    e.rstSent      = sent->text().trimmed();
    e.rstRcvd      = rcvd->text().trimmed();
    e.gridSquare   = grid->text().trimmed().toUpper();
    e.myGridSquare = myGrid->text().trimmed().toUpper();

    e.mySotaRef.clear();
    e.myPotaRef.clear();
    if (const QString mine = myActivation->text().trimmed().toUpper();
        !mine.isEmpty()) {
        if (mine.contains(QLatin1Char('/'))) { e.mySotaRef = mine; }
        else                                 { e.myPotaRef = mine; }
    }
    e.sotaRef.clear();
    e.potaRef.clear();
    if (const QString theirs = theirActivation->text().trimmed().toUpper();
        !theirs.isEmpty()) {
        if (theirs.contains(QLatin1Char('/'))) { e.sotaRef = theirs; }
        else                                   { e.potaRef = theirs; }
    }

    e.name         = name->text().trimmed();
    e.qth          = qth->text().trimmed();
    e.country      = country->text().trimmed();
    e.comment      = comment->text().trimmed();

    m_all[idx] = e;
    if (saveAll()) { reload(); }
}

void LogbookWindow::deleteSelected()
{
    // Collect source indices first: deleting by view row while the view
    // is being rebuilt underneath is how you remove the wrong contact.
    QList<int> victims = selectedSourceRows();
    if (victims.isEmpty()) { return; }

    const QString question = victims.size() == 1
        ? QStringLiteral("Delete the contact with %1?")
              .arg(m_all.at(victims.first()).call)
        : QStringLiteral("Delete %1 contacts?").arg(victims.size());

    if (QMessageBox::question(this, QStringLiteral("Logbook"), question,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }

    std::sort(victims.begin(), victims.end(), std::greater<int>());
    for (int idx : victims) { m_all.removeAt(idx); }
    if (saveAll()) { reload(); }
}

// ── Map ─────────────────────────────────────────────────────────────

void LogbookWindow::setPositionFallback(PositionFallback fn)
{
    m_fallback = std::move(fn);
    if (m_map) { m_map->setPositionFallback(m_fallback); }
}

void LogbookWindow::setSatellites(SatelliteService* svc)
{
    m_satellites = svc;
    if (m_map) { m_map->setSatellites(svc); }
}

void LogbookWindow::openMap()
{
    if (!m_map) {
        m_map = new QsoMapWindow(this);
        m_map->setQrzClient(m_qrz);
        m_map->setSatellites(m_satellites);
        // Eine Station, die der Betreiber im Logbuch nachschlaegt, landet
        // auf der offenen Karte — der Flug ist der Sinn der Karte.
        connect(m_detail, &QsoDetailPane::stationLocated, m_map,
                [this](const QString& call, double lat, double lon, const QString& caption) {
            if (m_map && m_map->isVisible()) { m_map->flyToStation(call, lat, lon, caption); }
        });
    }
    m_map->setPositionFallback(m_fallback);

    // Home comes from whichever contact recorded it most recently. The
    // panel knows the operator's locator, but the map is opened from
    // here — and the log carries the same answer, with the advantage of
    // being right for an imported log made from a different station.
    QString home;
    for (const LogEntry& e : m_all) {
        if (!e.myGridSquare.trimmed().isEmpty()) {
            home = e.myGridSquare;
            break;   // m_all is newest first
        }
    }
    m_map->setHomeGrid(home);
    m_map->setEntries(m_all);

    // Rows picked out in the table go to the map as a selection. With
    // none picked, the map falls back to its date range — opening it
    // with nothing selected should still show something.
    QVector<LogEntry> marked;
    for (int idx : selectedSourceRows()) { marked.append(m_all.at(idx)); }
    m_map->setSelection(marked);

    m_map->show();
    m_map->raise();
    m_map->activateWindow();
}

// ── Import ──────────────────────────────────────────────────────────

QString LogbookWindow::makeBackup(QString* error) const
{
    if (!QFile::exists(m_path)) { return {}; }   // nothing to lose yet

    const QString stamp = QDateTime::currentDateTimeUtc()
                              .toString(QStringLiteral("yyyyMMdd-hhmmss"));
    const QString dest = m_path + QStringLiteral(".") + stamp
                       + QStringLiteral(".bak");
    if (!QFile::copy(m_path, dest)) {
        if (error) {
            *error = QStringLiteral("couldn't write %1").arg(dest);
        }
        return {};
    }
    return dest;
}

void LogbookWindow::importAdif()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import ADIF"), QString{},
        QStringLiteral("ADIF (*.adi *.adif);;All files (*)"));
    if (path.isEmpty()) { return; }
    importAdifFile(path);
}

// ── Der Dateidialog und das Einlesen sind zwei Dinge ────────────────
//
// Sie standen in einer Funktion, und damit war der ganze Weg —
// lesen, zusammenfuehren, sichern, in die Tabelle stellen — nur mit
// einem Klick eines Menschen erreichbar. Kein Test kam daran vorbei,
// weil getOpenFileName stehenbleibt und auf eine Maus wartet.
//
// Der Betreiber am 2026-08-21: „logbuch leer". Ob daran die Knoepfe,
// das Einlesen oder das Zusammenfuehren schuld war, liess sich nicht
// nachsehen, sondern nur raten. Genau der Zustand, aus dem in dieser
// Sitzung schon mehrere Fehlschluesse gekommen sind.
//
// Jetzt nimmt importAdifFile() einen Pfad. Der Dialog sucht ihn aus,
// ein Test reicht ihn hin. Derselbe Weg, beide Male.
void LogbookWindow::importAdifFile(const QString& path)
{
    QString err;
    const QVector<LogEntry> incoming = AdifLog::read(path, &err);
    if (!err.isEmpty()) {
        tellOperator(QStringLiteral("Couldn't read that file:\n%1").arg(err));
        return;
    }
    if (incoming.isEmpty()) {
        // Distinguish "empty" from "not ADIF": a file the parser found
        // no records in is usually the wrong file, not an empty log.
        tellOperator(
            QStringLiteral("No contacts found in that file.\n\n"
                           "ADIF records are marked with <EOR>; if this "
                           "is a Cabrillo or CSV export it needs "
                           "converting first."));
        return;
    }

    importEntries(incoming, QStringLiteral("that file"), /*fromQrz*/ false);
}

void LogbookWindow::importEntries(const QVector<LogEntry>& incoming,
                                  const QString& source, bool fromQrz)
{
    AdifLog::MergeResult r = AdifLog::merge(m_all, incoming);

    // Was aus dem QRZ-Logbuch kommt, IST im QRZ-Logbuch — der Upload
    // braucht es nicht mehr zu schicken. uploadedToQrz ist kein Feld,
    // das der Betreiber sieht und pflegt, also darf es hier gesetzt
    // werden, auch an Kontakten, die er schon hatte.
    int markedQrz = 0;
    if (fromQrz) {
        QHash<QString, QVector<const LogEntry*>> byCall;
        for (const LogEntry& in : incoming) {
            byCall[in.call.trimmed().toUpper()].append(&in);
        }
        for (LogEntry& e : r.merged) {
            if (e.uploadedToQrz) { continue; }
            const auto it = byCall.constFind(e.call.trimmed().toUpper());
            if (it == byCall.constEnd()) { continue; }
            for (const LogEntry* in : it.value()) {
                if (AdifLog::isSameQso(e, *in)) { e.uploadedToQrz = true; ++markedQrz; break; }
            }
        }
    }

    // "Nothing new" is not the same as "nothing to do". A confirmation
    // report from LoTW or eQSL is a file of contacts you already have,
    // and the whole point of importing it is the fields it carries that
    // your copies do not. Bailing out on added == 0 threw those away
    // and told the operator everything was fine.
    if (r.added == 0 && r.enriched == 0 && markedQrz == 0) {
        tellOperator(
            QStringLiteral("All %1 contacts in %2 are already in "
                           "your log, and none of them carried anything "
                           "your copies were missing. Nothing to do.")
                .arg(incoming.size()).arg(source));
        return;
    }

    QString question =
        QStringLiteral("%1 contacts in %4.\n\n"
                       "%2 are new and will be added.\n"
                       "%3 are already in your log.\n")
            .arg(incoming.size()).arg(r.added).arg(r.skipped).arg(source);
    if (markedQrz > 0) {
        question += QStringLiteral(
            "\n%1 of yours will be marked as present in your QRZ logbook, "
            "so Upload will not send them again.\n").arg(markedQrz);
    }
    if (r.enriched > 0) {
        question += QStringLiteral(
            "\nOf those, %1 carry fields your copies do not have — "
            "confirmations, awards data or another logger's notes. Those "
            "fields will be filled in. Nothing you can see and edit in "
            "this window is overwritten.\n")
            .arg(r.enriched);
    }
    question += QStringLiteral(
        "\nYour current log is copied to a dated backup first. "
        "Go ahead?");

    if (!askOperator(question)) { return; }

    // Back up before touching anything. This is the one operation here
    // that rewrites many records at once, and unlike an edit or a delete
    // it cannot be put right from the table afterwards.
    QString backupErr;
    const QString backup = makeBackup(&backupErr);
    if (!backupErr.isEmpty()) {
        if (!askOperator(
                QStringLiteral("Couldn't make a backup first: %1\n\n"
                               "Import anyway?").arg(backupErr))) {
            return;
        }
    }

    m_all = r.merged;
    if (!saveAll()) {
        // saveAll already explained itself and left the old file intact.
        reload();
        return;
    }
    reload();

    QString done = QStringLiteral("Added %1 contacts, skipped %2 already "
                                  "present.").arg(r.added).arg(r.skipped);
    if (r.enriched > 0) {
        done += QStringLiteral("\nFilled in missing fields on %1 of them.")
                    .arg(r.enriched);
    }
    if (markedQrz > 0) {
        done += QStringLiteral("\nMarked %1 as present in your QRZ logbook.")
                    .arg(markedQrz);
    }
    if (!backup.isEmpty()) {
        done += QStringLiteral("\n\nBackup: %1").arg(backup);
    }
    tellOperator(done);
}

// ── Wer die Rueckfragen stellt ──────────────────────────────────────
//
// Voreingestellt ein Fenster, wie bisher. Ein Test setzt stattdessen
// eine Funktion ein, die „ja" sagt und die Meldungen mitschreibt.
//
// Der Grund ist nicht Bequemlichkeit: ohne diese Stelle ist der
// Import-Weg fuer einen Test unerreichbar, weil QMessageBox auf eine
// Maus wartet. Ein Weg, den kein Test gehen kann, ist ein Weg, ueber
// den man nur Vermutungen hat — und davon hatte diese Sitzung genug.
bool LogbookWindow::askOperator(const QString& question)
{
    if (m_ask) { return m_ask(question); }
    return QMessageBox::question(this, QStringLiteral("Import"), question,
               QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
           == QMessageBox::Yes;
}

void LogbookWindow::tellOperator(const QString& message)
{
    if (m_tell) { m_tell(message); return; }
    QMessageBox::information(this, QStringLiteral("Import"), message);
}

// ── Export ──────────────────────────────────────────────────────────

void LogbookWindow::exportAdif()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export ADIF"),
        QStringLiteral("longpath-log.adi"),
        QStringLiteral("ADIF (*.adi *.adif)"));
    if (path.isEmpty()) { return; }

    // Exports what is on screen, not the whole file. Filtering to one
    // band or one callsign and exporting that is a normal thing to want,
    // and the count is shown so nobody exports 12 records thinking they
    // got 1200.
    QVector<LogEntry> subset;
    subset.reserve(m_visible.size());
    for (int i : m_visible) { subset.append(m_all.at(i)); }

    QString err;
    if (!AdifLog::write(path, subset, &err)) {
        QMessageBox::warning(this, QStringLiteral("Logbook"),
            QStringLiteral("Export failed:\n%1").arg(err));
        return;
    }
    QMessageBox::information(this, QStringLiteral("Logbook"),
        QStringLiteral("Exported %1 contacts.").arg(subset.size()));
}

void LogbookWindow::exportCsv()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export CSV"),
        QStringLiteral("longpath-log.csv"),
        QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) { return; }

    QVector<LogEntry> subset;
    subset.reserve(m_visible.size());
    for (int i : m_visible) { subset.append(m_all.at(i)); }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Logbook"),
            QStringLiteral("Export failed:\n%1").arg(f.errorString()));
        return;
    }
    QTextStream out(&f);
    out << AdifLog::toCsv(subset);
    out.flush();
    QMessageBox::information(this, QStringLiteral("Logbook"),
        QStringLiteral("Exported %1 contacts.").arg(subset.size()));
}

// ── Cabrillo (2026-08-10) ───────────────────────────────────────────

void LogbookWindow::exportCabrillo()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export Cabrillo"),
        QStringLiteral("longpath-log.cbr"),
        QStringLiteral("Cabrillo (*.cbr *.log)"));
    if (path.isEmpty()) { return; }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Logbook"),
            QStringLiteral("Export failed:\n%1").arg(f.errorString()));
        return;
    }

    const QString myCall = AppSettings::instance()
        .value(QStringLiteral("User/Callsign"), QString{})
        .toString().trimmed().toUpper();

    // Mode → Cabrillo's two letters. Everything digital is DG except
    // RTTY, which contests treat as its own thing.
    auto cbrMode = [](const LogEntry& e) {
        const QString m = e.mode.trimmed().toUpper();
        if (m == QLatin1String("CW"))  { return QStringLiteral("CW"); }
        if (m == QLatin1String("SSB") || m == QLatin1String("USB")
            || m == QLatin1String("LSB") || m == QLatin1String("AM")
            || m == QLatin1String("FM")) { return QStringLiteral("PH"); }
        if (m == QLatin1String("RTTY")) { return QStringLiteral("RY"); }
        return QStringLiteral("DG");
    };
    // Frequency in kHz; when only the band is known, its lower edge —
    // Cabrillo robots accept a band-representative figure.
    auto cbrFreq = [](const LogEntry& e) {
        if (e.freqMHz > 0.0) {
            return QString::number(qRound(e.freqMHz * 1000.0));
        }
        static const QHash<QString, int> kEdge = {
            {QStringLiteral("160m"), 1800}, {QStringLiteral("80m"), 3500},
            {QStringLiteral("60m"), 5330},  {QStringLiteral("40m"), 7000},
            {QStringLiteral("30m"), 10100}, {QStringLiteral("20m"), 14000},
            {QStringLiteral("17m"), 18068}, {QStringLiteral("15m"), 21000},
            {QStringLiteral("12m"), 24890}, {QStringLiteral("10m"), 28000},
            {QStringLiteral("6m"), 50000},  {QStringLiteral("2m"), 144000},
        };
        return QString::number(
            kEdge.value(e.band.trimmed().toLower(), 0));
    };

    QTextStream out(&f);
    out << "START-OF-LOG: 3.0\n"
        << "CREATED-BY: Longpath\n"
        << "CALLSIGN: " << (myCall.isEmpty()
                                ? QStringLiteral("NOCALL") : myCall)
        << "\n";

    int written = 0;
    for (int i : m_visible) {
        const LogEntry& e = m_all.at(i);
        const QDateTime u = e.timeOn.toUTC();
        if (!u.isValid() || e.call.trimmed().isEmpty()) { continue; }

        // Exchange: the grid where one is known, a placeholder where
        // not. Contests differ; the dialog below says to check this.
        const QString sentX = QStringLiteral("---");
        const QString rcvdX = e.gridSquare.trimmed().isEmpty()
            ? QStringLiteral("---")
            : e.gridSquare.trimmed().toUpper().left(4);

        out << QStringLiteral("QSO: %1 %2 %3 %4 %5 %6 %7 %8 %9 %10\n")
            .arg(cbrFreq(e), 5)
            .arg(cbrMode(e))
            .arg(u.toString(QStringLiteral("yyyy-MM-dd")))
            .arg(u.toString(QStringLiteral("hhmm")))
            .arg(myCall.isEmpty() ? QStringLiteral("NOCALL") : myCall, -10)
            .arg(e.rstSent.trimmed().isEmpty()
                     ? QStringLiteral("59") : e.rstSent.trimmed(), 3)
            .arg(sentX, 6)
            .arg(e.call.trimmed().toUpper(), -10)
            .arg(e.rstRcvd.trimmed().isEmpty()
                     ? QStringLiteral("59") : e.rstRcvd.trimmed(), 3)
            .arg(rcvdX, 6);
        ++written;
    }
    out << "END-OF-LOG:\n";
    out.flush();

    QMessageBox::information(this, QStringLiteral("Logbook"),
        QStringLiteral("Wrote %1 QSO lines.\n\nCabrillo exchanges differ "
                       "by contest — check the exchange columns against "
                       "the rules before submitting.").arg(written));
}

// ── Statistics (2026-08-10) ─────────────────────────────────────────

void LogbookWindow::showStatistics()
{
    // The Kennzahlen view (2026-09-18, benchmark item 4): six tiles over
    // the filtered view -- same rule as every export here: filter to one
    // year or one band and the numbers answer for exactly that. One
    // modeless dialog, reused, refreshed whenever the log or the filter
    // changes while it is open.
    if (!m_statsDialog) {
        m_statsDialog = new QDialog(this);
        m_statsDialog->setWindowTitle(QStringLiteral("Logbook statistics"));
        m_statsDialog->setModal(false);
        m_statsDialog->resize(900, 560);
        m_statsDialog->setStyleSheet(QStringLiteral("QDialog { background: %1; }")
                                         .arg(QLatin1String(Style::kAppBg)));
        auto* lay = new QVBoxLayout(m_statsDialog);
        lay->setContentsMargins(0, 0, 0, 0);
        auto* scroll = new QScrollArea(m_statsDialog);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setStyleSheet(QStringLiteral("QScrollArea { background: %1; border: none; }")
                                  .arg(QLatin1String(Style::kAppBg)));
        m_statsView = new LogbookStatsWidget(scroll);
        scroll->setWidget(m_statsView);
        lay->addWidget(scroll, 1);
        auto* closeBtn = new QPushButton(QStringLiteral("Close"), m_statsDialog);
        closeBtn->setStyleSheet(Style::buttonBaseStyle());
        connect(closeBtn, &QPushButton::clicked, m_statsDialog, &QDialog::hide);
        auto* foot = new QHBoxLayout;
        foot->setContentsMargins(10, 4, 10, 10);
        foot->addStretch(1);
        foot->addWidget(closeBtn);
        lay->addLayout(foot);
    }
    m_statsDialog->show();
    m_statsDialog->raise();
    m_statsDialog->activateWindow();
    refreshStatsView();   // after show(): it only works on a visible dialog
}

void LogbookWindow::refreshStatsView()
{
    if (!m_statsView || !m_statsDialog || !m_statsDialog->isVisible()) { return; }
    QVector<LogEntry> shown;
    shown.reserve(m_visible.size());
    for (int i : m_visible) { shown.push_back(m_all.at(i)); }
    m_statsView->setStats(LogbookStats::compute(shown, m_cty,
                                                QDateTime::currentDateTimeUtc()));
}


// ── QRZ-Logbuch abholen ──────────────────────────────────────────────

void LogbookWindow::setQrzLogbookUploader(QrzLogbookUploader* uploader)
{
    m_qrzUploader = uploader;
}

void LogbookWindow::syncFromQrz()
{
    const QString key = m_qrzUploader ? m_qrzUploader->apiKey() : QString();
    if (key.trimmed().isEmpty()) {
        tellOperator(QStringLiteral(
            "No QRZ logbook key yet.\n\nAdd it under Tools > QRZ — it is the "
            "logbook access key from your QRZ logbook's settings page, not "
            "your QRZ password."));
        return;
    }
    if (!m_qrzFetcher) {
        m_qrzFetcher = new QrzLogbookFetcher(this);
        connect(m_qrzFetcher, &QrzLogbookFetcher::progress, this,
                [this](int records, int page) {
            if (m_syncBtn) {
                m_syncBtn->setText(QStringLiteral("Syncing… %1 (page %2)")
                                       .arg(records).arg(page));
            }
        });
        connect(m_qrzFetcher, &QrzLogbookFetcher::finished, this,
                [this](bool ok, const QString& adif, int records, const QString& error) {
            if (m_syncBtn) {
                m_syncBtn->setEnabled(true);
                m_syncBtn->setText(QStringLiteral("Sync QRZ"));
            }
            if (!ok) {
                tellOperator(QStringLiteral("Couldn't fetch your QRZ logbook:\n%1")
                                 .arg(error));
                return;
            }
            if (records == 0 || adif.trimmed().isEmpty()) {
                tellOperator(QStringLiteral("Your QRZ logbook is empty — nothing to merge."));
                return;
            }
            mergeFetchedEntries(adif);
        });
    }
    if (m_qrzFetcher->isBusy()) { return; }
    m_qrzFetcher->setApiKey(key);
    if (m_syncBtn) {
        m_syncBtn->setEnabled(false);
        m_syncBtn->setText(QStringLiteral("Syncing…"));
    }
    m_qrzFetcher->fetchAll();
}

void LogbookWindow::mergeFetchedEntries(const QString& adif)
{
    QVector<LogEntry> incoming = AdifLog::parse(adif);
    if (incoming.isEmpty()) {
        tellOperator(QStringLiteral(
            "QRZ sent data, but no contacts could be read from it."));
        return;
    }
    // Aus dem QRZ-Logbuch kommt, was dort steht — die Markierung
    // „hochgeladen" gilt fuer jeden dieser Datensaetze.
    for (LogEntry& e : incoming) { e.uploadedToQrz = true; }
    importEntries(incoming, QStringLiteral("your QRZ logbook"), /*fromQrz*/ true);
}

} // namespace Longpath
