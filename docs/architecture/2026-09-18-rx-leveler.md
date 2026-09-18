# RX-Leveler — aus dem Zeus Station Engine portiert

**Stand:** 18. September 2026
**Anlass:** Punkt 5 der Benchmark-Liste in
[docs/design/2026-09-17-zeus-benchmark.md](../design/2026-09-17-zeus-benchmark.md)
(„weiter" — nach dem CW-Decoder der nächste Technik-Punkt, der nicht
mit der Gestaltungs-Sitzung kollidiert). Zeus zeigt „RX LVLR" als
Schalter in seiner DSP-Leiste mit einem Auto/Custom-Profil; der ganze
Regler liegt offen im GPL-Engine (`Station.Engine.Hosting/
DspPipelineService.cs`, ~300 Zeilen Kern + Beweis-Gatter, dazu
`InPassbandSnrEstimator.cs`, 290 Zeilen). Thetis hat nichts
Vergleichbares — Zeus sagt es selbst: „this always-on leveler (which
Thetis lacks)".

## Was das Ding tut

WDSPs AGC hält das *Signal* auf Pegel; es kann eine schwache Station
nicht von Bandrauschen unterscheiden. Steht AGC-T für die schwache
Station, kommt das Rauschen zwischen den Wörtern genauso laut. Der
Leveler sitzt **nach** der ganzen WDSP-Kette (nach AGC, nach NR, nach
AF-Gain) und regelt die *empfundene Lautheit* langsam auf einen
Zielwert (−18 dBFS Effektivwert):

* **Absenken ist bedingungslos** — eine laut ankommende Station wird
  noch im selben Block eingefangen; eine Peak-Wache verhindert, dass
  über eine Pause „gesparte" Verstärkung auf den ersten lauten Block
  gekippt wird („blasts the speaker" — genau der Fehler, den der
  Leveler abstellen soll).
* **Anheben ist fail-closed** — positive Verstärkung gibt es nur,
  solange eine *unabhängige HF-Messung* (der Passband-SNR-Schätzer aus
  dem Spektrum) ein aufgelöstes Signal im Durchlassbereich bestätigt,
  und nie in der Nähe von ADC-Übersteuerung (> −3 dBFS). Audio-Amplitude
  allein kann Rauschen nicht von Sprache unterscheiden; Rauschen allein
  kann die Anhebung also nie öffnen.
* **Pausen** (unter −50 dBFS) blenden die angewandte Verstärkung auf
  Eins, der Regler merkt sich seinen Wert 600 ms (Hang) und baut ihn
  dann mit 4,5 dB je 33 ms ab — kein Neuanlauf nach jedem Wort, kein
  angehobenes Rauschen.

Auto = die Konstanten aus Zeus. Eigene = Ziel (−30…−6 dBFS), maximale
Anhebung (0…24 dB), Attack (20…2000 ms), Release (20…5000 ms), Hang
(0…2000 ms) — exakt Zeus' `RxLevelerConfig`.

## Was gebaut wurde

