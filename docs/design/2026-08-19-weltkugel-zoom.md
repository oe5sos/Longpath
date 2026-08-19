# Die Weltkugel näher heranholen — was geht, was es kostet

Frage des Betreibers (2026-08-19): „die Weltkugel würde ich mit Zoom
machen, wie bei Google Earth. welche Möglichkeiten gibt es hier?"

## Zuerst: Zoom gibt es schon

Vor dem Bauen gesucht, wie immer. `GlobeWidget` (969 Zeilen, NereusSDR-eigen) kann heute:

* **Zoomen** — Mausrad und `zoomBy()`, bis 2026-08-19 fest begrenzt auf
  0,6× bis 6×
* **Drehen** durch Ziehen (`m_viewLat` / `m_viewLon`, Breite auf ±85°
  begrenzt)
* Tag-Nacht-Grenze, Großkreis mit Antennen-Öffnungswinkel, Punkte aus
  dem Logbuch, langsames Eigendrehen, `lookAlongBearing()` mit Animation
* `resetView()` zurück auf Anfang

Dazu gibt es **`FlatMapWidget`** (1007 Zeilen): flache Karte,
Zoom **1× bis 12×**, Maidenhead-Raster, das mit dem Zoom feiner wird,
Klick zum Identifizieren. `QsoMapWindow` schaltet zwischen beiden um.

**Die Frage ist also nicht „wie bekommen wir Zoom", sondern „warum
fühlt er sich nicht wie Google Earth an".** Dafür gibt es zwei Gründe,
und nur einer davon ist der Zoom.

## Grund 1: die Kamera zoomte zur Mitte

Google Earth zoomt dorthin, **wo der Zeiger steht**. Wir zoomten auf die
Scheibenmitte — wer eine Insel am Rand vergrößern wollte, musste erst
hindrehen, dann zoomen, dann nachkorrigieren.

**Erledigt am selben Tag.** Die Umkehrung der orthographischen
Projektion (Bildpunkt → Ort) fehlte; sie ist jetzt da
(`GlobeWidget::unproject`, Snyder-Formeln, 14 Testfälle über den
Rundgang Ort → Bildpunkt → Ort). Beim Hineinzoomen wandert die Kamera
ein Drittel des Weges zu dem Ort unter dem Zeiger.

*Ein Drittel und nicht der ganze Weg:* den Punkt exakt unter dem Zeiger
festzunageln erfordert auf einer Kugel eine Drehung um die Sehachse, und
die verdreht den Horizont. So bleibt Nord oben, und es fühlt sich an wie
Hinfliegen.

## Grund 2: die Textur, nicht die Zoomstufe

**Das ist die eigentliche Grenze.** Unsere Weltbilder kommen von der
NASA (Blue Marble, `eoimages.gsfc.nasa.gov`, bei Bedarf geladen und
zwischengespeichert):

| Bild | Größe | Auflösung am Äquator |
| --- | --- | --- |
| klein | 2048 × 1024 | ≈ 0,18° ≈ **20 km** je Bildpunkt |
| groß | 5400 × 2700 | ≈ 0,067° ≈ **7,4 km** je Bildpunkt |

Bei 6× auf einer 300-Bildpunkt-Kugel tastet man bereits **unterhalb** der
Texturauflösung ab. Jede weitere Zoomstufe vergrößert Matsch.

Google Earth hat dieses Problem nicht, weil es **Kacheln nachlädt**: je
näher, desto mehr Bilddaten, bis unter einen Meter.

**Teilweise erledigt:** die Zoom-Decke hängt jetzt an der geladenen
Textur statt fest bei 6× zu stehen — ohne Textur 6×, mit dem großen Blue
Marble rund 16×, hart begrenzt auf 24×. Das holt heraus, was in den
vorhandenen Bildern steckt, und nicht mehr.

## Die Möglichkeiten, von billig nach teuer

### A. Noch größere Offline-Textur *(klein)*

Die NASA bietet Blue Marble auch als **21600 × 10800** (≈ 1,8 km je
Bildpunkt, rund 100 MB als JPEG). Als dritter Kandidat neben klein und
groß, nur auf Wunsch geladen.

* **Gewinn:** viermal feiner als heute, also Küstenlinien und große
  Inseln statt Farbflächen. Keine Netzabhängigkeit im Betrieb.
* **Kosten:** Arbeitsspeicher. Schon die 5400er ist entpackt 58 MB (der
  Grund, warum `WorldTexture` sie zwischen Kugel und flacher Karte
  teilt); die 21600er wären etwa **933 MB** entpackt — das geht so
  nicht. Man müsste sie **kacheln**: nur den sichtbaren Ausschnitt in
  der hohen Auflösung halten, den Rest grob.
* **Aufwand:** mittel, weil das Kacheln dazugehört. Ohne Kacheln: nein.

