# Der Wasserfall in AetherSDR — was er anders macht

Aufgenommen am 2026-08-15 gegen AetherSDR `31b29583`, verglichen mit
NereusSDR `feature/rotor-logbook`.

Grundlage für Aufgabe #100. Dies ist eine Bestandsaufnahme, kein Plan
zum Abschreiben: einiges davon löst Probleme, die NereusSDR nicht hat
(native FLEX-Kacheln, KiwiSDR), und das gehört nicht mit übernommen.

## Der eine Unterschied, aus dem die anderen folgen

NereusSDR hält den Wasserfall als **fertige Farbe**:

```cpp
QImage m_waterfall;      // Format_RGB32, Ringpuffer
int    m_wfWriteRow{0};
```

AetherSDR hält ihn als **Intensität** und färbt erst beim Zeichnen:

```cpp
WaterfallHistoryBuffer m_waterfallHistory;   // 8 Bit je Punkt
QVector<qint64>        m_wfHistoryTimestamps;
QVector<double>        m_wfHistoryRowCenterMhz;
QVector<double>        m_wfHistoryRowBwMhz;
```

Aus diesem einen Entschluss folgt fast alles Übrige.

Eine RGB32-Zeile ist ein Ergebnis. Man kann sie anzeigen und sonst
nichts: nicht umfärben, nicht umrechnen, nicht befragen. Eine
Intensitätszeile ist eine Messung, und eine Messung kann man später
noch einmal anders darstellen.

## Was das konkret ermöglicht

### 1. Palettenwechsel wirkt rückwirkend

In NereusSDR gilt ein neues Farbschema ab der nächsten Zeile. Was schon
im Bild steht, behält seine alten Farben — der Wasserfall bekommt einen
sichtbaren Streifen an der Stelle, wo umgeschaltet wurde.

AetherSDR färbt die sichtbaren Zeilen neu aus der Intensität. Der
Mechanismus ist ein Kennzeichen (`waterfallPaletteToken()`) aus Schema
plus Theme-Generation; stimmt es nicht mehr mit dem der sichtbaren
Zeilen überein, werden sie neu eingefärbt. Bemerkenswert ist, dass die
Theme-Generation mit im Kennzeichen steckt: ein Neuladen der
Theme-Datei schreibt die Farbverläufe aller Voreinstellungen um, und
das muss denselben Weg gehen wie ein Schemawechsel.

Für uns unmittelbar einschlägig — wir haben gerade `Gedämpft`
eingeführt und die Theme-Datei erreicht die Farbverläufe.

### 2. Speicher: 20 Minuten statt eines Bildschirms

```cpp
static constexpr qint64 kWaterfallHistoryMs = 20LL * 60LL * 1000LL;
static constexpr int kRowsPerChunk = 256;   // Blöcke, nicht am Stück
```

Ein Byte je Punkt statt vier, und der Speicher entsteht blockweise,
sobald die Historie ihn erreicht — nicht im Voraus. Bei 2000 Punkten
Breite kostet ein Block 512 kB; wer die App zwei Minuten laufen lässt,
zahlt nicht für zwanzig.

### 3. Zeitachse und Zurückscrollen

Weil jede Zeile einen Zeitstempel hat (`m_wfHistoryTimestamps`), kann
die Zeitachse beschriftet werden und man kann zurückziehen —
`m_draggingTimeScale`, `m_wfHistoryOffsetRows`, `m_wfLive`. Ohne
Zeitstempel ist beides nicht zu haben: eine Zeile im RGB-Ring weiß
nicht, wann sie entstanden ist.

### 4. Der Frequenzrahmen je Zeile

Das ist der Teil, den man erst versteht, wenn man ihn vermisst.

Jede Zeile merkt sich, bei welcher Mittenfrequenz und Bandbreite sie
aufgenommen wurde. Beim Zeichnen rechnet der Shader jede Zeile einzeln
in den aktuellen Ausschnitt um:

```glsl
float sourceU = 0.5 + (targetCenterOffsetMhz - rowFrame.x) / rowFrame.y
    + (v_uv.x - 0.5) * targetBandwidthMhz / rowFrame.y;
```

Ohne das gilt der Ausschnitt von jetzt für Zeilen von vorhin. Wer
während des Zuschauens das Band wechselt oder hineinzoomt, bekommt eine
Historie, die verschoben und gestreckt daneben liegt — sie sieht
richtig aus und ist falsch. Genau das tut NereusSDR heute.

