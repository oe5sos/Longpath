#pragma once

// Ein Audiogerät öffnen — aber nicht für immer.
//
// `IAudioBus::open()` kann ohne Zeitlimit stehenbleiben. Bei PortAudio heißt
// das `Pa_OpenStream`, und das greift auf CoreAudio zu: Geräte aufzählen, HAL,
// und bei einem Aufnahmegerät die Mikrofon-Freigabe. Hängt eine dieser Stufen
// — offene Berechtigungsfrage, Schnittstelle von einem anderen Programm
// exklusiv gehalten, träger Treiber —, dann kommt der Aufruf nie zurück.
//
// Das wäre halb so schlimm, läge er nicht auf dem Oberflächen-Faden und in der
// verschachtelten Ereignisschleife von `RadioModel::connectToRadio`. Dort
// friert mit ihm die GESAMTE Oberfläche ein, samt Verbindungsdialog und seinem
// „Abbrechen". Am 2026-10-03 live erlebt; die Untersuchung mit allen Zahlen
// steht in `docs/architecture/2026-10-03-verbindungshaenger-mikrofon.md`.
//
// Hier, und nicht in AudioEngine.cpp, damit es prüfbar ist: der Prüfstand
// `tests/tst_bus_mit_frist.cpp` gibt einen Bus herein, dessen `open()`
// absichtlich stehenbleibt, und sieht nach, ob die Frist greift und wer den
// Bus danach aufräumt.

#include <memory>

#include <QString>

#include "core/IAudioBus.h"

namespace Longpath::Audio {

/// Frist für einen einzelnen Öffnungsversuch.
///
/// Gemessen begründet: am 2026-10-03 brauchte der ganze Audio-Start 20 ms
/// (Mikrofon 18 ms, alle fünf VAX-Busse zusammen unter 1 ms). 5000 ms liegen
/// zwei Zehnerpotenzen darüber und können im gesunden Betrieb nicht auslösen —
/// auch nicht bei einem kalten CoreAudio nach dem Hochfahren.
inline constexpr int kBusOeffnenFristMs = 5000;

/// Öffnet `bus` mit Frist.
///
/// Rückgabe: der offene Bus, oder `nullptr`, wenn das Öffnen scheiterte ODER
/// die Frist ablief. In beiden Fällen ist der Grund geloggt.
///
/// Nach einem Zeitlimit gehört der Bus dem Öffnungsfaden, der ihn aufräumt,
/// sobald `open()` endlich zurückkommt — es bleibt also nichts halb Gebautes
/// im Besitz des Aufrufers liegen, und der Aufrufer darf ihn nicht mehr
/// anfassen.
///
/// WARUM der Aufrufer währenddessen blockiert, statt die Ereignisschleife zu
/// pumpen: dann bliebe die Oberfläche bedienbar, und der Bediener könnte
/// mitten im Verbindungsaufbau ein zweites Mal auf Verbinden tippen — genau
/// die Wiedereintritts-Falle, gegen die `connectToRadio` seinen
/// Fortschrittsdialog modal hält. Die Frist macht aus „für immer eingefroren"
/// ein „einmal kurz stehengeblieben". Das ist der ganze Zweck; mehr
/// Bedienbarkeit wäre hier mehr Risiko.
std::unique_ptr<IAudioBus> oeffneMitFrist(std::unique_ptr<IAudioBus> bus,
                                          const AudioFormat& fmt,
                                          const QString& wofuer,
                                          int fristMs = kBusOeffnenFristMs);

}  // namespace Longpath::Audio
