# Freies Raster: was trägt, was fehlt, in welchen Schritten

**Stand 2026-08-17, angenommen und begonnen am 2026-08-18.**

> **Festlegung des Betreibers, 2026-08-18, vor dem ersten Strich Code:**
>
> „Ein Feld im Raster ist ein **Behälter**, kein Widget. Es hat Ort und
> Größe — und eine **Liste** von Widgets, nicht eines. Das ist die
> Voraussetzung dafür, Stehwelle und S-Meter nebeneinander in ein
> Fenster zu legen, und es ist genau das Modell, das ContainerWidget +
> MeterWidget + ItemGroup schon abbilden: ein Behälter, viele Inhalte,
> in einem Zeichendurchgang.
>
> Baust du das Feld erst als Einzel-Widget und rüstest die Liste später
> nach, muss die gespeicherte Anordnung **zweimal wandern** — und der
> Kennungs-Fehler von heute Abend hat gezeigt, was eine
> Anordnungswanderung still verlieren kann."
>
> Damit beantwortet: Frage 1 aus §4 (Gitter oder freie Rechtecke) →
> **Gitter mit Zellen**, und die Zelle ist ein Behälter.

**Drei Anforderungen aus dem Zielbild**
(`~/Downloads/nereus-zielbild-gesamt.html`, 2026-08-17):

1. Der **Panadapter ist selbst ein Feld**, mit seinen Werkzeugen in der
   eigenen Kopfleiste (Zoom, Geschwindigkeit, Mute, TX, Schloss,
   Schließen). Die VFO-Flagge darüber fällt ersatzlos weg —
   **erledigt am 2026-08-18**, siehe §8. Das Feld selbst steht noch aus:
   der Panadapter hängt weiter im `QSplitter` neben der Spalte.
2. Der **Inhalt passt sich der Feldgröße an**, nicht nur der Rahmen.
3. Das **+** legt neue Felder an.

**Stand der Umsetzung**

| Schritt | Stand |
| --- | --- |
| 1 · Behälter, einspaltig | **fertig, 2026-08-18** — `GridCell`, `GridCellWidget`, `AppletGrid`; `AppletPanelWidget` benutzt sie. Sichtbar unverändert. |
| 2 · Spalten nach Breite + freies Setzen | offen — erst nach dem Flaggen-Umzug. Bringt die zweite der beiden Bewegungen, siehe §6a. |
| 3 · Spannweiten + Profil | offen |
| 4 · `Density` | offen |
| 5 · übrige Applets | offen |

---

**Ursprünglicher Vorschlag zum Lesen, nicht zum Ausführen.**
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

> **Entschieden 2026-08-18: Gitter.** Und die Zelle ist ein Behälter mit
> einer Liste, nicht ein Platz für ein Widget — siehe die Festlegung
> ganz oben. `GridCell` trägt `row/col/rowSpan/colSpan` und eine
> `QStringList applets`; `GridCellWidget` hält `QList<QWidget*>`.

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

**Schritt 1 — Der Behälter, einspaltig.**  ✅ *fertig 2026-08-18*
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


---

## 6 · Was Schritt 1 tatsächlich geworden ist (2026-08-18)

Drei neue Dateien unter `src/gui/applets/`:

| Datei | Rolle |
| --- | --- |
| `GridCell.{h,cpp}` | reine Anordnung: Id, Ort, Spannweite, **Liste** von Panelkennungen, Titel, Schloss. Kein Qt-Elternteil, prüfbar ohne Fenster, direkt ins Profil schreibbar. |
| `GridCellWidget.{h,cpp}` | das Feld auf dem Schirm: Kopfleiste (Griff · Titel · nachgestellt) plus **N** Inhalte. |
| `AppletGrid.{h,cpp}` | `QGridLayout` über den Feldern; `columns()`, `moveCell()`, `arrangement()` / `applyArrangement()`. |

`AppletPanelWidget` benutzt sie: aus `m_stackLayout` (ein `QVBoxLayout`
voller anonymer Hüllen) ist `m_grid` geworden, aus der Hülle ein Feld.
Sichtbar ändert sich nichts — solange es **eine** Spalte gibt, *ist* die
Zeile der Index.

