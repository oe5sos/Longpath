# Zeus-Plugin-System: Inventar und Abgleich mit Longpath

**Stand:** 17. September 2026
**Quelle:** `../zeus-station-engine` @ `324e865` (Release v2.0.19,
GPL-2.0-or-later / GPL-3.0), Projekte `Zeus.Plugins.Contracts` (1.170
Zeilen), `Zeus.Plugins.Host` (7.956 Zeilen), Einschleifpunkte in
`Zeus.Dsp/Wdsp/WdspDspEngine.cs` und
`Station.Engine.Hosting/DspPipelineService.cs`; dazu
`github.com/Zeus-SDR/zeus-community-features` (SDK-Schnappschuss,
Hello-World-Vorlage, `registry.json` vom 17.09.2026, 21 Einträge).
**Anlass:** „genau analysieren … inkl. plugin … nachbauen" — erster
Schritt des Engine-Inventars, wie am 17.09. abgestimmt (Plugin-System
zuerst, dann WDSP-Abweichungen, dann Stationsprotokoll).

Was hier steht, stammt ausschließlich aus GPL-Quelltext und öffentlicher
Dokumentation. Der proprietäre Zeus-Client (zeus-web / ZeusProduct) wurde
nicht angefasst; wo er eine Rolle spielt, steht es ausdrücklich dabei.

---

## 1. Quellenlage: was offen ist und was nicht

| Baustein | Lizenz | Liegt vor? |
| --- | --- | --- |
| Plugin-Verträge (`Zeus.Plugins.Contracts`, 7 Erweiterungs-Schnittstellen, Manifest, Registry-Typen) | GPL-2.0-or-later | ✅ vollständig, zweimal (Engine + SDK-Schnappschuss, bis auf einen Kommentar identisch) |
| Plugin-Host (Loader, Manager, Settings-Store, Installer, Registry-Client, Id-Migration, AudioChain, VST/AU-Scanner + -Controller) | GPL-2.0-or-later | ✅ vollständig |
| Einschleifpunkte in DSP/Engine (`SetTxAudioPluginHandler`, `SetRxAudioPluginHandler`, `IAudioModemPort`, Taps, Monitor-Inject) | GPL | ✅ vollständig |
| `Station.AudioRing` (Außer-Prozess-Audioring, shm) | **MIT** | ✅ vollständig, 960 Zeilen |
| **`AudioPluginBridge`** — die Klasse, die `AudioChain` an die WDSP-Naht hängt, Taps fächert, Preview fährt | — | ❌ nur in Kommentaren erwähnt, sitzt im proprietären Produkt-Host |
| **VST3-/AU-Bridge-Binaries** (JUCE-basierter Außer-Prozess-Engine, `zeus-au-bridge`) | proprietär | ❌ ausdrücklich nicht Teil des Repos; offener Baum baut gegen `Zeus.Plugins.VstHostStub` |
| Web-Oberfläche (React, `registerPanel`, Panels, Features-Store) | proprietär | ❌ nur der Vertrag ist dokumentiert (CONTRIBUTING.md §3–4) |
| Die 21 Plugins der Registry (EQ, Compressor, FT8, FreeDV, Logbook, Recorder, …) | **alle GPL** (2.0-or-later bzw. 3.0-only) | ⚠️ nur als kompilierte Zips auf `downloads.zeussdr.com`; **kein Quell-Repo** in der Zeus-SDR-Org. GPL verpflichtet zur Quellabgabe an Empfänger — auf Anfrage erhältlich, aber nicht öffentlich. |

Kurz: der **Mechanismus** ist komplett offen, die **Anbindung** an den
Produkt-Host und die **Inhalte** (Plugins, Bridges, Web-UI) nicht.

---

## 2. So ist das Zeus-Plugin-System gebaut

### 2.1 Ein Paket auf der Platte

