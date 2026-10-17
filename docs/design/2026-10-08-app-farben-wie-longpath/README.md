# App am Telefon: Farben und Hintergrund 1:1 wie Longpath

**2026-10-08.** Betreiber: *„ich würde gerne die grafik vom longpath
software 1:1 auf das app in bezug auf farben und hintergrund verwenden."*
Dann: *„erledige bitte 1:1, ggf. 2te variante als option."*

| Blatt | |
| --- | --- |
| [1 — heute](1-heute.jpg) | der Stand davor |
| [2 — wie Longpath](2-wie-longpath.jpg) | **gebaut, ab jetzt die Vorgabe** |

Dieselbe Attrappe, dieselbe Laufzeit, dasselbe Gerätemaß.

## Was tatsächlich abwich

Die Palette der App war schon fast vollständig 1:1 — Wert für Wert
gegen `src/gui/StyleConstants.h` verglichen:

| Rolle | Longpath | App | |
| --- | --- | --- | --- |
| Fläche · Panel · versenkt | `#08080a` `#0c0c0e` `#050507` | gleich | ✓ |
| Textleiter (6 Stufen) | `#dcdce1` … `#58585e` | gleich | ✓ |
| anfassbar · gemessen | `#4a7ba8` · `#d8a55f` | gleich | ✓ |
| Glasknopf · Auswahl | `#222227/#141417` · `#2f5f92/#1e3d5f` | gleich | ✓ |
| Kurve · Gitter · Skala | `#c2924f` `#8a8f96` `#9aa0a8` | gleich | ✓ |
| **Panadapter-Fläche** | **`#08080a`** (`kAppBg`) | `#141e27` | ✗ |
| **Wasserfall-Rampe, untere Hälfte** | Grau (`app-bg → panel → border → text-inactive`) | Blau | ✗ |

## Die Begründung im Quelltext stimmte nicht

In `handfunke/stil.css` stand: *„Longpath hat genau EINE blaue Fläche,
den Panadapter (#141e27)."*

Longpath hat **keine**. `SpectrumWidget.cpp` füllt Panadapter *und*
Wasserfall mit der Rolle `app-bg` (`#08080a`), und das blaue `#0f0f1a`
ist seit dem 2026-08-15 weg — es steht dort nur noch als `// war` im
Kommentar. Genau das war damals der Einwand des Betreibers („der
Spektrumbereich im Hauptfenster ist noch blaustichig"). Die blaue Fläche
gab es nur in dieser App.

## Ein Befund, der über die Farbe hinausgeht

In drei Durchgängen mit derselben Attrappe sind die Träger im Wasserfall
**mit Longpaths Rampe zu sehen** (warme Striche) und **mit der blauen
nicht** (gleichförmiges Blau). Der Quelltext begründete das Blau mit dem
Gegenteil: am Telefon sei im Grau alles verschwunden.

Drei Durchgänge an einer Attrappe sind kein Beweis, und **im Hellen ist
es nicht geprüft** — genau dafür wurde das Blau gebaut. Darum bleibt es.

## Die zweite Fassung als Option

Eine Zeile in `handfunke/app.js`:

```js
const WASSERFALL_BLAU = false;   // true = die blaue Fassung
```

Sie schaltet beides zugleich — die Rampe *und* (über
`data-wasserfall`) die Panadapter-Fläche im Stilblatt. Beide Fassungen
sind am laufenden Bild gegengeprüft.

**Kein Schalter in der Oberfläche.** Die App hat kein Einstellungsblatt
(das Kopplungsblatt lässt sich nicht absichtlich öffnen), und ein
Farbknopf gehört nicht in eine Fernbedienung fürs Funkgerät. Wenn du
draußen im Hellen vergleichen willst, sag es — dann bekommt sie einen
richtigen Schalter.
