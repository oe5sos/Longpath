# Die Fußleiste — Entwurf vor dem Umbau

Betreiber-Entscheidung 2026-08-18: **erst ein Entwurf, dann bauen.**
Der Grund steht in der Entscheidung selbst — hier hängen zwei Flächen
zusammen. Was nach unten wandert, muss oben verschwinden, sonst hat
jede Bedienung zwei Orte. Genau daran ist an einem Tag schon zweimal
Zeit verlorengegangen (der doppelte `[PROC]`-Knopf, der doppelte
S-Meter).

## Der Unterschied, um den es geht

Zeus stellt unten **Bedienung** hin. Wir stellen **Zustand** hin.

| | Zeus Link | NereusSDR heute |
| --- | --- | --- |
| Schalter | MOX · VOX · TUNE · PS · Mute · CTUN · SPLIT · RIT · DUP · SA · ZOOM | — |
| Anzeigen | MIC-Pegel, dBFS, RADIO, Chat | CAT · TCI · CPU · PA · OVERLOAD · A/TX/LSB/2.9k/NR1/ANT1 |

Das ist keine halbe Arbeit auf unserer Seite. Es ist eine andere
Aufteilung, und sie hat einen Grund: die Bedienung liegt bei uns in den
Applets, wo sie zusammen mit ihren Reglern steht.

## Was ein Umbau tatsächlich kostet

Die Leiste ist nicht frei zusammensetzbar. `ChromeBarController` ist
seit dem Bottom-Banner-Umbau die **eine** Layout-Instanz: sie misst die
natürlichen Breiten einmal, und eine Faltleiter blendet bei Enge Sprossen
in fester Reihenfolge aus (`RxDashboard::badgeForRung`, Sprossen 5–9).
Jeder neue Schalter braucht darum drei Dinge, nicht eines:

1. eine **Sprosse** in der Faltleiter — also eine Antwort auf „was fällt
   zuerst weg, wenn das Fenster schmal wird?"
2. eine **reservierte Breite**, wenn er nie springen darf (die vier
   Sicherheitsanzeigen INH/PA/OVL/TX haben je 50 px fest, damit ein
   Alarm seine Nachbarn nicht verschiebt)
3. eine Entscheidung, **was in der Applet verschwindet** — sonst steht
   MOX zweimal da

## Drei Zuschnitte, von klein nach groß

### A — Nur die vier Sendeschalter

`MOX · VOX · TUNE · PS` wandern nach unten. Sie sind der Kern von Zeus'
Leiste, sie gehören zusammen, und sie sind die einzigen, die man im
Betrieb *ohne hinzusehen* treffen will.

In der TxApplet bleiben sie — aber als **Anzeige**, nicht als Schalter?
Nein: dann sind es wieder zwei Bedienorte. Sie verschwinden dort ganz;
die TxApplet behält Leistung, Mikrofon, Bandbreite und die Kette.

*Kosten:* vier Sprossen, vier reservierte Breiten. `PS` braucht eine
Zustandsfarbe (aus, aktiv, kalibrierend), die es unten noch nicht gibt.

### B — A plus die vier VFO-Schalter

Zusätzlich `CTUN · SPLIT · RIT · DUP`. Das ist die Hälfte von Zeus.

Diese vier sind heute **Anzeigen** in der unteren Leiste (RIT als Pille,
Rung 11/12). Sie zu Schaltern zu machen ist der kleinere Eingriff, weil
sie schon dort sind — aber es ändert ihre Natur: eine Pille, die auch ein
Schalter ist, muss anders aussehen als eine, die nur meldet.

*Kosten:* vier Sprossen mehr, plus eine Regel für „Pille, die schaltet"
gegen „Pille, die meldet". Ohne die Regel wird jede Anzeige irgendwann
versehentlich anklickbar.

### C — Zeus vollständig

Zusätzlich `Mute · SA · ZOOM`. Damit ist die untere Leiste die
Hauptbedienfläche und die Applets werden zu Einstellflächen.

*Kosten:* der ganze Zuschnitt der rechten Spalte ändert sich mit. Das ist
kein Leistenumbau mehr, das ist die Frage, wofür die Applets da sind.

## Was in jedem Fall bleibt

`CAT · TCI · CPU · PA · OVERLOAD` und die Slice-Kennung. Das sind
Zustände ohne Schalter, und Zeus hat für sie eigene Orte (die linke
Leiste, die Kopfzeile). Sie nach oben zu verschieben wäre ein zweiter
Umbau und gehört nicht in denselben Schritt.

## Empfehlung

**A zuerst, dann ansehen.** Vier Schalter sind genug, um zu beurteilen,
ob sich die Leiste richtig anfühlt — und wenig genug, dass ein
Rückbau eine Stunde kostet statt eines Tages. B und C sind danach
dieselbe Entscheidung noch einmal, mit Erfahrung statt mit Vermutung.

## Was vorher zu klären ist

1. **Verschwinden die Schalter in der Applet wirklich?** Meine Antwort
   ist ja — zwei Orte für dieselbe Handlung sind der Fehler, den wir
   heute zweimal aufgeräumt haben. Aber es ist deine Fläche.
2. **Was fällt zuerst weg, wenn es eng wird?** Die Faltleiter braucht
   eine Reihenfolge. Mein Vorschlag: `PS` vor `TUNE` vor `VOX` vor `MOX`
   — MOX zuletzt, weil ein Sender, den man nicht abschalten kann, das
   schlechteste Ende ist.
3. **Darf ein Schalter unten anders aussehen als in der Applet?** Wenn
   nein, brauchen beide dieselbe Knopfform; wenn ja, braucht die Leiste
   eine eigene.
