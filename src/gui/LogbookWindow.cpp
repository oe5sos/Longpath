// =================================================================
// src/gui/LogbookWindow.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — see LogbookWindow.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "LogbookWindow.h"

#include "core/AdifLog.h"
#include "core/QsoUploader.h"
#include "gui/QsoMapWindow.h"
#include "gui/StyleConstants.h"

#include <QAction>
#include <QDateTimeEdit>
#include <QHash>
#include <QMenu>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QTextStream>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>

namespace NereusSDR {

namespace {

enum Column {
    ColDate = 0, ColTime, ColCall, ColBand, ColMode,
    ColSent, ColRcvd, ColName, ColQth, ColCountry,
    ColGrid, ColDistance, ColComment, ColumnCount
};

QStringList headerLabels()
{
    return {QStringLiteral("Date"),  QStringLiteral("UTC"),
            QStringLiteral("Call"),  QStringLiteral("Band"),
            QStringLiteral("Mode"),  QStringLiteral("Sent"),
            QStringLiteral("Rcvd"),  QStringLiteral("Name"),
            QStringLiteral("QTH"),   QStringLiteral("Country"),
            QStringLiteral("Grid"),  QStringLiteral("km"),
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
    resize(1000, 560);
    buildUi();
    reload();
}

// ── UI ──────────────────────────────────────────────────────────────

void LogbookWindow::buildUi()
{
    setStyleSheet(QStringLiteral("QDialog { background: %1; }")
                      .arg(QString::fromLatin1(Style::kAppBg)));

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(10, 10, 10, 10);
    col->setSpacing(8);

    // Search
    auto* top = new QHBoxLayout;
    top->setSpacing(6);
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(
        QStringLiteral("Search call, name, QTH, country, grid or comment"));
    m_search->setClearButtonEnabled(true);
    m_search->setStyleSheet(QString::fromLatin1(Style::kLineEditStyle));
    top->addWidget(m_search, 1);

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
    auto* adifBtn = new QPushButton(QStringLiteral("Export ADIF…"), this);
    auto* csvBtn  = new QPushButton(QStringLiteral("Export CSV…"), this);
    for (QPushButton* b : {m_editBtn, m_deleteBtn, m_uploadBtn, mapBtn,
                           importBtn, adifBtn, csvBtn}) {
        b->setStyleSheet(Style::buttonBaseStyle());
        top->addWidget(b);
    }
    col->addLayout(top);
    connect(mapBtn, &QPushButton::clicked, this, &LogbookWindow::openMap);
    connect(importBtn, &QPushButton::clicked,
            this, &LogbookWindow::importAdif);

    connect(m_uploadBtn, &QPushButton::clicked, this, [this]() {
        if (m_uploaders.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("Upload"),
                QStringLiteral("No logging service is set up yet.\n\n"
                               "Add one under Tools."));
            return;
        }
        QMenu menu(this);
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
            uploadSelected(map.value(chosen));
        }
    });

    // Table
    m_table = new QTableWidget(0, ColumnCount, this);
    m_table->setHorizontalHeaderLabels(headerLabels());
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSortingEnabled(false);   // we own the order
    m_table->setStyleSheet(QStringLiteral(
        "QTableWidget { background: %1; alternate-background-color: %4;"
        "  color: %2; border: 1px solid %3; gridline-color: %3;"
        "  font-size: 11px; }"
        "QTableWidget::item:selected { background: %6; color: %2; }"
        "QHeaderView::section { background: %4; color: %5; border: none;"
        "  border-bottom: 1px solid %3; padding: 3px 6px; font-size: 10px; }"
    ).arg(QString::fromLatin1(Style::kInsetBg),
          QString::fromLatin1(Style::kTextPrimary),
          QString::fromLatin1(Style::kBorderSubtle),
          QString::fromLatin1(Style::kButtonBg),
          QString::fromLatin1(Style::kTextSecondary),
          QString::fromLatin1(Style::kAccent)));
    col->addWidget(m_table, 1);

    m_stats = new QLabel(QString{}, this);
    m_stats->setWordWrap(true);
    m_stats->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }")
        .arg(QString::fromLatin1(Style::kTextSecondary)));
    col->addWidget(m_stats);

    connect(m_search, &QLineEdit::textChanged, this, [this]() {
        applyFilter();
        refreshTable();
        updateStats();
    });
    connect(m_editBtn,   &QPushButton::clicked, this, &LogbookWindow::editSelected);
    connect(m_deleteBtn, &QPushButton::clicked, this, &LogbookWindow::deleteSelected);
    connect(adifBtn,     &QPushButton::clicked, this, &LogbookWindow::exportAdif);
    connect(csvBtn,      &QPushButton::clicked, this, &LogbookWindow::exportCsv);
    connect(m_table, &QTableWidget::itemDoubleClicked,
            this, [this](QTableWidgetItem*) { editSelected(); });
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
    applyFilter();
    refreshTable();
    updateStats();
}

void LogbookWindow::applyFilter()
{
    const QString needle = m_search->text().trimmed();
    m_visible.clear();
    m_visible.reserve(m_all.size());

    for (int i = 0; i < m_all.size(); ++i) {
        if (needle.isEmpty()) { m_visible.append(i); continue; }
        const LogEntry& e = m_all.at(i);
        // One box across every text field. An operator looking for
        // "Fort Salonga" should not first have to decide that it is a
        // QTH and not a comment.
        const bool hit =
               e.call.contains(needle, Qt::CaseInsensitive)
            || e.name.contains(needle, Qt::CaseInsensitive)
            || e.qth.contains(needle, Qt::CaseInsensitive)
            || e.country.contains(needle, Qt::CaseInsensitive)
            || e.gridSquare.contains(needle, Qt::CaseInsensitive)
            || e.band.contains(needle, Qt::CaseInsensitive)
            || e.mode.contains(needle, Qt::CaseInsensitive)
            || e.comment.contains(needle, Qt::CaseInsensitive);
        if (hit) { m_visible.append(i); }
    }
}

