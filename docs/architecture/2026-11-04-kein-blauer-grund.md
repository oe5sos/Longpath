# Das alte Blau ist weg — an der Quelle

2026-11-04. Betreiber: „zum hundertsten mal der hintergrund blau". Und
am 2026-11-02: „ich will diesen blauen hintergrund überall weg, er
kommt immer wieder und es ist ärgerlich, dass ich das immer wieder
sagen muss."

## Warum es immer wiederkam

`#0f0f1a` ist der Grund der alten, blaustichigen Palette (HAUSSTIL.md).
Er stand roh in 27 Stylesheets — Hauptfenster, Einstellungsdialog,
Verbindungsmaske, Setup- und DSP-Seiten, Wisdom-Fortschritt,
AI-Issue-Dialog, Park-Info, `kPageStyle`, Schriftfarben auf
Akzentknöpfen — und als `QColor(0x0f, 0x0f, 0x1a)` siebenmal in den
Instrumenten (ItemGroup, MeterItem, SwrChartWidget).

`Style::themed()` (ThemeQss.cpp) schreibt solche Literale nur um, wenn
ein Thema **aktiv** ist: `if (!p.moves) return qss;`. Im Protokoll des
Betreibers steht „Theme: aus". Also galten die Literale bei ihm
unverändert — während eine Messung mit Thema (2026-11-01) „themed() ist
in Ordnung" ergab und die Quelle offen ließ. Jede Behebung „dort, wo er
es gerade sah" (zuletzt die Pillen des Kombi-Feldes, die Karten des
Auswählers) ließ die übrigen Stellen stehen.

## Die Behebung

Überall `#08080a` (`Style::kAppBg`), auch in den Instrumenten. Und ein
Prüfstand, der die Rückkehr verhindert: `tests/tst_kein_blauer_grund.cpp`
liest den ganzen Quellbaum und verbietet `#0f0f1a` in jedem String und
jeder QColor; Kommentare dürfen es nennen, damit die Geschichte lesbar
bleibt. Alte Fassung: 27 + 7 Stellen rot.

Nicht angefasst: die Konstanten `kGlassSel*` (die Mulde für Auswahl, seit
2026-11-02) und `kBlueBg`/`kBlueBorder` für das, was wirklich blau sein
darf (anfassbare Flächen nach HAUSSTIL „Blau = anfassbar").

## Nachtrag 2026-11-05: die zweite Quelle

Am Stand 71c76e2d2 war der Streifen rechts im Hauptfenster noch da
(gemessen #0e0e1b). Quelle: `ContainerWidget.h` hielt die Vorgabe als
Klammer-Initialisierung `{0x0f, 0x0f, 0x1a}`, die der Prüfstand nicht
suchte, und `serialize()` schrieb sie in jedes Profil (Feld 12,
`#ff0f0f1a`), `deserialize()` legte sie als Stylesheet auf den Inhalt.
Gleiches Muster in `MeterItem.h` (`SolidColourItem`).

Behoben: Vorgabe = `kAppBg`; beim Laden wird das alte Blau als
damalige Vorgabe erkannt und zu `kAppBg` (eine echte Betreiberfarbe
bleibt); der Prüfstand `tst_kein_blauer_grund` fängt auch die
Klammerform. Lehre: eine Farbe kann als Text, als Aufruf und als
Klammer-Initialisierung stehen; ein Scanner braucht alle drei.
