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

**Diese Tabelle ist vom 2026-10-02 und damit überholt.** Am 2026-10-04
nachgezählt, nicht geschätzt:

| | ANAN 10E (P1) | Anvelina (P2) | SunSDR2 QRP |
| --- | --- | --- | --- |
| Zeilen | 4385 | 3774 | **2692** |
| Signale nach oben | 10 | 12 | **8** |
| Leere Rümpfe | 0 | 0 | **14** |

Und die „leeren Rümpfe" sind nicht alle Arbeit: vier davon gibt es an
diesem Gerät gar nicht (`setTrxRelay`, `setUserDigOut`,
`setPuresignalRun`, `setWatchdogEnabled`), zwei gehören zum Senden und
sieben zum Mikrofonweg — beide Gruppen warten nicht auf Arbeit, sondern
auf **Bestätigung der Opcode-Nummern**. Die Einteilung mit Begründung
steht seit `6f15428a` im Quelltext selbst.

Die Zahl, die zählt, ist eine andere: **im Empfang ist die QRP
gleichwertig.** Was noch fehlt, ist Senden.

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

---

# Neuer Mangel, am 2026-10-03 abends gemessen: Longpath sagt beim Trennen nichts

`SunSdrRadioConnection::disconnect()` schickt dem Gerät **keinen einzigen
Rahmen** — es schließt Sockets, hält Timer an, räumt auf. Dem Funkgerät
wird nie gesagt, dass der Strom aufhören soll.

**Was das in Zahlen heißt**, mit `tcpdump` nach einem Longpath-Ende
gemessen (46 Sekunden Mitschnitt, niemand hörte zu):

| | Pakete/s | Kopien je Folgenummer |
| --- | --- | --- |
| Reststrom, niemand quittiert | **1940** | **8,1** |
| Longpath im Betrieb, mit Blockantwort | 240 | 1,00 |

Zwei Dinge auf einmal:

1. **Die Achtfachung ist abschließend erklärt.** Sie ist keine Eigenart
   des Geräts, sondern genau die Folge fehlender Quittierung — 8,1 Kopien
   ohne, 1,00 mit Blockantwort. Damit ist die Frage vom 2026-09-23 zu.
2. **Die QRP streamt nach dem Trennen unbegrenzt weiter**, mit 2,3 MB/s
   ins Leere, und der Mac antwortet auf jedes Paket mit ICMP „port
   unreachable". Bekannt war das als Kuriosum („85 leftover I/Q packets",
   `setFixedPortBindingEnabledForTest`) — es sind aber nicht 85 Pakete,
   sondern ein Dauerzustand bis zum Ausschalten des Geräts.

Und sehr wahrscheinlich ist das der Grund, warum ExpertSDR2 am selben
Abend nicht verbinden konnte: das Gerät stand noch im Streaming-Zustand
der vorigen Sitzung.

**Was fehlt, ist der Stopp-Befehl.** ArtemisSDR führt für die DX
`SUNSDR_OP_POWER_OFF 0x02`, und die Boot-Folge dort ruft ihn beim Umbau
der Empfangswege auf. Welche Nummer das bei der QRP ist, wissen wir
nicht, und geraten wird sie nicht (Abschnitt 3a).

**Der Mitschnitt dafür ist der leichteste von allen:** ExpertSDR2
verbinden lassen und dann **beenden**. Der letzte Rahmen, der hinausgeht,
bevor der Strom verstummt, ist der Stopp-Befehl. Damit wäre `disconnect()`
vollständig — und das Gerät nach jedem Longpath-Ende still.

---

# Stand am 2026-10-04 — nach dem ersten Tag in der laufenden App

Bis gestern wurde alles am Prüfstand und am Messlauf gemessen. Heute lief
Longpath selbst gegen die QRP, über die Automationsbrücke ferngesteuert.
Das hat drei Fehler aufgedeckt, die kein Prüfstand gezeigt hat — und
einer davon stand genau an der Stelle, die gestern als „erledigt" galt.

## Was sich an der Liste oben ändert

| Posten | Neuer Stand |
| --- | --- |
| Veralteter Kommentar an `setSampleRate` (312 500 Hz) | **erledigt** — beim Umbau am 2026-10-03 verschwunden |
| `setAntennaRouting` (Opcode `0x15`) | **verdrahtet, aber stumm** (`8e54decb`). Scharf nur mit `LONGPATH_SUNSDR_ANTENNE=1`; die Auswahlbytes stammen von der DX/PRO und sind an der QRP nicht bestätigt — siehe Abschnitt 3a, die Opcodes der QRP sind andere |
| 96 kHz | kam in der **App** nie am Gerät an: `RadioModel` schiebt die Rate vor dem Verbinden hinein, der Sitzungs-Reset warf sie weg (`88844941`) |

