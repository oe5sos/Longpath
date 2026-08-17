# Freies Raster: was trägt, was fehlt, in welchen Schritten

**Stand 2026-08-17. Vorschlag zum Lesen, nicht zum Ausführen.**
Auftrag des Betreibers: „Punkt 4 (freies Raster, Inhalt passt sich der
Größe an) heute nicht anfangen — stattdessen zum Abschluss einen
Vorschlag dazu ausarbeiten und ablegen, den ich morgen lese."

Das Ziel, wie ich es verstanden habe: Widgets liegen nicht mehr in
einer festen Spalte untereinander, sondern auf einer freien Fläche —
und ihr **Inhalt** richtet sich nach der Größe, die sie dort bekommen,
statt nur beschnitten oder gestreckt zu werden.

---

## 1 · Was die vorhandene Schicht schon trägt

Mehr als man denkt. Vier der fünf Bausteine sind da.

### Anordnung und Persistenz

`ContainerManager` kennt drei Andockarten (`PanelDocked`, `OverlayDocked`,
`Floating`) und kann zwischen ihnen wechseln. **`OverlayDocked` ist
bereits ein freies Raster**: der Container liegt frei über dem
Panadapter, `ContainerWidget::updateDrag` verschiebt ihn geklemmt,
`endDrag` und `endResize` schreiben Ort und Größe in
`m_dockedLocation` / `m_dockedSize`, und `serialize()` legt beides in
den Feldern 2–5 ab. Achsensperre gibt es auch schon
(`updateDockedPositions`).

Das heißt: *eine* Ebene des Programms kann schon, was Punkt 4 will —
nur benutzt sie heute niemand für die Applets.

### Die Fläche selbst

`QSplitter` teilt Spektrum und Panel; `ChromeBarController` ist die
eine Layoutautorität für die untere Leiste. Für ein freies Raster
fehlt eine dritte Fläche nicht — die Applet-Spalte *ist* sie, sie ist
nur einspaltig ausgelegt.

### Ablösen und Geometrie im Profil

Seit heute: `AppletFloatingWindow`, `ensureOnVisibleScreen`, und im
Profil `floatingApplets` mit Rechteck, Bildschirmkennung und
Stapelstelle je Applet. Das Muster „das Profil führt, das Fenster
meldet" steht und ist erprobt.

### Inhalt, der sich nach der Größe richtet

Vereinzelt vorhanden, und genau das ist der interessante Teil:

* `AppletPanelWidget::resizeEvent` skaliert die Kopfhöhe über ein
  Seitenverhältnis und klemmt sie auf 40 % der Gesamthöhe.
* `SpectrumWidget` rechnet seine gesamte Geometrie aus der Fenstergröße
  und hat seit heute einen Sammeltimer dafür.
* Die neuen Instrumente rechnen Bogenmaße als **Verhältnisse** der
  Fläche (`kRadiusOfWidth`, `kTroughOfRadius`, `kPivotFromBottom`), nicht
  in absoluten Zahlen — sie wachsen also schon mit.
* `FrequencyInstrument::sizeHint` gibt je Variante eine andere Höhe
  zurück.

---

## 2 · Was fehlt

### a) Ein Rasterbehälter für Applets

`AppletPanelWidget` ist ein `QVBoxLayout` in einem `QScrollArea`. Es
kennt Reihenfolge (`moveApplet`, `setAppletOrder`), aber keine Spalten
und keine Zellen. Für ein freies Raster braucht es einen Behälter, der
je Applet **Zelle und Spannweite** kennt statt nur einen Index.

Zwei gangbare Formen:

* **Gitter mit fester Spaltenzahl** (2–4 je nach Breite), Applets
  belegen 1×1, 2×1, 1×2 … Berechenbar, testbar, und der Umbruch bei
  schmaler Fläche ist eine Regel statt einer Handbewegung.
* **Freie Rechtecke wie bei `OverlayDocked`**, mit Fangraster. Näher am
  „frei" des Auftrags, aber jede Fläche muss dann von Hand aufgeräumt
  werden, und beim Verkleinern des Fensters überlappen sich Dinge.

**Ich würde das Gitter vorschlagen**, mit Spannweiten. Es hält die
Zusicherung „nichts überlappt, nichts fällt heraus" ohne dass der
Bediener sie einhalten muss.

### b) Größenstufen statt Skalierung

Heute skaliert Inhalt (Kopfhöhe, Bogenradius). Was fehlt, ist die
Entscheidung, bei welcher Größe ein Widget **etwas anderes** zeigt —
nicht dasselbe kleiner. Beispiele aus dem, was schon dasteht:

| Widget | klein | mittel | groß |
|---|---|---|---|
| Zeiger/Balken | nur Wert + Einheit | Fußzeile dazu | volle Teilung, Nachlaufzeiger |
| Frequenz | Variante C (nur Zahl) | A (Bandstreifen) | B (flacher Bogen) |
| RX/TX-Applets | Kernregler | + zweite Reihe | volle Gruppen |

