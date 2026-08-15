# Roadmap — Design als eigene Schicht

> „Es werden immer Änderungen von Nereus kommen, die ich dann downloade
> und die sich dann automatisch meiner Farben und meinem Design anpassen
> sollen. **Technik Nereus, Design ich.**"
> — OE5SOS, 2026-08-15

Dieser Satz entscheidet die Architektur, nicht die Farbwahl.

---

## Warum der bisherige Weg nicht reicht

Heute steht das Design **im Quelltext**: `StyleConstants.h` ist eine
C++-Datei, und die 1737 Hex-Literale in 134 Widgets erst recht. Jede
Änderung daran ist eine Änderung an Nereus.

Das hat zwei Folgen, und die zweite ist die schlimmere:

1. **Jeder Download kollidiert.** `StyleConstants.h` ist genau die
   Datei, die ein Upstream-Commit auch anfasst. Merge-Konflikt bei jedem
   Update, für immer.
2. **Neue Widgets sind nicht angepasst.** Ein Panel, das mit dem
   nächsten Download kommt, bringt Nereus-Farben mit und weiß nichts
   von deiner Palette. Du müsstest es jedes Mal nachziehen — genau das,
   was du nicht willst.

Beides ist lösbar, aber nicht durch weiteres Umfärben. Es braucht eine
**Schicht**.

---

## Die Schicht

```
   ┌─────────────────────────────────────────────────┐
   │  ~/Library/…/NereusSDR/themes/oe5sos.json       │   ← DU
   │  Rollen → Werte. Kein C++. Nicht im Repo.       │
   └──────────────────────┬──────────────────────────┘
                          │  beim Start geladen
   ┌──────────────────────▼──────────────────────────┐
   │  Theme  ·  Style::themed()  ·  Polish-Filter    │   ← Brücke
   │  Ein Einhängepunkt in main()                    │
   └──────────────────────┬──────────────────────────┘
                          │
   ┌──────────────────────▼──────────────────────────┐
   │  src/  —  1737 Literale, 134 Widgets            │   ← NEREUS
   │  darf bleiben wie es ist, auch nach Downloads   │
   └─────────────────────────────────────────────────┘
```

Der Kern: **der Quelltext muss nichts wissen.** Er schreibt weiter
`#00b4d8` hin; die Theme-Datei sagt „wo Nereus `#00b4d8` malt, male
`#c2924f`". Ein Download bringt neue Widgets mit alten Nereus-Farben —
und die Schicht bildet sie ab, ohne dass jemand sie anfasst.

### Der eine Einhängepunkt

Qt hat dafür einen Haken, den man nur einmal setzen muss:

```cpp
// main.cpp — eine Zeile
app.installEventFilter(new Style::ThemeFilter(&app));
```

Der Filter horcht auf `QEvent::Polish`. Qt schickt das an **jedes**
Widget, kurz bevor es zum ersten Mal gezeichnet wird — auch an eines,
das erst mit dem nächsten Download in den Baum kommt. Der Filter liest
dessen Stylesheet, schickt es durch `themed()` und setzt es zurück.

Das ist die Antwort auf „soll sich automatisch anpassen": nicht 400
Aufrufstellen einwickeln, sondern einmal einhängen.

### Was der Haken nicht erreicht

Ehrlich, weil es die Grenze der Idee ist:

- **QPainter-Code.** S-Meter, Spektrum, Wasserfall, Charts malen mit
  `QColor(…)` direkt. Dafür gibt es keinen Haken; diese Stellen müssen
  `Style::…` benutzen. Rund 600 Vorkommen, und darunter sind die
  größten Farbflächen der App.
- **Widgets, die ihr Stylesheet später wechseln** (Zustandswechsel nach
  dem Polish). Die brauchen ein explizites `Style::themed(...)`.
- **Struktur.** Pillenform, Gruppenüberschriften, Panelköpfe, Laufweite
  — das ist Layout, keine Farbe, und keine Theme-Datei der Welt kann es
  nachrüsten.

---

## Phasen

### ✅ Phase 0 — Zählen statt schätzen · erledigt

`tools/colour_audit.py`. Ergab 1737 Literale, 276 Farben, 241 davon
namenlos. Die Schätzung vorher war „dreißig Dateien" und lag um das
Vierfache daneben.

### ✅ Phase 1 — Die ununterscheidbaren einsammeln · erledigt

221 Vorkommen in 52 Dateien auf Palettenwerte angeglichen (ΔE < 8).
**276 → 202 Farben.** Wert durch Wert, geprüft Zeile für Zeile mit
maskierten Hex-Werten.

### ✅ Phase 2 — Grautöne entblauen · erledigt

