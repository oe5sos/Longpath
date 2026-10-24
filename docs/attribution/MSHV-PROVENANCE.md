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
| `src/HvMsPlayer/libsound/HvGenFt8/gen_ft8.cpp:262` (`f0 = f_tx + 60.0*islot`, Klammer 100..5000 Hz) | `src/core/Ft8Tx.cpp` (`tonleiter`) | Verfahren + Konstanten |
| `src/HvMsPlayer/libsound/HvGenFt8/gen_ft8.cpp:290` (`d_iwave[k1] += sin(phi)`) | `src/core/Ft8Tx.cpp` (`vorbereitenMehrere`) | Verfahren |
| `src/HvMsPlayer/libsound/HvGenFt8/gen_ft8.cpp:303` (`d_iwave[z]/(double)nslots`) | `src/core/Ft8Tx.cpp` (`vorbereitenMehrere`) | Verfahren |

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

## Multi Answering — Stufe 1 übernommen (2026-10-22)

Der Betreiber hat es am 2026-10-22 beauftragt („mshv wäre meine erste
wahl, sofern möglich"), nachdem es am 2026-10-21 ausdrücklich noch
nicht beauftragt war.

**Übernommen ist das Verfahren der Tonerzeugung**, kein Code:

| Was | Wert |
| --- | --- |
| Abstand zweier Plätze | **60 Hz** |
| Grenzen | 100 … 5000 Hz |
| Zusammenführung | alle Kurven in eine summiert |
| Pegel | durch die **Zahl der Plätze** geteilt |

Die Teilung ist kein Geschmack: der Sender hat eine Höchstamplitude.
Zwei Plätze heißen **−6 dB je Station**, vier **−12 dB**. Wer das
weglässt, übersteuert.

Belegt im Rundlauf gegen den **eigenen** Dekoder
(`tests/tst_ft8_multi_answering.cpp`): drei Nachrichten in ein Fenster,
alle drei kommen wieder heraus.

**Noch nicht übernommen:** die Ablaufsteuerung für mehrere QSOs
gleichzeitig (MSHVs `hvmultianswermodw.cpp`, rund 3000 Zeilen, davon
der größte Teil Fensterwerk) und die Fläche dafür. `Ft8Ablauf` ist eine
reine Rechenvorschrift je QSO — mehrere davon nebeneinander ist der
nächste Schritt, nicht dieser.

**Was dabei zu bedenken ist:** mehrere überlagerte Signale haben keine
konstante Hüllkurve mehr. Damit fällt der Grund weg, PureSignal bei
FT8 abzuschalten — und es wird gebraucht, weil Überlagerung
Intermodulation erzeugt.

## Stufe 2 — das Modell der Plätze (2026-10-24)

Gelesen, nicht übersetzt: `src/HvTxW/hvmultianswermodw.h` [2.76.7]
hält zwei Listen nebeneinander — `LsQueue` (wer angerufen hat und
wartet) und `LsNow` (wer gerade abgearbeitet wird) — dazu

```
#define MAXSL 6   //2.71 old=5     (Zeile 137)
```

als harte Obergrenze und einen Regler darunter (`SBslots`).

`src/core/Ft8Mehrfach.{h,cpp}` übernimmt **das Modell**, keinen
Quelltext: `kMaxPlaetze = 6`, Warteschlange, Nachrücken beim
Freiwerden, keine Doppelten. Die 2990 Zeilen von
`hvmultianswermodw.cpp` sind nicht geholt — sie hängen an MSHVs
eigener Oberfläche und an Dingen, die Longpath anders löst.

Die Obergrenze ist kein Geschmack: der Pegel wird durch die Zahl der
Plätze geteilt (`gen_ft8.cpp:303`, siehe oben). Sechs Plätze sind
−15,6 dB je Station.
