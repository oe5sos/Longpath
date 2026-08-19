# Der Baukasten: Kacheln statt festem Fenster

Antwort auf „jedes Fenster sollte man frei ändern können in der Größe.
beim Panadapter sehe ich keine Möglichkeit" und „so sollte der Baukasten
funktionieren" (Betreiber, 2026-08-19, mit Bildschirmaufnahme von **Zeus
Link** als Vorbild).

## Was das Vorbild tut

Aus der Aufnahme abgelesen (nicht aus dem Quelltext — Zeus Link ist
fremde Software, hier zählt nur, was der Betreiber sieht und will):

1. **Arbeitsflächen.** Linke Randleiste mit Symbolen, `+` legt eine neue
   an. Der Dialog *NEW WORKSPACE* bietet „Blank workspace", eine
   eingebaute Vorlage oder die Kopie eines gespeicherten Layouts, dazu
   Beschriftung und ein Symbol aus einer Palette.
2. **Kachel-Katalog.** Der Dialog *ADD PANEL* hat links Kategorien
   (ALL / SPECTRUM / VFO / METERS / DSP / LOG / TOOLS / AMPLIFIERS /
   TUNERS / CONTROLS / SWITCHES), oben eine Suche und darunter die
   Einträge mit Schlagwörtern: *Multi Panel* (tabs · tabbed · dock ·
   group), *S-Meter*, *QRZ Lookup*, *TDoA Geolocation*, *Azimuth Map*,
   *Rotator Compass*, *Rotator Dial* und weitere.
3. **Kacheln.** Jede hat einen Kopf mit Greifpunkt (⠿), Schloss und ✕,
   liegt in einem Raster und lässt sich an der Ecke in der Größe ziehen.
   Der **Panadapter ist eine Kachel unter mehreren** — im Bild liegen
   S-Meter und BAND darüber, FREQUENCY · VFO darunter.
4. **Reiter-Bündel.** Das *Multi Panel* nimmt mehrere Kacheln auf und
   zeigt sie als Reiter (in der Aufnahme: „PANADAPTER · WORL…" und
   „QRZ LOOKUP" im selben Rahmen).

## Was wir davon schon haben

Erst suchen, dann bauen. Das Ergebnis ist erfreulich:

| Baustein | Stand bei uns |
| --- | --- |
| Kachel-Rahmen mit Kopf, Andocken, Losreißen, Größe ziehen | **`ContainerWidget`** — genau das, plus Achsensperre |
| Eigenes Fenster für eine Kachel | **`FloatingContainer`** |
| Verwaltung, drei Andockarten, Persistenz | **`ContainerManager`** |
| Gespeicherte Layouts mit Namen | **`LayoutProfiles`** (`names()`, `save()`) |
| Panadapter-Anordnungen | **`PanadapterStack`** mit 9 Layouts + Pan-Menü |
| Inhalte für Kacheln | 12 Applets, 35+ Messgerät-Vorlagen, Rotor/Log-Panel, Antennenfenster, Spot-Hub |
| Panadapter in der Höhe verstellbar | **seit `ba6dacfa`** (View → Applets below panadapter) |

**Der Baukasten ist zu drei Vierteln gebaut.** Was fehlt, ist nicht die
Mechanik, sondern **eine Fläche, auf der beliebige Kacheln liegen
dürfen**, und **ein Katalog, aus dem man sie holt**.

## Was genau fehlt

1. **Freie Kachelfläche.** Heute ist die Aufteilung fest verdrahtet:
   ein `QSplitter` mit Panadapter und Applet-Leiste. Über dem Panadapter
   kann nichts liegen, und zwei Kacheln nebeneinander unter ihm auch
   nicht. Gebraucht wird ein Rasterbereich, in dem Kacheln nach Zeile
   und Spalte liegen und die Trennlinien ziehbar sind.
2. **Katalog-Dialog.** Ein „Kachel hinzufügen" mit Kategorien, Suche und
   Schlagwörtern. Die Inhalte gibt es alle schon — sie sind nur an
   festen Stellen verbaut statt aus einer Liste wählbar.
3. **Reiter-Bündel.** Mehrere Kacheln in einem Rahmen als Reiter.
   `ContainerWidget` kann das heute nicht.
4. **Arbeitsflächen als Bedienfläche.** `LayoutProfiles` speichert
   Layouts schon; es fehlt die Randleiste zum Umschalten und der Dialog
   „neu / aus Vorlage / als Kopie".

## Vorschlag: vier Schritte, jeder für sich brauchbar

Kein Umbau in einem Stück. Jeder Schritt lässt sich benutzen, bevor der
nächste kommt, und jeder ist einzeln zurücknehmbar.

**Schritt 1 — Kachelfläche unter dem Panadapter.** Aus dem heutigen
`QSplitter` wird ein senkrechter Splitter mit dem Panadapter oben und
einem Rasterbereich unten, in den `ContainerWidget`s wandern können.
Damit erfüllt sich der ursprüngliche Wunsch („darunter sollten andere
Widgets kommen") vollständig, und das Zielbild ist zur Hälfte da.
*Aufwand: mittel. Sichtbar: sofort.*

**Schritt 2 — Katalog.** Der Dialog mit Kategorien und Suche, der aus
einer Registratur der vorhandenen Inhalte schöpft. Braucht als
Vorarbeit eine Liste „was gibt es überhaupt" — die existiert heute nur
implizit, verteilt über `MainWindow`, `ContainerManager` und die
Applet-Sichtbarkeitssteuerung.
*Aufwand: mittel. Sichtbar: sofort.*

**Schritt 3 — Kachelfläche auch über dem Panadapter.** Erst wenn
Schritt 1 sich im Betrieb bewährt hat. Dann ist der Panadapter wirklich
„eine Kachel unter mehreren".
*Aufwand: klein, wenn Schritt 1 richtig gebaut ist.*

**Schritt 4 — Reiter-Bündel und Arbeitsflächen-Leiste.** Das Bequeme
zuletzt: mehrere Kacheln in einem Rahmen, und die Randleiste zum
Umschalten zwischen gespeicherten Anordnungen.
*Aufwand: mittel bis groß.*

## Was ich nicht ohne Zustimmung anfange

Schritt 1 berührt die Fensteraufteilung, an der jede Anzeige hängt, und
`m_spectrumFrac` (die Aufteilung Spektrum gegen Wasserfall) steckt in
vielen Rechnungen in `SpectrumWidget`. **Halb fertig ist hier schlechter
als nicht angefangen** — ein Betreiber mit zerschossenem Fenster kann
nicht mehr funken. Deshalb: Schritt 1 als eigene, zusammenhängende
Arbeit mit Prüfmatrix, nicht nebenbei.

Nicht übernommen wird die Kachel *TDoA Geolocation* aus dem Vorbild —
die setzt fremde Dienste und mehrere Empfänger voraus und ist ein
eigenes Vorhaben, kein Baukasten-Teil.

---

*Erhoben am 2026-08-19. Das Vorbild wurde aus der Bildschirmaufnahme des
Betreibers abgelesen; der Bestand durch Suche im Baum, nach der Regel,
die an zwei Tagen sieben Fehlalarme aufgedeckt hat.*

## Modification history (NereusSDR)

* 2026-08-19 — angelegt von Martin Fischer, KI-gestützt über Anthropic
  Claude (Cowork).
