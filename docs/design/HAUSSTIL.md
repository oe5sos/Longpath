# NereusSDR — Hausstil

Verbindlich für jede sichtbare Änderung. Vorbild ist **Zeus Link** —
nicht dessen Palette, sondern dessen Disziplin.

Stehende Anweisung, OE5SOS, 2026-08-15:

> „Es wird zwar in Zukunft immer technische Neuerungen von Nereus geben,
> will diese aber immer auf Design geändert haben."

Ein Feature ist also erst fertig, wenn es in dieser Sprache herauskommt —
nicht, wenn es funktioniert.

Gerendert zum Ansehen: [`zeus-hausstil.html`](zeus-hausstil.html).

---

## Die eine Regel, aus der die anderen folgen

**Farbe bedeckt höchstens zwei Prozent der Fläche.** Alles andere ist
Grau auf Fast-Schwarz. Wirkt ein Screenshot bunt, ist er falsch — egal
wie gut die einzelnen Farben gewählt sind.

> „will keine auffälligen Farben, sehr dezent, Stil. auch leichte
>  Übergänge." — OE5SOS, 2026-08-15

---

## Token

Grautöne sind **neutral**, nie blaustichig. Die bisherige Palette
(`#0f0f1a`, `#0a0a18`, `#203040`, `#1a2a3a`) hatte durchgehend Blaustich.

> **App-Grund, korrigiert 2026-08-15.** Hier stand `#050506`. Der Code
> und [`oe5sos.example.json`](oe5sos.example.json) tragen für dieselbe
> Rolle `#08080a`, und zwei Zahlen für eine Rolle sind schlimmer als
> die falschere von beiden — dann streiten Vorlage und Programm, und
> wer nachschlägt, bekommt je nach Quelle eine andere Antwort. Die
> Theme-Datei gewinnt: sie ist die Stelle, an der die Palette
> tatsächlich gewechselt wird.

### Flächen

| Rolle | Wert |
|---|---|
| App-Grund | `#08080a` |
| Panel | `#0c0c0e` |
| Panelkopf | `linear-gradient(#16161a, #111113)` |
| Versenkt (Glas) | `#000000` + `inset 0 2px 8px #000`, `inset 0 1px 0 #ffffff0a` |
| Knopf | `#16161a` · hover `#1e1e22` |
| Rahmen fein | `#191a1d` · Rahmen `#26262b` |

### Text

| Rolle | Wert |
|---|---|
| Werte | `#c4c4c9` — kein reines Weiß |
| Beschriftung | `#8e8e93` |
| Skala | `#5c5c60` |
| Inaktiv | `#3d3d41` |

### Bedeutung — jede Farbe genau ein Job

| Rolle | Wert |
|---|---|
| Auswahl | Füllung `linear-gradient(#254a72, #1e3d5f)`, Rahmen `#2f5c86`, Text `#cfe2f5` |
| Messwert / Kurve | `#c2924f` |
| Warnung / Grenze | `#a8853f` |
| Bestätigt | `#6fa384` |
| Sendet / Gefahr | `#a86b6d` — nur MOX/TX darf kräftiger, `#c25a5c` |
| Panelakzent | `linear-gradient(#a87a3c, #8a6a30)`, 3 px links im Kopf |

### Instrumentenglimmen

S-Meter, SWR-Zifferblatt, Gain Reduction:

```
radial-gradient(#5c5842 85 % → #33322a 45 % → transparent)
Zeiger und Teilung cremeweiß #c8c8c0 — nie farbig
```

### Die Trennung, um die es geht

**Türkis `#00b4d8` wird abgeschafft.** Es machte zwei Jobs gleichzeitig —
„ausgewählt" und „gemessen". Genau diese Trennung ist der Kern:

> **Blau = anfassbar. Warm = gemessen.**

---

## Die acht Regeln

1. **Jede Bedienung steht in einer benannten Gruppe.** Über jeder
   Knopfreihe eine Versalzeile: 8–9 px, Laufweite `.18em`, Farbe Skala.
   Nie ein Knopf ohne Überschrift.
