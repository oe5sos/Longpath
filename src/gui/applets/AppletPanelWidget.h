// src/gui/applets/AppletPanelWidget.h

// =================================================================
// src/gui/applets/AppletPanelWidget.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Scrollable applet-panel pattern (260px fixed width, 16px title
//                 bars) ported from AetherSDR `src/gui/AppletPanel.{h,cpp}`.
// =================================================================

#pragma once

#include <QWidget>
#include <QList>
#include <QMap>

class QVBoxLayout;
class QScrollArea;
class QPushButton;
class QMenu;

namespace NereusSDR {

class AppletWidget;
class AppletGrid;
class GridCellWidget;

// Scrollable vertical stack of applets with AetherSDR AppletPanel styling.
// This is a SINGLE widget that goes into ContainerWidget::setContent().
// It internally manages N child widgets (applets + meters) in a scrollable layout.
//
// From AetherSDR AppletPanel.cpp:
// - Root background: #0a0a18
// - Fixed width: 260px
// - Each child wrapped with a gradient title bar (16px)
// - Scroll area: QFrame::NoFrame, HScrollBarAlwaysOff, widgetResizable true
class AppletPanelWidget : public QWidget {
    Q_OBJECT
public:
    explicit AppletPanelWidget(QWidget* parent = nullptr);

    // Set a header widget (e.g., MeterWidget) that stays visible above
    // the scroll area. Height scales dynamically with width using the
    // given aspect ratio (default 2.0 = AetherSDR's 280:140). Calling
    // again replaces the existing header (old wrapper + widget deleted).
    void setHeaderWidget(QWidget* widget, const QString& title, float aspectRatio = 2.0f);

    // Remove the current header widget and its title-bar wrapper without
    // installing a new one. Safe to call when no header is set. Used by
    // ContainerManager to detach the MeterWidget before a top-level
    // reparent (Qt 6.11.0 D3D11 QRhiWidget does not survive the HWND
    // recreation that reparent triggers on Windows).
    void clearHeaderWidget();

    QWidget* headerWidget() const { return m_headerWidget; }


    // Add an applet — wraps it with a title bar and adds to the scroll stack
    void addApplet(AppletWidget* applet);

    // Remove an applet (and its title bar wrapper) from the scroll stack.
    // The applet widget itself is hidden and reparented to nullptr (not deleted).
    void removeApplet(AppletWidget* applet);

    // Toggle visibility of an already-added applet without removing it
    // from the layout. Preserves stack position when re-shown. No-op for
    // null or unknown applets. NereusSDR-original (no Thetis equivalent).
    void setAppletVisible(AppletWidget* applet, bool visible);

    // Install a menu on the panel's top-right ☰ button. Until this is

    // Add a raw widget (e.g., MeterWidget) with a custom title to the scroll area
    void addWidget(QWidget* widget, const QString& title);

    /// Die Applets in der Reihenfolge, in der sie im Stapel STEHEN —
    /// nicht in der, in der sie angemeldet wurden. Nach einem Zug mit
    /// der Maus sind das zwei verschiedene Listen, und wer speichern
    /// will, meint diese hier.
    QList<AppletWidget*> applets() const { return m_applets; }

    // ── Verschieben ──────────────────────────────────────────────────
    //
    // OE5SOS, 2026-08-15: „Die einzelnen Widget, sodass man jedes
    // auswählen, aktivieren und verschieben kann."
    //
    // Auswählen und Aktivieren gab es (das Plus, AppletVisibility­-
    // Controller). Verschieben nicht — die Griffpunkte ⋮⋮ in den
    // Titelleisten waren gemalt und taten nichts. Ein Griff, der nach
    // Anfassen aussieht und beim Anfassen nichts tut, ist schlimmer als
    // gar keiner: er verspricht.

    /// Ein Applet an eine Stelle im Stapel setzen. Stellen außerhalb
    /// werden auf den Rand geklemmt. Gibt true zurück, wenn sich etwas
    /// bewegt hat.
    bool moveApplet(AppletWidget* applet, int toIndex);

    /// Die gespeicherte Reihenfolge herstellen. Applets, die in der
    /// Liste fehlen, bleiben hinten in ihrer bisherigen Folge stehen —
    /// so verschwindet ein neu hinzugekommenes Widget nicht, nur weil
    /// die gespeicherte Liste es noch nicht kennt.
    void setAppletOrder(const QList<AppletWidget*>& order);

    /// Die Stelle eines Applets im Stapel — die Zahl, an die es beim
    /// Andocken zurückkehren soll. -1, wenn es nicht in dieser Spalte
    /// steht.
    int appletPosition(AppletWidget* applet) const;

    /// Testnaht: baut das Kontextmenü der Titelleiste dieses Applets,
    /// ohne exec() zu rufen. Der Aufrufer besitzt das Menü.
    /// Gleiches Muster wie AmpApplet::buildContextMenuForTesting().
    ///
    /// Es gibt sie, weil das Ablösen jetzt NUR über dieses Menü läuft.
    /// Ein einziger Pfad, der nicht geprüft wird, ist ein einziger
    /// Pfad, der eines Tages stillschweigend nichts mehr tut.
    QMenu* buildTitleBarMenuForTesting(AppletWidget* applet);

signals:
    /// Nach einem abgeschlossenen Zug. Nicht während des Ziehens: wer
    /// darauf hin speichert, soll einmal speichern und nicht vierzigmal.
    void appletsReordered();