## Die drei Fehler vom 2026-10-04

1. **Die eingestellte Rate wurde beim Verbinden weggeworfen.** Longpath
   meldete `Connecting with sampleRate= 96000`, stellte WDSP darauf ein —
   und das Gerät streamte mit 48 (Stromkopf `0100`, 240 Nummern/s). Also
   Daten einer Rate in einem Kanal einer anderen, derselbe Riss wie am
   2026-09-24. Behoben; danach `0200` und 960 Nummern/s.

   **Warum kein Prüfstand das fing:** die vorhandene Prüfung rief
   `setSampleRate` **nach** `connectToRadio` — in der Reihenfolge, die
   geht, nicht in der, die die Anwendung nimmt.

2. **Ein toter Lautsprecher-Ausgang galt als offen** (`c8761850`).
   `isOpen()` ist bei PortAudioBus ein Zeigervergleich; stirbt der Strom
   darunter, schreibt Longpath weiter hinein, ohne dass etwas auffällt.
   Jetzt fragt `ensureSpeakersOpen` über `Pa_IsStreamActive` nach.

3. **Der Abmelde-Rahmen wurde nicht nachgeschickt** (`9c48cb5c`). Die QRP
   bedient **einen** Client und hält die Sitzung fest. Blieb der Stopp
   unquittiert — oder wurde eine Instanz hart beendet —, nahm das Gerät
   **niemanden mehr an**, und Longpath meldete „no beacon reply". Das hat
   den Betreiber eine halbe Stunde gekostet; erst Aus- und Einschalten
   half. Der Stopp geht jetzt bis zu dreimal hinaus, und die
   Fehlermeldung nennt diesen Fall **zuerst**.

## Was im Empfang jetzt noch fehlt

**Nichts mehr — der Empfang ist funktional vollständig.**

