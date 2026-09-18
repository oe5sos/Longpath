# Zeus' Stationsprotokoll v1: Inventar und Abgleich mit Longpath (Teil 3)

**Stand:** 17. September 2026
**Quelle:** `../zeus-station-engine` @ `324e865` (v2.0.19, GPL):
`Station.Engine.Hosting/StationEngineEndpoints.cs`,
`StationProtocolEndpoints.cs`, `StreamingHub.cs` (1 401 Zeilen, `/ws`),
`Zeus.Contracts` (8 856 Zeilen DTOs und Frame-Layouts), `StationEngine/
Program.cs` (Bindung, Token, CORS); dazu das README von
`Zeus-SDR/zeus-watch` (Wear-OS-Client) für die Sicht eines dünnen Clients.
**Anlass:** dritter und letzter Teil des Zeus-Engine-Inventars, nach
[Teil 1](2026-09-17-zeus-plugin-system-inventar.md) (Plugin-System) und
[Teil 2](2026-09-17-zeus-wdsp-fork-inventar.md) (WDSP-Fork).

---

## 1. Was das Protokoll ist — und was nicht

Zeus ist zweigeteilt: ein headless **Engine-Prozess** (offen) und ein
**Web-Client** (React, proprietär), die über Loopback reden. Das
„Stationsprotokoll v1" ist diese Naht: **REST für Steuerung, ein
WebSocket für Ströme und Zustand.** Es ist kein Interop-Standard wie TCI
oder CAT — für fremde Programme bietet Zeus dieselben Schnittstellen wie wir
(`app.UseTciServer(...)`, `/api/tci/*`, `/api/cat/*`).

**Umfang:** 325 HTTP-Routen (eine `Map*`-Zählung über den offenen Baum),
davon `/api/tx` 45, `/api/radio` 40, `/api/station` 24, `/api/rx` 22,
`/api/godseye` 15 (Satelliten/Feeds), `/api/bands` 14, `/api/vna` 11,
`/api/contest-log` 8, `/api/filter` 7, `/api/audio` 7, `/api/cat` 6,
`/api/tdoa` 4, `/api/kiwi` 4, `/api/ft8` 4, `/api/cw` 4, `/api/tci` 3 …
Alles, was die Oberfläche kann, ist eine Route: `POST /api/tx/mox`,
`/api/tx/ps/*` (9 PureSignal-Routen), `/api/rx/nr3/model/download`,
`/api/radio/atu/tune`, `/api/cw/send`, `/api/filter/presets/reset`.

**Der WebSocket `/ws`** trägt binäre Frames mit gemeinsamem Kopf
(`WireFormat.HeaderSize`, Typ/Flags/Länge/Seq/Zeitstempel):

| Typ | Richtung | Inhalt |
| --- | --- | --- |
| `0x01 DisplayFrame` | Server → Client | je RX: `HzPerPixel` + `Width` float32 Panadapter-dB + `Width` float32 Wasserfall-dB — **der Server rechnet FFT und Pixelabbildung**, der Client zeichnet nur |
| `0x02 AudioPcm` | Server → Client | float32 PCM, `RxId`, Kanäle, Rate; unkomprimiert |
| `0x03 Status` | Server → Client | Zustands-DTO (JSON) — `StateDto` mit `Receivers[]` (bis 10 DDCs + reservierter Kiwi-Index 10), `WireVersion` 3 |
| `0x20 MicPcm` | Client → Server | Mikrofon, f32le mono 48 kHz in 960-Sample-Blöcken (20 ms) |
| `0x21 AudioStreamRequest`, `0x22 DisplayStreamRequest`, `0x24 NativeMicStreamRequest`, `0x25 CwDecoderRequest` | Client → Server | Abonnements (refcounted) |
| `0x23 ClientDiagnosticLog` | Client → Server | Frontend-Fehler über den Socket, weil der Desktop-Webview nur sechs HTTP-Verbindungen je Host hat (`docs/lessons/webview-connection-budget.md`) |

