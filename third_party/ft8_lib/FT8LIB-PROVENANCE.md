# ft8_lib — Herkunft und Umfang

**Upstream:** <https://github.com/kgoba/ft8_lib>
**Pinned SHA:** `9fec6ca39886edbf96f4f5e71edc76da5074e871` (2025-08-23)
**Lizenz:** MIT — © 2018 Kārlis Goba, Wortlaut in `LICENSE`
**Übernommen am:** 2026-10-15

## Warum

Der Betreiber, 2026-10-15: „ft8 und ft4 sollte auch angedacht werden.
zeus, aether usw. haben da schon arbeit geleistet."

Nachgesehen, und der Befund fiel anders aus als erwartet:

| | FT8/FT4 |
| --- | --- |
| Thetis | dekodiert **nicht** — Ton per VAC an WSJT-X |
| AetherSDR | dekodiert **nicht** — dasselbe |
| Zeus | dekodiert **nativ**, ebenfalls mit ft8_lib (plus `wsprd`), im Plugin `com.kb2uka.digital` |

Longpath hatte bis heute den WSJT-X-Weg: `WsjtxClient` sammelt fremde
Decodes über UDP ein. Das setzt ein zweites Programm, virtuelle
Tonkabel und eine zweite Einrichtung voraus — auf einem Berg drei
Dinge zu viel.

## Was übernommen ist, und was nicht

Nur der Kern, 4718 Zeilen:

```
ft8/      constants, crc, decode, encode, ldpc, message, text
common/   monitor.{c,h}, common.h      (die Wasserfall-Sammlung)
fft/      kiss_fft, kiss_fftr          (von monitor.c gebraucht)
```

**Nicht übernommen:** `common/audio.*` und `common/wave.*` (PortAudio-
und WAV-Anbindung — Longpath hat seine eigene), `demo/`, `test/`,
`utils/`, das Makefile.

`fft/kiss_fft` bleibt drin, obwohl Longpath FFTW mitbringt: `monitor.c`
ruft es direkt auf, und eine Umstellung wäre eine Änderung an fremdem
Quelltext mit eigener Prüflast. Die Datei ist klein und läuft nur,
während FT8 zuhört.

## Änderungen am Upstream

**Vier Stellen wegen MSVC** (2026-10-22) **und eine Erweiterung**
(2026-10-30). Alle übrigen übernommenen Dateien sind Byte für Byte
gleich.

| Datei | Was | Ersetzt durch |
| --- | --- | --- |
| `ft8/decode.c:516` | `float s2[n_tones]` | feste Größe 512 + Wachposten |
| `common/monitor.c:142-143` | `timedata[me->nfft]`, `freqdata[me->nfft/2+1]` | `malloc`/`free`, einmal je Aufruf |
| `common/gfsk.c:60,68` | `dphi[n_wave + 2*n_spsym]`, `pulse[3*n_spsym]` | `malloc`/`free`, einmal je Aussendung |
| `ft8/message.c` | `stpcpy()` (POSIX, fehlt in MSVCs Bibliothek) | Ersatz unter `#ifdef _MSC_VER` |

### Erweiterung: Nachrichtentyp 5 (EU VHF Contest) — 2026-11-04

| Datei | Was |
| --- | --- |
| `ft8/message.c` | neue Funktionen `to_grid6()` (statisch) + `ftx_message_decode_eu_vhf()`, ein `case` im `switch` von `ftx_message_decode()` |
| `ft8/message.h` | Deklaration dazu; **und ein berichtigter Kommentar** an `FTX_MESSAGE_TYPE_WWROF` |

Dieselbe Klasse wie beim DXpedition-Typ darunter: upstream erkennt den
Typ (`ftx_message_get_type()` liefert ihn), **dekodiert ihn aber
nicht** — der `switch` lässt ihn in den `default`-Zweig laufen und
gibt `FTX_MESSAGE_RC_ERROR_TYPE` zurück. `Ft8Decoder.cpp` wirft die
Zeile daraufhin weg. In einem EU-VHF-Contest fiel damit **jede**
Contest-Station still durch, von „niemand da" nicht zu unterscheiden.

