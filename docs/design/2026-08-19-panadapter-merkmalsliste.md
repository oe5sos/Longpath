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

## Korrektur vom selben Tag: der Bandplan war nie eine Lücke

**Diese Liste hatte den Bandplan als Empfehlung Nummer 1. Das war
falsch.** Er ist vollständig portiert und läuft:

* `src/models/BandPlanManager.{h,cpp}` und `BandPlan.h`
* fünf Pläne als JSON in `resources/bandplans/`, im `qrc` eingetragen
* `SpectrumWidget::drawBandPlan()`, in **beiden** Malwegen gerufen
  (CPU bei :3576, GPU bei :8994)
* Schriftgröße persistiert unter `BandPlanFontSize`, Vorgabe 6 pt

Der Vergleich hat ihn übersehen, weil er nach Methodennamen sucht:
AetherSDR heißt der Schalter `setShowBandPlan(bool)`, bei uns
`setBandPlanFontSize(int)` mit 0 = aus. Zwei Namen, ein Merkmal — genau
der Fehlalarm, vor dem der Abschnitt „Namensunterschiede" unten warnt,
und ich bin ihm selbst aufgesessen.

**Was wirklich fehlt, ist etwas anderes und Billigeres:** eine
Bedienfläche. Es gibt keinen Schalter, keine Plan-Auswahl und keine
Schriftgrößen-Einstellung — weder im Setup noch in der Overlay-Leiste.
Der Bandplan ist damit derselbe Fall wie die Modusgruppen am Vortag:
gebaut, geprüft, an keiner Fläche.

Die Lehre für den Rest dieser Liste: **jeder Punkt gehört im Baum
gesucht, bevor er gebaut wird.** Ein Merkmalsvergleich über
Methodennamen findet Lücken, die keine sind, und übersieht Lücken, die
zwischen zwei Namen liegen.

---

## Was AetherSDR hat und wir nicht

### Lohnt sich — mein Vorschlag zum Holen

| Merkmal | Methoden | Warum |
| --- | --- | --- |
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

**Zuerst: den Bandplan bedienbar machen.** Er ist da und läuft, ihm
fehlt nur der Schalter. Das ist der billigste sichtbare Gewinn dieser
ganzen Liste.

**Danach sieben Merkmale holen, in dieser Reihenfolge:**

1. Squelch-Linie mit Automatik — direkter Nutzen im Betrieb
2. Abstimmhilfen und Einfachklick — Bedienung
3. S-Verlauf — eigener Streifen, sichtbar aufwendiger
4. TX im Wasserfall
5. SWR-Kurve (Controller ist da)
6. Erweiterter Durchlass
7. Ausbreitungsvorhersage — hängt an externen Daten, deshalb zuletzt

Und jeder dieser sieben wird **erst im Baum gesucht**, bevor er gebaut
wird — siehe die Korrektur oben.

Jedes davon ist ein eigener Port mit eigener Provenance-Zeile, jedes für
sich prüfbar, jedes einzeln zurücknehmbar. Zusammen sind es Wochen — als
Klon wäre es dieselbe Arbeit, nur ohne Zwischenstände und mit dem
Verlust unserer 120 eigenen Merkmale.

---

*Erhoben am 2026-08-19 durch Vergleich der öffentlichen Setter beider
`SpectrumWidget`-Header. Die Methodenzählung ist ein Näherungsmaß für
Merkmale — sie überschätzt dort, wo ein Merkmal mehrere Setter hat, und
unterschätzt dort, wo einer mehrere trägt.*
