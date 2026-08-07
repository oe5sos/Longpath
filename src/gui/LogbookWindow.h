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
#include <QVector>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace NereusSDR {

class LogbookWindow : public QDialog {
    Q_OBJECT
public:
    explicit LogbookWindow(const QString& adifPath, QWidget* parent = nullptr);

    // Re-read the file. Called when the panel logs a contact so the two
    // views cannot drift apart.
    void reload();

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

    // Index into m_all for the given visible row, or -1.
    int sourceRow(int viewRow) const;
    bool saveAll();

    QString m_path;

    QVector<LogEntry> m_all;      // everything in the file
    QVector<int>      m_visible;  // indices into m_all, after filtering

    QLineEdit*    m_search{nullptr};
    QTableWidget* m_table{nullptr};
    QLabel*       m_stats{nullptr};
    QPushButton*  m_editBtn{nullptr};
    QPushButton*  m_deleteBtn{nullptr};
};

} // namespace NereusSDR
