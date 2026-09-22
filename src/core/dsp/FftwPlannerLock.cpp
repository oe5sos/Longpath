// =================================================================
// src/core/dsp/FftwPlannerLock.cpp  (Longpath)
// =================================================================
//
// Ported from AetherSDR source:
//   src/core/dsp/FftwPlannerLock.cpp [@7f68dda0], original licence from
//   AetherSDR source is included below
//
// =================================================================
// Modification history (Longpath):
//   2026-09-22 — Reimplemented for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude. Namespace change.
// =================================================================

/*  FftwPlannerLock.cpp

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

#include "core/dsp/FftwPlannerLock.h"

namespace Longpath {

namespace {

// Funktionslokal und damit garantiert vor dem ersten Gebrauch gebaut —
// eine Sperre, die erst nach dem ersten Planer entsteht, sperrt nichts.
// Absichtlich nie zerstoert: FFTW-Nutzer koennen in Destruktoren
// statischer Objekte liegen, und ein Mutex, der vor seinem letzten
// Benutzer stirbt, ist schlimmer als keiner.
std::mutex& doubleMutex()
{
    static std::mutex* m = new std::mutex();
    return *m;
}

std::mutex& floatMutex()
{
    static std::mutex* m = new std::mutex();
    return *m;
}

} // namespace

std::unique_lock<std::mutex> fftwPlannerLock()
{
    return std::unique_lock<std::mutex>(doubleMutex());
}

std::mutex& fftwPlannerMutex()
{
    return doubleMutex();
}

std::unique_lock<std::mutex> fftwfPlannerLock()
{
    return std::unique_lock<std::mutex>(floatMutex());
}

std::mutex& fftwfPlannerMutex()
{
    return floatMutex();
}

} // namespace Longpath
