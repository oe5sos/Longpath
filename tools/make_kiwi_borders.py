#!/usr/bin/env python3
"""Erzeugt src/gui/widgets/KiwiWorldBorders.cpp aus den Laendergrenzen, die
schon beim Contestprogramm liegen (Natural Earth 1:110m admin_0_countries,
Public Domain). Aufruf:
python3 -I tools/make_kiwi_borders.py \\
  third_party/contestprogramm/resources/geo/country_borders_110m.dat \\
  > src/gui/widgets/KiwiWorldBorders.cpp"""
import sys

print('''// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen. Laendergrenzen fuer die Kiwi-Weltkarte.
//
// DATEN: Natural Earth, 1:110 Mio. "admin_0_countries", Public Domain
// -- https://www.naturalearthdata.com/about/terms-of-use/. Uebernommen aus
// third_party/contestprogramm/resources/geo/country_borders_110m.dat (dort
// steht die Herkunft), Laenge und Breite je mal 100. Erzeugt von
// tools/make_kiwi_borders.py -- die Datei nicht von Hand aendern.
//
//   2026-11-28 -- Neu angelegt.

#include "gui/widgets/KiwiWorldBorders.h"

namespace Longpath {

namespace {
// Je Ring: Punktzahl n, dann n mal (Laenge*100, Breite*100).
const short kBorders[] = {''')
rings = 0
for line in open(sys.argv[1], encoding='utf-8'):
    line = line.strip()
    if not line or line.startswith('#'):
        continue
    if '\t' in line:
        line = line.split('\t', 1)[1]
    pts = []
    for pair in line.split():
        parts = pair.split(',')
        if len(parts) != 2:
            continue
        q = (round(float(parts[0]) * 100), round(float(parts[1]) * 100))
        if not pts or pts[-1] != q:
            pts.append(q)
    if len(pts) < 2:
        continue
    rings += 1
    print(str(len(pts)) + ',' + ','.join(f'{a},{b}' for a, b in pts) + ',')
print('''};
} // namespace

const short* kiwiBorderData() { return kBorders; }
int kiwiBorderRingCount() { return %d; }

} // namespace Longpath''' % rings)