2. **Genau ein Knopf pro Gruppe leuchtet.** Aktiv gefüllt, inaktiv fast
   unsichtbar, am Ende ein `…` für den Rest. Drei sichtbar, nicht
   dreizehn.
3. **Jedes Panel hat denselben Kopf:** 3 px Akzentbalken, ⠿ zum Ziehen,
   Titel versal mit weiter Laufweite, rechts Schloss und ✕.
4. **Zahlen sind Monospace, Wörter nicht.** Alles, was sich ändert und
   verglichen wird — Frequenz, SWR, dB, ADC, Zeit, Rufzeichen.
5. **Zustand steht als umrandete Kapsel rechts oben,** immer an
   derselben Stelle.
6. **Ein Modul erklärt sich in zwei Zeilen:** Name, darunter ein Satz in
   Grau. Erst dann Regler.
7. **Unbekannt ist ein Strich, keine Null.** `——` statt `0.00` — eine
   Null sieht aus wie eine Messung. Die Koppler-Anzeige zeigt heute
   `VOR 0 · RÜCK 0`, wenn gar nicht gemessen wurde.
8. **Die Fußleiste trägt nur Zustände.** Inventar gehört ins Menü.

---

## Weiche Übergänge

- **Kurven:** dünne Linie (1,6 px) plus Verlauf darunter, 22 % → 0 zur
  Grundlinie. Gewicht ohne Lautstärke.
- **Flächen:** Panelköpfe und aktive Pillen mit 4-%-Verlauf und 1 px
  Lichtkante oben. Nicht als Effekt sichtbar, nur als Tiefe.
- **Skalen:** ein durchgehender Verlauf statt harter Zonen grün/gelb/rot.
- **Zustandswechsel:** 150 ms `ease`. Nichts springt.

---

## Instrumente

- **Pegel sind Verlaufssäulen, keine LED-Ketten.** Verlauf dunkel oben →
  warm unten, Skalenziffern winzig links daneben, Übersteuerungsmarke als
  roter Strich. Überschrift zweizeilig: Quelle oben (`MIC`), Art darunter
  (`PK`).
- **Bogeninstrumente glimmen.** Radialverlauf in entsättigtem Oliv unter
  dem Bogen — kein benennbarer Farbton, nur der Eindruck einer Skala
  hinter Glas.
- **Große Zahlen liegen in schwarzem Glas.** Schatten nach innen,
  Lichtkante oben, Einheit klein und rechts abgesetzt.
- **Ablesegitter:** gleich große Felder, Überschrift versal klein, Wert
  monospaced darunter; darunter eine Zustandsreihe, in der alle
  Möglichkeiten sichtbar sind und die zutreffende eine Spur heller ist.
  Man sieht dann auch, was es *nicht* ist.

---

## Maße

```
Pille          Höhe 26–28 px, Radius 6, Polsterung 0 11 px
Panel          Radius 8, Kopf 32–34 px
Versenktes     Radius 6
Gruppenabstand 22–26 px waagrecht
Knopfabstand    5 px innerhalb einer Gruppe
```

**Nie Radius 3.** Das ist der Qt-Standardwert und lässt alles nach Qt
aussehen.

---

## Wie die Palette gewechselt wird

Stand 2026-08-15: **1733 Hex-Literale, 276 verschiedene Farben, 130
Dateien.** 45 davon haben einen Namen in `StyleConstants.h`; 1115
Literale entsprechen exakt einem dieser Namen, 618 sind Farben, die in
der Palette gar nicht vorkommen.

1141 der Literale stehen **in Qt-Stylesheet-Strings**:

```cpp
w->setStyleSheet("QLabel { color: #c8d8e8; font-size: 13px; }");
```

Solche Strings lassen sich nicht durch eine Konstante ersetzen, ohne sie
in eine `.arg()`-Kette umzubauen — 130 Dateien Handarbeit mit hohem
Fehlerrisiko und ohne Möglichkeit, zwischendurch zu bauen.