Der Rahmen wird an zwei Stellen geführt: `m_wfHistoryRow*` für die
gespeicherte Historie und `m_wfVisibleRow*` für den Ring, den die GPU
sieht. Die zweite Fassung ist eine Textur (`rowFrequencyFrame`), die
der Shader Zeile für Zeile abfragt.

### 5. Weiches Scrollen

Eine empfangene Zeile erscheint sofort und wandert dann über die
beobachtete Zeilendauer um genau eine Zeile weiter. Ein `QTimer` mit
`Qt::PreciseTimer` treibt das, `m_waterfallScrollDistanceRows` sagt
wie weit.

Der Shader rekonstruiert dabei mit einem kubischen B-Spline über vier
benachbarte Zeilen:

```glsl
float w0 = (1.0 - 3.0*f + 3.0*f2 - f3) / 6.0;
float w1 = (4.0        - 6.0*f2 + 3.0*f3) / 6.0;
float w2 = (1.0 + 3.0*f + 3.0*f2 - 3.0*f3) / 6.0;
float w3 = f3 / 6.0;
```

Der Kommentar daneben nennt den Grund für die vier getrennten
Abfragen: jede der vier Zeilen wird durch **ihren eigenen**
Frequenzrahmen geholt. Fasste man sie zusammen, wechselte die Schärfe
im Takt der Zeilen — „alternating sharp/blurred".

Zwei Randfälle stehen ausdrücklich im Code, und beide sind Fehler, die
jemand erst im Betrieb gesehen hat:

- Die unterste Bildzeile wird abgeschnitten, sonst spiegelt der
  Vier-Zeilen-Kern die neueste Zeile als ein Pixel breites Echo an den
  unteren Rand.
- Das Alter wird vor dem Anwenden des Schreibzeigers begrenzt, sonst
  greift der Kern an den Enden der Historie auf das andere Ende des
  Rings.

## Was ich NICHT übernehmen würde

- **`m_waterfallSupplemental`** und die zweite Textur. Sie fangen
  native FLEX-Kacheln auf, die breiter sind als der Ausschnitt. Wir
  haben keine.
- **`WaterfallRate` mit der Flex-Kurve.** Die gemessene, stark
  nichtlineare Kennlinie gilt für die Anzeigemaschine im Flex-Gerät.
  Unser Protokoll-2-Gerät liefert rohe Spektren, für uns ist der
  lineare Zweig richtig — und der ist eine Handvoll Zeilen.
- **Der Vorschau-/Resize-Apparat.** `m_resizeBufferSettleTimer` und
  die fünf Zähler daneben lösen ein Ruckeln beim Fensterziehen, das
  wir erst haben werden, wenn der Rest steht.

## Reihenfolge für Aufgabe #100

Vier Schritte, jeder für sich lauffähig und prüfbar:

1. **`WaterfallHistoryBuffer` portieren.** 8-Bit-Intensität in
   256-Zeilen-Blöcken, das Einfärben wandert ans Ende der Kette.
   Sofort sichtbarer Gewinn: der Palettenwechsel wirkt rückwirkend,
   und der Streifen beim Umschalten auf `Gedämpft` verschwindet.
   Prüfbar ohne Oberfläche.
2. **Zeitstempel je Zeile**, dann Zeitachse und Ziehen. Baut auf 1
   auf, weil ein Zeitstempel neben einer Intensitätszeile liegt.
3. **Frequenzrahmen je Zeile.** Der Schritt, der die Historie ehrlich
   macht. Zuerst auf der CPU beim Umzeichnen — der Shader kann später
   folgen, das Ergebnis ist dasselbe, nur langsamer.
4. **Weiches Scrollen** mit dem B-Spline-Kern. Reine Optik, deshalb
   zuletzt: es macht ein richtiges Bild schöner, aber ein falsches
   nicht richtiger.

Die Schritte 1 bis 3 sind ohne GPU zu haben. NereusSDR malt den
Wasserfall heute auf der CPU, und die drei ersten Schritte ändern
daran nichts — sie ändern, was gespeichert wird, nicht wer es malt.

## Quellen

- `AetherSDR/src/gui/WaterfallHistoryBuffer.{h,cpp}`
- `AetherSDR/src/gui/SpectrumWidget.{h,cpp}` — `paintWaterfallRows­FromHistory`,
  `waterfallPaletteToken`, `rebuildWaterfallViewport`
- `AetherSDR/resources/shaders/texturedquad_rowframes.frag`
- `AetherSDR/src/core/WaterfallRate.h`
- `AetherSDR/tests/waterfall_history_buffer_test.cpp`,
  `tests/waterfall_rate_test.cpp`