```
~/Library/Application Support/Zeus/plugins/<id>/     (macOS; Windows %APPDATA%\Zeus\plugins; Linux $XDG_DATA_HOME/zeus/plugins)
├── plugin.json          Manifest, schemaVersion 1
├── <Entry>.dll          .NET-10-Assembly mit genau einer öffentlichen IZeusPlugin-Klasse
├── ui/*.js              optional: ES-Module für die Web-Oberfläche
└── *.vst3               optional: mitgeliefertes VST3
```

Manifest-Felder: `id` (`^[a-z][a-z0-9.]*[a-z0-9]$`, reverse-DNS), `name`,
`version` (SemVer), `author`, `license`, `homepage`, `sdk {abi, minVersion}`,
`entrypoint {assembly, type?}`, `capabilities[]`, `permissions {network,
fileSystemRead, fileSystemWrite}`, `ui {modules[], panels[{id, title, icon,
slot, category}]}`, `audio {format vst3|au, vst3Path|auComponentId, vst3Uid,
slot, channels, sampleRate}`.
ABI-Prüfung: `sdk.abi == 1` **und** `minVersion.Major == Host.Major` **und**
`minVersion <= Host` (Host: ABI 1, SDK 1.5.0). Sonst wird nicht geladen.

### 2.2 Lebenszyklus (PluginLoader → PluginManager → PluginContext)

1. **Start:** verzögerte Deinstallationen abschließen (`.pending-delete`-
   Marker, weil Windows DLLs im ALC sperrt) → Id-Migrationen (60 s Deckel)
   → jedes Verzeichnis mit `plugin.json` katalogisieren → aktivieren, außer
   „gescannte" VST/AU-Plugins (die nur bei persistierter Kettenmitgliedschaft).
   `SafeMode` überspringt alles.
2. **Laden:** Manifest lesen + validieren + ABI prüfen → **Schattenkopie**
   des ganzen Plugin-Ordners nach `$TMP/zeus-plugin-shadow/<pid>-<startticks>/`
   (damit der Installer das Paket atomar ersetzen kann, während das alte noch
   gemappt ist; verwaiste Schattenordner toter Prozesse werden beim ersten
   Loader-Bau gefegt, PID-Wiederverwendung über Startzeit abgesichert) →
   eigener **einsammelbarer `AssemblyLoadContext`** je Plugin
   (`Zeus.Plugins.Contracts`, `System.*`, `Microsoft.*` kommen aus dem
   Default-Kontext, damit Typidentität stimmt; alles andere privat aufgelöst)
   → Typ per Reflection (`entrypoint.type` oder erstes öffentliches
   `IZeusPlugin`) → parameterloser Konstruktor.
3. **Aktivieren:** `PluginContext` bauen (Slots je nach Capability genullt)
   → `InitializeAsync(ctx, ct)` mit **10 s Timeout**; wirft es, wird entladen
   und der Fehler geworfen. Danach Ereignis `PluginActivated` (Abonnenten
   laufen synchron; ihre Ausnahmen werden geschluckt).
4. **Deaktivieren:** erst aus der Aktiv-Tabelle nehmen, dann
   `PluginDeactivated` (Host gibt Ketten-Slots, Routen frei, **solange die
   Instanz noch lebt**) → `ShutdownAsync` mit **5 s Timeout** → ALC entladen →
   Schattenordner löschen.
5. **Rehydrieren:** `RehydratePluginSettingsAsync` ruft `InitializeAsync`
   ein zweites Mal **mit demselben Kontext**, damit ein Plugin nach einem
   Profilwechsel seine Einstellungen neu liest; scheitert es, bleibt die
   alte Instanz aktiv (ein lebendes TX-Plugin darf nie aus der Kette fallen).
6. Je Plugin-Id ein `SemaphoreSlim` (`PluginOperationGate`), damit
   Installieren/Deinstallieren pro Plugin seriell, über Plugins hinweg
   parallel läuft.

### 2.3 Capabilities — Deklaration, keine Sandbox

