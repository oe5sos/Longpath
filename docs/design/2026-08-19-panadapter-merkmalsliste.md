# Panadapter: AetherSDR gegen NereusSDR, Merkmal für Merkmal

Antwort auf „den panadapter von aether klonen und einbauen" (Betreiber,
2026-08-18). **Ein Klon ist der falsche Weg**, und die Zahlen sagen
warum.

## Die Ausgangslage

| | AetherSDR | NereusSDR |
| --- | --- | --- |
| Zeilen `SpectrumWidget` | 19.600 | 9.700 |
| Öffentliche Setter | **209** | **215** |
| Nur dort vorhanden | ~90 | **~120** |

**Wir sind nicht kleiner, nur anders.** Der Zeilenunterschied kommt aus
Dingen, die uns nicht betreffen — KiwiSDR-Anbindung, FlexRadio-Slices,
MQTT. Nach Methoden gezählt haben *wir* mehr.

Ein Klon würde also 120 eigene Merkmale gegen 90 fremde tauschen. Was
Sinn ergibt, ist die Gegenrichtung: aus den 90 die holen, die wir
wirklich wollen.

## Was AetherSDR hat und wir nicht

### Lohnt sich — mein Vorschlag zum Holen

| Merkmal | Methoden | Warum |
| --- | --- | --- |
| **Bandplan-Überlagerung** | `setShowBandPlan`, `setBandPlanShowSpots`, `bandPlanFontSize` | Farbige Bandsegmente im Spektrum. Das auffälligste fehlende Stück — man sieht sofort, wo CW, wo SSB, wo Bake. |
| **S-Verlauf** | `setShowSHistory`, `setSHistoryMarkers`, `setSHistorySnapToStep`, `setShowSHistoryQrm` | Signalstärke über Zeit als eigener Streifen. Zeigt QSB und QRM, die im Wasserfall untergehen. |
| **Abstimmhilfen** | `setShowTuneGuides`, `setSingleClickTune`, `setSpectrumCursor` | Führungslinien beim Abstimmen; Einfachklick statt Doppelklick. |
| **Squelch-Linie** | `setSquelchLine`, `setAutoSquelchEnable`, `setAutoSqlMarginDb` | Die Schwelle im Bild statt nur als Zahl. Automatik mit Abstand zum Rauschboden. |
| **Ausbreitungsvorhersage** | `setPropForecast`, `setPropForecastVisible` | Erwartete Reichweite im Panadapter. |
| **TX im Wasserfall** | `setShowTxInWaterfall`, `setTxWaterfallSlice` | Das eigene Sendesignal im Verlauf sehen. |
| **Erweiterter Durchlass** | `setExtendedPassband`, `setExtendedFrequencyLine` | |
| **SWR-Kurve** | `setSwrSweepPoints` | Wir haben den `SwrSweepController`, aber keine Darstellung im Panadapter. |

### Braucht erst Phase 3F

| Merkmal | Warum es wartet |
| --- | --- |
| Scheiben-Überlagerung (`setSliceOverlay*`, 7 Methoden) | Setzt mehrere Scheiben auf einem Panadapter voraus. |
| Scheiben-Kopplung (`setSliceLinkPairs`, `setSplitPair`) | Dasselbe. |
| `setCenterLockSliceId`, `setHasTxSlice` | Dasselbe. |

### Betrifft uns nicht

KiwiSDR (9 Methoden), MQTT, `setRadioOwnsDbmScale` und die
FlexRadio-seitige Schwarzwert-Automatik (3) — das sind Anbindungen an
Geräte und Dienste, die NereusSDR nicht spricht. **15 der 90 fallen
damit weg, ohne dass etwas fehlt.**

### Namensunterschiede, kein Merkmalsunterschied

`setFftLineColor` / `setFftFillColor` / `setFftHeatMap` heißen bei uns
`setLineWidth` / `setFillColor` / `setHeatmapEnabled`. `setTnfMarkers`
ist unser `setNotchMarkers`. Beim Vergleich leicht für eine Lücke zu
halten — sind aber acht Fehlalarme.

## Was wir haben und AetherSDR nicht

Der Vollständigkeit halber, weil es die Richtung des Handels bestimmt:

* **Spitzenblasen** (11 Methoden) — `setPeakBlobs*`, benannte Spitzen mit
  Halten und Abfallen
* **Rauschboden** (9) — Schnellangriff, eigene Farben, Gitterkopplung
* **Notch-Darstellung** (6) — sichtbare Kerben, Mindestbreite, Auswahl
* **Wasserfall-Feinsteuerung** (14) — Detektor, Mittelung, AGC-Versatz,
  Zeitstempel, Verlaufsdauer
* **Klarheit** (3) — `setClarity*`, die adaptive Abstimmung aus 3G-9c
* **Breitband** (`setWidebandBins`) — der P2-Breitbandpfad aus 3F-F
* **IMD-Messung** (2) — die PureSignal-Zweitonanzeige

## Empfehlung

**Acht Merkmale holen, in dieser Reihenfolge:**

1. Bandplan-Überlagerung — die größte sichtbare Lücke
2. Squelch-Linie mit Automatik — direkter Nutzen im Betrieb
3. Abstimmhilfen und Einfachklick — Bedienung
4. S-Verlauf — eigener Streifen, sichtbar aufwendiger
5. TX im Wasserfall
6. SWR-Kurve (Controller ist da)
7. Erweiterter Durchlass
8. Ausbreitungsvorhersage — hängt an externen Daten, deshalb zuletzt

Jedes davon ist ein eigener Port mit eigener Provenance-Zeile, jedes für
sich prüfbar, jedes einzeln zurücknehmbar. Zusammen sind es Wochen — als
Klon wäre es dieselbe Arbeit, nur ohne Zwischenstände und mit dem
Verlust unserer 120 eigenen Merkmale.

---

*Erhoben am 2026-08-19 durch Vergleich der öffentlichen Setter beider
`SpectrumWidget`-Header. Die Methodenzählung ist ein Näherungsmaß für
Merkmale — sie überschätzt dort, wo ein Merkmal mehrere Setter hat, und
unterschätzt dort, wo einer mehrere trägt.*