**Entfernt, weil das Raster die Frage nicht mehr stellt:**
`appletPosForStackIndex()`. Es rechnete die Stelle in `m_applets` aus
dem Stapelindex nach, weil im Stapel auch Nicht-Applets lagen. Im Raster
ist die Zuordnung Feld → Inhalt eindeutig; die Folge kommt jetzt direkt
aus dem Raster, statt aus zwei Quellen zusammengerechnet zu werden.

**Umbenannt:** die Anordnung heißt `arrangement()`, nicht `layout()` —
letzteres gehört `QWidget` und meint etwas anderes. Zwei Bedeutungen für
denselben Namen an derselben Klasse sind genau die Sorte Verwechslung,
die in dieser Woche zweimal Zeit gekostet hat.

**Drei Tests mussten mit**, und alle drei aus demselben Grund: sie
prüften die **Bauart** statt der **Zusicherung** — sie suchten das
`QVBoxLayout` im Rollbereich und lasen dort einen Index ab. Sie prüfen
jetzt dieselbe Aussage über die öffentliche Antwort
(`appletPosition()`, `contentsMargins()` am Widget). Ein Test, der beim
Umbau rot wird, ohne dass sich am Bild etwas ändert, misst die falsche
Sache.

## 6a · Zwei Bewegungen, die nicht dasselbe sind

**Beobachtung des Betreibers, 2026-08-18:** ein abgelöstes Feld landet
auf dem Schreibtisch, nicht auf der Nereus-Fläche. Das ist richtig so —
und es ist der Anlass, den Unterschied hier festzuhalten, bevor ihn
jemand für einen Fehler hält.

| | **Ablösen** | **Freies Setzen** |
| --- | --- | --- |
| Wohin | eigenes Fenster des Fensterverwalters | innerhalb der Nereus-Fläche |
| Wofür | zweiter Bildschirm, danebenlegen | Anordnung im Hauptfenster |
| Träger | `AppletFloatingWindow` (`Qt::Window`) | `AppletGrid` / `GridCellWidget` |
| Geometrie | Bildschirmkennung + Rechteck, Profilfeld `floatingApplets` | Zelle + Spannweite, Profilfeld `cells` (Schritt 3) |
| Geste | **nur** über den Menüpunkt | Ziehen am Griff |
| Stand | gebaut | Schritt 2 |

