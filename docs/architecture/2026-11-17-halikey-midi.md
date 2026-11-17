# HaliKey MIDI: Fußtaster und Morsetaste

Stand 2026-11-17. Betreiber: „Halikey Midi sollte funktionieren, bei Aether
funktioniert das bereits, informiere dich und baue!" — und mitten im Bau:
„Ich benötige HaliKey für den Fußtaster!!!" Damit ist der Fußtaster die
Hauptsache und die Morsetaste die Zugabe.

## Das Gerät

HaliKey MIDI (Halibut Electronics): ein USB-MIDI-Gerät, jeder Kontakt kommt
als Note On / Note Off. Laut Handbuch:

| Note | Kontakt |
| --- | --- |
| 20 | Spitze der Tastenbuchse — linkes Paddle (Punkt), auch eine Handtaste |
| 21 | Ring der Tastenbuchse — rechtes Paddle (Strich) |
| 30 | Handtaste |
| 31 | PTT-Eingang |

Der Kanal steht nicht im Handbuch und wird nicht ausgewertet. Die vier
Noten sind einstellbar.

## Die Kette

```
MIDI-Note ─► MidiKeyInput ─┬─► PTT ───────────────► CwTastung ─► CwTastziel ─► MOX
                           └─► IambicKeyer ─► Kante ─► CwTastung ─► CwTastziel ─► cw key
                                                │
                                                └─► CwMithoerton (lokal, sofort)
```

