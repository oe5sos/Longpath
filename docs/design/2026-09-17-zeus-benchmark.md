# Zeus als Maßstab: Funktionen und Gestaltung, zweite Runde

**Stand:** 17. September 2026, abends
**Anlass:** „suche bitte noch einmal genau und intensiv, ggf. auch wo anders.
aber zeus ist die referenz, auch vom design die bench mark." Die erste Runde
(Teile 1–3, [Plugins](2026-09-17-zeus-plugin-system-inventar.md),
[WDSP](2026-09-17-zeus-wdsp-fork-inventar.md),
[Protokoll](2026-09-17-zeus-stationsprotokoll-inventar.md)) hatte den
Engine-Quelltext gelesen und die Produktseite von Zeus als „Produktumfang, kein
Protokollthema" abgetan. Das war zu eng. Diese Runde misst **Longpath an Zeus
als Produkt**: was der Bediener bekommt, und wie es aussieht.

**Was diesmal gelesen wurde — alles öffentlich, nichts Proprietäres:**
- Engine-Quelltext **auf v2.0.26 nachgezogen** (`8970f2d`, 16.09.; vorher
  v2.0.19): 325 Routen, ~200 Dienste in `Station.Engine.Hosting` (Namen sind
  Funktionsliste), neue Dienste seit 2.0.19 (APRS, HL2-IO-Board, TX-IQ-Timing).
- **Download-Manifest** `downloads.zeussdr.com/manifest.json` mit den
  vollständigen Änderungsnotizen 2.0.24 / 2.0.25 / 2.0.26 (13.–16.09.).
- **Website** zeussdr.com: Positionierung, Feature-Suite, **zwölf
  Screenshots** in Originalauflösung (`assets/zeus-*.webp`, u. a. die
  Betriebskonsole 1920×805, die 3D-Ansicht, FT8, Audio-Werkzeuge,
  Feature-Katalog, Logbuch, Chat, Recorder, VST-Host, Verstärker); die
  **Mobile-Web-App** app.zeussdr.com (Pairing-Seite); das README des
  Wear-OS-Clients.
- Der öffentliche **UI-Stilvertrag** (CONTRIBUTING §4 des
  community-features-Repos): Tokens, Regeln, Prüfnachweise.
- Gegenprobe bei uns: Stichwortsuche über 1 075 Quelldateien, `ROADMAP.md`,
  `HAUSSTIL.md`, und ein **Erststart-Bildschirmfoto** der aktuellen Longpath
  (Martins Bau von 19:46, über die Automatisierungsbrücke, eigenes Profil
  `benchmark-shot`, nicht verbunden — sein laufendes Exemplar blieb unberührt).

Die Zeus-Bilder liegen nicht im Repo (Urheberrecht der Autoren); sie sind unter
`https://zeussdr.com/assets/<name>.webp` jederzeit abrufbar. Unser eigenes
Bild: [`2026-09-17-zeus-benchmark/longpath-erststart-1440.png`](2026-09-17-zeus-benchmark/longpath-erststart-1440.png).

---

## 1. Zeus in Zahlen