Die Zeile, die hier zuerst stand („unverändert eines: `micPttFromRadio`,
dafür braucht es den Mitschnitt"), war falsch: ich hatte eine ältere
Aussage abgeschrieben, ohne den Code zu prüfen. `micPttFromRadio` ist am
2026-10-02 gebaut worden (`1e4eb060`) und wird **ohne Protokollwissen**
aus dem Stromkopf abgeleitet — `0xFD` heißt, das Gerät sendet, `0xFE`
heißt Empfang. Mit Flankenerkennung (240 Pakete je Sekunde dürfen nicht
240 Meldungen ergeben) und mit Abgrenzung gegen eigenes MOX. Geprüft in
`sendezustandAmGeraetMeldetPtt`.

Offen ist nur die **Bestätigung am Gerät**, und die braucht keinen
Mitschnitt, sondern einen **50-Ω-Abschluss**: wer die Mikrofontaste
drückt, bringt das Gerät in den Sendezustand, und das gehört nicht an
eine offene Buchse.

Damit ist Martins Auftrag vom 2026-10-02 — „er soll am stand von
anvelina und anan sein, absolut gleichwertig" — **für den Empfang
erfüllt**, vorbehaltlich dieser einen Live-Bestätigung. Was bleibt, ist
das Senden, und das ist ein eigenes Kapitel (Abschnitt 4).

## Zwei offene Beobachtungen, beide ohne Antenne messbar

- **Wiederholungen bei 96 kHz:** 1,2 statt 1,0 — rund 105 überflüssige
  Pakete je Sekunde. Die Quittung wirkt (401 → 105), ist aber
  unvollständig. Den Kopf des Geräts zu spiegeln bringt nachweislich
  nichts. Nächste Versuche stehen im Verbindungsablauf-Dokument.
- **Lautstärke:** der Betreiber hört die QRP „sehr sehr leise". Die
  Umrechnung der Proben ist nachgerechnet richtig (24 Bit ins obere Ende
  eines 32-Bit-Worts, geteilt durch 2³¹), eine geräteeigene Pegel-Eichung
  gibt es bei keinem der drei Geräte. Offen, ob es an der fehlenden
  Antenne liegt oder eine echte Lücke ist — der Vergleich gegen die
  Anvelina steht noch aus und ist auf Wunsch des Betreibers vertagt.

---

# Nachtrag 2026-10-04, Nachmittag — zwei Lücken geschlossen

Beide Funde stammen aus **Richtigstellungen des Betreibers**, nicht aus
eigener Analyse. Das ist kein Zufall: zu beiden Punkten stand hier eine
Behauptung, die nie am Gerät geprüft worden war.

## Der zweite Empfänger war nie stumm

Betreiber: *„es waren immer beide rx und rx2"* — in ExpertSDR2 liefen
also stets beide. Damit war der zweite Datenstrom nie ein Rätsel.
Nachgerechnet aus seinem Mitschnitt:

| | Effektivwert | Q ungleich null |
| --- | --- | --- |
| Kanal 0 | −130,0 dBFS | 21,2 % |
| Kanal 1 | **−127,9 dBFS** | **31,5 %** |

Kanal 1 trägt echtes I/Q, sogar kräftiger als Kanal 0. Die Zeile
„wird angenommen, bleibt aber stumm" war falsch — Longpath hat ihn
weggeworfen, weil `maxReceivers = 1` stand.

Zwei Änderungen (`db9cf495`): Fähigkeiten auf zwei Empfänger, und der
Stromstart-Modus hängt jetzt an **Empfängerzahl UND Rate** statt nur an
der Rate. Ohne das hätte ein zweiter Empfänger bei 48 kHz nie Daten
bekommen können.

Am Gerät bestätigt (`b33072d9`): 480 statt 240 Nummern/s, Kanal 0 und 1
je **0 verworfen**, 0,00 % Verlust.

## Die Lautstärke war eine falsch geeichte Zahl

Der Betreiber hat es mehrfach gemeldet; entscheidend war sein Satz, dass
**ExpertSDR2 am selben Gerät ohne Antenne perfekt laut** ist. Damit war
die fehlende Antenne als Erklärung erledigt.

Der Abgleich `rxLevelTrimDb` stand auf +20,0 dB — am 2026-09-25 gemessen,
aber über den **TCI-Weg** (`rx_sensors`). Der native Treiber ist ein
anderer Weg mit anderer Skalierung. Statt eine neue Zahl zu raten wurde
der Abgleich einstellbar gemacht; der Betreiber hat **+40,0 dB**
eingestellt und bestätigt. Fest eingetragen in `c8735386`.

## Zurückgenommen: die Antennenwahl

`8e54decb` hatte `setAntennaRouting` verdrahtet (hinter einem Schalter,
standardmäßig stumm). Beim Bauen schlug ein Wächter an, den dieses
Projekt genau dafür hat: `tst_sunsdr_protocol` prüft, dass die
DX-stämmigen Sende-Rahmenbauer **keine** Aufrufstelle haben.

Der Wächter hat recht. Ein Schalter, der standardmäßig aus ist, hebt ihn
nicht auf — verdrahtet ist verdrahtet, und eine Umgebungsvariable ist
schnell gesetzt. `setAntennaRouting` ist wieder leer; `0x15` ist im
Abschluss-Prüfplan der **erste** zu bestätigende Befehl, weil er als
einziger nichts erzeugt.

---

# Der zweite Empfänger: wo die Kette wirklich endet (2026-10-04, abends)

Der Beleg vom Nachmittag (`b33072d9`) war **halb**. Gemessen war die
Treiberseite: beide Kanäle kommen an, 0 verworfen. Was er nicht zeigte:
ob Kanal 1 oben bei einem zweiten Empfänger landet.

Am Gerät nachgeholt, mit `activeRxCount = 2` und **einer** Scheibe:

    Connecting with sampleRate= 48000 ... activeRxCount= 2
    Created RX channel 0 / Created RX channel 1
    SunSdr: aktive Empfaenger -> 2
    SunSdr: Stromstart-Rahmen -> zwei Stroeme, je 48 kHz
    ReceiverManager: first feedIqData forwarded; hw= 0 -> rx0
    ReceiverManager: first feedIqData DROPPED; hw= 1  map= "hw0->rx0"

Das Gerät schickt also zwei Ströme (480 Nummern/s), der Treiber reicht
beide hoch, WDSP hat zwei Kanäle — und `ReceiverManager` kennt nur einen
Empfänger.

**Das ist kein Fehler, sondern die Bauweise.** Ein Empfänger entsteht in
`RadioModel::syncReceiverToStream`, und die wird gerufen, wenn sich eine
**Scheibe** an einen Strom bindet. Mit einer Scheibe gibt es einen
Empfänger, gleich was `activeRxCount` sagt. RX2 erscheint, sobald eine
zweite Scheibe da ist — dafür steht `maxSlices` seit heute auf 2.

Die Routenführung selbst ist geprüft (`tst_sunsdr_zweiter_empfaenger_oben`,
ohne Funkgerät): Kanal 1 landet bei Empfänger 1, mit dessen Daten, und
fällt weg, wenn es keinen zweiten gibt.

**Was offen bleibt:** mit einer Scheibe und `activeRxCount = 2` fordert
Longpath zwei Ströme an und wirft den zweiten eine Ebene höher weg —
doppelte Netzlast ohne Gegenwert. Sauberer wäre, den Stromstart-Modus an
die Zahl der **gebundenen Ströme** zu hängen statt an den gespeicherten
Wert. Bis dahin sagt die Meldung wenigstens, was fehlt, statt nur
`map="hw0->rx0"`.


---

# Bilanz am Ende des 2026-10-04

Der Empfang ist **gleichwertig**. Was heute dazukam, in der Reihenfolge,
in der es gefunden wurde:

| Was | Wie belegt |
| --- | --- |
| Die eingestellte Rate kam beim Verbinden nie am Gerät an | am Gerät: Stromkopf `0100` → `0200`, 240 → 960 Nummern/s |
| Ein toter Lautsprecher-Ausgang galt als offen | Prüfstand rot gegen die alte Fassung |
| Der Abmelde-Rahmen wurde nicht nachgeschickt | am Gerät quittiert nach Versuch 1 |
| **Lautstärke +20 → +40 dB** | vom Betreiber am Gerät eingestellt |
| **Zwei Empfänger, Ende zu Ende** | am Gerät: 480 statt 240 Nummern/s, 0 verworfen |
| Das Verbinden erholt sich selbst | fünf Anläufe à 18 s, Prüfstand rot-vor-grün |

**Was noch fehlt — und woran es hängt:**

| Offen | Hängt an |
| --- | --- |
| Mikrofon-PTT bestätigen | **50-Ω-Abschluss** (gebaut, aber nie im Sendezustand gesehen) |
| Senden überhaupt | derselbe Abschluss, davor die Opcode-Bestätigung |
| Restliche Rahmen des Verbindungsablaufs | **zwei Minuten ExpertSDR2 mit Mitschnitt**, ohne Antenne |
| Wiederholungen bei 96 kHz | derselbe Mitschnitt, aber **auf 96 kHz** |

**Was keiner mehr versuchen soll** (alles gemessen und wirkungslos):

- den Kopf der Blockantwort spiegeln
- zwei Stille-Ströme statt einem zurückschicken
- die Quittung vor das Verwerfen ziehen

**Und eine Mahnung an mich selbst:** zweimal an diesem Tag habe ich aus
**einer** Messung eine Ursache gemacht — bei den Wiederholungen und beim
Verbindungsaussetzer. Beide Male war die Zahl richtig und der Schluss
falsch. Eine Ursache braucht mehr als einen Durchgang.

---

# Bilanz am 2026-10-07 — eine Zeile dieser Liste war falsch

Zwei Zeilen weiter oben stehen so:

> | Meldet das Gerät von sich aus Messwerte? | **Nein.** |
> | Gibt es einen Weg, etwas abzufragen? | `0x0c` → statisch, keine Messwertquelle |

**Die zweite stimmt, die erste nicht.** Das Gerät meldet sehr wohl von
sich aus — nur nicht auf dem Steuerweg, auf dem gesucht wurde, sondern
im **Datenstrom**: 20 Rahmen je Sekunde mit 77 Byte, darin zwei
Gleitkommazahlen, die sich ändern. Longpath hat sie an der
Längenprüfung weggeworfen (alles unter 1210 Byte galt als unbrauchbar),
und genau deshalb konnte die Suche sie nicht finden: sie kamen die ganze
Zeit an.

Seit `032aa953` liest Longpath sie. Einzelheiten im
Verbindungsablauf-Dokument.

**Warum der Fehler zehn Tage überlebt hat**, und das ist die Lehre: die
Frage lautete „meldet das Gerät von sich aus etwas?", gesucht wurde aber
nur dort, wo Meldungen erwartet wurden — auf dem Steuerport. Der
Datenstrom galt als „I/Q und sonst nichts", weil er so gebaut war. Eine
Verneinung ist nur so weit gültig wie der Ort, an dem gesucht wurde,
und dieser Ort stand nirgends dabei.

## Was im Empfang damit jetzt wirklich fehlt

| Punkt | Stand |
| --- | --- |
| Messwerte **lesen** | gebaut (`032aa953`) |
| Messwerte **beschriftet anzeigen** | fehlt — erst wenn feststeht, welcher Wert was ist |
| Spannung und Strom | **offen**: ExpertSDR2 zeigt sie, im Mitschnitt stehen sie nirgends |
| Mikrofon-PTT am Gerät bestätigen | braucht den 50-Ω-Abschluss |

Alles andere im Empfang steht. Die Zuordnung der beiden Werte ist der
nächste Schritt und braucht entweder den Betreiber (Zahl neben
ExpertSDR2 legen) oder eine Messung, in der sich die Last ändert.
