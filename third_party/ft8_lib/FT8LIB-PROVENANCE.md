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
