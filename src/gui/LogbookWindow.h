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
// =================================================================

#include "models/LogEntry.h"

#include <QDialog>
#include <QList>
#include <QVector>

#include <functional>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace NereusSDR {

class QsoUploader;

class LogbookWindow : public QDialog {
    Q_OBJECT
public:
    explicit LogbookWindow(const QString& adifPath, QWidget* parent = nullptr);

    // Re-read the file. Called when the panel logs a contact so the two
    // views cannot drift apart.
    void reload();

    // Places selected contacts can be sent. Not owned — the window is
    // one of several users of the same uploaders, and whoever holds the
    // credentials holds the objects.
    void setUploaders(const QVector<QsoUploader*>& uploaders);

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
    void applyFilter();
    void refreshTable();
    void updateStats();
    void editSelected();
    void deleteSelected();
    void exportAdif();
    void exportCsv();
    void importAdif();
    void uploadSelected(QsoUploader* target);
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
    QTableWidget* m_table{nullptr};
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
};

} // namespace NereusSDR
