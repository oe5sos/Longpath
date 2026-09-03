#pragma once

// =================================================================
// src/gui/WindowPlacement.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// ── Ein Fenster auf einen sichtbaren Schirm holen ────────────────────
//
// Der Rumpf dieser Funktion stand seit 2026-04-17 als
// FloatingContainer::ensureVisiblePosition() im Baum und löste den Fall
// „der Monitor ist beim Start nicht mehr da" bereits vollständig. Er
// hing dort nur an einer Klasse fest, die ein Meter-Fenster ist — er
// fasst nichts von ihr an ausser zwei Grössenkonstanten.
//
// Am 2026-08-16 wurde er hierher gehoben, damit die Applet-Fenster
// dieselbe Lösung BENUTZEN, statt eine zweite halbe zu bekommen.
// Verhalten unverändert; FloatingContainer ruft jetzt hierher.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Ursprung als FloatingContainer::ensureVisiblePosition,
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
//   2026-08-16 — Als freie Funktion herausgezogen, Verhalten
//                 unverändert. Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork).
// =================================================================

#include <QPoint>
#include <QSize>

class QWidget;

namespace Longpath {

// ── Schwebende Fenster aufs Raster ────────────────────────────────────
//
// Betreiber 2026-09-02: "die schwebenden Fenster sollen zueinander in
// einer Linie sein". Alle vier schwebenden Fensterklassen (ToolWindow,
// PanFloatingWindow, AppletFloatingWindow, FloatingContainer) werden
// per NATIVEM macOS-Fenstergriff gezogen, nicht per eigenem Maus-Code
// -- ein direktes Runden in moveEvent() wuerde also gegen das
// Betriebssystem kaempfen (es zieht dorthin, wo die Maus ist, wir
// zoegen sofort zurueck: sichtbares Ruckeln unter dem Cursor). Der
// gedaempfte Weg schnappt erst, wenn die Bewegung kurz zur Ruhe
// gekommen ist -- ein Rueckstoss durch das Runden selbst loest genau
// eine weitere, dann bereits treffende Runde aus, kein Endlosdreh.
//
// Reiner Punkt-Helfer, keine Fensteroperation selbst.
constexpr int kSnapGridPx = 8;
QPoint snappedTopLeft(const QPoint& pos, int grid = kSnapGridPx);

// Von moveEvent() jeder schwebenden Fensterklasse mit einer Zeile
// aufzurufen. Legt bei Bedarf einen einmaligen, an `w` gebundenen
// Debounce-Timer an (kein Member in der aufrufenden Klasse noetig) und
// startet ihn neu; erst wenn er ablaeuft, ohne dass diese Funktion
// zwischenzeitlich erneut aufgerufen wurde, schnappt er `w` aufs
// Raster -- also nach dem Loslassen, nicht waehrend des Ziehens.
void snapToGridAfterSettle(QWidget* w, int grid = kSnapGridPx,
                           int delayMs = 180);

/// Dafür sorgen, dass `w` auf einem verbundenen Bildschirm zu sehen
/// ist. Verschoben wird, wenn (a) keine Geometrie gespeichert war,
/// (b) die gespeicherte bei (0,0) liegt, (c) sie ausserhalb jedes
/// verbundenen Schirms liegt, oder (d) sie das `anchor`-Fenster in
/// seiner HEUTIGEN Grösse/Lage um weniger als 80×40 Bildpunkte
/// ueberlappt -- LETZTERES NUR, wenn `anchor` bereits sichtbar ist
/// (Betreiber 2026-08-31: "S-Meter usw. liegen frei am Desktop" --
/// eine Position, gespeichert aus einer breiteren/Vollbild-Sitzung,
/// blieb bislang auch dann unangetastet, wenn das Hauptfenster
/// seither viel kleiner geworden ist; ein erster, grosszuegigerer
/// Anlauf ["beruehrt sich ueberhaupt"] liess noch ein Fenster durch,
/// das nur mit einem schmalen Rand an der Hauptfenster-Kante
/// haengenblieb. Zweiter Fund, selber Abend, "vor allem rotor,
/// bandfilter und panadapter sind immer anders als abgespeichert":
/// Rotor/Bandfilter/Panadapter werden alle waehrend MainWindows
/// eigenem Konstruktor wiederhergestellt, lange vor dem ersten
/// show() -- zu diesem Zeitpunkt hat `anchor` seine Vollbild-/
/// Maximiert-Groesse noch nie erreicht, jede Ueberlapp-Pruefung
/// dagegen verwarf die gespeicherte Position also bei JEDEM Start,
/// nicht nur manchmal). `anchor` gibt den Zielschirm vor (in der
/// Regel das Hauptfenster); das Fenster wird auf dessen Schirm
/// zentriert. Liegt die Geometrie schon richtig, geschieht nichts.
///
/// ── Zwei bekannte Schwächen, bewusst stehen gelassen ────────────────
///
/// Beide stammen aus dem Ursprung und werden hier NICHT behoben, weil
/// eine Verhaltensänderung an dieser Stelle jedes freistehende Fenster
/// betrifft. Wer sie angeht, tut es als eigenen Schritt:
///
///  1. `(0,0)` wird als „keine Geometrie" gelesen. Das ist ein gültiger
///     Wert, der als Sonderfall missbraucht wird: auf einem Aufbau mit
///     einem Schirm links oder über dem Hauptschirm ist (0,0) eine
///     ganz normale Position, und ein Fenster, das dorthin gehört,
///     wird bei JEDEM Start zur Mitte gezogen. Richtig wäre ein
///     eigenes Feld „Geometrie vorhanden ja/nein" im gespeicherten
///     Satz, statt einen Wert doppelt zu belegen.
///
///  2. Der Sichtbarkeitstest ist `intersects()` — ein Bildpunkt
///     Überlappung zählt als sichtbar. Ein Fenster, das zu 99 % über
///     die Kante hängt, gilt damit als in Ordnung. Sinnvoll wäre eine
///     Mindestfläche in der Grössenordnung von 80 × 40 px, also
///     genug, um das Fenster mit der Maus wieder hereinziehen zu
///     können.
void ensureOnVisibleScreen(QWidget* w, QWidget* anchor, QSize minSize);

} // namespace Longpath
