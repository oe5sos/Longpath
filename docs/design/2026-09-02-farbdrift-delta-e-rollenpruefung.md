# Farbdrift: Rollenprüfung der ΔE<8-Gruppe (2026-09-02)

## Ausgangslage

`tools/colour_audit.py --collapse` listet namenlose Farbwerte, die
perzeptuell (ΔE76 < 8) von einer bereits benannten `StyleConstants.h`-Farbe
nicht zu unterscheiden sind, und bezeichnet sie als "ersetzbar, keine
Entscheidung". Diese Behauptung wurde heute an allen 33 Gruppen (400
Fundstellen) einzeln geprüft — nicht nach Farbabstand, sondern danach,
ob die Farbe an jeder Fundstelle wirklich dieselbe UI-Rolle spielt wie
die vorgeschlagene Zielkonstante.

## Ergebnis

**7 von 400 Fundstellen (1,75 %) waren tatsächlich rollenkonsistent.**
Die übrigen 393 scheiterten an genau der Falle, die dieses Projekt
bereits einmal erlebt hat (siehe `tools/colour_audit.py`s eigene
`#205070`-Geschichte in seiner Kopfdokumentation): derselbe Hexwert
spielt an verschiedenen Stellen verschiedene Rollen, und ein zufälliger
Farbabstand zu einer benannten Konstante sagt darüber nichts aus.

### Methode

Für jede Gruppe (angeordnet nach Fundstellenzahl) las ein Agent jede
einzelne Fundstelle im echten Quellcode, bestimmte die tatsächliche
UI-Rolle (Hintergrund? Rahmen? Text? welches Widget?), verglich sie mit
der durch andere reale Vorkommen der Zielkonstante belegten Rolle, und
prüfte zusätzlich auf geschützten Kontext (portierte Thetis-Werte,
`ThemeQss.cpp`-Legacy-Tabelle, `Style::role("key", "#hex")`-Vorgabewerte).
Bei den 15 größten Gruppen (≥5 Fundstellen) lief zusätzlich eine
unabhängige, ausschließlich verschärfende Gegenprüfung — Grundhaltung:
im Zweifel unsicher, nie von unsicher auf sicher zurückstufen.

### Zwei Beispiele, die die Falle konkret zeigen

**`#203040` (81 Fundstellen) → vorgeschlagen `kButtonAltHover` (ΔE 7,9).**
Falsch: Ein Code-Kommentar in `FmApplet.cpp` benennt `#203040`
ausdrücklich als `kButtonHover`s eigenen historischen Wert;
`kButtonAltHover` gehört zu einer ganz anderen Zahl (`#204060`), die
nur zufällig farblich nah an `#203040` liegt. `StyleConstants.h`s
eigener "war #…"-Migrationskommentar bestätigt das. Die meisten der 81
Fundstellen sind zudem Rahmenfarben (border), nicht
Hover-Hintergrundflächen — eine zweite, unabhängige Rollen-Divergenz.

**`#8e8e93` (20 Fundstellen) → vorgeschlagen `kTextTertiary` (ΔE 1,0).**
Falsch: `#8e8e93` war laut `StyleConstants.h`s eigenem
Migrationskommentar ("2026-08-20 angehoben") der ALTE Wert von
`kTextSecondary`, nicht von `kTextTertiary` (dessen alter Wert war
`#76767a`). Ein noch lebender Kommentar in `VfoStyles.h:126`
(`// kTextSecondary`) bestätigt das direkt am Code. Die erste
Analysepass hatte 11 von 20 Stellen als sicher eingestuft; die
Gegenprüfung hat alle 11 wieder verworfen.

### Bekannter Schutzfall bestätigt: `ParametricEqWidget.cpp`

Sieben der kleinen Gruppen (8 Fundstellen insgesamt) liegen in
`src/gui/widgets/ParametricEqWidget.cpp` — exakt die Datei, die laut
`scripts/verify-style-drift.py`s eigener Dokumentation am 2026-08-18
schon einmal fälschlich umbenannt und zurückgenommen werden musste
(acht portierte Thetis-Werte). Alle acht wurden in dieser Prüfung
korrekt als geschützt erkannt und nicht angefasst.

## Was tatsächlich angewendet wurde (7 Fundstellen, alle einzeln
gegen den echten Code geprüft, gebaut und getestet)

| Datei | Zeile(n) | Farbe → Ziel |
| --- | --- | --- |
| `src/gui/applets/VaxApplet.cpp` | 140, 205, 232 | `#556070` → `kTextInactive` |
| `src/gui/applets/RadeApplet.cpp` | 58, 125 | `#708090` → `kTextScale` |
| `src/gui/KiwiWaterfallStripWidget.cpp` | 38 | `#000000` → `kInsetBg` |
| `src/gui/TitleBar.cpp` | 575 | `#a8a8ae` → `kTextSecondary` |

Bei `#708090` zeigt sich die Rollenprüfung auch innerhalb einer einzigen
Datei trennscharf: `RadeApplet.cpp:58/125` (Text) wurden als sicher
eingestuft, `RadeApplet.cpp:117/403` (Hintergrundfüllung derselben
Statusanzeige) korrekt als unsicher — dieselbe Farbe, zwei Rollen, in
derselben Datei.

## Konsequenz für künftige Arbeit an dieser Baustelle

`ΔE < 8` bleibt eine brauchbare Kandidatenliste zum Prüfen, ist aber
keine Freigabe zum Anwenden — auch nicht über `colour_audit.py --apply`.
Der verbleibende Rückstand (79 namenlose Farben gegen die alte Decke
von 63, siehe `docs/design/style-drift-baseline.json`) braucht dieselbe
Einzelfallprüfung wie hier, Gruppe für Gruppe — kein Sweep, egal wie
klein der gemessene Farbabstand aussieht.
