# FlexRadio VITA-49 — gemessen an einer FLEX-8400

Aufgezeichnet am 2026-10-18 an OE5SOS' FLEX-8400 (172.30.30.12, SmartSDR
4.3.16.42034, API 1.4.0.0). Zwei Läufe: 1832 Datagramme / 2,1 MB ohne
Ton, dann 5160 Datagramme / 5,7 MB mit Ton.

Diese Datei ist der Gegenprüfstein für den Port von AetherSDRs
`PanadapterStream` (2279 Zeilen). **Nichts daraus ist geraten:** jede
Zahl unten steht so im Mitschnitt, und jede Feldbedeutung ist gegen die
Vorlage gelesen (`../AetherSDR/src/core/PanadapterStream.cpp` bei
`d58e2b8a`) und danach am Mitschnitt nachgerechnet.

Entwurf und Einordnung: `docs/architecture/2026-10-18-flex-stufe1-design.md`.

---

## 1. Wie der Strom überhaupt anfängt

Ein Port, ein Socket, alles darüber — Panadapter, Wasserfall, Ton,
Messwerte kommen auf **demselben** UDP-Port und werden an der
Paketklasse auseinandersortiert.

Gemessene Reihenfolge:

1. TCP 4992 verbinden, `client gui <uuid>` — ohne GUI-Anmeldung gibt es
   keine Scheibe und keinen Panadapter (siehe Entwurf §4).
2. Einen eigenen UDP-Port binden (Betriebssystem wählt).
3. **Ein Null-Byte an die Maschine auf UDP 4992 senden.** Sie lernt
   IP:Port aus der Absenderadresse. Aus
   `PanadapterStream.cpp:261-268 [@d58e2b8a]`.
4. `client udpport <port>` über TCP.
5. `display pan set <panId> xpixels=1024 ypixels=700` — **ohne Breite
   weiß die Maschine nicht, wie viele Bins sie schicken soll.**

### Abweichung zur Vorlage, gemessen

AetherSDR notiert an Schritt 3:

> This is required on firmware v1.4.0.0 where the TCP "client udpport"
> command may return 0x50001000 ("command not supported").

**Auf dieser Maschine antwortet `client udpport` mit Code 0x0**, also
angenommen — obwohl sie genau API 1.4.0.0 meldet. Vorsicht beim Deuten:
das Null-Byte ging im Messlauf **vor** dem TCP-Befehl hinaus, die
Maschine kannte den Endpunkt also schon. Was belegt ist: der TCP-Befehl
wird hier nicht abgelehnt. Was **nicht** belegt ist: ob er allein
genügt. Beide Wege bleiben, in dieser Reihenfolge.

Nach Schritt 5 kam der Strom sofort; kein weiterer Befehl war nötig.

---

## 2. Der Kopf — 28 Byte, für alle Klassen gleich

```
38 5c 00 24 | 40 00 00 00 | 00 00 1c 2d | 53 4c 80 03 | 6a d1 07 44 | 00000000 00000000
```

