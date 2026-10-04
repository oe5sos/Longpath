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

---

# Messreihe 2026-10-03: die vier Konfigurationsrahmen einzeln

Jeder Rahmen einzeln geschickt, nachdem die Verbindung stand und die
Frequenz gesetzt war, je sieben Sekunden gemessen. Alles bekannte Rahmen
aus dem Mitschnitt — keine erfundenen Werte, keine HF.

| Lauf | Quittungen | Q ungleich null | Blöcke/s |
| --- | --- | --- | --- |
| Referenz (kein Zusatzrahmen) | 3 | 24,0 % | 292,2 |
| `0x10` | 4 | 21,9 % | 288,6 |
| `0x16` | 4 | 23,1 % | 286,3 |
| `0x18` (Haupttakt) | 4 | 21,2 % | 286,5 |
| `0x1c` | 3 (+1 unbeantwortet) | 21,9 % | 289,2 |

**Ergebnis: alle vier werden angenommen und quittiert, keiner ändert
etwas Messbares.** Weder Abtastrate noch Paketrate noch der Q-Anteil
bewegen sich (die Schwankung von 21–24 % ist Rauschen am offenen
Eingang). Besonders `0x18` ist damit **nicht** der Weg zur Abtastrate über
48 kHz, obwohl seine Nutzlast den Haupttakt 307,2 MHz trägt.

Was ExpertSDR2s doppelte Datenrate verursacht, steckt also in den
Rahmen, die in diesen dreizehn **nicht** enthalten sind. Ohne Mitschnitt
kommt man hier nicht weiter — und Opcodes zu raten ist am Funkgerät
verboten.

## Der Nebenfund, der die Buchführung rechtfertigt

Im `0x1c`-Lauf: **„3 Quittungen gesehen, 1 Rahmen unbeantwortet"**, und im
Inventar fehlt `0x08`. Der **VFO-Frequenzrahmen wurde nicht quittiert** —
er ist unterwegs verloren gegangen, Steuerkanal hin oder zurück.

Das ist kein Einzelfall-Kuriosum, sondern ein echter Mangel: **wenn ein
Frequenzrahmen verloren geht, steht das Gerät auf einer anderen Frequenz
als Longpath anzeigt**, und bis heute hätte das niemand gemerkt. UDP
garantiert nichts, und dieser Treiber hat nie hingesehen.

Daraus folgt ein nächster Schritt, der aber **eine Entscheidung des
Betreibers braucht**, weil er Rahmen an das Gerät schickt: einen
unquittierten Rahmen **einmal** nachschicken. Ein Treiber, der von selbst
wiederholt, wirkt auf das Funkgerät — das gehört nicht ohne Freigabe
eingebaut. Für eine Frequenz ist das Nachschicken offensichtlich richtig;
für andere Rahmen muss man es je Opcode entscheiden.

---

# Der zweite Unterempfänger: angenommen, aber nicht eingeschaltet (2026-10-03)

Geschickt wurde der `0x07`-Rahmen mit **sub = 1** aus dem Mitschnitt
(ExpertSDR2s Wert, 1,905 MHz) — ein echter Rahmen, reiner Empfang, keine
HF. Dreimal wiederholt, dazu dreimal dieselbe Zahl Läufe ohne ihn:

| | Quittungen | unbeantwortet | Blöcke/s | Stromsorten |
| --- | --- | --- | --- | --- |
| mit `0x07 sub=1` | **4, 4, 4** | 0, 0, 0 | 288,5 | 1 |
| ohne | 3, 3, 3 | 0, 0, 0 | 288–292 | 1 |

**Das Gerät akzeptiert und quittiert den zweiten Unterempfänger.** Die
QRP kennt diesen Platz also — `BoardCapabilities` führt
`maxReceivers = 1` bis heute mit dem Vermerk „NOT confirmed … whether the
QRP hardware supports a second receiver at all is open", und das ist
damit einen Schritt weiter.

**Aber es kommt kein zweiter Strom:** Paketrate unverändert, weiterhin
genau eine Stromsorte (`0xfe`, Zustandsbytes `0100`). Der Platz wird
angenommen und bleibt stumm — er ist nicht **eingeschaltet**.

Und damit passt die Rechnung von heute früh wieder zusammen, nur genauer:

* ExpertSDR2 bekommt 480 Pakete/s mit **zwei verschiedenen** Paketen je
  Folgenummer — das wären zwei Empfänger zu je 240 Blöcken.
* ExpertSDR2 setzt `0x07` für **beide** Plätze (sub 0 und sub 1).
* Longpath setzt nur Platz 0, und bekommt genau einen Strom.

