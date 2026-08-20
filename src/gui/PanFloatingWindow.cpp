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

#include <QScreen>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QMoveEvent>
#include <QResizeEvent>

namespace Longpath {

PanFloatingWindow::PanFloatingWindow(PanadapterApplet* applet, QWidget* parent)
    : QWidget(parent, Qt::Window)
    , m_applet(applet)
{
    setWindowTitle(QStringLiteral("Longpath - Pan %1")
                       .arg(applet ? applet->panId() : QString()));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    if (applet) {
        layout->addWidget(applet);
    }
    // Eine kleinere Untergrenze als die, die der Panadapter im Splitter
    // mitbringt. Ohne das zieht die Mindestgroesse des Inhalts das
    // Fenster beim Anzeigen auf — beim Selbsttest am 2026-08-20 ging es
    // bildschirmfuellend auf, was wie ein Fehler aussieht und den
    // Arbeitsplatz zustellt.
    setMinimumSize(420, 240);
    resize(900, 460);
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
    requestDock();
    event->accept();
}

void PanFloatingWindow::moveEvent(QMoveEvent*)
{
    emit geometryChanged(panId(), saveGeometry());
}

void PanFloatingWindow::resizeEvent(QResizeEvent*)
{
    emit geometryChanged(panId(), saveGeometry());
}

} // namespace Longpath