#### Zwei falsche Fährten, die hier geschlossen sind

**1. Typ 0.2 ist tot.** Upstream führt ihn als „EU VHF contest", und
das war er einmal. WSJT-X 2.7.0 dekodiert ihn selbst nicht mehr:

> From WSJT-X `lib/77bit/packjt77.f90:330-331` [v2.7.0] —
> `else if(i3.eq.0 .and. n3.eq.2) then` / `unpk77_success=.false.`

Wer dort baut, baut an einem Format, das niemand mehr sendet.

**2. Der Name `FTX_MESSAGE_TYPE_WWROF` täuscht**, und der Kommentar
daneben beschrieb die Felder falsch (`1 28 28 1 7 9`) — mit einem
Fragezeichen von upstream selbst. Maßgeblich ist:

> From WSJT-X `lib/77bit/packjt77.f90:593-610` [v2.7.0] —
> `! Type 5  <PA3XYZ> <G4ABC/P> R 590003 IO91NP      h12 h22 r1 s3 S11 g25`
> `read(c77,1060) n12,n22,ir,irpt,iserial,igrid6` /
> `1060 format(b12,b22,b1,b3,b11,b25)` / `nrs=52+irpt` /
> `write(cexch,1022) nrs,iserial` / `1022 format(i2,i4.4)`

Der Enum-Name bleibt unverändert, damit fremder Quelltext sich nicht
ändert; der Kommentar ist berichtigt und verweist auf die Quelle.

Die Locator-Umrechnung ist ebenfalls übernommen, nicht gerechnet:

> From WSJT-X `lib/77bit/packjt77.f90:1577-1607` [v2.7.0] —
> `subroutine to_grid6(n,grid6,ok)` / `j1=n/(18*10*10*24*24)` …

#### Vier Stellen, an denen man sich vertut — je ein Prüffall

`tests/tst_ft8_eu_vhf.cpp`, 12 Fälle:

* **Der Rapport ist kein dB-Wert**, sondern `52 + irpt` (drei Bit,
  also 52…59). Im Contest ist es faktisch immer 59, aber die Bits
  können auch weniger tragen.
* **Die laufende Nummer hat immer vier Stellen** (`format(i4.4)`).
  „5901" statt „590001" wäre ein anderer Austausch.
* **Der Locator ist sechsstellig**, 25 Bit. Im UKW-Contest zählt genau
  die sechste Stelle für die Entfernungspunkte; vier Stellen wären bis
  zu 100 km daneben.
* **Beide Rufzeichen stehen nur als Hash** (12 und 22 Bit). Ohne
  Eintrag in der Tabelle steht `<...>` — kein Fehler, sondern alles,
  was gesendet wurde.

#### Warum die Prüfnutzlast anders entsteht als beim DXpedition-Lauf

Dort kamen die Rufzeichenfelder aus ft8_libs **eigenem** Packer, damit
ein Fehler in der Bitreihenfolge sich nicht in beide Richtungen
aufhebt. Diesen Weg gibt es hier nicht: für Typ 5 hat ft8_lib **keinen**
Packer. Der Ersatz ist ein generischer Bit-Setzer (Bitposition hinein,
Wert hinein), der mit der Auspackrechnung in `message.c` keine Zeile
teilt — dort Schiebeweiten und Masken je Feld, hier Bit für Bit. Dazu
`ftx_message_get_type()` als unabhängige Probe, dass `i3` wirklich dort
sitzt, wo der Setzer es hinlegt.