void LogbookWindow::refreshTable()
{
    m_table->setRowCount(m_visible.size());
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
        put(ColBand, e.band);
        put(ColMode, e.submode.isEmpty() ? e.mode : e.submode);
        put(ColSent, e.rstSent);
        put(ColRcvd, e.rstRcvd);
        put(ColName, e.name);
        put(ColQth, e.qth);
        put(ColCountry, e.country);
        put(ColGrid, e.gridSquare);
        put(ColDistance, e.distanceKm > 0.0
                ? QString::number(e.distanceKm, 'f', 0) : QString{});
        put(ColComment, e.comment);
    }
    m_table->resizeColumnsToContents();
}

void LogbookWindow::updateStats()
{
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
        connect(u, &QsoUploader::uploadFinished, this,
                [this](const QString& call, bool ok, bool duplicate,
                       const QString& message) {
            if (m_pending <= 0) { return; }
            --m_pending;
            if (ok) {
                ++m_okCount;
                if (duplicate) { ++m_dupCount; }
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
            const bool anyFailed = !m_failures.isEmpty();
            m_okCount = 0;
            m_dupCount = 0;
            m_failures.clear();
            m_uploadBtn->setEnabled(true);

            if (anyFailed) {
                QMessageBox::warning(this, QStringLiteral("Upload"), text);
            } else {
                QMessageBox::information(this, QStringLiteral("Upload"), text);
            }
        }, Qt::UniqueConnection);
    }
}

void LogbookWindow::uploadSelected(QsoUploader* target)
{
    if (!target) { return; }
    const QList<int> rows = selectedSourceRows();
    if (rows.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Upload"),
            QStringLiteral("Select the contacts to upload first."));
        return;
    }
    if (m_pending > 0) { return; }   // a batch is already running

    // Confirm the count. Sending 400 contacts because a stray Ctrl-A
    // selected the whole log is not recoverable at the far end.
    if (rows.size() > 1) {
        if (QMessageBox::question(this, QStringLiteral("Upload"),
                QStringLiteral("Send %1 contacts to %2?")
                    .arg(rows.size(), 0).arg(target->serviceName()),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
    }

    m_pending  = rows.size();
    m_okCount  = 0;
    m_dupCount = 0;
    m_failures.clear();
    m_uploadBtn->setEnabled(false);

    for (int idx : rows) { target->upload(m_all.at(idx)); }
}

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

// ── Correcting ──────────────────────────────────────────────────────

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
    when->setTimeZone(QTimeZone::UTC);

    auto* band    = new QLineEdit(e.band, &dlg);
    auto* mode    = new QLineEdit(e.mode, &dlg);
    auto* submode = new QLineEdit(e.submode, &dlg);
    auto* sent    = new QLineEdit(e.rstSent, &dlg);
    auto* rcvd    = new QLineEdit(e.rstRcvd, &dlg);
    auto* grid    = new QLineEdit(e.gridSquare, &dlg);
    auto* myGrid  = new QLineEdit(e.myGridSquare, &dlg);
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

void LogbookWindow::openMap()
{
    if (!m_map) { m_map = new QsoMapWindow(this); }
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

    QString err;
    const QVector<LogEntry> incoming = AdifLog::read(path, &err);
    if (!err.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Import"),
            QStringLiteral("Couldn't read that file:\n%1").arg(err));
        return;
    }
    if (incoming.isEmpty()) {
        // Distinguish "empty" from "not ADIF": a file the parser found
        // no records in is usually the wrong file, not an empty log.
        QMessageBox::warning(this, QStringLiteral("Import"),
            QStringLiteral("No contacts found in that file.\n\n"
                           "ADIF records are marked with <EOR>; if this "
                           "is a Cabrillo or CSV export it needs "
                           "converting first."));
        return;
    }

    const AdifLog::MergeResult r = AdifLog::merge(m_all, incoming);

    if (r.added == 0) {
        QMessageBox::information(this, QStringLiteral("Import"),
            QStringLiteral("All %1 contacts in that file are already in "
                           "your log. Nothing to do.").arg(incoming.size()));
        return;
    }

    const QString question =
        QStringLiteral("%1 contacts in the file.\n\n"
                       "%2 are new and will be added.\n"
                       "%3 are already in your log and will be skipped.\n\n"
                       "Your current log is copied to a dated backup "
                       "first. Go ahead?")
            .arg(incoming.size()).arg(r.added).arg(r.skipped);

    if (QMessageBox::question(this, QStringLiteral("Import"), question,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
        != QMessageBox::Yes) {
        return;
    }

    // Back up before touching anything. This is the one operation here
    // that rewrites many records at once, and unlike an edit or a delete
    // it cannot be put right from the table afterwards.
    QString backupErr;
    const QString backup = makeBackup(&backupErr);
    if (!backupErr.isEmpty()) {
        if (QMessageBox::warning(this, QStringLiteral("Import"),
                QStringLiteral("Couldn't make a backup first: %1\n\n"
                               "Import anyway?").arg(backupErr),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes) {
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
    if (!backup.isEmpty()) {
        done += QStringLiteral("\n\nBackup: %1").arg(backup);
    }
    QMessageBox::information(this, QStringLiteral("Import"), done);
}

// ── Export ──────────────────────────────────────────────────────────

void LogbookWindow::exportAdif()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export ADIF"),
        QStringLiteral("nereus-log.adi"),
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
        QStringLiteral("nereus-log.csv"),
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

} // namespace NereusSDR
