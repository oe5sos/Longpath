# Nativer RTTY-Decoder — Scoping & Umsetzung

**Status:** GEBAUT UND VERSCHIFFT, 2026-09-06 (Betreiber-Freigabe: "ja bitte
rtty"). `src/core/RttyDecoder.{h,cpp}`, `src/gui/RttyDecoderSensitivity.h`,
`src/gui/applets/RttyDecoderApplet.{h,cpp}` + Verdrahtung in
`AudioEngine`/`MainWindow`. Sichtbar nur im Modus DIGL (RTTY ist eine
DIGL-Untermenge, siehe `RxApplet::applyModeVisibility`s eigene Regel "RTTY
-> NUR DIGL"). Der ursprüngliche Recherche-Teil dieses Dokuments (unten)
bleibt unverändert stehen — er war die Grundlage für den Bau, nicht nur eine
Vorstudie, die verworfen wurde.

**Companion:** `tests/tst_rtty_decoder.cpp` (Ende-zu-Ende gegen ein
synthetisches AFSK-Signal geprüft — echte Baudot/ITA2-Zeichen kommen
tatsächlich richtig raus, nicht nur "kompiliert"), `tests/tst_rtty_decoder_sensitivity.cpp`.

**Live am ANAN 10e bestätigt (2026-09-07):** Modus auf DIGL geschaltet,
Panel erschien angedockt unten rechts wie vorgesehen, zeigte korrekt
"Mark 2295 Hz · Shift 170 Hz" (live von der Scheibe gelesen), Pegelbalken
liefen, Text dekodierte (erwartungsgemäß wirres Rauschen ohne Antenne).
Baudrate/Umkehrpolarität/Empfindlichkeit-Regler funktionsfähig. Ein
Nachtrag zur Sichtbarkeits-Korrektur (Commit `a7dadd82`, siehe unten) war
noetig, weil eine wiederhergestellte Sitzung direkt in DIGL/RADE starten
kann, ohne dass ein Moduswechsel-Signal je feuert.

Eine Randnotiz: `dumpTree` (Automatisierungs-Bruecke) meldete waehrend
der Live-Pruefung `visible: false` fuer das angedockte Panel, obwohl es
auf dem Bildschirm sichtbar war (Betreiber-Bestaetigung) -- eine
dumpTree-Eigenart bei Widgets in einem scrollbaren Bereich, kein echter
Fehler. Fuer kuenftige Fernpruefungen: bei Widgets im AppletPanelWidget-
Scrollbereich lieber am Bildschirm nachsehen (Screenshot/Betreiber
fragen) statt sich allein auf `dumpTree`s `visible`-Feld zu verlassen.

**Zweiter Stale-Slice-Fund, behoben (2026-09-07, Commit `d4c001af`):**
bei einer spaeteren, systematischen Bereich-fuer-Bereich-Pruefung am
selben Abend zeigte ein Bandklick (per-Band gemerkter Modus DIGL -> USB)
`RxApplet`s Mark/Shift-Zeile korrekt, aber `RttyDecoderApplet` blieb
sichtbar mit den Werten der ALTEN Scheibe -- `MainWindow` band das
Applet nur einmal, beim allerersten `sliceAdded(0)`, nie erneut bei
`RadioModel::activeSliceChanged`. Dieselbe Fehlerklasse, die `CommandBar`
am selben Tag hatte (`db0c50cf`). Fix: `MainWindow::
rebindRttyRadeAvailability(SliceModel*)`, aufgerufen sowohl aus
`wireSliceToSpectrum()` als auch aus dem `activeSliceChanged`-Handler.
Neuer Regressionstest in `tests/tst_rtty_decoder_applet.cpp`.

## Woher das kommt

Martin fragte allgemein nach "digitalen Programmen" (FT8, PSK31, RTTY &
Co.). Recherche in Thetis + AetherSDR ergab: FT8/FT4/JT65/PSK31 werden von
BEIDEN Referenzen nicht selbst dekodiert, nur per virtuellem Audiokabel +
CAT/TCI an ein externes Programm (WSJT-X, fldigi, JS8Call) weitergereicht
— Longpath hat diese Grundlage schon (DIGU/DIGL-Modi mit eigenem Klick-
Tune-Versatz, `WsjtxClient` fuer Spot-Empfang). Das selbst nachzubauen
waere ein sehr grosser DSP-Aufwand ohne jedes Vorbild.

**RTTY ist anders:** `~/Longpath/AetherSDR` hat einen echten, eigenstaen­
digen RTTY-Decoder (Baudot/ITA2), der ohne externes Programm direkt im
eigenen Fenster dekodiert. Kein Thetis-Aequivalent (Thetis hat gar keinen
nativen RTTY/PSK-Decoder), also gilt hier wie bei der Dev-Automatisierungs-
Bruecke: AetherSDR ist die alleinige Quelle, kein Zwei-Quellen-Abgleich
noetig.

## Was AetherSDR tatsaechlich hat

Gelesen: `src/core/RttyDecoder.{h,cpp}` (94+308 Zeilen),
`src/gui/RttyDecoderSensitivity.h` (30 Zeilen, reine Funktion), plus die
Verdrahtung in `src/gui/MainWindow_DigitalModes.cpp` (nur
`routeRttyDecoderOutput()` + `refreshRttyDecodeState()`, zusammen ~80
Zeilen — die restlichen ~1360 Zeilen dieser Datei sind RADE/FDV/DAX/WFM/
AX.25, nicht RTTY) und ~29 RTTY-Stellen in `PanadapterApplet.cpp` (Text-
Anzeige, Mark/Space-Pegelbalken, Lock-Anzeige, Empfindlichkeits-Regler).

**Architektur:** eigener `QThread`, gespeist mit rohem 24-kHz-Mono-PCM
(`feedAudio()`). Zwei Biquad-Bandpaesse (Mark/Space, Bandbreite = 3×Baud),
Einhuellenden-Erkennung, Schmitt-Trigger mit Hysterese gegen Klappern an
der Grenze, einfache proportionale Taktkorrektur (25 % pro Flanke),
Start-Stop-Rahmenerkennung, Baudot/ITA2-Tabellenumsetzung inkl. LTRS/FIGS-
Umschaltung. Parameter live aenderbar: Markenfrequenz (Default 2125 Hz),
Shift (Default 170 Hz — Standard-HF-RTTY), Baudrate (Default 45.45 —
Standard-Amateurfunk-RTTY), Umkehrpolaritaet. Meldet Konfidenz pro Zeichen
und Mark/Space-Pegel + SNR + Lock-Status zweimal pro Sekunde.

**Kein PSK31-Aequivalent** existiert irgendwo (weder Thetis noch
AetherSDR) — nur RTTY waere mit echtem Vorbild machbar.

## Realistische Groesse fuer Longpath (nicht nur der Kerncode)

| Teil | AetherSDR-Groesse | Fuer Longpath |
| --- | --- | --- |
| Kerndecoder (`RttyDecoder.{h,cpp}`) | 402 Zeilen | 1:1 uebertragbar, eigener Namensraum |
| Empfindlichkeits-Mapping | 30 Zeilen | 1:1 uebertragbar |
| MainWindow-Verdrahtung | ~80 Zeilen | neu zu schreiben gegen Longpaths `SliceModel`/Audio-Pfad (anderer Zuschnitt als AetherSDRs `MainWindow_DigitalModes.cpp`) |
| Bedienoberflaeche (Text, Pegel, Lock, Regler) | ~29 Fundstellen in `PanadapterApplet.cpp` | neu als eigenes Longpath-Applet (Longpath hat keine `PanadapterApplet`-Klasse — eigenes Widget im `applets/`-Stil noetig) |

**Grobe Gesamtschaetzung: 700–1000 Zeilen** — deutlich kleiner als
Midi2Cat (~13.300), ein realistisch in einer Sitzung baubares Feature.

## Anknuepfungspunkt in Longpath

Longpath hat DIGU/DIGL bereits vollstaendig aus Thetis portiert
(`SliceModel::m_diglOffsetHz`/`m_diguOffsetHz`, TCI-Klick-Tune-Versatz).
Ein RTTY-Panel wuerde sich anbieten, sobald die aktive Scheibe in DIGU
oder DIGL steht und der Bediener es oeffnet — genau der Zustand, den
Longpath schon unterscheidet, kein neuer Modus noetig.

## Offene Punkte vor dem Bauen

- Wo bekommt der Decoder sein Audio her? AetherSDR speist ihn aus
  `PanadapterStream::audioDataReady` (24 kHz Mono/Stereo). Longpaths
  Audio-Pfad (`RxDspWorker`/`AudioEngine`) muesste den passenden Abgriff
  liefern — vermutlich denselben Tap-Punkt wie die VAC-Kalibrierung
  (AF-Gain-unabhaengiger Pegel, siehe Architektur-Notiz zu Phase 3M-3a-iv),
  aber das ist noch nicht gegengeprueft.
- GPLv3-Herkunftsvermerk noetig (AetherSDR-Header + Eintrag in
  `aethersdr-reconciliation.md`, wie bei `DevAutomationServer`).
- Reines Lesefeature ohne TX-Bezug — keine Sicherheitsfreigabe noetig,
  anders als bei Midi2Cat oder der Automatisierungs-Bruecke Phase 1.
