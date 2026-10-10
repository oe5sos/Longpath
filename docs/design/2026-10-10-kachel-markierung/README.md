# Die eingeschaltete Stufe markieren (2026-10-10)

> „wenn ich ein feld aktiviere sollte das feld oben als ganze markiert
>  sein, andere farbe. mache mir vorschläge um es gut ersichtlich zu
>  machen. blau, gelb, usw."

Bis heute wechselte in der Kettenreihe nur der **Rahmen** die Farbe:
ein Pixel, auf Schwarz, aus zwei Metern keine Aussage. `0-heute.png`
zeigt den Stand, um den es ging.

## Vier Entwürfe, je ein Blatt

| Blatt | Entwurf | Preis |
|---|---|---|
| `1-blaues-glas.png` | Ganze Fläche in der Auswahlfarbe des Hauses | keiner — **gewählt** |
| `2-blaue-kappe.png` | 3-px-Streifen oben, Kachel bleibt dunkel | Streifen und Arbeitsbalken reden beide von oben herunter |
| `3-bernstein.png` | Warm gefüllt | der Arbeitsbalken muss dunkel werden; Warm hiesse dann zweierlei |
| `4-linke-flanke.png` | 4-px-Balken links wie der Panelakzent | kostet Breite: aus „Compressor" wird „Compres…" |

Der Betreiber hat **1** gewählt.

## Was gebaut wurde

`gebaut-blaues-glas.png` — die wirkliche Anzeige, nicht die Skizze.

* **Eingeschaltet und die Kette läuft:** Fläche `kGlassSelTop →
  kGlassSelBot`, Rand `kGlassSelBorder`, Schrift `kGlassSelText`.
  Keine neue Farbe: Blau heisst in HAUSSTIL.md überall „angefasst,
  eingeschaltet", Warm heisst „gemessen".
* **Eingeschaltet, Kette aus:** dieselbe Markierung, geparkt —
  gemischt aus `kInsetBg` und der Auswahlfarbe, damit sie beim
  Themenwechsel mitgeht. Ein Hexwert wäre stehengeblieben.
* **Aus:** unverändert.
* **Der Arbeitsbalken bleibt bernstein**, nur heller (`#f0c060` statt
  `#d09020`) — auf der blauen Fläche verschwamm der alte Wert.
* **Der offene Reiter trägt einen weissen Ring** ausserhalb der
  Kachel. Zwei Fragen, zwei Zeichen: die Farbe sagt „läuft", der Ring
  sagt „das sehe ich gerade an". Der Ring folgt dem **Reiternamen**,
  nicht der Reiternummer — die Reiter werden umgebaut und die
  Voice-check-Seite kommt und geht.

## Prüfstand

`tests/tst_strip_row_marks_active_stages.cpp` misst nicht, ob eine
Farbe gesetzt wird, sondern was auf der Fläche ankommt. **Gegenprobe
gefahren:** mit der alten Füllung fallen zwei der drei Fälle durch
(`blueLead > 40` gegen `#1a1a1e`: Blau liegt dort vier Stufen über
Rot). Beim Bauen des Prüfstands selbst aufgefallen und mit
festgehalten: `QWidget::render()` malt den hellen Systemgrund in die
Fugen, wenn man ihm keine Palette gibt — die erste Ringsuche fand 100
helle Punkte, bevor es einen Ring gab.
