# Kurve: weiche Kante und Hof — drei Blätter zur Wahl

**2026-10-08.** Zwei Zutaten der am 2026-09-17 gewählten Richtung
**„Glas & Tiefe"** (dort wörtlich: „Kurven mit Hof"), gebaut und
schaltbar. **Beide ab Werk aus** — diese Seite ist die Entscheidung.

Schalter: **Setup › Display › Render** → *Soft trace edge* / *Trace halo*.

## Was zu sehen ist

Dieselbe Szene, Bild für Bild gleich (fester Samen): Rauschboden
−135 dBm, S9-Träger, getastetes CW, SSB-Sprache, eine FT8-Gruppe.
Martins Anzeigestand (3D, Pan Average, WF Sample, Clarity an),
Linienbreite 1 px wie von ihm am 2026-08-26 entschieden.

Die Ausschnitte sind 1:1 in Gerätepixeln — dort entscheidet sich das,
nicht im ganzen Bild.

| Blatt | Beide Schalter | Ausschnitt |
| --- | --- | --- |
| [1 — heute](1-heute.png) | aus / aus | [Ausschnitt](1-heute-ausschnitt.png) |
| [2 — weiche Kante](2-weiche-kante.png) | **an** / aus | [Ausschnitt](2-weiche-kante-ausschnitt.png) |
| [3 — weiche Kante + Hof](3-weiche-kante-und-hof.png) | **an** / **an** | [Ausschnitt](3-weiche-kante-und-hof-ausschnitt.png) |

## Was jede Zutat tut

**Weiche Kante.** Die GPU zeichnet die Kurve als Band aus zwei
Scheitelpunkten je Bildspalte. An steilen Flanken ergibt das eine
Treppe, weil das Band harte Kanten hat. Jetzt wird der Kern einen halben
Gerätepixel schmaler gerechnet, und außen läuft je ein Streifen von
voller Deckkraft auf 0 aus. Die Linie wird dadurch **nicht breiter** —
außen steht sie weiter, aber mit Deckkraft 0. Kein neuer Shader.
Gilt auch für die Spitzenhaltelinie.

**Hof.** Ein schwaches Band um die lebende Kurve: 22 % an der Linie,
0 bei 2,5 px, darunter gezeichnet. Die 22 % sind geborgt, nicht gewählt
— derselbe Wert, mit dem `HAUSSTIL.md` §Weiche Übergänge die Füllung
unter der Kurve auslaufen lässt. Nur an der lebenden Kurve; zwei Höfe
übereinander (Kurve + Spitzenhaltelinie) wären Nebel.

## Mein Rat

**Blatt 2.** Die weiche Kante allein ist die ruhigere Verbesserung: die
Treppe verschwindet, sonst ändert sich nichts.

Der Hof macht das Band in diesem Rauschen **merklich dicker und
nebliger** — im Ausschnitt 3 sieht man es deutlich. Er würde in einem
ruhigen Band mit wenigen Signalen besser aussehen als in diesem. Das ist
eine Aussage über diese Szene, nicht über den Hof.

## Bewusste Lücke

Beides wirkt auf dem **GPU-Weg**. Der QPainter-Rückfall (greift nur,
wenn QRhi nicht aufsetzt) malt die Kurve als `QPen` — der hat seine
weiche Kante vom Antialiasing und kennt keinen Hof. Dort sehen die
Schalter nach nichts aus.

## Nachstellen

```
cmake --build build --target tst_rendering_werkbank
LONGPATH_GRAB_DIR=/pfad LONGPATH_RENDER_HIDDEN=1 \
  LONGPATH_RENDER_VARIANTS=k_kurve_heute,l_kurve_weich,m_kurve_weich_hof \
  ./build/tests/tst_rendering_werkbank
```

Geometrie: `src/gui/KurvenStreifen.h`, festgenagelt in
`tests/tst_kurven_streifen.cpp`.