Was fehlt, ist der **Einschalter**. ArtemisSDRs Tabelle führt ihn für die
DX als `SUNSDR_OP_RX2_ENABLE 0x1B`, und die Boot-Folge dort ruft
`sunsdr_send_u32_cmd(SUNSDR_OP_RX2_ENABLE, rx2_enabled)`. Welche Nummer
das bei der QRP ist, **wissen wir nicht** — bei drei gemessenen Befehlen
liegt die QRP um eins unter der DX, bei `0x01` nicht. Deshalb wird sie
hier nicht geraten: ein falsch getroffener Opcode kann am Funkgerät etwas
auslösen, das niemand bestellt hat, und zwei Nummern weiter liegen Drive
(`0x17`) und PA-Freigabe (`0x24`).

**Der nächste Schritt ist damit klar umrissen und braucht zwei Minuten
Mitschnitt:** ExpertSDR2 verbinden lassen und dabei RX2 ein- und
ausschalten. Der Rahmen, der sich dabei ändert, ist der Einschalter — und
dann ist der zweite Empfänger kein offener Punkt mehr, sondern eine
Einstellung.

---

# Zweite Messreihe 2026-10-03, nach dem Wiedereinschalten

Der Akku war leer, das Gerät war aus — und damit ergab sich ein Fall, der
sonst schwer herzustellen ist: ein **frisch eingeschaltetes** Gerät.

## Der Einkanal-Zustand ist ein Einschaltzustand, A/B gemessen

Zwei Läufe direkt hintereinander, am selben Gerät, Minuten auseinander:

| Lauf | Frequenzrahmen `0x07`? | **Q ungleich null** | Quittungen |
| --- | --- | --- | --- |
| A | **nein** | **0,0 %** | 1 (nur Zustandsrahmen) |
| B | ja (40 m) | **21,9 %** | 3 |