| Klasse | Aufgabe |
| --- | --- |
| `MidiKeyInput` (`src/core/midi/`) | RtMidi-Eingang. `deute()` (Botschaft → Ereignis) und `waehlePort()` (nie „der erste Port") sind reine Funktionen. Schnelle Senken auf dem MIDI-Faden, dazu Qt-Signale. |
| `IambicKeyer` (`src/core/cw/`) | Jambus-Geber A/B, ganzer Port aus AetherSDR [@d58e2b8a], eigener Faden, Zeitraster. |
| `CwMithoerton` (`src/core/cw/`) | Lokaler Sinus mit 5-ms-Kosinusflanken; `fuelle()` ist rein. |
| `CwTastung` (`src/core/cw/`) | Die Steuerung: Einstellungen (`CwKey/…`), zwei Schalter, Sendefreigabe, Nachlauf, Wachen. Gehört `RadioModel`. |
| `FlexCwTastziel` (`src/models/`) | Die schmale Stelle zum Gerät: `bereit / sendet / freigabeSteht / fordereSenden / taste`. |
| `HaliKeyGroup` (`src/gui/setup/`) | Die Einstellgruppe, auf Setup → CAT & Network → MIDI Control. |

RtMidi 6.0.0 liegt unter `third_party/rtmidi/`, so wie AetherSDR es führt
(`docs/attribution/RTMIDI-PROVENANCE.md`).

## Wo es steht

**Setup → CAT & Network → MIDI Control.** Dort hat der Betreiber am
17.11. gesucht; die Seite zeigte noch den Platzhalter („MIDI mapping
table will appear here", ein gesperrtes „Learn…"), während die Gruppe
einen Commit lang unter Keyboard → Shortcuts lag. Der Platzhalter ist
weg, die Gruppe steht an seiner Stelle. Eine Einstellung gehört dorthin,
wo man nach ihr sieht.

## Zwei Schalter

- **Read a HaliKey MIDI** — die Taste wird gelesen. Ohne den zweiten
  Schalter geht nichts zum Gerät: eine Morsetaste macht nur den Mithörton
  (Üben), der Fußtaster tut nichts.
- **Key the transmitter** — ab Werk **aus**. Erst damit sendet etwas.

Ein Programm, das beim Anstecken einer Taste von selbst sendebereit ist,
sendet einmal im falschen Augenblick.

## Der Fußtaster

Er hält das Senden, solange er gedrückt ist — in **jeder Betriebsart** und
an **jedem Gerät**, genau wie „Hold to talk" auf der Leertaste
(`MoxController::onSpacePtt`: `setMox(gedrückt)`). Er geht durch
`RadioModel::setMox`, also durch dieselbe Freigabe wie der MOX-Knopf.

Zwei Wege, je nachdem, wo der Stecker sitzt:

- **PTT-Eingang** (Note 31): in jeder Tastenart.
- **Tastenbuchse** (Note 20, 21 oder 30): mit Tastenart „Foot switch
  (PTT)". Dann läuft kein Geber und klingt kein Ton.

Fällt das Senden von außen (MOX aus, Stehwelle), während der Fuß noch
unten ist, holt er es **nicht** zurück — er fordert nur auf der Kante.
Wache: nach `kPttHoechstensMs` (10 min) gibt er das Senden zurück
(fehlendes Note Off, abgezogenes Kabel).

## Die Morsetaste

Nur an der FlexRadio und nur in CWL/CWU. Die Flex kennt keinen
Paddle-Befehl; sie nimmt `cw key 1|0`, und die Jambus-Zeit macht der
Klient (wie AetherSDR). Gesendet wird `cw key immediate 1|0` über die
Befehlsleitung — AetherSDRs Rückfall ohne NetCW-Strom.

- Der erste Tastendruck fordert das Senden an. Wird es verweigert, geht
  **keine einzige Kante** hinaus.
- Getastet wird nur, während `sendet()` gilt — und das heißt an der Flex:
  MOX steht **und** `xmit 1` ist wirklich hinausgegangen
  (`RadioModel::flexGetastet`). MOX allein reicht nicht: `flexTasten` kann
  nach der Freigabe noch nein sagen (Stehwelle, Transverter auf nur
  Empfang, Verriegelung), und ein `cw key` ohne `xmit` ließe eine Flex mit
  eingeschaltetem Break-in von selbst senden, an allen Toren vorbei.
- Nach der letzten Kante läuft der Nachlauf (`CwKey/HangMs`, 600 ms), dann
  geht das Senden zurück — aber nur, wenn die Taste es selbst geholt hat.
- Fällt das Senden von außen, geht die Taste sofort hoch, und getastet
  wird erst wieder, wenn sie einen Nachlauf lang los war.
- Wache: steht die Taste am Gerät oder ein Kontakt am Eingang
  `kHoechstensUntenMs` (15 s) unverändert, geht alles hoch.

Der Mithörton wird dort geschaltet, wo die Kante entsteht (Geber- oder
MIDI-Faden), atomar. `kante()` auf dem Hauptfaden fasst ihn nicht an —
sonst schaltete eine verspätete Kante einen schon beendeten Punkt noch
einmal ein (AetherSDR #4976).

## Die eine Änderung an der Sendefreigabe

`BandPlanGuard::checkMoxAllowed` lehnt CWL/CWU bisher grundsätzlich ab
(„CW TX coming in Phase 3M-2"): Longpath hat keine eigene CW-Sendekette.
Ohne Änderung hätte die Taste nie getastet — `bereit()` verlangt CW, und
in CW sagte die Freigabe nein.

Neu ist ein achter Parameter `cwKeyedOutside` (Vorgabe `false`): CW geht
durch die **Betriebsarten**-Prüfung, wenn der Aufrufer sagt, dass der
Träger außerhalb unserer Sendekette getastet wird. `RadioModel` sagt das
nur, wenn alle drei gelten (`cwTastwegOffen()`): die Taste ist
eingeschaltet, „Key the transmitter" steht, und eine Flex hängt. Die
Frequenz- und Bandprüfung läuft unverändert — geöffnet wird eine
Betriebsart, nie eine Bandkante. Für jeden anderen Aufrufer und jedes
andere Gerät bleibt CW gesperrt wie bisher.

Nebenwirkung, gewollt: mit beiden Schaltern an und einer Flex lässt sich
in CW auch der MOX-Knopf drücken. Ohne Taste sendet die Flex dabei nichts.

## Bewusst nicht gebaut

- **NetCW-Strom** (VITA-49 mit Zeitstempeln, AetherSDR #4823/#4890):
  `cw key immediate` trägt die Laufzeit der Befehlsleitung als Zittern.
  Das ist der nächste Schritt, wenn es am Gerät hörbar ist.
- **Probengenaue Flanken** im Mithörton nach dem Raster des Gebers.
- **Erkennen von Anstecken/Abziehen**: die Taste wird beim Einschalten und
  mit „Rescan" gesucht. „Rescan" öffnet immer neu (ein toter Griff nach
  Abziehen und Anstecken meldet sich nicht).
- **CW an ANAN/Anvelina** — das bleibt 3M-2.
- Allgemeine MIDI-Zuordnung mit „Learn" (AetherSDRs `MidiControlManager`).

## Prüfstände

| Prüfstand | Was er hält |
| --- | --- |
| `tst_midi_key_input` | Noten des Handbuchs, Anschlag 0 = Loslassen, jeder Kanal, Portwahl |
| `tst_cw_mithoerton` | Stille, Pegel, Flanke ohne Sprung |
| `tst_iambic_keyer` | Punkt, Strich = 3 Punkte, Zusammendrücken A und B, am geplanten Zeitpunkt gemessen |
| `tst_cw_tastung` | Üben schickt nichts; verweigertes Senden tastet nie; Nachlauf; fremdes Senden bleibt; Ende von außen; Wachen; Fußtaster auf beiden Wegen |
| `tst_cw_tastweg_freigabe` | CW-Öffnung nur mit Ansage und nie über die Bandkante; MOX allein reicht nicht; `RadioModel` ab Werk aus |
| `tst_cw_key_setup_page` | Die Gruppe steht auf MIDI Control, kein Platzhalter mehr; ab Werk nichts angehakt, zweiter Schalter ohne den ersten gesperrt |

Zu jeder Gruppe ist eine Gegenprobe gegen die neutralisierte Fassung
gefahren (rot gesehen).

## Am Gerät

Nicht geprüft — das macht der Betreiber, zuerst mit Abschlusswiderstand.
Der Ablauf steht im PR.
