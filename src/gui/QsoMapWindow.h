#pragma once

// =================================================================
// src/gui/QsoMapWindow.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Where the contacts in a period actually were. Two views of the same
// data because they answer different questions: the globe shows the
// paths as the signals took them, the flat map shows all of them at
// once. Half the Earth is always facing away from a sphere, and "where
// have I worked this year" is a question about all of it.
//
// Only contacts with a usable locator can be placed. The window says
// how many were left out rather than quietly drawing fewer dots than
// the log has records — a map that silently omits a third of the log
// is worse than no map.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "models/LogEntry.h"

#include <QDialog>
#include <QVector>

class QCheckBox;
class QDateEdit;
class QLabel;
class QPushButton;
class QStackedWidget;

namespace NereusSDR {

class FlatMapWidget;
class GlobeWidget;

class QsoMapWindow : public QDialog {
    Q_OBJECT
public:
    explicit QsoMapWindow(QWidget* parent = nullptr);

    // The contacts to draw, and where the operator is. Both may be
    // replaced whenever the log changes.
    void setEntries(const QVector<LogEntry>& entries);
    void setHomeGrid(const QString& grid);

private:
    void buildUi();
    void applyRange();
    void setQuickRange(int days);   // 0 means everything

    QVector<LogEntry> m_all;
    QString m_homeGrid;

    QStackedWidget* m_stack{nullptr};
    GlobeWidget*    m_globe{nullptr};
    FlatMapWidget*  m_flat{nullptr};

    QDateEdit*   m_from{nullptr};
    QDateEdit*   m_to{nullptr};
    QCheckBox*   m_paths{nullptr};
    QLabel*      m_summary{nullptr};
    QPushButton* m_viewBtn{nullptr};
};

} // namespace NereusSDR
