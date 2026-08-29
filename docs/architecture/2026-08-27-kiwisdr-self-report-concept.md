# KiwiSDR-Selbsttest — Konzept

Status: **Variante B/C umgesetzt** (2026-08-27, siehe unten und
`2026-08-27-kiwisdr-design.md` §KIWI-WASSERFÄLLE-Panel). **Variante A**
(S-Meter-Testlauf über mehrere Kiwis) bleibt **Konzept, kein Auftrag** —
Kein Longpath-Port aus Thetis oder AetherSDR, eine eigene Idee. Bevor
daraus ein `*-plan.md` mit Aufgabenliste wird, müssen die offenen
Fragen am Ende dieses Dokuments entschieden sein (Feature-Umfang/
Architektur laut `CLAUDE.md`s Autonomiegrenzen).

## Die Idee

Anlass war die Frage: "wo komme ich bei welchem Kiwi an, und bekomme
ich eine Rückmeldung". Für CW/Digimodes beantwortet das bereits das
Reverse Beacon Network (viele RBN-Skimmer laufen selbst auf KiwiSDR-
Hardware) — Longpath hat das schon (`DxClusterClient`, Spot Hub). Für
SSB und "ganz allgemein, egal welcher Mode" gibt es kein automatisches
Decode-Netz, das das leisten könnte.

Die Idee für diese Lücke: eine **manuell ausgelöste Abfrage einer
kleinen, selbst gewählten Liste öffentlicher Kiwis**, während der
Betreiber sendet — jeder ausgewählte Kiwi wird kurz auf die eigene
TX-Frequenz gestellt, sein S-Meter-Wert während des Sendefensters
gelesen, danach wieder getrennt. Ergebnis: eine Tabelle "an diesem Ort
kam ich mit diesem Pegel an" über mehrere geografisch verteilte
Empfänger gleichzeitig — kein Sprachdecoder, aber ein objektiver
Pegelvergleich, den es heute für SSB so nicht gibt.

## Was schon da ist (Wiederverwendung, keine Neuerfindung)

Alle Bausteine existieren bereits als Nebenprodukt des normalen
Kiwi-Empfangs (siehe `2026-08-27-kiwisdr-design.md`):

| Baustein | Woher |
| --- | --- |
| Mehrere gleichzeitige Kiwi-Verbindungen | `KiwiSdrManager` verwaltet schon `QVector<KiwiSdrAntennaProfile>`, ein Worker-Thread für alle Clients |
| Öffentliche Empfänger mit Ort/Band | `KiwiPublicDirectory` liefert `grid`, `gps`, `bands`, `ext_api`-Policy je Empfänger |
| Auf eine Frequenz stellen, ohne eine Scheibe dauerhaft zu binden | `primeProfileTracking()` / `updateSliceTracking()` |
| Pegel lesen | `meterReadingReady(id, MeterReading)` — existiert bereits, wird heute nur nicht ausgewertet, weil kein Verbraucher zuhört |
| Wissen, wann gesendet wird | `TransmitModel::moxChanged` / `tuneChanged` — dieselben Signale, an denen schon die Sendesperre hängt |
| Höflichkeits-Grundsätze | `KiwiPublicDirectory`/`KiwiPublicReceiverPicker`: ehrlicher User-Agent, `ext_api`-Policy respektiert, keine Hintergrund-Polls |

**Es fehlt nur die Orchestrierung** — eine neue, kleine Koordinationsklasse
(Arbeitstitel `KiwiSelfCheckController`) und ein Dialog, kein neuer
Netzwerkcode, kein neues Protokoll.

## Skizze des Ablaufs

1. Betreiber öffnet **Radio → KiwiSDR → Selbsttest…** (neuer Menüpunkt).
2. Dialog zeigt eine kurze Liste — entweder die eigenen gespeicherten
   Profile, oder eine Handvoll aus dem öffentlichen Verzeichnis, vorab
   nach Band + `ext_api`-Erlaubnis gefiltert (derselbe Filter, den der
   bestehende Picker schon anwendet). Betreiber wählt per Checkbox aus
   — **kein Vorschlag, keine Automatik, die selbst eine Liste erweitert.**
3. Ein Klick auf **„Jetzt testen"** verbindet die ausgewählten Kiwis
   kurzzeitig, stellt jeden auf die aktuelle TX-Frequenz/Mode der
   sendenden Scheibe (`updateSliceTracking`-Pfad, aber ohne
   Scheiben-Zuordnung — reine Abfrage, kein Empfang für den Nutzer).
4. Sobald `moxChanged(true)` kommt, sammelt der Controller für die
   Dauer der Aussendung `meterReadingReady` je Kiwi.
5. Nach Sendeende: Tabelle mit Empfänger-Name/Ort/Entfernung/
   höchstem und mittlerem S-Meter-Wert. Danach werden alle
   Testverbindungen wieder getrennt — **keine bleibt offen.**

## Warum absichtlich NICHT automatisch/mächtiger

- **Kein Mengen-Abklappern des öffentlichen Verzeichnisses.** Immer
  eine vom Betreiber selbst gewählte, kleine Liste — passt zur
  bestehenden "ein Klick = ein Abruf"-Haltung und schont fremde,
  ehrenamtlich betriebene Empfänger.
- **Kein Speichern/Weiterverteilen der Ergebnisse.** Reine
  Momentaufnahme für den Betreiber selbst, kein eigenes
  Spot-Netzwerk, keine Veröffentlichung an Dritte (anders als PSK
  Reporter/RBN, die das bewusst und mit Zustimmung aller Beteiligten
  tun).
- **Kein Dauerbetrieb.** Ein Testlauf hat Anfang und Ende; nichts
  bleibt im Hintergrund verbunden oder pollt weiter.

## Offene Fragen (vor einem Umsetzungsplan zu klären)

1. **Woher kommt die Test-Liste standardmäßig** — nur aus den eigenen
   gespeicherten Profilen, oder darf der Dialog auch unverbundene
   öffentliche Empfänger direkt anzeigen (dann bräuchte er einen
   eigenen, kleineren Ausschnitt aus `KiwiPublicDirectory`, nicht den
   vollen Picker)?
2. **Wie lang darf ein Testlauf dauern** — reicht ein einzelner
   Sendezyklus (z. B. beim nächsten TUNE/PTT), oder soll der
   Betreiber eine feste Testdauer vorgeben (dann bräuchte es eine
   eigene, kurze Testaussendung statt eines echten QSOs)?
3. **Wie genau ist der S-Meter-Vergleich überhaupt** — Kiwis
   unterscheiden sich in Antenne, Vorverstärkung, Kalibrierung; ein
   roher dBm-Vergleich zwischen zwei fremden Empfängern ist mit
   Vorsicht zu lesen. Reicht eine Tabelle mit klarem Hinweis darauf,
   oder soll es von Anfang an nur eine relative Einordnung
   ("deutlich/leise/nicht gehört") statt absoluter dBm-Werte zeigen?
4. **Gehört das ins bestehende KiwiSDR-Menü oder eher in den
   Spot Hub** (Werkzeuge, wo RBN/PSK Reporter schon sitzen) — letzteres
   würde es begrifflich näher an die bestehende "wer hat mich gehört"-
   Funktionalität rücken, ist aber ein anderer Dialog-Rahmen.

Dieses Dokument ist Diskussionsgrundlage, kein Zeitplan — die nächste
Stufe wäre ein `*-plan.md` mit Aufgabenliste, erst nachdem die vier
Punkte oben entschieden sind.
