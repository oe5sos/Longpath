// =================================================================
// src/core/dsp/FftwPlannerLock.h  (Longpath)
// =================================================================
//
// Ported from AetherSDR source:
//   src/core/dsp/FftwPlannerLock.h [@7f68dda0], original licence from
//   AetherSDR source is included below
//
// =================================================================
// Modification history (Longpath):
//   2026-09-22 — Reimplemented for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude. Namespace change;
//                 the call-site inventory in the comments is Longpath's
//                 own (FFTEngine, WidebandFftEngine, RxChannel, WDSP).
// =================================================================

/*  FftwPlannerLock.h

This file is part of AetherSDR.

Copyright (C) 2024-2026 AetherSDR Contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <mutex>

namespace Longpath {

// DIE SPERREN GEHOEREN FFTW, NICHT EINER EINZELNEN KLASSE.
//
// Jede Genauigkeit hat ihren eigenen, prozessweiten Planer, und keiner
// von beiden ist threadsicher. Der Wissensspeicher (wisdom) der doppelten
// Genauigkeit ist ebenfalls prozessweit, und dasselbe gilt fuer die
// Paarung fftw_alloc_* / fftw_free ueber Threads hinweg. Jeder Plan, jedes
// Verwerfen eines Plans, jeder Import/Export von Wissen und jede
// FFTW-Belegung muss mit der Sperre SEINER Genauigkeit serialisiert
// werden — nicht mit einem Mutex, den sich jedes Teilsystem selbst haelt.
// Zwei Mutexe ueber einem Planer serialisieren nichts.
//
// fftw_execute() ist threadsicher und darf NICHT serialisiert werden: es
// liegt auf dem Echtzeitpfad.
//
// Was Longpath hier zu serialisieren hat:
//
//   einfache Genauigkeit (fftwf_):
//     FFTEngine          — ein Panadapter je Scheibe, jeder auf seinem
//                          eigenen Faden; zwei Scheiben, die gleichzeitig
//                          die FFT-Groesse wechseln, planen gleichzeitig.
//     WidebandFftEngine  — das Breitbandbild, eigener Faden.
//   doppelte Genauigkeit (fftw_):
//     RxChannel          — die Impulsantwort-Messung (fftw_plan_dft_1d).
//     WDSP               — OpenChannel/CloseChannel planen intern, und
//                          WDSPwisdom() laeuft bei uns auf einem eigenen
//                          Faden (WdspEngine::initialize), waehrend die
//                          Oberflaeche weiterlaeuft.
//
// Die Kommentare an FFTEngine.cpp:331 und WidebandFftEngine.cpp:21
// behaupteten, FFTW_ESTIMATE weiche „dem globalen FFTW-Mutex" aus. Das
// stimmt nicht: ESTIMATE misst nicht, aber es fasst denselben globalen
// Planerzustand an wie MEASURE. Der Unterschied ist die Dauer, nicht die
// Sicherheit.
//
// FFTWs EIGENE Antwort, und warum sie keine ist: fftw_make_planner_thread_safe()
// deckt den Planer ab, braucht aber BEIDE Schreibweisen (fftw_ und fftwf_
// sind unabhaengige Planer) und deckt WEDER den Wissensspeicher NOCH die
// malloc/free-Kante ab. Auf macOS und Linux liegt sie ausserdem in
// libfftw3_threads / libfftw3f_threads, die hier kein Ziel verlinkt.
[[nodiscard]] std::unique_lock<std::mutex> fftwPlannerLock();
[[nodiscard]] std::mutex& fftwPlannerMutex();

[[nodiscard]] std::unique_lock<std::mutex> fftwfPlannerLock();
[[nodiscard]] std::mutex& fftwfPlannerMutex();

} // namespace Longpath