Sieben Flags: `ReadRadioState`, `ControlRadio`, `AudioStream`,
`NetworkAccess`, `FileSystemRead`, `FileSystemWrite`, `PersistSettings`
(implizit immer). ABI 1 **gewährt jede deklarierte Capability automatisch**;
nicht deklarierte Dienste sind im Kontext einfach `null`. Der Code sagt es
selbst: „not a security sandbox or an operator-consent boundary". Der
Community-Vertrag ergänzt: PureSignal ist tabu, kein Plugin darf selbst
tasten („must never auto-key a transmitter").

### 2.4 Was der Kontext einem Plugin gibt (`IPluginContext`)

| Slot | Bedingung | Inhalt |
| --- | --- | --- |
| `Logger` | immer | mit Plugin-Id getaggt |
| `Settings` | immer | Schlüssel/Wert, LiteDB, **eine Collection je Plugin-Id** in derselben `zeus-prefs.db` — Plugins sehen einander nicht |
| `PluginRootPath`, `HostDataDirectory` | immer | eigener Ordner (dort ohne FS-Capability les-/schreibbar); Datenordner mit `zeus-logbook.db` |
| `Radio` (`IRadioStateReader`) | `ReadRadioState` | `FrequencyHz`, `Mode`, `Band`, `Mox` + drei Ereignisse; wickelt `RadioService`-Snapshot |
| `RadioController` | `ControlRadio` | `SetFrequencyAsync`, `SetModeAsync`, `SetMoxAsync` — **mehr nicht** (kein Filter, kein AGC, keine Slices) |
| `Playback` (`IAudioPlaybackSink`) | Host-abhängig | `PlayLocal` (in den RX-Bus gemischt, SPSC-Ring, Rückgabe `false` = voll, **denselben Block wiederholen**), `PlayOnAir` (in die TX-Kette, nur hörbar während Bediener MOX hält — **tastet nie**), `BeginLocalMonitor()`-Token |
| `Qrz` (`IQrzLookup`) | `NetworkAccess` + QRZ konfiguriert | Host-vermittelt mit **den QRZ-Zugangsdaten des Bedieners** und gemeinsamem Rate-Limit; wirft nie |
| `OperatorIdentity` | immer | Rufzeichen/Locator mit Host-Vorrangregel (Override → Plugin-Fallback → QRZ-Heimat) |

### 2.5 Die sieben Erweiterungs-Schnittstellen

| Schnittstelle | Wofür | Realtime-Vertrag |
| --- | --- | --- |
| `IBackendPlugin.MapEndpoints` | eigene HTTP-Routen unter `/api/plugins/<id>/…` | Control-Thread; läuft über **eine veränderliche `EndpointDataSource`**, damit Installieren zur Laufzeit sofort greift (früher 404 bis Neustart) |
| `IUiPlugin.ShouldLoadUi` | UI nur bei Bedarf laden | einmal nach Init |
| `IAudioPlugin` | Einschub-Stufe in der TX- **oder** RX-Kette (Slot aus Manifest: `tx.post-leveler`, `tx.pre-cfc`, `rx.post-demod`) | `Process(in, out, ctx)` auf dem Audio-Faden: **kein alloc, kein Lock, kein IO, kein Throw**; `InitializeAudioAsync` 1 s Timeout; Anforderungen (Rate/Kanäle/Blockgröße) werden geprüft, sonst nicht geladen |
| `IRxAudioTapPlugin` | nur **mithören** nach Demod (Recorder, Decoder, Meter) | `OnRxAudio(samples, ctx)` 48 kHz mono float32, Puffer nur während des Aufrufs gültig; `ctx.Receiver` 0 = Haupt-RX, 1+ = Sub-RX |
| `ITxAudioTapPlugin` | mithören TX: `OnTxMicAudio` (rohes Mikrofon, auch ohne MOX) und `OnTxAirAudio` (**aus dem TX-IQ zurückdemoduliert** — was wirklich rausgeht; nur bei laufendem TX-Monitor) | wie oben |
| `IAudioModemPlugin` | modusgebundenes Modem (FreeDV): `SyncMode(byte)`, `ProcessRx/Tx` in place, `FlushRx/Tx`, `FinishTx`, `DrainTx` (Schwanz-Drain über eigenen Worker) | reentrant von mehreren Fäden; Host besitzt die Modusauswahl |
| `ILogbookPlugin` (V1/V2/V3) | **das Logbuch ist selbst ein Plugin**: CRUD, Seiten, Worked-Before-Zusammenfassung, ADIF-Import/Export, QRZ-Upload-Status, QSL/LoTW-Status, Tags, Lat/Lon (V3) | async, Host hält die stabile `/api/log/*`-Schnittstelle |

`AudioBlockContext` (ref struct): `SampleRate`, `Channels`, `Frames`,
`SampleTime` (monotoner Zähler), **`Mox`** (false = Vorschau-Lauf nur für
Meter, z. B. Mikrofon ohne MOX — Plugins, die bei TX etwas Bleibendes tun,
müssen darauf verzweigen), `Receiver`.

### 2.6 Einschleifpunkte und die `AudioChain`

**TX** (`WdspDspEngine.ProcessTxBlock`): eine `volatile`-Delegatreferenz
wird **vor** `fexchange2` gelesen; Plugins sehen Mikrofon mono float32 bei
48 kHz in TXA-Blockgröße. Bei digitalem TX, Roger-Beep oder injiziertem
Audio wird der Handler übersprungen, aber installiert gelassen. Wirft der
Handler, fällt der Block auf Durchreichung zurück (4 Warnungen, dann
Stille im Log). Ohne Handler ist der Pfad bit-identisch zu „keine Plugins".

**RX** (`DspPipelineService`, Haupt-RX): Reihenfolge nach WDSP-Demod →
MOX-Fade → **Modem** (`IAudioModemPort.ProcessRx`, FreeDV; AF-Gain danach,
weil WDSPs Panel-Gain auf dem verworfenen Modemsignal lief) → Produkt-Insert
→ **RX-Plugin-Handler** (`rx.post-demod`, z. B. CW-SCAF) → adaptiver Squelch
→ RX-Leveler → **Seitenton-Mischung** (nach dem Plugin, damit der Filter den
Seitenton nicht verformt) → **Monitor-Inject** (Recorder-Wiedergabe) →
Limiter → Post-TX-Fade → Kiwi-Mischung (Kiwi umgeht Plugins/Squelch/Fades
ganz) → Veröffentlichung + Tap-Fan-out. Die TX-Kette ist **bewusst
TX-only** (Bedienerentscheidung 30.04.2026): RX-Plugins sind eine
getrennte Kette mit eigenen Instanzen, kein geteilter IIR-Zustand.

**`AudioChain`** (8 Slots, seriell): Ping-Pong zwischen Ausgabe und
Scratch-Puffer ohne Allokation; **Master-Bypass** = ein `memcpy`
(bit-identisch, Slot-Bypass-Zustände bleiben erhalten); Slot-Bypass;
**NaN/Inf-Reparatur nach jedem Slot** (auf 0 gesetzt, gezählt); In-/Out-
Peak-Meter mit 750-ms-Frische; Telemetrie (letzte/maximale Blockdauer in
µs, Blockzähler, Reparaturzähler); `DrainProcessing()` wartet per SpinWait,
bis kein Block mehr den alten Slot-Graphen sieht, **bevor** ein entferntes
Plugin heruntergefahren wird; zweite `Process`-Überladung mit fremdem
Scratch für den Pre-MOX-Vorschaupfad, damit sich Live-TX und Vorschau am
MOX-Rand nicht denselben Puffer teilen.

### 2.7 VST3/AU-Hosting

Manifest-`audio`-Block genügt, das Plugin braucht kein C#: der Host wickelt
das VST in ein synthetisches `IAudioPlugin` (`VstHostAudioPlugin`). VST3
läuft in einem **eigenen JUCE-basierten Engine-Prozess** (Supervisor startet
neu, „dead-man's pedal" setzt abstürzende Plugins auf eine Blacklist,
Shell-VSTs wie WaveShell werden in Sub-Plugins expandiert, `vst3Uid` wählt
eines). AU (macOS) über eine In-Prozess-Bridge und die AudioComponent-
Registry (`aufx:lpas:appl`). Beide Scanner (`VstDirectoryScanService`,
`AuComponentScanService`) erzeugen je gefundenem Effekt ein **generiertes
Paket** (Stub-Assembly + synthetisiertes Manifest) in den Namensräumen
`com.openhpsdr.zeus.{vst,rxvst,au,rxau}.*`, die die Plugin-Liste ausblendet.
Die Bridge-Binaries selbst sind proprietär.

### 2.8 Der Außer-Prozess-Weg: `Station.AudioRing` (MIT) + `ProductPluginAudioPort`

Ein zweiter, ganz anderer Plugin-Mechanismus für **fremde Prozesse**
(FT8-Engine, Sequencer, „Wave-P2"): Shared-Memory-Ring, 48 kHz float32,
960-Sample-Blöcke (20 ms), 32 Slots je Richtung, Magic `ZARG`, Version 4,
Cross-Process-Signale. Darüber ein **Lease-Modell** mit HTTP-Endpunkten
(`/api/station/product-audio/attach`, `…/rx-audio/lease/{id}`,
`…/tx-audio/lease/{id}`, `…/tx/lease`, `…/key/release`): Capture ist eine
nicht blockierende Kopie; Injektion und **Tastung** verlangen zusätzlich
einen im Speicher gehaltenen Bediener-„Arm", der mit dem Lease stirbt;
Heartbeat 100 ms; ein getasteter Produzent darf **12 Blöcke (240 ms)**
Unterlauf verpassen (Stille wird eingesetzt, RF bleibt kontinuierlich),
danach wird er als tot widerrufen. Anlass laut Kommentar: FT8-TX brach am
23.07.2026 nach ~8 s ab, zweimal.

### 2.9 Registry, Installer, Store

`registry.json` (GitHub raw, Zeus-SDR/zeus-community-features) → Einträge
mit `versions[{version, sdkAbi, sdkMinVersion, platforms, downloadUrl,
sha256}]`, `channel` (official/community), `verified`, **`subscription
{required, monthlyPriceCents, checkoutUrl}`**. Installer: Zip laden,
SHA-256 prüfen, eingebettetes `plugin.json` **strikt** validieren (kein
absoluter `vst3Path`, kein `..`), atomar entpacken (`.backup-`-Ordner),
aktivieren; Deinstallation verzögert per Marker, wenn DLLs gesperrt.
Install/Uninstall-Routen sind **nur von Loopback** erlaubt (403 sonst);
`402 Payment Required`, wenn ein `IPluginInstallAccessGate` das Abo
verneint. Pull-Request-basierter „Custody"-Workflow: Maintainer laden das
Zip einmal, prüfen Größe/Hash und hosten es selbst auf
`downloads.zeussdr.com`.

### 2.10 UI-Vertrag (Web, proprietäre Seite)

`ui/*.js` als ESM mit externalisiertem React; Default-Export bekommt genau
`{ registerPanel({id, component}), callBackend(method, path, body) }`.
Drei Slots: `workspace.<feature>` (vom Bediener hinzufügbares Panel),
`tx-audio-tools.chain`, `rx-audio-tools.chain`. Kategorien spiegeln das
Add-Panel-Modal (spectrum/vfo/meters/dsp/log/tools/amplifiers/controls/
switches/plugins). Styling über CSS-Tokens (`--line`, `--panel-border`, …),
Pflicht-Screenshots hell/dunkel.

### 2.11 REST-Oberfläche des Plugin-Systems

`GET /api/plugins` (Katalog + ABI/SDK), `GET /api/plugins/{id}`,
`GET /api/plugins/registry`, `POST /api/plugins/install` (Quellen
`zip-url`, `url`, `file`, `registry`), `POST /api/plugins/install/zip`
(Multipart), `DELETE /api/plugins/{id}`, `GET /api/plugins/{id}/ui/{*}`
(Cache-Control no-store), Plugin-eigene Routen `/api/plugins/{id}/…`.

---

## 3. Das Ökosystem: 21 Plugins, und was Longpath davon schon hat

| Plugin (Registry-Id) | Zweck | Lizenz | Bei uns |
| --- | --- | --- | --- |
| `…samples.eq` | 10-Band-Parametric-EQ mit Kurve | GPL-2 | ✅ `ClientEq` (Strip) |
| `…samples.compressor` | VCA-Kompressor (Thr/Ratio/Att/Rel/Knee/Makeup) | GPL-2 | ✅ `ClientComp` |
| `…samples.noisegate` | Peak-Gate mit 3-dB-Hysterese | GPL-2 | ✅ `ClientGate` |
| `…samples.reverb` | Schroeder (4 Comb / 2 Allpass, Damping) | GPL-2 | ✅ `ClientReverb` |
| `…samples.exciter` | Aphex-Exciter (HP → tanh → gerade Harmonische) | GPL-2 | ⚠️ teilweise: `ClientTube` (Sättigung), `ClientPudu`; kein reiner HP-Exciter |
| `…samples.bass` | MaxxBass-artige fehlende Grundwelle | GPL-2 | ❌ — für SSB-Sprechfunk fraglich |
| `org.openhpsdr.freedv` | FreeDV-Modem (aus dem Kern ausgelagert) | GPL-2 | ✅ RADE_U/RADE_L nativ in `RxChannel`/`TxChannel` |
| `org.openhpsdr.digital` | nativer FT8/FT4-Engine (aus dem Kern ausgelagert) | GPL-2 | ⚠️ anderer Weg: WSJT-X über VAX + CAT/TCI; `DigitalApplet`, PSK-Reporter |
| `org.openhpsdr.logbook` | Logbuch-Backend (ADIF, QRZ-Upload, QSL/LoTW) | GPL-2 | ✅ `LogbookWindow`, ADIF, `WorkedBefore`, `QrzClient` — LoTW-Status? (prüfen) |
| `org.openhpsdr.recorder` | RX/TX-WAV-Recorder mit lokaler Wiedergabe | GPL-2 | ✅ `QsoRecorder`, `WavRecorder`, `WavPlayer`, `VoiceKeyer` |
| `org.openhpsdr.voyeur` / `com.kb2uka.voyeur` | Netz-Monitor: parkt auf Frequenz, loggt jeden Durchgang, **transkribiert** | GPL-2 | ⚠️ Bausteine da (`AsrService`/Whisper, Recorder, Log), kein „Parken + je Durchgang loggen"-Automat |
| `org.openhpsdr.netlogger` | NetLogger-XML-API-Browser (US-Netze) | GPL-2 | ❌ — für OE5 kaum relevant |
| `…plugins.pgxl`, `…plugins.tgxl` | 4O3A PGXL / TGXL | GPL-3 | ✅ `PgxlConnection`, `TgxlConnection`, `AmpApplet` |
| `…plugins.rf2k` | RF2K-S | GPL-2 | ✅ |
| `org.openhpsdr.kpa500`, `org.openhpsdr.vk3amp` | Elecraft KPA500, VK-AMPS | GPL-2 | ❌ — keine Hardware hier |
| `…plugins.antennagenius` | 4O3A Antenna Genius (LAN) | GPL-3 | ❌ — keine Hardware hier |
| `com.zeussdr.plugins.remoteaccess` | WebRTC-Fernzugriff | GPL-2 | ❌ — eigenes Großthema, nicht Plugin-Frage |
| `…plugins.localsignon` | Offline-Konto/Abo-Entitlement, 4,99 USD/Monat | GPL-2 | — kein Bedarf (kommerzielles Gating) |

Lesart: die „Auslagerung von FreeDV und FT8 in Plugins" ist Zeus'
Store-Strategie, kein Können, das uns fehlt. Der Store selbst (Abo, Custody,
Verified, Id-Migration) ist Produktinfrastruktur.

---

## 4. Abgleich Baustein für Baustein

| Zeus | Bei uns heute | Lücke? |
| --- | --- | --- |
| TX-Einschub vor WDSP, 8 Slots, Master-/Slot-Bypass, In/Out-Peak | `StripChain` in `TxWorkerThread::dispatchOneBlock` vor `fexchange0`: 8 **feste** Stufen (Gate, EQ, DeEss, Comp, Tube, Pudu, Reverb, Limiter), Master + je Stufe, In/Out-Peak | strukturell gleich; Stufen sind kompiliert statt geladen |
| NaN/Inf-Reparatur nach jedem Slot, Reparaturzähler | **nichts** — `StripChain.cpp`/`ClientFinalLimiter.cpp` kennen kein `isfinite` | ✳ klein, konkret: ein NaN aus einer Stufe wandert in WDSP/PureSignal |
| Ketten-Telemetrie (µs je Block, Maximum) | nichts | niedrig |
| `ctx.Mox` (Vorschau-Lauf ohne Senden) | Off-Air-Monitor (`TxChannel::setOffAirMonitor`) + Voice-Check-Taps pre/post Strip | ✅ gelöst, anders |
| `DrainProcessing()` vor Slot-Abbau | Stufen sind statisch; Tap-Abbau hat eigene Quiescence-Zähler (Race-Fix 06.09.) | ✅ |
| RX-Taps (generischer Fan-out, `Receiver`-Index) | vier **benannte** Tap-Slots (`m_qsoTap`, `m_asrTap`, `m_wavRecordTap`, `m_rttyTap`), je Slice | ✅ gleichwertig für unsere Zwecke; ein 5. Verbraucher heißt ein 5. Slot |
| TX-Taps: Mikrofon roh + „Air" aus TX-IQ | `preStripAudioReady`/`postStripAudioReady` + Off-Air-Monitor | ✅ |
| **RX-Einschub in place** (`rx.post-demod`), getrennte RX-Kette | RADE fest verdrahtet; **kein generischer In-place-Einschub** nach Demod (z. B. CW-SCAF, RX-EQ, AU-Effekt) | ✳ mittel — Kandidat |
| Modem-Naht (`IAudioModemPort`: SyncMode, Flush an MOX-Kanten, Tail-Drain) | RADE nativ in RX/TX-Channel | ✅ gleichwertig, keine zweite Modem-Art in Sicht |
| **VST3/AU in der TX-/RX-Kette** | nichts (`grep VST3\|AudioUnit` → 0 Dateien) | ✳ echte Funktionslücke; Zeus' Bridge ist proprietär → **nichts zu portieren, nur nachzubauen** |
| Monitor-Inject (Plugin spielt lokal in den RX-Bus) | `WavPlayer`/`MasterMixer` | ✅ |
| `PlayOnAir` ohne Tastung | `VoiceKeyer`/DVK | ✅ |
| Außer-Prozess-Audio mit Lease/Arm/Key (`AudioRing`, MIT) | VAX (CoreAudio-HAL-Treiber + shm, 4 virtuelle Eingänge, 1 Ausgang) + CAT/TCI-Tastung | ✅ anderer, standardkonformer Weg (WSJT-X & Co.); AudioRing nur relevant, wenn wir je eigene Begleitprozesse bauen |
| Plugin-Settings je Id (LiteDB) | `AppSettings`/QSettings je Applet | ✅ |
| Logbuch als austauschbares Backend | fest eingebaut | kein Bedarf |
| HTTP-Routen je Plugin, UI-Panels per ESM | Applet-Gitter (`registerApplet`, 26 Aufrufe), C++ | Architektur grundverschieden (siehe 5) |
| QRZ-Vermittlung mit Bediener-Zugang + Rate-Limit | `QrzClient` | ✅ |
| Registry/Installer/Store/Abo | — | kein Bedarf |

---

## 5. Bewertung

**Ein Zeus-artiges In-Prozess-Plugin-System ist für Longpath kein
Portierziel.** Zeus ist ein headless .NET-Engine mit HTTP/WebSocket-
Protokoll und einem React-Client; Plugins sind .NET-Assemblies mit
Reflection-Laden, einsammelbaren Load-Contexts, ASP.NET-Routen und
ES-Modulen — das gesamte Trägersystem existiert bei uns nicht. In C++/Qt
hieße das Äquivalent `QPluginLoader` mit einer stabilen C-ABI, Symbol-
Versionierung, ABI-Bruch bei jedem Qt-/Compiler-Update, und einem zweiten
Bausystem für Plugin-Autoren — für ein Ein-Bediener-Projekt ohne Store ein
Trägersystem ohne Fracht. Das Hauptmotiv von Zeus (Store, Abos,
Entitlements, Community-Einreichungen) trifft uns nicht.

**Was sich trotzdem mitnehmen lässt** — drei konkrete Punkte, in
Reihenfolge des Nutzens:

1. **AU-Effekte in der TX-Kette (macOS)** — die einzige echte
   *Funktions*lücke gegenüber Zeus, die Bediener spüren („mein
   Lieblings-Kompressor-Plugin in den Sendezweig"). Zeus' Bridge ist
   proprietär, also Eigenbau: AudioToolbox `AudioComponentFindNext`
   (`aufx`), `AudioUnitRender` mit Pull-Callback aus unserem Block, als
   **neunte, optionale StripChain-Stufe** mit denselben Regeln (kein alloc
   im Callback, Bypass bit-identisch). Passt zu „nur Mac"; VST3 später
   über das VST3-SDK (GPLv3-kompatibel), falls je Windows.
2. **Generischer RX-Einschub nach Demod** — ein `rxInsert`-Slot in
   `RxChannel` an derselben Stelle wie Zeus (nach Demod/Modem, **vor**
   Squelch und Seitenton), heute leer. Erster Nutzer: RX-EQ oder ein AU-
   Effekt auf dem Empfangsweg; zweiter: CW-SCAF. Kleiner Eingriff, aber
   ein Design-Punkt (wo im Pfad, je Slice oder global) — gehört auf die
   ROADMAP, nicht in einen Alleingang.
3. **NaN/Inf-Guard nach der StripChain** (und Zähler ins Log/Diagnose) —
   eine Stunde Arbeit, verhindert, dass eine defekte Stufe die TX-Kette
   inkl. PureSignal vergiftet. Ohne Rückfrage machbar.

Nicht übernehmen: Registry/Installer/Store, Capabilities-Modell, Id-
Migrationen, Web-UI-Vertrag, AudioRing (VAX deckt es ab), FT8-Engine im
Prozess (WSJT-X-Weg bleibt), NetLogger, Entitlement-Plugins.

**Wenn die GPL-Plugins selbst interessant werden** (z. B. der Voyeur-
Automat „parken, jeden Durchgang loggen, transkribieren" als Vorlage für
unsere ASR-Bausteine): die Quellen sind nicht veröffentlicht, aber die
Zips sind GPL — die Maintainer (KB2UKA/N9WAR) sind zur Quellabgabe
verpflichtet; eine höfliche Anfrage im `zeus-community-features`-Repo ist
der saubere Weg, nicht der Decompiler.

---

## 6. Nächste Schritte

- [ ] Punkt 3 (NaN-Guard) einbauen, sobald der Baum frei ist — eigener
      Worktree, Regressionstest gegen einen NaN-liefernden Stub.
- [ ] Punkte 1 und 2 als Einträge auf `ROADMAP.md` vorschlagen; Martin
      entscheidet Reihenfolge.
- [ ] Inventar Teil 2: **WDSP-Abweichungen** in `native/wdsp` (Zeus-Fork
      mit libspecbleach + RNNoise) gegen `third_party/wdsp` — was ist
      dort wirklich anders als bei Warren Pratt / Thetis.
- [ ] Inventar Teil 3: **Stationsprotokoll v1** (HTTP/WebSocket, 296
      Routen) — nur die Teile, die für TCI-/Fernbedienungs-Ideen taugen.
