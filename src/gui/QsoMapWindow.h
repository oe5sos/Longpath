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
//   2026-08-10 — Band and mode filter pills, a station card for a
//                 clicked marker, the Maidenhead grid overlay switch,
//                 and export of the filtered view to Google Earth
//                 (KML). The pills are rebuilt from whatever bands and
//                 modes the log actually contains — a fixed list would
//                 offer 60 m to an operator who has never used it and
//                 omit whatever oddity they have. AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "models/LogEntry.h"

#include <QDialog>
#include <QSet>
#include <QVector>

#include <functional>

class QCheckBox;
class QDateEdit;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QStackedWidget;

class QComboBox;
class QEvent;

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

    /// Rufzeichen und Portrait auf den Kartenmarker legen. Aus den
    /// Einstellungen und den beiden Caches, ohne Netzwerk — was schon
    /// geholt wurde, liegt auf der Platte; was nicht, bleibt ein Punkt.
    void applyStationMarker();

    // Rows picked out in the log's table. Passing a non-empty selection
    // switches the window to showing only those; the operator can flip
    // back to the date range, where they stay picked out in colour
    // rather than being the only thing drawn.
    void setSelection(const QVector<LogEntry>& selected);

    // Last resort for a contact with no locator: the centre of its DXCC
    // entity. Injected because cty.dat lives with the radio model and
    // this window is opened from the logbook, which has no radio.
    //
    // Without it a log whose contacts carry no GRIDSQUARE — which is
    // most logs imported from elsewhere, and every QSO logged before a
    // QRZ lookup filled the field — draws an empty map.
    using PositionFallback =
        std::function<bool(const QString& call, double& lat, double& lon)>;
    void setPositionFallback(PositionFallback fn);

protected:
    void closeEvent(QCloseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    void buildUi();
    void rebuild();
    void setQuickRange(int days);   // 0 means everything
    // Zoom whichever of the two views is on screen.
    void zoomActiveView(double factor);
    void resetActiveView();

    // Pills for the bands and modes the log actually contains,
    // recreated whenever the entries change.
    void rebuildFilterPills();
    // The filtered view, as a KML file, opened in Google Earth.
    void exportKml();
    // Everything the log knows about a clicked marker's station.
    void showStationInfo(const QString& label);
    // How many contacts sit in a clicked grid square, and who.
    void showGridInfo(const QString& locator);

    // Filter keys: band lower-cased, mode upper-cased, blanks as "?".
    static QString bandKey(const LogEntry& e);
    static QString modeKey(const LogEntry& e);

    QVector<LogEntry> m_all;
    QVector<LogEntry> m_selected;
    QString m_homeGrid;
    PositionFallback m_fallback;

    QStackedWidget* m_stack{nullptr};
    GlobeWidget*    m_globe{nullptr};
    FlatMapWidget*  m_flat{nullptr};

    // ── Wahl des Kartenhintergrunds ─────────────────────────────────
    //
    // Der Ordner wird beim OEFFNEN der Liste gelesen, nicht beim Start:
    // wer eine Datei ablegt, waehrend das Programm laeuft, soll sie ohne
    // Neustart sehen. Dafuer der Ereignisfilter auf der Liste selbst --
    // showPopup() ist kein Signal, an das man sich haengen koennte.
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QComboBox* m_background{nullptr};
    void reloadBackgroundList();
    void applyBackgroundChoice(int index);

    QDateEdit*   m_from{nullptr};
    QDateEdit*   m_to{nullptr};
    QCheckBox*   m_paths{nullptr};
    QCheckBox*   m_grid{nullptr};
    QCheckBox*   m_onlySelected{nullptr};
    QLabel*      m_summary{nullptr};
    QLabel*      m_info{nullptr};        // station card / grid answer
    QPushButton* m_viewBtn{nullptr};

    // Filter pills and their state. An empty active set means "no
    // filtering" (everything shown), so a fresh window hides nothing.
    QHBoxLayout*  m_pillRow{nullptr};
    QSet<QString> m_offBands;    // pills the operator has turned OFF
    QSet<QString> m_offModes;

    // What the current filters left visible — the set the KML export
    // sends, so Google Earth shows exactly what the window shows.
    QVector<LogEntry> m_lastShown;
    // The date controls and the quick-range buttons, greyed together
    // while only the selection is shown — a live date field that
    // changes nothing is worse than a disabled one.
    QVector<QWidget*> m_rangeControls;
};

} // namespace NereusSDR
