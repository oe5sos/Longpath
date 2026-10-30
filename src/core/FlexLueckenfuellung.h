#pragma once

// =================================================================
// src/core/FlexLueckenfuellung.h  (Longpath)
// =================================================================
//
// Ported from AetherSDR (https://github.com/aethersdr/AetherSDR),
// GPLv3, primary author Jeremy [KK7GWY]:
//   src/core/PacketLossConcealment.{h,cpp} at d58e2b8a
//     — AudioPlcState, kMaxConcealPackets, applyConcealmentFade().
//
// AetherSDR carries no per-file licence headers, so per
// docs/attribution/HOW-TO-PORT.md rule 6 the citation is at project
// level. Both projects are GPLv3, so the code carries forward under
// the same licence per GPLv3 section 5.
//
// =================================================================
// Warum das hier steht, und warum erst jetzt
// =================================================================
//
// Beim Port des Flex-Tonwegs am 2026-10-18 wurde die Lueckenfuellung
// bewusst NICHT mitgenommen (nachzulesen in AETHERSDR-PORTS.md: „Nicht
// uebernommen: … PacketLossConcealment"). Der Grund war richtig: ohne
// Messung war nicht bekannt, ob ueberhaupt Pakete fehlen, und eine
// Fuellung, die nichts zu fuellen hat, ist nur ein Weg, Fehler zu
// verstecken.
//
// Am 2026-10-29 ist es gemessen (PR #348). An Martins FLEX-8400 ueber
// 2,4-GHz-WLAN fehlen durchgehend 1,2 % der Pakete — auf dem Tonstrom
// 441 von 35 105 in drei Minuten. Der Betreiber hat Kabel
// ausgeschlossen („nur wlan"), damit ist der Verlust eine Tatsache,
// mit der der Tonweg leben muss.
//
// =================================================================
// Was die Fuellung tut, und was sie NICHT tut
// =================================================================
//
// Ein fehlendes Paket ist nicht nur eine Luecke, es ist auch ein
// SPRUNG: ohne Fuellung folgt auf die letzte Probe des einen Pakets
// unmittelbar die erste des naechsten. Die Welle bricht (das Knacken),
// und der Ton laeuft um die fehlende Zeit ZU KURZ — bei 1,2 % Verlust
// gehen auf eine Minute 0,7 Sekunden verloren.
//
// Die Fuellung schiebt die fehlende Zeit als Stille wieder ein, mit
// einer Kosinus-Blende hinunter und wieder hinauf (je ~2 ms). Damit
// stimmt die Zeitachse, und der Sprung wird zu einem kurzen
// Verstummen.
//
// Sie ERFINDET keinen Ton. Wer eine Silbe verliert, hoert die Silbe
// nicht; er hoert statt eines Knacksens eine Pause. Das ist alles —
// und es ist das Ehrlichste, was an dieser Stelle moeglich ist.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Ported to Longpath by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork). Namespace AetherSDR →
//                 Longpath::FlexVita. Arbeitet auf QVector<float>
//                 statt QByteArray, weil der Flex-Tonstrom in
//                 Longpath so durch die Signale geht. Sonst
//                 zeichengetreu: dieselbe Kosinus-Blende, dieselben
//                 48 Blendrahmen, dieselbe Decke von 8 Paketen.
// =================================================================

#include <QVector>

namespace Longpath::FlexVita {

// Der Zustand je Tonstrom. Reine Daten; `fuelleLuecke` aendert sie.
//
// From AetherSDR PacketLossConcealment.h:10-16 [@d58e2b8a] —
// AudioPlcState, Feld fuer Feld.
struct Lueckenzustand {
    int   letzteRahmen{0};    ///< Stereorahmen im letzten guten Paket
    int   offeneLuecken{0};   ///< fehlende Pakete, noch nicht gefuellt
    float randL{0.0f};        ///< letzte ausgegebene Probe, fuer die Blende
    float randR{0.0f};
};

// Decke fuer aufeinanderfolgende gefuellte Pakete.
//
// From AetherSDR PacketLossConcealment.h:20 [@d58e2b8a] —
// kMaxConcealPackets = 8, dort mit „~80 ms at 10 ms/pkt" begruendet.
// An der Flex sind es 128 Rahmen zu 24 kHz, also 5,33 ms je Paket —
// dieselbe 8 deckt hier 43 ms. Die Zahl bleibt die der Vorlage: sie
// ist eine Decke gegen das Verlaengern von Stille, kein Zeitwert.
//
// Darueber hinaus wird NICHT weiter gefuellt. Eine Luecke von einer
// halben Sekunde (gemessen: 594 ms) ist kein Aussetzer mehr, den man
// glatt buegeln darf — da schweigt der Ton, und das soll man hoeren.
constexpr int kMaxGefuellteLuecken = 8;

// ~2 ms Blende bei 24 kHz.
//
// From AetherSDR PacketLossConcealment.cpp:22 [@d58e2b8a] —
// kFadeFrames = 48, dort kommentiert als „~2 ms @ 24 kHz". Die Flex
// liefert ebenfalls 24 kHz (gemessen), die Zahl stimmt unveraendert.
constexpr int kBlendRahmen = 48;

/// Schiebt die fehlende Zeit vor `ton` ein: Blende hinunter vom
/// gemerkten Rand, Stille, Blende hinauf in den neuen Ton.
///
/// Gibt `ton` unveraendert zurueck, wenn die Fuellung aus ist, nichts
/// offen ist, oder es noch kein gutes Paket gab (ohne Vorgaenger gibt
/// es keinen Rand, von dem herunterzublenden waere — Abwesenheit ist
/// keine Aussage). In jedem Fall wird `z` auf den Rand der AUSGABE
/// nachgezogen und `offeneLuecken` auf 0 gestellt.
///
/// `ton` ist Stereo verschraenkt, wie der Flex-Strom ihn liefert.
QVector<float> fuelleLuecke(QVector<float> ton, Lueckenzustand& z,
                            bool an);

}  // namespace Longpath::FlexVita
