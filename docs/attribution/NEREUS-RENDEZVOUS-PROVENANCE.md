# NereusSDR rendezvous — Longpath-Herkunftsnachweis

Dieses Blatt verzeichnet, was Longpath aus NereusSDRs Fernzugriffs-Arbeit
uebernommen hat, und in welcher Form.

Longpath **ist** ein Fork von NereusSDR (`boydsoftprez/NereusSDR`, © J.J.
Boyd, KG4VCF), siehe `CLAUDE.md`. Diese Uebernahme ist darum kein Port aus
einem fremden Projekt, sondern das Nachziehen eines Teilbaums aus unserem
eigenen Upstream — dieselbe Lizenz, dieselbe Codebasis. Das Blatt steht
hier trotzdem, weil der Teilbaum gross, eigenstaendig und **unveraendert**
ist: wer ihn aktualisieren will, soll auf einen Blick sehen, woher er kommt
und warum an ihm nichts angepasst wurde.

## Was uebernommen wurde

| Pfad | Stand | Form |
| --- | --- | --- |
| `rendezvous/` (273 Dateien: Dienst, WebSocket-Relay, Konformitaetssuite, Tests, Deploy-Rezepte) | `upstream/main` am 2026-10-28 | **unveraendert, Byte fuer Byte** |

Dazu gelesen, aber nicht kopiert (sie bleiben im Upstream und werden dort
gelesen):
`docs/architecture/2026-09-23-rendezvous-v1.md`,
`docs/architecture/2026-08-02-remote-station-identity-and-pairing-design.md`,
`docs/architecture/2026-09-23-station-link-v1.md` (Abschnitt 3.6).

## Warum unveraendert

Der Rendezvous ist ein **Protokoll**, kein Programmteil. Seine Transkripte
tragen feste Praefixe (`"NereusSDR rendezvous id v1\n"`,
`"NereusSDR rendezvous register v1\n"`), die in die Kennung jeder Station
eingerechnet sind. Wer sie umbenennt, spricht ein anderes Protokoll: nicht
mehr vertraeglich mit NereusSDRs Dienst, nicht mehr mit ihrer
Konformitaetssuite pruefbar, und jede spaetere Fassung muesste von Hand
nachgezogen werden.

Darum gilt dieser Teilbaum als **Fremdcode wie `third_party/`**: er wird
nicht umbenannt, nicht umformatiert und nicht an unseren Hausstil
angepasst. Die Longpath-Regel „ein neuer Verweis auf Nereus ist ein
Fehler" (`CLAUDE.md`) meint, was das Programm zeigt und nach aussen meldet
— nicht die Namen in einem uebernommenen Protokollbaum. Nach aussen zeigt
Longpath von alldem nur die Kopplungsnummer und die Wortmarke Longpath.

Unsere eigene Seite des Protokolls steht getrennt davon in
`handfunke/fernzugriff/` und ist Longpath-eigener Code.

## Wie aktualisiert wird

```
git fetch upstream
git checkout upstream/main -- rendezvous/
python3 -m pytest rendezvous/tests -q
python3 handfunke/fernzugriff/pruefe-kennung.py
```

Der zweite Lauf ist die Probe des Dienstes auf sich selbst, der dritte die
unserer Kennung gegen dieselben Vektoren. Schlaegt der dritte fehl, hat
sich das Protokoll geaendert: dann zuerst
`docs/architecture/2026-09-23-rendezvous-v1.md` im Upstream lesen,
Abschnitt 11 („Changing the rendezvous"), und erst danach Code anfassen.

## Lizenz

NereusSDR und Longpath stehen beide unter GPLv3 (Wurzel-`LICENSE`). Die
Copyright-Vermerke in den uebernommenen Dateien bleiben unveraendert
stehen; sie zu entfernen waere ein Lizenzverstoss, kein Stilfehler.
