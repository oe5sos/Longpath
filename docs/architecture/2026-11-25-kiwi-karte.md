# Kiwi-Karte, fünf Kiwis, „Wo werde ich gehört“ (2026-11-25)

Betreiber (OE5SOS): *„man sieht im kiwi sdr window nur 3 kiwis … eine weltkarte
um zu sehen, wo welche kiwis verfügbar sind … bis zu 5 kiwis gleichzeitig
aufmachen, ein signal senden und dann sehen, wo man empfangen wird.“*
Gewählt: Entwurf A + B + C (Blätter in `~/Longpath/entwuerfe-kiwi-karte-2026-11-25/`).

## Was gebaut ist

| Stück | Datei | Prüfstand |
| --- | --- | --- |
| Liste im Applet wächst mit der Fensterhöhe (war hart 176 px = 3 Zeilen) | `KiwiSdrApplet.cpp` `updateListHeight()` | `tst_kiwi_applet_shows_receivers` (4 → 12 Zeilen; Gegenprobe gegen 176 px) |
| Weltkarte (QPainter, Natural Earth 110m, Public Domain) | `widgets/KiwiWorldMap.*`, `widgets/KiwiWorldCoast.cpp` (erzeugt von `tools/make_kiwi_coast.py`), Herkunft `docs/attribution/KIWI-WORLD-COAST.md` | `tst_kiwi_world_map` |
| Karte im Auswahldialog (frei / teilweise / voll / nicht erreichbar, Hover = Daten, Klick wählt die Tabellenzeile, geöffnete Kiwis mit Ring und Großkreislinie) | `KiwiPublicReceiverPicker.cpp` | `tst_kiwi_world_map` |
| Mithörer-Gruppe, höchstens fünf Kiwis insgesamt | `core/KiwiGroup.*` | `tst_kiwi_group` |
| Fenster „Wo werde ich gehört“: Karte (Ausschnitt um die eigenen Kiwis), Kacheln mit Mini-Wasserfall, Tabelle, Aufnahme je Kiwi | `KiwiHearingDialog.*`, `MainWindow_KiwiGroup.cpp`, Knopf „Heard …“ im Applet | `tst_kiwi_group`, `tst_kiwi_group_window` |

## Warum Mithörer keine Scheibe bekommen

Der Kiwi von „Mein Signal“ hängt an einer Scheibe und damit an
`kiwiControllableSlice`. Fünf Kiwis an fünf Scheiben hätten fünf VFOs, fünf
Töne in der Mischung und fünf Stellen für die Sicherheitsschranke bedeutet.
Ein Mithörer soll nur ein Bild liefern: `KiwiSdrManager::setProfileListener`
verbindet ihn **ohne Zuordnung und ohne Ton**, Frequenz und Ansicht kommen
über `primeProfileTracking` (Sendefrequenz der Sendescheibe, nur gelesen).
`tst_kiwi_group_window` prüft: keine neue Scheibe, keine Zuordnung, ein Kiwi mit
Scheibe wird nicht Mithörer.

## Höflichkeit gegenüber den öffentlichen Kiwis

Je Mithörer ein Kanal; das Verzeichnis nur auf Klick (Sitzungsspeicher wie
bisher); Mithörer werden nicht gespeichert und nicht beim Start verbunden; wer
`ext_api` nicht erlaubt, erscheint weder in der Liste noch auf der Karte
(Offline-Kiwis mit erlaubter Schnittstelle nur grau auf der Karte, nicht wählbar).

## Nicht geprüft

Live gegen echte öffentliche Kiwis (keine Verbindung ohne Wort des Betreibers):
ob der Server ohne Ton-Zuordnung wirklich Wasserfallzeilen auf die gesendete
Ansicht liefert, die Verzugsmessung je Kiwi, der Mitschnitt eines Mithörers
(`setDecodeAudioWhenInactive`). Die Sendefrequenz-Nachführung der Mithörer
läuft im Sekundentakt (der Client weist Gleiches selbst ab).

## Nachtrag 2026-11-25: getrennter Kiwi und Lautstärke

* Ist ein Kiwi weg (belegt, Fehler, getrennt), dunkeln beide Anzeigen ihr Bild ab
  und zeigen den Grund und den Knopf „Anderen Kiwi wählen“ (`KiwiLostOverlay`).
  Ein ersetzter Kiwi wird weggeräumt (`MainWindow::retireKiwi`), der Eintrag in der
  Liste bleibt. Von selbst wird nicht gewechselt (Entscheidung des Betreibers).
* „Server-Limit“ heißt: der Server hat eine stehende Verbindung mit `too_busy`
  beendet (Grenze des Kiwi-Betreibers für Programme). Longpath umgeht das nicht.
* Lautstärke je Kiwi: Regler in der Zeile der KIWISDR-Liste, −30 … +24 dB,
  gespeichert unter `KiwiVolumes`, oberhalb 0,8 weich begrenzt.
