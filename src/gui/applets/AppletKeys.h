#pragma once

// =================================================================
// src/gui/applets/AppletKeys.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// ── EINE Kennung, nicht zwei ─────────────────────────────────────────
//
// Bis 2026-08-18 trug jedes Applet zwei Namen:
//
//   Panelkennung   „Rx"  — m_appletsById, AppletVisibilityController,
//                          der Auswaehler, das Profilfeld „visible"
//   Eigenkennung   „rx"  — AppletWidget::appletId(), die Profilfelder
//                          „order" und „floatingApplets",
//                          AppSettings „AppletStackOrder"
//
// Sie stimmen bei vier von neunzehn Applets ueberein. Bei den uebrigen
// laufen sie auseinander: rx/Rx, TX/Tx, PHCW/PhoneCw, vax/Vax, amp/Amp,
// tuner/Tuner, tci/Tci, pure_signal/PureSignal, RADE/Rade,
// tci_clients/ClientChain.
//
// Jede Stelle, an der ein Schluessel der einen Sorte in einer Karte der
// anderen nachgeschlagen wurde, gab still nullptr zurueck. Das war kein
// Absturz und keine Warnung — nur ein Nichts, das wie ein Ergebnis
// aussieht. Zwei Fehler kamen daher:
//
//   1. „Als Fenster abloesen" nahm das RX-Panel aus der Spalte, fragte
//      die Sichtbarkeit unter „rx" ab, bekam false und zeigte das
//      fertig gebaute Fenster nie. Das Applet war weg, und der Zustand
//      wurde so gespeichert.
//   2. Die gespeicherte Reihenfolge loeste beim Start nur vier von
//      vierzehn Eintraegen auf; der Rest fiel weg.
//
// Diese Datei ist die Bruecke, und sie ist absichtlich eine FREIE
// Funktion ueber einer uebergebenen Karte statt einer Methode auf
// MainWindow: MainWindow laesst sich im Pruefstand nicht bauen (ein
// blosses `MainWindow w;` startet echte UDP-Suche im Netz), und eine
// Aufloesung, die niemand pruefen kann, ist genau die Art Code, in der
// dieser Fehler entstanden ist.
//
// ── Welche Kennung gewinnt ───────────────────────────────────────────
//
// Die PANELKENNUNG, ueberall. Sie steht im Auswaehler, im Menue und im
// Profil und ist die einzige, die ein Mensch je zu Gesicht bekommt.
// appletId() bleibt, wo es hingehoert: als Eigenname des Widgets.
//
// Aufnahmen von vor dem 2026-08-18 nennen Eigenkennungen. appletFor()
// nimmt beide Sorten an, damit eine bestehende Anordnung nicht beim
// ersten Start nach diesem Update verlorengeht — dieselbe Ruecksicht
// wie bei Migration v9.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-18 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QHash>
#include <QString>

namespace Longpath {

class AppletWidget;

/// Die Karte Panelkennung → Applet, wie MainWindow sie haelt.
using AppletMap = QHash<QString, AppletWidget*>;

namespace AppletKeys {

/// Die Panelkennung eines Applets. Steht es nicht in der Karte, ist
/// seine Eigenkennung das Beste, was es gibt — ein solches Applet steht
/// dann auch nicht im Auswaehler und wird nicht ins Profil gehalten.
QString panelIdFor(const AppletMap& map, const AppletWidget* applet);

/// Das Applet zu einer Kennung, gleich welcher Sorte.
AppletWidget* appletFor(const AppletMap& map, const QString& key);

/// Eine Kennung beliebiger Sorte auf die Panelkennung bringen. Eine
/// unbekannte Kennung bleibt unveraendert: eine Aufnahme von vor einem
/// Update kann Widgets nennen, die es nicht mehr gibt, und die sollen
/// nicht stillschweigend zu etwas anderem werden.
QString canonical(const AppletMap& map, const QString& key);

} // namespace AppletKeys
} // namespace Longpath
