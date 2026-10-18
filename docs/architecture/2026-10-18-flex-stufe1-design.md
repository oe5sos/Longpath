# Flex-Treiber, Stufe 1 — Erkennung und Befehlskanal

Stand 2026-10-18. Betreiber: OE5SOS. Gerät: FLEX-8400, 172.30.30.12,
SmartSDR 4.3.16.42034, API 1.4.0.0. Basis: `e5515581` (origin/main).

**Was diese Stufe beantworten soll:** trägt der Entwurf, bevor die 2279
Zeilen VITA-49 angefasst werden. Nicht: ein halbes Gerät bauen.

---

## 1. Der Umfang, gemessen

`~/Longpath/AetherSDR` bei `d58e2b8a` (`v26.8.3-144-gd58e2b8a`), ein
vollständiger SmartSDR-Client in C++/Qt6. Der Flex-Draht liegt dort
**nicht** im Flex-Backend, sondern verteilt:

| Datei | Zeilen | Was |
|---|---|---|
| `src/core/RadioDiscovery.{h,cpp}` | 481 | UDP 4992, Erkennungsruf |
| `src/core/RadioConnection.{h,cpp}` | 823 | TCP 4992, Sätze C/R/S/M |
| `src/core/PanadapterStream.{h,cpp}` | 2279 | VITA-49 |
| `src/core/backends/flex/FlexBackend.*` | 1537 | Übersetzung aufs Modell |
| `src/core/CommandParser.{h,cpp}` | 192 | die Satzlehre selbst |
| `src/core/backends/IRadioBackend.h` | 1086 | die Naht, hinter der das sitzt |

Zum Vergleich in Longpath: `SunSdrRadioConnection` 4518 Zeilen,
`P2RadioConnection` 4909.

**Stufe 1 deckt die ersten beiden Zeilen** (481 + 823 = 1304 Zeilen
Vorlage), ohne die 2279 Zeilen VITA.

Was im Longpath-Baum an Flex schon lag, bevor diese Stufe begann:

* `FlexRadioDiscoveryBroadcaster` (573 + 91) — Longpath **sendet** einen
  Flex-Erkennungsruf, damit die PGXL es findet. Darin steht der
  28-Byte-Kopf Byte für Byte, Bank gegengeprüft an einer
  FLEX-8600-Aufzeichnung vom 2026-05-19.
* `SmartSdrApiListener` (1231 + 336) — antwortet auf die Handvoll
  Kopplungsbefehle der PGXL. Kein Radio.

---

## 2. Was gebaut ist

Zwei neue Klassen, beide **keine** `RadioConnection`-Unterklasse:

* `src/core/FlexRadioDiscovery.{h,cpp}` — hört passiv auf UDP 4992,
  liest den Ruf, übersetzt ihn in Longpaths eigenes `RadioInfo` und
  meldet ihn über `radioDiscovered`/`radioUpdated`/`radioLost`.
* `src/core/FlexRadioConnection.{h,cpp}` — TCP 4992, Satzlehre
  V/H/C/R/S/M, Anmeldung, und `sliceFrequencyChanged(int, double)`.

Dazu: `ProtocolVersion::Flex = 4`, `HPSDRHW::Flex8400 = 14`, drei
Beschriftungen in `ConnectionPanel`, die Verdrahtung in `RadioModel`
(`flexDiscovery()`), und `tests/tst_flex_stufe1.cpp` (29 Fälle).

### Warum keine RadioConnection-Unterklasse

`RadioConnection` ist durch und durch OpenHPSDR: Alex-Antennenwege,
ADC-Zahlen, Metis-Rahmen, C&C-Register — und setzt voraus, dass der
**Client** rechnet. Bei Flex rechnet das Radio. Dieselbe Trennung wie
bei `TciClient` und beim KiwiSDR-Weg: eine eigene Klasse daneben, nicht
eine Unterklasse, die neun Zehntel ihrer Basis nicht benutzt.

### Warum `client gui` NICHT gesendet wird

AetherSDR sendet es (`RadioModel.cpp:6540 [@d58e2b8a]`) und muss das
auch — es ist der Hauptbediener. Ein GUI-Client übernimmt die Maschine
und wirft bei Bedarf einen anderen hinaus; AetherSDR führt dafür eigens
eine „reclaim-eviction guard" (`:6580`). Zum **Lesen** braucht es das
nicht. Stufe 1 bleibt Mitleser, damit Martins Maschine weiter an
SmartSDR oder am Maestro hängen darf.

