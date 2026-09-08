// no-port-check: AetherSDR-derived NereusSDR file. Top-level QWidget
// wrapper for detaching a PanadapterApplet to a second monitor is adapted
// structurally from AetherSDR src/gui/PanFloatingWindow.{h,cpp} [@0cd4559].
// Registered in docs/attribution/aethersdr-reconciliation.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/PanFloatingWindow.cpp  (NereusSDR)
// =================================================================
//
// Ported (structurally) from AetherSDR src/gui/PanFloatingWindow.{h,cpp}
// [@0cd4559].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// See PanFloatingWindow.h for full Modification history (NereusSDR).
// =================================================================

#include "gui/PanFloatingWindow.h"
#include "gui/PanadapterApplet.h"
#include "core/AppSettings.h"
#include "gui/FramelessResizer.h"
#include "gui/MacFloatingWindowBehavior.h"
#include "gui/WindowChrome.h"
#include "gui/WindowPlacement.h"

#include <QScreen>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QMoveEvent>
#include <QResizeEvent>

namespace Longpath {

PanFloatingWindow::PanFloatingWindow(PanadapterApplet* applet, QWidget* parent)
    // ── Qt::Tool, nicht Qt::Window ──────────────────────────────────
    //
    // Auf macOS wird aus Qt::Tool ein NSPanel mit der Sammelregel
    // „FullScreenAuxiliary": es SCHWEBT ueber einem Elternfenster, das
    // im Vollbild steht, statt in dessen Vollbildflaeche einzuziehen.
    //
    // Genau daran hing die Klage des Betreibers vom 2026-08-20 („es
    // geht immer ein neues fenster bildschirm fuellend auf"). Sein
    // MainWindowGeometry sagt Rahmen (0,33)-(1469,955) mit gesetztem
    // Vollbild-Vermerk. Ein Qt::Window, das waehrenddessen aufgeht,
    // landet in derselben Flaeche und wirkt bildschirmfuellend — Qt
    // meldet dabei brav 900x460, gemessen am 2026-08-20. Die Zahl war
    // richtig und das Bild trotzdem falsch, weshalb kein Test es sehen
    // konnte.
    //
    // Ein Werkzeugfenster steht ausserdem immer ueber seinem
    // Elternfenster und nicht im Programmumschalter — beides richtig
    // fuer ein abgeloestes Teil der Konsole.
    : QWidget(parent, Qt::Tool)
    , m_applet(applet)
{
    setWindowTitle(QStringLiteral("Longpath - Pan %1")
                       .arg(applet ? applet->panId() : QString()));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Eigene Leiste, eigener Anfasser ─────────────────────────────
    //
    // Der Betreiber hat den Panadapter ausdruecklich mitgemeint: „wie
    // zum Beispiel den Pen Adapter kleiner nach rechts und nach links
    // vergroessern lassen". Bis heute hing hier der Rahmen des
    // Betriebssystems — schiebbar, aber fremd, und ohne sichtbaren
    // Anfasser in der Ecke.
    //
    // topMoveReserve == Leistenhoehe: der obere Streifen gehoert dem
    // Ziehen. Sonst schnappt der Resizer den Griff weg und das Fenster
    // laesst sich nicht mehr bewegen.
    m_titleBar = new WindowTitleBar(
        QStringLiteral("Panadapter %1").arg(applet ? applet->panId()
                                                   : QString()), this);
    connect(m_titleBar, &WindowTitleBar::closeRequested,
            this, &QWidget::close);
    connect(m_titleBar, &WindowTitleBar::dockRequested,
            this, &PanFloatingWindow::requestDock);
    // Je Panadapter ein eigener Schluessel: zwei festgestellte Fenster
    // duerfen sich nicht gegenseitig ueberschreiben.
    m_titleBar->setLockKey(QStringLiteral("Pan_%1")
                               .arg(applet ? applet->panId()
                                           : QStringLiteral("A")));
    layout->addWidget(m_titleBar);

    if (applet) {
        layout->addWidget(applet);
    }

    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    FramelessResizer::install(this, 6, m_titleBar->height());
    attachResizeGrip(this);
    // Eine kleinere Untergrenze als die, die der Panadapter im Splitter
    // mitbringt. Ohne das zieht die Mindestgroesse des Inhalts das
    // Fenster beim Anzeigen auf — beim Selbsttest am 2026-08-20 ging es
    // bildschirmfuellend auf, was wie ein Fehler aussieht und den
    // Arbeitsplatz zustellt.
    setMinimumSize(420, 240);
    resize(900, 460);

    restoreGeometryState();

    // Betreiber 2026-08-31: "S-Meter usw. liegen frei am Desktop" --
    // derselbe Grund wie bei ToolWindow (siehe dort): restoreGeometryState()
    // rief den vorhandenen Klammer-Helfer nie auf, eine aus einer
    // breiteren/Vollbild-Sitzung gespeicherte Position blieb also auch
    // dann stehen, wenn `parent` seither viel kleiner geworden ist. Nach
    // restoreGeometryState(), vor applyDefaultSize()'s eigener,
    // unabhaengiger Erstlauf-Groesse -- dieselbe "wer zuerst kommt"-
    // Reihenfolge, die m_sizedOnce ueberall in dieser Klasse schon
    // durchhaelt.
    ensureOnVisibleScreen(this, parent, QSize(420, 240));

    // Betreiber, wiederholt gemeldet: siehe AppletFloatingWindow.cpp,
    // derselbe Grund -- der "FullScreenAuxiliary"-Kommentar oben war nie
    // mehr als eine Annahme.
    enableFullScreenAuxiliaryBehavior(this);
}

// Die Groesse setzen, NACHDEM der Inhalt sichtbar ist.
//
// Nicht im Baukasten und nicht in showEvent — beides ist zu frueh.
// PanadapterStack::floatPanadapter versteckt das Applet, bevor es
// umgehaengt wird (der QRhi-Kontext muss sauber abgebaut werden), zeigt
// das Fenster und erst DANACH das Applet. Solange es versteckt ist,
// verlangt die Anordnung fast nichts; sobald es auftaucht, verlangt sie
// ihre echte Mindestgroesse und zieht das Fenster mit auf.
//
// Beim Selbsttest am 2026-08-20 sah man genau das: Qt meldete brav
// 900x460, auf dem Schirm stand ein bildschirmfuellendes Fenster. Die
// Zahl war richtig und trotzdem falsch, weil sie zum falschen Zeitpunkt
// gesetzt wurde.
void PanFloatingWindow::applyDefaultSize()
{
    if (m_sizedOnce) { return; }   // wer nachher zieht, darf es behalten
    m_sizedOnce = true;

    QSize want(900, 460);
    if (QScreen* sc = screen()) {
        const QSize avail = sc->availableSize();
        want.setWidth (qMin(want.width(),  avail.width()  - 80));
        want.setHeight(qMin(want.height(), avail.height() - 80));
    }

    // Die Untergrenze des Inhalts darf nicht groesser sein als das, was
    // wir wollen — sonst zieht sie das Fenster gleich wieder auf.
    if (m_applet) { m_applet->setMinimumSize(360, 200); }
    resize(want);
}

PanFloatingWindow::~PanFloatingWindow() = default;

PanadapterApplet* PanFloatingWindow::applet() const
{
    return m_applet.data();
}

QString PanFloatingWindow::panId() const
{
    return m_applet ? m_applet->panId() : QString();
}

void PanFloatingWindow::requestDock()
{
    emit dockRequested(panId());
}

void PanFloatingWindow::closeEvent(QCloseEvent* event)
{
    // 2026-09-01 Diagnose (Betreiber: "nach vollbild... anordnung wieder
    // falsch" -- der schwebende Panadapter landete nach einem Vollbild-
    // Wechsel angedockt, PanFloating_pan-0 stand danach auf False). Klaert,
    // ob dieser closeEvent tatsaechlich waehrend eines Vollbild-Uebergangs
    // des Hauptfensters eintrifft (macOS-Fenstermanagement-Nebenwirkung)
    // oder aus einem anderen Grund -- wird nach der Bestaetigung entfernt.
    qWarning() << "[PanFloatClose]" << panId() << "m_shuttingDown="
               << m_shuttingDown << "ownWindowState=" << windowState()
               << "spontaneous=" << event->spontaneous();

    // From AetherSDR PanFloatingWindow.cpp:84-95 [@0cd4559].
    //
    // Beim Beenden: hinnehmen und still sein. Der Betreiber sah am
    // 2026-08-22, was die alte Fassung anrichtete — sie bat AUCH beim
    // Beenden ums Zurueckhaengen, und das Umhaengen eines
    // GPU-Spektrums mitten im Abbau greift auf einen toten
    // Zeichenkontext zu. Sein Bild: der Panadapter stand allein ueber
    // dem Schreibtisch, der Wasserfall roter Schrott.
    if (m_shuttingDown) {
        event->accept();
        return;
    }

    // Sonst: NICHT schliessen, sondern zurueckdocken. Das Ignorieren
    // ist Absicht (Aether ebenso) — wer das Fenster schliesst UND
    // gleichzeitig zurueckhaengt, laesst zwei Abbauten um dasselbe
    // Widget rennen.
    requestDock();
    event->ignore();
}

void PanFloatingWindow::moveEvent(QMoveEvent*)
{
    emit geometryChanged(panId(), saveGeometry());
    saveGeometryState();
    // Betreiber 2026-09-02: schwebende Fenster sollen zueinander
    // fluchten. Gedaempft (siehe WindowPlacement.h) -- ein direktes
    // Runden hier wuerde gegen das native Ziehen kaempfen.
    snapToGridAfterSettle(this);
}

void PanFloatingWindow::resizeEvent(QResizeEvent*)
{
    emit geometryChanged(panId(), saveGeometry());
    saveGeometryState();
}

void PanFloatingWindow::saveGeometryState()
{
    // Je Panadapter ein eigener Schluessel, wie beim Schloss oben --
    // zwei abgeloeste Panadapter duerfen sich nicht gegenseitig
    // ueberschreiben.
    AppSettings::instance().setValue(
        QStringLiteral("PanFloatGeometry_%1").arg(panId()), saveGeometry());
}

void PanFloatingWindow::restoreGeometryState()
{
    const QByteArray st = AppSettings::instance()
        .value(QStringLiteral("PanFloatGeometry_%1").arg(panId()))
        .toByteArray();
    if (st.isEmpty()) { return; }   // erstes Mal -- applyDefaultSize() entscheidet
    restoreGeometry(st);
    // Verhindert, dass das nachtraegliche applyDefaultSize() (vom
    // Stack aufgerufen, NACHDEM das Applet wieder sichtbar ist) die
    // gerade wiederhergestellte Groesse ueberschreibt.
    m_sizedOnce = true;
}

} // namespace Longpath
