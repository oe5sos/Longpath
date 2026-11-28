# Kiwi-Weltkarte: Küstenlinie

- **Datei:** `src/gui/widgets/KiwiWorldCoast.cpp` (erzeugt, nicht von Hand ändern)
- **Quelle:** Natural Earth, Datensatz `ne_110m_land` (Land, 1:110 Mio.),
  <https://www.naturalearthdata.com/>, geladen am 2026-11-25 aus
  <https://github.com/nvkelso/natural-earth-vector> (`geojson/ne_110m_land.geojson`).
- **Lizenz:** Public Domain. Natural Earth: „No permission is needed to use
  Natural Earth. Crediting the authors is unnecessary." (Nutzungsbedingungen,
  <https://www.naturalearthdata.com/about/terms-of-use/>). Der Hinweis hier ist
  Herkunftsangabe, keine Pflicht.
- **Bearbeitung:** nur äußere Ringe, auf 0,1° gerundet, doppelte
  Folgepunkte entfernt. Erzeuger: `tools/make_kiwi_coast.py`.
- Ausgelassen: Inseln kleiner als das 110-m-Raster, Binnenmeere als Löcher.

## Ländergrenzen (2026-11-28)

- **Datei:** `src/gui/widgets/KiwiWorldBorders.cpp` (erzeugt, nicht von Hand ändern)
- **Quelle:** Natural Earth 1:110 Mio., `admin_0_countries`, Public Domain —
  übernommen aus
  `third_party/contestprogramm/resources/geo/country_borders_110m.dat`
  (dort vereinfacht, Douglas-Peucker 0,03°).
- **Erzeuger:** `tools/make_kiwi_borders.py`.