Das ist keine neue Technik — `FrequencyInstrument` hat die drei
Varianten schon, und `InstrumentApplet` hält beide Formen in einem
Stapel. **Was fehlt, ist die Regel, wer umschaltet und wann**, und die
Antwort darauf, ob die Größenstufe die Wahl des Bedieners überschreiben
darf. Meine Empfehlung: nein — eine ausdrückliche Wahl gewinnt, die
Automatik greift nur, solange keine getroffen wurde. Sonst springt das
Fenster beim Ziehen und der Bediener verliert seine Einstellung.

### c) Eine Stelle, die die Größenstufe bestimmt

Heute fragt jedes Widget selbst (`resizeEvent`). Bei einem Raster mit
Zellen muss die Stufe **vom Behälter** kommen, sonst kennt ein Widget
nur seine Pixel und nicht seine Rolle. Eine schmale Schnittstelle
genügt:

```cpp
enum class Density { Compact, Normal, Full };
virtual void setDensity(Density d);   // auf AppletWidget
```

Der Behälter rechnet die Stufe aus Zellgröße und Spannweite und drückt
sie hinein. Ein Widget, das die Stufe nicht überschreibt, bleibt wie
es ist — der Umbau bricht also nichts.

### d) Persistenz

`AppletStackOrder` ist eine Liste von Kennungen. Ein Raster braucht je
Applet Zelle, Spannweite und (wenn (b) kommt) die ausdrücklich gewählte
Stufe. Das gehört ins Profil, neben `floatingApplets` — dieselbe Regel,
dasselbe Schreibmuster (Ende der Geste in den Speicher, Beenden auf die
Platte).

### e) Die Geste

Ziehen im Raster ist **nicht** dasselbe wie das heutige Umsortieren.
Und hier gilt die Lehre vom Vormittag: AetherSDR hat das Ablösen per
Zug wegen der Reparent-Abstürze (#2495, #4319, #4617) auf einen
einzigen bewussten Pfad gelegt. Ein Zug INNERHALB des Rasters wechselt
kein Top-Level-Fenster und ist unbedenklich; ein Zug, der aus dem
Raster hinausführt, wäre wieder derselbe Fall. **Der Rand des Rasters
darf keine Ablösegeste tragen.**

---

## 3 · Vorschlag in Schritten

Jeder Schritt ist für sich lauffähig und für sich zu beurteilen.

**Schritt 1 — Der Behälter, einspaltig.**
`AppletGrid` neben `AppletPanelWidget`, zunächst mit einer Spalte und
ohne Spannweiten. Sichtbar ändert sich nichts; die Applets liegen nur
in Zellen statt in einer Box. Damit ist die Umstellung von Reihenfolge
auf Zellen erledigt, bevor irgendetwas Sichtbares davon abhängt.
*Prüfbar:* dieselbe Reihenfolge wie vorher, dasselbe Bild.

**Schritt 2 — Spalten nach Breite.**
Zwei Spalten ab einer Breite, drei ab einer weiteren. Die Schwellen
gehören dir. Applets belegen weiter 1×1.
*Prüfbar:* nichts überlappt, nichts fällt heraus, der Umbruch ist
berechenbar.

**Schritt 3 — Spannweiten und Zellen im Profil.**
Ein Applet darf zwei Spalten belegen. Zelle und Spannweite ins Profil,
Schreiben am Ende der Geste.
*Prüfbar:* Anordnung überlebt Neustart und Profilwechsel.

**Schritt 4 — `Density` auf `AppletWidget`.**
Schnittstelle plus Regel im Behälter, aber zunächst nur für die drei
neuen Instrumente — sie haben ihre Stufen schon.
*Prüfbar:* dieselbe Zelle in drei Größen zeigt drei Fassungen, und
eine ausdrückliche Wahl bleibt stehen.

**Schritt 5 — Die übrigen Applets nachziehen.**
Zwölf Stück, einzeln, nach Bedarf. Wer nichts überschreibt, bleibt wie
er ist.

---

## 4 · Was vorher zu entscheiden ist

Drei Fragen, die die Schritte 2 bis 4 bestimmen, und die keine
Codefrage sind:

1. **Gitter oder freie Rechtecke?** Meine Empfehlung steht oben.
2. **Bei welchen Breiten bricht es um?** Zwei Zahlen genügen.
3. **Darf die Automatik eine ausdrückliche Wahl überschreiben?** Meine
   Empfehlung: nein.

---

## 5 · Was heute schon dagegen spricht, es zu überstürzen

Der Panelaufbau trägt gerade drei Dinge gleichzeitig, die noch nicht
fertig sind: das Entdoppeln des S-Meters, das Ausblenden der
VFO-Flagge und den Umschalter im Panelkopf. Ein Rasterumbau darüber
hinweg würde alle drei mit anfassen. Die Reihenfolge, die du für heute
gesetzt hast, hält sie auseinander — und Schritt 1 oben ist bewusst so
geschnitten, dass er erst danach kommt, ohne dass etwas davon abhängt.
