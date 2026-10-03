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

### 2. ~~Longpath schickt den Stromstart-Rahmen gar nicht~~ — FALSCH, am 2026-10-03 widerlegt

**Diese Behauptung war falsch, und zwar meine.** Sie stand hier einen
halben Tag und hat eine Versuchsreihe in die falsche Richtung gelenkt.

`stateSyncFrameForTest()` — der eine Rahmen, den Longpath nach der
Beacon-Antwort schickt — ist

```
03ff01000c0000000000010000007648ea9e 010000000c08040302020202
```

also **Opcode 0x01 mit genau der Tabelle `01000000 0c080403 02020202`**,
und damit **bitgleich** mit dem `0x01`-Rahmen aus dem
ExpertSDR2-Mitschnitt. Longpath schickt den Stromstart-Rahmen also
längst, und zwar byteidentisch. Der Kommentar an der Funktion sagt es
ausdrücklich: „Opcode 0x01 — SUNSDR_OP_STATE_SYNC in ArtemisSDR's
naming".

Verwechselt hatte ich ihn mit dem Frequenzrahmen `03ff0800…`, der an
anderer Stelle in derselben Datei steht. Eine Dateistelle gelesen, die
andere angenommen.

**Was daraus wirklich folgt:** Longpath schickt beim Verbinden vier
Rahmen — `0x01` (Stromstart), `0x04` (Vorverstärker), `0x07` (DDC-
Frequenz), `0x08` (VFO) — und bekommt für jeden eine Quittung. Das deckt
sich genau mit dem ersten Mithör-Bericht. Was gegenüber ExpertSDR2 fehlt,
sind also nicht der Startrahmen, sondern die Konfigurationsrahmen
`0x0c`, `0x10`, `0x16`, `0x18`, `0x1c` und die weiteren, die in den
dreizehn nicht enthalten sind.

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

---

# Nachtrag 2026-10-03: der erste echte Lauf mit Mithören

Zehn Minuten Betrieb am Gerät (ohne Antenne, Akku), Bericht beim Beenden:

```
Mithoeren -- Steuerrahmen 4 (davon unlesbar 0), Sorten: Steuerkanal 4, Strom 1
Steuerkanal op=0x01 len=0 | 1x | bei   0 ms
Steuerkanal op=0x04 len=0 | 1x | bei  13 ms
Steuerkanal op=0x07 len=0 | 1x | bei  98 ms
Steuerkanal op=0x08 len=0 | 1x | bei  99 ms
Strom       op=0xfe len=2 | 149691x | Aenderungen 0 | 0…623712 ms | Werte: 0100
```

## 1. Das Gerät quittiert jeden Steuerrahmen

Longpath schickt beim Verbinden genau vier Rahmen — Zustandsrahmen
(`0x08`), Vorverstärker (`0x04`), DDC-Frequenz (`0x07`), VFO (`0x08`) —
und bekommt **vier Quittungen mit demselben Opcode und leerer Nutzlast**
zurück, alle innerhalb von 99 ms. Diese Rückrichtung ist dem Treiber bis
zum 2026-10-02 vollständig entgangen.

Das ist nutzbar: **keine Quittung = Rahmen verworfen.** Damit lässt sich
prüfen, ob ein Werkbank-Rahmen angekommen ist, statt ins Blaue zu
schicken — genau das Problem, das die Prüfsummen-Sache aufgeworfen hat
(„ein Rahmen mit falschem Ende wird stillschweigend verworfen" — offenbar
nicht ganz stillschweigend).

## 2. Von sich aus meldet das Gerät nichts

Nach 99 ms kommt auf dem Steuerkanal **über zehn Minuten kein einziger
weiterer Rahmen**. Kein S-Meter, keine Spannung, keine Temperatur, keine
Übersteuerungsmeldung.