Das hat eine Folge, die Abschnitt 4 beschreibt.

---

## 3. Der Draht, gelesen statt vermutet

Am 2026-10-18 mit einem Mitleser auf UDP 4992 aufgenommen.

### 3.1 Der Erkennungsruf

620 Byte, 1 Hz, von 172.30.30.12 nach 255.255.255.255:4992.

```
Kopf:  38 53 00 9b 00 00 08 00 00 00 1c 2d 53 4c ff ff
       6a d4 7a b9 00 00 00 00 00 00 00 00
Nutzlast:
  discovery_protocol_version=3.1.0.4 model=FLEX-8400
  serial=1025-1213-8400-1196 version=4.3.16.42034 nickname=Martin
  callsign=OE5SOS ip=172.30.30.12 port=4992 status=Available
  inuse_ip= inuse_host= max_licensed_version=v3
  radio_license_id=00-1C-2D-05-34-28 fpc_mac= wan_connected=0
  licensed_clients=2 available_clients=2 max_panadapters=2
  available_panadapters=2 max_slices=2 available_slices=2
  gui_client_ips= gui_client_hosts= gui_client_programs=
  gui_client_stations= gui_client_handles=
  min_software_version=3.8.0.0 external_port_link=1
  license_is_unknown=0 is_system_model=0 turf_region=EU
```

Byte 0 = 0x38, Byte 1 = 0x50|Zähler, Byte 2–3 = 0x009b = 155 Worte =
620 Byte. **Der 2026-05 an einer 8600 beschriebene Kopf stimmt auch für
eine 4.3.16-Maschine** — der eigene Baum war hier verlässlich.

### 3.2 Drei Stellen, an denen AetherSDRs Leser etwas anderes erwartet

AetherSDRs `parseDiscoveryPacket` (`RadioDiscovery.cpp:179-237
[@d58e2b8a]`) kennt keinen Kopf und liest das ganze Datagramm als Text.
Das funktioniert, weil Kopfbytes durch `cleanDiscoveryValue` und die
Regel „Wort ohne '=' überspringen" von selbst herausfallen. Drei seiner
Schlüssel treffen aber nicht, was die echte Maschine sendet:

| AetherSDR liest | Die Maschine sendet | Folge dort |
|---|---|---|
| `inuse` | `inuse_ip=`, `inuse_host=` | `inUse` bleibt immer false |
| `mf_enable` | `licensed_clients`, `available_clients` | Vorgabe true bleibt stehen |
| `max_licensed_version` als int | `v3` — mit „v" davor | `toInt()` gibt still 0 |

Longpath liest alle drei richtig; `tst_flex_stufe1` hält das fest, damit
es beim nächsten Abgleich nicht zurückfällt.

### 3.3 Das Phantom-Flex, unabhängig bestätigt

Der Mitleser sah **zwei** Rufer:

```
172.30.30.110  model=FLEX-6400 serial=7341-1489-4925-4009
               version=0.7.0 nickname=Longpath callsign=OE5SOS
```

Das ist Longpath selbst (PID 76168 hält UDP 4992), gesendet vom
`FlexRadioDiscoveryBroadcaster`, damit die PGXL es findet. Genau dieses
Phantom stand am 2026-10-18 in Martins Maestro-Geräteliste.

`FlexRadioDiscovery` verwirft jeden Ruf, dessen **Absenderadresse** eine
eigene Adresse dieses Rechners ist — live bestätigt
(`eigener Ruf von "172.30.30.110" — uebergangen`). Der Filter hängt an
der Adresse, nicht am Spitznamen: dieselbe Maschine von einer fremden
Adresse wäre eine fremde Maschine.