### B. Umschalten auf die flache Karte, wenn es eng wird *(klein)*

Google Earth wird beim Hineinzoomen flach — aus gutem Grund: eine Kugel,
von der man nur noch ein Zwanzigstel sieht, ist eine schlechte flache
Karte. **Wir haben beide Ansichten schon**, `QsoMapWindow` schaltet
bereits zwischen ihnen um.

* **Gewinn:** Der Übergang „immer näher" ohne neue Technik, mit dem
  Maidenhead-Raster der flachen Karte, das ohnehin mit dem Zoom feiner
  wird.
* **Kosten:** ein Umschaltpunkt, an dem das Bild springt. Sauber gelöst
  mit einer kurzen Überblendung.
* **Aufwand:** klein. **Mein Vorschlag als nächster Schritt.**

### C. Kachel-Streaming wie Zeus Link *(mittel bis groß, Entscheidung nötig)*

Zeus zeigt in seiner Panadapter-Kachel eine Leaflet-Karte mit
Esri-Satellitenbild und OpenFreeMap. Das ist der echte
Google-Earth-Weg.

* **Gewinn:** Auflösung praktisch unbegrenzt, Straßen, Orte, Beschriftung.
* **Kosten, und die sind keine Kleinigkeit:**
  - **Netz im Betrieb.** Wer im Portabelbetrieb ohne Verbindung sitzt,
    sieht graue Kacheln, wo heute eine Kugel steht.
  - **Nutzungsbedingungen.** OSM-Kacheln haben eine Nutzungsrichtlinie
    mit Begrenzungen, Esri-Bilder verlangen Namensnennung. Beides ist
    eine Frage an dich, nicht an mich.
  - **Zwischenspeicher auf der Platte** samt Verfallsregeln.
  - **Umprojektion.** Kacheln sind Web-Mercator, unsere Kugel ist
    orthographisch — für die Kugel muss jede Kachel je Bild umgerechnet
    werden. Für die *flache* Karte entfällt das: dort passen Kacheln
    direkt.
* **Aufwand:** groß für die Kugel, mittel für die flache Karte allein.

### D. Kleinigkeiten, die viel ausmachen *(je winzig)*

* **Doppelklick auf einen Ort → dorthin fliegen** (animiert, wie
  `lookAlongBearing` es für Peilungen schon tut).
* **Knöpfe „+" und „−"** — im Quelltext steht selbst der Satz „a wheel is
  not an affordance": nichts auf dem Bild sagt, dass die Kugel zoomt.
* **Tastatur**: `+`/`−`, Pfeiltasten zum Drehen, `0` für zurück.
* **Maßstabsleiste**, damit „wie nah bin ich" eine Zahl hat.
* **Zoom auf die Gegenstation** mit einem Knopf, wenn ein Ziel gesetzt
  ist — der häufigste Wunsch im Betrieb.

## Nachtrag: D ist gebaut

Am selben Tag umgesetzt, mit 23 Testfällen:

* **Doppelklick auf die Kugel → dorthin fliegen** (Kamera und Zoom
  animiert). Der Doppelklick *neben* die Kugel setzt weiter zurück.
* **Tastatur**: `+` / `−` zoomen, Pfeiltasten drehen, `0` setzt zurück,
  `T` fliegt zur Gegenstation (ohne gesetztes Ziel absichtlich still).
* **Rechtsklick-Menü**: Zoom in / Zoom out / Fly to station / Reset view.
* Die Animation zieht jetzt **Breite und Zoom** mit, nicht nur die Länge —
  vorher sprang beides, was beim Hinfliegen wie ein Bildfehler aussieht.

**Und ein Fund, der aus einem falschen Test von mir kam:** ab etwa 1,15×
füllt die Scheibe das ganze Fenster — „neben der Kugel" gibt es dann
nicht mehr. Der Rückweg hing genau an dieser Stelle und wäre unerreichbar
gewesen, sobald man hineingezoomt hat. Deshalb liegt er jetzt zusätzlich
auf der Taste `0` und im Rechtsklick-Menü. Ein Testfall hält den Grund
fest, damit niemand die Doppelklick-Geste später „aufräumt".

## Was ich empfehle

1. ~~**D zuerst**~~ — **erledigt**, siehe Nachtrag.
2. **B danach** — nutzt, was schon gebaut ist, und bringt den
   Google-Earth-Eindruck ohne neue Abhängigkeit.
3. **A nur mit Kacheln**, sonst ist der Speicherbedarf nicht vertretbar.
4. **C nur, wenn du eine Netzabhängigkeit im Betrieb willst** — und dann
   zuerst für die flache Karte, wo die Umprojektion entfällt.

---

## Modification history (NereusSDR)

* 2026-08-19 — angelegt von Martin Fischer, KI-gestützt über Anthropic
  Claude (Cowork).
