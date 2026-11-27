#!/usr/bin/env python3
"""Erzeugt src/gui/widgets/KiwiWorldCoast.cpp aus Natural Earth ne_110m_land.geojson
(Public Domain). Aufruf: python3 -I tools/make_kiwi_coast.py land.geojson > KiwiWorldCoast.cpp
Das Kopfstueck der erzeugten Datei steht in der Datei selbst."""
import json, sys
d = json.load(open(sys.argv[1]))
rings = []
for f in d['features']:
    g = f['geometry']
    polys = [g['coordinates']] if g['type'] == 'Polygon' else g['coordinates']
    for p in polys:
        rings.append(p[0])
for r in rings:
    pts = []
    for lon, lat in r:
        q = (round(lon * 10), round(lat * 10))
        if not pts or pts[-1] != q:
            pts.append(q)
    print(str(len(pts)) + "," + ",".join(f"{a},{b}" for a, b in pts) + ",")