17 Werte in `StyleConstants.h`. Der wichtigste: `kBorder`
`#205070` → `#2c2c31`. Der umrandete jeden Knopf im Programm.

### ✅ Phase 3 — Bedeutungsfarben dämpfen · erledigt

24 Werte. Auswahl gedämpftes Blau, Messwert warm, Bestätigt Salbei,
Gefahr entsättigt. Türkis `#00b4d8` ist raus.

### ✅ Phase 4 — Theme als Datei · erledigt

`gui/styles/Theme.{h,cpp}`. Die Palette kommt aus einer JSON-Datei
außerhalb des Quellbaums:

```
~/Library/Application Support/NereusSDR/themes/*.json
```

Vorlage mit allen 46 Rollen: [`oe5sos.example.json`](oe5sos.example.json).
Kopieren, umbenennen, eine Zeile ändern, App neu starten.

```json
{ "name": "OE5SOS", "colors": { "border": "#2c2c31", "measured": "#c2924f" } }
```

Was nicht drinsteht, malt NereusSDR wie immer. Ein Schlüssel der Form
`#rrggbb` ersetzt eine Farbe, die noch keine Rolle hat — der Notausgang
für die 162, die es davon gibt.

Der wichtigste Test ist nicht, dass Laden geht, sondern
`abrokenFileChangesNothingAtAll`: eine Datei mit einem Tippfehler wird
komplett abgelehnt, mit einer Meldung, die den Fehler nennt, und das
laufende Theme bleibt unverändert. Halb übernommen wäre schlimmer als
gar nicht — dann sucht man den Fehler im Programm statt in der Datei.

### ✅ Phase 5 — Der Einhängepunkt · erledigt

`Style::ThemeFilter` auf `QEvent::Polish` und `QEvent::StyleChange`,
eine Zeile in `main.cpp`. Qt schickt Polish an jedes Widget, kurz bevor
es zum ersten Mal gezeichnet wird — auch an eines, das erst mit dem
nächsten Download in den Baum kommt.

Dass das terminiert, hängt an einer Eigenschaft, die vorher aus einem
anderen Grund gebaut wurde: `themed()` ist idempotent. Der Filter setzt
das Stylesheet, das löst StyleChange aus, der Filter läuft wieder — und
bricht ab, weil der zweite Durchlauf nichts mehr ändert.

`tst_theme_filter::aWidgetThatNeverHeardOfTheThemeGetsItAnyway` ist
wörtlich der Download-Fall.

Für Malcode ohne Stylesheet gibt es `Style::role()`:

```cpp
p.setPen(QColor(Style::role("measured", Style::kAmberText)));
```

### Phase 6 — Der Malcode

Die rund 600 `QColor`-Stellen in den zeichnenden Widgets auf
`Style::…` ziehen. Nach Sichtbarkeit: **Wasserfall** und **S-Meter**
zuerst — das sind die größten Farbflächen. Dann Spektrum, Charts,
Meter.

*Prüfbar:* `python3 tools/colour_audit.py` erfasst auch die
`QColor(0x.., 0x.., 0x..)`-Schreibweise, die es heute noch übersieht.

### Phase 7 — Die namenlosen 162

88 Farben mit ΔE 8–18 entscheiden, 74 mit ΔE > 18 benennen oder
streichen. Die auffälligsten zuerst: `#ffff00` reines Gelb (10×),
`#adff2f` Grüngelb (11×). In einer Oberfläche mit deinem Anspruch an
Dezenz haben die nichts verloren.

### Phase 8 — Struktur

Erst wenn Farbe steht, denn sonst kämpfen zwei Baustellen um dieselbe
Beurteilung. Gruppenüberschriften, Pillen mit Radius 6, Panelköpfe mit
Akzentbalken, Monospace für alle veränderlichen Ziffern, ein `…` pro
Gruppe statt dreizehn Knöpfen nebeneinander. Siehe
[`HAUSSTIL.md`](HAUSSTIL.md).

### Phase 9 — Downloadfest machen

Ein Skript, das nach einem Update sagt, was das Theme nicht erreicht:
neue Literale, neue `QColor`-Stellen, neue Widgets ohne Rollen. Damit
ist „Technik Nereus, Design ich" keine Absicht mehr, sondern eine
Prüfung, die durchläuft oder nicht.

---

## Die Reihenfolge, kurz

Phasen 4 und 5 sind der Kern — davor ist alles nur Umfärben, danach ist
es eine Schicht. Phase 6 bringt die größte sichtbare Wirkung, weil
Wasserfall und S-Meter mehr Fläche haben als alle Knöpfe zusammen.
Phase 8 kommt zuletzt, weil Struktur und Farbe sich sonst gegenseitig
die Beurteilung verderben.