    // ── Ablösen — EIN Weg, und der ist der Menüpunkt ─────────────────
    //
    // OE5SOS, 2026-08-17. Eine erste Fassung hatte auch ein Ziehen über
    // eine seitliche Schwelle; sie ist wieder heraus.
    //
    // Der Grund ist keine Geschmacksfrage. AetherSDR hat für dieselbe
    // Sache genau das getan, und ausdrücklich wegen Abstürzen beim
    // Umhängen über Top-Level-Grenzen:
    //
    //   #2495 — QRhiWidget behält beim Reparent einen Aufräum-Rückruf
    //           auf freigegebenen Zustand; float/dock verdirbt die GPU-
    //           Darstellung, und beim Beenden stürzt es ab. AetherSDR
    //           räumt deshalb JEDES QRhiWidget-Kind vor jedem Reparent
    //           ab (prepareRhiChildrenForReparent, an drei Stellen in
    //           containers/ContainerManager.cpp).
    //   #4319 — D3D11 bricht, wenn beim Herauslösen Texturen neu
    //           angelegt werden.
    //   #4617 — dieselbe Familie.
    //
    // Nachgesehen: AetherSDRs Applet-Ziehen ist ein QDrag mit der
    // Kennung "application/x-aethersdr-applet", und die einzige Stelle,
    // die den Wurf annimmt, ist der eigene Rollbereich der Spalte
    // (AppletPanel.cpp:160-230, AppletDropArea). Ein Zug endet dort
    // IMMER in einem Umsortieren. Das Ablösen läuft über die
    // Andockarten des ContainerWidget — ein bewusster, einzelner Pfad.
    //
    // NereusSDR macht es genauso: Ziehen bleibt auf das Umsortieren
    // innerhalb der Spalte beschränkt, wo kein Fensterwechsel
    // stattfindet. Bequemlichkeit später, wenn der Pfad nachweislich
    // hält.
    //
    // Das Panel löst auch jetzt NICHT selbst ab: es kennt weder die
    // Fensterliste noch das Profil, und ein Widget, das sich selbst aus
    // seinem Elternteil herausoperiert, ist der kürzeste Weg zu einem
    // Zeiger, den zwei Stellen zu halten glauben. MainWindow hört zu
    // und macht die Arbeit.
    //
    /// `dockIndex` ist die Stelle, an der das Applet stand — sie muss
    /// mit hinaus, weil das Applet nach dem Ausbau nicht mehr sagen
    /// kann, wo es herkam.
    void appletDetachRequested(AppletWidget* applet, int dockIndex);

    /// Das ✕ im Fensterkopf. Wie beim Ablösen macht das Panel es NICHT
    /// selbst: es kennt die Sichtbarkeitsverwaltung nicht. MainWindow
    /// hoert zu und setzt AppletVisibilityController.
    void appletHideRequested(AppletWidget* applet);

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    // Wrap a child widget with a 16 px title bar (grip dots + label).
    // When `trailing` is non-null, the title bar is bumped to 22 px to
    // accommodate the trailing widget (right-aligned, after the stretch).
    // Used by setHeaderWidget to embed the ☰ menu button in the S-Meter
    // title bar — keeps the button reachable without a dedicated row
    // that competes with the master-volume strip above.
    QWidget* wrapWithTitleBar(QWidget* child, const QString& title,
                              QWidget* trailing = nullptr);

    QVBoxLayout* m_rootLayout = nullptr;     // top-level: header + scroll
    QVBoxLayout* m_headerLayout = nullptr;   // fixed header above scroll area
    QScrollArea* m_scrollArea = nullptr;

    // ── Schritt 1 des freien Rasters (2026-08-18) ────────────────────
    //
    // Der Stapel aus Huellen in einem QVBoxLayout ist ein Raster mit
    // EINER Spalte geworden. Sichtbar aendert sich nichts — die Zeile
    // IST der Index, solange es eine Spalte gibt. Genau deshalb laesst
    // sich der Umbau von „Reihenfolge" auf „Ort" allein und pruefbar
    // machen, bevor Spalten, Spannweiten und Density darauf aufbauen.
    //
    // Aus der Huelle ist ein FELD geworden: ein Behaelter mit
    // Kopfleiste und einer LISTE von Inhalten (Festlegung des
    // Betreibers, siehe GridCell.h). Schritt 1 legt eines je Feld
    // hinein; das Datenmodell traegt schon mehrere.
    AppletGrid* m_grid = nullptr;
    QList<AppletWidget*> m_applets;
    QMap<AppletWidget*, GridCellWidget*> m_wrappers;  // Applet → Feld

    // ── Ziehen ───────────────────────────────────────────────────────
    int  stackIndexOf(QWidget* wrapper) const;
    AppletWidget* appletForWrapper(QWidget* wrapper) const;
    void dragTo(int globalY);

    /// Das Kontextmenü einer Titelleiste — der einzige Weg zum Ablösen.
    void showTitleBarMenu(QWidget* titleBar, const QPoint& globalPos);

    QMap<QWidget*, QWidget*> m_titleBars;   // Titelleiste → Feld
    AppletWidget* m_dragApplet{nullptr};
    int  m_dragStartY{0};
    bool m_dragging{false};
    /// Erst ab hier ist es ein Zug und kein verwackelter Klick.
    /// Senkrecht gemessen — waagerecht gibt es nichts zu tun, siehe die
    /// Begründung bei appletDetachRequested.
    static constexpr int kDragThresholdPx = 4;
    QWidget*      m_headerWidget  = nullptr;   // header widget for dynamic resize
    QWidget*      m_headerWrapper = nullptr;   // title-bar wrapper of the header
    float         m_headerAspect  = 0.0f;      // width/height ratio for header

    // ☰ menu button. Created in the constructor (hidden, parented to
    // `this` for memory management). When setHeaderWidget runs it gets
    // re-parented into the S-Meter title bar via wrapWithTitleBar.
};

} // namespace NereusSDR