Deshalb der Umweg über **`Style::themed()`** (`gui/styles/ThemeQss.h`):

```cpp
w->setStyleSheet(Style::themed("QLabel { color: #c8d8e8; }"));
```

Die Funktion bildet die alten Palettenwerte auf die jeweils aktive
Theme-Farbe ab. Zwei Eigenschaften machen das brauchbar:

- **Heute ändert sie nichts.** Die Abbildung ist zunächst die Identität,
  also ist das Einwickeln eines Aufrufs risikofrei und unsichtbar.
- **Es geht Datei für Datei.** Eine eingewickelte Datei folgt dem neuen
  Theme, eine nicht eingewickelte behält die alten Farben, und nichts
  bricht dazwischen.

Wenn alle Aufrufe eingewickelt sind, ist der Palettenwechsel eine
Tabelle in einer Datei.

### Das größere Hindernis: 241 namenlose Farben

`themed()` bewegt nur, was einen Namen hat. Die Inventur sagt:

```
python3 tools/colour_audit.py
```

Der Ausgangsstand war **276 verschiedene Farben, 241 davon namenlos**:
78 verschiedene Blautöne, 40 Grüntöne, 40 Rottöne. Das ist keine
Palette, das ist Drift — jeder hat hingeschrieben, was gerade passend
aussah. Eine Palette mit 276 Farben lässt sich nicht umstellen.

Das Skript misst den Abstand jeder namenlosen Farbe zur nächsten
benannten in CIE-Lab und teilt danach ein:

| Abstand | Bedeutung |
|---|---|
| ΔE < 8 | nicht unterscheidbar — ersetzbar, keine Entscheidung |
| ΔE 8–18 | sichtbar, aber nah — eine Entscheidung pro Farbe |
| ΔE > 18 | eigene Farbe — braucht einen Namen oder fällt weg |

`kEqBand*` scheidet als Ziel aus. Die acht Töne existieren, um
überlagerte Entzerrerkurven auseinanderzuhalten; ein Knopf im
Hauptfenster, der daran hängt, ändert seine Farbe, sobald jemand die
EQ-Töne nachjustiert.

### Reihenfolge

1. ✅ **ΔE < 8 einsammeln** — erledigt 2026-08-15.
   `--apply` hat 221 Vorkommen in 52 Dateien angeglichen, 79 Farben.
   Wert durch Wert (`#404858` → `#3a4a5a`), nicht Wert durch Konstante:
   reine Textersetzung, die den Build nicht brechen kann, und
   anschließend greift `themed()`.
   **276 → 202 Farben.**
2. **ΔE > 18 benennen** — 74 Farben, 187 Vorkommen. Die häufigsten:
   `#adff2f` (11×), `#ffff00` (10×), `#c14848` (6×). Jede braucht eine
   Rolle oder muss verschwinden. Reines Gelb und Grüngelb haben in einer
   Oberfläche mit diesem Anspruch nichts verloren.
3. **ΔE 8–18 einzeln entscheiden** — 88 Farben, 201 Vorkommen. Meist
   wird die Antwort dieselbe sein wie in Schritt 1.
4. **`setStyleSheet` einwickeln**, Fenster für Fenster.
5. **Rechte Spalte der Theme-Tabelle bewegen.**

Schritte 1 bis 3 ändern nichts Sichtbares und sind jederzeit
unterbrechbar.

### Wie Schritt 1 geprüft wurde

Nicht „sieht gut aus": Zeile für Zeile, mit maskierten Hex-Werten
verglichen. Wenn die Ersetzung irgendwo mehr angefasst hätte als eine
Farbe, wären die beiden Seiten auseinandergelaufen.

```sh
git diff -U0 -- <datei> | grep '^-' | sed -E 's/#[0-9a-fA-F]{6}/HEX/g'
git diff -U0 -- <datei> | grep '^+' | sed -E 's/#[0-9a-fA-F]{6}/HEX/g'
# müssen für alle 52 Dateien identisch sein
```

### Prüfung

```sh
python3 tools/colour_audit.py
```

---

