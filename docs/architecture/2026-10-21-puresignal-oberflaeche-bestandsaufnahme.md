# PureSignal-Oberfläche aus NereusSDR — Bestandsaufnahme

Stand 2026-10-21. Keine Zeile portiert; dies ist die Grundlage, damit
die Arbeit danach geordnet läuft statt stückweise.

## Woher der Auftrag kommt

Der Betreiber wies auf „ein neueres Interface für PureSignal" im
Upstream hin, nachdem wir einen Abend lang an einer IMD-Messung
gesucht hatten. Der Hinweis zahlte sich sofort aus: Upstreams
`docs/architecture/2026-10-12-puresignal-imd-diagnosis.md` beschreibt
unter **„double cropping"** genau den Fehler, den unser
`drawImdOverlay` hatte — ein zweiter Schnitt auf dem längst
zugeschnittenen `m_renderedPixels`. Behoben am selben Abend
(`31a594c5e`).

Seine Frage danach war berechtigt: „ich dachte, du hast alles von pure
signal von nereus geholt". Geholt wurde im Oktober das **Rechenwerk**
(WDSP 2.11, PureSignal 3, PR #298) — **nicht die Oberfläche**. Das war
eine Lücke in der damaligen Durchsicht.

## Was fehlt

**4801 Zeilen in 14 Dateien**, gemessen gegen `upstream/main`
(Code-Spitze dort: `cd059a154cf47fae90897300f20c318fa41ad643`).

| Datei | Zeilen | hängt an |
| --- | --- | --- |
| `third_party/wdsp/src/ps3_abi.h` | 82 | — (siehe unten) |
| `src/models/PureSignalSettings.{h,cpp}` | 343 | `core/NereusCoreExport.h` **fehlt auch** |
| `src/core/dsp/Ps3Snapshot.h` | — | `ps3_abi.h` |
| `src/core/dsp/Ps3DisplayAdapter.{h,cpp}` | 246 | `Ps3Snapshot.h` |
| `src/core/session/Ps3DisplayCodec.{h,cpp}` | — | Snapshot |
| `src/core/session/PureSignalSessionFacade.{h,cpp}` | 971 | vieles |
| `src/core/spectrum/MoxDisplayHold.h` | — | — |
| `src/gui/PureSignalViewStyle.h` | — | `gui/styles/WorkspacePreview.h` **fehlt auch** |
| `src/gui/PureSignalInstruments.h` | — | ViewStyle + Snapshot |
| `src/gui/ImdMeasurementFrame.h` | — | unser `ImdOverlay.h` ✓ |
| `src/gui/ImdMeasurementPresenter.{h,cpp}` | 136 | Frame |
| `src/gui/ImdMeasurementPanel.{h,cpp}` | 255 | Presenter |
| `src/gui/ImdWaterfallAnnotations.{h,cpp}` | 36 | — |
| `src/gui/MoxDisplayController.{h,cpp}` | 817 | vieles |

In eigener Fassung vorhanden und NICHT zu überschreiben:
`PureSignal.{h,cpp}`, `PsForm.{h,cpp}`, `AmpViewChart`, `AmpViewWindow`,
`ImdOverlay`, `PureSignalApplet`.

## Der Haken: es sind nicht nur 14 Dateien

**`ps3_abi.h` ist keine TAPR-Datei.** Es ist Upstreams eigene
WDSP-Erweiterung (© J.J. Boyd, KG4VCF, GPLv2-or-later, Stand
2026-09-22): eine schmale Schnittstelle, über die C++-Rufer Anzeige-
und Statusdaten aus PureSignal 3 bekommen, samt
`PS3_MAX_DISPLAY_SAMPLES 4096`, `PS3_DISPLAY_CORRECTION_POINTS 512`
und einem Abbruchweg für Dateivorgänge.

Unser vendored WDSP ist TAPR 2.11 **ohne** diese Erweiterung. Der Port
beginnt also im Fremdquelltext, nicht in der Oberfläche.

Zwei weitere Grundlagen fehlen ebenfalls (`NereusCoreExport.h`,
`gui/styles/WorkspacePreview.h`) — beides kleine Stücke, aber sie
zeigen, dass Upstream eine Infrastruktur drumherum hat.

## Reihenfolge

1. **Grundlage** — `ps3_abi.h` in das vendored WDSP, dazu die zwei
   fehlenden Kopfzeilen. Ohne das trägt nichts darüber.
2. **Daten** — `Ps3Snapshot`, `Ps3DisplayAdapter`, `Ps3DisplayCodec`.
   Hier liegt der Befund, den Upstream selbst dokumentiert: WDSP
   schreibt die Messwerte in sechzehn Hüllkurven-Eimer in
   **Eimer-Major-Reihenfolge**, die X-Werte sind darum NICHT monoton.
   Upstream behält die Reihenfolge und baut eine sortierte Kopie nur
   für die Anzeige. Wer hier sortiert, verfälscht DSP-Daten.
3. **Einstellungen** — `PureSignalSettings`.
4. **Messbild** — `ImdMeasurementFrame`, `-Presenter`, `-Panel`,
   `ImdWaterfallAnnotations`. Dazu gehört die **Frische-Regel**: ein
   Bild verfällt nach `max(1000 ms, fünf erwartete Bildabstände)`.
5. **Steuerung** — `MoxDisplayController`, `PureSignalSessionFacade`.
   Die beiden Brocken (817 + 971 Zeilen), zuletzt.

## Was Upstream selbst einschränkt

Aus deren eigener Diagnose: **kein Nachweis an echtem HF oder
PA-Koppler.** Alle Bilder und Zahlen dort sind synthetische Vorlagen;
Windows-GPU, Linux-offscreen und ein Prüfstand am Gerät stehen aus.

Unsere 14-dB-Messung am echten Sender ist insofern mehr, als Upstream
hat — ihre Absolutwerte sind durch denselben doppelten Zuschnitt
entwertet gewesen, unsere auch. Siehe
`longpath-puresignal-imd-gemessen` in der Erinnerung.

---

## Nachtrag 2026-10-22 — Upstreams sechs Befunde, bei uns nachgeprüft

Die Bestandsaufnahme oben zählt 14 Dateien und 4801 Zeilen auf, die von
NereusSDRs neuerem PureSignal-Interface zu holen wären. Der Betreiber hat
die Schätzung von drei bis fünf Tagen zu Recht angezweifelt
(„warum 3-5 Tage, wenn es eigentlich schon gut läuft"). Statt zu
kopieren wurde deshalb umgekehrt vorgegangen: Upstreams eigene Diagnose
lesen (`2026-10-12-puresignal-refresh-design.md`, Abschnitt *Confirmed
source findings*), jeden der sechs Befunde bei uns nachprüfen, und nur
beheben, was wirklich fehlt.

| | Befund | bei uns | behoben |
| --- | --- | --- | --- |
| 1 | Doppelter Zuschnitt des sichtbaren Bereichs | vorhanden | 2026-10-21 (`31a594c5e`) |
| 2 | Alle-sechs-Gating, stilles Versagen | vorhanden | 2026-10-21 (Diagnose + Ablesen als untere Schranke) |
| 3 | Analyse und EMA laufen im Malvorgang | vorhanden | 2026-10-22 (`analyseImd()`) |
| 4 | Bindung an `activeSpectrumWidget()` statt je Pan | vorhanden | 2026-10-22 (`wireImdOverlayForPan()`) |
| 5 | DUP-Vorgabe weicht ab | **trifft uns nicht** | — |
| 6 | GPU-Textur wird beim Torschluss nicht entwertet | vorhanden | 2026-10-22 (`entwerteImdSchicht()`) |

### Zu Befund 5

Upstreams neuerer `MoxDisplayController` fährt DUP aus. Bei uns steht
`m_displayDuplex{true}`, und das ist Absicht: der Panadapter bleibt beim
Senden live und zeigt den Rückweg — genau das, was Thetis' „duplex"-Modus
tut, wenn `DisplayDuplex=true` die Rückweg-DDC auf das RX1-Fenster legt
(Begründung steht an der Stelle selbst in `SpectrumWidget.h`). Auf `false`
gesetzt stünde das Zweiton-Tor aus Thetis `display.cs:5008` dauerhaft zu
und die ganze Messung wäre tot. Hier liegt keine Lücke, sondern eine
Architekturentscheidung — eine Übernahme wäre eine Regression.

### Was das für die 14 Dateien heißt

Von den sechs bestätigten Mängeln, um deren Behebung Upstreams neueres
Interface gebaut ist, sind nach dem 22.10. **fünf bei uns beseitigt und
einer gegenstandslos** — ohne eine einzige der 14 Dateien zu holen. Die
Eingriffe betrafen drei Dateien, die wir schon hatten
(`SpectrumWidget.{h,cpp}`, `MainWindow.{h,cpp}`), und kosteten zusammen
keinen halben Tag.

Damit ist der Port nicht mehr eine Frage der Fehlerbehebung, sondern eine
Frage der **Oberfläche**: Upstreams Inspektor, die verbundene AmpView und
die IMD-Schiene sind Bedienelemente, die wir so nicht haben. Ob sie
gebaut werden, ist eine Gestaltungsentscheidung des Betreibers und keine
technische Notwendigkeit mehr.

### Eine Lücke im Prüfnetz, nebenbei gefunden

`scripts/verify-no-captured-slice-spectrum-wiring.py` meldet wörtlich
„no slice control wired to activeSpectrumWidget()" — und war grün,
während Befund 4 genau diese Bindung hatte. Das Skript prüft nur
Scheiben-Bedienelemente, nicht Anzeigezustände wie MOX, Zweitongenerator
oder Messhaken. Wer die Prüfung ausweitet, findet mit einiger
Wahrscheinlichkeit weitere Einmal-Bindungen derselben Art.
