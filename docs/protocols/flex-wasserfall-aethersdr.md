# Der Bildweg der FLEX-8400, nachgelesen bei AetherSDR

*Aufgenommen am 2026-10-23. Quelle: AetherSDR @d58e2b8a, der Client,
der dieselbe Maschine bedient und dort funktioniert.*

Anlass: der Betreiber (OE5SOS) am 2026-10-22, nachdem Ton, Abstimmen
und S-Meter liefen — „das rot ist eigenartig gewesen, auch die
zeitliche laenge, bis der wasserfall anfaengt", und:

> die architektur von flex ist eine andere, die wir bei longpath und
> avelina usw. haben. kannst du nicht genau bei aether nachsehen

Er hatte recht. Der Unterschied ist strukturell.

## Zwei Stroeme, nicht einer

Die Flex fuehrt **zwei getrennte Bildstroeme**, und AetherSDR zeichnet
auch aus beiden:

| Klasse | Inhalt | Umrechnung | Ziel |
|---|---|---|---|
| `0x8003` | Panadapter-FFT | Bildzeile → dBm ueber `y_pixels` | Spektrumkurve |
| `0x8004` | Wasserfallkachel | `qint16 / 128` → **Intensitaet** | Wasserfall |

Longpath zieht nur den ersten und leitet den Wasserfall aus den
FFT-Bins ab — mit eigenem Zeitgeber und eigenen Schwellen. Daher der
fremde Takt und die fremde Farbe.

## Die FFT (0x8003)

Identisch zu Longpath, und damit belegt richtig
(PanadapterStream.cpp:980-995):

```
pixel = clamp(bin, 0, yPix - 1)
dbm   = maxDbm - (pixel / (yPix - 1)) * (maxDbm - minDbm)
dbm   = clamp(dbm, minDbm, maxDbm)
```

Longpath klemmt genauso (`FlexVitaReader::binToDbm`) — hier ist kein
Unterschied. (Beim ersten Durchlesen habe ich das Gegenteil notiert;
falsch, nachgesehen am 2026-10-23.)

Der Vollausschlag, den Martins Rahmen am LETZTEN Bin zeigt (Nummer
4095, Wert 0 → `maxDbm` → −40 dBm), kommt nicht vom fehlenden Klemmen,
sondern daher, dass der Wert wirklich null ist. Null heisst in dieser
Kodierung „ganz oben". AetherSDR faengt solche Nullen nur als
ZUSAMMENHAENGENDEN Anhang ab (`hasZeroFilledFftGrowthSuffix`,
mindestens 16 Bins, nach einem Wachstum von `x_pixels`) — Longpath hat
denselben Schutz. Ein einzelner Bin faellt durch beide. Kosmetisch,
aber er zieht jede Spitzenstatistik hoch; darum steht im Bandbericht
das 99. Perzentil neben der Spitze.

Der gemeldete `y_pixels` ist der Maszstab, nicht die Frame-Hoehe:
Werte ab `y_pixels` sind am Boden abgeschnittene Messpunkte, kein
Hinweis auf eine hoehere Skala.

## Die Wasserfallkachel (0x8004)

Unterkopf, 36 Byte ab Byte 28, big-endian
(PanadapterStream.cpp:1018-1035):

```
int64  FrameLowFreq        Hz × 2^20  (VitaFrequency)
int64  BinBandwidth        Hz × 2^20
uint32 LineDurationMS      <- der Takt der Maschine
uint16 Width               Bins je Zeile
uint16 Height              Zeilen in dieser Kachel (meist 1)
uint32 Timecode            fuer den Zusammenbau
uint32 AutoBlackLevel      <- Schwarzpunkt, von der Maschine gerechnet
uint16 TotalBinsInFrame
uint16 FirstBinIndex
```

Nutzlast: `Width × Height` uint16, big-endian. Umrechnung:

```
wert = (float)(qint16)roh / 128.0f
```

**Das sind KEINE dBm.** Am Geraet gemessen liegen sie bei +88 bis
+120; AetherSDRs eigener Kommentar sagt „typical noise floor ~96-106,
signal peaks ~110-115, colour-mapped in SpectrumWidget". Longpath hat
dafuer `FlexVita::tileColourValue()` — geschrieben, nie benutzt — und
einen Prueffall, der genau das festhaelt
(`kachelwerteSindEinFarbmassstabKeineDbm`).

Wer diese Werte in Longpaths dBm-Wasserfall gibt, bekommt sofort ein
einfarbiges Bild am oberen Anschlag. Also genau das Rot, das behoben
werden soll.

## Die Einfaerbung (SpectrumWidget.cpp:8393, intensityToWaterfallLevel)

```
level = clamp((intensitaet - blackThresh) / rangeWidth, 0, 1)
```

mit drei Faellen fuer `blackThresh` / `rangeWidth`:

1. **Radio-seitig** (nur wenn der Betreiber es waehlt, und
   `AutoBlackLevel > 0`):
   `lowRaw  = clamp(AutoBlackLevel + (50 - offset) * 0.5 * 128, 0, 65535)`
   `highRaw = wfHighThresholdRaw(lowRaw, colorGain)` (kubische Kurve)
   `blackThresh = lowRaw / 128`, `rangeWidth = (highRaw - lowRaw) / 128`

