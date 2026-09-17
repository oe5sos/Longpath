# Design-Durchsicht: Bandfilter zuerst, dann das ganze Bild

**Stand:** 17. September 2026, Nachmittag
**Anlass:** „was können wir bei longpath im design optimieren und
verbessern, vor allem bandfilter, aber auch das komplette design" —
„will stetig das design und technik verbessern."
**Grundlage:** das Bildschirmfoto des Betreibers von 15:11 (ANVELINA
PRO 3, 40 m LSB, 7.192.500, Fenster 1470 × 858), der Quelltext auf
`d3303922`, `HAUSSTIL.md`, die Drift-Ratsche.

Dies ist eine Befundliste mit Vorschlägen. Gebaut ist davon nichts —
Gestaltung entscheidet der Betreiber am Bild. Für den Bandfilter liegen
Entwurfsblätter bei (ein Bild je Variante, mit dem echten Widget
gemalt, wirkliche Größe 620 × 140 Punkte bei Retina-Dichte).

---

## 1. Bandfilter (`BandwidthFilterPane`, `BandwidthFilterApplet`)

### Was auf dem Foto auffällt

| Nr. | Befund | Beleg |
| --- | --- | --- |
| B1 | **Die Achse liest sich krumm.** Marken bei 7.189 · 7.191 · 7.192 · 7.194 · 7.197 — gleiche Abstände (2 kHz), ungleiche Zahlen. Die Marken sitzen bei VFO ± k·2 kHz; steht der VFO auf 7.192.**500**, liegen alle auf halben Kilohertz, und die dreistellige Anzeige rundet mal auf, mal ab. | `BandwidthFilterPane.cpp` Achsenschleife (`for (int hz = -(m_spanHz / 2 / stepHz) * stepHz …`) |
| B2 | **Die Anteilszellen liegen mitten in der Kurve.** Sieben dunkle Kästen („−3.0k 16dB" …) decken das untere Drittel des gefüllten Durchlasses — genau die Fläche, die der Betreiber heute Vormittag „von oben bis unten" haben wollte. Zwei Zellen melden dieselbe Lage (−1.4k, −1.4k). | `paintEvent`, Block „Was liegt DRIN?" |
| B3 | **Die Feinskala oben im Durchlass** (Linie mit drei Teilstrichen) sagt laut eigenem Kommentar „nichts Neues" und liegt als zweiter Rahmen quer durch die Säule. | `paintEvent`, Block „Feinskala oben im Durchlass" |
| B4 | **Blaustichige Graus.** Hintergrund `#101014`, Gitter `#141418`, Pegelraster `#1c1c22`, Griff-Füllung `#1e2a36` / `#16202a`, Breitenkästchen `#0b0b0d` — alles namenlos, vier davon blau (b > r = g). Hausstil: „Grautöne sind neutral, nie blaustichig." | fünf Literale in `paintEvent` |
| B5 | **Radius 3** am Breitenkästchen und an beiden Griffen. Hausstil: „Nie Radius 3. Das ist der Qt-Standardwert." | `drawRoundedRect(box, 3, 3)`, `drawRoundedRect(h, 3, 3)` |
| B6 | **Zahlen proportional.** Kantenwerte („3.00 kHz", „100 Hz"), Breite („2.9 kHz"), Achse („7.191") und dBm-Marken stehen in der Textschrift; die Größen sind Punktabzüge (−2, −3, −3.5, −4) neben der Schriftleiter. Hausstil Regel 4: Zahlen sind Monospace; Leiter {9, 11, 13, 16, 22, 38}. | `small`, `value`, `tiny`, `mark` in `paintEvent` |
| B7 | **RX1-Kapsel in Blau** (Rahmen und Füllung). Sie ist eine Beschriftung, keine Bedienung — „Blau ist anfassbar." Mit einem Empfänger sagt sie außerdem nichts, was der Kopf nicht schon sagt. | Block „Die Beschriftung des Empfaengers" |
| B8 | **Bedienzeile**: `LSB LOW [100 Hz ▲▼] WIDTH [2900 Hz ▲▼] HIGH [3000 Hz ▲▼] VAR 1 VAR 2 ↺ Centre … AUTO`. Drei Qt-Spinboxen mit Pfeilen, Wortmarken als fette 11-px-Labels *neben* den Feldern. Hausstil Regel 1 (Versalzeile *über* der Gruppe) und „große Zahlen liegen in schwarzem Glas" sind hier nicht angekommen. | `BandwidthFilterApplet::buildUI` |

### Vorschläge (Blätter liegen bei)

- **0 IST** — der heutige Stand, 620 × 140.
- **A Spitzenmarken** — B1, B3, B4, B5, B6, B7 umgesetzt; statt der
  Zellenreihe bekommen die zwei, drei höchsten Spitzen im Durchlass eine
  kleine Marke *über* der Spitze (Tonlage im Betrag · dB über dem Flur),
  wie ein Analysator seine Peaks beschriftet. Nichts verdeckt die Fläche.
- **B Nur Kurve** — wie A, ohne jede Beschriftung im Durchlass.
- **C Zellen bleiben** — wie A, aber die Zellenreihe von heute bleibt,
  wie sie ist (zeigt, was allein die Punkte B1/B3–B7 ausmachen).

Die Bedienzeile (B8) ist ein eigenes Blatt, nach der Wahl oben.

**Unstrittig, kein Ermessen** (sollte in jeder Variante rein): B1
(Achse), B4 (Graus), B5 (Radius), B6 (Monospace/Leiter). Ermessen: B2,
B3, B7, B8.

---

## 2. Das ganze Bild

### Technik, sichtbar auf dem Foto

| Nr. | Befund | Beleg |
| --- | --- | --- |
| T1 | **Fußleiste läuft bei 1470 px über.** Rechts steht „● ON AI" — das R ist abgeschnitten; links steht nur „CH 1 / 20m (idle)", **CH 0 fehlt**, obwohl beide Ketten auf Sprosse 4 liegen und gemeinsam falten müssten. Zu klären am laufenden Gerät (`dumpTree` zeigt `chainIndicator0/1` samt Sichtbarkeit). | `ChromeBarItems.cpp` (beide Ketten Sprosse 4), AX-Baum des Fensters: letztes Element `[1398, 810, 44 × 16]` in einer 1440 breiten Leiste |
| T2 | **„20m (idle)" auf 40 m.** Bei leerer Kette trägt der Ruhezustand das zuletzt geschaltete Band. Hausstil Regel 7: Unbekannt ist ein Strich, keine alte Zahl — `idle` ohne Band oder `——`. | `AlexController.cpp:138` |
| T3 | **Drift-Ratsche rot**: 79 namenlose Farben (Decke 63), 1563 Literale (1522), 3 Schriftgrößen außerhalb der Leiter (`RttyDecoderApplet.cpp:151/344/350`, 10 px), 21 keyed_port_defaults (19). Seit 20.08. auf FEHLER, und sie wächst. | `python3 scripts/verify-style-drift.py --verbose` |
| T4 | **Radius 3 an 24 Stellen**: 12 × `border-radius: 3px` (MainWindow, QsoRecorder, KiwiSdr ×3, Rx, AppletPanel, Dvk, Asr, GridCell, VfoTileRow) und 12 × `drawRoundedRect(…, 3, 3)` (SpectrumWidget ×3, StripGraphics ×2, TitleBar, SpectrumStatusOverlay, LayoutThumbnail, FlatMap, SwrCurve, BandwidthFilterPane ×2). | `grep` |
| T5 | **Alte blaustichige Palette in Konstanten**: `kSliderStyle` (`#1a2a3a`), `kButtonStyle` (`#1a2a3a`, `#304050`, `#c8d8e8`, `#0f0f1a`), `kOverlayBtnBg` (`rgba(20,30,45)`), `kPanadapterBg` (`#141e27`). Die TX-Regler (Leistung/Tune) laufen sichtbar in einer blauen Rinne. | `StyleConstants.h:164, 420, 844–853` |

### Gestaltung, sichtbar auf dem Foto

| Nr. | Befund | Vorschlag |
| --- | --- | --- |
| G1 | **Zwei Sprachen in einer Oberfläche.** „Bandwidth Filter", „Frequenz", „Leistung", „Tune", „VORLAUF", „SIGNAL AVE", „Spitze S9", „no target", „callsign", „Log QSO", „↺ Centre", „Click to connect"; im Verbindungsdialog „Disconnected — Das Gerät wurde gefunden …". | Eine Sprache festlegen. Empfehlung: Englisch für alles Sichtbare (die Fachwörter MOX, VOX, TUNE, BW, S-Meter sind ohnehin englisch; Zeus und Thetis sind englisch), Deutsch bleibt in Kommentaren und Dokumenten. Alternativ Deutsch durchgehend mit `tr()` — dann aber überall. |
| G2 | **Panelköpfe** („Bandwidth Filter", „Panadapter pan-0", „Rotor / Log", „S-Meter", „TX", „Frequenz"): fett, gemischte Schreibung, 11 px. Hausstil Regel 3: Titel versal mit weiter Laufweite (`Style::capsFont`). „pan-0" ist ein interner Bezeichner im Titel. | Kopf auf `capsFont(base, 9)`, Farbe Beschriftung; interne Kennungen raus aus dem Titel. Ein Blatt, weil es jedes Panel trifft. |
| G3 | **Fünf blaue Knöpfe in einer Reihe** (40m · LSB · 2.9k · 100 Hz · NR2) plus blauer Lautstärkeregler plus blaues „Longpath" plus blaue Durchlass-Säule: Blau ist die Auswahlfarbe, aber in Summe liegt es deutlich über zwei Prozent. Dazu die **gelbe Glühbirne** rechts oben — die einzige gesättigte Fremdfarbe im Kopf. | Auswahl auf den Hausstil-Verlauf `#254a72 → #1e3d5f` (heute `kBlueBg = #3576e0`, kräftiger als die Vorgabe); Lautstärke auf Grau mit Bernstein-Wert; Glühbirne auf Skala-Grau. |
| G4 | **Verbindungsdialog** — der erste Bildschirm, den jeder sieht: Qt-Gruppenrahmen mit Titel im Rand („Discovered Radios", „Selected Radio"), Qt-Tabellenkopf, kräftige Auswahlzeile, gesättigtes Grün für „free", eine gemischtsprachige Fehlerzeile mit Aufzählungspunkt. | Gruppen als Versalzeile ohne Rahmen (Regel 1), Tabelle ohne Kopfrahmen, Auswahl im Hausstil-Blau, „free/saved" als Kapsel statt Farbtext. |
| G5 | **Frequenz-Applet**: POWER und SWR stehen ohne Wert da (leer, nur Haarlinie); „S8 DBM" — eine S-Stufe mit dBm-Einheit. S-Meter: „SIGNAL AVE / Spitze S9 / **S8 dBm**" dasselbe. | `——` statt leer (Regel 7); Einheit nur bei dBm-Wert, bei S-Stufe keine. |
| G6 | **Wasserfall/3D-Spektrum** in Blau-Grün-Rot ist die bunteste Fläche des Bildes. Das ist Nutzereinstellung — aber die Vorgabe könnte eine Hausstil-Palette sein (Fast-Schwarz → Stahlgrau → Bernstein → Creme), wie Zeus sie zeigt. | Eine „Longpath"-Palette als wählbare Vorgabe, kein Zwang. |
| G7 | **Slider-Griffe** (TX: Leistung, Tune) sind blaue Kugeln auf blauer Rinne — Qt-Optik. | Griff als schmale Pille im Knopfgrau mit Bernstein-Wert daneben (steht schon da: „8", „1"). |

---

## 3. Reihenfolge, vorgeschlagen

1. **Bandfilter, Blatt wählen** (A/B/C, oder Mischung) → bauen, live
   prüfen, dann die Bedienzeile als zweites Blatt.
2. **T1/T2 Fußleiste** — echter Fehler, am Gerät reproduzieren.
3. **T3–T5 Ratsche** — die unstrittigen Aufräumarbeiten (Radius 3,
   blaustichige Konstanten, drei 10-px-Schriften), Datei für Datei,
   jede Stelle nach ihrer Rolle.
4. **G1 Sprache** — eine Entscheidung, dann eine Durchsicht aller
   sichtbaren Strings.
5. **G2 Panelköpfe, G3 Blau-Summe, G4 Verbindungsdialog** — je ein Blatt.
6. G5–G7 nebenbei, wenn das jeweilige Applet ohnehin angefasst wird.

---

## 4. Nachtrag, Abend: Richtung gewählt und angefangen

Die vier Bandfilter-Blätter aus Abschnitt 1 hat der Betreiber abgelehnt
(„gefällt keines") — Feinschliff war nicht gefragt. Auf die Frage „was
kannst du generell beim Rendering anbieten, komplett Longpath" gab es
vier Richtungen als Skizzen desselben Ausschnitts (Knopfreihe ·
Bandfilter · TX-Feld): **1 Zeus-Disziplin · 2 Haarlinie/leicht · 3 Glas
& Tiefe · 4 Analog-warm/Phosphor.** Gewählt: **3**.

Was „Glas & Tiefe" konkret ist, steht als Abschnitt in
`StyleConstants.h` (`kGlass*`): Flächen versenkt (Schwarz, Innenschatten
oben, Lichtkante), Knöpfe erhaben (Verlauf, Lichtkante, dunkle
Unterkante), Zahlen in Glaschips, Kurven mit Hof, Auswahl als gedeckter
Blauverlauf.

**Gebaut (Zweig `design/glas-und-tiefe`, zwei Commits):**

| Schritt | Bauteil | Stand |
| --- | --- | --- |
| 1 | `BandwidthFilterPane` neu gezeichnet, Achse auf runden Frequenzen (B1), Zellen/Feinskala weg (B2/B3), neutrale Graus (B4), Radius (B5), Monospace (B6), RX-Kapsel grau (B7) | fertig, Blatt + grab |
| 1 | `BandwidthFilterApplet` Bedienzeile: Versalzeilen, Glasfelder ohne Pfeile (B8) | fertig, grab |
| 1 | `GridCellWidget`: Titel versal mit Laufweite (G2, erster Teil) | fertig |
| 2 | `HGauge` Vorlauf/SWR versenkt, Bernstein-Verlauf, Mono-Zahl (G7 z. T.) | fertig, Blatt |
| 2 | Regler (`sliderHStyle`), Wertchips (`insetValueStyle`), Messingtaste, Knöpfe app-weit mit Licht/Schattenkante | fertig |
| 2 | `CommandBar` Pillen erhaben, Auswahl gedeckt (G3, erster Teil) | fertig, Blatt |

**Offen, in dieser Reihenfolge:** Panel-Flächen selbst (Verlauf, Radius,
Schatten — braucht einen Zellabstand > 0 im `AppletGrid`, heute 0, also
ein Blatt vorher); T1/T2 Fußleiste; Verbindungsdialog (G4); Sprache
(G1); Frequenz-Applet/S-Meter (G5); Wasserfall-Palette (G6).
