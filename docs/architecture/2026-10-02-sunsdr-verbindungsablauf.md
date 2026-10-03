# Was ExpertSDR2 beim Verbinden sagt — ausgewertet aus dem, was wir schon haben

**Stand 2026-10-02 nachts.** Dieses Blatt wertet dreizehn echte
ExpertSDR2-Rahmen aus, die seit dem 2026-09-25 im Projekt liegen — als
Prüfdaten für die CRC in `tests/tst_sunsdr_protocol.cpp`
(`controlFrameCrcReproducesThirteenCapturedFrames`). Sie wurden dort nur
als Bytes verglichen; **was in ihnen steht, hat nie jemand ausgewertet.**

Damit lässt sich der zweite Schritt des Paritätsplans
(`2026-10-02-sunsdr-paritaet.md`) ohne neuen Mitschnitt beginnen.

## Die dreizehn Rahmen, gedeutet

| Opcode | sub | Nutzlast als Zahlen | Deutung |
| --- | --- | --- | --- |
| `0x04` | 0 | 0 / 1 / 2 / 3 | Vorverstärker, vier Stufen — **gebaut** |
| `0x07` | 0 | 142 240 100 → **14,224010 MHz** | DDC-Frequenz Unterempfänger 0 — **gebaut** |
| `0x07` | **1** | 19 050 000 → **1,905000 MHz** | **DDC-Frequenz Unterempfänger 1** |
| `0x08` | 0 | 0 | VFO-Frequenz, mit **0 Hz** gesendet |
| `0x10` | 0 | 0 | unbekannt, ein Schalter auf „aus"? |
| `0x16` | 0 | 1, 1, 0, 0, 200, 30, 700, 7, 60 | Konfigurationsblock |
| `0x18` | 0 | 0, **307 200 000**, 0 | 307,2 MHz — der Haupttakt des Geräts |
| `0x1c` | 0 | vier Worte, byteweise `13 37 0c 04 / 14 49 04 01 / 14 ae 47 01 / 3b 4f 52 00` | sieht nach Kalibrierwerten aus |
| `0x0c` | 0 | *(leer)* | reiner Auslöser ohne Parameter |
| `0x01` | 0 | 1, dann `0c 08 04 03 / 02 02 02 02` | **Strom starten** (Tabelle, nicht ein Wert) |

## Zwei Funde, die den Plan verändern

### 1. Die QRP wird mit ZWEI Unterempfängern angesprochen

`0x07` kommt zweimal, mit **sub 0 und sub 1**, auf zwei verschiedenen
Frequenzen (14,224 MHz und 1,905 MHz). In `BoardCapabilities` steht
dagegen:

> Single receiver assumed — **NOT confirmed**. The boot macro sets
> RX2_ENABLE=0 … whether the QRP hardware supports a second receiver at
> all is open.

Der Mitschnitt zeigt, dass ExpertSDR2 den zweiten Platz **adressiert**.
Das beweist noch nicht, dass die Hardware zwei Empfänger wirklich
liefert — aber die Frage ist damit von „völlig offen" auf „einen Versuch
wert" gerückt, und der Versuch ist billig: `0x07` mit sub 1 schicken und
sehen, ob ein zweiter Strom kommt.

### 2. Longpath schickt den Stromstart-Rahmen gar nicht

Longpath schickt beim Verbinden **einen** Rahmen, und das ist ein
`0x08`-Frequenzrahmen (im Code „state-sync" genannt). Im Mitschnitt ist
`0x08` aber der Rahmen mit **0 Hz** — und der, der nach Form und Inhalt
„Strom starten" heißt, ist `0x01` mit einer kleinen Tabelle als Nutzlast
(`01000000 0c080403 02020202`).

Das ist der erste belastbare Kandidat für die Erklärung der
**Achtfachung**: wenn das Gerät nie den richtigen Startbefehl bekommt,
läuft es in einem anderen Betriebszustand — und genau das ist der
beobachtete Unterschied (ExpertSDR2 bekommt zwei verschiedene Pakete je
Folgenummer, Longpath achtmal dasselbe).

## Der Versuch, fertig zum Abschicken

`sendBenchFrames()` liest zwei Umgebungsvariablen, PRE geht **vor**
Longpaths eigenem Rahmen hinaus, EXTRA **danach**. Damit lässt sich der
Ablauf ohne Neubau ausprobieren. Reihenfolge hier: erst Konfiguration und
Takt, dann Kalibrierung, dann Auslöser, dann Frequenzen — und der
Stromstart `0x01` zuletzt, weil ExpertSDR2 seinen Startrahmen auch als
letztes schickt.

```bash
export LONGPATH_SUNSDR_PRE=03ff160024000000000001000000e080ab4c01000000010000000000000000000000c80000001e000000bc020000070000003c000000,03ff18000c000000000001000000302af6b30000000000804f1200000000,03ff1c00100000000000010000000b63797c13370c041449040114ae47013b4f5200,03ff100004000000000001000000a444f1b700000000,03ff0c000000000000000100000037f7affe
export LONGPATH_SUNSDR_EXTRA=03ff01000c0000000000010000007648ea9e010000000c08040302020202
open /Applications/Longpath.app
```

**Was dabei zu beachten ist:**

* Die beiden `0x07`-Rahmen sind hier **nicht** dabei: sie tragen
  ExpertSDR2s eigene Frequenzen (14,224 MHz / 1,905 MHz) und würden das
  Gerät verstimmen. Longpath setzt seine Frequenz ohnehin selbst.
* Die Reihenfolge ist **begründet geraten**, nicht mitgeschnitten. Das
  Prüfdaten-Array ist nach Opcode gruppiert, nicht nach Zeit. Ein echter
  Mitschnitt (`tools/sunsdr_handshake_diff.py`) würde die Reihenfolge
  klären und auch die Rahmen zeigen, die in diesen dreizehn fehlen —
  gemessen wurden „rund zwei Dutzend".
* Woran man den Erfolg erkennt, ohne hinzuhören: im Log steht jetzt
  `Kopien je Nummer`. **8,0 heißt unverändert, 1,0 oder 2,0 heißt, der
  Versuch hat gewirkt.** Das ist der erste Fall, in dem diese Zahl eine
  Entscheidung trägt.
* Und das Mithören zeigt, **was das Gerät auf diese Rahmen antwortet** —
  bisher war diese Richtung komplett blind.

## Was offen bleibt

* Die Bedeutung von `0x10`, `0x16`, `0x1c` ist nur erahnt. Die Werte in
  `0x16` (200, 30, 700, 7, 60) sehen nach Zeiten und Frequenzen aus;
  700 könnte ein CW-Ton sein, mehr ist das nicht.
* Ob `0x18` (Haupttakt) auch die **Strom-Abtastrate** umstellt, ist die
  Frage hinter dem offenen Punkt „Abtastrate über 48 kHz". Der dritte
  Wert in seiner Nutzlast ist 0 — ein Platz, in dem eine Rate stehen
  könnte.
