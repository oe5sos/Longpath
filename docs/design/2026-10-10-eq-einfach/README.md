# Entzerrer vereinfachen (2026-10-10)

> „mit dem eq bin ich als anwender nicht zufrieden. wie können wir
>  dies verbessern, vereinfachen. das klicken der linie bewegt
>  teilweise die komplette kurve, was ich oft nicht will. als user
>  sollte das grundsätzlich einfach gestaltet werden."

## Erledigt (Commit „Entzerrer: ein Zug ändert eine Größe")

Die beiden Ursachen des Rutschens, nachgelesen statt geraten:

* Zehn Bänder, alle mit **Q = 0,707** (rund zwei Oktaven breit), alle
  mit Pegel 0 — also liegen alle zehn Griffe **auf der Nulllinie**.
  Die Linie ist die Griffreihe.
* Ein Zug änderte **Frequenz und Pegel zugleich**.

Jetzt: ziehen = Pegel, ⌥ = Frequenz, ⇧ = Breite; ein Klick allein
schaltet nichts mehr ein; der Zug rechnet relativ zum Griffpunkt.

## Gebaut (Commit „Entzerrer: zwei Ansichten, eine Seite")

`eq-einfach-echt.png` und `eq-profi-echt.png` — die wirkliche Seite,
aus dem Prüfstand gerendert.

**Einfach** ist der Standard. Sieben benannte Regler: WÄRME 300 ·
FÜLLE 450 · MULM 650 · NASAL 950 · KLARHEIT 1,4k · PRÄSENZ 2,5k ·
SCHÄRFE 3,8k. **Profi** ist ein Knopf daneben und zeigt die heutige
Seite unverändert.

Entscheidend ist, woran die sieben Regler hängen: an den **sieben
freien Griffen** des Bandsatzes (`EqBandLayout::kFirstSpare`…), nicht
an den Bändern, die ein Ausgangspunkt geformt hat. Folgen:

* Wer „Clean voice" gewählt hat, hört weiter Clean voice — die Regler
  legen sich darüber, statt es zu überschreiben.
* Die Regler stehen beim ersten Öffnen auf null. Das ist die ehrliche
  Aussage: das hier ist, was *du* dazugetan hast.
* Die Griffe sind in beiden Wegen dieselben — nach einem Profil
  genauso wie nach Zurücksetzen.

Unter jedem Namen steht die **wirkliche** Frequenz des Bandes, nicht
die, die der Name verspricht. In der einfachen Ansicht zeichnet die
Kurve nur die sieben Punkte, die sich auch ziehen lassen: ein Punkt,
der auf einen Zug nicht reagiert, ist schlimmer als kein Punkt.

`tests/tst_eq_two_views.cpp` hält die Zusage fest, die das Ganze erst
vertretbar macht: **Umschalten ändert den Ton nicht** — hin und
zurück, und kein Band hat sich bewegt.

## Nachtrag: die Breite (2026-10-10, am Gerät)

> „ich denke, dass die bereiche teilweise viel zu groß sind, die
>  geändert werden."

Stimmt, und es war eine geerbte Zahl. Die freien Griffe kommen mit
**Q = 0,707** auf die Welt (Butterworth, ClientEqs neutraler
Vorgabewert) — das sind rund **zwei Oktaven**. Wer bei 1,4 kHz
anhebt, hebt von 600 Hz bis 5 kHz an; als „Klarheit" ist das die
halbe Stimme.

Jetzt **Q = 2,0**, rund zwei Drittel einer Oktave. Die Griffe stehen
im Abstand von etwa 0,58 Oktaven, greifen also noch ineinander — bei
Q = 2,5 würden sie sich gerade berühren und die Summe bekäme Wellen.

Gesetzt wird das **beim Ziehen**, nicht beim Umschalten der Ansicht:
die Zusage „Umschalten ändert den Ton nicht" bleibt. Gleichzeitig
setzt der Regler seinen Griff auf **seine** Frequenz — am Gerät stand
NASAL auf 2 kHz, oberhalb von KLARHEIT auf 1,5 kHz, weil in der
Profi-Ansicht geschoben worden war. Für genau diesen Fall gibt es
**„Ordnen"**: ein Griff, alle sieben zurück auf Frequenz und Breite,
Pegel bleiben.

## Der erste Vorschlag (überholt)

`eq-einfach.png` — die Seite als **sechs benannte Regler** auf festen
Frequenzen, in der Sprache der Sache statt in Hertz:

| Regler | Frequenz | wofür |
|---|---|---|
| Rumpeln | 80 Hz | Trittschall, Wind, Tischpoltern |
| Mulm | 250 Hz | das Dumpfe, wenn man zu nah am Mikrofon sitzt |
| Körper | 500 Hz | Fülle der Stimme |
| Klarheit | 1,5 kHz | Verständlichkeit im Pile-up |
| Präsenz | 3 kHz | das „vorne" |
| Zischen | 7 kHz | S-Laute |

Die Frequenzen stehen fest — es gibt nichts, was verrutschen kann.
Die Punkte in der Kurve lassen sich **nur senkrecht** ziehen, der
gepunktete Strich sagt das. Darunter dieselben sechs als Fader mit
Mittenrasterung und Zahl.

Unten drei Ausgangspunkte (Glatt / Stimme hell / Stimme warm) und
**„Alles anzeigen …"** — dahinter steht die heutige Seite
unverändert: zehn Bänder, Filterarten, Referenzkurven, Glättung,
Peak Hold, Undo. Nichts geht verloren, es steht nur nicht mehr im Weg.
