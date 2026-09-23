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