| | Zeus | Longpath |
| --- | --- | --- |
| Version / Datum | 2.0.26 / 16.09.2026 | 0.6.2 auf der Website, Ast bei 0.6.3-rc |
| Kadenz | **13 Releases in 20 Tagen** (2.0.14 am 27.08. → 2.0.26 am 16.09.), jede mit Änderungsnotizen | zurückgestellt seit 05.09. (Gerätetest) |
| Team | zwei Maintainer (KB2UKA, N9WAR), Issue-Nummern bis #2196 | einer + KI |
| Geräte | HL2 (RX+TX), ANAN G2/G2 MkII (RX+TX), ANAN-100D/Angelia (RX, P1 auch TX), Anvelina (Profil-Steuerung, „physisch unbestätigt"), **IC-7760 experimentell** (USB, RX-Audio, Scope), KiwiSDR | ANAN 10E, ANVELINA PRO 3, HL2 (Code vorhanden), SunSDR2 QRP nativ **und** via TCI, FLEX-8400 via AetherSDR/TCI, KiwiSDR |
| Plattformen | Win x64/ARM, macOS, Linux (deb/AppImage, arm64), Web-App Android/iOS, Wear OS | macOS (Fokus), Windows/Linux in CI |
| Modell | offener Engine, proprietärer Client, Feature-Katalog mit 22 Schaltern, Abo-Gating („24-Stunden-Autorisierung", Local Sign-On 4,99 $/Monat) | GPL, alles drin |

Die Kadenz ist der eigentliche Maßstab: Zeus liefert, was wir als
„Prüfstand + Release später" halten, im Zwei-Tage-Takt mit Notizen.

---

## 2. Funktionsmatrix

Legende: ✅ gleichwertig oder besser · ⚠️ teilweise / anders · ❌ fehlt ·
— kein Bedarf für uns. Quelle Zeus: Route/Dienst/Notiz in Klammern.

### A · Empfang und DSP

| Zeus | Longpath | Stand |
| --- | --- | --- |
| NR, NR2 (+Trained, post2), NR3 RNNoise mit Modell-Upload/-Download, NR4, **NNR** mit eigenen Reglern (`/api/rx/nr*`, 2.0.24) | 8 NR-Modi inkl. NNR (14.09.), DFNR im Prüfstand | ✅ |
| **RX-Audio-Profile** — DSP-Regler + RX-Suite + Plugins als Profil gespeichert (2.0.26 #2166) | Mic-Profile für TX (`MicProfileManager`), **kein RX-Profil** | ❌ klein |
| DSP-Regler in der Kopfleiste, Menüs bleiben im Fenster (2.0.26) | Kopfleiste BAND/MODE/FILTER/STEP/NR ✅ | ✅ |
| **RX-Leveler** („constant loudness", `/api/rx/leveler`, Beweis-gebunden gegen Rauschen-Aufholen, ADC-Overload-Veto) | AGC; kein Nachleveler | ❌ mittel — Zeus' Notiz beschreibt genau das Problem „AGC hebt Rauschen an" |
| Adaptiver Squelch + fester FM-Squelch (2.0.24) | Squelch, SSQL (Voice-Squelch) ✅ | ✅ |
| Auto-AGC-T aus dem Rauschboden (`AutoAgcNoiseFloorTracker`, Thetis-Port) | `tst_slice_auto_agc` ✅ | ✅ |
| Filtergröße/-fenster/-phase pro RX und TX bis 262 144 Taps (`/api/rx/filter-window`, `filter-phase`) | 1024–16 384, Low-Latency/Linear ✅ | ✅ (Ultra-Auflösung bewusst nicht) |
| Notches (`/api/rx/notches`), ANF, SNB, NB, NBP | ✅ | ✅ |
| Diversity mit Phasen-/Gain-Reglern **und acht Speichern in der Fußleiste** (2.0.24) | `DiversityApplet` ✅, Speicher? | ⚠️ Speicher prüfen |
| Multi-DDC bis 10 RX + Kiwi-Slot, per-RX Mute/LO/Zoom | Slices, Multi-RX ✅ | ✅ |
| Breitband-Spektrum mit **Signaldetektor** (`WidebandSignalDetector`, `/api/radio/wideband/signals`) | Wideband-FFT ✅, Detektor? | ⚠️ |
| **In-Passband-SNR-Schätzer** (`InPassbandSnrEstimator`, S-Meter zeigt „SNR 57") | S-Meter ohne SNR | ❌ klein, hübsch |
| Frequenzkalibrierung gegen Referenz (`FrequencyCalibrationService`, Thetis WWV) | `CalibrationController` WWV ✅ | ✅ |
| S-Meter-Kalibrierung (`/api/radio/smeter-calibration`) | `RxMeterCalibration` ⚠️ | ⚠️ |
| Transverter mit eigener Antennenroute und **eigener PA-Kalibrierung** (2.0.24/26) | Transverter ✅ (68 Dateien), PA-Kal je Transverter? | ⚠️ |

### B · Senden und TX-Audio

| Zeus | Longpath | Stand |
| --- | --- | --- |
| Noise Gate · 10-Band-EQ · Kompressor · **Aural Exciter** · **Bass Enhancer** · Reverb (sechs native Prozessoren mit Live-Metern, Feature-Katalog) | Gate · EQ · De-Esser · Comp · Tube · PUDU (Exciter) · Reverb · Limiter | ✅ (Bass fehlt, für SSB egal) |
| **VST3 / Audio-Unit-Hosting** in TX- und RX-Kette (Scanner, Rack, Bypass, Meter) | nichts | ❌ **die** sichtbare Lücke, siehe Teil 1 Mitnahme 1 |
| TX-Audio-Profile mit Import/Export (`4k Voodoo [Native]`) | Mic-Profile + Strip-Presets ✅, Import/Export? | ⚠️ |
| CFC mit Presets (`/api/tx/cfc/presets`), CESSB (auch 4 kHz), Phasenrotator mit **Auto-Kalibrierung + Asymmetrie-Anzeige** (WDSP 2.0) | CFC ✅, CESSB ✅, Phasenrotator manuell | ⚠️ (Upstream-WDSP-Frage) |
| PureSignal 3.0 mit `/api/tx/ps/*` (Feedback-Quelle, Auto-Attenuate, Save/Restore, Monitor), Zwei-Ton-Start-Meldung | PS 2.x (Thetis), 22 Wrapper, Auto-Attenuate? | ⚠️ (WDSP 2.1) |
| **TX-Stufenmeter** MIC/LEV/ALC PK+AVG als VU-Säulen + Forward-Power/SWR-Zeiger (Screenshot) | HGauge-Balken, Meter-Container mit `NeedleItem` ✅ | ✅ anders |
| TX-Fidelity-Policy / Station-Profil-Katalog (`TxFidelityPolicyStore`) | Voice-Check + Strip-Tuner (unser eigener Weg) | ✅ anders |
| Roger-Beep (`/api/tx/roger-beep`), RX-Resume-Delay, Prekey-Delay | ❌ / ✅ (MoxController-Timer) | ⚠️ Roger-Beep fehlt (niedrig) |
| **QRM-/Signal-Simulator** (`SignalJammerTxSource`, `/api/tx/qrm/text`) | nichts | — (Testwerkzeug) |
| Anti-VOX gegen Lautsprecher-Rückkopplung (2.0.24) | Anti-VOX ✅ | ✅ |
| PA-Kalibrierung lernt je Leistungsziel (2.0.24), Drive-Profile je Band | `PaSetupPages`, Drive je Band ✅ | ✅ |
| Serieller PTT / externer PTT (`SerialPttService`, `ExternalPttService`) | `PttSource` Mic/CAT/Cw/…, **kein serieller Fußschalter** | ❌ klein |

### C · Digital und CW

| Zeus | Longpath | Stand |
| --- | --- | --- |
| **Native FT8/FT4** im Fenster: Decodes, Auto-CQ, **Anrufer-Warteschlange**, Decodes auf dem Globus, Signalreport aus gemessener Leistung, Country/DXCC/Zone (2.0.26) | WSJT-X extern über VAX + CAT/TCI, `WsjtxClient`, PSK-Reporter, Spot-Farben für WSJT-X | ⚠️ bewusst extern (06.09. entschieden); Zeus zeigt, wie es integriert aussieht |
| FreeDV (Codec2-Modem) als Plugin | **RADE** nativ (neuer als Zeus' Codec2-Modem) | ✅ voraus |
| **CW-Konsole**: Keyer + **Decoder** (Goertzel, adaptive Schwelle, Morse-FSM, Timing-Schätzer) + Makro-Bänke, decodierte Geschwindigkeit + SNR (2.0.26 #2181) | CWX-Keyer ✅, **kein CW-Decoder**, Makros nur über TCI-Befehle | ❌ **mittel** — der Decoder-Kern ist GPL im Engine (`CwDecoder/`, ~7 Dateien), portierbar wie der RTTY-Decoder |
| RTTY | nicht gefunden | ✅ voraus (RTTY-Decoder 06.09.) |
| HF-APRS-Betriebsfenster + weltweite APRS-Verfolgung (2.0.24) | nichts | ❌ niedrig |
| Winlink (Pat) Mail-Arbeitsplatz (2.0.25) | nichts | ❌ niedrig |

### D · Logbuch, Spots, Karte

| Zeus | Longpath | Stand |
| --- | --- | --- |
| Logbuch-Arbeitsplatz: Tabelle + **Worked-the-World-Globus** (NASA Blue Marble, Tag/Nacht, 1844 Standorte) + Kennzahlen-Kacheln (Log/Bänder/Modi/Aktivität 26 Wochen/Top-Länder/**Awards DXCC·WAS·Grids**) + Detailkarte je Rufzeichen mit QRZ-Kopf, History, Notizen, Tags, QSL/LoTW | `LogbookWindow`, ADIF, `WorkedBefore`, `QsoConfirmation`, `QsoMapWindow` (Weltkugel-Zoom), Spot-Hub | ⚠️ **Awards-Zentrum und Kennzahlen fehlen**; unser Logbuch ist Tabelle + Karte |
| QRZ-Awards-Center mit Schätzung der fehlenden Credits (2.0.24) | nichts | ❌ mittel |
| Logbuch-Backend als Plugin, „Zehntausende QSOs ohne Ruckeln" | Qt-Tabelle (Accessibility-Explosion 07.09. behoben) | ✅ |
| Spots: DX-Cluster, RBN, PSK-Reporter, POTA, SOTA, Watchlist (`SpotHubDialog`) | ✅ **voraus** (Zeus: DxCluster-DTOs, kein POTA/SOTA gefunden) | ✅ |
| **Zeus HUD**: Globus mit APRS-Stationen, NOAA-Blitzen, Flugzeugen (ADS-B), Schiffen (AIS), Kameras, **Satelliten-Vorhersage** (SGP4) — „GodsEye" | Weltkarte mit Kartenhintergrund, QSO-Karte, Rotor-Richtung | ⚠️ Sat-Vorhersage wäre für VHF/UHF (Contest!) interessant, Rest ist Schau |
| Stations-Favoriten / Speicher (`/api/station/favorites/{slot}`, „SAVE MEM") | `TuneMemoryStore` (Tuner), Bandstapel ✅, **keine Frequenzspeicher** | ⚠️ klein |
| Kontest-Log (`/api/contest-log`) | eigenes Contestprogramm (getrennt) | ✅ anders |
| **ZeusChat** (Text, Fotos, Sprachnachrichten, Push-to-Talk zwischen Zeus-Stationen) | nichts | — (Community-Produkt) |

### E · Hardware

| Zeus | Longpath | Stand |
| --- | --- | --- |
| RF2K-S, PGXL, TGXL, Antenna Genius, KPA500, Mercury LUX, VK3AMP, **SPE Taurus** (mit Auto-Tune-Koordinator), HF-AUTO (UDP) | RF2K-S, PGXL, TGXL ✅, Antennenumschalter-Toast | ✅ für unsere Geräte |
| **Zeus Tuner Companion**: manuelle Tuner-Einstellungen je Tuner/Antenne/Frequenz merken, mit Notizen, Backup (2.0.25) | `TuneMemoryStore` (TGXL_TuneMemory_Ant) ✅ | ✅ |
| HL2 IO-Board + HL2+-Codec-Regler, HL2-GPIO, **G2-Wartung** (FPGA/p2app-Update, Backup, SD-Karte) | HL2 IoBoard ✅ (49 Treffer), keine G2-Wartung | ⚠️ (kein G2 hier) |
| Rotor | im Engine **nicht gefunden** | ✅ voraus (ARCO live 16.09.) |
| Radio **reclaim** (belegtes HPSDR-Radio per Stop übernehmen, mit Bestätigung) | ? | ⚠️ prüfen — nützlich, wenn eine abgestürzte Session das Radio hält |
| Geräte-Stopp-Erholung, P1-Startfehler-Erholung, P2-Auto-Connect, Preferred Radio | Reconnect-Timer, Connect-Watchdog (28.08.) ✅ | ✅ |
| VNA: NanoVNA, HL2-VNA-Sweep, Sweep über das verbundene Radio, `VnaMath` (11 Routen) | SWR-Sweep über Radio + `.s1p`-Import ✅ | ✅ (NanoVNA-Direktanbindung fehlt, niedrig) |
| IC-7760 experimentell über USB | SunSDR nativ/TCI, FLEX via TCI | ✅ anders |

### F · Station, Sicherheit, Betrieb

| Zeus | Longpath | Stand |
| --- | --- | --- |
| Sende-Lease + Herzschlag, kein Auto-Key, Auto-CQ nur mit Bediener-Quittung (`OperatorAckStore`) | MOX-Freigabe bei Client-Verlust + Wachhund (PR #16, heute) | ✅ seit heute |
| Bandplan-Regionen IARU R1/R2/R3, US-FCC-Klassen, EI, G (`BandPlans/*.json`) | `BandPlanGuard` (Region 1) ✅ | ✅ für uns |
| Transmit-Timeout, SWR-Schutz | `SwrProtectionController` ✅ | ✅ |
| Offline-Betrieb innerhalb der 24-h-Autorisierung (Abo) | — | — |
| **Einstellungs-Backup/Profile-Export** (`UnifiedDatabaseBackup`, `PrefsProfileExport`), Layout als portable `.zeus-layout`-Datei teilen | Profile ✅, **kein Layout-Export** | ⚠️ klein |
| Onboarding-Zustand (`OnboardingEndpoints`) | nur VAX-Erststart-Dialog | ⚠️ |
| Windows-Firewall ohne Prompt, ASIO, Virtual-Cable-Katalog | `VirtualCableDetector` ✅, PortAudio | ✅ |

### G · Oberfläche und Arbeitsplatz

| Zeus | Longpath | Stand |
| --- | --- | --- |
| Panels in Workspaces, Add-Panel-Katalog mit Kategorien (spectrum/vfo/meters/dsp/log/tools/amplifiers/controls/switches), Lock/Close je Panel, **Teilen** | Applet-Gitter, 26 `registerApplet`, Dock/Float, Layouts ✅ | ✅ (Teilen fehlt) |
| Theme-Einstellungen, **hell und dunkel**, 200 % Skalierung Pflicht, Fußleisten-Pins, Pan/Wf-Split, Toolbar-Einstellungen | Themes als Datei (dunkel), UI-Skalierung ✅, **kein heller Modus** | ⚠️ hell fehlt (niedrig für einen Funkraum) |
| Panadapter-/Wasserfall-**Deckkraft** getrennt (2.0.26), Frequenzbänder-Ribbons platzierbar (2.0.24), 3D WebGPU | Ribbons ✅, 3D ✅, Deckkraft? | ⚠️ |
| **Globale Hotkeys** (`GlobalHotkeyBindingsEndpoints`), MIDI-Learn (`MidiLearnFrame`) | `CatMidiControlPage`, wenige `QShortcut` | ❌ Hotkeys fehlen (mittel für CW/Contest) |
| Feature-Katalog mit Ein/Aus je Funktion | View > Applets | ✅ anders |
| Diagnose: Ring-Log, Redaktion, Datei-Sink, Client-Log-Beacon, Audio-Diagnose, TX-Turnaround-Telemetrie | Support-Bundle, Audio-Diagnose-Dialog, PII-Redaktion ✅ | ✅ |

### H · Fern und Mobil

| Zeus | Longpath | Stand |
| --- | --- | --- |
| Mobile Web-App (Android/iOS, Pairing, LAN → automatisch Remote), Remote Access (WebRTC-Plugin), **Wear-OS-Uhr** mit PTT, Mini-Panadapter, S-Meter, Leistung/SWR | TCI-Server, Fernbedienungs-App zurückgestellt | ❌ groß — bewusst offen (siehe Teil 3 §3.2) |

### I · Was Longpath hat und Zeus nicht (im offenen Material)

RTTY-Decoder · ASR/Whisper-Transkription (Zeus' Voyeur-Plugin macht das, aber
als Zusatz) · Voice-Check/Strip-Tuner („Broadcast Optimizer" ist bei Zeus
proprietär) · Rotor (ARCO) · SunSDR-Nativtreiber und TCI-**Client** ·
POTA/SOTA-Spots · KiwiSDR-Wasserfall-OOM-Fixes · SVG-/PNG-Kartenhintergründe ·
Automatisierungsbrücke.

---

## 3. Der Gestaltungs-Maßstab

### 3.1 Was Zeus' Oberfläche tut (aus den Screenshots)

- **Eine Kopfleiste, Gruppen mit Versal-Etikett darüber:** MODE · FILTER ·
  BAND · FAVORITES · STEP · FRONT-END · AGC · SQL · AF · ROGER, jede Gruppe
  drei Schnellwahl-Knöpfe + „···" für mehr; Schieber inline mit Zahl daneben;
  rechts oben `Disconnect`. Longpaths Kopfleiste ist derselbe Gedanke
  (BAND/MODE/FILTER/STEP/NR mit „…") — wir haben ihn im August von dort
  übernommen.
- **Panelkopf überall gleich:** Griff ▌ · Titel in Versalien mit Laufweite ·
  optional Live-Punkt (grün = aktiv) · Zahnrad · Schloss · ✕. Longpath: ⋮⋮ ·
  Titel · ⚙ · ↗ · ✕ — gleich, bis auf das Schloss (Layout sperren).
- **Die Konsole ist beim Start voll:** Logbuch mit Globus und Kennzahlen,
  Bandfilter, Panadapter+Wasserfall, S-Meter (Zeiger **und** Balken), großes
  VFO-Feld, TX-Chain, DSP, TX-Stufenmeter, QRZ-Lookup, Verstärker — Zeus'
  eigene Zeile dazu: „A dense station console that still scans cleanly."
- **Fußleiste als Betriebszustand:** links MOX · VOX · TUNE · PS · Mute ·
  Mic-Pegel · CTUN · SPLIT · RIT · DUP · SAVE MEM; rechts RADIO · Rufzeichen ·
  Chat · Symbole. Longpath: MOX · VOX · TUNE · PS · CWX · DVK · FDX ·
  „Click to connect" · CAT/TCI · CPU · PA · OVERLOAD · ON AIR — sehr nah.
- **Zahlen sind Monospace und farbig nach Bedeutung:** Frequenz und Messwerte
  Bernstein (14.074, −85 dBm, 121 DXCC), Auswahl und Links Blau (Rufzeichen,
  aktive Knöpfe), OK Grün (QRZ ✓, Status OK), TX/Gefahr Rot (TX-Knopf,
  STANDBY-Rahmen, BYPASSED). Genau unsere Regel „Blau ist anfassbar, Warm ist
  gemessen" — mit dem Unterschied, dass Zeus Grün als vierte Bedeutung führt.
- **Großes VFO-Feld** mit gedimmter führender Null (`0 7.240.000`), Hinweis
  „MHz · click to type · wheel on a digit to step" darunter, links die
  Zeilen-Tabs (TX 7.240 / MULTI RX / KIWI), unten drei Icon-Knöpfe (Ton, Schloss,
  TX). Unser RX-Applet zeigt im getrennten Zustand „no radio" und **gar keine
  große Frequenz**.
- **Zeiger-Instrumente als Akzent**, nicht als Stil: S-Meter-Zeiger, Forward
  Power/SWR beim Verstärker und in den TX-Stufenmetern — dunkles Chrom, warmer
  Lichtfleck, alles andere flach. Dazu VU-Säulen (MIC/LEV/ALC, PK/AVG).
- **Leere Zustände erklären sich:** „TYPE A CALLSIGN ABOVE AND PRESS ENTER TO
  LOOK IT UP ON QRZ", „No note recorded", „No tags". Der NR3-Block erklärt
  inline, welches Modell läuft und wo Modelle herkommen (Xiph-Link).
- **Einstellungen als linke Seitenliste** (PA SETTINGS · PURESIGNAL · AUDIO
  TOOLS · DSP · BAND PLAN · QRZ · NETWORK · HOTKEYS · DISPLAY · FEATURES ·
  HAMCLOCK · ZEUS DIGITAL · SERVER · RADIO · RECEIVERS · CALIBRATION · ABOUT),
  Inhalt rechts, Werkzeugfenster (EQ) als schwebendes macOS-Fenster.
- **Feature-Katalog** als Liste mit Icon, Name, Ein-Zeilen-Beschreibung,
  Schalter, Rubrik-Zähler und Suchfeld — der Store als Einstellungsseite.
- **Dichte:** 9–10 px Versal-Etiketten, 11–12 px Text, 13 px Knöpfe, überall
  Haarlinien statt Kästen; Radius klein (4–6 px); Panels ohne Verlauf, nur
  eine Kante heller als der Grund.

### 3.2 Der Stilvertrag, Token für Token

Zeus' öffentlicher Vertrag (CONTRIBUTING §4) verlangt: **fünf Flächen**
(`--bg-0…3`, `--bg-inset`), **vier Textstufen** (`--fg-0…3`), Linien/Panel
(`--line`, `--line-strong`, `--panel-border`, `--panel-top`, `--panel-bot`),
**fünf Bedeutungsfarben** (`--accent`, `--accent-bright`, `--ok`, `--amber`,
`--tx`), zwei Schriften, vier Radien, drei Bewegungs-Tokens — und kein rohes
Hex in Feature-CSS.

`HAUSSTIL.md` hat dieselbe Struktur (Flächen, Text, „jede Farbe genau ein
Job", Instrumentenglimmen, Maße). Unterschiede, die zählen:

| Zeus-Regel | Bei uns |
| --- | --- |
| Kein rohes Hex außerhalb der Tokens | 127 benannte Konstanten, aber noch **63 namenlose Farben / 1 487 Literale** (Drift-Decke) — der Weg dahin ist die ROADMAP |
| Hell **und** dunkel Pflicht | nur dunkel |
| 200 % Skalierung Pflicht, Panels ohne feste Größen | `UiScalePercent` ✅; **feste Breiten** gibt es (siehe 3.3) |
| Sichtbarer Tastaturfokus, zugängliche Namen | nicht geprüft |
| Reduzierte Bewegung respektieren, keine Dauer-Animation | Weiche Übergänge im Hausstil ✅ |
| Loading / empty / error / disconnected / unavailable **ausdrücklich zeigen** | teils („no radio", „Click to connect"), nicht systematisch |
| **Screenshots als Prüfnachweis** in jedem PR (hell/dunkel, schmal, 200 %, alle Zustände) | Regel „live selbst prüfen" (Gedächtnis), nicht als PR-Pflicht |
| `--ok` Grün als vierte Bedeutungsfarbe | Grün nur `kBadgeOkBg` |

### 3.3 Longpath beim ersten Start — gemessen, nicht gefühlt

Bild: [longpath-erststart-1440.png](2026-09-17-zeus-benchmark/longpath-erststart-1440.png)
(Martins Bau 19:46, frisches Profil, 2560×1600, nicht verbunden).

1. **Die rechte Spalte ist zu schmal für ihren Inhalt:** „LAUTSTÄR…",
   „RAUSCHMI…", „NB / SNB /", „ATT 0dE", „A…" sind abgeschnitten; der Text
   wird beschnitten statt umbrochen oder die Spalte breiter gemacht. Das ist
   das Erste, was ein neuer Bediener sieht. (Zeus' Regel: Panels passen sich
   ein, nie feste Breiten.)
2. **Zwei Sprachen auf einem Bildschirm:** BAND · MODE · FILTER · STEP ·
   PANADAPTER · Click to connect · ON AIR neben LAUTSTÄRKE · RAUSCHMINDERUNG
   · VORLAUF · Leistung. Zeus ist durchgehend englisch. Ob deutsch oder
   englisch, ist deine Entscheidung — **eine** muss es sein.
3. **Kein Frequenz-Feld beim Start.** Das RX-Applet zeigt „no radio", aber
   nirgends eine große Frequenz; Zeus zeigt das VFO-Feld immer, mit
   gedimmter Null. Das VFO-Feld ist bei jedem SDR das Zentrum — es fehlt
   als Anker.
4. **Die Konsole ist zu drei Vierteln leer:** ein Wasserfall ohne Daten füllt
   1450×540 px, dazu Panadapter-Gitter ohne Kurve. Zeus füllt den Start mit
   Logbuch, Globus, Meter, VFO, TX-Chain — Dinge, die auch ohne Radio etwas
   zeigen. Für uns hieße das: Logbuch/Karte/QRZ-Lookup im Standardlayout,
   ein S-Meter, das im Leerlauf einen Ruhezustand zeichnet.
5. **Zwei Panel-Kopfstile nebeneinander:** PANADAPTER hat Griff ▌ + grünen
   Punkt + Titel (Zeus-Muster), RX/TX/PHONE·CW haben ⋮⋮ + Titel + ⚙ + ↗ + ✕.
   Ein Muster.
6. **Die Fußleiste mischt Zustände und Knöpfe** (MOX/VOX/TUNE/PS anklickbar,
   CWX/DVK/FDX grau, „Click to connect" als Text, PA ✓, OVERLOAD, ON AIR) —
   Zeus trennt: links Schalter, rechts Status. Bei uns ist „Click to connect"
   der einzige Weg zum Radio und sieht wie ein Etikett aus.
7. **Knopfhöhe und Radius** sind bei uns größer (≈ 36 px, Radius 8) als bei
   Zeus (≈ 26 px, Radius 4); dadurch trägt die Kopfleiste weniger Gruppen in
   derselben Breite (5 statt 10). Das ist Geschmack — aber es erklärt, warum
   AGC/SQL/AF bei uns nicht in der Kopfleiste sind.

Nicht vergleichbar aus diesem Bild: der verbundene Zustand (Meter, Kurven,
Wasserfall) — dafür hätte ich Martins Radio belegen müssen.

### 3.4 Und „Glas & Tiefe"?

Zeus ist flach, haarlinienfein, dicht. Die heute gewählte Richtung 3 ist
bewusst plastischer (Verläufe, Lichtkanten, versenkte Rinnen, Messing-MOX).
Das ist kein Widerspruch zum Maßstab: **Zeus ist der Maßstab für Dichte,
Konsistenz, Zustände und Vollständigkeit, nicht für die Oberflächenhaptik.**
Die sieben Punkte in 3.3 gelten in jeder Richtung; keiner davon ist eine
Stilfrage.

---

## 4. Andere Referenzen, kurz

Nur wo sie Zeus in einem Punkt schlagen: **SmartSDR** (Flex) für Slice-Flags
und Profile (Global/TX/Mic — unser Profilsystem ist näher an Flex als an
Zeus); **SDR Console** für Recorder/Analyser-Werkzeuge; **ExpertSDR3** als
TCI-Heimat (unser TCI-Client zielt darauf); **Thetis** für die Tiefe der
DSP-Optionen (unsere Zitierquelle); **AetherSDR** als Qt-Vorbild (3DSS,
RTTY, Automation, Kiwi kamen von dort); **deskHPSDR** für den WDSP-2.1-Sync
(Teil 2). Keine davon verschiebt die Liste unten.

---

## 5. Die Liste, nach Wert für Longpath

| # | Was | Größe | Warum |
| --- | --- | --- | --- |
| 1 | **Erststart in Ordnung bringen** (3.3 Punkte 1–3, 5): Spalte passt, eine Sprache, VFO-Feld immer da, ein Panelkopf | Tage | Der erste Eindruck; alles Gestaltung, nichts Technik; passt in „Glas & Tiefe" |
| 2 | **Standardlayout füllen** (3.3 Punkt 4): Logbuch + Karte + S-Meter im Start-Arbeitsplatz | Tage | Zeus' Konsole wirkt vollständig, weil sie vollständig *startet* |
| 3 | **CW-Decoder** aus dem GPL-Engine portieren (`CwDecoder/`, Goertzel + adaptive Schwelle + FSM), als Applet neben dem RTTY-Decoder, mit Geschwindigkeit/SNR | Tage | Einzige nennenswerte Betriebsfunktion, die offen vorliegt und uns fehlt |
| 4 | **Logbuch-Kennzahlen + Awards** (Bänder/Modi/Aktivität/Top-Länder, DXCC/WAS/Grid-Zähler aus dem eigenen Log) | Woche | Sichtbarster Unterschied im Logbuch; Daten haben wir |
| 5 | **RX-Leveler** (Zeus' Design: Beweis-gebunden, ADC-Veto) | Tage | Löst „AGC hebt Rauschen an" — Betriebsnutzen |
| 6 | **AU-Effekte** in der TX-Kette (Teil 1 Mitnahme 1) | Woche+ | Die eine Funktionslücke, die Bediener nennen |
| 7 | Globale Hotkeys · Frequenzspeicher · RX-Profile · Layout-Export · SNR im S-Meter · Roger-Beep · serieller PTT | je Tag | Kleinkram, der Zeus „fertig" wirken lässt |
| 8 | **Release-Kadenz**: 0.6.3 raus, dann Notizen je Release wie Zeus | — | Der Maßstab, der am meisten zählt und nichts kostet |
| 9 | Fernbedienung (Web/Uhr), FT8 nativ, Satelliten, APRS, Winlink, Chat | Monate | Bewusst nicht jetzt; FT8/APRS/Winlink sind Zeus' Produktstrategie |

Was ich nicht übernehmen würde, auch nicht als Maßstab: Abo-Gating, Store,
ZeusChat, GodsEye-Kameras/Schiffe/Flugzeuge, Ultra-Auflösungsfilter.

Alles hier ist beobachtet und aus GPL-Quelltext gelesen; kein proprietärer
Zeus-Code wurde angefasst. Die drei Fixes von heute (#14, #15, #16) sind die
ersten Ergebnisse dieses Maßstabs.
