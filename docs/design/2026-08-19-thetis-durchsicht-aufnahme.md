# Thetis-Durchsicht, Runde 1: Aufnahme und Wiedergabe

**Stand:** 19. August 2026, Nachtschicht
**Quelle:** `../Thetis` @ `v2.10.3.15-5-g852bf0e`,
`Project Files/Source/Console/clsAudioRecordPlayback.cs` (4042 Zeilen,
MW0LGE)
**Anlass:** „wenn du zeit hast kannst du auch gerne bei thetis
nachschauen, was wir vor dort aus einbauen könnten und verbessern."

Runde 1 sieht nur auf Aufnahme und Wiedergabe, weil NereusSDR heute
genau dort etwas Neues bekommen hat. Weitere Runden zu anderen
Bereichen stehen aus.

---

## Was Thetis kann und wir nicht

| # | Thetis | Bei uns | Braucht Hardware? | Wert |
| --- | --- | --- | --- | --- |
| 1 | `AudioBitDepthMode` — float32 / PCM 32 / 24 / **16** / 8, mit `DitherEnabled` | ✅ **erledigt** 19.08. | nein | **hoch** — 30 min Stereo sind bei uns 690 MB, als PCM16 noch 173 MB |
| 2 | `GetJSONDetailsFromFile` — Beschreibung wieder einlesen | ✅ **erledigt** 19.08. | nein | **hoch** — die Liste zeigt Dateinamen statt QSO |
| 3 | `OkToRecord(pfad)` + Platzprüfung **während** der Aufnahme (Zeitgeber, stoppt selbst) | ✅ **erledigt** 19.08. (nur vor dem Start, siehe unten) | nein | **hoch** — eine vergessene Aufnahme kann die Platte füllen |
| 4 | `DeleteRecording(..., delete_containing_folder_if_empty)` | ✅ **erledigt** 19.08. (Rechtsklick in der Liste) | nein | mittel |
| 5 | `PlayFileViaPCAudio` — Aufnahme über die Lautsprecher anhören | nichts | nein | mittel — Nachhören gehört zum Aufnehmen |
| 6 | `PlayFileViaWDSP(..., adjustGain_dB)` + `MoxOnPlayback` — Datei senden | nichts | **ja** | hoch, aber erst mit Funkgerät prüfbar |
| 7 | `AudioRecordRxSource.ReceiverInputIQ` — auch I/Q mitschneiden | nur Ton | nein | niedrig (eigenes Vorhaben, Phase 3M) |
| 8 | `AudioRecordTxSource.TransmitterOutputIQ` | nur Mikrofon | ja | niedrig |
| 9 | `GenerateMP3File` | nein | nein | niedrig — braucht einen Kodierer im Baum |
| 10 | `PCInputSource` Both / Left / Right | nein | ja | niedrig |

## Was wir haben und Thetis nicht

Nicht alles läuft in eine Richtung:

* **Eine Datei, zwei Spuren.** Thetis schreibt je Quelle eine eigene
  Datei. Unsere Stereodatei hält beide Seiten synchron — bei einem QSO
  ist genau das die Frage: wer wann was gesagt hat. Zwei Dateien laufen
  auseinander, sobald jemand eine davon schneidet.
* **Die Ausrichtung.** Der Empfang ist bei uns die Uhr, die Sprechspur
  wird bis zum aktuellen Empfangsstand mit Stille aufgefüllt. Thetis
  braucht das nicht, weil dort jede Quelle ihre eigene Datei bekommt.

## Vorschlag für die Reihenfolge

Ohne Hardware, heute Nacht machbar:

1. **PCM16 statt float32** (#1). Viertelt jede Datei. Dither dazu, weil
   16 Bit ohne Dither bei leisen Stellen hörbar rasselt.
2. **Beschreibung wieder einlesen** (#2). Die Liste zeigt dann
   „19.08. 18:02 · DL1ABC · 14.205.000 · LSB · 6:41" statt eines
   Dateinamens.
3. **Platzprüfung** (#3) vor dem Start.
4. **Löschen aus der Liste** (#4).
5. **Nachhören über die Lautsprecher** (#5).

Braucht dich:

6. **Wiedergabe über WDSP mit Tastung** (#6) — dieselbe Baustelle wie
   die Wiedergabe im Sprachspeicher. Das gehört auf die Bank, nicht in
   die Nacht.

## Was in der Nacht vom 19. auf den 20. daraus wurde

Punkte 1 bis 4 sind gebaut, mit Tests, ohne Hardware:

* **PCM16 mit Dither** (`writeWavStereo16`). Dreieckverteilt, mit festem
  Startwert — dieselbe Aufnahme zweimal gespeichert ergibt zweimal
  dieselbe Datei, sonst ist kein Vergleich möglich. Vier neue Testfälle
  in `tst_wav_file`, darunter die Kappung (ein Überlauf, der umläuft,
  klingt wie ein Schuss).
* **Beschreibung wieder einlesen** (`readQsoDescription`). Die Liste
  zeigt jetzt Zeit, Rufzeichen, Frequenz, Modus und Dauer.
* **Platzprüfung vor dem Start**, gerechnet gegen die volle halbe
  Stunde. Eine Aufnahme, die nach 25 Minuten am Platz scheitert, ist
  schlimmer als eine, die gar nicht erst anfängt. Der laufende Zeitgeber
  aus Thetis entfällt bei uns: wir sammeln im Arbeitsspeicher und
  schreiben erst am Ende, die Frage stellt sich also während der
  Aufnahme nicht.
* **Löschen per Rechtsklick**, mit Rückfrage; die Beschreibung geht mit.

Nebenbei ein Test repariert, der aus Glück grün war: `tst_qso_recorder`
prüfte, ob sich beide Kanäle zu null mitteln, und verglich **einen**
Wert gegen 1e-6. Mit Dither hebt sich der Rundungsfehler an genau
dieser Stelle zufällig auf — der Test blieb grün, ohne noch etwas zu
prüfen. Jetzt über 600 Werte, mit einer Schranke, die 16 Bit plus
Dither zulässt.

## Eine Sache, die mir dabei aufgefallen ist

Unsere 30-Minuten-Grenze schützt die Platte, aber nicht den
Arbeitsspeicher: 30 min Stereo float32 sind **345 MB je Spur**, zusammen
knapp 700 MB, und die liegen bei uns im RAM, bis jemand speichert.
Thetis schreibt fortlaufend in die Datei und hat das Problem nicht.

Das ist keine Kleinigkeit und auch nicht mit PCM16 erledigt (die
Umrechnung passiert erst beim Speichern). Der saubere Weg ist
fortlaufendes Schreiben — ein eigener Schritt, kein Anhängsel.
Bis dahin ist die Grenze das, was uns schützt, und sie steht sichtbar
im Fenster.
