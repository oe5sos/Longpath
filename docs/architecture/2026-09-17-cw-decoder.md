# Nativer CW-Decoder — aus dem Zeus Station Engine portiert

**Stand:** 17. September 2026
**Anlass:** Punkt 3 der Benchmark-Liste in
[docs/design/2026-09-17-zeus-benchmark.md](../design/2026-09-17-zeus-benchmark.md)
(„egal" — Wahl fiel auf die Technik, nicht auf die Gestaltung, die an
diesem Tag in einer anderen Sitzung lief). Zeus' CW-Konsole (2.0.26,
#2181/#2183) zeigt decodierten Text, Geschwindigkeit und SNR; der Decoder
darunter liegt offen im GPL-Engine (`Station.Engine.Hosting/CwDecoder/`,
929 Zeilen C#, fünf Klassen), die Konsole selbst nicht.

## Was gebaut wurde

| Datei | Herkunft | Inhalt |
| --- | --- | --- |
| `src/core/CwDecoderCore.{h,cpp}` | **Port** aus Zeus @8970f2d, GPL-2.0-or-later (KB2UKA/N9WAR) | Goertzel-Bank (11 Bins à 25 Hz um den Empfangston, 256-Sample-Blöcke = 5,3 ms, Umschalten erst nach 8 Siegen in Folge), adaptive Schwelle (Rauschboden-EMA, Ein ×12 / Aus ×5, Warmlauf, Wiederaufnahme nach 2 s digitaler Null), Punktlängen-Schätzer (unterer Cluster der Elementdauern), Morse-Zustandsmaschine (Muster → Zeichen, Konfidenz aus der Passung). Kein Qt. |
| `src/core/CwDecoder.{h,cpp}` | Longpath-original | QObject-Hülle: Stereo-Abgriff → Mono → Kern, `textDecoded(text, confidence)`, `statsUpdated(wpm, snr, tone, trackedHz)` mit 10 Hz. **Kein Arbeitsfaden** (anders als `RttyDecoder`): 11 Bins × 256 Samples je 5 ms sind Mikrosekunden auf dem GUI-Faden. |
| `src/gui/applets/CwDecoderApplet.{h,cpp}` | Longpath-original, nach `RttyDecoderApplet` | Tonkapsel (TON/KEIN TON), erfasster Ton, WPM, SNR, SNR-`HGauge`, Textfeld (4000 Zeichen), `TON: ◀ 600 Hz ▶` (25-Hz-Schritte, Start auf `CWPitch`, nicht persistiert), Leeren. Sichtbar nur in CWL/CWU — dieselbe Verfügbarkeitsachse wie RADE und RTTY (`MainWindow::rebindRttyRadeAvailability`). |
| `AudioEngine::setCwTap` | Longpath-original, fünfter Abgriff | Gleicher Bau wie QSO/ASR/WAV/RTTY: eigener `AudioTapRing`, atomarer Zeiger, Quiescence-Zähler beim Abschalten. |

## Zwei Abweichungen vom Original — und warum

Beide fand der Prüfstand mit synthetischem Morse (PARIS-Timing, 5-ms-
Raised-Cosine-Flanken, weißes Rauschen), bevor eine Zeile Oberfläche
stand:

1. **Buchstaben-/Wortpausen-Schwellen 2,0 / 5,0 Punktlängen statt 3,0 /
   5,5.** Zeus schätzt die Punktlänge aus gemessenen *Tondauern*. Jede
   Tastflanke lässt einen Ton länger und die Pause danach kürzer messen
   als nominal — bei 5-ms-Flanken und hohem SNR liest ein 60-ms-Punkt
   70 ms, die 180-ms-Buchstabenpause 170 ms. „Pause ≥ 3,0 × Punkt" (210 ms)
   feuert dann nie; ganze Wörter fielen zu einem „?" zusammen. Die
   Mittelpunkte zwischen den nominalen 1/3/7-Punkt-Pausen halten nach
   beiden Seiten Abstand (Elementpause ≈ 0,7 < 2,0 < Buchstabenpause ≈ 2,4;
   Buchstabenpause < 5,0 < Wortpause ≈ 5,9).
2. **Angezeigte Geschwindigkeit aus dem Mittel von Ton- und
   Elementpausen-Cluster.** Derselbe Flankeneffekt verlängert Töne und
   verkürzt Pausen um denselben Betrag; das Mittel ist die wahre
   Punktlänge. Zeus' `1200 / Tonpunkt` las bei 20 WPM 17,5 WPM.

Beide Stellen sind im Quelltext als „Longpath deviation" / „Longpath
addition" markiert, der Rest ist Zeile für Zeile zitiert
(`// From Zeus station-engine Station.Engine.Hosting/CwDecoder/<Datei>.cs:<Zeilen> [@8970f2d]`).

Was bewusst wie bei Zeus blieb: der Kaltstart nimmt 20 WPM an; ein viel
langsamerer Geber (12 WPM) verliert das erste Wort, bis die periodische
Neuclusterung (alle sechs Elemente) greift; in Stille wandert der
erfasste Ton mit dem Rauschen (die Anzeige zittert dann — kein Fehler).

## Prüfung

`tests/tst_cw_decoder.cpp` (9 Fälle): sauberer Text bei 20 WPM Zeichen
für Zeichen, Geschwindigkeit folgt 12 und 35 WPM (±15 %), Ton 80 Hz neben
der Vorgabe wird erfasst, Rauschen allein schreibt nichts, −3 dB
Breitband-SNR decodiert das Rufzeichen (≈19 dB Verarbeitungsgewinn im
Goertzel-Bin), `reset()` vergisst die Geber-Zeit, die Hülle liest den
linken Kanal und klemmt den Ton auf 100…2000 Hz.
`tests/tst_cw_decoder_applet.cpp` (3 Fälle): Abgriff folgt dem Slice,
löst beim Entfernen, zieht beim Umbinden um. `tst_audio_engine_tap_
teardown_race` um den fünften Abgriff ergänzt. App gebaut, Applet über die
Automatisierungsbrücke in einer Zweitinstanz (Profil, CWU/20 m) gerendert.

**Nicht am Funkgerät geprüft** — wie beim RTTY-Decoder gehört der
Live-Test (Bake oder QSO auf 20 m CW) Martin: Panel im CW-Modus
andocken, Ton auf den Mithörton, Text lesen, WPM/SNR beobachten.

## Offen

- CW-Makro-Bänke und die Keyer-Kopplung aus Zeus' CW-Konsole (unser CWX-
  Applet ist getrennt; ein kombiniertes Panel wäre Gestaltung).
- Die Konfidenz je Zeichen wird nur als Schwelle (0,15) genutzt, nicht
  angezeigt — Zeus zeigt sie auch nicht.
- Kaltstart bei sehr langsamen Gebern: ein zweiter Cluster-Durchlauf
  gleich nach dem ersten Element würde das erste Wort retten; nicht
  gemacht, weil Zeus es auch nicht tut und der Prüfstand es als
  Kaltstart-Verhalten festhält.