2. **Selbst geschaetzt** (die uebliche Wahl):
   `blackThresh = autoBlackThresh + (50 - offset) * 0.5`
   `rangeWidth  = max(1, 120 - colorGain * 0.91)`

   `autoBlackThresh` wird aus der Kachel selbst verfolgt: Mittel ueber
   jeden achten Bin, dann Mittel der Bins UNTER diesem Mittel — das
   ist der Rauschteppich, und er wird direkt als Schwarzpunkt genommen.

3. **Von Hand**: `blackThresh = 160 - blackLevel`, `rangeWidth` wie oben.

AetherSDR benutzt Fall 1 ausdruecklich NUR auf Wunsch, mit der
Begruendung, der Schwarzpunkt der Maschine ziele auf SmartSDRs andere
Darstellung.

## Was AetherSDR dem Panadapter schickt, und Longpath nicht

Beim Anlegen (RadioModel.cpp:4966-4967) nur:

```
display pan set <id> xpixels=1024 ypixels=700
display pan set <id> min_dbm=-130 max_dbm=-40
```

Im Betrieb kennt es aber diese Befehle, die Longpath nie schickt:

```
display pan set <id> band=<n>
display pan set <id> bandwidth=<MHz>
display pan set <id> center=<MHz> bandwidth=<MHz>
display pan set <id> rxant=<ANT>          <- eigene Antennenwahl fuer das BILD
display pan set <id> rfgain=<dB>          <- nur Vielfache von 8
display pan set <id> fps=<n>
display pan set <id> average=<n>
display pan set <id> weighted_average=<n>
display pan set <id> noise_floor_position[_enable]=<n>
display pan set <id> loopa / loopb / daxiq_channel
```

Der Panadapter hat also eine **eigene Antennenwahl**, getrennt von der
Scheibe. An Martins Geraet standen beide auf ANT1, aber das ist eine
Falle, auf die man sehen muss.

## Was davon gebaut ist

**Der Kachelweg steht** (2026-10-23). `SpectrumWidget::
zeigeFremdeWasserfallzeile()` nimmt eine Kachelzeile an, bildet sie
auf die Fensterbreite ab (Maximum je Bildpunkt, damit ein schmaler
Traeger nicht zwischen zwei Punkte faellt) und faerbt sie in der
INTENSITAETSDOMAENE nach Fall 2 oben. Solange Kacheln kommen,
schweigt der aus der FFT abgeleitete Weg; nach zwei Sekunden ohne
Kachel uebernimmt er wieder.

Gemessen im Pruefstand `tst_flex_wasserfall_kachel`:

| | Teppich | Traeger bei 14.125 MHz |
|---|---|---|
| in der Intensitaetsdomaene | hoechste Stufe **8** von 255 | Stufe 64 an Punkt **726**, erwartet 727 |
| in dBm (der Fehler) | jeder Punkt **255** von 255 | Stufe 255 an Punkt **0** |

Die untere Zeile ist die Gegenprobe, und sie ist genau das
einfarbige Bild am oberen Anschlag — bei Farbschema 0 also Rot.

## Der Wasserfall ist ein EIGENER Strom

Der Panadapter-Zustand nennt ihn: `waterfall=0x42000000` neben
`panadapter=0x40000000`. Er hat **eigene Einstellungen**, und Longpath
hat sie bis zum 2026-10-23 nie angefasst. An Martins Geraet gemessen:

```
display waterfall 0x42000000 … x_pixels=4096 line_duration=80
                               color_gain=50 auto_black=1 black_level=0
```

`x_pixels` folgt dem Panadapter (gut), `line_duration` taktet die
Zeilen (80 ms) — aber **`auto_black=1` stand in ALLEN fuenf
Statusmeldungen**. Die Maschine rechnet dann ihren eigenen
Schwarzpunkt in die Kachel, gezielt auf SmartSDRs Darstellung,
waehrend der Client obendrauf noch einmal schaetzt.

AetherSDR stellt ihn darum ausdruecklich ab
(`RadioModel::configureWaterfall`):

```
display panafall set <wfId> auto_black=0 black_level=15 color_gain=50
```

mit Rueckfall auf `display waterfall set <wfId> …` fuer Fassungen ohne
Panafall-Adressierung. Longpath schickt das jetzt auch, mit
`black_level=0` statt 15 — bei abgeschaltetem `auto_black` ist das der
Handwert der Maschine, und gefaerbt wird ohnehin im Client.

## Eine bewusste Abweichung von der Vorlage

AetherSDR tastet beim Abbilden auf die Fensterbreite den naechsten Bin
ab und interpoliert linear (SpectrumWidget.cpp:8512-8521). Longpath
nimmt das MAXIMUM ueber die Bins, die auf den Bildpunkt fallen.

Grund: bei Martins Geraet liegen 4096 Bins auf rund 1150 Bildpunkten.
Mit Abtastung faellt ein schmaler Traeger in der Mehrzahl der Faelle
ganz heraus. Longpath hat denselben Fall am 2026-09-25 schon einmal
entschieden (`wfRowSourceReduce`, Rendering-Werkbank): „ein schmaler
Traeger (CW, FT8, Bake) lag in der Haelfte der Faelle genau in der
verworfenen Spalte: gestrichelt oder unsichtbar".

## Was noch zu bauen waere

1. **`band=` und `bandwidth=`** mitschicken, damit der Panadapter dem
   Bandwechsel folgt und nicht auf dem stehenbleibt, was zuletzt
   jemand eingestellt hat (gemessen: 200 kHz um 14.100, waehrend die
   Scheibe auf 14.382 stand).