## Vor jeder Änderung

- Neue Dateien **von Hand** in `CMakeLists.txt` eintragen
  (`CORE_SOURCES` / `MODEL_SOURCES` / `GUI_SOURCES`) — es wird nicht
  geglobbt.
- `./tools/syntax_check.sh <datei>…` laufen lassen.
- Bauen und Starten kann nur der Betreiber:
  `cd ~/Desktop/neureus/NereusSDR && ./build.sh && ./run.sh`
  Tests: `cd ~/Desktop/neureus/NereusSDR && ./tools/run_tests.sh`

## Die Grenze, die kein Design überschreibt

Der SWR-Durchlauf tastet den Sender. Nichts an der Oberfläche darf dazu
verleiten, außerhalb der Amateurbänder zu messen — auch nicht mit 1 W.
`BandPlanGuard` bleibt die Autorität. Wer den ganzen Bereich sehen will,
lädt eine `.s1p` vom VNA.

---

## Die Falle, die einen Monat Typografie unsichtbar gemacht hat

*Gefunden am 2026-08-18, gemeldet vom Betreiber an der Frequenzanzeige.*

Ein Qt-Stylesheet **kaskadiert auf jedes untergeordnete Widget** und
**gewinnt gegen `setFont()`**. In `AppletWidget`s Konstruktor stand:

```cpp
setStyleSheet("QLabel { color: …; font-size: 11px; }");
```

Damit war die gesamte Schriftleiter *innerhalb aller Applets* außer
Kraft. Sechs benannte Stufen, reduziert auf eine — und zwar lautlos:
kein Fehler, kein Test, nichts. Die Instrumente setzten ihre Größen
weiter korrekt per `setFont()`, und keine davon kam an.

Sichtbar wurde es erst dort, wo zwei Größen voneinander abhängen: die
Frequenzanzeige berechnet die feste Breite jeder Ziffer aus der Zelle
der 38-px-Schrift und zeichnete die Glyphe mit 11 px. Jedes
Ziffernschild dreieinhalbmal so breit wie sein Zeichen — die Zeile
zerfiel zu `7 . 1 3 1 . 3 0 0`. Überall sonst sah es lediglich
*etwas klein* aus, und daran gewöhnt sich das Auge.

### Die Regel, die daraus folgt

> **Farbe darf ins Stylesheet, Größe nicht.**

Farbe *soll* kaskadieren — ein Applet gibt seinen Kindern seinen
Textton. Größe soll erben, aber überschreibbar bleiben, und das kann
nur `setFont()`:

```cpp
// richtig — erbt an alle Kinder, ein Kind mit eigenem setFont() gewinnt
setFont([this] { QFont f = font(); f.setPixelSize(Style::kFontSmall); return f; }());
setStyleSheet("QLabel { color: …; }");
```

### Woran man es erkennt

Ein `font-size` in einem Stylesheet, das auf einem **Behälter** gesetzt
wird (`this`, ein Panel, ein Applet) statt auf einem einzelnen Widget.
Auf einem einzelnen Label ist es harmlos — dort ist es die Aussage über
genau dieses Label.

```sh
grep -rn "setStyleSheet" src | grep -v "->" | grep "font-size"
```

Am 2026-08-18 war `AppletWidget` die einzige Stelle. `InstrumentApplet`,
`GridCellWidget`, `ContainerWidget` und `InstrumentSpine` setzen ihre
Stylesheets auf einzelne Labels und sind in Ordnung.

### Was daran verallgemeinerbar ist

Die drei Abstandskonstanten in `FrequencyInstrument.cpp` waren nie
schuld — sie standen richtig da und rechneten mit der richtigen Zelle.
Der Fehler lag zwei Ebenen darüber, in einer Datei, die mit Frequenzen
nichts zu tun hat.

**Wenn eine Maßangabe nicht wirkt, ist die Ursache selten dort, wo sie
steht.** Bei Qt-Stylesheets ist die erste Frage nicht „stimmt der
Wert?", sondern „wer überschreibt ihn von oben?".
