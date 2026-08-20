#pragma once

// =================================================================
// src/gui/LogbookWindow.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Thetis has no logbook at all.
//
// The dock panel shows the last twelve contacts, which is enough to
// confirm a write went through and nothing more. This is the log you
// actually go looking in: search it, fix the callsign you fat-fingered,
// export it, and see what you have worked.
//
// It reads the ADIF file rather than holding a model in memory. The
// file is the log; anything cached beside it is a second version of the
// truth waiting to disagree.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-08-10 — Statistics view (per band / mode / year, unique calls
//                 and squares, furthest DX), Cabrillo skeleton export,
//                 and the Band column coloured by band so a mixed log
//                 can be scanned. AI-assisted via Anthropic Claude
//                 (Cowork), operator Martin Fischer.
// =================================================================

#include "core/CallsignCache.h"
#include "core/LogFilter.h"
#include "models/LogEntry.h"

#include <QDialog>
#include <QList>
#include <QVector>

#include <functional>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QSplitter;
class QTableWidget;
class QVBoxLayout;

namespace Longpath {

class QrzClient;
class QsoDetailPane;
class QsoUploader;

class LogbookWindow : public QDialog {
    Q_OBJECT
public:
    explicit LogbookWindow(const QString& adifPath, QWidget* parent = nullptr);

    // Re-read the file. Called when the panel logs a contact so the two
    // views cannot drift apart.
    void reload();

private:
    void showRowMenu(const QPoint& pos);

public:

    // Places selected contacts can be sent. Not owned — the window is
    // one of several users of the same uploaders, and whoever holds the
    // credentials holds the objects.
    void setUploaders(const QVector<QsoUploader*>& uploaders);

private slots:
    /// Eine echte Slot-Methode, damit Qt::UniqueConnection greift --
    /// bei einem Lambda tut die Fahne schweigend nichts.
    void onUploadFinished(const QString& call, bool ok, bool duplicate,
                          const QString& message);

public:

    // Lets the detail pane fetch a name and a portrait for the selected
    // contact. Not owned, and optional: the logbook is opened on
    // machines with no QRZ account, where the pane simply says so.
    void setQrzClient(QrzClient* qrz);

signals:
    // ── Point the beam at a contact ──────────────────────────────────
    //
    // Every entry already carries the bearing to it, and the rotor
    // panel that owns this window can already turn. The two have never
    // been connected, so an operator wanting to work a station again
    // read the bearing off the screen and typed it into the dial.
    //
    // A signal rather than a direct call because this window must not
    // know about a rotor: it is opened on machines that have none, and
    // a logbook that needs a rotor to compile is a logbook with a
    // rotor-shaped hole in it.
    void turnRotorRequested(double bearingDeg, const QString& call);

public:

    // Passed straight to the map: the centre of a contact's DXCC entity,
    // for contacts that carry no locator. cty.dat lives with the radio
    // model, which this window has no handle on.
    using PositionFallback =
        std::function<bool(const QString& call, double& lat, double& lon)>;
    void setPositionFallback(PositionFallback fn);

signals:
    // The file changed underneath other views (edit or delete).
    void logChanged();

private:
    void buildUi();
    void buildFilterBar(QVBoxLayout* col);
    // Refill the band and mode lists from what is actually in the log,
    // keeping the current choice if it is still there. Offering every
    // band in existence would make most of the list a way to get no
    // results.
    void refreshFilterChoices();
    LogFilter currentFilter() const;
    void applyFilter();
    // Order m_visible by the current sort column. Done here rather than
    // by QTableWidget's own sorting, which reorders rows underneath the
    // index mapping and would make every row action touch the wrong
    // contact.
    void applySort();
    void saveHeaderState();
    void restoreHeaderState();
    void restoreSplitState();
    void refreshTable();
    void updateStats();
    void editSelected();
    void deleteSelected();
    void exportAdif();
    void exportCsv();
    // Cabrillo 3.0 skeleton (2026-08-10): QSO lines from the filtered
    // view, exchange column filled with the grid where one is known.
    // Contests differ in what they exchange, so the file is a starting
    // point to edit, and the dialog says so.
    void exportCabrillo();
    void importAdif();
    // Numbers about the filtered view: per band, per mode, per year,
    // unique calls and squares, furthest DX. (2026-08-10)
    void showStatistics();
    // Send these records. With rows marked in the table it is those;
    // with none marked it is everything not yet uploaded, which is the
    // question an operator actually has after a session.
    void uploadEntries(const QList<int>& sourceRows, QsoUploader* target);
    QList<int> outstandingRows() const;
    void openMap();

    // Timestamped copy of the log file beside itself. Taken before an
    // import, which is the one operation here that touches many records
    // at once and cannot be undone from the table.
    QString makeBackup(QString* error) const;

    // Index into m_all for the given visible row, or -1.
    int sourceRow(int viewRow) const;
    // Source indices of every selected row, in display order.
    QList<int> selectedSourceRows() const;
    bool saveAll();

    QString m_path;

    QVector<LogEntry> m_all;      // everything in the file
    QVector<int>      m_visible;  // indices into m_all, after filtering

    QLineEdit*    m_search{nullptr};
    QComboBox*    m_bandBox{nullptr};
    QComboBox*    m_modeBox{nullptr};
    QLineEdit*    m_gridEdit{nullptr};
    QLineEdit*    m_countryEdit{nullptr};
    QCheckBox*    m_unconfirmedOnly{nullptr};
    QCheckBox*    m_useDates{nullptr};
    QDateEdit*    m_fromDate{nullptr};
    QDateEdit*    m_toDate{nullptr};
    QPushButton*  m_clearBtn{nullptr};

    QTableWidget*  m_table{nullptr};
    QSplitter*     m_split{nullptr};
    QsoDetailPane* m_detail{nullptr};

    // What QRZ said about the callsigns in this log, kept between runs.
    // Owned here rather than shared with the rotor panel: the window
    // outlives a single lookup and a log of two thousand contacts is
    // the only place the persistence actually earns its keep.
    CallsignCache m_callCache;

    // Which column the table is ordered by. Newest first is the useful
    // default: the contact you want is nearly always the last one.
    int m_sortColumn{0};
    Qt::SortOrder m_sortOrder{Qt::DescendingOrder};
    // Set once the operator has resized or hidden a column, so the
    // automatic fit-to-contents stops fighting their choices.
    bool m_headerRestored{false};
    QLabel*       m_stats{nullptr};
    QPushButton*  m_editBtn{nullptr};
    QPushButton*  m_deleteBtn{nullptr};
    QPushButton*  m_uploadBtn{nullptr};

    QVector<QsoUploader*> m_uploaders;

    // One map window, reused, so a second click raises the existing one
    // instead of stacking copies of the same picture.
    class QsoMapWindow* m_map{nullptr};
    PositionFallback m_fallback;

    // Outstanding uploads for the current batch, so the summary can be
    // reported once instead of one message box per contact.
    int m_pending{0};
    int m_okCount{0};
    int m_dupCount{0};
    QStringList m_failures;
    // Source indices in the batch being uploaded, so a success can be
    // written back against the right record instead of against every
    // past contact with the same station.
    QList<int> m_uploadBatch;
};

} // namespace Longpath
