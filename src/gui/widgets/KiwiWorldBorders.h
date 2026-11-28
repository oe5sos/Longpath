// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen. Laendergrenzen fuer die Kiwi-Weltkarte (Daten und
// Herkunft: KiwiWorldBorders.cpp).
//
//   2026-11-28 -- Neu angelegt.

#pragma once

namespace Longpath {

// Je Ring: Punktzahl n, dann n mal (Laenge*100, Breite*100).
const short* kiwiBorderData();
int kiwiBorderRingCount();

} // namespace Longpath
