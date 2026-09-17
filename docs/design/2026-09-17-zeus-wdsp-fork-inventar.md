# Zeus' WDSP-Fork: Inventar und Abgleich mit Longpath (Teil 2)

**Stand:** 17. September 2026
**Quellen (vier Bäume, alle lokal):**
- **Z** — Zeus `native/wdsp` (`../zeus-station-engine` @ `324e865`, Release
  v2.0.19, GPL), eigene Patch-Liste in `native/wdsp/ZEUS-PATCHES.md`
- **L** — Longpath `third_party/wdsp/src` (`origin/main` @ `ac93b097`;
  `version.c` meldet 129, dazu Thetis' NR3/NR4 und seit 14.09. das NNR aus 2.1.0)
- **T** — Thetis `Project Files/Source/wdsp` (`../Thetis` @ v2.10.3.15, MW0LGE)
- **U** — Upstream WDSP **2.1.0** (Warren Pratt NR0V, 2026-09-04), als
  unabhängiger Import in deskHPSDR `wdsp-2.10` @ `e307f0f` (`../deskhpsdr`),
  mit `WDSP_Guide__Rev_2_1_0.pdf`
**Anlass:** Teil 2 des Zeus-Engine-Inventars („3, dann 2"), nach
[Teil 1](2026-09-17-zeus-plugin-system-inventar.md) und PR #14.

---

## 1. Methode

Zeus hat den ganzen Baum umformatiert (K&R-Klammern, zwei Leerzeichen), ein
Zeilen-Diff ist daher wertlos (`wcpAGC.c`: 257 Zeilen Unterschied, null
Substanz). Alle vier Bäume wurden mit einem kleinen Normalisierer
(Kommentare, Leerraum und Klammern entfernt, ein Statement je Zeile) auf
eine Form gebracht und dann Datei für Datei verglichen: **Z−U** sind Zeus'
eigene Änderungen, **U−L** ist, was ein Upstream-Nachzug auf 2.1.0 für uns
bedeuten würde, **L−T** zeigt, wo Longpath von Thetis abweicht.

Ergebnis in einem Satz: Longpath ist in fast allen Dateien identisch mit
Thetis (L−T = 0), Zeus' eigene Änderungen sind **klein und gut abgegrenzt**
(elf Dateien mit Substanz), und der große Unterschied zwischen Zeus und uns
ist **nicht Zeus**, sondern Upstream 2.0.0/2.1.0.

---

## 2. Zeus' eigene WDSP-Änderungen (Z−U), mit Bewertung

| # | Datei(en) | Was Zeus geändert hat | Bei uns | Wert |
| --- | --- | --- | --- | --- |
| 1 | `delay.c` | `set_delay_value_unlocked` **klemmt** die Phasenzahl auf `(WSDEL−1)·L + (L−1)` und setzt `tdelay` auf den realisierten Wert | **nicht geklemmt.** `SetDelayValue` rechnet `snum = phnum / L` ohne Grenze; `xdelay` fängt nur *einen* Überlauf (`n -= rsize`). `PsForm.cpp:358` erlaubt AmpDelay bis **25 000 000 ns = 25 ms**; `WSDEL = 1025` Samples sind bei 192 kHz TX-Rate **5,3 ms** → darüber liest der Ring außerhalb seiner Grenzen | **hoch, klein** — echter Fehler, ~10 Zeilen, zitierfähig portierbar |
| 2 | `fir.c` | Einmal-FFTs für den Filterentwurf (`fir_fsamp`, `mp_imp`) mit **`FFTW_ESTIMATE`** statt `FFTW_PATIENT` geplant | `FFTW_PATIENT` für jeden Entwurf. Solange die Wisdom-Datei die Größe kennt, egal; für eine unbekannte Größe (großes `nc`) plant FFTW sekundenlang für eine Transformation, die genau einmal läuft | **mittel, klein** — nur Planungszeit, keine Ergebnisänderung; erst messen (Filterwechsel bei 16k Taps ohne Wisdom) |
| 3 | `firmin.c/h` | **FIRCORE**: drei gemeinsame FFTW-Pläne + Staging-Puffer statt `3·nfor` `FFTW_PATIENT`-Pläne (bei `nc = 16384`, `size = 256` sind das 192 statt 3); reiner `nc`-Wechsel behält die Pläne und dimensioniert nur die Partitionen um; `pfactor`-Parameter; MP-Impuls über den prozessweiten Cache, kalte Transformation auf 1 048 576 Punkte gedeckelt bei ≥ 4-facher Überabtastung | Thetis-Stand: ein Plan je Partition | **mittel, mittel** — spürbar bei Kanalanlage und Filtergrößenwechsel; Kernstück der Faltung, braucht Prüfstand (bit-gleiche Ausgabe gegen alt) |
| 4 | `impulse_cache.c/h` | 64-MB-Byte-Budget je Bucket (4096 Einträge nur noch zweite Grenze); **eigener Cache-Mutex** um jede Operation (bisher nur um das use-Flag), Interlocked-Lazy-Init, `lock_mp_generation()`; Dateiformat v2 „ZFIR" mit 64-Bit-Hash auf allen Systemen | `init_impulse_cache` wird aufgerufen (`WdspEngine.cpp:263`); Thetis-Stand mit Zähler-Grenze und Flag-Mutex | **niedrig** — Härtung für 262 144-Tap-Filter und parallele Kanäle; ohne Ultra-Auflösung kein Druck. Das 32/64-Bit-Hash-Thema betrifft uns nicht (nur 64-Bit-Bauten) |
| 5 | `fir.c` | **Kaiser-Fenster** (`wintype 2`, β = 8, Bessel-I₀ als Reihe) | nur Blackman-Harris (0/1) | niedrig für sich; Teil von #6 |
| 6 | `bandpass.c/h`, `TXA.c/h` | **TX-Filterprofil**: `filter_master_nc`, `filter_cleanup_nc`, `filter_mp`, `ApplyTXABandpassProfile()`; die Auflösung sitzt auf bp1 (nach dem Kompressor), wenn der läuft, sonst bp0; bp2 (osctrl-Aufräumer) bleibt kurz und minimalphasig, damit PureSignals osctrl-Bypass beim Scharfschalten kein Riesen-FIR neu baut. **Neue TX-Vorgaben: 16 384 Taps, minimalphasig, Kaiser.** Der Engine bietet 64 … 262 144 Taps an, mit Warnung vor langsamen Moduswechseln | Thetis-Vorgabe: `max(2048, dsp_size)` Taps, linearphasig, Blackman-Harris; `SetTXABandpassNC/MP` vorhanden | **Produktentscheidung** — technisch interessant ist nur „TX minimalphasig als Vorgabe" (weniger TX-Latenz, für Sprache ohne Nachteil); 262 144-Tap-Filter sind ein Marketing-Merkmal mit CPU-Preis |
| 7 | `osctrl.c/h` | `setBandwidth_osctrl` / `SetTXAosctrlBandwidth`: Peakfenster des **CESSB-Überschwingreglers folgt der TX-Bandbreite** (3 oder 4 kHz; Regel bewahrt Thetis' 3-kHz-Geometrie exakt); dazu **Nicht-endlich-Riegel im Peakfenster** („must not poison the peak window indefinitely") und ein `WDSP_OSCTRL_CORE_TEST`-Prüfstand | `a->bw = 3000.0` fest (`osctrl.c:67`), TX-Filter-Obergrenze aber 10 000 Hz (`TxApplet.cpp:591`) → bei ESSB mit CESSB ist das Fenster für 3 kHz bemessen (bei 48 kHz: 5 statt 3 Samples) — funktioniert, nur weniger genau | **niedrig** — der NaN-Riegel bestätigt das Thema aus PR #14 unabhängig; Bandbreite nur, falls Martin ESSB > 3 kHz mit CESSB fährt |
| 8 | `rnnr.c` | `RNNRloadModel` gibt Erfolg zurück und **probt** das Modell (`rnnoise_create`) vor Übernahme; `RNNRmodelLoaded()` | Thetis-Stand: stille Übernahme, `NULL` bei Fehler → eingebautes Modell | niedrig — Robustheit, falls NR3 auf eine kaputte Modelldatei zeigt |
| 9 | `RXA.c` | `RXAbp1CheckEx` (Upstream-Signatur + NR3/NR4) | eigene Thetis-Variante, gleich im Ergebnis | — |
| 10 | `calcc.c` | `psccF`-Float-IQ-Wrapper + Staging-Puffer je Kalibrator | — | nein (C#-Bequemlichkeit) |
| 11 | `wisdom.c`, `utilities.*`, `linux_port.*`, `wdsp_export.h`, `wdsp.h` | Portabilität, Exporte, Debug-Helfer | eigene Lösung | nein |

`FDnoiseIQ.c` (131 000 Zeilen Rauschrahmen-Tabelle) ist bei Zeus tot und nur
„gegen unbeabsichtigtes Löschen" im Baum, weil das 2.1.0-EMNR es nicht mehr
braucht. **Bei uns lebt es noch:** `emnr.c:570/932` (Thetis-Stand) liest
`FDnoise`/`FDnoise_frames` für die psychoakustische Nachbearbeitung
(`post2`, Rev 1.27), und das CMake-GLOB übersetzt es. Erst ein
Upstream-Nachzug (Abschnitt 3) macht es entbehrlich.

Was bei Zeus **nicht** in WDSP steckt, obwohl der Changelog es nahelegt:
NR3/NR4 (`rnnr.c`, `sbnr.c`) sind **Thetis-Code** (MW0LGE), den Zeus wie wir
übernommen hat — wir haben ihn seit April. Auto-Notch ist unverändertes
Warren-Pratt-`anf.c` (schon am 28.08. geklärt).

---

## 3. Der eigentliche Unterschied: Upstream 2.0.0 / 2.1.0 (U−L)

Longpath steht auf der **Thetis-Linie (WDSP 1.29 + MW0LGE)** und hat aus
2.1.0 nur das NNR geholt. Zeus und deskHPSDR haben **ganz** auf 2.1.0
gewechselt. Warrens Revisionsgeschichte (Handbuch Rev 2.1.0, S. 209):

| Rev | Datum | Inhalt | Bei uns |
| --- | --- | --- | --- |
| 2.0.0 | 2026-07-01 | **PureSignal 3.0** — neue Datenstrukturen, „very robust algorithms, simpler operation" (`calcc.c`: 1 842 normalisierte Zeilen Unterschied, `iqc.c` 219, eine `iqc`-Instanz statt `p0/p1`, `create_calcc` mit weniger Argumenten, `GetPSDisp` neue Signatur) | PS 2.x wie Thetis; 22 API-Wrapper in `TxChannel`, `PureSignal.cpp` spiegelt `PSForm.cs` |
| 2.0.0 | | **Free-Curves** für EQ und CFC (direkte Kurvensteuerung; `eq.c` 405, `cfcomp.c` 406, `fcurve.c` 89) | Thetis-EQ (10-Band) + eigene Client-EQ im Strip |
| 2.0.0 | | Phasenrotator mit **Auto-Kalibrierung** und Asymmetrie-Anzeige (`phrot.c` neu, bei uns in `iir.c`) | manuell; eigener `ClientPhaseRotator` im Strip |
| 2.0.0 | | Neuer **RX-Eingangsdezimator** (bessere Aliasunterdrückung, bis 6144 kS/s) | Thetis-Stand |
| 2.0.0 | | UKW-**Stereo**-Demodulator (`wbfm.c`) | fehlt (Rundfunk, für Amateurfunk nebensächlich) |
| 2.0.0 | | CW-APF-Verbesserungen; Wisdom wird neu berechnet | DoublePole/Matched/Gaussian aus 1.29 vorhanden |
| 2.1.0 | 2026-09-04 | **NNR** ✅ (ee5b9fa1) · PS-3.0-Tuning, weniger CPU bei Kalibrierung · **Effizienz in der Filtererzeugung** (`nurbs*.c`, `extrapolate.c` neu; `fir.c` 159, `firmin.c` 12, `icfir.c` 74, `cfir.c` 92) · `wdsp.h` als generierter Prototyp-Header | nur NNR |

Das ist eine **eigene, strategische Entscheidung**, kein Zeus-Thema:
Upstream-2.1.0-Nachzug (wie Zeus/deskHPSDR) gegen Thetis-Linie (MW0LGE wird
irgendwann nachziehen, und unsere Thetis-Ports zitieren Zeile für Zeile
`v2.10.3.x`). Der Preis: PS 3.0 ist ein Neuport von `PureSignal.cpp` und der
Setup-Seite mit Gerätetest (ANAN 10E kann PureSignal), Free-Curves brauchen
Oberfläche. Der Nutzen: das robustere PureSignal ist genau das, worüber
ANAN-Nutzer am meisten klagen. Empfehlung: **nicht jetzt**, aber als Posten
auf der ROADMAP mit Blick auf Thetis' nächstes Release.

---

## 4. Empfehlung, in Reihenfolge

1. **`delay.c`-Klemme portieren** (#1) — ein echter Fehler an einer Stelle,
   die der Bediener mit dem AmpDelay-Feld erreichen kann. Eigener kleiner PR,
   Zeus-Zitat (GPL-2.0-or-later, KB2UKA/N9WAR) im Kopf, Regressionstest:
   `SetPSTXDelay(25e-3)` bei 192 kHz darf den Ring nicht verlassen und muss
   den realisierten Wert zurückgeben.
2. **`FFTW_ESTIMATE` für Entwurfs-FFTs** (#2) — vorher messen: Filterwechsel
   bei 16 384 Taps ohne passende Wisdom, Zeit von `SetRXABandpassNC` bis
   Rückkehr. Wenn > 100 ms, portieren; sonst lassen.
3. **FIRCORE-Planteilung** (#3) — nur mit Prüfstand, der die Faltungsausgabe
   bit-gleich gegen den alten Kern hält; lohnt, wenn Punkt 2 zeigt, dass
   Filterwechsel bei uns stocken.
4. **Nicht übernehmen:** Ultra-Auflösung (#6 Taps bis 262 144), Cache-Härtung
   (#4) ohne #6, `psccF` (#10), Portabilitätsschicht (#11).
5. **Martin fragen:** TX-Filter minimalphasig als Vorgabe (#6, eine Zeile in
   `TXA.c` bzw. ein Setup-Haken) — hörbar nur als weniger TX-Latenz;
   ESSB > 3 kHz mit CESSB (#7) — falls ja, `setBandwidth_osctrl` portieren.
6. **Upstream 2.1.0 / PureSignal 3.0** — als ROADMAP-Posten, nicht als
   Nebenprodukt dieses Inventars.

Teil 3 (Stationsprotokoll v1, 296 Routen) steht noch aus.