**Das verändert Schritt 3 des Paritätsplans.** Die Messwerte liegen nicht
bereit und müssen nicht nur „nach oben gegeben" werden — sie müssen
**abgefragt** werden, oder es gibt sie nicht. ArtemisSDRs Tabelle führt
dafür `INFO_QUERY` (DX `0x07`) und `STATE_REQ_A/B` (DX `0x0E`/`0x10`);
welche Nummer das bei der QRP ist, ist offen (siehe den
Opcode-Versatz-Befund im Paritätsplan, Abschnitt 3a) — und Opcodes raten
wird am Gerät nicht gemacht.

Auch die Zustandsbytes im Stromkopf tragen nichts: `0100` über 149 691
Pakete, **null Änderungen**.

## 3. Die Achtfachung tritt im heutigen Betrieb nicht auf

Gemessen: 149 691 Pakete in 623,7 s = **240,0 Pakete/s** bei 240
Blöcken/s, also **eine** Kopie je Folgenummer. Die Blockantwort wirkt, wie
am 2026-09-24 gemessen.

**Damit ist eine Formulierung weiter oben in diesem Blatt falsch:** der
Verbindungs-Dialog ist nicht „die Wurzel der Achtfachung" — die ist seit
dem 2026-09-24 erledigt. Was offen bleibt, ist etwas anderes und
Größeres:

| | Pakete/s | Nummern/s | je Nummer | Proben/s |
| --- | --- | --- | --- | --- |
| Longpath heute | 240 | 240 | 1 gleiches | 48 000 |
| ExpertSDR2, selbes Gerät | 480 | 240 | **2 verschiedene** | **96 000** |

ExpertSDR2 bekommt also die **doppelte Datenmenge**. Und dazu passt der
zweite Fund aus den dreizehn Rahmen: `0x07` kommt mit **sub 0 und sub 1**,
auf 14,224 MHz **und** 1,905 MHz — zwei ganz verschiedene Frequenzen.

**Hypothese, die beides zusammenbringt:** die zweite Paketsorte je
Folgenummer ist der **zweite Empfänger**. Dann wäre „ExpertSDR2 bekommt
zwei verschiedene Pakete" keine Eigenart des Stroms, sondern einfach ein
zweiter DDC — und die QRP könnte zwei Empfänger, was
`BoardCapabilities` bis heute als „not confirmed" führt.

Der Versuch dazu bleibt derselbe (PRE/EXTRA oben), das **Erfolgsmaß
ändert sich**: nicht „Kopien je Nummer sinkt von 8 auf 1" (sie ist schon
1), sondern **„Pakete/s verdoppelt sich auf 480 und die Kopien je Nummer
steigt auf 2"** — zwei Pakete je Nummer mit verschiedenem Inhalt. Beides
steht in der Folgenummern-Zeile, die seit dem 2026-10-03 alle 60 s im Log
mitläuft.

---

# Der Versuch, gefahren am 2026-10-03 (Messlauf, ohne Antenne)

Gefahren mit `tests/tst_sunsdr_messlauf.cpp` gegen die echte QRP
(192.168.16.200), Sandkasten-Einstellungen, kein MOX, kein Drive, keine
PA-Freigabe — also keine HF. Referenzmaß vorher: 240 Nummern/s, **1,20
Kopien je Nummer**, keine Verluste.

## Ergebnis 1: alle sechs Rahmen werden angenommen

`0x16`, `0x18`, `0x1c`, `0x10`, `0x0c`, `0x01` — **jeder wird quittiert**
(gleicher Opcode zurück, leere Nutzlast). Die Prüfsummen stimmen also, und
das Gerät verwirft nichts davon. Die Quittung ist damit ein belastbares
Mittel, um zu sehen, ob ein Rahmen angekommen ist.

## Ergebnis 2: die Datenrate ändert sich NICHT

