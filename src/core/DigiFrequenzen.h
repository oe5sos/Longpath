// no-port-check: Longpath-eigene Tafel. Die Zahlen sind die
// veroeffentlichten Anrufstellen aus der Vorgabeliste von WSJT-X
// (`FrequencyList.cpp`, Default_Frequencies) -- Konvention der
// Betriebsart, kein Quelltext von dort.
// =================================================================
// src/core/DigiFrequenzen.h  (Longpath)
// =================================================================
//
// Wo auf einem Band FT8 und FT4 liegen.
//
// Betreiber am 2026-10-21, vor der Antenne stehend: „ich will nicht
// die freq suchen" und „quasi wie jtdx". Genau das ist hier gemeint:
// wer Digital waehlt, soll nicht nachschlagen muessen, wo das Band
// seine Anrufstelle hat.
//
// Eine reine Tafel mit einer Nachschlagefunktion, absichtlich ohne
// jede Kenntnis von Fenstern oder Funkgeraeten -- so laesst sie sich
// pruefen, ohne eines von beiden zu bauen.
//
// Die Zahlen sind Skalenfrequenzen in Hertz und gelten fuer oberes
// Seitenband (USB/DIGU), so wie WSJT-X und JTDX sie fuehren. Die
// Tonlage im Durchlass kommt obendrauf, sie gehoert nicht hierher.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-21 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Claude Code).
// =================================================================

#pragma once

#include "models/Band.h"

#include <QtGlobal>

namespace Longpath::DigiFrequenzen {

/// Die digitalen Betriebsarten, die Longpath selbst dekodiert.
enum class Digimodus {
    Ft8,
    Ft4,
};

/// Die Anrufstelle (Skalenfrequenz, Hertz) fuer Band und Betriebsart.
///
/// Gibt **0** zurueck, wenn es fuer dieses Band keine gibt -- 60 m hat
/// keine FT4-Stelle, und fuer GEN/WWV/XVTR und die Rundfunkbaender
/// gibt es ueberhaupt keine. Null heisst „ich weiss es nicht", nicht
/// „null Hertz": der Aufrufer laesst die Frequenz dann stehen, statt
/// das Geraet irgendwohin zu stellen.
qint64 anruffrequenz(Band band, Digimodus modus);

/// Ob die Skalenfrequenz schon auf der Anrufstelle ihres Bandes steht.
///
/// Mit Spielraum, weil ein Geraet nicht immer auf das Hertz genau
/// meldet (und weil der Betreiber von Hand ein paar hundert Hertz
/// danebenstehen darf, ohne dass ihm jemand hineinstellt).
bool stehtSchonDort(qint64 skalenfrequenzHz, Digimodus modus,
                    qint64 spielraumHz = 500);

} // namespace Longpath::DigiFrequenzen
