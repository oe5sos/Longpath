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

Daraus folgten genau zwei Wege:

**(a) Mitlesen.** Longpath bleibt Beobachter. Sobald SmartSDR oder der
Maestro verbunden ist, legt dieser Scheiben an, und `sub slice all`
liefert sie. Kostet keine Zeile Code mehr. Longpath übernimmt die
Maschine nie.

**(b) Selbst Bediener werden.** `client gui <uuid>`, dann bei Bedarf
`display panafall create` + `slice create`. Das ist, was ein echter
Treiber tun muss.

### Nachtrag 13:40 — am echten Gerät gefahren, und es kam anders

Das Stationsnetz war wieder da (`172.30.30.110`, Ping zur 8400 52 ms).
Die Sonde durchgelaufen:

```
GEFUNDEN   FLEX-8400 (Martin)  172.30.30.12  maxRx=2
API        1.4.0.0
KENNUNG    ACA918B0-… (flüchtig, nur für diese Sonde)
PAN        display pan 0x40000000  center=14.100000 bandwidth=0.200000
                                   waterfall=0x42000000
ZUSTAND    slice 0  mode=USB rxant=ANT1
                    ant_list=ANT1,ANT2,RX_A,XVTA in_use=1
>>> FREQUENZ  Scheibe 0 = 14.100000 MHz
ANGEMELDET als GUI-Client
SCHEIBEN   1 vorhanden [0]
--- Aufräumen ---  nichts angelegt, nichts zu entfernen
```

**Die Frequenz ist gelesen.** Das Ziel der Stufe 1 ist am echten Gerät
belegt.

Drei Befunde, die §4 korrigieren bzw. ergänzen:

