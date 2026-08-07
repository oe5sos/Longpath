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

    // Marked contacts each get their own line, up to this many.
    //
    // A cap rather than a promise to draw everything: the arcs are
    // slerped per repaint, and past a few hundred the globe stops
    // turning smoothly — which is precisely when a large selection is
    // being looked at. Contacts beyond the limit are counted in the
    // summary rather than silently dropped.
    static constexpr int kMaxMarkedPaths = 500;

    // The contacts to draw, and where the operator is. Both may be
    // replaced whenever the log changes.
    void setEntries(const QVector<LogEntry>& entries);
    void setHomeGrid(const QString& grid);

    // Rows picked out in the log's table. Passing a non-empty selection
    // switches the window to showing only those; the operator can flip
    // back to the date range, where they stay picked out in colour
    // rather than being the only thing drawn.
    void setSelection(const QVector<LogEntry>& selected);

private:
    void buildUi();
    void rebuild();
    void setQuickRange(int days);   // 0 means everything

    QVector<LogEntry> m_all;
    QVector<LogEntry> m_selected;
    QString m_homeGrid;

    QStackedWidget* m_stack{nullptr};
    GlobeWidget*    m_globe{nullptr};
    FlatMapWidget*  m_flat{nullptr};

    QDateEdit*   m_from{nullptr};
    QDateEdit*   m_to{nullptr};
    QCheckBox*   m_paths{nullptr};
    QCheckBox*   m_onlySelected{nullptr};
    QLabel*      m_summary{nullptr};
    QPushButton* m_viewBtn{nullptr};
    // The date controls and the quick-range buttons, greyed together
    // while only the selection is shown — a live date field that
    // changes nothing is worse than a disabled one.
    QVector<QWidget*> m_rangeControls;
};

} // namespace NereusSDR
