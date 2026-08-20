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

#include <QSize>

class QWidget;

namespace Longpath {

/// Dafür sorgen, dass `w` auf einem verbundenen Bildschirm zu sehen
/// ist. Verschoben wird, wenn (a) keine Geometrie gespeichert war,
/// (b) die gespeicherte bei (0,0) liegt, oder (c) sie ausserhalb jedes
/// verbundenen Schirms liegt. `anchor` gibt den Zielschirm vor (in der
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
