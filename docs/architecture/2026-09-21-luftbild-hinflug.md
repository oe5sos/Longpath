# Luftbild und Hinflug auf der QSO-Karte

**Stand:** 21. September 2026 · **Anlass:** Benchmark-Wunsch des Betreibers
(„die 2D-Animation nach Eingabe des Rufzeichens"): eine Station eingeben und
sehen, *wo* sie ist — nicht als Punkt auf einer Weltkarte, sondern als Ort.

## Was gebaut wurde

1. **`GibsTileLayer`** (`src/gui/widgets/`): Kacheln von NASA GIBS im
   Raster EPSG:4326 — Blue Marble (Stufen 0–7, 500 m) und Landsat WELD
   (Stufen 0–11, 31 m, Jahreskomposit). Frei, ohne Schlüssel; Vermerk
   verpflichtend (`attribution()`, wird gezeichnet, solange Kacheln
   sichtbar sind). Warteschlange mit sechs gleichzeitigen Abrufen,
   `QNetworkDiskCache` (256 MB) unter `CacheLocation/gibs-tiles`,
   Speichercache 600 Kacheln, Fehlschläge werden 60 s nicht wiederholt.
   Netz abschaltbar (`setNetworkEnabled`), dann nur Speicher/Platte.

2. **`FlatMapWidget`**: zeichnet die Kacheln ab `kImageryFromZoom` (2x)
   über das Weltbild, unter der Nacht. Fehlt eine Kachel, springt die
   nächste gröbere aus dem Speicher ein (bis vier Stufen). Der Zoom endet
   mit Luftbild erst bei der Landsat-Auflösung (`maxZoom()`, aus der
   Fensterbreite gerechnet), sonst wie bisher bei 12x. `flyTo()` fliegt
   animiert (Mitte linear mit InOutCubic, Zoom logarithmisch mit Bogen
   nach außen, `flightZoomAt()`); `setFocusStation()` zeichnet Ring,
   Rufzeichen und Großkreis vom eigenen Standort.

3. **`QsoMapWindow`**: Feld „Fly to call…" (Enter), Haken „Imagery";
   `lookupAndFly()` sucht zuerst im Log (Locator), fragt dann QRZ, fällt
   auf die Landesmitte (`PositionFallback`) zurück; `flyToStation()` schaltet
   auf die flache Karte, fliegt, und schreibt Rufzeichen, Name/Ort,
   Entfernung und Peilung in die Stationskarte.

4. **Logbuch → Karte:** `QsoDetailPane::stationLocated` feuert nach einem
   Lookup mit Koordinaten oder Locator; `LogbookWindow` fliegt die offene
   Karte dorthin.

## Warum EPSG:4326 und nicht Web-Mercator

`FlatMapWidget::project()` ist naiv (Länge nach rechts, Breite nach unten).
GIBS liefert genau dieses Raster; jede Kachel fällt ohne Umprojektion auf
ihren Platz. Das Raster: 512 px, Ursprung −180/+90, Stufe *z* deckt
288°/2^z je Kachel — Stufe 0 zwei Kacheln für die Welt, Stufe 11
2560 × 1280. Kacheln am rechten und unteren Rand ragen über die Karte
hinaus; gezeichnet wird nur der Teil innerhalb.

## Prüfung

- `tst_gibs_tile_geometry` — Stufenwahl, Kachelindizes (Österreich auf
  Stufe 7 = 8 Kacheln), Kanten, Adressen, Speicher ohne Netz.
- `tst_flat_map_flyto` — Sprung und animierter Flug landen gleich, Bogen
  nach außen, Zoomdecke folgt dem Luftbild, Zielstation und Kacheln aus
  dem Speicher werden gezeichnet.
- `tst_qso_map_flyto_window` — Rufzeichen tippen, Enter, Flug, Karte;
  mit `LONGPATH_GRAB_DIR` zusätzlich echte Kacheln und ein PNG.

## Offen

- Landsat WELD ist ein Komposit von 2000; neuere globale Luftbilder gibt
  es frei nur mit Bedingungen (USGS nur USA, Esri/OSM mit Nutzungsregeln).
- Die Kugel kennt den Flug nicht; sie schaltet auf die flache Karte um.
- Nächste Stufen aus derselben Liste: QRZ-Logbuch-Abgleich (Pull),
  Satelliten in Sicht zum QSO-Zeitpunkt, Live-Schichten.
