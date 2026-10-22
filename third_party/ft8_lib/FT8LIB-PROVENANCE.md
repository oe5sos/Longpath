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

**Eine, in `ft8/decode.c`** (2026-10-22). Alle übrigen übernommenen
Dateien sind Byte für Byte gleich.

`ft8_decode_multi_symbols()` legte sein Zwischenfeld als Array
variabler Länge an:

```c
float s2[n_tones];
```

Das ist C99 und für GCC und Clang kein Problem. Der
Microsoft-Compiler kennt es nicht und lehnt es ab (C2057 / C2466 /
C2133). Aufgefallen am 2026-10-22 beim ersten Windows-Paket seit dem
2026-10-07 — und so lange unbemerkt, weil `ci.yml` Windows mit **MinGW**
baut und grün war, während `release.yml` mit **MSVC** baut und abbrach.

Ersetzt durch ein Feld fester Größe mit Wachposten. Die Grenze ist
nicht geschätzt: die Funktion behandelt `n_syms` 1, 2 und 3 (`j1`,
`j2`, `j3`) und kann strukturell nichts darüber, denn die letzte Summe
hat genau drei Glieder. Damit ist `n_tones <= 512`. Das Verhalten für
jede Eingabe, die der Upstream erzeugt, ist unverändert; der
Wachposten steht nur für eine künftige Fassung mit mehr Symbolen da.
Die Begründung steht auch im Quelltext an der Stelle selbst.

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
