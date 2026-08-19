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

## Zweite Korrektur: „Abstimmhilfen" waren drei Setter, einer ist eine Lücke

Bei der Suche vor dem Bauen (2026-08-19) hat sich die Zeile
**Abstimmhilfen** in drei Teile zerlegt, von denen nur einer fehlte:

* `setShowTuneGuides` — **echte Lücke**, gebaut in diesem Commit.
* `setSingleClickTune` — **keine Lücke, eine Vorliebe.** Wir stimmen
  längst beim Einfachklick ab, bedingungslos, portiert aus derselben
  AetherSDR-Stelle: `SpectrumWidget.cpp` in `mouseReleaseEvent`, 4
  Bildpunkte Schwelle. AetherSDRs Schalter steht auf `false` als
  Vorgabe, dort ist der Doppelklick der Normalfall. Ihn zu holen heißt,
  eine Option zu bauen, die unser Verhalten verschlechtert. Nicht
  gebaut; als Betriebsfrage dem Betreiber vorgelegt.
* `setSpectrumCursor` — **kein Merkmal.** Setzt die Mauszeiger-*Form*
  während Zieh-Gesten (`Qt::CrossCursor`, `SizeHorCursor`,
  `ClosedHandCursor`), samt Kommentar über einen macOS-Absturz im
  Zeigerwechsel. Vierter Fehlalarm der Sorte, vor der der Abschnitt
  „Namensunterschiede" warnt — diesmal nicht ein Name für zwei
  Merkmale, sondern ein Setter, der gar keins ist.

**Damit steht die Zwischenbilanz bei drei von sieben anders als
notiert** (Squelch-Automatik offen, SWR-Kurve vorhanden, Abstimmhilfen
gedrittelt). Die Regel „erst im Baum suchen" hat sich zweimal an zwei
Tagen bezahlt.

## Merkmal 1 ist geschlossen: die Automatik nimmt den *sichtbaren* Boden

`setAutoSquelchEnable` + `setAutoSqlMarginDb` sind am selben Tag
nachgezogen. Der interessante Teil war nicht die Rechnung — Boden plus
Abstand — sondern **welcher** Boden.

Wir führen drei Kandidaten:

1. `m_nfLerpAverage` aus `processNoiseFloor` — Thetis-treu, und der Wert,
   den die **NF-Linie malt**.
2. Die Perzentil-plus-EWMA-Schätzung des `ClarityController`
   (`NoiseFloorEstimator`) — treibt nur das mitlaufende Gitterminimum.
3. AetherSDRs eigener Zweipass-Mittelwert in
   `updateAutoSquelchFromBins`, der beim Port mitgekommen wäre.

Genommen ist (1), und der Grund steht schon als Kommentar in
`onNoiseFloorChanged`: die Clarity-Schätzung ergibt einen anderen Wert
als Thetis' Verfahren, weshalb die NF-Anzeige sie ausdrücklich *nicht*
nimmt — sonst schwebte die Linie über dem sichtbaren Rauschen. Dieselbe
Überlegung gilt für die Schwelle: **wer die Linie sieht, muss die
Schwelle daran ablesen können.** Ein dritter Schätzer hätte eine
Schwelle ergeben, die neben der gezeichneten Linie liegt, und niemand
hätte gewusst, welche der beiden lügt.