Warum das Ablösen ausdrücklich **nicht** per Zug geht, steht in §2e: das
Umhängen über eine Top-Level-Grenze ist die AetherSDR-Absturzfamilie
(#2495, #4319, #4617). Ein Zug **innerhalb** des Rasters wechselt kein
Fenster und ist unbedenklich — er ist genau die Bewegung, die Schritt 2
bringt.

Sie zu verwechseln hätte zwei Folgen, die beide unangenehm sind: wer
„frei setzen" meint und „ablösen" bekommt, verliert sein Feld auf einem
Bildschirm, den er vielleicht gar nicht sieht; wer „ablösen" meint und
„frei setzen" bekommt, findet sein Feld nicht auf dem zweiten Monitor.

**Der Auswähler soll später beides anbieten**, benannt und getrennt —
etwa „Als Fenster ablösen" gegen „Auf der Fläche frei setzen". Heute
kennt er nur die erste Bewegung, weil es die zweite noch nicht gibt.

---

## 7 · Was Schritt 2 noch braucht

Die drei Anforderungen aus dem Zielbild sind noch offen und gehören
nicht in denselben Schritt:

* **Panadapter als Feld** — er hängt heute im `QSplitter` neben der
  Spalte, nicht im Raster. `GridCellWidget` nimmt ihn bereits an (es
  hält `QWidget*`, nicht `AppletWidget*`); was fehlt, ist die Auflösung
  des Splitters und die Werkzeugzeile in seiner Kopfleiste.
* **Inhalt nach Feldgröße** — das ist Schritt 4 (`Density`), und die
  Regel „eine ausdrückliche Wahl gewinnt" steht in §2b.
* **Das + legt Felder an** — heute legt `AddWidgetButton` Applets
  *sichtbar*, nicht Felder *an*. `AppletGrid::addCell()` steht bereit;
  die Frage, was in ein frisch angelegtes leeres Feld hineinkommt,
  gehört zum Auswähler und nicht zum Raster.


---

## 8 · Die VFO-Flagge ist weg (2026-08-18)

Punkt 1 des Zielbilds, erste Hälfte. Der Panadapter ist noch kein Feld,
aber der schwebende Kasten darüber ist es nicht mehr.

**Was umgezogen ist, mit eigenen Tests:**

| von der Flagge | wohin |
| --- | --- |
| Frequenz mit Rad und Klick | `FrequencyInstrument` |
| `parseUserFrequency` (29 Testfälle) | ebendort — **Fund**, siehe unten |
| Lautstärke, Stumm, Binaural | `RxApplet` |
| die sieben Rauschminderungen | `RxApplet` |
| NB/NB2, SNB, ANF, APF + Hz | `RxApplet` |
| die sieben NR-Schnellregler | `DspQuickPopups` — **Fund** |
| S-Meter-Balken | Zeigerinstrument als Applet |
| RADE-SNR | Kennung `MeterBinding::RadeSnr` |
| Scheibenfarben A–D | `SliceColors.h` — **Fund** |
| Antennen inkl. BYPS | `AntennaPopupBuilder` (war schon dort) |
| X/RIT, VAX, TX, Antenne als Anzeige | Pillen in der unteren Leiste |

**Drei Funde**, alle beim zeilenweisen Abgleich statt beim Bauen:

1. **Der Schnellregler.** Rechtsklick auf eine Rauschminderung öffnete
   auf der Flagge drei bis fünf Regler für *diese* Minderung; die
   Setup-Seite war darin nur der Verweis ganz unten. Der erste Umzug
   hatte den Verweis für das Ganze genommen. 28 der 30
   `SliceModel`-Setzer, die die Flagge bediente, sind diese Regler.
2. **`parseUserFrequency`.** 98 Zeilen, 29 Testfälle, schließt
   Fehlerbericht #73 (europäische Tausendertrennung, Dezimalpunkt ohne
   Einheit). `FrequencyInstrument::commitEdit` rief bis dahin ein
   blosses `toDouble()` mit der Annahme MHz — „7.230.000" wäre still
   verworfen worden, „7230" als 7230 MHz gelandet.
3. **`sliceColor`.** Die vier Scheibenfarben standen als statische
   Methode auf der Flagge und werden von der RxApplet geteilt.

**Was ersatzlos fällt, benannt statt verschwiegen:** die Schnellwahl
USB/CW/DIG (der volle Wähler bleibt), `recordToggled`/`playToggled`
(waren an nichts verdrahtet), und sechs Signale des
Mehrfach-Panadapters — Scheibe schließen, TX-Übergabe, Abtastrate,
Filterpolitik, Scheibe entfernen, Antennenwahl je Scheibe.

**Die offene Frage für Phase 3F**, hier vermerkt statt still gelassen:
*die Flagge war die Bedienfläche einer zweiten Scheibe.* Ohne sie
bedient die RxApplet immer die **aktive** Scheibe. Scheibe B braucht
eine eigene Fläche — im Zielbild ein eigenes Feld im Raster, und damit
genau das, wofür Schritt 3 die Spannweiten bringt.

Ebenfalls für 3F: `SpectrumWidget::sliceMarkerGeometry` lieferte eine
Marke **je Flagge**, mit Frequenz und Filterkanten aus dem Widget. Jetzt
liefert es die eine Marke des Panadapters. Die richtige Quelle ist
ohnehin `SliceModel`, nicht ein Widget — eine Marke, die aus der
Anzeige liest, kann nicht stimmen, wenn die Anzeige gerade nicht da ist.

**Rückgezogene Provenance mit Fundstelle:** vier Zeilen in
`THETIS-PROVENANCE.md` (VfoWidget ×2, VfoModeContainers ×2) und eine im
AetherSDR-Index. `verify-provenance-sync.py` kannte „zurückgezogen"
nicht und hätte sie als verwaiste Zeilen gemeldet; es kennt den Fall
jetzt — **und verlangt dabei einen Commit**: eine Rücknahme ohne
Fundstelle ist keine Rücknahme, sondern ein Verlust mit Fußnote.