| Byte | Bedeutung | Gemessen |
|---|---|---|
| 0 | `0x38` — VITA-49 Typ 3 (ExtDataWithStream), ClassID vorhanden | immer 0x38 |
| 1 | `0x50 \| Zähler` — TSI=1 (UTC), TSF=1, 4-Bit-Rollzähler | fortlaufend 12,13,14,15,0,1,… |
| 2-3 | Paketgröße in 32-Bit-Worten | 0x0024 = 36 Worte = 144 Byte ✓ |
| 4-7 | **StreamID** — der Verteilerschlüssel | `0x40000000` Panadapter, `0x42000000` Wasserfall |
| 8-11 | pad + OUI FlexRadio Systems | `00 00 1c 2d` |
| 12-13 | Info-Klasse | `0x534c` („SL") |
| 14-15 | **Paketklasse** | `0x8003` / `0x8004` |
| 16-19 | Zeitstempel, UNIX-Sekunden | 1792083780 |
| 20-27 | Zeitstempel, Bruchteil (64 Bit) | im Mitschnitt immer 0 |

**Die StreamID ist genau die Kennung aus dem TCP-Zustand.** Der
Panadapter meldete sich dort als `display pan 0x40000000` mit
`waterfall=0x42000000` — dieselben Zahlen stehen im Kopf. Man muss also
nichts erraten und nichts zuordnen: wer den Zustandsschub gelesen hat,
weiß, welcher Strom zu welchem Fenster gehört.

Alle Pakete tragen einen **4-Byte-Anhang** (Trailer); die Nutzlast ist
jeweils `Größe − 28 − Unterkopf − 4`.

---

## 3. Paketklasse 0x8003 — Panadapter-FFT

Unterkopf **12 Byte**, big-endian
(`PanadapterStream.cpp:895-902 [@d58e2b8a]`):

| Offset | Feld | Typ |
|---|---|---|
| 0 | `startBin` | u16 |
| 2 | `numBins` | u16 |
| 4 | `binSize` | u16 |
| 6 | `totalBins` | u16 |
| 8 | `frameIndex` | u32 |

Danach `numBins × binSize` Byte Bindaten.

Gemessen:

```
len= 144  startBin=   0  numBins=  50  binSize=2  totalBins=  50  frame=6
          Nutzlast=104  numBins*binSize=100  Rest=4   (Anhang)
len=1384  startBin=   0  numBins= 670  binSize=2  totalBins=1024
len= 752  startBin= 670  numBins= 354  binSize=2  totalBins=1024
```

* `binSize = 2`: die Bins sind **uint16**.
* `totalBins` sprang von **50 auf 1024**, als `xpixels=1024` gesetzt
  wurde — das Feld folgt also der eingestellten Breite.
* Ein Rahmen über 1024 Bins kommt in **zwei** Datagrammen
  (670 + 354 = 1024) und wird über `startBin` zusammengesetzt.
* **459 Rahmen im Mitschnitt, alle vollständig, keiner lückenhaft.**

### Was ein Bin bedeutet — die Stelle, an der man sich irrt

Die uint16-Werte sind **keine** dBm und **kein** 0…65535-Bereich,
sondern **Y-Pixelpositionen**: 0 = oben = `max_dbm`,
`ypixels − 1` = unten = `min_dbm` (`PanadapterStream.h`, Kommentar zu
`setYPixels`). Wer sie als Rohpegel liest, bekommt Unsinn, der plausibel
aussieht. Die Umrechnung braucht also `min_dbm`/`max_dbm`/`ypixels`,
und die stehen im TCP-Zustand des Panadapters — nicht im Paket.

Daraus folgt die Reihenfolge beim Bauen: **erst** den Panadapter-Zustand
lesen, **dann** Bins deuten.

---

## 4. Paketklasse 0x8004 — Wasserfall-Kacheln

Unterkopf **36 Byte**, big-endian
(`PanadapterStream.cpp:1019-1034 [@d58e2b8a]`):

| Offset | Feld | Typ |
|---|---|---|
| 0 | `frameLowFreq` | i64, **Hz × 2²⁰** |
| 8 | `binBandwidth` | i64, **Hz × 2²⁰** |
| 20 | `tileWidth` | u16 |
| 22 | `tileHeight` | u16 |
| 24 | `timecode` | u32 |
| 28 | `autoBlackLevel` | u32 |
| 32 | `totalBinsInFrame` | u16 |
| 34 | `firstBinIndex` | u16 |

Gemessen:

```
len=1384  frameLowFreq=14542897152000  -> 13.869188 MHz
          binBandwidth=   196608000    ->  0.0001875 MHz = 187,5 Hz/Bin
          width=658 height=1  timecode=4  autoBlack=0x00001f22
          totalBinsInFrame=2460  firstBinIndex=0
          Nutzlast=1320  width*height*2=1316  Rest=4  (Anhang)
```

* **Festkomma, nicht Hz.** `14542897152000 / 2²⁰ = 13 869 188 Hz`. Ein
  Leser, der das als Hz nimmt, landet bei 14,5 Terahertz. AetherSDR
  dekodiert diese beiden Felder deshalb **bedingungslos** als Festkomma
  (deren #4412), damit eine Kachel, die etwas unter DC überhängt, nicht
  für glatte Hz gehalten wird.
* `height = 1`: eine Kachel ist **eine Zeile**.
* Die Zeile kommt in **vier** Kacheln:
  `firstBinIndex` 0, 658, 1316, 1974 → `658·3 + 486 = 2460` ✓ genau
  `totalBinsInFrame`.
* Über alle 924 Pakete: Breiten nur 658 und 486,
  `totalBinsInFrame` immer 2460.
* `timecode` trennt die Zeilen; AetherSDR beginnt einen neuen Rahmen bei
  Wechsel von `timecode` **oder** `totalBinsInFrame`.

### Der Wasserfall ist breiter als der Panadapter

Der Panadapter stand auf Mitte 14,100000 MHz, Breite 0,200000 MHz. Die
Kachel meldet aber Unterkante 13,869188 MHz und
`2460 × 187,5 Hz = 461,25 kHz` Gesamtbreite. Beide Ströme desselben
Fensters decken also **nicht** denselben Bereich ab, und die
Zuordnung Bin → Frequenz muss je Strom aus dessen eigenem Unterkopf
kommen — beim Wasserfall steht sie drin, beim Panadapter nicht.

---

## 5. Paketklasse 0x03E3 — Ton

### Der Ton braucht einen eigenen Strom

Im ersten Lauf kam **kein einziges** Tonpaket. `slice set 0 audio_mute=0`
und `audio_level=50` wurden beide mit Code 0 angenommen und änderten
nichts. Es fehlt ein eigener Strom:

```
stream create type=remote_audio_rx compression=none   → R|0|4000008
```

`compression=none` ist die LAN-Vorgabe, `opus` die für WAN
(`RadioModel::audioCompressionParam [@d58e2b8a]`). Die Antwort trägt die
Stromkennung, und **genau die steht dann im VITA-Kopf**: `0x04000008`.

### ⚠ Nebenwirkung auf die Station

Die Firmware hat `mute_local_audio_when_remote=1` (im Radio-Zustand
nachzulesen). **Sobald ein `remote_audio_rx`-Strom existiert, schweigen
die Hardware-Ausgänge des Geräts** — der Betreiber hört an seiner
eigenen Station nichts mehr. AetherSDR schiebt deshalb vor dem Öffnen
eine 0 hin (`RadioModel.cpp:8438-8443 [@d58e2b8a]`).

Der Messlauf hat es genauso gemacht und danach zurückgestellt:

```
radio set mute_local_audio_when_remote=0     (vor dem Öffnen)
…
stream remove 4000008
radio set mute_local_audio_when_remote=1     (zurück)
slice set 0 audio_mute=1
```

Wer diesen Strom öffnet, ohne daran zu denken, macht die Station stumm
und sucht den Fehler dann im Verstärker.

### Der Aufbau

**Kein Unterkopf.** Nach den 28 Byte Kopf kommen direkt die Werte:
big-endian **float32, Stereo verschränkt** (L, R, L, R, …) —
`PanadapterStream.cpp:1142-1157 [@d58e2b8a]`.

Gemessen, 3382 Pakete, **alle 1052 Byte**:

```
Kopf: 38 50 01 07 | 04 00 00 08 | 00 00 1c 2d | 53 4c 03 e3 | …
      StreamID 0x04000008 = die Kennung aus der stream-create-Antwort
      Nutzlast 1024 Byte = 128 Rahmen × float32-Stereo, genau aufgehend
      Werte alle endlich und alle in [-1, 1]; min -0,054 max +0,065
      Links ≠ Rechts, also echtes Stereo, kein Mono im Stereorahmen
```

3382 × 128 = 432 896 Rahmen in ~20 s ≈ **24 kHz Stereo**. Das ist
dieselbe Form, die `AudioEngine::feedKiwiSdrAudioData` schon
entgegennimmt (`pcm24kStereoFloat`) — der Tonweg aus §5.1 des Entwurfs
passt also ohne Umbau.

Nicht gemessen: die schmale Klasse `0x0123` (int16-Mono, reduzierte
Bandbreite — kommt mit `client set send_reduced_bw_dax=1`) und die
Messwerte `0x8002`. Abwesenheit ist keine Aussage; dazu ist hier nichts
belegt.

---

## 6. Zwei Fallen, die erst der Mitschnitt zeigt

### 6.1 Die vier Null-Byte am Ende sind KEIN VITA-Anhang

Jedes Paket aller drei Klassen ist 4 Byte länger als seine Nutzdaten,
und diese 4 Byte sind immer `00 00 00 00`:

```
FFT  len=1384  numBins=670  Bindaten enden bei 1380   Rest 4   T-Bit=0
FFT  len= 752  numBins=354  Bindaten enden bei  748   Rest 4   T-Bit=0
WF   len=1384  width=658    Daten enden bei 1380      Rest 4   T-Bit=0
WF   len=1040  width=486    Daten enden bei 1036      Rest 4   T-Bit=0
Ton  len=1052  1024 Byte = 128 × float32-Stereo       Rest 0   T-Bit=0
```

Das **T-Bit im Kopf ist überall 0**, es ist also kein angekündigter
VITA-Trailer, sondern Füllung — und beim Ton fehlt sie ganz.

**Die Falle:** wer die Anzahl der Werte aus der *Datagrammlänge*
ableitet statt aus dem Unterkopf, liest bei FFT und Wasserfall **zwei
Werte zu viel**, beide 0. Und 0 heißt im Pixelcode der Maschine
`max_dbm`, also **Vollausschlag** — man bekommt an jedem Rahmenrand ein
erfundenes Riesensignal. Richtig ist: `numBins` bzw.
`tileWidth × tileHeight` zählen, den Rest wegwerfen.

### 6.2 Die VITA-Zeitstempel stehen still

Über den ganzen Mitschnitt, alle drei Klassen:

```
FFT        Sekundenstempel: 1 verschiedener Wert   Bruchteil: 1 Wert
Wasserfall Bruchteil überall 0
Ton        Sekundenstempel: 1 Wert    Bruchteil: 11009, 3381× Abstand 0
```

Der Kopf sagt TSI=1 (UTC) und TSF=1 (Sample-Count) — die Felder tragen
aber keine laufende Zeit. **Ein Leser, der Ton nach diesen Zeitstempeln
taktet oder Pakete danach ordnet, bleibt stehen.** Was wirklich läuft:

* der **4-Bit-Rollzähler** in Byte 1 des Kopfes (fortlaufend geprüft:
  12,13,14,15,0,1,…) — für Verlusterkennung,
* `frameIndex` im FFT-Unterkopf — für Rahmengrenzen,
* `timecode` im Wasserfall-Unterkopf — für Zeilengrenzen.

AetherSDR benutzt die VITA-Zeitstempel an keiner Stelle; das sah nach
einer Auslassung aus und ist eine richtige Entscheidung.

---

## 7. Wie man den Mitschnitt wiederholt

Die Sonde liegt im Kritzelverzeichnis der Sitzung (`vitacap`), nicht im
Baum — sie ist 120 Zeilen und hängt nur an `FlexRadioDiscovery` und
`FlexRadioConnection`. Was sie tut, steht in §1. Sie schreibt
`Länge (u32 BE) + Bytes` je Datagramm, damit die Paketgrenzen erhalten
bleiben; `*.bin` ist in `captures/.gitignore` nicht erfasst, große
Mitschnitte gehören trotzdem nicht in den Baum.

Vor jedem Lauf: `ifconfig en0 | grep "inet "` — die 8400 ist nur im
Netz `oe5sos` (172.30.30.0/24) erreichbar.