1. **Die Maschine legt Panadapter und Scheibe SELBST an**, sobald sich
   ein GUI-Client anmeldet — `display pan 0x40000000` (Mitte 14,1 MHz,
   Breite 200 kHz) und `slice 0`. Unser Weg „leere Liste ⇒ anlegen" hat
   also gar nicht gegriffen: `slice list` meldete **1**, und die
   „nicht verdoppeln"-Verzweigung hat richtig nichts getan. Damit ist
   diese Verzweigung kein Randfall, sondern der **Normalfall** an dieser
   Maschine. Der Anlegeweg bleibt trotzdem drin — er ist der Fall, den
   AetherSDR an seiner eigenen Hardware gesehen hat (deren #3212).
2. **Der Scheibenzustand kommt VOR der Antwort auf `client gui`.** Die
   Frequenz stand im Protokoll, bevor „angemeldet" gemeldet wurde.
   AetherSDR beschreibt genau das (`RadioModel.cpp:6535-6539
   [@d58e2b8a]`: „The radio dumps slice status in response to GUI-client
   registration, which lands BEFORE the `client gui` reply"); hier ist
   es bestätigt. Folge für Longpath: `flexSliceFrequencyChanged` kann
   feuern, während der Zustand noch `Connecting` ist. Für Stufe 1
   harmlos — die Frequenz wird nur weitergereicht.
3. **Es sammelt sich nichts an.** Zweiter Lauf mit einer anderen
   flüchtigen Kennung: wieder `pan 0x40000000`, wieder genau eine
   Scheibe `[0]`. Die Maschine stapelt also keine Panadapter je
   weggeworfener Kennung. Trotzdem bleibt das Merken der Kennung richtig
   (§8) — sie stellt Frequenz und Betriebsart wieder her.

Nebenbei abgelesen: die Antennen der 8400 sind
`ANT1, ANT2, RX_A, XVTA`.

### Gewählt: (b)

Martin am 2026-10-18 auf die Frage: „keine ahnung, entscheide selbst."

**(a) ist eine Sackgasse.** Ton und Panadapter hängen später an genau den
Scheiben und Panadaptern, die ein GUI-Client anlegt und besitzt. Ein Weg,
der nur funktioniert, solange SmartSDR nebenher läuft, führt zu keiner
Stufe 2 — und zu keinem Betrieb, bei dem Longpath die Station ist.

Gebaut ist damit (siehe `FlexRadioConnection.h`):

```
client program Longpath          → R|10000002| (abgelehnt, nicht tödlich)
client gui <uuid>                → R|0|<uuid der Maschine>
client station Longpath
sub slice all
slice list                       → leer?
   ja  → display panafall create x=100 y=100   (Rückfall: panadapter create)
         slice create pan=<id> freq=14.225000 antenna=ANT1 mode=USB
   nein → nichts anlegen (die Maschine hat uns Scheiben zurückgelegt)
```

Drei Dinge daran sind bewusst so:

* **Die Kennung wird von außen gesetzt** (`setGuiClientId`). Die Maschine
  legt Scheiben und Panadapter **je Kennung** zurück; eine neue Kennung
  bei jedem Start heißt jedes Mal eine neue Vorgabescheibe auf dem Gerät.
  `FlexRadioConnection` kennt `AppSettings` nicht, meldet die verwendete
  Kennung aber über `guiClientIdChosen`, damit der Aufrufer sie behält.
* **Vorhandene Scheiben werden nicht verdoppelt.** Genau der Fehler, den
  AetherSDRs `SliceRecreatePolicy` verhindert (deren #3212: die
  Sitzungswiederherstellung brachte den Panadapter ohne seine Scheibe
  zurück, und der alte Code legte einen zweiten, leeren Panadapter an).
* **`setMayCreateDefaultSlice(false)`** lässt den Weg nachsehen, ohne auf
  der Maschine etwas zu ändern. Für eine Messung, die nichts anfassen
  darf.

Was Stufe 1 weiterhin **nicht** tut: senden. Kein `xmit`, keine
TX-Scheibe, keine Leistung. Die Interlock-Meldung der Maschine
(`state=RECEIVE tx_allowed=0`) wird gelesen, nie geschrieben.

---

## 5. Der Haken: bei Flex rechnet das Radio — und die Naht steht schon

Das war die Frage, die Martin als „den eigentlichen Aufwand" bezeichnet
hat: 33 Stellen in `RadioModel` führen den Empfangsweg heute durch WDSP,
und ein Flex braucht daneben einen Weg, der **fertigen Ton und fertige
Panadapter-Daten** entgegennimmt.

Nachgezählt am 2026-10-18: es sind **34 Funktionen** in `RadioModel.cpp`,
die `RxChannel`/`rxChannel` anfassen (nach Funktionsnamen entdoppelt —
seine 33 trifft auf eins).

**Der Befund ist aber ein anderer als erwartet: keine dieser 34 Stellen
muss angefasst werden.** Die Naht existiert, und sie ist zweimal benutzt.

### 5.1 Der Tonweg

`AudioEngine` hat genau einen Punkt, an dem fertiger Ton hereinkommt:

```
rxBlockReady(int sliceId, const float* samples, int frames)
```

Dort landet WDSPs Ausgang — und dort landen auch schon zwei fremde
Quellen, jede über ihren eigenen Eingang:

| Eingang | Quelle | Was er tut |
|---|---|---|
| `feedKiwiSdrAudioData(sliceId, pcm24kStereoFloat)` | KiwiSDR | PCM annehmen, wandeln, `rxBlockReady` |
| `feedSunSdrAudioData(sliceId, interleavedStereo, frames, sourceRateHz)` | SunSDR2 QRP | Resampler je Rate anlegen, dann `rxBlockReady` |

`feedSunSdrAudioData` ist **40 Zeilen** (`AudioEngine.cpp:2411-2448`), und
davon sind zwei Drittel Vorsicht: keine Rate ⇒ kein Wandler (sonst wird
er mit 0 Hz gebaut), Rate geändert ⇒ Wandler neu (TCI darf
`audio_samplerate` jederzeit umstellen). Ein `feedFlexAudioData` ist
dasselbe Muster, ein drittes Mal.

### 5.2 Der Bildweg

`SpectrumWidget` braucht für eine fremde Quelle **keine zweite
Rohrleitung** — das steht dort schon so, mit Begründung
(`SpectrumWidget.h:1405-1421`):

> Ein KiwiSDR liefert seinen Wasserfall als fertige dBm-Werte über einen
> eigenen Frequenzausschnitt — nicht als I/Q um eine DDC-Mitte. Trotzdem
> braucht es dafür KEINE zweite Rohrleitung. […] Setzt man `m_ddcCenterHz`
> und `m_sampleRateHz` auf den Kiwi-Ausschnitt, ist der Kiwi einfach eine
> weitere Quelle von Bins — und Zoom, CTUN, Detektor, Mittelung,
> Wasserfall und Rücklauf arbeiten unverändert.

Die zwei Zugänge dafür sind da:

* `updateKiwiSpectrumDbm(binsDbm, …)` — fertige dBm, kein FFTEngine, kein
  zweiter Eichversatz (der Kiwi eicht selbst).
* `setKiwiDisplaySource(bool)` — solange sie steht, werden Bins **vom
  Gerät verworfen**, damit nicht zwei Quellen in denselben Panadapter
  schreiben.

Ein Flex liefert ebenfalls fertige Werte über einen eigenen Ausschnitt.
Der Weg ist derselbe; es braucht einen dritten Namen, nicht eine dritte
Rohrleitung.

### 5.3 Was daraus folgt

Ein Flex-Empfänger ist in Longpath eine **Scheibe ohne `RxChannel`**:
`SliceModel` führt Frequenz, Betriebsart und Filter wie immer, aber es
hängt kein WDSP-Kanal daran, und Ton und Bild kommen von außen an den
zwei Eingängen oben. Deshalb bleiben die 34 Stellen unberührt — sie
fragen alle nach einem `RxChannel`, und für eine Flex-Scheibe gibt es
keinen.

**Das Sicherheitstor ist auch schon da, und es muss wachsen.** Die
SunSDR-Arbeit hat eine Regel hinterlassen, die aus jeder Richtung
erzwungen wird (Ton, Panadapter-Zuordnung und Steuerung, nicht nur
Steuerung — ein früherer Durchgang hatte nur die Steuerung abgesichert
und wurde nach einer feindseligen Durchsicht nachgezogen):

> Eine Scheibe mit echter DDC-Bindung (`streamIndex() >= 0` — ein
> wirkliches, womöglich sendefähiges Gerät) wird aus diesem Weg **nie**
> gefüttert oder gesteuert.

`MainWindow_SunSdr.cpp:338 sunSdrControllableSlice()` ist das eine Tor,
durch das jeder Verbraucher muss; `:257` prüft zusätzlich
`isKiwiClaimed(slice)`. Eine dritte Quelle heißt: ein `isFlexClaimed`
daneben, in **demselben** Tor — nicht ein zweites Tor.

### 5.4 Der Umfang, den Stufe 2 wirklich hat

| Stück | Vorlage | Umfang |
|---|---|---|
| VITA-49-Empfang | `PanadapterStream.{h,cpp}` [@d58e2b8a] | 2279 Zeilen |
| Opus-Dekoder für den Ton | `OpusCodec.h` [@d58e2b8a] | 62 Zeilen Hülle; Opus selbst liegt schon im Baum (für RADE) |
| `feedFlexAudioData` | `feedSunSdrAudioData` | ~40 Zeilen |
| Bildweg | `updateKiwiSpectrumDbm` + `setKiwiDisplaySource` | dritter Name, keine neue Leitung |
| Verdrahtung im Fenster | `MainWindow_KiwiSdr.cpp` 875 / `MainWindow_SunSdr.cpp` 849 | derselbe Rahmen |
| Sicherheitstor | `sunSdrControllableSlice()` + `isKiwiClaimed` | ein `isFlexClaimed` im selben Tor |

Also: **VITA-49 ist das Stück Arbeit, nicht die Naht.** Die Naht ist ein
drittes Mal dasselbe Muster. AetherSDRs Antwort auf dieselbe Frage war
`IRadioBackend` (1086 Zeilen) — eine Schnittstelle, hinter der ein
Backend entweder rechnet oder fertige Daten liefert. Longpath braucht sie
nicht, weil es die Trennung schon an den zwei Eingängen hat, wo sie
praktisch anfällt.

Was dabei **offen** bleibt und keine Vorlage hat: eine Flex-Scheibe hat
radio-seitige DSP-Regler (NR, NB, ANF, AGC rechnen auf dem Gerät), die
Longpath heute als WDSP-Regler zeigt. Welche Oberfläche eine Scheibe
bekommt, deren Regler nach außen gehen statt in WDSP, ist eine
Gestaltungsfrage und gehört Martin — nicht dieser Naht.

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
| GUI-Anmeldung, `slice list`, Anlegen einer Vorgabescheibe | `tst_flex_stufe1`, **Rundlauf durch den echten Socket** gegen eine falsche Maschine auf 127.0.0.1 — Reihenfolge der Befehle, zurückgemeldete Kennung, Rückfall `panafall` → `panadapter create`, „vorhandene Scheibe nicht verdoppeln", abgelehnte Anmeldung, `setMayCreateDefaultSlice(false)` |
| Kennungen und Listen aus Antworten | `tst_flex_stufe1`; die drei Antwortformen von `display panafall create` stammen aus AetherSDRs eigenem Prüfstand (`radio_status_ownership_test.cpp:354-362 [@d58e2b8a]`), nicht aus einer Annahme — siehe §6.1 |
| **Frequenz am echten Gerät** | **BELEGT, 2026-10-18 13:40** — GUI-Anmeldung durch, `slice list` = [0], `slice 0 RF_frequency=14.100000` gelesen, nichts auf dem Gerät zurückgelassen. Protokoll im Nachtrag zu §4. |
| Zeile in der Geräteliste | `tst_flex_zeile_in_der_geraeteliste` — Platine „FLEX-8400", Spalte „Flex", IP, In-Use; ein zweiter Ruf führt die Zeile nach statt sie zu verdoppeln; ein Flex und ein OpenHPSDR stehen nebeneinander richtig beschriftet. **Mit Gegenprobe:** ohne die zwei `case`-Zweige meldet der Prüfstand „Unknown" und „P1" — genau der SunSDR-Fehler vom 2026-08-26. Live **nicht** gesehen (siehe §6.2). |

40 von 40 Prüffällen grün.

### 6.1 Ein geratenes Drahtformat, vom Prüfstand gefangen

Für den Rundlauf habe ich die Antwort auf `display panafall create`
zuerst **erfunden**: `pan=0x40000000,wf=0x42000000`. Der Prüfstand fiel
durch, und das war richtig — die Schlüsselform ist an **Leerzeichen**
getrennt und der zweite Schlüssel heißt `waterfall`, nicht `wf`. Richtig
sind drei Formen, alle aus AetherSDRs eigenem Prüfstand:

```
0x40000000,0x42000000                      Kommaform (so antwortet die Maschine)
40000000,0x42000000                        dasselbe, ungefüllt
pan=0x40000001 waterfall=0x42000001        Schlüsselform, an Leerzeichen
```

Der Leser fällt bei der erfundenen Mischform **zu** statt eine halbe
Kennung zu melden; das steht jetzt als eigener Prüffall drin.

---

## 7. Offen

1. *(erledigt 13:40)* Die GUI-Anmeldung am echten Gerät gefahren, die
   Frequenz gelesen — siehe Nachtrag zu §4.
2. *(erledigt)* Die Kennung wird dauerhaft gemerkt — siehe §8.
3. Die Zeile in der Geräteliste live ansehen — inhaltlich jetzt durch
   `tst_flex_zeile_in_der_geraeteliste` samt Gegenprobe abgedeckt, der
   Blick auf das echte Fenster fehlt weiter.
4. `V1.4.0.0` vs. `version=4.3.16.42034`: wenn eine Fassung angezeigt
   wird, die aus dem Ruf nehmen, nicht die aus dem Kanal.
5. **Stufe 2:** VITA-49 und die zwei Eingänge aus §5.1/§5.2. Der Draht
   dafür ist am 2026-10-18 gemessen und steht in
   `docs/protocols/flex-vita49-gemessen.md` — Kopf, beide Unterköpfe,
   Rahmenaufbau, und die zwei Stellen, an denen man sich sicher irrt
   (Bins sind Y-Pixel, nicht dBm; die Kachelfrequenzen sind Festkomma
   Hz × 2²⁰). Der **Tonweg fehlt dort** und braucht einen eigenen
   Messlauf, bevor er portiert wird.

Nicht mehr offen: das Phantom-Flex (§3.3) — auf main behoben, `e5515581`
/ PR #272. Und die Richtungsfrage aus §4 — entschieden: (b).

---

## 8. Die Verdrahtung: `RadioModel` fährt den Kanal

Nachgelegt am 2026-10-18, nachdem die Richtung entschieden war.

`RadioModel::connectToRadio()` zweigt **als Erstes** ab:

```cpp
if (info.protocol == ProtocolVersion::Flex) {
    m_lastRadioInfo = info;
    m_intentionalDisconnect = false;
    connectFlexRadio(info);
    return;
}
```

Der Abzweig sitzt vor allem Übrigen, weil alles darunter OpenHPSDR ist —
`HardwareProfile`, Alex-Antennenwege, OC-Matrix, Platinenfähigkeiten,
Dämpfungsbereiche — und davon gilt für ein Flex nichts. `connection()`
bleibt `nullptr`; ein Prüffall hält das fest.

`connectFlexRadio()` legt den Kanal an und verdrahtet fünf Dinge:

| Signal | Was passiert |
|---|---|
| `guiClientIdChosen` | in `AppSettings` unter `FlexGuiClientId` schreiben |
| `guiClientRegistered` | Zustand → `Connected`, `flexGuiClientRegistered` weiter |
| `guiClientRejected` | Zustand → `Disconnected` **und** `connectAttemptFailed` mit dem Code der Maschine |
| `sliceFrequencyChanged` | als `flexSliceFrequencyChanged` weitergereicht |
| `disconnected` / `errorOccurred` | Zustand → `Disconnected` |

Drei Entscheidungen darin:

* **Die GUI-Kennung liegt global, nicht per MAC.** Sie identifiziert
  *Longpath* gegenüber jeder Maschine, nicht ein Gerät — anders als die
  übrigen Hardware-Einstellungen, die per MAC liegen. So wie AetherSDR es
  auch hält (`AppSettings::persistentGuiClientId` [@d58e2b8a]).
* **`disconnectFromRadio()` schließt den Flex-Kanal mit.** Er hängt nicht
  an `m_connection`, also räumt `teardownConnection()` ihn nicht ab —
  ohne die zusätzliche Zeile bliebe Longpath nach „Disconnect" als
  GUI-Client auf der Maschine angemeldet und hielte sie besetzt.
* **Die Frequenz wird weitergereicht, nicht eingespeist.** Wohin sie im
  Modell gehört, entscheidet Stufe 2 zusammen mit Ton und Bild (§5).
  `flexSliceFrequencyChanged` hört heute nur das Protokoll.

### Und die Leiste sagt, was wirklich läuft

`ConnectionPanel`s Statusleiste hatte schon einen Zweig für „verbunden,
aber ohne `RadioConnection`" — genau der Flex-Fall. Er zeigt jetzt:

```
Connected — FLEX-8400 (Martin)  (command channel only, no audio yet)
```

Ein bloßes „Connected" wäre irreführend: der Betreiber sieht ein
verbundenes Gerät und hört nichts, und das sieht aus wie ein Fehler.

### Geprüft

Drei Prüffälle fahren den ganzen Weg durch ein echtes `RadioModel` gegen
die falsche Maschine: verbinden → anmelden → leere `slice list` →
Panadapter → Scheibe → **Frequenz 14.074 MHz gelesen** → Kennung
gespeichert → trennen. Dazu: eine gespeicherte Kennung wird
wiederverwendet, und ein Flex erzeugt keine `RadioConnection`.

43 + 5 = **48 Prüffälle grün** (`tst_flex_stufe1` 43, `tst_flex_zeile_in_der_geraeteliste` 5).

### 6.2 Warum die Zeile nicht live angesehen wurde

Zweimal nicht möglich, aus zwei verschiedenen Gründen: zuerst lief
Longpath (PID 76168) und eine zweite Fassung daneben hätte den Betrieb
gestört; danach war die 8400 nicht mehr erreichbar, es wäre also auch
keine Zeile erschienen. Ein Blick, den man nicht tun kann, ist kein
Beleg — deshalb der Prüfstand mit Gegenprobe. Der Blick bleibt trotzdem
ausstehend (§7).