Gegenprobe gegen die Fassung ohne den `case` gefahren: **8 von 12
Fällen rot**, alle mit `RC=5` (`FTX_MESSAGE_RC_ERROR_TYPE`). Der
Locator-Schranken-Fall wurde dabei geschärft — er prüft jetzt auf
`FTX_MESSAGE_RC_ERROR_GRID` genau, weil die alte Fassung dort
ebenfalls einen Fehler lieferte (nur einen anderen) und ein „irgendein
Fehler" nichts belegt hätte.

### Erweiterung: Typ 5 SENDEN — 2026-11-04

| Datei | Was |
| --- | --- |
| `ft8/message.c` | `from_grid6()` + `put_bits()` (statisch), `ftx_message_encode_eu_vhf()`, `encode_eu_vhf_text()` (statisch) + Aufruf in `ftx_message_encode()` vor dem Freitext-Rückfall |
| `ft8/message.h` | Deklaration von `ftx_message_encode_eu_vhf()` |

Die Leseseite darüber half beim Empfangen; senden konnte Longpath den
Austausch weiterhin nicht. `ftx_message_encode()` kennt nur drei Wege
— `encode_std`, `encode_nonstd`, `encode_free`. Ein Contest-Austausch
wäre dort als **Freitext** auf dem Band gelandet: lesbar nur für den,
der denselben Fehler macht.

Die Bitlage und die Schranken sind übernommen, nicht gerechnet:

> From WSJT-X `lib/77bit/packjt77.f90:1398-1431` [v2.7.0] —
> `if(nwords.lt.4 .or. nwords.gt.5) return` /
> `if(w(1)(1:1).ne.'<' .or. w(2)(1:1).ne.'<') return` /
> `if(nx.lt.520001 .or. nx.gt.594095) return` /
> `irpt=nx/10000 - 52` / `iserial=mod(nx,10000)` /
> `if(iserial.gt.2047) iserial=2047` /
> `1010 format(b12.12,b22.22,b1,b3.3,b11.11,b25.25,b3.3)`

Die Reihenfolge in `ftx_message_encode()` ist ebenfalls die von
WSJT-X: Typ 5 als letzter Sonderfall, **vor** dem Freitext-Rückfall
(`packjt77.f90:186-190`).

#### Unabhängig bestätigt: MSHV kommt auf dieselben Bitgrenzen

Die Bitlage stammt aus WSJT-X. Eine zweite, unabhängig geschriebene
Umsetzung derselben Spezifikation bestätigt sie Feld für Feld —
MSHV (LZ2HV, GPLv3), `src/HvMsPlayer/libsound/HvPackUnpackMsg/pack_unpack_msg77.cpp:883-901`:

```cpp
else if (i3==5)//2.39 new EU VHF contest
    int n12     = BinToInt32(c77,0,12);
    int n22     = BinToInt32(c77,12,34);
    int ir      = BinToInt32(c77,34,35);
    int irpt    = BinToInt32(c77,35,38);
    int iserial = BinToInt32(c77,38,49);
    int igrid6  = BinToInt32(c77,49,74);
    if (igrid6<0 || igrid6>18662399) { unpk77_success=false; return ""; }
```

Dieselben sechs Grenzen, dieselbe Locator-Schranke, dasselbe
Zusammensetzen von `cexch` aus `52+irpt` und vier Stellen Nummer. Das
ist mehr wert als ein weiterer eigener Prüffall: zwei Umsetzungen, die
sich nicht kennen, sind an derselben Stelle gleich.

(Fact-only-Zitat, kein Port — MSHV-PROVENANCE.md führt die Bibliothek
bereits.)

#### Die Grenze, die das Format setzt

**Eine laufende Nummer über 2047 wird still auf 2047 gekappt.** Das
Feld hat elf Bit, WSJT-X lässt den Austausch aber bis 594095 zu — die
Prüfung erlaubt also 4095, das Feld trägt 2047. Die Gegenstation
trägt dann eine falsche Nummer ins Log. Das ist kein Fehler dieses
Ports, sondern die Grenze des Formats; im UKW-Contest mit FT8 ist sie
praktisch unerreichbar, aber „praktisch unerreichbar" ist kein Grund,
es zu verschweigen. `tests/tst_ft8_eu_vhf.cpp` hält es fest.