Weitere typisierte Frames in `Zeus.Contracts`: `MoxStateFrame`,
`PsMetersFrame`, `PaTempFrame`, `MicPeakFrame`, `FilterFrame`,
`CwEngineStatusFrame`, `CwDecodedTextFrame`, `AlertFrame`,
`AudioChainOrderFrame`, `AudioMasterBypassFrame`, `ChatEventFrame`,
`MidiLearnFrame`, `WsjtxInboundReplyFrame`.

**Versionierung:** `GET /api/station/version` → `protocol: 1`;
`WireContract.Version = 3` im Zustands-DTO; Frames sind „append-only",
neue Felder mit Default, damit alte Clients weiterlesen.

**Transport und Sicherheit:** Bindung `Loopback` (Standard) oder `Lan`;
LAN nur mit HTTPS und eigenem Zertifikat; Zugriffstoken über
`ZEUS_STATION_ACCESS_TOKEN` (`UseStationAccessTokenAuthorization`); CORS-
Policy „ZeusLinkLocalAttach"; **Plugin-Installation/-Entfernung nur von
Loopback** (403 sonst). Der Wear-OS-Client (`zeus-watch`) verbindet per
HTTPS mit **Zertifikats-Pinning** (Byte-Vergleich), findet die Station über
`GET /product/status` und sendet **nie über die globalen REST-Routen**: Senden
läuft über einen WebSocket `/product/watch/session` mit **Sende-Lease,
Herzschlag 2×/s, Abbruch nach 2 s Stille**; fällt das Mikrofon aus, stoppt
der Herzschlag. Dieselbe Lease-/Arm-/Key-Mechanik wie beim Außer-Prozess-
Audio aus Teil 1 (§2.8): Capture frei, Injektion und Tastung nur mit
gehaltenem Lease und Bediener-„Arm", toter Produzent wird nach 12 Blöcken
(240 ms) widerrufen.

---

## 2. Abgleich mit Longpath

| Zeus | Bei uns | Befund |
| --- | --- | --- |
| REST + WS zwischen Engine und eigener Oberfläche | ein Prozess, Qt-Signale; keine Naht nötig | — |
| TCI-Server, CAT | `TciServer` (Thetis-Port, WebSocket-Text + Binär-Streams), CAT | ✅ gleichwertig; das ist die Schnittstelle für fremde Programme auf beiden Seiten |
| Automatisierung/Diagnose über HTTP (`/api/diagnostics/*`, `/api/tx/diag`) | `LONGPATH_AUTOMATION=1`-Brücke (dumpTree/grab/get) | ✅ anderer Weg, gleicher Zweck |
| Server-seitige Display-Frames (FFT + Pixel-Mapping im Engine) | TCI liefert IQ, kein Spektrum; Panadapter rechnet die App | ⚠️ für einen **dünnen Fernclient** (Handy, Uhr) der entscheidende Unterschied — siehe 3.2 |
| Zustands-DTO mit Wire-Version, append-only | TCI-Textprotokoll (Version in `TciProtocol`) | ✅ |
| **Sende-Lease mit Herzschlag**, Widerruf bei totem Produzenten; „Transmit never touches the global REST endpoints" | `TciServer::onClientDisconnected()` gibt nur den **TX-Audio-Mutex** frei; **MOX bleibt gesetzt**, wenn der Client, der `trx:0,true` schickte, stirbt — exakt wie Thetis (`ClientDisconnectedHandler` → `RefreshStreamRunState()`), dessen Verhalten wir Zeile für Zeile zitieren | ✳ **echte Sicherheitslücke der Thetis-Linie** — siehe 3.1 |
| Zertifikats-Pinning + Token für LAN-Clients | TCI ohne Authentifizierung (LAN-Vertrauen, wie Thetis/ExpertSDR) | für ein Fernbedienungs-Vorhaben relevant, heute nicht |
| Loopback-only für Installations-Mutationen | kein Plugin-System | — |
| `/api/godseye`, `/api/tdoa`, Satelliten, Contest-Log, VNA | KiwiSDR ✅, Rotor ✅, Contest-Log (Contestprogramm getrennt), VNA (`.s1p`-Import beim SWR-Durchlauf) | Produktumfang, kein Protokollthema |

---

## 3. Was mitzunehmen ist

### 3.1 MOX-Freigabe, wenn der tastende TCI-Client verschwindet — Sicherheitslücke, Empfehlung: bauen

