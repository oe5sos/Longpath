#pragma once

// =================================================================
// src/gui/applets/GridCellWidget.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// ── Das Feld auf dem Schirm ──────────────────────────────────────────
//
// Ein Behaelter mit Kopfleiste und N Inhalten. Die Anordnung dazu steht
// in GridCell; dieses Widget ist ihre sichtbare Seite.
//
//   Kopfleiste:  ⣿  TITEL                          [Werkzeuge]
//   Inhalt:      Applet 1
//                Applet 2
//                …
//
// Die Kopfleiste ist der GRIFF — nur sie, nicht der Inhalt darunter.
// Sonst zoege jeder Regler das ganze Feld mit. Das war schon bei
// AppletPanelWidget::wrapWithTitleBar so und bleibt so.
//
// ── Warum die Kopfleiste zum Feld gehoert und nicht zum Applet ───────
//
// Weil ein Feld mehrere Applets tragen kann. Zwei Applets mit je einer
// Kopfleiste sind zwei Felder untereinander, kein Feld mit zwei
// Inhalten — und genau der Unterschied ist die Festlegung des
// Betreibers vom 2026-08-18.
//
// Der Titel kommt vom einen Inhalt, solange es einer ist. Bei mehreren
// braucht das Feld einen eigenen Namen; sonst stritten zwei Titel um
// dieselbe Zeile.
//
// ── Schritt 1 ────────────────────────────────────────────────────────
//
// Sichtbar aendert sich nichts: ein Applet je Feld, eine Spalte,
// dieselbe Kopfleiste wie bisher (Griff, Titel, wahlweise ein
// nachgestelltes Widget). Die Werkzeuge des Zielbilds — Schloss und
// Schliessen — sind Schritt 3 und stehen hier noch nicht.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-18 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QList>
#include <QString>
#include <QWidget>

class QLabel;
class QVBoxLayout;
class QHBoxLayout;

namespace NereusSDR {

class AppletWidget;

class GridCellWidget : public QWidget {
    Q_OBJECT

public:
    /// `id` ist die Kennung des FELDES, nicht die eines Inhalts.
    explicit GridCellWidget(const QString& id, QWidget* parent = nullptr);

    QString cellId() const { return m_id; }

    // ── Inhalte ──────────────────────────────────────────────────────

    /// Haengt ein Widget hinten an. Das Feld uebernimmt die
    /// Elternschaft; removeWidget gibt sie wieder her, OHNE zu loeschen.
    ///
    /// QWidget, nicht AppletWidget: der Betreiber hat „eine Liste von
    /// Widgets" gesagt, und der Panadapter — im Zielbild selbst ein
    /// Feld — ist keines der zwoelf Applets.
    void addWidget(QWidget* w);
    void removeWidget(QWidget* w);

    /// Alles im Feld, in Anzeigefolge.
    QList<QWidget*> widgets() const { return m_contents; }

    /// Nur die Applets darunter. Fuer alles, was mit Panelkennungen
    /// und dem Auswaehler zu tun hat.
    QList<AppletWidget*> applets() const;

    bool contains(QWidget* w) const { return m_contents.contains(w); }
    bool isEmpty() const { return m_contents.isEmpty(); }

    // ── Kopfleiste ───────────────────────────────────────────────────

    /// Der Griff. AppletPanelWidget haengt seinen Ereignisfilter hier
    /// ein — nicht am Feld, damit ein Zug im Inhalt nichts verschiebt.
    QWidget* titleBar() const { return m_titleBar; }

    /// Eigener Titel. Leer heisst „nimm den Titel des einen Inhalts".
    void setTitle(const QString& title);
    QString title() const { return m_title; }

    /// Ein nachgestelltes Widget in der Kopfleiste (heute: das ☰ der
    /// S-Meter-Kopfzeile). Wird NICHT uebernommen — der Aufrufer
    /// behaelt den Besitz, weil dasselbe Widget die Kopfzeile wechseln
    /// kann.
    void setTrailingWidget(QWidget* w);

    /// Hoehe der Kopfleiste. 22 px mit nachgestelltem Widget, sonst
    /// Style::kTitleBarH — wortgleich mit dem Verhalten von
    /// wrapWithTitleBar, damit Schritt 1 dasselbe Bild ergibt.
    void updateTitleBarHeight();

private:
    void refreshTitleText();

    QString m_id;
    QString m_title;

    QWidget*     m_titleBar{nullptr};
    QHBoxLayout* m_titleLayout{nullptr};
    QLabel*      m_titleLabel{nullptr};
    QVBoxLayout* m_contentLayout{nullptr};
    QWidget*     m_trailing{nullptr};

    QList<QWidget*> m_contents;
};

} // namespace NereusSDR