#### Belege

`tests/tst_ft8_eu_vhf.cpp` wächst auf 20 Fälle. Der entscheidende ist
**nicht** der Rundlauf — der wäre grün, auch wenn Packer und
Auspacker dieselbe falsche Bitreihenfolge hätten. Er ist
`derPackerLegtDieFelderDorthinWoWsjtxSieErwartet`: die gepackte
Nutzlast wird mit demselben unabhängigen Bit-für-Bit-Leser zerlegt,
mit dem auch die Lesefälle gegen WSJT-X gebaut sind, und Feld für
Feld gegen die Erwartung geprüft.

Dazu, was der Packer **abweisen** muss: Rapport außerhalb 52…59,
Nummer 0 oder über 4095, vierstelliger Locator, Locator außerhalb des
Rasters. Und was der Texterkenner **nicht** an sich ziehen darf:
Sätze ohne spitze Klammern (die Klammern sagen „gehasht"), mit nur
einem gehashten Rufzeichen, mit Austausch außerhalb der Schranken.

Gegenprobe ohne `encode_eu_vhf_text` im Weg: die beiden Textweg-Fälle
rot.

### Erweiterung: Nachrichtentyp 0.1 (DXpedition Mode) — 2026-10-30

| Datei | Was |
| --- | --- |
| `ft8/message.c` | neue Funktion `ftx_message_decode_dxpedition()` + ein `case` im `switch` von `ftx_message_decode()` |
| `ft8/message.h` | Deklaration dazu |

Upstream kennt den Typ (`FTX_MESSAGE_TYPE_DXPEDITION` wird von
`ftx_message_get_type()` korrekt erkannt), **dekodiert ihn aber
nicht**: der `switch` lässt ihn in den `default`-Zweig laufen und gibt
`FTX_MESSAGE_RC_ERROR_TYPE` zurück. Jede Nachricht einer
DX-Expedition im Fox-Betrieb fiel damit still durch, und der Hound sah
nie, dass er aufgerufen wurde — von außen nicht von „die Expedition
ist nicht zu hören" zu unterscheiden.

Der Aufbau ist nicht geraten, sondern aus WSJT-X übernommen:

> From WSJT-X `lib/77bit/packjt77.f90:309-329` [v2.7.0] —
> `read(c77,1010) n28a,n28b,n10,n5` / `1010 format(2b28,b10,b5)` /
> `irpt=2*n5 - 30` /
> `msg=trim(call_1)//' RR73; '//trim(call_2)//' '//trim(call_3)//' '//crpt`

Zwei Stellen, an denen man sich vertut, und beide haben einen eigenen
Prüffall in `tests/tst_ft8_dxpedition.cpp`:

* **Der Rapport läuft in ZWEI-dB-Schritten** (`irpt = 2*n5 - 30`). Wer
  einen Schritt von 1 dB annimmt, liest jeden Rapport halb so groß und
  um 15 dB verschoben.
* **Das dritte Rufzeichen ist nur ein 10-Bit-Hash.** Ohne Eintrag in
  der Hash-Tabelle steht dort `<...>` — das ist kein Fehler, sondern
  alles, was im Paket steht.

Die Rufzeichenfelder im Prüfstand kommen aus ft8_libs **eigenem**
Packer (`ftx_message_encode_std`), nicht aus einer eigenen Rechnung:
ein reiner Rundlauf wäre grün, auch wenn die Bitreihenfolge nicht die
von WSJT-X ist — derselbe Fehler in beide Richtungen hebt sich auf.

Die erste Gruppe sind Arrays variabler Länge, die vierte ein fehlendes
Symbol beim Binden:

```
ft8_static.lib(message.c.obj) : error LNK2019: unresolved external
    symbol stpcpy referenced in function ftx_message_decode_std
Longpath.exe : fatal error LNK1120: 1 unresolved externals
```

Der Ersatz hält denselben Vertrag — kopieren einschließlich Null,
Zeiger **auf** die Null zurück — und ist gegen das echte `stpcpy`
nachgerechnet (gleicher Rückgabeversatz, gleiches Ergebnis). Er wird nur
für MSVC übersetzt; überall sonst bleibt die Bibliothek unberührt.

Arrays variabler Länge sind C99. GCC und Clang übersetzen sie, der
Microsoft-Compiler kennt sie nicht und lehnt sie ab (C2057 / C2466 /
C2133).

**Warum es erst jetzt auffiel:** `ci.yml` baut Windows mit **MinGW**
(GCC) und war bei jedem PR grün; `release.yml` baut mit **MSVC** und
brach ab. Zwei Compiler, ein Plattformname — die CI kann einen
MSVC-Bruch grundsätzlich nicht fangen. Gemerkt am 2026-10-22 beim ersten
Windows-Paket seit dem 2026-10-07.

**Warum zweimal gesucht wurde:** der erste Durchgang fand nur
`decode.c`, weil er mit einem Suchmuster suchte. Der zweite ließ den
Compiler suchen (`-Wvla` über `compile_commands.json`) und fand alle
fünf Stellen im Baum — die zwei weiteren hier, dazu drei in
`rade`, `libspecbleach` und PortAudios CoreAudio-Teil, die Windows
nicht betreffen (sie lagen schon im Baum, als Windows zuletzt
erfolgreich baute). Dabei noch eine Falle: `-w` steht in den
Bau-Zeilen dieser Dateien und übersteuert jedes später angehängte
`-W...`; ohne es zu entfernen meldet der Compiler nichts.

**Zu den Grenzen:** in `decode.c` ist sie nachweisbar — die Funktion
behandelt `n_syms` 1, 2 und 3 (`j1`, `j2`, `j3`) und kann strukturell
nichts darüber, denn die letzte Summe hat genau drei Glieder, also
`n_tones <= 512`. In `monitor.c` und `gfsk.c` gibt es keine solche
Grenze (sie hängen an Abtastrate und Symboldauer), darum dort der
Haufen statt einer geratenen Zahl. Es kostet nichts: `monitor_process()`
belegt einmal je Tonblock (bei FT8 rund 40 ms) statt je Teilblock, und
`synth_gfsk()` läuft einmal je Aussendung.

Das Verhalten ist unverändert. Belegt durch die Prüfläufe auf clang:
`tst_ft8_dekodiert` 8, `tst_ft8_subtraktion` 9, `tst_ft8_am_ton` 9,
`tst_ft8_senden` 38, `tst_ft8_rechenzeit` 3 — alle grün. `gfsk.c` sitzt
dabei auf beiden Seiten: der Prüfstand erzeugt sein Signal damit, und
gesendet wird damit auch.

Die Begründung steht an jeder der drei Stellen auch im Quelltext.

**Eine Ergänzung:** `common/gfsk.{c,h}`. Die beiden Funktionen
`gfsk_pulse()` und `synth_gfsk()` stehen im Upstream nur im
Beispielprogramm `demo/gen_ft8.c` und haben dort keine Kopfdatei.
Sie sind Zeile für Zeile herauskopiert, ohne dessen `main()`, und
tragen die Herkunft im Kopf. Longpath braucht sie zweimal: der
Prüfstand erzeugt sein Prüfsignal mit **derselben** Rechnung, mit der
später auch gesendet wird — sonst prüfte er den Dekoder gegen eine
Modulation, die es so nie gibt.

Alles Übrige, was Longpath dazutut, steht in
`src/core/Ft8Decoder.{h,cpp}`.
