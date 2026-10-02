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
| `setAntennaRouting` auf die drei Buchsen (A1/A2/A3) | Opcode 0x15 bestätigt, Rahmenbauer liegt fertig in `SunSdrProtocol.h`. Umschalten ohne Senden ist am Gerät harmlos |
| `setTxDrive` (Leistungsstellung) | Opcode 0x17 bestätigt, Rahmenbauer liegt fertig. Ohne MOX entsteht keine HF |
| `setTxFrequency` | Frequenz-Opcode 0x08 ist am Gerät bewiesen (dreimal gegen ExpertSDR2) |

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
   Betreibers braucht.
5. **Der Rest.** Abtastrate, zweiter Empfänger, Antennenumschaltung,
   Mikrofon-Zubehör.

Die Reihenfolge ist nicht nach Aufwand sortiert, sondern danach, was das
Nächste erst möglich macht: ohne 1 ist 3 geraten, ohne 2 ist der Strom
falsch, und ohne richtigen Strom ist Senden eine Wette.
