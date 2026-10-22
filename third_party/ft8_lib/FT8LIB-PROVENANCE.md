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

**Arrays variabler Länge, an drei Stellen** (2026-10-22). Alle übrigen
übernommenen Dateien sind Byte für Byte gleich.

| Datei | Feld | Ersetzt durch |
| --- | --- | --- |
| `ft8/decode.c:516` | `float s2[n_tones]` | feste Größe 512 + Wachposten |
| `common/monitor.c:142-143` | `timedata[me->nfft]`, `freqdata[me->nfft/2+1]` | `malloc`/`free`, einmal je Aufruf |
| `common/gfsk.c:60,68` | `dphi[n_wave + 2*n_spsym]`, `pulse[3*n_spsym]` | `malloc`/`free`, einmal je Aussendung |

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
