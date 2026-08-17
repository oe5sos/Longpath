#pragma once

// =================================================================
// src/gui/applets/AppletFloatingWindow.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// ── Ein Applet als eigenes Fenster ───────────────────────────────────
//
// OE5SOS, 2026-08-16:
//
//   „Alle Widgets einzeln verschiebbar und frei am Bildschirm
//    positionierbar, auch auf einem zweiten Monitor."
//
// Das Gegenstück zu FloatingContainer, eine Ebene tiefer: der dort
// verwaltete ContainerWidget ist eine ganze Messfläche, hier geht es um
// EIN Applet aus der Spalte.
//
// ── Warum keine Wiederverwendung von FloatingContainer ───────────────
//
// Die Klasse ist an ContainerWidget gewachsen: takeOwner() nimmt einen
// ContainerWidget*, die Einklapp-Kopplung hängt an dessen
// minimisedChanged, und der Fenstertitel lautet „NereusSDR Meter
// [12345]". Ein Applet ist kein Meter. Was WIEDERVERWENDET wird, ist
// der Teil, der es verdient: ensureOnVisibleScreen() aus
// gui/WindowPlacement.h — derselbe Rumpf, der den Fall „der Monitor ist
// beim Start nicht mehr da" schon gelöst hat.
//
// ── Eigentum ─────────────────────────────────────────────────────────
//
// Das Fenster BESITZT das Applet, solange es abgelöst ist (Qt-Eltern-
// schaft über das Layout). Beim Andocken gibt releaseApplet() es
// unbeschädigt heraus; erst danach darf das Fenster sterben. Wer das
// Fenster löscht, ohne vorher releaseApplet() zu rufen, nimmt das
// Applet mit — und MainWindow hält rohe Zeiger darauf.
//
// ── Geometrie ────────────────────────────────────────────────────────
//
// Das Fenster BESITZT sie nicht, es MELDET sie. Besitzer ist das
// Profil (Entscheidung des Betreibers, 2026-08-16). geometrySettled()
// kommt am Ende einer Geste, nicht je Bildpunkt — dasselbe Muster wie
// StripEqPanel und ContainerWidget::endDrag().
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-16 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QPointer>
#include <QString>
#include <QWidget>

class QTimer;

namespace NereusSDR {

class AppletWidget;

class AppletFloatingWindow : public QWidget {
    Q_OBJECT

public:
    /// `applet` wird in das Fenster gehängt. `dockIndex` ist die Stelle
    /// im Stapel, an die es beim Andocken zurückkehrt — ohne sie landet
    /// jedes zurückgedockte Applet unten, und der Betreiber sortiert
    /// nach jedem Ausflug neu.
    AppletFloatingWindow(AppletWidget* applet, int dockIndex,
                         QWidget* parent = nullptr);
    ~AppletFloatingWindow() override;

    AppletWidget* applet() const { return m_applet.data(); }
    QString appletId() const;

    int  dockIndex() const { return m_dockIndex; }
    void setDockIndex(int idx) { m_dockIndex = idx; }

    /// Das Applet unbeschädigt herausgeben — Elternteil nullptr, nicht
    /// gelöscht. Danach hält dieses Fenster es nicht mehr, und der
    /// Aufrufer ist dran, es wieder einzuhängen.
    AppletWidget* releaseApplet();

signals:
    /// Der Bediener will es zurück in die Spalte (Fenster geschlossen).
    void dockRequested(const QString& appletId);

    /// Lage oder Grösse stehen still. Am ENDE der Geste, nicht je
    /// Bildpunkt: wer darauf hin ins Profil schreibt, soll einmal
    /// schreiben und nicht vierzigmal.
    void geometrySettled(const QString& appletId);

protected:
    void closeEvent(QCloseEvent* ev) override;
    void moveEvent(QMoveEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;

private:
    void scheduleGeometryReport();

    QPointer<AppletWidget> m_applet;
    int      m_dockIndex{-1};
    QTimer*  m_settleTimer{nullptr};

    /// Gleiche Grössenordnung wie StripEqPanel. Lang genug, dass ein
    /// Zug über den halben Schirm eine Meldung erzeugt und nicht
    /// hundert; kurz genug, dass ein Absturz kurz nach dem Loslassen
    /// nichts kostet.
    static constexpr int kSettleMs = 400;
};

} // namespace NereusSDR
