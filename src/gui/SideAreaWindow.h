#pragma once

// =================================================================
// src/gui/SideAreaWindow.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Der Seitenbereich ────────────────────────────────────────────────
//
// Betreiber 2026-09-25: „vielleicht könnte ich im bereich des rotors ein
// eigenes menu erstellen, sodass ich hier mehrere windows haben und das
// aktive immer aufgeht … wenn ich diesen bereich nicht immer brauche und
// öfters tauschen möchte". Aus drei Entwurfsblättern gewählt: Variante 2,
// „Symbolleiste am Rand".
//
// Ein schwebendes Fenster (Qt::Tool, wie die anderen schwebenden Fenster)
// mit einer schmalen Leiste am rechten Rand: ein Symbol je Seite. Klick
// auf ein Symbol zeigt diese Seite; Klick auf das schon aktive klappt den
// Bereich zu — dann bleibt nur die Leiste stehen, am selben rechten Rand.
// „+" unten in der Leiste bittet den Besitzer, eine Seite dazuzulegen;
// Rechtsklick auf ein Symbol bittet ihn, sie herauszunehmen.
//
// Das Fenster BESITZT seine Seiten nur, solange sie darin stehen: takePage()
// gibt sie unbeschädigt heraus (Elternteil nullptr), genau wie
// AppletFloatingWindow::releaseApplet(). Was eine Seite ist (Rotor/Log,
// ein Applet) und wohin sie zurückgeht, weiß nur MainWindow.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-25 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QHash>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QWidget>

class QScrollArea;
class QStackedWidget;
class QTimer;
class QVBoxLayout;

namespace Longpath {

class WindowTitleBar;
class SideRailButton;

class SideAreaWindow : public QWidget {
    Q_OBJECT
public:
    explicit SideAreaWindow(QWidget* parent = nullptr);
    ~SideAreaWindow() override;

    // ── Seiten ───────────────────────────────────────────────────────

    /// Legt `content` als Seite `id` unten an die Leiste an. Der Inhalt
    /// kommt in einen Rollbereich (wie in AppletFloatingWindow: sonst
    /// bestimmt seine Mindestgröße die des Fensters). Schon vorhanden:
    /// tut nichts. Die erste Seite wird aktiv.
    void addPage(const QString& id, const QString& title, QWidget* content);
    /// Gibt den Inhalt heraus, unbeschädigt und ohne Elternteil. Die
    /// Seite verschwindet aus der Leiste; war sie aktiv, wird die
    /// nächste aktiv. nullptr, wenn es sie nicht gibt.
    QWidget* takePage(const QString& id);
    bool hasPage(const QString& id) const { return m_pages.contains(id); }
    QWidget* pageContent(const QString& id) const;
    QStringList pageIds() const { return m_order; }

    QString activeId() const { return m_active; }
    /// Zeigt die Seite. Klappt einen zugeklappten Bereich auf.
    void setActive(const QString& id);

    // ── Zuklappen ────────────────────────────────────────────────────

    bool isCollapsed() const { return m_collapsed; }
    /// Zu: nur die Leiste bleibt, der rechte Rand bleibt stehen. Auf:
    /// wieder die gemerkte Breite, nach links hin.
    void setCollapsed(bool on);
    /// Die Breite im aufgeklappten Zustand (auch während zugeklappt).
    int expandedWidth() const;
    void setExpandedWidth(int w);

    /// Der Klick auf ein Leistensymbol, wie ihn der Bediener macht:
    /// inaktiv → zeigen, aktiv → zu- bzw. aufklappen.
    void railClicked(const QString& id);

    /// Lage, Seiten, aktive Seite, zugeklappt — für das Layout-Profil.
    /// Die Geometrie ist immer die AUFGEKLAPPTE.
    QVariantMap captureState() const;

    static constexpr int kRailW = 38;

    // ── Für Tests ────────────────────────────────────────────────────
    QStringList railIdsForTest() const;
    QString titleForTest() const;

signals:
    void activeChanged(const QString& id);
    /// `delta` = Breite, die frei wurde (zu) bzw. wieder gebraucht wird
    /// (auf). Der Besitzer kann Nachbarn um genau so viel mitziehen.
    void collapsedChanged(bool collapsed, int delta);
    /// „+" in der Leiste; `globalPos` = wo ein Menü aufgehen soll.
    void addRequested(const QPoint& globalPos);
    /// Rechtsklick → „Aus dem Seitenbereich nehmen".
    void removeRequested(const QString& id);
    /// Lage/Größe stehen still (Ende der Geste), oder Seiten/Zustand
    /// haben sich geändert — Zeit, ins Profil zu schreiben.
    void stateSettled();

protected:
    void paintEvent(QPaintEvent* ev) override;
    void moveEvent(QMoveEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;
    void closeEvent(QCloseEvent* ev) override;

private:
    struct Page {
        QString title;
        QPointer<QWidget> content;
        QScrollArea* scroll{nullptr};
        SideRailButton* button{nullptr};
    };

    void refreshRail();
    void scheduleSettle();

    QWidget*        m_body{nullptr};
    WindowTitleBar* m_titleBar{nullptr};
    QStackedWidget* m_stack{nullptr};
    QWidget*        m_rail{nullptr};
    QVBoxLayout*    m_railLayout{nullptr};
    SideRailButton* m_addButton{nullptr};
    QTimer*         m_settleTimer{nullptr};

    QHash<QString, Page> m_pages;
    QStringList m_order;
    QString m_active;
    bool m_collapsed{false};
    int  m_expandedWidth{0};
    bool m_applyingCollapse{false};

    static constexpr int kSettleMs = 400;
};

} // namespace Longpath
