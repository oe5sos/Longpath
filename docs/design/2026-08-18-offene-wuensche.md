# Offene Wünsche vom 18. August, abends

Vom Betreiber im Lauf des Abends genannt, hier festgehalten statt in
einer Unterhaltung zu versickern. Noch **keine** Entwürfe — jeder Punkt
braucht zuerst eine Sichtung dessen, was es schon gibt.

## 1 · Panadapter von AetherSDR

**Gesagt:** „in der nacht den panadapter von aether klonen und einbauen,
eigenes fenster, mit unterschiedlichen hintergründen, frei wählbar"

**Befund vom selben Abend, in drei Teilen:**

| Teil | Stand |
| --- | --- |
| Eigenes Fenster | **Gibt es schon** — `PanFloatingWindow`, Phase 3F Sub-Epic D |
| Freie Hintergründe | **Heute Nacht portiert** — Bild + Deckkraft + Füllfarbe |
| Vollständiger Klon | **Offen, und kein Nachtprojekt** |

Zum dritten Punkt die Zahl, die ihn entscheidet: AetherSDRs
`SpectrumWidget` hat **19.600 Zeilen** (17.474 + 2.162), unserer 9.700.
Unser Panadapter stammt bereits aus derselben Architektur — GPU über
QRhi, Wasserfall als Ringpuffer, dieselbe Aufteilung. Ein „Klon" wäre
also kein Port, sondern ein Abgleich Merkmal für Merkmal: was kann
AetherSDR, was wir nicht können?

**Vorschlag:** eine Merkmalsliste beider Seiten, dann entscheidest du je
Zeile. Das ist ein halber Tag Lesearbeit und danach eine Liste, an der
man Wochen abarbeiten kann — statt eines Klons, der beides halb ist.

## 2 · Logbuch nach Zeus-Vorbild

**Gesagt:** „zeus hat auch ein tolles logbook"

**Was es bei uns gibt:** der Branch heißt `feature/rotor-logbook`, es
gibt `QsoDetailPane`, `AdifParser`, `DxccWorkedStatus`. Also Teile.

**Was zu klären ist, bevor irgendetwas gebaut wird:** Zeus' Logbuch ist
im Bild nicht zu sehen — es liegt hinter einem der Symbole der linken
Leiste. Ohne ein Bild davon würde ich raten. **Ein Foto genügt.**

## 3 · Zoomen wie Google Earth

**Gesagt:** „zoomen ähnlich wie google eather"

**Wie ich es verstehe:** stufenloses Zoomen um den Mauszeiger herum,
mit Schwung — nicht das heutige Rasten in Stufen. Der Panadapter zoomt
bereits über `visibleBinRange()` (Bin-Auswahl statt Neuberechnung), das
ist die richtige Grundlage: die Anzeige folgt sofort, die FFT wird erst
am Ende der Geste neu geplant.

**Was fehlt, ist die Geste, nicht die Technik.** Zu klären: zoomt es zum
Zeiger oder zur Mitte? Läuft es nach dem Loslassen aus? Gibt es eine
Grenze, ab der es einrastet?

## 4 · Aufnahme von QSOs und von sich selbst

**Gesagt:** „ein recordertool werden wir auch brauchen, wo ich alle
meine qso, aber auch mich selbst aufnehmen kann"

**Was es gibt:** `ClientPuduMonitor` nimmt bereits eine Ansage auf und
spielt sie zurück (Voice Check, 2026-08-11) — also der Weg
Mikrofon → Datei → Wiedergabe steht. Der Roadmap-Punkt **3M Recording**
(WAV, I/Q, zeitgesteuert) ist geplant, aber nicht begonnen.

**Zwei verschiedene Dinge in einem Satz**, und sie brauchen
verschiedene Antworten:

* **QSOs mitschneiden** — der Empfangston, dauerhaft, auffindbar.
  Fragen: immer oder auf Knopfdruck? Wie lange aufheben? Ein Eintrag je
  QSO oder ein durchlaufendes Band mit Marken?
* **Sich selbst aufnehmen** — die eigene Modulation, zum Anhören.
  Das ist der Voice Check, den es gibt; er bräuchte nur einen Platz,
  an dem die Aufnahmen bleiben.

---

## Was ich morgen ohne weitere Angaben tun kann

1. Die **Merkmalsliste Panadapter** (Punkt 1) — reine Lesearbeit an
   beiden Bäumen.
2. Die **Sichtung fürs Logbuch** (Punkt 2) — was von `QsoDetailPane`,
   `AdifParser` und `DxccWorkedStatus` schon trägt.

Alles andere braucht ein Bild oder eine Antwort.

---

**Nachtrag 2026-08-18, 23:xx:** Das Foto des Zeus-Logbuchs kommt erst in
den nächsten Tagen. Punkt 2 bleibt bis dahin unberührt — die Teile, die
es bei uns gibt (`QsoDetailPane`, `AdifParser`, `DxccWorkedStatus`),
werden gesichtet, aber nichts daran gebaut. Ein Logbuch nach Vermutung
zu entwerfen und es dann am Bild zu korrigieren kostet mehr als das
Warten.
