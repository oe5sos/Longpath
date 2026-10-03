# Die QRP auf Augenhöhe mit Anvelina und ANAN

**Auftrag, 2026-10-02:** „er soll am stand von anvelina und anan sein,
absolut gleichwertig."

Dieses Blatt sagt erstens, wo die QRP heute wirklich steht — gemessen am
Code, nicht geschätzt —, und zweitens in welcher Reihenfolge die Lücken
zugehen. Es ist ein Fahrplan über mehrere Durchgänge, kein Durchgang.

## Woran gemessen wird

| | ANAN 10E | Anvelina Pro 3 | SunSDR2 QRP |
| --- | --- | --- | --- |
| Treiber | `P1RadioConnection` | `P2RadioConnection` | `SunSdrRadioConnection` |
| Zeilen | 4385 | 3774 | **1500** |
| Signale nach oben | 13 | 13 | **4** |
| Leere Pflichtmethoden | 0 | 0 | **16** |

Die Zeilenzahl ist kein Maß für Güte, die anderen beiden Zeilen sind es:
jedes Signal ist eine Meldung, die der Betreiber am Bildschirm sieht, und
jede leere Methode ein Knopf in Longpath, der beim QRP ins Leere greift.

## Was fehlt — in drei Sorten getrennt

Die erste Liste zählte sechzehn leere Methoden. Ein Teil davon ist gar
keine Lücke: die QRP hat die Hardware nicht, um die es geht. Das steht in
`BoardCapabilities.cpp` (`kSunSdr2Qrp`) und ist dort belegt. Ohne diese
Trennung arbeitet man an Dingen, die es am Gerät nie geben wird.

### 1. Entfällt — die QRP hat es nicht (kein Arbeitsaufwand)

| Fehlt gegenüber ANAN | Warum es entfällt |
| --- | --- |
| `setPuresignalRun`, `psPairedIqDataReceived` | `hasPureSignal = false`; ExpertSDR selbst blendet PS aus |
| `widebandFrameReady` | `widebandAdcs = 0`, kein Breitbandstrom dokumentiert |
| Diversity-Empfang | `hasDiversityReceiver = false` |
| Alex-Filter, Alex-TX-Wege, `setAlexRxBpf` | `hasAlexFilters/hasAlexTxRouting = false`; drei feste Buchsen (A1 2 m, A2/A3 KW) statt Filtermatrix |
| `setUserDigOut`, `setTrxRelay` | `ocOutputCount = 0`, kein Penny/OC-Board auf diesem Protokoll |
| `setPreamp` (an/aus) | Anderes Modell: ein Opcode mit vier festen Stufen (−20/−10/0/+10 dB) — als `setPreampModeIndex` **gebaut** |
| `setWatchdogEnabled` | HPSDR-FPGA-Wachhund; die QRP hält die Verbindung über Blockantwort/Lebenszeichen, beides gebaut |
| PA-Profil, Bandbreitenwächter, Mithörton | `hasPaProfile/hasBandwidthMonitor/hasSidetoneGenerator = false` |
| Vollduplex | „MOX on SunSDR shuts down the RX LO" — die Hardware kann es nicht |

### 2. Reine Software — baubar ohne Antenne, ohne Mitschnitt, ohne Gerät

| Was | Stand |
| --- | --- |
| **Mithören**: aufnehmen, was das Gerät meldet | **erledigt**, PR #150 |
| `iqPacketLoss`, `iqSequenceGap` nach oben melden | Der Treiber hat die Folgenummern und zählt sie nicht aus — ANAN meldet beides. Rein rechnerisch, kein Protokollwissen nötig |
| Veralteter Kommentar an `setSampleRate` (nennt die widerlegten 312 500 Hz) | Textfehler, irreführend beim Lesen |
| `setAntennaRouting` auf die drei Buchsen (A1/A2/A3) | Opcode 0x15, Rahmenbauer liegt fertig. **Nicht bench-bestätigt** — die Selektorbytes sind aus zitierbarer Quelle, aber an der QRP nie geprüft, und A3 hat einen RX/TX-Split (RX 0x03, TX 0x02). Baubar, Live-Prüfung nötig |

### 2a. Zwei Korrekturen an dieser Einteilung (2026-10-02, beim Bauen gefunden)

Zwei Punkte standen zuerst unter „reine Software". Der Code selbst sagt,
warum sie dort nicht hingehören — und beide Fehleinschätzungen hätten am
Gerät wehgetan:

