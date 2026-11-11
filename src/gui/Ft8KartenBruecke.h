// no-port-check: Longpath-original, no Thetis logic.
//
// =================================================================
// src/gui/Ft8KartenBruecke.h  (Longpath)
// =================================================================
//
// ── Vom FT8-Fenster zur QSO-Karte und zurueck (2026-11-11) ──────────
//
// Das FT8-Fenster lebt im Applet-Panel des Hauptfensters, die QSO-Karte
// im Logbuch (zweimal: als eigenes Fenster und eingebettet in der
// Logbuch-Spalte). Zwischen beiden liegen MainWindow, RotorLogbookPanel
// und LogbookWindow -- vier Ebenen, durch die ein Signal und ein Rueckweg
// gefaedelt werden muessten, und jede Ebene kennt dann FT8.
//
// Stattdessen eine Bruecke mit genau zwei Wegen:
//
//   * hin:    gehoert(g)   -- jede Station, die das FT8-Fenster hoert;
//                             jede offene Karte haengt sich daran.
//   * zurueck: rufe(g)     -- ein Klick auf der Karte; die Bruecke reicht
//                             ihn an Ft8Applet::rufeVonDerKarte() weiter,
//                             also an DENSELBEN Antwortweg wie der
//                             Doppelklick in der Fundliste.
//
// Angeschlossen wird an EINER Stelle (MainWindow, nach dem Bau des
// FT8-Fensters). Ein Pruefstand schliesst sein eigenes Fenster ueber
// dieselbe Funktion an, nicht ueber eine nachgebaute Verdrahtung.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11 — Created in C++20/Qt6 for Longpath by Martin Fischer
//                 (OE5SOS), AI-assisted via Anthropic Claude (Claude Code).
// =================================================================
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/LocatorJagd.h"

#include <QMetaObject>
#include <QObject>
#include <QPointer>

namespace Longpath {

class Ft8Applet;

class Ft8KartenBruecke : public QObject {
    Q_OBJECT
public:
    static Ft8KartenBruecke& instanz();

    /// Das FT8-Fenster anschliessen. Ein zweiter Aufruf ersetzt das
    /// vorige -- es gibt EIN FT8-Fenster, und ein Klick darf nicht an
    /// zwei Ablaeufe gehen. nullptr trennt.
    void verbinde(Ft8Applet* applet);
    bool verbunden() const { return !m_applet.isNull(); }

    /// Ein Klick auf einer Karte. Leer heisst: angenommen, das QSO
    /// laeuft. Sonst der Grund, warum nicht -- in Worten fuer den
    /// Betreiber.
    QString rufe(const Ft8Gehoert& g);

signals:
    void gehoert(const Longpath::Ft8Gehoert& g);

private:
    Ft8KartenBruecke() = default;
    QPointer<Ft8Applet>     m_applet;
    QMetaObject::Connection m_verbindung;
};

} // namespace Longpath
