#pragma once

// =================================================================
// src/core/FlexFrequenzSpur.h  (Longpath)
// =================================================================
//
// Das Urteil der Frequenzspur: hat das PROGRAMM die Scheibe von der
// Maschine weggeschoben?
//
// Anlass, offen seit 2026-10-29: beim Verbinden mit dem FLEX-8400
// steht die Scheibe reproduzierbar neben der Maschine, jedes Mal um
// dieselben 80 kHz.
//
//     Geraet 7.1790 MHz -> Scheibe las 7.2590 MHz
//     Geraet 7.3086 MHz -> Scheibe las 7.3886 MHz
//     Geraet 7.0740 MHz -> Scheibe las 7.1540 MHz   (30.10., 10:03)
//
// Die Spur selbst ist eine Protokollzeile in MainWindow_Flex.cpp. Ihr
// Urteil steht hier, weil GENAU DAS falsch sein kann: eine Spur, die
// bei jedem Handgriff des Bedienenden laut wird, ist kein Instrument,
// sondern Laerm — und eine, die nie laut wird, belegt nichts.
//
// Drei Dinge muessen zusammenkommen:
//
//   1. Die Wache ist OFFEN. Ist sie gesetzt, kam der Wert von der
//      Maschine selbst — dann ist Gleichstand zu erwarten und kein
//      Unterschied zu melden.
//   2. Die Maschine hat sich ueberhaupt schon gemeldet. Ohne ihre
//      Frequenz gibt es nichts zu vergleichen; eine fehlende Angabe
//      ist kein Befund (siehe die Regel „Abwesenheit ist keine
//      Aussage").
//   3. Der Abstand ist groeszer als die Schwelle. Ein Kilohertz laesst
//      Rundungen und das Nachziehen der Scheibe durch, 80 kHz nicht.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <cmath>

namespace Longpath {
namespace FlexFrequenzSpur {

/// Die Schwelle, ab der ein Abstand ein Befund ist, nicht eine
/// Rundung. Ein Kilohertz: der gesuchte Versatz ist achtzigmal so
/// grosz, und kleiner stellt niemand von Hand nach.
inline constexpr double kSchwelleHz = 1000.0;

/// Hat das Programm die Scheibe von der Maschine weggeschoben?
///
/// \param scheibeHz  wohin die Scheibe gerade gesetzt wurde
/// \param geraetHz   was die Maschine zuletzt selbst gemeldet hat
///                   (0 oder kleiner: sie hat sich noch nicht gemeldet)
/// \param wacheGesetzt  true, wenn der Wert VON der Maschine kam
inline bool istVersatz(double scheibeHz, double geraetHz,
                       bool wacheGesetzt,
                       double schwelleHz = kSchwelleHz)
{
    if (wacheGesetzt)      { return false; }
    if (!(geraetHz > 0.0)) { return false; }
    return std::abs(scheibeHz - geraetHz) > schwelleHz;
}

} // namespace FlexFrequenzSpur
} // namespace Longpath
