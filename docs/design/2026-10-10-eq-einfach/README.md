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

## Vorschlag (offen, Entscheidung des Betreibers)

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