Heute: WSJT-X (oder ein künftiger Fernclient) tastet per `trx:0,true;`,
stürzt ab oder verliert die Verbindung → `onClientDisconnected()` räumt den
Audio-Mutex und die Resampler, **der Sender bleibt getastet**, bis der
Bediener oder eine andere PTT-Quelle ihn löst. Bei FT8 (USB, kein Ton) ist
das ein getasteter PA ohne Steuersignal; bei FM/AM oder mit angehaltenem
Ton im Ring ist es ein Träger. Thetis hat dieselbe Lücke; Zeus hat sie mit
Lease + Herzschlag geschlossen.

Vorschlag (klein, `TciServer` + `MoxController`): merken, **welcher Client
MOX gesetzt hat** (`QPointer<QWebSocket> m_moxOwner`, gesetzt in
`handleTrxCommand` bei `true`, gelöscht bei `false` oder wenn eine andere
PTT-Quelle übernimmt); in `onClientDisconnected()`: ist dieser Client der
Eigentümer und MOX an → MOX aus (über den vorhandenen `PttSource::Cat`-Weg
oder eine neue Quelle `Tci`), Log-Zeile, Statusmeldung. Optional ein
zweiter Schritt nach Zeus-Vorbild: WebSocket-Ping ohne Pong innerhalb von
N s → Verbindung trennen → derselbe Weg. **Sicherheitsrelevant, deshalb
Martins Freigabe vorab**, kein Alleingang; Regressionstest mit
`tst_tci_*`-Prüfständen (Client verbindet, tastet, Socket fällt → MOX
false innerhalb eines Event-Loop-Durchlaufs).

### 3.2 Für die zurückgestellte Mobil-Fernbedienung: die Uhr als Maßstab

Die Zeus-Uhr zeigt, wie dünn ein Fernclient sein kann, wenn der Server
**fertige Bildzeilen** liefert: Mini-Panadapter + Wasserfall, S-Meter,
Leistung, SWR, PTT, Frequenz/Band/Modus — ohne FFT auf der Uhr. Über TCI
müsste ein Handy-Client IQ empfangen und selbst rechnen. Wenn die
Mobil-App (siehe `longpath-mobile-remote-app-idea`) je kommt, wäre ein
kleiner **Spektrum-Frame-Stream** (je Slice `HzPerPixel` + `Width` dB-Werte
für Pan und Wasserfall, 10–20 Hz) neben TCI der Baustein, der sie leicht
macht — und ein Sende-Lease mit Herzschlag der, der sie sicher macht.
Nichts davon jetzt; Notiz für die Entwurfsphase.

### 3.3 Nicht übernehmen

REST-Fassade (unsere Oberfläche ist im Prozess), Zertifikats-/Token-
Schicht (TCI im LAN reicht), Frame-Formate (kein zweiter Client),
`/product/*`-Endpunkte (Store/Entitlement), GodsEye/TDoA (schon am 14.09.
abgelehnt).

---

## 4. Bilanz des Zeus-Engine-Inventars (Teile 1–3)

| Teil | Ergebnis | Mitnahmen |
| --- | --- | --- |
| 1 Plugin-System | kein Portierziel (Store-Infrastruktur) | NaN-Guard ✅ PR #14 · AU-Stufe (offen) · RX-Einschub (offen) |
| 2 WDSP-Fork | elf kleine Zeus-Änderungen; der große Unterschied ist Upstream 2.1.0 | `delay.c`-Klemme ✅ PR #15 · `FFTW_ESTIMATE` (messen) · FIRCORE-Pläne (Prüfstand) · PS 3.0 als ROADMAP-Frage |
| 3 Stationsprotokoll | interne Naht, kein Interop-Standard | **MOX-Freigabe bei Client-Verlust** (Freigabe nötig) · Spektrum-Frames für die Mobil-App (Entwurfsnotiz) |

Damit ist der GPL-Teil von Zeus durchgesehen. Was Zeus darüber hinaus kann
(Broadcast Optimizer, Voyeur, Web-Oberfläche), sitzt im proprietären Client
und bleibt, wie am 28.08. festgehalten, Blackbox — beobachtbar, nicht lesbar.