**Das ist nicht die Behebung des Phantoms, und sie ist auch nicht mehr
nötig:** `e5515581` („Kein Phantom-Flex mehr im Netz: der Ausruf wartet
auf den 4O3A-Schalter", PR #272) liegt seit 10:01 am 2026-10-18 auf
main und hängt den Ausruf hinter den 4O3A-Hauptschalter. Was der
Mitleser hier sah, war die **laufende alte Fassung** (PID 76168, zur
Messzeit noch nicht neu gebaut) — die Messung ist also echt, der Fehler
dahinter war beim Messen schon behoben.

Der Eigenruf-Filter bleibt trotzdem richtig und nötig: wer eine PGXL
betreibt, schaltet den Ausruf bewusst ein, und dann ruft Longpath wieder
aus. Ohne den Filter stünde es ab diesem Moment in seiner eigenen
Liste.

### 3.4 Der Befehlskanal

```
TX  (TCP connect 172.30.30.12:4992)
RX  V1.4.0.0
RX  H5081495E
TX  C1|client program Longpath
TX  C2|sub slice all
RX  M10000001|Client connected from IP 172.30.30.110
RX  S5081495E|radio slices=2 panadapters=2 lineout_gain=60 … mf_enable=1 …
RX  S5081495E|radio filter_sharpness VOICE level=2 auto_level=1
RX  S5081495E|radio oscillator state=tcxo setting=tcxo locked=1 … tcxo_present=1
RX  S5081495E|interlock acc_txreq_enable=0 … tx1_enabled=1 … timeout=0
RX  S5081495E|eq rx mode=0 63Hz=10 …
RX  R1|10000002|unknown client program Longpath
RX  R2|0|
RX  S0|interlock tx_client_handle=0x00000000 state=RECEIVE … tx_allowed=0
```

Vier Befunde:

1. **`V1.4.0.0` ist die API-Fassung, nicht die Software-Fassung.** Die
   Maschine meldet im Ruf `version=4.3.16.42034`, im Kanal `1.4.0.0`.
   Wer die Kanalfassung anzeigt, zeigt das Falsche.
2. **`client program Longpath` wird abgelehnt:**
   `R1|10000002|unknown client program Longpath`. Die Maschine prüft den
   Programmnamen gegen eine ihr bekannte Liste. **Nicht tödlich** — der
   Kanal läuft weiter, `sub slice all` danach wird mit `R2|0|`
   angenommen. Der Befehl bleibt drin: er ist die dokumentierte
   Anmeldefolge, und die Ablehnung ist gemessen, nicht geraten.
   Impersonieren (einen fremden Programmnamen senden) ist keine Option.
3. **Der Zustandsschub kommt ungefragt und reichlich** — `radio`,
   `filter_sharpness` ×3, `static_net_params`, `oscillator`,
   `interlock`, `eq` ×2, `waveform`. Darunter Dinge, die Longpath heute
   schon zeigt (TCXO gerastet, Interlock, Lineout/Headphone-Pegel).
4. **`S0|…` hat Handle 0** — ein Schub ohne Empfängerbindung. Der Leser
   muss damit umgehen; er tut es.

### 3.5 Nur-Lese-Abfragen

```
C|slice list        → R|0|                (LEER)
C|info              → R|0|model="FLEX-8400",chassis_serial="1025-1213-8400-1196",
                           name="Martin",callsign="OE5SOS",gps="Not Present",
                           atu_present=0,num_scu=1,num_slice=2,num_tx=1,
                           software_ver=4.3.16.42034,mac=00:1C:2D:05:34:28,
                           ip=172.30.30.12,netmask=255.255.255.0,
                           gateway=172.30.30.1,region="EU",options="None",
                           1750_tone_burst=1,diversity_allowed=0,
                           iaru_region=1,cw_decode_available=1
C|version           → R|0|SmartSDR-MB=4.3.16.42034#PSoC-MBTRX=1.0.6.0#
                           PSoC-MBPA100=0.0.1.32#FPGA-MB=0x4784dfc8
C|display pan list  → R|50001000|          (abgelehnt)
```

`info` bestätigt Martins Angaben vollständig und ist die Quelle für
alles, was eine Platinentabelle bei OpenHPSDR liefert: `num_scu=1`,
`num_slice=2`, `num_tx=1`, `atu_present=0`, `iaru_region=1`,
`diversity_allowed=0`.

---

## 4. Der Befund, der Stufe 1 beantwortet

**`slice list` ist leer. Die Maschine hat keine Scheiben.**

Es gibt also keine Frequenz zu lesen — nicht, weil der Leser sie nicht
findet, sondern weil keine existiert. Scheiben werden von einem
**GUI-Client** angelegt; im Ruf steht `gui_client_stations=` leer und
`inuse_ip=` leer, es sitzt niemand auf der Maschine.

`max_slices=2` und `num_slice=2` sind die **lizenzierte Zahl**, nicht
die vorhandene.

Daraus folgen genau zwei Wege, und das ist die Entscheidung, die Stufe 1
herausgearbeitet hat:

**(a) Mitlesen.** Longpath bleibt Beobachter. Sobald SmartSDR oder der
Maestro verbunden ist, legt dieser Scheiben an, und `sub slice all`
liefert sie. Kostet keine Zeile Code mehr — der Weg steht und ist
geprüft, er braucht nur eine Gegenstelle. Longpath übernimmt die
Maschine nie.

**(b) Selbst Bediener werden.** `client gui <uuid>` senden, dann
`slice create`. Das ist, was ein echter Treiber tun muss, und es ist der
erste Schritt, der Martins Maschine tatsächlich anfasst (und bei Bedarf
einen anderen Client hinauswirft).

Stufe 1 hat (b) **nicht** getan: es ändert den Zustand seiner laufenden
Station, und das ist keine Entscheidung, die eine Leseprobe nebenbei
trifft.

---

## 5. Der Haken für Stufe 2: bei Flex rechnet das Radio

Heute, gemessen am 2026-10-18: **34 Funktionen** in `RadioModel.cpp`
führen den Empfangsweg durch WDSP (`RxChannel`/`rxChannel`,
nach Funktionsnamen entdoppelt — Martins Zahl 33 trifft auf eins):

```
activateSliceChannel      deactivateSliceChannel   reconcileNotchCount
addNotchForSlice          flushNotchEditPush       reshiftSlicesOnStream
addSlice                  onMoxHardwareFlipped     rxAnf
applyStreamDspGeometry    onSliceBandChanged       rxBin
bindSliceToStream         openRxChannelPool        rxChannelForSlice
commitStreamSampleRateChange  pushNotchOrigin      rxNrIndex
connectToRadio            rebuildDspOptionsForMode rxSignalDbm
setActiveRxCountLive      reconcileExternalDiversityRoute
setSampleRateLive         sliceRxChannels          slicesOnStream
syncNotchesToAllChannels  syncNotchesToChannel     teardownConnection
wireConnectionSignals     wireNotchModel           wireRadeChannel
wireSliceSignals
```

Für ein Flex braucht es daneben einen Weg, der **fertigen Ton und
fertige Panadapter-Daten entgegennimmt**. Das ist der Aufwand, nicht der
Draht. AetherSDR hat dafür `IRadioBackend` (1086 Zeilen) — eine Naht, an
der ein Backend entweder rechnet oder fertige Daten liefert. Longpath
hat diese Naht nicht; `RadioModel` ist die Naht.

Was Stufe 1 dazu beiträgt: sie zeigt, dass Erkennung und Steuerung
sauber **neben** diesem Weg liegen können (eigene Klassen, eigene
Signale, kein WDSP), und dass die Geräteliste keine zweite Zeilenart
braucht. Die Naht wird erst gebraucht, wenn Ton und Bild kommen.

---

## 6. Was geprüft ist, und wie

| Sache | Belegt durch |
|---|---|
| Kopf- und Schlüsselsatz des Rufs | `tst_flex_stufe1`, gegen die 620 echten Bytes |
| Die drei AetherSDR-Abweichungen | `tst_flex_stufe1` |
| Eigenruf wird verworfen | `tst_flex_stufe1` + live (`übergangen`, 13× in 14 s) |
| Liste anlegen/nachführen/vergessen | `tst_flex_stufe1` |
| Satzlehre V/H/R/S/M | `tst_flex_stufe1`, inkl. Code mit gesetztem oberstem Bit |
| Frequenz MHz→Hz, unlesbar ≠ null | `tst_flex_stufe1` |
| Erkennung am echten Gerät | live: `FLEX-8400 (Martin)`, MAC `00:1C:2D:05:34:28` |
| Befehlskanal am echten Gerät | live: V/H, `sub slice all` → `R2|0|` |
| **Frequenz am echten Gerät** | **NICHT belegt — keine Scheibe vorhanden, siehe §4** |
| **Zeile in der Geräteliste** | **NICHT live gesehen** — Longpath läuft gerade (PID 76168), eine zweite Fassung daneben zu starten hätte Martins Betrieb gestört. Verdrahtung steht, Beschriftung steht. |

29 von 29 Prüffällen grün.

---

## 7. Offen

1. **Die Entscheidung aus §4** — mitlesen (a) oder Bediener werden (b).
2. Die Zeile in der Geräteliste live ansehen.
3. `V1.4.0.0` vs. `version=4.3.16.42034`: wenn eine Fassung angezeigt
   wird, die aus dem Ruf nehmen, nicht die aus dem Kanal.

Nicht mehr offen: das Phantom-Flex (§3.3) — auf main behoben, `e5515581`
/ PR #272.
