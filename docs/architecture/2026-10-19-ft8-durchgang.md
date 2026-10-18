# Der erste FT8-Durchgang an der Antenne

**Stand 2026-10-18.** FT8, FT4 und RTTY sind gebaut, geprüft und auf
main — aber **nie an einem echten Signal gehört**. Dieses Blatt macht
daraus einen Durchgang statt sieben Einzelentdeckungen.

Dauer: eine halbe Stunde. Kein Senden nötig — der erste Durchgang ist
reines Zuhören.

---

## Vorher, einmal

| | |
| --- | --- |
| Rufzeichen/Locator | stehen schon: `OE5SOS` / `JN67VV` |
| App | `open /Applications/Longpath.app` |
| Radio | verbinden (das Fenster zeigt sonst *Disconnected*) |
| Frequenz | **14.074** (20 m) — auf 14.210 steht nichts von FT8 |
| Betriebsart | **MODE → „…" → DIGU** — daran hängt die Freigabe des Fensters |
| Fenster | **„+" rechts unten → FT8 Decoder**, dann **↗** für ein eigenes Fenster |

---

## Die vier Fragen, in dieser Reihenfolge

### 1. Dekodiert es überhaupt?

Eine Viertelstunde zuhören. Erwartet: alle 15 Sekunden erscheinen
Zeilen, Form `HHMMSS  -12   0.2  1234  CQ DL1ABC JN49`.

* **Zeilen kommen** → weiter mit Frage 2.
* **Keine Zeilen, Balken läuft** → der Dekoder bekommt Ton, findet
  nichts. Erst Frequenz und Seitenband prüfen (DIGU, nicht DIGL).
* **Balken läuft nicht** → es kommt kein Ton an. Dann hat der Abgriff
  das Problem, nicht der Dekoder.

**Aufschreiben: wie viele Zeilen je Fenster.** Auf 20 m am Abend sind
zehn bis dreißig normal. Deutlich weniger heißt: der Dekoder findet
nur die lautesten, und dann lohnt ein Blick auf die Pegelkette.

### 2. Stimmen die Zahlen?

Die Spalten sind dieselben wie in WSJT-X: Zeit, dB, DT, Hz, Nachricht.

* **DT** sollte zwischen −1,0 und +1,0 liegen. Systematisch daneben
  heißt: die Uhr des Rechners geht falsch — nicht Longpath.
* **Hz** muss zur Stelle im Wasserfall passen. Tut es das nicht, ist
  die Umrechnung im Streifen falsch, und der Klick trifft daneben.

### 3. Zeigt der Wasserfall, was zu hören ist?

Der Streifen über der Liste: **jede dekodierte Zeile muss einen
sichtbaren Gipfel an ihrer Hz-Stelle haben.** Das ist die Probe auf
die Anzeige — sie ist bisher nur an einem erfundenen Ton geprüft.

Dann einmal **in den Streifen klicken**: das Feld „TX Hz" muss
mitspringen, und die grüne Marke auf die geklickte Stelle.

### 4. FT4 — der einzige Punkt mit echtem Risiko

Auf **7.047** oder **14.080**, Knopf **FT4** im Fenster.

FT4 hat nur **7,5 Sekunden** je Durchgang, und die Rechenzeit muss
hineinpassen. Der Prüfstand sagt, sie tut es — aber am Schreibtisch
ohne Signal rechnet der Dekoder auch kaum etwas.

**Aufschreiben: kommen die Zeilen noch VOR dem Ende des nächsten
Fensters?** Wenn der Balken schon wieder läuft, bevor die vorigen
Funde stehen, ist die Rechenzeit zu knapp. Das wäre der eine Befund,
der Arbeit nach sich zieht.

---

## Erst danach: senden

Mit **CQ** oder Doppelklick auf eine Zeile. **„Auto" bleibt zunächst
aus** — ein Fenster je Druck auf *Send*, damit man sieht, was
hinausgeht, bevor die Automatik es selbst tut.

Vorher eine freie Stelle im Wasserfall suchen und anklicken: die
Sendestelle stand bis heute fest auf 1500 Hz, und dort ist auf einem
vollen Band immer schon jemand.

---

## Was dieses Blatt NICHT prüft

* **RTTY** — eigener Durchgang, DIGL, und es gibt dort keinen
  QSO-Ablauf, nur „Text senden".
* **Das Logbuch** — erst nach einem vollständigen QSO.
* **Die Farben** (neues Land, schon gearbeitet) — sie brauchen eine
  geladene `cty.dat` und ein gefülltes Logbuch.
