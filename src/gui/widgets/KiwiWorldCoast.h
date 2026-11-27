// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen. Die Kuestenlinie der Kiwi-Weltkarte als Zahlenfeld;
// Herkunft und Lizenz (Natural Earth, Public Domain) stehen in
// KiwiWorldCoast.cpp und docs/attribution/KIWI-WORLD-COAST.md.
//
//   2026-11-25 -- Neu angelegt.

#pragma once

namespace Longpath {

// Je Ring: Punktzahl n, dann n mal (Laenge*10, Breite*10), Grad mal zehn.
const short* kiwiCoastData();
int kiwiCoastShortCount();
int kiwiCoastRingCount();

} // namespace Longpath