Weiter nicht übernommen: AetherSDRs `drawAutoSqlFloor` (gestrichelte
Bodenlinie plus „Floor -120 dBm"). Unsere NF-Anzeige malt genau diesen
Wert bereits, mit einstellbaren Farben und Linienbreite. Zwei Striche auf
derselben Höhe sind kein Merkmal.

## Dritte Korrektur: „Erweiterter Durchlass" tun wir schon, ohne Schalter

Gesucht am 2026-08-19, bevor gebaut wurde. Die beiden Setter machen
zusammen **eine** Sache: Mittellinie und Durchlassfläche nicht nur im
Spektrum zeichnen, sondern **hinunter in den Wasserfall** verlängern.
`m_extendedFrequencyLine` verlängert die Linie, `m_extendedPassband` die
Fläche; beide stehen bei AetherSDR auf `aus`.

Bei uns läuft genau das, unbedingt und ohne Schalter:

* Durchlassfläche im Wasserfall — `SpectrumWidget.cpp`, direkt nach der
  Spektrumsfläche, dieselbe `m_rxFilterColor`
* Filterkanten — vier Linien, zwei davon über `wfRect.top()` bis
  `wfRect.bottom()`
* Mittellinie — `drawLine(vfoX, specRect.top(), vfoX, wfRect.bottom())`,
  also über beide Bereiche in einem Zug

Damit ist es **derselbe Fall wie `setSingleClickTune`**: nicht eine
Lücke, sondern ein Schalter, um unser Verhalten *abzuschalten*. Portiert
würde er zwei Optionen hinzufügen, deren Vorgabestellung schlechter ist
als der Istzustand. Nicht gebaut — und wenn jemand die Verlängerung
abschalten will, ist das eine Betriebsfrage und kein Port.

**Bilanz der Regel „erst im Baum suchen":** von sieben Merkmalen waren
nach der Prüfung drei anders als notiert, plus der Bandplan davor. Vier
Fehlalarme in zwei Tagen, jeder einzelne wäre als Klon mitgebaut worden.

## Vierte Korrektur: der S-Verlauf ist kein Streifen, und TX im Wasserfall haben wir

Beide Punkte gesucht, bevor gebaut wurde. Beide sind anders als notiert.

### S-Verlauf — kein eigener Streifen, sondern ein Erkenner

Die Liste sagte „Signalstärke über Zeit als eigener Streifen", und daraus
folgte die Schätzung „Tagesstück wegen der Höhenaufteilung". **Falsch.**
Die S-Verlauf-Marken laufen bei AetherSDR durch `drawSpotMarkers` —
dieselbe Kollisionsstapelung wie die DX-Spots, mit `sLabel(peakDbm)` als
Etikett (also „S7" statt eines Rufzeichens) und einer Unterdrückung, wenn
ein echter Spot binnen 3 kHz liegt. Kein Streifen, keine
Höhenaufteilung.

Der Aufwand liegt woanders, nämlich in der **Quelle**:

* `src/core/VoiceSignalDetector.cpp`, 314 Zeilen
* `src/core/SignalClassifier.cpp`, 88 Zeilen — **lädt ein Modell von der
  Platte** (`loadModel(path)`), unterscheidet QRM von Sprache
* dazu in `MainWindow` eine Einträgeverwaltung mit `lastSeenMs`,
  `peakDbm`, `widthHz`, Alterung, und dem Sonderfall „Sprache über einem
  als QRM eingeordneten Eintrag" (zwei Marken gleichzeitig)

**Was das für uns heißt:** die Zeichenhälfte haben wir bereits — die
Spot-Marken samt Kollisionsstapelung sind seit 3J-2 portiert. Und der
Kern des Betriebsnutzens („welche Frequenzen waren zuletzt belegt, wie
stark") steckt bei uns im `PeakBlobDetector` mit
`ActivePeakHoldTrace`: N Spitzen, `max_dBm` je Spitze,
Zeitstempel der letzten Anhebung, Haltezeit, Abfallrate, wahlweise nur
innerhalb des Filters. Genau die 11 Setter, die im Abschnitt „Was wir
haben und AetherSDR nicht" stehen.

Die echte Lücke ist damit **nicht ein Anzeigemerkmal, sondern die
Einordnung** (QRM gegen Sprache) — und die hängt an einem
Modelldatei-Erkenner. Das ist eine **Architekturentscheidung**, keine
Portierung: eine neue Abhängigkeit, eine mitzuliefernde Modelldatei, ein
Erkenner im FFT-Strom. Gehört dem Betreiber vorgelegt, nicht nebenbei
gebaut. Aus der Reihenfolge „Nummer 3 von sieben" ist damit ein eigener
Vorschlag geworden.

### TX im Wasserfall — vorhanden, mit Schalter, den AetherSDR nicht hat

`setShowTxInWaterfall` heißt bei AetherSDR: während des Sendens weiter
Wasserfallzeilen aus den FFT-Werten schieben, damit das eigene Signal im
Verlauf erscheint. Bei uns läuft das **von sich aus** — `pushWaterfallRow`
schiebt durch, und es gibt sogar den umgekehrten Schalter dazu:
`WaterfallStopOnTx` (Task 2.8, Vorgabe aus) hält den Wasserfall beim
Senden an, wenn jemand das ausdrücklich will. Dazu kommen
`m_showTxFilterOnRxWaterfall` und `m_showTxZeroLineOnWaterfall` als
TX-Überlagerungen im Wasserfall.

Einschränkung, damit die Notiz nicht mehr behauptet als geprüft ist: der
*Mechanismus* ist da und der Schalter auch. Ob der Inhalt beim Senden
tatsächlich das TX-Signal zeigt, ist eine Frage an die Bank, nicht an den
Quelltext — sie steht auf der TX-Prüfliste.

**Fehlalarm-Bilanz nach zwei Tagen: fünf.** Bandplan, Einfachklick,
Zeigerform, erweiterter Durchlass, TX im Wasserfall — plus der S-Verlauf,
der keiner ist, sondern etwas anderes und Größeres. Von sieben
„Merkmalen" der Liste bleiben nach der Prüfung **zwei** übrig, die
wirklich Panadapter-Arbeit sind: die SWR-Kurve (Darstellung, Controller
vorhanden) und die Ausbreitungsvorhersage (fremde Daten).

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
