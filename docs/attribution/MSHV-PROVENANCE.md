# MSHV Provenance — Longpath port inventory

Dieses Blatt führt, was Longpath aus **MSHV** (LZ2HV) übernommen hat.

Longpath steht unter GPLv3 (Wurzel-`LICENSE`). MSHV ebenfalls — die
Lizenzdatei wurde **selbst gesehen**, nicht von einer Projektseite
abgeschrieben: `license.txt` und `COPYING.txt` im Quellarchiv sind beide
der vollständige GPLv3-Text (627 Zeilen). Es gibt keinen
Lizenzkonflikt.

## Upstream

- **Projekt:** MSHV (Multi Stream HV)
- **Autor:** Hrisimir Hristov, LZ2HV
- **Bezugsquelle:** SourceForge, `MSHV_2767_Full_Source_Code.zip`
  (7 940 682 Byte, veröffentlicht 2026-10-05)
- **Fassung:** 2.76.7
- **Geholt:** 2026-10-21
- **Entpackt nach:** `~/Longpath/MSHV/` (neben dem Longpath-Baum, wie
  Thetis und AetherSDR)
- **Sprache:** C++ (Qt)

## Lizenz

GPLv3, `license.txt` / `COPYING.txt` im Quellbaum.

MSHV selbst ist eine C++-Neufassung der WSJT-X-Dekoder. Die Kopfzeile
jeder Dekoderdatei nennt beide Seiten, und genau diese Kopfzeile ist in
die Longpath-Datei übernommen:

```
/* The algorithms, source code, look-and-feel of WSJT-X and related
 * programs, and protocol specifications for the modes FSK441, FT8, JT4,
 * JT6M, JT9, JT65, JTMS, QRA64, ISCAT, MSK144, are Copyright © 2001-2017
 * by one or more of the following authors: Joseph Taylor, K1JT; Bill
 * Somerville, G4WJS; Steven Franke, K9AN; Nico Palermo, IV3NWV; Greg Beam,
 * KI7MT; Michael Black, W9MDB; Edson Pereira, PY2SDR; Philip Karn, KA9Q;
 * and other members of the WSJT Development Group.
 *
 * MSHV FT8 Decoder
 * Rewritten into C++ and modified by Hrisimir Hristov, LZ2HV 2015-2020
 * May be used under the terms of the GNU General Public License (GPL)
 */
```

## Was portiert wurde

| Upstream | Longpath | Art |
|---|---|---|
| `src/HvDecoderMs/decoderft8.cpp:464-536` (`subtractft8`) | `src/core/Ft8Subtract.cpp` | port |
| `src/HvDecoderMs/decoderft8.cpp:90-120` (Filter + Kantenkorrektur) | `src/core/Ft8Subtract.cpp` | port |
| `src/HvDecoderMs/decoderft8.cpp:272` (`K_SUB 1.9962`) | `src/core/Ft8Subtract.cpp` | Konstante |
| `src/HvDecoderMs/decoderft8.cpp:335-336` (`nsym=79`, `nsps=1920`) | `src/core/Ft8Subtract.h` | Konstante |

## Was bewusst NICHT portiert wurde

**`gen_ft8cwaveRx()` (Zeilen 329-379), der komplexe GFSK-Modulator.**
Longpath hat denselben Modulator bereits im Haus: `synth_gfsk()` aus
`third_party/ft8_lib/common/gfsk.c` (MIT, mitgeliefert) ist die reelle
Fassung derselben Rechnung — gleiche `dphi_peak`, gleiche
Pulsaufsummierung, gleiche Scheinsymbole vorn und hinten, gleiche
Rampen. Aus der reellen Fassung wird das analytische Signal gebildet;
für ein bandbegrenztes Signal ist das Ergebnis dasselbe. Denselben
Modulator ein zweites Mal zu schreiben wäre Verdopplung ohne Gewinn.

**`BestIdtft8()` (Zeilen 381-462), die Dreipunkt-Feinjustierung des
Zeitversatzes.** Ersetzt durch eine volle Kreuzkorrelation, und zwar
aus einem gemessenen Grund (`tests/tst_ft8_subtraktion`,
`wieGenauSchaetztDerDekoder`):

```
gemeldet f=1000,000 Hz (wahr 1000,000)
gemeldet dt=0,6400 s  (wahr 0,5000)
```

Die Tonlage stimmt auf das Hertz, der Zeitversatz liegt um **1680
Proben** daneben. MSHVs Suchbereich ist ±90 Proben — bei seinem eigenen
Dekoder reicht das, weil der den Versatz feiner meldet als `ft8_lib`.
Mit unseren Zahlen träfe eine Dreipunktsuche nie. Die Kreuzkorrelation
kostet ein FFT-Paar und findet die Stelle probengenau.

## Gemessen

| | |
|---|---|
| Abgezogenes Signal | **34,3 dB leiser** |
| Zwei Stationen auf derselben Tonlage, die schwächere 12 dB darunter | eine Runde findet **1**, zwei Runden finden **2** |
| Volles Band (15 Signale) | 15 gefunden, **228 ms** |

## Was MSHV sonst noch könnte

Multi Answering (mehrere Stationen gleichzeitig abarbeiten) ist
**nicht** portiert und war auch nicht beauftragt. Es greift in den
Sendeweg, nicht in den Dekoder. Siehe die Notiz im Gedächtnis
(`longpath-ft8-programme-vergleich`).
