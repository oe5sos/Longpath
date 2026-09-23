# Werkbank: Longpath gegen ein SunSDR2-QRP-Messgeraet

Der native QRP-Treiber ist am echten Geraet bewiesen (2026-08-26:
Discovery → Beacon → Zustandsrahmen → echter IQ-Strom, Panadapter,
Wasserfall und Ton, ohne dass ExpertSDR2 lief). Nur steht das Geraet
nicht immer da — und in dem Monat, in dem es nicht dastand, ist an
diesem Treiber nichts mehr geprueft worden. Am HL2 hat genau so ein
Messgeraet drei echte Fehler gefunden.

Darum gibt es jetzt eins: `tools/sunsdr_sim.py`.

**Longpath-eigen.** Nichts darin stammt aus ExpertSDR2 oder einer
anderen fremden Quelle. Der Rahmenaufbau steht in unseren eigenen
Dateien (`src/core/sunsdr/SunSdrProtocol.{h,cpp}` — Kopfzeilen,
Probenformat, Portnummern), der Ablauf in
`docs/architecture/2026-08-26-sunsdr-connection-plan.md`.

## Was es nachstellt

| | |
|---|---|
| Steuerkanal | UDP 50001 — Suchanfrage (Opcode 0x00) → Beacon; Zustandsrahmen (0x01) startet den Strom; Frequenz (0x08) wird dekodiert und protokolliert; jeder andere Opcode wird mit Nummer gemeldet |
| Datenstrom | UDP 50002 — 1210-Byte-Pakete (10 Byte Kopf + 1200 Byte Proben), 200 Probenpaare je Paket |
| Rate | 312 500 Hz → 1562,5 Pakete/s, 1,9 MB/s |
| Signal | ein Ton (Vorgabe +10 kHz, −30 dBFS) auf Rauschen (−80 dBFS) |
| Lebenszeichen | ohne Lebenszeichen verstummt der Strom nach 8 s — **wie das echte Geraet**; mit `--keepalive-timeout 0` nie |

```bash
python3 tools/sunsdr_sim.py                      # Vorgaben
python3 tools/sunsdr_sim.py --tone-offset 25000 --tone-dbfs -40
python3 tools/sunsdr_sim.py --keepalive-timeout 0
```

## Die Werkbank fahren

```bash
cmake --build build --target tst_sunsdr_sim_workbench

python3 tools/sunsdr_sim.py > /tmp/sunsdr-sim.log 2>&1 &
LONGPATH_SUNSDRSIM=127.0.0.1:50001 QT_QPA_PLATFORM=offscreen \
  ./build/tests/tst_sunsdr_sim_workbench
```

Der Pruefstand **ueberspringt sich selbst**, solange `LONGPATH_SUNSDRSIM`
nicht gesetzt ist — die CI hat kein Messgeraet.

Stationen: Verbinden (Suchanfrage → Beacon → Zustandsrahmen) →
Empfangsstrom (Blockzahl UND Probeninhalt, ein stiller Strom faellt
durch) → Frequenz setzen → zehn Sekunden Dauerlauf, in denen das
Lebenszeichen den Strom halten muss → Trennen.

## Zwei Dinge, die der Aufbau braucht

**Die festen Ports gehoeren dem Messgeraet.** Der Treiber bindet 50001
und 50002 normalerweise selbst (genau das war 2026-08-26 der erste
Live-Fehler: ein fluechtiger Port bekam nie eine Antwort). Auf einer
Maschine, auf der beide laufen, geht das nicht zweimal. Die Werkbank
ruft darum `setFixedPortBindingEnabledForTest(false)`; das Messgeraet
antwortet auf den Absenderport und lernt den Datenport aus dem ersten
Lebenszeichen des Treibers.

**Ueber die Rueckschleife gibt es keine Rundsendeadresse.** Die
Suchanfrage ging bis zum 2026-09-23 ausschliesslich an die
Rundsendeadressen der Schnittstellen — auf `lo0` gibt es keine, also
erreichte sie ein Messgeraet auf 127.0.0.1 nie. Der Treiber schickt die
Anfrage jetzt zusaetzlich geradeaus an die eingetragene Adresse. Das
ist nicht nur fuer die Werkbank gut: ein WLAN mit Client-Isolation, ein
Router mit gefilterter Rundsendung, ein Gast- oder anderes VLAN — in all
diesen Faellen kam die Anfrage nie an, obwohl die Adresse des Geraets im
Eintrag steht.

## Was noch nicht geht