| Datei | Herkunft | Inhalt |
| --- | --- | --- |
| `src/core/audio/RxAudioLeveler.{h,cpp}` | **Port** aus Zeus @8970f2d (`DspPipelineService.cs:174-280, 336-363, 384-701, 839-848, 4453-4456, 10245-10364`; `Dtos.cs:862-893`; `RadioService.cs:4248-4266`) | Zustand, 40 Konstanten, `process()` (Kern), Softlimiter (Knie 0,74, Decke 0,84), Beweis-Automat (`advanceEvidence`, `evidenceIsCurrent`, `evidenceAllowsBoost`), Profil samt Normalisierung. Kein Qt, keine Allokation. |
| `src/core/InPassbandSnrEstimator.{h,cpp}` | **Port** aus Zeus @8970f2d (`InPassbandSnrEstimator.cs`, ganze Datei) | SNR im Durchlassbereich aus einem dB/Hz-Spektrum: robuste Mediane (MAD) zweier bewachter Referenzzonen links und rechts, Steigung dazwischen, über den Durchlass integriert und von der Gesamtleistung abgezogen; verweigert die Aussage bei < 4 Referenz-Bins je Seite, > 30 dB Kippung (belegtes Nachbar-Seitenband) oder Überschuss unter 3,5 σ. Konfidenz 0,01…1. Settle 2,25 s je Schlüssel (Filter, Abstimmung, Geometrie), Neustart nach jedem Tastintervall. |
| `src/models/PassbandSnrTracker.{h,cpp}` | Longpath-original | Die Brücke: nimmt jeden Stream-Frame aus `FFTEngine::fftReadyLinear`, mittelt linear (τ 0,75 s, WDSPs `avenger()` Fall 1 mit Frame-Zeitstempel statt fester fps), normiert auf 1 Hz (`norm_oneHz`), wertet alle 200 ms je Slice (Zeus' 5-Hz-Kadenz: 3 Treffer = 0,6 s öffnen, 6 Fehlschläge = 1,2 s schließen), liest den ADC-Peak über den `RxChannel`, treibt den Beweis-Automaten und schreibt (`resolved`, `adcRisk`, Zeitstempel) atomar in den Kanal. Zieht Durchlassbereich und DDC-Mitte aus dem `SliceModel` (pull, keine Signal-Verdrahtung). Signal `signalQualityUpdated(slice, snrDb, confidence, resolved)` für eine spätere Anzeige. |
| `RxChannel::applyRxLeveler` | Longpath-original Einbau | Letzte Stufe in `processIq()` — nach DFNR/MNR, **vor** dem TCI-Abgriff, damit Lautsprecher, VAX, TCI und die Decoder-Abgriffe dasselbe hören (Zeus regelt seinen ganzen RX-Bus). Sechs Parameter-Atomics, drei Beweis-Atomics, Alter der Beweise auf dem DSP-Faden geprüft (750 ms). AF-Gain-Offset `20·log10(afGain)` als Referenzverschiebung, damit der Leveler den Lautstärkeregler nicht aufhebt. |
| `SliceModel` | Longpath-original | `levelerEnabled` + Profil als Q_PROPERTYs, je Slice persistiert (nicht je Band — eine Lautheits-Vorliebe, keine Bandeinstellung). Aus: Vorgabe wie bei Zeus (`RxLevelerEnabled = false`). |
| `RxApplet` + `DspQuickPopup::showRxLeveler` | Longpath-original | `LVL`-Schalter in der Lautstärke-Reihe neben `BIN`; Rechtsklick öffnet das Profil (Auto/Eigene + fünf Regler mit Zeus' Bereichen, Reset) im Schnellregler-Idiom der NR-Knöpfe. |

## Vier Abweichungen vom Original — und warum

Alle vier sind im Quelltext markiert („Longpath deviation D1…D4");
der Rest ist Zeile für Zeile zitiert.

1. **D1 Stereo.** Zeus regelt einen Mono-Bus. Longpaths WDSP-Ausgang ist
   ein L/R-Paar (Dual-Mono oder binaural). Eine Verstärkung aus beiden
   Kanälen (RMS über beide, Peak über beide), auf beide angewandt — das
   Stereobild verschiebt sich nie. Prüfstand: 6,02 dB Kanalabstand
   bleibt auf 0,05 dB erhalten.
2. **D2 Blockraten-Unabhängigkeit.** Zeus' „je Block"-Konstanten (2 dB
   Anhebung, 6 dB Absenkung, 18 Blöcke Hang, 4,5 dB Abbau) sind für
   seinen 30-Hz-Audio-Tick (~1600 Frames) gedacht. Longpath reicht dem
   Leveler je WDSP-Ausgabeblock (256 Frames bei 192 kHz Eingang = 5,3 ms).
   Ungeskaliert liefe alles sechsmal so schnell (375 dB/s Anhebung, 96 ms
   Hang). Jede Block-Konstante wird mit `blockMs / (1000/30)` skaliert —
   dieselbe Umrechnung, die Zeus selbst für Attack/Release/Hang des
   Custom-Profils macht. Prüfstand: 256- und 1600-Frame-Blöcke erreichen
   nach 0,2 s dieselbe Verstärkung (±1,5 dB).
3. **D3 Gegenprüfung aus demselben Spektrum.** Zeus prüft den gemittelten
   Schätzer gegen den *aktuellen* WDSP-Stufenmesser (`RXA_S_AV` +
   S-Meter-Kalibrierung), damit eine verschwundene Station keinen alten
   Mittelwert am Leben hält. Longpath kalibriert S-Meter und Spektrum
   getrennt; ein Vergleich Messer-gegen-Spektrum trüge diesen Versatz.
   Die „aktuelle" Leistung ist daher die ungemittelte Passband-Leistung
   des laufenden Frames — gleiche Skala, gleiche Absicht, die Schwellen
   1,5 / 0,25 dB bedeuten, was sie sagen. Der Gatter-Rechenweg ist
   unverändert (`rxCalibrationDb = 0`).
4. **D4 Aus heißt aus.** Zeus lässt bei „RX LVLR off" Absenkung und
   Softlimiter weiterlaufen (nur die Anhebung schließt). Longpaths
   bestehender Audiopfad darf bei ausgeschaltetem Leveler nicht
   angefasst werden (kein geändertes Vorgabe-Verhalten) — also echter
   Bypass, mit einer Ausnahme: nach dem Abschalten wird noch vorhandene
   positive Verstärkung über Blöcke auf Eins abgebaut (6 dB je 33 ms),
   damit man ein Ausblenden hört statt eines Sprungs; dann wird der
   Regler vergessen und der Block geht unverändert durch. Eine gehaltene
   Absenkung endet an der Blockgrenze. Dieser Punkt hat einen echten
   Fehler gefunden: die erste Fassung merkte sich „im Abbau" erst *nach*
   dem Block und ließ die Absenkung beim Abschalten deshalb aus — der
   Tracker-Prüfstand fiel durch, der Bypass fragt jetzt den
   Regler-Zustand selbst.

Was bewusst wie bei Zeus blieb: kein Modus-Ausschluss (auch DIGU/DIGL
werden geregelt, wenn eingeschaltet — Zeus regelt seinen TCI-Strom mit);
das Tastintervall lässt den Beweis über die ordentlichen sechs
Fehlschläge fallen, nicht sofort (während MOX ist der RX-Ton ohnehin
stumm); die Anzeige der Diagnosewerte gibt es nur als 1-Hz-Logzeile
(`QT_LOGGING_RULES="nereus.dsp.debug=true"`), nicht im Applet.

## Datenweg

```
FFTEngine (Stream n) ──fftReadyLinear──▶ PassbandSnrTracker (Hauptfaden)
                                          │  linear mitteln, dB/Hz
                                          │  alle 200 ms je Slice:
                                          │    InPassbandSnrEstimator
                                          │    ADC-Peak (RXA_ADC_PK)
                                          │    Beweis-Automat
                                          ▼
                                   RxChannel (Atomics: resolved, adcRisk, ms)
                                          │
RxDspWorker (DSP-Faden) ─▶ fexchange2 ─▶ NR ─▶ applyRxLeveler ─▶ Abgriffe ─▶ Mixer
```

## Prüfung

`tests/tst_rx_audio_leveler.cpp` (18 Fälle): Anhebung ohne Beweis
bleibt zu, mit Beweis −40 → −18 dBFS (+22 dB) und Ausgang am Ziel;
Rate blockgrößen-unabhängig (D2); laute Ankunft ohne Beweis im Block
abgefangen, Decke 0,84 nie überschritten, Peak-Ziel 0,74 nach Einregeln;
+22 dB gehalten und dann 0,5-Peak-Block: keine Übersteuerung (Peak-Wache);
Abschalten baut in 4 Blöcken ab und räumt den Zustand; Pause blendet
sofort, hält 18 Blöcke, baut 4,5 dB je Block ab; Stereo (D1); Custom
Attack 100 vs. 2000 ms; AF −6 dB verschiebt das Ziel; Beweis-Automat 3/6,
ADC-Veto, veralteter Frame; Alter 750 ms; Überschuss-Schwellen;
Softlimiter monoton; Normalisierung; NaN-Eingang → Stille.
`tests/tst_in_passband_snr_estimator.cpp` (10 Fälle): −100-dB-Ton über
−140 dB/Hz → SNR ≈ +6,2 dB auf 1 dB, Rauschen allein verweigert, LSB,
300-dB-Kippung verweigert, 20-dB-Kippung gefolgt, Durchlass außerhalb,
Settle/Keying/Schlüsselwechsel.
`tests/tst_passband_snr_tracker.cpp` (9 Fälle, echter WDSP-Kanal):
Ton im Durchlass → nach Settle + 3 Verdikten `resolved`, SNR ≈ +17 dB,
Kanal-Gatter offen und ein schwacher Block wird angehoben; Ton weg →
nach 6 Fehlschlägen zu; ADC −1 dBFS → sofort Veto; Keying; fremder
Stream; **Kanal-Bypass bei Aus + Abbau beim Abschalten (D4)**; veralteter
Beweis schließt.
`tests/tst_rx_leveler_ui.cpp` (5 Fälle): LVL folgt dem Slice in beide
Richtungen; Rechtsklick-Profil zeigt Auto/Eigene und fünf Regler mit
Zeus' Grenzen, schreibt ins Modell, Reset; Einstellungen überleben
Speichern/Laden, Normalisierung beim Laden.

App gebaut, `RxApplet` über die Automationsbrücke gerendert (LVL neben
BIN). **Offen: Live-Test am Radio** — beim Bau waren drei Longpath-
Instanzen mit dem Radio unterwegs, ein vierter Client kam nicht in Frage.
Was am Gerät zu prüfen ist: schwache Station in SSB, `LVL` ein, nach
~3 s (Settle) muss die Logzeile `boost yes` zeigen und die Lautheit
hochkommen, ohne dass das Rauschen zwischen den Wörtern mitkommt; dann
AGC-T zudrehen und eine laute Station: kein Blast, Logzeile `applied`
negativ; Sendezweig: nach MOX ~3 s Pause, bevor die Anhebung wieder
öffnet.

## Herkunft

`docs/attribution/ZEUS-PROVENANCE.md` — drei neue Zeilen (Leveler,
Schätzer, Brücke als Referenz). Zeus-Engine ist GPL-2.0-or-later;
„Zeus"/„ZeusSDR" sind Marken der Betreiber, hier nominativ genannt.
