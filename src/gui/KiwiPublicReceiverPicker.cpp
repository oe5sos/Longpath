// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from AetherSDR src/gui/KiwiPublicReceiverPicker.cpp [@31b29583].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//
// Modification history (Longpath):
//   2026-08-23 — Portiert (Stufe 4). Begruendung der einen Abweichung
//                steht in der Kopfdatei.

#include "gui/KiwiPublicReceiverPicker.h"
#include "core/KiwiOwnSignal.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/KiwiWorldMap.h"
#include "core/AppSettings.h"
#include "core/Maidenhead.h"

#include <QAccessible>
#include <QDialogButtonBox>
#include <algorithm>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

namespace Longpath {

namespace {

// Session-scoped cache of the last successful directory fetch, shared across
// every picker instance opened this run.  Being a process-static, it is NOT
// persisted to disk — it dies with the app, so a new session always starts
// fresh.  This is what lets repeat "Browse public…" opens re-serve the list
// without hitting the operators' servers again; "Refresh list" overwrites it.
QVector<KiwiPublicReceiver> g_sessionCache;
bool g_haveSessionCache = false;

enum Column {
    ReceiverColumn,
    LocationColumn,
    DistanceColumn,
    UsersColumn,
    ApiColumn,
    LimitsColumn,
    ColumnCount,
};

// "http://host:port" -> "host:port" (what KiwiSdrClient::normalizeEndpoint wants).
QString endpointFromUrl(const QString& url)
{
    const QUrl u(url);
    QString ep = u.host();
    if (u.port() > 0)
        ep += QStringLiteral(":") + QString::number(u.port());
    return ep.isEmpty() ? url : ep;
}
} // namespace

KiwiPublicReceiverPicker::KiwiPublicReceiverPicker(QWidget* parent)
    : QDialog(parent)
    , m_dir(new KiwiPublicDirectory(this))
{
    resize(900, 720);

    setWindowTitle(tr("Public KiwiSDR receivers"));

    auto* outer = new QVBoxLayout(this);

    auto* topRow = new QHBoxLayout;
    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Filter by name, location, or host…"));
    m_search->setClearButtonEnabled(true);
    topRow->addWidget(m_search, 1);
    auto* mapToggle = new QPushButton(tr("Map"));
    mapToggle->setCheckable(true);
    mapToggle->setToolTip(tr("Show or hide the world map. Drag the bar between map and list to resize."));
    mapToggle->setChecked(AppSettings::instance().value("KiwiPicker/MapVisible", "True").toString() == "True");
    topRow->addWidget(mapToggle);
    m_refresh = new QPushButton(tr("Refresh list"));
    m_refresh->setToolTip(tr("Re-fetch the public directory from the network."));
    topRow->addWidget(m_refresh);
    outer->addLayout(topRow);

    m_table = new QTableWidget(0, ColumnCount, this);
    m_table->setHorizontalHeaderLabels(
        {tr("Receiver"), tr("Location"), tr("Distance"), tr("Users"),
         tr("API"), tr("Limits")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(ReceiverColumn, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(LocationColumn, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(DistanceColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(UsersColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ApiColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(LimitsColumn, QHeaderView::ResizeToContents);

    // Karte ueber der Tabelle (Entwurf A, 2026-11-25). Der Teiler laesst
    // dem Betreiber die Aufteilung; Karte und Tabelle zeigen dasselbe.
    m_map = new KiwiWorldMap(this);
    m_split = new QSplitter(Qt::Vertical, this);
    m_split->setChildrenCollapsible(false);
    m_split->addWidget(m_map);
    m_split->addWidget(m_table);
    m_split->setHandleWidth(9);
    m_split->setStyleSheet(QStringLiteral("QSplitter::handle { background: %1; }")
                               .arg(QLatin1String(Style::kBorderMuted)));
    m_map->setMinimumHeight(60);
    m_split->setStretchFactor(0, 3);
    m_split->setStretchFactor(1, 2);
    outer->addWidget(m_split, 1);
    m_map->setVisible(mapToggle->isChecked());
    connect(mapToggle, &QPushButton::toggled, this, [this](bool on) {
        m_map->setVisible(on);
        AppSettings::instance().setValue("KiwiPicker/MapVisible", on ? "True" : "False");
    });

    m_status = new QLabel(tr("Loading public receivers…"));
    m_status->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                .arg(QLatin1String(Style::kTextSecondary)));
    outer->addWidget(m_status);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    m_ok->setText(tr("Add selected"));
    m_ok->setEnabled(false);
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    outer->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &KiwiPublicReceiverPicker::acceptCurrentRow);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_table, &QTableWidget::itemDoubleClicked, this,
            [this](QTableWidgetItem*) { acceptCurrentRow(); });
    connect(m_table, &QTableWidget::itemSelectionChanged, this,
            [this] { m_ok->setEnabled(!m_table->selectedItems().isEmpty()); });
    connect(m_map, &KiwiWorldMap::pointClicked, this, [this](const QString& id) {
        for (int row = 0; row < m_table->rowCount(); ++row) {
            QTableWidgetItem* it = m_table->item(row, ReceiverColumn);
            if (it && it->data(Qt::UserRole).toString() == id) {
                m_table->selectRow(row);
                m_table->scrollToItem(it);
                return;
            }
        }
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        const int row = m_table->currentRow();
        QTableWidgetItem* it = row >= 0 ? m_table->item(row, ReceiverColumn) : nullptr;
        m_map->setSelectedId(it && m_table->item(row, ReceiverColumn)->isSelected()
                                 ? it->data(Qt::UserRole).toString() : QString());
    });
    connect(m_search, &QLineEdit::textChanged, this, &KiwiPublicReceiverPicker::applyFilter);
    connect(m_refresh, &QPushButton::clicked, this, &KiwiPublicReceiverPicker::startFetch);

    connect(m_dir, &KiwiPublicDirectory::ready, this, &KiwiPublicReceiverPicker::onReady);
    connect(m_dir, &KiwiPublicDirectory::failed, this, [this](const QString& err) {
        m_status->setText(tr("Could not load directory: %1").arg(err));
        m_refresh->setEnabled(true);
    });

    // Re-serve the session cache if we already fetched once this run; only the
    // first browse (or an explicit "Refresh list") touches the network.
    if (g_haveSessionCache) {
        m_fromCache = true;
        onReady(g_sessionCache);
    } else {
        startFetch();
    }
}

void KiwiPublicReceiverPicker::setPreselection(const QString& ownGrid,
                                               double frequencyHz)
{
    m_preselect = true;
    m_ownGrid = ownGrid.trimmed();
    m_frequencyHz = frequencyHz;
    applyFilter();
}

void KiwiPublicReceiverPicker::startFetch()
{
    m_fromCache = false;
    if (!m_fetchEnabled) { return; }
    m_status->setText(tr("Loading public receivers…"));
    m_refresh->setEnabled(false);
    m_dir->fetch();
}

void KiwiPublicReceiverPicker::onReady(const QVector<KiwiPublicReceiver>& receivers)
{
    // Populate (or overwrite) the session cache from every successful fetch.
    // Serving from the cache passes the same vector straight back through here,
    // which is a harmless no-op assignment.
    g_sessionCache = receivers;
    g_haveSessionCache = true;

    m_refresh->setEnabled(true);
    m_apiReceivers.clear();
    m_hiddenWebOnly = 0;
    m_hiddenUnknown = 0;
    m_offlineReceivers.clear();
    for (const auto& r : receivers) {
        if (r.offline) {
            // Nur fuer die Karte: wer seinen Empfaenger nur ueber die eigene
            // Seite anbietet, bleibt auch dort unsichtbar (wie in der Liste).
            if (r.apiPolicy() != KiwiPublicReceiver::ApiPolicy::Disabled) {
                m_offlineReceivers.push_back(r);
            }
            continue;
        }
        // Honor the operator: only receivers that allow the external API are
        // listed. Web-only (ext_api == 0) are excluded entirely, as are
        // receivers that don't publish a policy (we can't confirm API is OK).
        if (!r.mayConnectViaApi()) {
            if (r.apiPolicy() == KiwiPublicReceiver::ApiPolicy::Disabled)
                ++m_hiddenWebOnly;
            else
                ++m_hiddenUnknown;
            continue;
        }
        m_apiReceivers.push_back(r);
    }
    applyFilter();
}

void KiwiPublicReceiverPicker::applyFilter()
{
    const QString needle = m_search->text().trimmed();

    // Vorauswahl (2026-11-24, "Mein Signal am Kiwi"): nach Entfernung zum
    // eigenen Locator geordnet, nur was die Frequenz abdeckt und nicht voll
    // ist. Ohne Vorauswahl bleibt es bei der Reihenfolge des Verzeichnisses.
    QVector<KiwiCandidate> ordered;
    if (m_preselect) {
        ordered = kiwiPreselect(m_apiReceivers, m_ownGrid, m_frequencyHz);
    } else {
        for (const auto& r : m_apiReceivers) {
            KiwiCandidate c;
            c.receiver = r;
            ordered.append(c);
        }
    }
    QVector<const KiwiCandidate*> matches;
    matches.reserve(ordered.size());
    for (const KiwiCandidate& c : ordered) {
        const KiwiPublicReceiver& r = c.receiver;
        if (!needle.isEmpty()
            && !r.name.contains(needle, Qt::CaseInsensitive)
            && !r.location.contains(needle, Qt::CaseInsensitive)
            && !r.url.contains(needle, Qt::CaseInsensitive)) {
            continue;
        }
        matches.append(&c);
    }
    // Wo Longpath wohl keinen Platz bekommt, ans Ende; wer schon einmal
    // getrennt hat, ganz nach hinten. Innerhalb gleicher Stufe bleibt die
    // Reihenfolge (Entfernung).
    std::stable_sort(matches.begin(), matches.end(),
                     [this](const KiwiCandidate* a, const KiwiCandidate* b) {
        return placeRank(a->receiver) < placeRank(b->receiver);
    });

    // Bug fix 2026-09-07: this used to insertRow()+setItem() one row at a
    // time, re-running on EVERY keystroke in the search box. Each
    // insertRow()/setItem() fires its own model signal, and Qt's macOS
    // accessibility bridge rebuilds the WHOLE table's accessible element
    // array from scratch on every one of those whenever an accessibility
    // observer (VoiceOver, remote-control software, or a11y-based
    // automation) is attached -- confirmed to crash Longpath the same
    // way for LogbookWindow::refreshTable() (see that fix, same date,
    // for the full writeup and Qt-source citations). The public
    // directory realistically runs several hundred entries, so this was
    // the same O((rows x cols)^2) shape, at a worse retrigger rate.
    // setRowCount() stays outside the blocked scope so the view's own
    // geometry/scrollbars stay correct; only the per-cell population is
    // batched into one accessibility notification afterward.
    m_table->setRowCount(matches.size());
    {
        const QSignalBlocker modelBlocker(m_table->model());
        for (int row = 0; row < matches.size(); ++row) {
            const KiwiPublicReceiver& r = matches.at(row)->receiver;
            const double km = matches.at(row)->km;

            auto* nameItem = new QTableWidgetItem(r.name.isEmpty() ? r.url : r.name);
            nameItem->setToolTip(r.url);
            // Carry the endpoint + a suggested name on the row's first item.
            nameItem->setData(Qt::UserRole, endpointFromUrl(r.url));
            nameItem->setData(Qt::UserRole + 1,
                              QUrl(r.url).host().left(16));  // short default name
            m_table->setItem(row, ReceiverColumn, nameItem);
            nameItem->setData(Qt::UserRole + 2, km);
            {
                double lat = 0.0, lon = 0.0;
                const bool has = kiwiReceiverLatLon(r, lat, lon);
                nameItem->setData(Qt::UserRole + 3, lat);
                nameItem->setData(Qt::UserRole + 4, lon);
                nameItem->setData(Qt::UserRole + 5, has);
            }
            m_table->setItem(row, LocationColumn, new QTableWidgetItem(r.location));
            m_table->setItem(row, DistanceColumn, new QTableWidgetItem(
                km >= 0.0 ? QStringLiteral("%1 km").arg(qRound(km))
                          : QString()));
            m_table->setItem(row, UsersColumn, new QTableWidgetItem(
                QStringLiteral("%1/%2").arg(r.users).arg(r.usersMax)));
            auto* apiItem = new QTableWidgetItem(r.apiBadge());
            const int rank = placeRank(r);
            if (rank == 2) {
                apiItem->setText(tr("%1 · disconnected Longpath").arg(r.apiBadge()));
                apiItem->setToolTip(tr("This Kiwi disconnected Longpath shortly after "
                                       "connecting: its places for programs were full."));
            } else if (rank == 1) {
                apiItem->setText(tr("%1 · probably full").arg(r.apiBadge()));
                apiItem->setToolTip(tr("As many users as the operator allows programs: "
                                       "this Kiwi will probably disconnect Longpath "
                                       "after a few seconds."));
            }
            if (rank > 0) {
                apiItem->setForeground(QColor(QLatin1String(Style::kAmberWarn)));
            }
            m_table->setItem(row, ApiColumn, apiItem);

            auto* limitsItem = new QTableWidgetItem(r.connectionLimitBadge());
            if (r.advertisesConnectionLimit()) {
                limitsItem->setToolTip(tr("This receiver advertises connection limits, "
                                          "but the public directory does not publish "
                                          "the configured duration."));
            } else {
                limitsItem->setToolTip(tr("No connection limit is advertised in the "
                                          "public directory."));
            }
            m_table->setItem(row, LimitsColumn, limitsItem);
        }
    }
    m_table->viewport()->update();
    rebuildMap(matches);
    {
        QAccessibleTableModelChangeEvent tableEvent(
            m_table, QAccessibleTableModelChangeEvent::DataChanged);
        tableEvent.setFirstRow(0);
        tableEvent.setLastRow(qMax(0, m_table->rowCount() - 1));
        tableEvent.setFirstColumn(0);
        tableEvent.setLastColumn(ColumnCount - 1);
        QAccessible::updateAccessibility(&tableEvent);
    }

    const int shown = matches.size();
    QString status = tr("%1 receivers allow API access").arg(shown);
    QStringList hiddenParts;
    if (m_hiddenWebOnly > 0)
        hiddenParts << tr("%1 web-only").arg(m_hiddenWebOnly);
    if (m_hiddenUnknown > 0)
        hiddenParts << tr("%1 policy-unknown").arg(m_hiddenUnknown);
    if (!hiddenParts.isEmpty())
        status += tr(" (%1 hidden)").arg(hiddenParts.join(QStringLiteral(", ")));
    if (m_preselect) {
        const QString grid = m_ownGrid.isEmpty() ? tr("(locator missing)") : m_ownGrid;
        // Ohne Frequenz (kein Funkgeraet) nicht "passend zu 0.000 MHz".
        status += m_frequencyHz > 0.0
            ? tr("  ·  sorted by distance from %1, fitting %2 MHz")
                  .arg(grid)
                  .arg(m_frequencyHz / 1.0e6, 0, 'f', 3)
            : tr("  ·  sorted by distance from %1").arg(grid);
    }
    if (m_fromCache)
        status += tr("  ·  cached — use “Refresh list” to update");
    m_status->setText(status);
    m_ok->setEnabled(!m_table->selectedItems().isEmpty());
}

void KiwiPublicReceiverPicker::setOpenEndpoints(const QSet<QString>& endpoints)
{
    m_openEndpoints = endpoints;
    applyFilter();
}

void KiwiPublicReceiverPicker::showReceiversForTest(
    const QVector<KiwiPublicReceiver>& receivers)
{
    m_fetchEnabled = false;
    onReady(receivers);
}

int KiwiPublicReceiverPicker::placeRank(const KiwiPublicReceiver& r) const
{
    if (m_evicting.contains(endpointFromUrl(r.url))) { return 2; }
    return r.programPlacesProbablyFull() ? 1 : 0;
}

void KiwiPublicReceiverPicker::rebuildMap(
    const QVector<const KiwiCandidate*>& matches)
{
    QVector<KiwiMapPoint> points;
    const auto make = [this](const KiwiPublicReceiver& r, double km,
                             KiwiMapPoint::State forced, bool useForced) {
        KiwiMapPoint pt;
        if (!kiwiReceiverLatLon(r, pt.lat, pt.lon)) { return KiwiMapPoint(); }
        pt.id = endpointFromUrl(r.url);
        pt.name = r.name.isEmpty() ? r.url : r.name;
        if (useForced) {
            pt.state = forced;
        } else if (placeRank(r) > 0
                   || (r.usersMax > 0 && r.users >= r.usersMax)) {
            pt.state = KiwiMapPoint::State::Full;
        } else if (r.users > 0) {
            pt.state = KiwiMapPoint::State::Partial;
        } else {
            pt.state = KiwiMapPoint::State::Free;
        }
        pt.open = m_openEndpoints.contains(pt.id);
        const QString stateText = pt.state == KiwiMapPoint::State::Offline
            ? tr("not reachable")
            : pt.state == KiwiMapPoint::State::Full ? tr("full")
            : pt.state == KiwiMapPoint::State::Partial ? tr("partly busy")
            : tr("free");
        QStringList lines;
        lines << QStringLiteral("%1 — %2").arg(pt.name, stateText);
        if (!r.location.isEmpty()) { lines << r.location; }
        if (km >= 0.0) { lines << tr("Distance: %1 km").arg(qRound(km)); }
        if (r.usersMax > 0) {
            lines << tr("Free channels: %1 of %2")
                         .arg(qMax(0, r.usersMax - r.users)).arg(r.usersMax);
        }
        if (!r.bands.isEmpty()) { lines << tr("Band: %1 Hz").arg(r.bands); }
        if (!r.snr.isEmpty()) { lines << tr("SNR: %1").arg(r.snr); }
        pt.detail = lines.join(QLatin1Char('\n'));
        return pt;
    };
    for (const KiwiCandidate* c : matches) {
        const KiwiMapPoint pt = make(c->receiver, c->km, {}, false);
        if (!pt.id.isEmpty()) { points.append(pt); }
    }
    for (const KiwiPublicReceiver& r : m_offlineReceivers) {
        const KiwiMapPoint pt = make(
            r, kiwiReceiverDistanceKm(r, m_ownGrid), KiwiMapPoint::State::Offline, true);
        if (!pt.id.isEmpty()) { points.append(pt); }
    }
    m_map->setPoints(points);
    double lat = 0.0, lon = 0.0;
    const QString grid = m_ownGrid.isEmpty() ? AppSettings::stationGridSquare() : m_ownGrid;
    if (isValidGridSquare(grid)) {
        calculateLatLonFromGridSquare(grid, lat, lon);
        m_map->setHome(lat, lon, true, grid.left(6).toUpper());
    } else {
        m_map->setHome(0, 0, false, QString());
    }
}

void KiwiPublicReceiverPicker::acceptCurrentRow()
{
    const int row = m_table->currentRow();
    if (row < 0) return;
    QTableWidgetItem* item = m_table->item(row, ReceiverColumn);
    if (!item) return;
    m_selectedEndpoint = item->data(Qt::UserRole).toString();
    m_selectedName = item->data(Qt::UserRole + 1).toString();
    m_selectedKm = item->data(Qt::UserRole + 2).toDouble();
    m_selectedLat = item->data(Qt::UserRole + 3).toDouble();
    m_selectedLon = item->data(Qt::UserRole + 4).toDouble();
    m_selectedHasPos = item->data(Qt::UserRole + 5).toBool();
    accept();
}

} // namespace Longpath