Senden. `sendTxIq()` ist leer, und die TX-Opcodes sind reine Kodierer
ohne Draht. Das Messgeraet meldet ein TX-Paket, wenn eins kaeme
(„Strom: TX-Paket empfangen") — mehr kann es nicht pruefen, solange am
echten Geraet nicht gemessen wurde, was die QRP dort erwartet.

## Was die Bank am 2026-09-23 ergeben hat

Die QRP stand einen Vormittag zur Verfuegung (ohne Antenne, also nur
Empfang). Mitgeschnitten wurden 30 000 Pakete in 15,46 s ueber einen
eigenen Ethernet-Adapter (`en9`, 192.168.16.100 ↔ .200, 100 Mbit
voll-duplex). Die Datei lag bei `~/qrp-capture.pcap`.

**Die QRP schickt jeden Datenblock achtmal.**

```
29 683 IQ-Pakete in 15,46 s      = 1920 Pakete/s auf dem Draht
 3 718 verschiedene Nutzlasten   =  240 Bloecke/s  (alle 4,17 ms)
       3 703 davon exakt achtmal
       Kopien ueber ~32 ms verteilt: 11,6 / 8,0 / 4,0 / 3,0 / 2,0 / 2,0 / 2,0 ms
       und verschraenkt mit den Nachbarbloecken
```

240 Bloecke/s × 200 Probenpaare = **48 000 Proben/s**. Der Treiber
rechnete mit 312 500 Hz (`kProfileQrp`) und reichte alle acht Kopien
weiter — die Signalverarbeitung bekam also jede Probe achtmal, auf
einer Achse, die um den Faktor 6,5 danebenlag. Dass das nie auffiel,
liegt daran, dass Rauschen in jedem Massstab wie Rauschen aussieht.

Die Diagnosezeile im Treiber, die „byte-identical to the one before
them" zaehlt, meldete dabei immer 0 — die Kopien kommen eben nicht
hintereinander.

**Warum 48 kHz?** Weil unser Zustandsrahmen ein mitgeschnittener
Byte-Block vom 2026-08-26 ist, den wir unveraendert zurueckspielen. Was
immer ExpertSDR2 damals eingestellt hatte, stellt die QRP heute wieder
ein. Die Rate zu WAEHLEN braucht den Opcode dafuer — offen, siehe
`sunsdr-bench-ohne-antenne.md`.

**Nebenbei mitgemessen:**

- **Statusrahmen**, 77 Byte, Opcode 0x00, exakt 20/s. Darin zwei
  Fliesskommazahlen, die sich waehrend des Laufs bewegten: 36,5 → 37,0
  und 27,0 → 27,5 (Offset 15 und 19) — das sind Temperaturen. Dazu ein
  16-Bit-Wert um 33 560, der leicht driftet, und ein schneller Zaehler
  bei Offset 6. Longpath wirft diese Rahmen heute weg.
- **Das Lebenszeichen geht sauber hinaus**: acht Stueck in 15,5 s, also
  alle 1,93 s. Der Verbindungsabriss, der die Sitzung eroeffnet hatte,
  kam vom Messgeraet dieser Werkbank: es lief noch und hielt die festen
  Ports 50001/50002, die der Treiber selbst binden wollte. **Messgeraet
  aus, bevor echte Hardware drankommt.**

Das Messgeraet stellt diese Eigenschaften seither nach
(`--repeat 8 --block-rate 240`, Statusrahmen mit 20/s), und die
Werkbank prueft, dass der Treiber sieben von acht Kopien verwirft.

## Die Werkbank gegen das ECHTE Geraet

Mit `LONGPATH_SUNSDR_FIXED_PORTS=1` bindet der Pruefstand die festen
Ports 50001/50002 statt fluechtiger — das echte Geraet antwortet nur
dorthin (am 2026-08-26 live gelernt). Dann darf allerdings **nichts
anderes** diese Ports halten: kein zweites Longpath, kein ExpertSDR2,
kein Messgeraet.

```bash
LONGPATH_SUNSDRSIM=192.168.16.200:50001 LONGPATH_SUNSDR_FIXED_PORTS=1 \
  QT_QPA_PLATFORM=offscreen ./build/tests/tst_sunsdr_sim_workbench
```

Am 2026-09-23 so gefahren, direkt gegen die QRP:

```
STATE 3 (verbunden), 400 Proben je Block, Spitze 1,7e-05 (Rauschflur ohne Antenne)
IQ-Bloecke nach 10 s: 149 -> 2552          = 240 Bloecke/s
DOPPELT verworfen: 17 754 bei 2 552        = 7 von 8, wie am Draht gemessen
```

Der Tonpegel wird am echten Geraet nur gegen „nicht still" geprueft —
ohne Antenne liegt dort nur der eigene Rauschflur.