* **`setTxDrive` gehört zu Sorte 4, nicht 2.** `buildDriveFrame()` trägt
  die ausdrückliche Anweisung „DO NOT WIRE THIS INTO setTxDrive() OR ANY
  OTHER CALLER YET": der Leistungsbyte darf nicht hinausgehen, solange es
  keine QRP-eigene Leistungs-Kalibriertabelle gibt. Die einzige bekannte
  ist DX/PRO-Hardware, nur 40 m, und im Entwurf als „very likely wrong
  for a QRP" vermerkt. Es gibt sogar einen Prüfstand, der **null
  Produktions-Aufrufstellen** erzwingt (`tst_sunsdr_protocol.cpp`,
  `buildDriveFrameHasNoProductionCallSites`). Das braucht einen
  Messaufbau am Ausgang, also den Dummy-Load.

* **`setTxFrequency` gehört zu Sorte 3, nicht 2.** Longpath stellt den
  Empfang schon über **denselben** Opcode ein, der im DX-Schema der
  primäre/TX-VFO ist: `setReceiverFrequency()` schickt 0x07 (DDC) **und**
  0x08 (VFO), und 0x08 entspricht ArtemisSDRs „freq, primary/TX VFO".
  Eine Sendefrequenz naiv über 0x08 zu setzen hätte also beim Senden die
  **Empfangsfrequenz mitgezogen** — Split und XIT wären kaputt, und zwar
  unsichtbar, bis man es am Gerät hört. Wie ExpertSDR2 zwei Frequenzen
  getrennt hält, muss der Mitschnitt zeigen.

### 3. Braucht einen Mitschnitt mit ExpertSDR2 — ohne Antenne, zwei Minuten am Gerät

Hier fehlt kein Code, sondern **Wissen**: welcher Opcode welchen Wert
trägt. Das Mithören aus PR #150 liefert einen Teil davon im normalen
Betrieb; der Rest braucht die Gegenprobe, weil nur ExpertSDR2 das Gerät
dazu bringt, die Sachen überhaupt zu schicken.

