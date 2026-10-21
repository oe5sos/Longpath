// =================================================================
// src/core/Ft8Subtract.h  (Longpath)
// =================================================================
//
// Ein dekodiertes FT8-Signal aus der Aufnahme herausrechnen.
//
// WOFUER: WSJT-X und JTDX dekodieren ein Zeitfenster MEHRFACH und
// ziehen nach jedem Durchgang die gefundenen Signale ab. Darunter
// werden schwaechere Stationen ueberhaupt erst auffindbar. Das ist
// der Unterschied zwischen zwoelf und fuenfundzwanzig Stationen je
// Durchgang -- und `ft8_lib`, unser Dekoder, kann es von sich aus
// nicht (nachgesehen: keine Subtraktionsfunktion).
//
// Betreiber am 2026-10-21, nach dem ersten Durchgang an der Antenne
// und der Frage nach anderen Programmen: „ok, dann mshv".
//
// HERKUNFT: Das Verfahren stammt aus MSHV (LZ2HV), GPLv3, siehe die
// Kopfzeile in Ft8Subtract.cpp und docs/attribution/MSHV-PROVENANCE.md.
// MSHV wiederum hat es von WSJT-X (K1JT und die WSJT-Gruppe).
//
// =================================================================
// Modification history (Longpath):
//   2026-10-21 — Ported from MSHV 2.76.7 for Longpath by Martin
//                 Fischer (OE5SOS), AI-assisted via Anthropic Claude
//                 (Claude Code).
// =================================================================

#pragma once

#include <cstdint>

namespace Longpath {

class Ft8Subtract {
public:
    /// Abtastrate, mit der gerechnet wird -- dieselbe wie im Dekoder.
    static constexpr int kSampleRate = 12000;

    /// Proben je Symbol bei 12 kHz. 1920 = 0,16 s, die FT8-Symbollaenge.
    /// From MSHV decoderft8.cpp:336 [2.76.7] — `int nsps=1920;`
    static constexpr int kSamplesPerSymbol = 1920;

    /// Symbole eines FT8-Durchgangs (7 Costas + 58 Daten + 7 + 7).
    /// From MSHV decoderft8.cpp:335 [2.76.7] — `int nsym=79;`
    static constexpr int kSymbols = 79;

    /// Ein dekodiertes Signal aus `dd` herausrechnen.
    ///
    /// `dd`      Monoproben bei kSampleRate, IN PLACE veraendert.
    /// `toene`   die 79 Symbole (0..7), wie sie `ft8_encode` liefert.
    /// `f0Hz`    die Tonlage des Signals im Durchlassbereich.
    /// `dtSek`   sein Zeitversatz im Fenster, wie der Dekoder ihn meldet.
    ///
    /// Gibt `false` zurueck, wenn die Groessen nicht passen (zu kurze
    /// Aufnahme, falsche Symbolzahl) -- dann bleibt `dd` unberuehrt.
    /// Falsch abziehen waere schlimmer als nicht abziehen: es erzeugt
    /// ein Stoersignal, wo vorher ein echtes stand.
    static bool zieheAb(float* dd, int ddCount,
                        const uint8_t* toene, int toeneAnzahl,
                        double f0Hz, double dtSekunden);
};

} // namespace Longpath
