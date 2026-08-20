// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/ToolWindow.h  (Longpath)
// =================================================================
//
// Ein eigenes Fenster fuer irgendein Widget — mit demselben Chrom wie
// alle anderen.
//
// ── Warum es das braucht ─────────────────────────────────────────────
//
// AppletFloatingWindow kann das schon, verlangt aber ein AppletWidget.
// Der Rotor ist keines: er haengt in einem QDockWidget beziehungsweise
// im unteren Bereich des Splitters und hat darum weder Ablöseknopf noch
// Anfasser noch Schloss.
//
// Der Betreiber, 2026-08-20: „rotor noch immer in keinem window welches
// man wie alle andern verschieben und vergroessern kann."
//
// Diese Klasse loest das allgemein: sie nimmt EIN Widget auf, gibt es
// auf Verlangen unbeschaedigt zurueck und bringt Titelleiste, Schloss
// und Anfasser mit. Was sie zeigt, ist ihr gleich — damit ist der
// naechste Fall (Logbuch, Karte, was auch immer) drei Zeilen statt
// einer neuen Klasse.
//
// Qt::Tool, nicht Qt::Window: auf macOS wird daraus ein NSPanel mit der
// Sammelregel „FullScreenAuxiliary". Es schwebt ueber einem
// Elternfenster im Vollbild, statt in dessen Flaeche einzuziehen —
// siehe die ausfuehrliche Begruendung in PanFloatingWindow.cpp.
//
// =================================================================
// Modification history (Longpath)
//   2026-08-20  Martin Fischer (OE5SOS), mit Claude (Opus 5) — neu.
// =================================================================

#pragma once

#include <QPointer>
#include <QWidget>

namespace Longpath {

class WindowTitleBar;

class ToolWindow : public QWidget {
    Q_OBJECT

public:
    /// `id` ist der Schluessel, unter dem Schloss und Lage gemerkt
    /// werden — zwei Werkzeugfenster duerfen sich nicht gegenseitig
    /// ueberschreiben.
    ToolWindow(QWidget* content, const QString& id, const QString& title,
               QWidget* parent = nullptr);
    ~ToolWindow() override;

    QString id() const { return m_id; }
    QWidget* content() const { return m_content.data(); }

    /// Gibt den Inhalt heraus und loest die Elternschaft — OHNE zu
    /// loeschen. Auf dieser Eigenschaft beruht das Andocken: der
    /// Aufrufer haengt das Widget wieder dorthin, wo es herkam.
    QWidget* releaseContent();

    /// Groesse NACH dem Anzeigen setzen. Vorher verlangt die Anordnung
    /// fast nichts; beim Anzeigen meldet sie ihre echte Untergrenze und
    /// zieht das Fenster auf. Dieselbe Falle wie bei den Applet- und
    /// Panadapterfenstern (2026-08-20).
    void applyDefaultSize(const QSize& want);

signals:
    /// Schliessen HEISST andocken: ein Werkzeug, das man wegklickt und
    /// das danach nirgends mehr auftaucht, ist verloren.
    void dockRequested(const QString& id);

protected:
    void closeEvent(QCloseEvent* ev) override;

private:
    QPointer<QWidget> m_content;
    QString           m_id;
    WindowTitleBar*   m_titleBar{nullptr};
    bool              m_sizedOnce{false};
};

} // namespace Longpath