Damit ist die Eingrenzung vom 2026-09-25 („ein einziger 0x07-Rahmen für
Unterempfänger 0 schaltet echtes I/Q ein, 0 % → 21 %") als **A/B-Messung
am frisch eingeschalteten Gerät** bestätigt. Vorher war das Gerät jeweils
schon länger an, und der Zustand ließ sich nur aus der Erinnerung
beschreiben.

Praktisch heißt das: Longpath setzt beim Verbinden ohnehin eine Frequenz,
also tritt der Zustand im Betrieb nicht auf. Wer aber **ohne**
Frequenzrahmen messen will — etwa an einem Prüfstand —, misst ein Gerät,
dessen Seitenbänder übereinanderliegen, und darf das nicht für den
Normalfall nehmen.

## Die Verlustrate von Steuerrahmen: null von 1212

Gemessen mit 150 Frequenzwechseln je Lauf (jeder Wechsel = zwei Rahmen,
`0x07` und `0x08`), viermal:

```
Lauf 1: 303 Quittungen gesehen, 0 unbeantwortet
Lauf 2: 303 Quittungen gesehen, 0 unbeantwortet
Lauf 3: 303 Quittungen gesehen, 0 unbeantwortet
Lauf 4: 303 Quittungen gesehen, 0 unbeantwortet
```

**Das korrigiert meine eigene Begründung von vorhin.** Oben steht, ein
Frequenzrahmen sei verloren gegangen, und das stimmt — aber „etwa einer
von fünfzehn Läufen" war aus einem Einzelfall geschlossen. Über 1212
Rahmen gemessen ist die Rate **null**. Der Steuerkanal am Kabel ist
zuverlässig.

Für das Nachschicken unquittierter Rahmen heißt das: es bleibt eine
sinnvolle Versicherung — eine verlorene Frequenz **bleibt** sonst
unbemerkt, und genau einmal ist es passiert —, aber es ist **nicht
dringend**. Die Ursache jenes einen Verlusts ist offen; er trat in einem
Lauf auf, in dem zusätzlich ein Konfigurationsrahmen hinausging.

---

# Zwei Korrekturen aus dem echten Betrieb des Betreibers (2026-10-03, 17:50)

Der Betreiber hat Longpath neu gestartet und die QRP verbunden — seine
Instanz hält damit Gerät und Ports, und sie sammelt selbst mit (das
Mithören ist seit 06:59 installiert). Ihr Log widerlegt zwei Dinge aus
meinen eigenen Messläufen.

## 1,00 Kopien je Nummer — mein Messlauf hat gemessen, was er verursacht hat

| | Kopien je Nummer |
| --- | --- |
| mein Messlauf | **1,20** |
| echter Betrieb | **1,00** |

Der Unterschied ist der Prüfstand: er ließ die Ereignisschleife in
500-ms-Blöcken laufen (`QTest::qWait(500)`), dadurch gingen die
Blockantworten verspätet hinaus, und das Gerät **wiederholte**. Die
„rund 50 bytegleichen Wiederholungen je Sekunde" sind also keine
Eigenschaft des Geräts, sondern eine Folge meiner Messung.

Behoben: der Messlauf tickt jetzt in 20 ms. Und als Merksatz: ein
Messgerät, das den Takt der Antworten verschiebt, misst sich selbst.

**Im echten Betrieb ist der Strom sauber** — 240 Nummern je Sekunde, eine
Kopie je Nummer, keine Spätlinge.

## „Das Gerät meldet von sich aus nichts" — präziser gefasst

Im Log des Betreibers steht 88 Sekunden nach dem Verbinden:

```
neue Rahmensorte auf Steuerkanal -- op=0x1 sub=0 len=6 nutzlast=51c300004928
```

Diese sechs Byte sind das Ende der **Beacon-Antwort**
(`03ff011a7c…51c300004928`). Das Gerät hat also auf eine Geräte-Rundfrage
**geantwortet** — es hat nichts von sich aus gemeldet. Der Befund bleibt
richtig, muss aber genauer heißen:

> **Die QRP antwortet, aber sie meldet nicht.** Quittungen auf
> Steuerrahmen, Antworten auf Abfragen (`0x0c`), Antworten auf
> Rundfragen — alles reaktiv. Kein unaufgeforderter Messwert.

Nützlich ist das trotzdem: solche Antworten im Inventar zeigen, dass das
Gerät erreichbar ist und wer es sucht.

---

# 2026-10-03 abends: die Mitschnitte lagen seit dem 23. September da

In `~/Longpath/werkzeug/mitschnitte/` liegen zwei ExpertSDR2-Mitschnitte
vom 2026-09-23, beide 17:04/17:31 — `expert-steuerung.pcap` (9 kB) und
`expert-rate.pcap` (12 MB). Aus ihnen sind damals die dreizehn Rahmen für
die CRC-Prüfung gezogen worden; **auf den Verbindungsablauf hat sie
niemand ausgewertet.** Das ist jetzt nachgeholt, und es beantwortet die
zwei größten offenen Fragen.

## Die QRP kann 96 kHz — gemessen

| Mitschnitt | Strompakete | Blöcke/s | Abtastrate |
| --- | --- | --- | --- |
| `expert-rate.pcap` | 28 561 in 59,6 s | **479** | **96 kHz** |

`BoardCapabilities` führt `maxSampleRate = 48000` und
`sampleRates = {48000, 0, …}`. Das ist **widerlegt**: an derselben QRP
lief ExpertSDR2 mit 96 kHz.

## Und damit ist die „doppelte Datenrate" erklärt — es war nie der zweite Empfänger

Am 2026-09-23 war gemessen: ExpertSDR2 bekommt 480 Pakete/s mit **zwei
verschiedenen** Paketen je Folgenummer, Longpath 240 mit einem. Daraus
war die Vermutung „das ist der zweite Empfänger" geworden — und sie
stimmt nicht. Die Struktur des 96-kHz-Stroms:

```
Folgenummern: 0, 0, 0, 1, 2, 2, 3, 3, 4, 4, 5, 5 …
4000 Pakete  ->  2000 verschiedene Nummern
Differenzen:  0 (2000x), 1 (1999x)
Zustandsbytes [8:9]: konstant 0100
```

**Bei 96 kHz schickt die QRP zwei Pakete je Folgenummer**, mit je 200
Probenpaaren — zusammen 400 je Nummer, bei 240 Nummern/s also 96 000
Proben je Sekunde. Die beiden Pakete tragen verschiedene Proben und sind
**nicht** unterscheidbar markiert: die Zustandsbytes sind in beiden
`0100`. Nur die Reihenfolge trennt sie.

### Die Konsequenz für Longpath, und sie ist unangenehm

Der Folgenummern-Zähler von heute behandelt ein zweites Paket mit
derselben Nummer als **Wiederholung** — richtig bei 48 kHz (dort sind die
Kopien bytegleich, am Gerät belegt), **falsch bei 96 kHz**: dort wäre es
die zweite Hälfte der Proben. **Longpath würde bei 96 kHz die Hälfte der
Daten wegwerfen**, und zwar lautlos.

Wer also die Rate umstellt, muss zugleich `processStreamDatagram`
umbauen: bei 96 kHz zählt nicht die Folgenummer, sondern die
Reihenfolge — erstes Paket einer Nummer = Proben 0…199, zweites =
200…399.

## Der vollständige Verbindungsablauf: 33 Rahmen, alle quittiert

Der Mitschnitt zeigt ExpertSDR2s Ablauf lückenlos (Auszug, Reihenfolge
original):

```
0x06 MOX=0   0x02        0x16 Konfig
0x00 Suche   <- 0x01 Beacon
0x05 (1200 B)  0x05 (1200 B)  0x12 (1024 B) -> <- 0x12 (20 B)
0x04 Preamp  0x03  0x17 Drive=0  0x11 fe  0x0f 6b  0x1a  0x04  0x15 Antenne
0x0c Abfrage -> <- 320 B      0x0d Abfrage -> <- 320 B
0x1c Kalibrierung   0x10   0x01 Stromstart
0x18 Haupttakt   0x07 sub0   0x07 sub1   0x13   0x04   0x16   0x08   0x06
0x18 x2   0x16   0x10 x2
```

**Siebzehn Opcodes, die Longpath nie schickt:** `0x02`, `0x03`, `0x05`,
`0x06`, `0x0c`, `0x0d`, `0x0f`, `0x10`, `0x11`, `0x12`, `0x13`, `0x15`,
`0x16`, `0x17`, `0x18`, `0x1a`, `0x1c`. Das Gerät **quittiert jeden
einzelnen davon** — die Nummern sind damit alle als „dem Gerät bekannt"
bestätigt.

**Zwei Abfragen, nicht eine:** `0x0c` und `0x0d`, beide mit 320 Byte
Antwort. `0x0d` beginnt mit `ad042467` und dann Nullen — eine andere
Struktur als `0x0c`.

**`0x05` und `0x12` tragen Speicherabbilder:** die Nutzlasten enthalten
wiederkehrende `00c1b77f`-Muster, also Zeiger eines 64-Bit-Prozesses, und
ihre Länge schwankt zwischen Mitschnitten (1200/1024 gegen 340/340). Das
sind keine Protokollfelder, die man nachbauen sollte — eher ungesäuberte
Puffer von ExpertSDR2.

---

# 2026-10-03, 19:32: die Abtastrate steht im Rahmen 0x01 — den Longpath längst schickt

Mitschnitt `rate-umschalten.pcap` (118 550 Pakete), ExpertSDR2 verbunden,
zweimal die Rate umgeschaltet. Die Paketrate über die Zeit:

| Zeit | Pakete/s |
| --- | --- |
| Phase A | 720 |
| Phase B | 1200 |
| Phase C | 720 |

Und im Steuerkanal ändert sich an genau diesen zwei Stellen **ein**
Rahmen: `0x01`, der Stromstart.

| | Nutzlast von `0x01` |
| --- | --- |
| Longpath heute | `01000000` `0c080403` `02020202` |
| ExpertSDR2, Phase A/C | `02000000` `0c080403` `02020202` |
| ExpertSDR2, Phase B | `02010000` `0a060403` `02020201` |

## Es sind zwei Ströme, unterschieden durch `byte9` im Stromkopf

Nach Kanal aufgeschlüsselt — und damit löst sich alles auf:

| Phase | `byte9 = 0` | `byte9 = 1` |
| --- | --- | --- |
| A / C | 240 Pakete/s, 1 je Nummer → **48 kHz** | 480 Pakete/s, 2 je Nummer → **96 kHz** |
| B | 480 Pakete/s, 1 je Nummer → **96 kHz** | 720 Pakete/s, 1,5 je Nummer → **144 kHz** |

ExpertSDR2 lässt sich also **zwei Ströme gleichzeitig** schicken, mit
**verschiedenen** Raten. Longpath bekommt einen, mit 48 kHz — und der
Unterschied steht im ersten Byte des `0x01`-Rahmens: **`01` gegen `02`**.
Dasselbe Byte erscheint im Stromkopf wieder als `byte8` (Longpath sieht
`0100`, ExpertSDR2 `0200`/`0201`).

Damit ist auch die Beobachtung vom 2026-09-23 endgültig erklärt
(„ExpertSDR2 bekommt zwei verschiedene Pakete je Nummer"): das waren die
zwei Ströme, und beim 96-kHz-Strom zusätzlich zwei Pakete je Nummer.

## Was daraus folgt — und es ist viel

1. **Die QRP kann 48, 96 und 144 kHz**, gemessen. `BoardCapabilities`
   führt `maxSampleRate = 48000`; das ist widerlegt.
2. **Die QRP kann zwei Ströme gleichzeitig.** Ob das zwei Empfänger sind
   oder Empfänger plus Panadapter, ist offen — aber es sind zwei
   unabhängig geratete Ströme, und `0x07` adressiert passend dazu zwei
   Unterempfänger (sub 0 und sub 1).
3. **Longpath braucht dafür keinen neuen Opcode.** Es schickt `0x01`
   bereits; nur die Nutzlast müsste von `01000000 0c080403 02020202` auf
   eine der gemessenen umgestellt werden. Die beiden bekannten Werte
   stehen oben, mit gültiger Prüfsumme im Mitschnitt.
4. **Vorher muss `processStreamDatagram` umgebaut werden**, sonst wird es
   schlimmer statt besser:
   * Der zweite Strom (`byte9 = 1`) wird heute **als derselbe behandelt**
     — die Proben beider Kanäle landen in einem Topf.
   * Bei 2 Paketen je Nummer gilt das zweite heute als **Wiederholung**
     und wird verworfen — es ist aber die zweite Hälfte der Proben.
   Beides zusammen heißt: einfach den Rahmen umstellen würde den Empfang
   **kaputtmachen**, nicht verbessern.
5. Die Bedeutung der Bytes `0c 08 04 03` gegen `0a 06 04 03` ist noch
   nicht entschlüsselt. Für den ersten Schritt braucht man sie nicht —
   die zwei gemessenen Nutzlasten reichen, um 48+96 bzw. 96+144 kHz zu
   bekommen.

---

# Die Messwert-Frage ist entschieden: die QRP gibt keine heraus

Alle Antworten, die das Gerät auf Abfragen schickt, aus **allen** drei
Mitschnitten zusammengetragen — die vom 2026-09-23 und die vom
2026-10-03, also über zehn Tage hinweg:

| Abfrage | Antworten | davon verschieden | Länge |
| --- | --- | --- | --- |
| `0x0c` | 5 | **1** | 320 Byte |
| `0x0d` | 5 | **1** | 320 Byte |
| `0x12` | 5 | **1** | 20 Byte |

**Bitgleich, über zehn Tage, über Neustarts und Bandwechsel hinweg.** Das
sind Werksdaten — Kalibrierung, Typ, Version —, keine Momentanwerte.
Dazu passt die Beobachtung vom Vormittag: unaufgefordert schickt das
Gerät gar nichts, und die Zustandsbytes im Stromkopf bleiben konstant.

Für die Paritätsliste heißt das abschließend:

> **Im Empfang gibt es bei der QRP keine Gerätemesswerte.** Nicht über
> `0x0c`, nicht über `0x0d`, nicht über `0x12`, nicht unaufgefordert und
> nicht im Stromkopf.

Das ist kein Mangel von Longpath, sondern eine Eigenschaft des Geräts.
Was P1 und P2 dort melden, ist ohnehin senderseitig
(`meterDataReceived` trägt Vorwärts- und Rückwärtsleistung,
`paTelemetryUpdated` PA-Temperatur und -Strom) — ob die QRP das beim
**Senden** herausgibt, ist eine eigene Frage und gehört zum Dummy-Load.

Das S-Meter rechnet Longpath selbst aus dem I/Q; Übersteuerung und
Mikrofon-PTT sind am 2026-10-03 aus dem Signal bzw. dem Stromkopf
gebaut. Damit ist die Meldeseite im Empfang vollständig, ohne dass das
Gerät einen einzigen Messwert liefert.

## Was in den Antworten steht, soweit lesbar

* `0x0c`: vier Kopfbytes, dann 39 IEEE-754-Doubles — darunter zwölf Paare
  (12,5 / −2,4) und Skalierungsfaktoren als Zweierpotenz-Brüche.
* `0x0d`: beginnt `ad042467`, danach überwiegend Nullen.
* `0x12`: `ee000300 07000000 41190000 41c27c00 01000100`. Die `4119`
  taucht auch in der Beacon-Antwort auf (dort neben der IP-Adresse des
  Geräts) — also eher Typ- oder Versionskennung als Messwert.

---

# Vorbehalt: Logzeilen konnten verloren gehen (Hinweis aus PR #171)

Eine Nebensitzung hat am 2026-10-03 gemessen, dass sich Logzeilen
gegenseitig zerschreiben, wenn mehrere Fäden gleichzeitig schreiben —
**zwischen 7 % und 55 % der Zeilen gingen ganz verloren** (behoben in
PR #171, dort noch offen). Das betrifft alles, was auf diesem Blatt aus
**Longpaths Logdatei** gelesen wurde, und gehört dazugesagt.

**Welche Befunde hängen an Logzeilen, und halten sie trotzdem?**

| Befund | Quelle | hält? |
| --- | --- | --- |
| Das Gerät quittiert jeden Steuerrahmen | Betriebslog **+** Zähler im Treiber **+** tcpdump | **ja**, dreifach |
| „1,00 Kopien je Nummer im echten Betrieb" | Betriebslog | ja — eine Zeile ist entweder da oder fehlt, ihre Zahlen werden nicht verfälscht |
| **„Das Gerät meldet von sich aus nichts"** | zuerst nur Betriebslog | **anfällig** — fehlende Zeilen hätten wie fehlende Meldungen ausgesehen |
| Raten, Kanäle, Paketzahlen | Zähler im Treiber, über `qInfo` im Prüfstand | ja, nicht über die Logdatei |
| Verbindungsablauf, Stopp-Rahmen, Abfrage-Antworten | **tcpdump** | ja, am Draht gemessen |

Der eine anfällige Schluss ist **unabhängig bestätigt**: der Mitschnitt
zeigt jedes Paket am Draht, und dort schickt das Gerät zwischen den
Quittungen tatsächlich nichts. Hätte das Log Zeilen verloren, wäre die
Lücke im tcpdump nicht zu sehen gewesen — sie ist es nicht.

**Regel daraus für künftige Messläufe:** ein Negativ-Schluss („es kommt
nichts") darf nicht allein auf einer Logdatei stehen. Zähler im Code oder
ein Mitschnitt am Draht — beides zählt, was wirklich ankam, und beides
war heute vorhanden.

---

# Gegenprobe: fangen die Prüfungen die Fehler, die sie fangen sollen?

Eine Nebensitzung gab am 2026-10-03 den Hinweis, der hier gefehlt hat:
*ein Prüfstand kann widersprechen — wenn man ihn zuerst gegen die ALTE
Fassung laufen lässt.* „Grün mit der Behebung sagt nichts, rot ohne sie
sagt alles."

Genau das war an diesem Tag nicht gemacht worden: alle Prüfungen zu den
drei Fehlern, die das Gerät aufgedeckt hat, wurden **nach** der Behebung
geschrieben. Nachgeholt, indem der Code jeweils zurückgebaut und derselbe
Prüfstand erneut gefahren wurde:

| Prüfung | gegen die alte Fassung |
| --- | --- |
| `zweiStroemeLandenAufVerschiedenenKanaelen` (Verwerfen vor der Zählung) | **rot** — `seqLost = 1` statt 0 |
| `wiederholungMitAbstandIstKeinSpaetling` (Zähler ohne Ring) | **rot** |
| `stromneustartWirdErkanntUndNichtZumDauerzustand` (kein Neuanfang) | **rot** |
| `einzelnerSpaetlingIstKeinNeuanfang` | **grün in beiden** |

Die ersten drei fangen also wirklich, was sie sollen. Die vierte ist kein
Fänger, sondern ein **Wächter**: sie soll in beiden Fassungen grün sein
und schlägt erst an, wenn die Neuanfang-Erkennung zu früh greift und ein
einzelnes verirrtes Paket den Zähler umdreht. Beides ist nützlich, aber
es ist nicht dasselbe — und wer das nicht prüft, hält Wächter für Fänger.

**Der Satz, der dabei zusammengekommen ist** (eine Hälfte von hier, eine
aus der Nebensitzung):

> Ein Prüfstand bestätigt, was man ihm vorgibt. Nur das Gerät
> widerspricht — oder die alte Fassung, gegen die man ihn laufen lässt.

Und dazu, aus demselben Abend, die Regel über Abwesenheit:

> Wo Daten ausbleiben könnten, darf man aus ihrem Fehlen nichts
> schließen; und wo sie ausbleiben, muss die Anzeige das sagen, statt den
> letzten Wert festzuhalten.

---

# Nachtrag: die Rate im laufenden Betrieb umstellen (gemessen 2026-10-03)

96 kHz war bis hierher nur über den Umgebungsschalter
`LONGPATH_SUNSDR_STROMMODUS=je96` gemessen — also **nicht** über den Weg,
den die Oberfläche nimmt. Den geht `RadioModel::setSampleRateLive`, und
der ruft am Treiber `setSampleRate()`, während die Verbindung schon
steht. Nachgemessen am echten Gerät, zwei Läufe über je 12 s:

| | 48 kHz (Grundwert) | nach `setSampleRate(96000)` im Betrieb |
| --- | --- | --- |
| Folgenummern | 240/s, 1,06 Kopien je Nummer | 400/s, 0 Spätlinge |
| angenommen / verloren | 1200 / 0 (0,00 %) | 4797 / 0 (0,00 %) |
| Stromkopf, Nutzlast | nur `0100` | `0100` → `0200`, `0201` |
| Kanal 0 | 286/s, 0 verworfen | 724/s, 0 verworfen |
| Kanal 1 | — | 1014/s, **alle** verworfen |

Die letzte Zeile des Stromkopfs ist der eigentliche Beleg: das Gerät
hatte einen Strom (`byte8 = 01`) und hat nach dem Rahmen **zwei**
(`byte8 = 02`, `byte9` wechselt 00/01). Es gibt also keinen Neustart der
Verbindung und keinen Umgebungsschalter dafür — ein Rahmen genügt, und
Longpath verliert dabei kein einziges Paket.

Kanal 1 wird vollständig verworfen, weil es keinen zweiten Empfänger
gibt. Das ist gewollt und kostet nur Netz, keine Richtigkeit.

**Was ich hier nicht erklären kann:** die beiden Kanalzähler kommen
ungleich heraus (724/s gegen 1014/s), obwohl `byte9` paarweise
wechseln sollte. Ein Teil davon ist Buchführung — die Kanalzähler laufen
ab dem Verbinden, also enthält Kanal 0 noch die 48-kHz-Phase, Kanal 1
nicht. Der Rest bleibt offen. Es ist **kein** Verlust: die
Folgenummernprüfung meldet 0 von 4797 über denselben Zeitraum. Wer hier
weitermacht, soll die Zähler erst ab dem Umstellen laufen lassen und
dann neu messen, statt diese Zahl zu deuten.

---

# Die Wiederholungen bei 96 kHz (gemessen 2026-10-04)

Bei 48 kHz meldet der Treiber 1,00 Kopien je Nummer, bei 96 kHz 1,2 bis
1,5. Das sind rund 105 bytegleiche Pakete je Sekunde, die niemand
braucht — und der Grund, warum die Verbindung 13,2 Mbit/s zieht.

**Die Quittung wirkt, sie ist nur nicht vollständig.** A/B am Gerät, je
8 s, `STROMMODUS=je96`:

| | Pakete/s | Wiederholungen/s | Kopien je Nummer |
| --- | --- | --- | --- |
| `BLOCKANTWORT=0` (aus) | 877 | 401 | 1,84 |
| Vorgabe (an) | 582 | 105 | 1,22 |

Sie nimmt also drei Viertel weg. Nebenbei: ohne Quittung sind es bei
96 kHz 1,84 Kopien, nicht die 8,1 von 48 kHz — das Wiederholverhalten
des Geräts hängt selbst von der Betriebsart ab.

**Widerlegt: den Kopf des Geräts spiegeln.** Die Blockantwort trägt fest
`byte8=0x01, byte9=0x00` (ein Strom, Kanal 0), während das Gerät bei
96 kHz mit `byte8=0x02` und wechselndem `byte9` sendet. Naheliegende
Vermutung: das Gerät ordnet die Quittung dem falschen Strom zu. Als
Schalter eingebaut und gemessen — **kein Unterschied**:

    fest:       147 → 117 → 104 → 99 Wiederholungen/s
    gespiegelt: 150 → 127 → 101 → 100

Der Schalter wurde danach wieder entfernt. `byte8`/`byte9` beschreiben
im Hinweg unseren eigenen Strom, nicht den des Geräts; die Vermutung war
von Anfang an wacklig, und die Messung hat sie erledigt.

**Noch offen, mit Hinweisen für den Nächsten:**

- Die Wiederholungen **fallen** über die ersten Sekunden (147 → 99) und
  pendeln sich bei ~100/s ein. Ein Teil ist also Einschwingen, nicht
  Dauerzustand. Wer hier misst, soll die erste Sekunde wegwerfen.
- Der Antwort-Ring hält 32 Nummern. Bei 96 kHz sind das nur ~54 ms, und
  die Nummern laufen innerhalb davon um. Ob dadurch Quittungen
  ausbleiben, die das Gerät noch erwartet, ist nicht gemessen — das wäre
  der nächste Versuch (Ring vergrößern, dieselbe A/B-Messung).
- Ungeprüft: ob das Gerät erwartet, dass wir bei zwei Strömen auch ZWEI
  Stille-Ströme zurückschicken statt einen. Das ist etwas anderes als
  den Kopf zu spiegeln und wäre der Versuch danach.

Es ist kein Richtigkeitsfehler: der Verlust liegt bei 0,02–0,04 %, der
Ton läuft. Es ist Netzlast.

---

# Offen: Beacon kommt am Rechner an, aber nicht in der App (2026-10-04)

Am Vormittag kam der Betreiber mehrfach nicht an die QRP: „no beacon
reply", Abbruch nach 3 s. Ausgeschlossen, jedes einzeln am Gerät geprüft:

- **Gerät und Netz** — mein Prüfstand verbindet in 51–54 ms, Sekunden
  vorher oder nachher, auf derselben Maschine.
- **Hängende Sitzung** — der Abmelde-Rahmen der vorigen Instanz war
  sauber quittiert (`Stopp beim Trennen quittiert nach Versuch 1`),
  34 s vor dem Fehlversuch.
- **Port im Eintrag (1024), `MANUAL:`-Schlüssel, TCI-Server auf 50001,
  seine Einstellungsdatei, seine installierte Fassung** — alle mit
  SEINEN Dateien und SEINEM Binary nachgefahren, alle verbinden.
- **Sockets** — ein Beobachter mit 10 Abfragen je Sekunde hat im Moment
  seines Klicks beide Ports gesehen: `UDP *:50001` und `UDP *:50002`.
  Kein Bindefehler im Log.

**Der entscheidende Befund** kommt aus einem `tcpdump` während eines
Fehlversuchs — vier Pakete, alle in dieselbe Richtung:

    10:27:45.327  192.168.16.200:50001 -> 192.168.16.100:50001  24 B  03 ff 01 1a 7c …
    10:27:50.798  dito
    10:28:04.804  dito
    10:28:09.978  dito

Das ist die **Beacon-Antwort**, viermal, korrekt adressiert an den
Rechner und an Port 50001 — genau den Port, den die App gebunden hat.
Die App hat trotzdem keine einzige verarbeitet (`beacon reply`-Zeile
kommt in den Fehlläufen null mal vor, im erfolgreichen Lauf einmal).

Die Datagramme erreichen also den Rechner, aber nicht den Socket der
App. Beide Ports werden mit `ShareAddress | ReuseAddressHint` gebunden;
bei `SO_REUSEPORT` stellt der Kern ein Unicast-Datagramm genau **einem**
Socket zu. Wer der zweite Socket wäre, ist offen — `RadioDiscovery`
spricht kein SunSDR, der TciServer hört auf TCP.

Zweiter offener Punkt aus demselben Mitschnitt: der Rahmenbau schickt
die Anfrage laut Code **auch direkt** an `m_radioInfo.address`, nicht
nur als Rundruf. Im Mitschnitt (Filter `host 192.168.16.200`) steht
davon **nichts** — dieses Paket ging nie hinaus.

**Wie es weitergeht, wenn es wieder auftritt:** die App mit
`QT_LOGGING_RULES='longpath.sunsdr.debug=true'` starten. Dann steht
jedes empfangene Steuerdatagramm im Protokoll, und die Frage „kommt es
im Socket an?" ist in einer Zeile beantwortet statt in einer halben
Stunde.

## Aufgeklärt: der Aussetzer dauert etwa eine Minute (2026-10-04, abends)

Der Befund oben („Beacon kommt am Rechner an, aber nicht in der App")
hatte eine einfachere Ursache, als er aussah — und sie ist messbar.

**Das Gerät sperrt nach einem ABRUPTEN Ende rund eine Minute.** Gemessen:

| Was vorher geschah | Gerät antwortet wieder nach |
| --- | --- |
| Prüfstand verbindet und trennt sauber | **3 s** |
| App: `disconnect`, dann `SIGTERM` | **sofort** |
| App: `disconnect`, dann `kill -9` | **~60 s** |

In allen drei Fällen war der Abmelde-Rahmen quittiert. Der Unterschied
ist also nicht der Stopp, sondern das **abrupte Schließen der Sockets**:
nach `kill -9` schickt das Gerät weiter an einen Port, den der Rechner
mit ICMP zurückweist — und danach schweigt es eine Weile.

**Damit ist Martins Vormittag erklärt.** Jeder Fehlversuch lag innerhalb
dieses Fensters; er hat sofort wieder geklickt, also wieder hinein. Das
Aus- und Einschalten half nicht, weil es den Zustand löste, sondern weil
es Zeit kostete.

**Zwei Konsequenzen:**

1. Eine verbundene Instanz **nie** mit `kill -9` beenden — `SIGTERM`
   oder das Fenster. Das gilt für Prüfläufe genauso wie für den Betrieb.
2. Schlägt eine Verbindung mit „keine Antwort" fehl, hilft **warten**,
   nicht erneut klicken. Eine Minute reicht.

Offen bleibt, ob Longpath das selbst abfangen kann — etwa, indem es nach
einem Fehlversuch nicht sofort wieder sucht, sondern das Fenster
abwartet und es dem Betreiber sagt. Das wäre die nächste Änderung an
dieser Stelle.

### Zweite Vermutung zu den Wiederholungen: auch widerlegt (2026-10-04)

Der im Abschnitt oben als „nächster Versuch" notierte Gedanke war: wenn
das Gerät **zwei** Ströme schickt, auch **zwei** Stille-Ströme
zurückschicken (byte8 = 0x02, je ein Paket mit byte9 0x00 und 0x01)
statt einem. Das ist etwas anderes als den Kopf zu spiegeln — dort ging
es um ein Paket mit fremdem Kopf, hier um zwei kohärente Ströme.

Eingebaut als Schalter, A/B über je 10 s bei je96:

    eine Antwort:   111 / 115 / 101 Wiederholungen je Sekunde
    zwei Antworten: 108 / 117 / 106

Ununterscheidbar. Der Schalter wurde wieder entfernt.

**Damit sind beide Vermutungen aus diesem Dokument erledigt.** Die rund
110 bytegleichen Wiederholungen je Sekunde bei 96 kHz sind Verhalten des
Geräts, auf das wir von hier aus keinen Hebel gefunden haben. Wer
weitermacht, soll **nicht** noch einmal am Kopf der Blockantwort drehen
— beides ist gemessen und negativ. Der nächste sinnvolle Schritt wäre
ein Mitschnitt von ExpertSDR2 **bei 96 kHz**: wenn es dort auch
wiederholt, ist es schlicht die Eigenart des Geräts und kein Mangel von
Longpath.
