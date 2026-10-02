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

## Was fehlt (vollständig, aus dem Code gezählt)

### A. Senden — gibt es nicht

`sendTxIq`, `setTxFrequency`, `setTxDrive`, `setAntennaRouting`,
`setPuresignalRun` sind leere Rümpfe. `setMox` hat seit dem 2026-09-02
einen echten Körper mit Bandplan-Prüfung, erreicht aber laut eigenem
Logtext „zero wire reachability": der Pacer tickt, es geht kein Byte
hinaus. Die vier Steuerrahmen-Bauer für MOX (0x06), Antenne (0x15),
Leistung (0x17) und PA (0x24) liegen fertig in `SunSdrProtocol.h` und
haben bis heute keine Aufrufstelle.

Der Plan dazu existiert („6-step TX-chain plan", Schritte 1–3 erledigt);
offen sind Diagnose-Oberfläche, Jitter-Prüfstand an der Bank und die
eigentliche Verdrahtung auf den Draht.

### B. Die Meldeseite — vier von dreizehn

`P2RadioConnection` meldet nach oben: `meterDataReceived`,
`paTelemetryUpdated`, `adcOverflow`, `micPttFromRadio`,
`psPairedIqDataReceived`, `widebandFrameReady`, `iqPacketLoss`,
`iqSequenceGap` sowie Spannung und Netzteil-Rohwert — dazu die vier, die
auch der QRP-Treiber hat (`connectFailed`, `errorOccurred`,
`frameReceived`, `iqDataReceived`).

**Die Wurzel ist eine einzige, und sie liegt tiefer als eine fehlende
Umrechnung:** der Treiber hat dem Gerät gar nicht zugehört.

1. `processControlDatagram()` stieg nach dem Handschlag sofort aus
   (`if (!m_awaitingBeacon || !m_profile) return; // drain only`). Jeder
   Steuerrahmen, den die QRP im Betrieb von sich aus schickt, fiel auf
   den Boden.
2. Im Kopf der Strompakete blieben der Opcode und die zwei Zustandsbytes
   `[8:9]` ungelesen, und TX-aktive Rahmen (0xFD) flogen ganz hinaus —
   dabei ist „das Gerät ist in den Sendezustand gegangen" genau eine der
   Meldungen, die fehlen.

Das ist auch der Grund, aus dem acht Opcodes seit dem 2026-08-26
unzugeordnet sind (0x03, 0x0c, 0x0d, 0x0f, 0x11, 0x13, 0x16, 0x1c): sie
kamen vielleicht die ganze Zeit herein, nur hat sie nie jemand angesehen.
Die Zuordnung galt bisher als Termin an der Bank — tcpdump mit sudo,
ExpertSDR2 daneben, `tools/sunsdr_opcode_watch.py`.

### C. Empfang und Betrieb

* `setActiveReceiverCount` leer — die QRP bleibt einkanalig, während
  Anvelina und ANAN mehrere DDCs bedienen.
* `setSampleRate` leer; der Kommentar dort nennt noch die alten,
  geratenen 312 500 Hz, gemessen sind 48 000 (2026-09-23). Welcher Opcode
  die Rate umstellt, ist offen.
* `setPreamp` leer (nur `setPreampModeIndex` ist gebaut),
  `setAntennaRouting` leer, `setWatchdogEnabled` leer.

### D. Mikrofon und Zubehör — alles leer

`setMicBoost`, `setLineIn`, `setMicTipRing`, `setMicBias`,
`setLineInGain`, `setMicXlr`, `setMicPTTDisabled`, `setUserDigOut`,
`setTrxRelay`.

### E. Die offene Protokollfrage, die alles andere färbt

Am 2026-09-23 gemessen: die QRP legt Longpath **achtmal denselben Block**
hin (bytegleich über alle 1200 Byte), ExpertSDR2 am selben Gerät in
derselben Stunde **zwei verschiedene**. Rahmenrate bei beiden 240/s. Der
einzige bekannte Unterschied: ExpertSDR2 schickt beim Verbinden rund zwei
Dutzend Steuerrahmen, Longpath einen. Und der Betreiber hört es — mit
Wiederholungsfilter klingt das Rauschen falsch, ohne richtig, also fehlt
uns an diesem Strom etwas Grundsätzliches.

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