| Was | Was dafür gebraucht wird |
| --- | --- |
| **Der Verbindungs-Dialog** (~20 Rahmen gegen unseren einen) — Wurzel der Achtfachung, die einzige Lücke, die hörbar ist | Mitschnitt beim Verbinden |
| `meterDataReceived` (S-Meter) | Welcher Rahmen den Pegel trägt |
| `adcOverflow` (Übersteuerung) | Vermutlich eines der Zustandsbytes — Mithören kann es zeigen |
| `micPttFromRadio` (PTT am Mikrofon) | Taste drücken, Rahmen vergleichen |
| Spannung/Strom/PA-Temperatur | Unbestätigt, ob die QRP das überhaupt meldet (`hasPaVoltsTelemetry/hasPaAmpsTelemetry = false`, „not confirmed") |
| Abtastrate über 48 kHz | Opcode unbekannt; Mitschnitt, bei dem ExpertSDR2 die Rate umstellt |
| Zweiter Empfänger | Offen, ob die Hardware es kann. Der Startablauf setzt `RX2_ENABLE=0` (Opcode 0x1B) — das beweist nur, dass RX2 abgeschaltet *wird* |
| Mikrofonweg (`setMicBoost`, `setLineIn`, `setLineInGain`, Buchsendetails) | Opcode 0x21 (MIC_SOURCE) ist bestätigt, die Werte nicht |

### 3a. Die Opcode-Nummern der QRP sind nicht die der DX (2026-10-03)

Aus den dreizehn mitgeschnittenen Rahmen und ArtemisSDRs eigener
Opcode-Tabelle (`sunsdr.h`) ergibt sich ein Befund, der **Schritt 4
umsortiert**:

| Befehl | QRP (am Gerät gemessen) | DX (ArtemisSDR) |
| --- | --- | --- |
| Vorverstärker | `0x04`, Werte 0…3 | `0x05`, Werte 0x80…0x83 |
| DDC-Frequenz | `0x07` | `0x08` (`FREQ_COMP`) |
| VFO-Frequenz | `0x08` | `0x09` (`FREQ_PRIMARY`) |
| erstes Byte des Rahmens | `0x03` | `0x32` |

Dreimal liegt die QRP **um eins darunter**, und die Nutzlast ist anders
codiert. Longpaths vier TX-Rahmenbauer tragen aber unverändert die
DX-Nummern: MOX `0x06`, Antenne `0x15`, Drive `0x17`, PA `0x24`.

**Daraus folgt ausdrücklich nicht „minus eins rechnen".** `0x01` passt
ohne Versatz zu DX' `STATE_SYNC`, die QRP hat also eine eigene Tabelle,
die in Teilen übereinstimmt. Die einzige haltbare Regel ist: **jede
Nummer einzeln bestätigen, bevor sie an ein Funkgerät geht.**

Was sonst passiert, ist durchgerechnet: schickt Longpath `0x06` in der
Annahme „MOX" und bedeutet es bei der QRP etwas anderes, geht beim ersten
Sendeversuch etwas Unbekanntes ans Gerät — genau der Fehler vom
2026-09-23, als unzugeordnete Opcodes an die QRP geschickt wurden.
Bestätigt wird darum **nicht durch Probieren am Gerät**, sondern durch
einen Mitschnitt, in dem ExpertSDR2 sendet.

Festgehalten ist das mit einer Sperre: `tst_sunsdr_protocol` erzwingt für
alle vier DX-stämmigen Rahmenbauer **null Produktions-Aufrufstellen**
(vorher nur für `buildDriveFrame`), mit zwei Gegenproben, damit die
Sperre nicht blind grün ist.

### 4. Braucht einen Abschluss am Ausgang — nicht zwingend eine Antenne

Senden: `sendTxIq` auf den Draht, MOX bis zum Gerät, Leistung, Zeitlage
der Pakete. Die Schritte 4–6 des bestehenden TX-Plans. **Ein 50-Ohm-
Abschluss (Dummy-Load) genügt** — bei 5 W braucht es dafür keine Antenne,
nur darf der Ausgang nicht offen bleiben. Bis dahin wird hier nichts
angefasst.

## Reihenfolge

1. **Mithören — erledigt in diesem Durchgang.** Beide Lesepfade nehmen
   jetzt auf, was vom Gerät kommt: je Rahmensorte Anzahl, erste und
   letzte Nutzlast, Anzahl der Änderungen; Bericht ins Log beim Trennen.
   Eine Nutzlast, die sich im Betrieb ändert, ist ein Messwert; eine, die
   konstant bleibt, ist Ausstattung. Das trennt die acht Opcodes auf,
   während der Betreiber normal funkt — ohne sudo, ohne ExpertSDR2
   daneben, ohne Termin an der Bank.
2. **Der Verbindungs-Dialog.** Die rund zwanzig Rahmen, die ExpertSDR2
   beim Verbinden schickt, nachbauen (`LONGPATH_SUNSDR_PRE`/`_EXTRA`
   stehen schon dafür bereit). Das ist die Wurzel von E und sehr
   wahrscheinlich die Tür zu mehreren der acht Opcodes — und es ist die
   einzige Lücke, die der Betreiber *hört*.
3. **Die Meldeseite.** Mit 1 und 2 als Grundlage: S-Meter, Übersteuerung,
   Spannung, Mikrofon-PTT vom Gerät nach oben geben, dieselben Signale
   wie P2.
4. **Senden.** Schritte 4–6 des bestehenden TX-Plans. Zuletzt, weil hier
   zum ersten Mal HF entsteht und jeder Versuch eine Freigabe des
   Betreibers braucht. **Beginnt nicht mit dem Verdrahten, sondern mit der
   Bestätigung der Opcode-Nummern** (siehe 3a): die vier TX-Rahmenbauer
   tragen DX-Nummern, und bei drei gemessenen Befehlen liegt die QRP um
   eins darunter.
5. **Der Rest.** Abtastrate, zweiter Empfänger, Antennenumschaltung,
   Mikrofon-Zubehör.

Die Reihenfolge ist nicht nach Aufwand sortiert, sondern danach, was das
Nächste erst möglich macht: ohne 1 ist 3 geraten, ohne 2 ist der Strom
falsch, und ohne richtigen Strom ist Senden eine Wette.


---

# Stand am Ende des 2026-10-03 — neu bilanziert

Nach einem Tag Messen am echten Gerät (ohne Antenne, ohne HF) ist die
Liste nicht nur kürzer, sondern auch anders geschnitten als gestern.

## Die „4 von 13 Meldungen" war eine irreführende Zahl

Sie zählte Signale, nicht Fähigkeiten. Aufgeschlüsselt:

| Fehlende Meldung | Was wirklich gilt |
| --- | --- |
| `iqPacketLoss`, `iqSequenceGap` | **gebaut** am 2026-10-02/03 |
| `psPairedIqDataReceived` | entfällt — kein PureSignal in der Hardware |
| `widebandFrameReady` | entfällt — `widebandAdcs = 0` |
| `meterDataReceived` | trägt **Vorwärts- und Rückwärtsleistung**, also reine **Sende**messwerte → gehört zu Schritt 4, nicht zum Empfang |
| `paTelemetryUpdated` | PA-Temperatur und -Strom → ebenfalls Senden |
| `supplyVoltsChanged`, `userAdc0Changed` | unbestätigt, ob die QRP überhaupt eine Spannung meldet |
| **`adcOverflow`** | **echte Lücke im Empfang** |
| **`micPttFromRadio`** | **echte Lücke** |

Im **Empfang** fehlen damit noch **zwei** Meldungen, nicht neun. Das
S-Meter rechnet Longpath ohnehin selbst aus dem I/Q — ein Geräte-S-Meter
braucht es dafür nicht.

**Noch am selben Tag auf eine reduziert:** `adcOverflow` ist gebaut, und
zwar ohne Protokollwissen — aus dem Signal selbst, denn eine Probe am
Anschlag ist eine Probe am Anschlag. Am Gerät auf drei Bändern
gegengemessen: keine Fehlalarme (der Rauschflur ohne Antenne liegt sechs
Zehnerpotenzen unter der Schwelle). **Offen im Empfang ist damit nur noch
`micPttFromRadio`** — und das braucht den Mitschnitt, weil die QRP den
Zustand nirgends von sich aus meldet.

## Was am 2026-10-03 am Gerät geklärt wurde

| Frage | Antwort |
| --- | --- |
| Meldet das Gerät von sich aus Messwerte? | **Nein.** Über zehn Minuten kein unaufgeforderter Steuerrahmen, Zustandsbytes im Strom konstant |
| Gibt es einen Weg, etwas abzufragen? | **Ja**, `0x0c` → 320 Byte, 39 Doubles. Aber **statisch** (band- und zeitunabhängig), also keine Messwertquelle |
| Ist das I/Q echt? | **Ja**, Q ungleich null 21–24 %, sobald `0x07` hinausgegangen ist |
| Tritt die Achtfachung noch auf? | **Nein**, 240 Nummern/s bei 1,20 Kopien — die Blockantwort wirkt |
| Schaltet `0x18` (Haupttakt) die Abtastrate? | **Nein**, keine messbare Wirkung |
| Kann die QRP einen zweiten Empfänger? | Der **Platz wird akzeptiert und quittiert** (`0x07 sub=1`, dreimal belegt), bleibt aber **stumm** — der Einschalter fehlt |
| Quittiert das Gerät Steuerrahmen? | **Ja**, jeden angenommenen, binnen 15–50 ms. Und es gehen welche **verloren**: ein VFO-Frequenzrahmen blieb unquittiert |

## Was jetzt wirklich noch fehlt

1. **Zwei Minuten Mitschnitt mit ExpertSDR2** — und zwar nicht mehr „für
   den Verbindungsablauf" allgemein, sondern für drei konkrete Fragen:
   welcher Rahmen RX2 einschaltet, welcher die Abtastrate stellt, und
   welche Rahmen überhaupt noch dazugehören (die dreizehn sind
   unvollständig).
2. **Ein 50-Ohm-Abschluss** für alles Sendeseitige — und davor die
   Bestätigung der Opcode-Nummern (Abschnitt 3a).
3. ~~Übersteuerung und~~ **Mikrofon-PTT**: Übersteuerung ist am
   2026-10-03 gebaut (aus dem I/Q, ohne Protokollwissen). Für das
   Mikrofon-PTT gilt weiter: das Gerät meldet den Zustand nicht von
   selbst, also steckt er in einer Abfrage oder in einem der unbekannten
   Rahmen — Mitschnitt.
4. **Unquittierte Rahmen nachschicken** — die eine Stelle, an der heute
   ein echter Mangel gefunden wurde (eine verlorene Frequenz bleibt
   unbemerkt). Braucht eine Entscheidung, weil der Treiber dann von
   selbst Rahmen wiederholt.