Nach dem Versuch: 240 Nummern/s, 1,22–1,23 Kopien je Nummer, und die
Doppelpakete sind weiterhin **ganz bytegleich** (das eingebaute Messgerät:
„Wiederholungen: 55, davon GANZ bytegleich: 55, verschieden: 0").

**Damit ist die Hypothese von heute früh widerlegt:** diese sechs Rahmen
schalten keinen zweiten Datenstrom frei, und die zweite Paketsorte, die
ExpertSDR2 bekommt, ist kein zweiter Empfänger, den man mit ihnen
einschaltet. Was weiterhin fehlt, sind die übrigen rund vierzehn Rahmen
des Verbindungsablaufs — und die gibt es nur aus einem Mitschnitt.

## Ergebnis 3, der eigentliche Gewinn: `0x0c` ist eine ABFRAGE

Auf `0x0c` (18 Byte, keine Nutzlast) antwortet das Gerät mit **320 Byte**:

```
10748be4 | 0000000000002940 3333333333 3303c0 | … (39 Doubles)
```

Die vier Kopfbytes `10748be4`, dann **39 IEEE-754-Doubles**, und deren
Verteilung ist aussagekräftig:

| Wert | Anzahl |
| --- | --- |
| **12,5** | **12** |
| **−2,4** | **12** |
| 0,999997 | 2 |
| 0,0078125 (= 1/128) | 5 |
| 0,00273437 | 2 |
| 3,05176e−05 (= 1/32768) | 3 |
| 0,000170898 / 4,27e−06 / 8,96e−07 | je 1 |

**Zwölf Paare (12,5 / −2,4)** — und zwölf ist genau die Zahl der
Kurzwellenbänder. Dazu Skalierungsfaktoren als Zweierpotenz-Brüche. Das
ist eine **Kalibriertabelle, die das Gerät selbst herausgibt.**

Warum das wichtig ist: `buildDriveFrame()` ist gesperrt, weil „no QRP
bench power-calibration table exists yet" — die einzige bekannte Tabelle
ist DX-Hardware, 40 m, „very likely wrong for a QRP". Wenn die zwölf
Paare die bandweise Leistungskalibrierung sind, kommt diese Tabelle **vom
Gerät**, und die Sperre für Schritt 4 hat eine Lösung, die niemand
abschätzen muss.

**Das war ein Kandidat — und die Gegenprobe hat ihn geschwächt.** Gefahren
am selben Tag, ohne etwas zu senden: dieselbe Abfrage auf **80 m und auf
10 m**, dazu zweimal im Abstand von 2,7 s.

> `Steuerkanal op=0x0c len=320 | 2x | Aenderungen 0`

Die Antwort ist **bitgleich** — weder bandabhängig noch zeitlich
veränderlich. Daraus folgt zweierlei:

* `0x0c` liefert **statische** Daten: Konfiguration oder Werkskalibrierung,
  keine Momentanwerte. **Als Quelle für S-Meter, Spannung oder Temperatur
  fällt es damit aus** — die Messwerte für Schritt 3 sind weiter nicht
  gefunden.
* Die zwölf Paare sind nicht *nachweislich* eine Bandtabelle. Zwölf
  gleiche Paare können eine werkseitig überall gleich gefüllte Tabelle
  sein — oder zwölf etwas anderes. Für die Leistungskalibrierung, die
  `buildDriveFrame()` fehlt, taugt das erst, wenn die Bedeutung bekannt
  ist; raten verbietet sich dort besonders, weil am Ende HF entsteht.

Was bleibt und zählt: **`0x0c` ist eine Abfrage, auf die das Gerät
strukturiert antwortet** — der erste bekannte Weg, überhaupt etwas aus der
QRP herauszufragen. Und die Quittungen zeigen, dass Rahmen ankommen.


## Ergebnis 4: das I/Q ist echt, sobald die Frequenz steht

Im selben Messlauf: **Q ungleich null bei 22,3 %** (40 m) und 19,3 %
(20 m). Der Einkanal-Zustand von 2026-09-25 (Q = 0, Seitenbänder
übereinander) tritt also nur auf, solange **kein** Frequenzrahmen `0x07`
hinausgegangen ist — mit ihm läuft echtes I/Q. Das ist die Bestätigung
der Eingrenzung vom 2026-09-25 („ein einziger 0x07-Rahmen für
Unterempfänger 0 schaltet echtes I/Q ein, 0 % → 21 %"), diesmal über
zwei Bänder und mit dem Messlauf reproduzierbar.
