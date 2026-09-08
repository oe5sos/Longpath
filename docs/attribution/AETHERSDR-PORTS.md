# AetherSDR whole-file ports

AetherSDR (https://github.com/aethersdr/AetherSDR) is GPLv3, as is
NereusSDR, so ported code carries forward under the same licence with
attribution per GPLv3 §5. Every file below has the project-level
AetherSDR copyright block in its own header; this table is the index.

Upstream reference: `../AetherSDR` at `3a1f59e`; the channel-strip
DSP below was taken later, at `31b29583`.

The wider structural debt to AetherSDR (applet panel shape, style
palette, meter widgets, CtyDatParser, AdifParser) is catalogued in
`aethersdr-reconciliation.md` and `aethersdr-contributor-index.md`.
This file tracks *whole-file ports* added after that reconciliation.

| NereusSDR file | AetherSDR source | Rev | Ported | Notes |
|---|---|---|---|---|
| `src/core/CallsignInfo.h` | `src/core/CallsignInfo.h`, `src/core/CallsignUtils.h` | `3a1f59e` | 2026-08-07 | Namespace change. JSON cache round-trip and cty.dat prefix-fallback fields omitted — NereusSDR reads cty.dat through its own `CtyDatParser` and has no lookup cache. |
| `src/core/QrzClient.h` | `src/core/QrzClient.h` | `3a1f59e` | 2026-08-07 | Namespace change; log category localised. |
| `src/core/QrzClient.cpp` | `src/core/QrzClient.cpp` | `3a1f59e` | 2026-08-07 | Namespace change; `LogManager` category → local `Q_LOGGING_CATEGORY(lcQrz)`; user agent renamed; `nickname` field dropped with the struct field. |
| `src/core/strip/ClientPhaseRotator.h`, `.cpp` | `src/core/ClientPhaseRotator.{h,cpp}` | `31b29583` | 2026-08-08 | Namespace change; include paths rebased onto `core/strip/`. DSP unchanged. |
| `src/core/strip/ClientGate.h`, `.cpp` | `src/core/ClientGate.{h,cpp}` | `31b29583` | 2026-08-08 | As above. Downward expander / noise gate, first stage of the chain. |
| `src/core/strip/ClientEq.h`, `.cpp` | `src/core/ClientEq.{h,cpp}` | `31b29583` | 2026-08-08 | As above. |
| `src/core/strip/ClientDeEss.h`, `.cpp` | `src/core/ClientDeEss.{h,cpp}` | `31b29583` | 2026-08-08 | As above. |
| `src/core/strip/ClientComp.h`, `.cpp` | `src/core/ClientComp.{h,cpp}` | `31b29583` | 2026-08-08 | As above. |
| `src/core/strip/ClientTube.h`, `.cpp` | `src/core/ClientTube.{h,cpp}` | `31b29583` | 2026-08-08 | As above. |
| `src/core/strip/ClientPudu.h`, `.cpp` | `src/core/ClientPudu.{h,cpp}` | `31b29583` | 2026-08-08 | As above. This is the AetherVoice exciter. |
| `src/core/strip/ClientReverb.h`, `.cpp` | `src/core/ClientReverb.{h,cpp}` | `31b29583` | 2026-08-08 | As above. No `setEnabled()` upstream; wet mix at zero is its bypass. |
| `src/core/strip/ClientFinalLimiter.h`, `.cpp` | `src/core/ClientFinalLimiter.{h,cpp}` | `31b29583` | 2026-08-08 | As above. No `setEnabled()` upstream — a brickwall that can be switched off is not one. |
| `src/core/SignalHistoryStore.h`, `.cpp` | `src/gui/MainWindow.{h,cpp}` — `SHistoryEntry` (`MainWindow.h:1023-1046`), `rebuildSHistoryForPan` (`:9568-9676`), `expireSHistoryMarkers` (`:9732-9755`), hit-merge from `onSpectrumReadyForSHistory` (`:9830-9877`) | `0cd4559` | 2026-08-19 | Extracted from `MainWindow` into a standalone, clock-injected class — upstream calls `QDateTime::currentMSecsSinceEpoch()` mid-computation, which makes the 2-minute voice→QRM rule untestable. All time constants and thresholds unchanged. Four documented deviations in the header: own class; no `carrierScore` (the CNN branch is dead upstream too — no model ships); one entry list instead of `QHash<panId, …>` (single pan until 3F); thresholds as fields instead of mid-computation `AppSettings` reads. Parameter renamed `signals` → `detections`: `signals` is a Qt macro for `public`. |
| `src/core/VoiceSignalDetector.h`, `.cpp` | `src/core/VoiceSignalDetector.{h,cpp}` | `0cd4559` | 2026-08-19 | Namespace change; `LogManager.h` → local `Q_LOGGING_CATEGORY(lcSHistory)`. Detection logic, thresholds and comments unchanged. The companion `SignalClassifier` (ONNX) is deliberately **not** ported: the detector never calls it, and upstream ships no `.onnx` model — see the header for the measurement. Upstream comment error preserved verbatim with a note beside it (`sLabel` example says `-85 → "S8"`; the scale and the code both give S7). |

## Why the upstream issue numbers stay in the comments

`#3990` (session-retry flag leak; latitude accepted without longitude)
and `#4043` (`<QRZDatabase>` root element swallowing `<Session>`) refer
to AetherSDR's tracker, not ours. They are kept because they document
failures that are silent until they bite: a half-coordinate pair puts
every European station in the Gulf of Guinea, and the root-element bug
broke login end-to-end with no visible error. `tests/tst_qrz_client.cpp`
pins both.

## Not ported

AetherSDR keeps QRZ passwords in the OS keychain via QtKeychain.
NereusSDR has no such dependency, so `src/core/CredentialStore.{h,cpp}`
is NereusSDR-original: macOS login keychain through the `security` CLI,
session-memory everywhere else, with `isPersistent()` so the UI can say
which of the two it got.

## ZeusSDR — design inspiration, no code and no data

The channel strip's equaliser picture was shown ZeusSDR's CFC editor as
a reference: numbered knots, two curves on one plot with a legend, a
numeric table under the graph, named preset buttons, a filled area under
the curve. Those patterns influenced the layout here.

ZeusSDR is not open source and its licence is unknown to this project.
So the line drawn is:

**Taken:** the shape of the interaction. Numbering the knots, putting
the numbers under the picture, naming the presets, drawing the target as
a second dashed curve. Layout conventions are not anybody's property and
several of these predate both programs by decades in studio equipment.

**Not taken:** any code, and any data. In particular the CFC preset
values visible in the reference screenshots — FLAT / VOICE / STUDIO /
ESSB / DX, each with ten frequency, compression and post-gain figures —
were NOT copied. Somebody chose those numbers and that is their work.

Every curve in `src/core/strip/StripTargets.cpp` is this project's own,
derived from the transmit bandwidth each profile is for and from what a
speech spectrum needs to survive it. They are opinions, they are
labelled as opinions in the code, and they can be argued with on their
own terms — which would be impossible if their real origin were an
undocumented copy.

This note exists because the alternative is somebody finding those
curves in two years, noticing the resemblance to a screenshot in the
issue tracker, and having no way to tell whether the project has a
licensing problem. It does not.

## Channel-strip stage graphics — behaviour taken from AetherSDR

`src/gui/applets/StripGraphics.cpp`, 2026-08-09. No code copied; these
widgets are NereusSDR-original and are written against NereusSDR's own
style tokens. What was taken is a set of decisions, from AetherSDR's
`src/gui/ClientCompCurveWidget.cpp` and `ClientGateCurveWidget.cpp` at
`31b29583`, after the bench reported that our first attempt was hard to
read and named AetherSDR as the thing that gets it right.

Read, and adopted:

  - **The ball is a one-pole smoother, not a peak hold.** Alpha 0.30 per
    tick on the widget's own 33 ms timer. AetherSDR's comment gives the
    reason — it "keeps the ball from twitching on silent frames where
    the peak meter reads -120 dBFS". Our first version used peak hold
    with decay, copied from our own level bars, which is right for a bar
    and wrong for a ball: it snaps up on every syllable.
  - **A 30 Hz timer per widget** rather than the window's 10 Hz meter
    tick. At 10 Hz a gliding ball is visibly stepped.
  - **A radial-gradient glow with a white core**, so the ball reads as a
    light source and stays findable over an amber threshold line.
  - **Labelled major ticks on BOTH axes**, minors between, unity dashed.
    We had deliberately left the axes bare; that was the main reason the
    picture was hard to read.
  - **The gate's deadband as a shaded band**, not two lines. The
    operator's question is whether the ball is inside the sticky zone,
    and a region answers it by containing the ball.
  - **A tick range of -80 dB for the gate** and -60 for the others,
    because a gate attenuates below where a compressor ever goes.
  - **Amber for the gate, cyan for the rest**, so the colour says what
    kind of stage it is before the title is read.

Constants that are AetherSDR's judgement rather than arithmetic — the
0.30 smoothing alpha, the 33 ms interval, the 12 dB tick spacing — are
noted as theirs at the point of use in `StripGraphics.h`.

## The equaliser's user interface — whole-file ports

Requested at the bench: "copy AetherSDR's EQ, functions and display,
1:1". `core/strip/ClientEq` was already a verbatim port of the same
upstream, so this reunites the DSP with the interface written for it.

| NereusSDR file | AetherSDR source | Rev | Ported | Notes |
|---|---|---|---|---|
| `src/gui/applets/eq/ClientEqCurveWidget.h`, `.cpp` | `src/gui/ClientEqCurveWidget.{h,cpp}` | `31b29583` | 2026-08-09, repainted 2026-08-11 | Namespace change; includes rebased. **All** colours now come from EqPalette.h — see below. Behaviour unchanged. |
| `src/gui/applets/eq/ClientEqFftAnalyzer.h`, `.cpp` | `src/gui/ClientEqFftAnalyzer.{h,cpp}` | `31b29583` | 2026-08-09 | Namespace change only. Takes `update(samples, count)`, so NereusSDR's `MicSpectrum` can feed it unchanged. |
| `src/gui/applets/eq/ClientEqEditorCanvas.h`, `.cpp` | `src/gui/ClientEqEditorCanvas.{h,cpp}` | `31b29583` | 2026-08-09 | Namespace change; includes rebased. |
| `src/gui/applets/eq/ClientEqParamRow.h`, `.cpp` | `src/gui/ClientEqParamRow.{h,cpp}` | `31b29583` | 2026-08-09 | As above. |
| `src/gui/applets/eq/ClientEqIconRow.h`, `.cpp` | `src/gui/ClientEqIconRow.{h,cpp}` | `31b29583` | 2026-08-09 | As above. |
| `src/gui/applets/eq/ClientEqOutputFader.h`, `.cpp` | `src/gui/ClientEqOutputFader.{h,cpp}` | `31b29583` | 2026-08-09 | Namespace change; includes rebased. |
| `src/gui/applets/eq/StripEqPanel.h`, `.cpp` | `src/gui/StripEqPanel.{h,cpp}` | `31b29583` | 2026-08-09, extended 2026-08-11 | Namespace change; includes rebased; talks to `EqHost` instead of AetherSDR's `AudioEngine`. 2026-08-11: NereusSDR-original Undo/Redo buttons + Ctrl+Z / Ctrl+Shift+Z window-scoped shortcuts added (backed by `EqHistory`/`EqHost`, both NereusSDR-original); three copies of the row-button stylesheet folded into `kRowButtonStyle`. |
| `src/core/ClientPuduMonitor.h`, `.cpp` | `src/core/ClientPuduMonitor.{h,cpp}` | `31b29583` | 2026-08-11 | The record-then-listen monitor, ported whole at the bench's request ("copy everything from Aether, A to Z"). Namespace change; includes rebased. Three named divergences, each forced by a missing AetherSDR subsystem: format ladder spelled out locally (no `AudioDeviceNegotiator`), `qCInfo/qCWarning(lcAudio)` instead of `AudioSummaryLogger`, `setOutputDevice` kept but unrouted (no `AudioOutputRouter`). Capture format unchanged (int16 stereo 24 kHz); MainWindow's feed lambda converts NereusSDR's float-mono-48k post-strip tap. Wiring in `MainWindow::wirePuduMonitor` mirrors upstream's MainWindow structure: strip-hosted buttons, `muteRxRequested` RX gate, auto-play on `recordingStopped`. |

### Colours: kept as NereusSDR's, which cost almost nothing

The bench asked for AetherSDR's functions with NereusSDR's colours. That
turned out to be nearly free, and the reason is worth recording: **seven
of the nine colours these widgets paint with are byte-identical to
NereusSDR's own style tokens**, because NereusSDR's palette descends
from AetherSDR in the first place — see `aethersdr-reconciliation.md`,
which lists the style palette among the structural debt.

  #0a0a18 → kPanelBg        #00b4d8 → kAccent
  #203040 → kButtonHover    #0f0f1a → kAppBg
  #405060 → kTextInactive   #1a2a38 → kTitleGradBot
  #304050 → kOverlayBorder  #0070c0 → kBlueBg

Those are now written as the tokens in painting code, so they follow
NereusSDR if it ever repaints. Inside Qt stylesheet strings they are
left as literals: substituting there means `.arg()` formatting through a
verbatim port, which is churn for no visual difference, since the values
already agree.

**Superseded 2026-08-11 — every colour is now NereusSDR's.**

The paragraph above used to say that six literals had no NereusSDR
equivalent and were left alone, on the reasoning that a colour chosen to
sit between two others stops working when snapped to one of them. Sound
reasoning; it did not survive the instruction that the equaliser is to
look like the rest of the program and nothing else.

Every colour in `src/gui/applets/eq/` now comes from
`src/gui/applets/eq/EqPalette.h`, which is **one table** mapping
AetherSDR's palette onto NereusSDR style tokens. The borrowed files
reference names from that table; none of them contains a hex literal.

That is deliberate and it is the same device as EqHost: a re-sync
against upstream re-applies one substitution rather than reconstructing
a hundred separate decisions. The port is no longer byte-identical to
`31b29583`, but the difference is mechanical and reviewable in a single
header.

**Three new colours.** The eight-hue band palette is functional — it is
how one band is told from another when several overlap, and it could not
be dropped. NereusSDR had four accents to spend on it, so a coral, a
blue and a violet were added to `StyleConstants.h` as
`kEqBand0`..`kEqBand7`, spaced to stay apart on #0a0a18.

**One behavioural change, and it is not AetherSDR's.** The default band
layout is NereusSDR's own — see `src/core/strip/EqBandLayout.h`.
`ClientEq::defaultBand()` is untouched; the panel and the presets call
the NereusSDR table instead, which adds seven spare shaping handles at
unity gain and switched off. AetherSDR's DSP is unaffected and the three
presets sound exactly as they did.
| `src/gui/FramelessMoveHelper.h` | `src/gui/FramelessMoveHelper.h` | `31b29583` | 2026-08-20 | Nur Namensraum geaendert. Fenster an der Titelleiste ziehen; auf macOS bewusst OHNE startSystemMove (manuell mit grabMouse). |
| `src/gui/FramelessResizer.{h,cpp}` | `src/gui/FramelessResizer.{h,cpp}` | `31b29583` | 2026-08-20 | Nur Namensraum geaendert. Groessenaenderung an allen Kanten und Ecken eines rahmenlosen Fensters, Filter auf dem QWindow statt im Widgetbaum. |
| `src/gui/FloatingContainerWindow.{h,cpp}` | `src/gui/WindowChrome.{h,cpp}` | `31b29583` | 2026-08-20 | Strukturell nachgebaut, nicht uebersetzt: eigene Titelleiste als Ziehgriff plus sichtbarer Anfasser unten rechts. Longpath-eigene Zutaten: der gelbe Zeus-Streifen links, der Andock-Knopf, und WA_NativeWindow auf dem Anfasser (macOS-Notwendigkeit wegen des nativen QRhi-Panadapters). |

## 3D stacked-trace spectrum surface ("3DSS")

AetherSDR v26.7.1 added a perspective stacked-trace display mode
("3DSS") as a toggle on its existing `SpectrumWidget`, not a separate
widget: a rolling history of FFT rows drawn back-to-front as a
receding trapezoid, height anchored to the noise floor. AetherSDR
ships both a GPU height-map mesh path (dedicated shaders, a static
~1.47M-vertex buffer, ring-buffered height texture) and a CPU QImage
fallback for when the mesh cannot be created. Longpath's first port
takes only the CPU path — see `src/gui/DssRenderer.h` for the full
rationale (no new shader/pipeline needed; the CPU surface composites
through the existing overlay textured-quad pipeline) and what was
deliberately left out (GPU mesh, native-tile "supplemental" overhang
channels — Longpath has no native-tile waterfall source — reprojection
on pan/zoom, deep scrollback history).

| NereusSDR file | AetherSDR source | Rev | Ported | Notes |
|---|---|---|---|---|
| `src/gui/DssRenderer.h`, `.cpp` | `src/gui/DssRenderer.{h,cpp}` | `31b29583` | 2026-09-03 | Reduced-scope CPU-only port: ring buffer + `pushRow` (resample, median-of-3, temporal blend) + `image()` and the geometry/colour passes of `rebuild()` carried over close to verbatim, including the shared geometry constants. `rebuild()`'s drawing pass is Longpath-original (2026-09-03): AetherSDR's QPainter painter's-algorithm surface (767 trapezoids + 767 AA lines per trace, back to front) measured 0.6–1.3 s per frame on an Apple-silicon MacBook Air; replaced by a front-to-back horizon rasteriser with the pen width reproduced as coverage — equivalent picture, 7–10 ms. GPU mesh accessors, `pushRowWithSupplemental`, `reprojectFrequencyFrame`, deep history scrollback, and `dssDepthVisibleSegments` omitted — see the file header. `SpectrumWidget` integration (`SpectrumRenderMode`, the "Spectrum: 2D / 3D" combo in `SpectrumOverlayPanel`'s Display flyout, the GPU-texture upload of the rendered QImage through the existing overlay pipeline) is Longpath-original, not a port: AetherSDR wires its GPU mesh path through `dss_mesh.vert`/`.frag` (not ported) and a "3D VIEW" menu section with Floor/Gain/Span sliders (not yet ported — Longpath's v1 uses AetherSDR's own defaults, floor offset −6 dB and zCurve 0.70, in place of dedicated controls). |
| `src/core/DevAutomationServer.h`, `.cpp` | `src/core/AutomationServer.{h,cpp}`, `docs/automation-bridge.md` | `d58e2b8a` | 2026-09-06 | Structural derivative, not a line-by-line port -- AetherSDR's file is ~12,000 lines across four phases and 25 verbs; this is a from-scratch, much smaller Phase 0 (read-only: `dumpTree` + `grab`), written for Longpath's own widget vocabulary. Carried over: the opt-in env-var gate (off unless `LONGPATH_AUTOMATION` is set), the `QLocalServer` line/JSON protocol shape, the per-widget JSON fields (class, objectName, accessibleName, enabled, visible, geometry, checked/text for checkable buttons, range for sliders/spinboxes, combo items, a generic best-effort `value`), and -- the one piece that is a direct, load-bearing technique, not just a shape -- `QRhiWidget::grabFramebuffer()` for GPU-widget capture (a real Qt 6.7+ API; AetherSDR's own comment pointed at exactly this call, `w->grab()` alone returns an empty image for a `QRhiWidget`). Target resolution widened same-day (2026-09-06) to fall back from objectName to a namespace-stripped class-name match, after dogfooding found the live panadapter carries no objectName of its own -- AetherSDR's own `resolveWidget()` documents the identical fallback for the identical reason. Everything else in AetherSDR's file (interact/drive verbs, live model reads, TX-safety gating, multi-instance identity, the MCP wrapper) is out of scope for this file -- see `docs/architecture/2026-09-06-dev-automation-bridge-design.md` for the phased plan. |
| `src/core/RttyDecoder.h`, `.cpp` | `src/core/RttyDecoder.{h,cpp}` | `d58e2b8a` | 2026-09-06 | Near-verbatim port of AetherSDR's native RTTY (Baudot/ITA2) decoder -- no Thetis equivalent exists at all, so this is sole-source like the automation bridge above. Carried over close to verbatim: mark/space biquad bandpass design, envelope-based Schmitt-trigger bit slicing, proportional (25%-per-edge) clock recovery, the 5-bit shift-register/LTRS-FIGS state machine, and the Baudot/ITA2 character tables (the international standard, not AetherSDR-original). Sample rate re-derived for Longpath's native 48 kHz WDSP RX output instead of AetherSDR's 24 kHz (the biquad design already parameterizes on sample rate, so this needed no resampler). Default mark/shift deliberately NOT copied from AetherSDR's own numeric defaults (2125/170 Hz) -- reads the already-existing, Thetis-sourced `SliceModel::rttyMarkHz`/`rttyShiftHz` (2295/170 Hz, `setup.designer.cs:40635-40665 [v2.10.3.13]`) instead, since that's the value the operator already tunes via `RxApplet`'s `RttyMarkShiftContainer`. `feedAudio()`'s signature changed to a raw `(const float*, int frames)` pointer/count pair instead of AetherSDR's `QByteArray`, to match Longpath's `AudioTapRing::read()` shape. Verified end-to-end (not just eyeballed) against a synthetic AFSK signal in `tests/tst_rtty_decoder.cpp`. |
| `src/gui/RttyDecoderSensitivity.h` | `src/gui/RttyDecoderSensitivity.h` | `d58e2b8a` | 2026-09-06 | Near-verbatim port: identical slider-to-confidence-threshold formula (0..100 -> 0.50..0.95) and rationale. Namespace only. |
| `src/gui/applets/RttyDecoderApplet.h`, `.cpp` | `src/gui/PanadapterApplet.cpp` (RTTY control set) | `d58e2b8a` | 2026-09-06 | Structural derivative, not a port -- Longpath has no `PanadapterApplet` equivalent, so the widget tree is new code against Longpath's own `AppletWidget` base class. The control SET (decoded-text output, mark/space level meters via `HGauge`, a lock/SNR status capsule, baud-rate/reverse-polarity/sensitivity controls) mirrors AetherSDR's RTTY panel. Mark/Shift are shown read-only here rather than duplicated as editable controls, since `RxApplet`'s existing `RttyMarkShiftContainer` already edits the same Thetis-sourced `SliceModel` fields -- this applet just reads them live. Visibility gate (`DSPMode::DIGL` only, "RTTY is a DIGL submode") is Longpath-native, matching the already-documented rule in `RxApplet::applyModeVisibility`, not an AetherSDR behavior. |

The rows below close a 2026-09-08 compliance-sweep gap: the first real
full-tree CI run against this branch (`CHECK_NEW_PORTS_FULL=1
scripts/check-new-ports.py`, added 2026-05-13, never actually executed
for this branch before its first pull request) found that every KiwiSDR
file, the ASR backend, and a handful of others already carry a complete
`// Ported from AetherSDR <path> [@31b29583]` header — they were simply
never given a row here. No header content changed; this is registration
only.

| `src/core/KiwiSdrClient.h`, `.cpp` | `src/core/KiwiSdrClient.{h,cpp}` | `31b29583` | 2026-08-23 | KiwiSDR client connection (Stufe 2). Namespace change, include paths rebased; KiwiSDR wire protocol itself credited to John Seamons (ZL/KF6VO), kiwisdr.com. |
| `src/core/KiwiSdrManager.h`, `.cpp` | `src/core/KiwiSdrManager.{h,cpp}` | `31b29583` | 2026-08-23 | Namespace change only. |
| `src/core/KiwiSdrProtocol.h`, `.cpp` | `src/core/KiwiSdrProtocol.{h,cpp}` | `31b29583` | 2026-08-23 | Namespace change only. |
| `src/core/KiwiSdrCredentialStore.h`, `.cpp` | `src/core/KiwiSdrCredentialStore.{h,cpp}` | `31b29583` | 2026-08-23 | Namespace change only. |
| `src/core/KiwiSdrRedirectPolicy.h`, `.cpp` | `src/core/KiwiSdrRedirectPolicy.{h,cpp}` | `31b29583` | 2026-08-23 | Namespace change only. |
| `src/core/KiwiSdrTxMutePolicy.h` | `src/core/KiwiSdrTxMutePolicy.h` | `31b29583` | 2026-08-23 | Namespace change only. |
| `src/core/KiwiPublicDirectory.h`, `.cpp` | `src/core/KiwiPublicDirectory.{h,cpp}` | `31b29583` | 2026-08-27 | `ext_api`-aware public receiver directory client. Namespace change only. |
| `src/core/ReceivePresentationSync.h`, `.cpp` | `src/core/ReceivePresentationSync.{h,cpp}` | `31b29583` | 2026-08-27 | Namespace change only. |
| `src/core/WaterfallRate.h` | `src/core/WaterfallRate.h` | `31b29583` | 2026-08-27 | Namespace change only. |
| `src/gui/KiwiPublicReceiverPicker.h`, `.cpp` | `src/gui/KiwiPublicReceiverPicker.{h,cpp}` | `31b29583` | 2026-08-27 | Public-directory picker dialog. Namespace change only. |
| `src/gui/KiwiRebindTracker.h` | `src/gui/KiwiRebindTracker.h` | `31b29583` | 2026-08-27 | Namespace change only. |
| `src/gui/KiwiSdrTraceMath.h` | `src/gui/KiwiSdrTraceMath.h` | `31b29583` | 2026-08-27 | Namespace change only. |
| `src/gui/MainWindow_KiwiSdr.cpp` | `src/gui/MainWindow_KiwiSdr.cpp` | `31b29583` | 2026-08-23 | Stufe 4 (Bedienflaeche). Upstream is ~2429 lines across ~30 `MainWindow` methods; most depend on features Longpath doesn't have yet (per-slice virtual antennas, band recall, diversity — Stufe 7, deliberately deferred). This file ports the bridge that does exist: manager state → applet display, audio + waterfall wiring. |
| `src/gui/WaterfallHistoryBuffer.h`, `.cpp` | `src/gui/WaterfallHistoryBuffer.{h,cpp}` | `31b29583` | 2026-08-27 | Namespace change only. |
| `src/gui/applets/KiwiSdrApplet.h`, `.cpp` | `src/gui/KiwiSdrApplet.{h,cpp}` | `31b29583` | 2026-08-27 | Directory path rebased onto `gui/applets/` to match Longpath's applet layout convention; otherwise namespace change only. |
| `src/gui/widgets/SliceColors.h` | `src/gui/SliceColors.h` | `0cd4559` | 2026-08-18 | The four per-slice colours (A cyan / B magenta / C green / D yellow). Extracted from a static `VfoWidget` method into its own header so it survives that class's later deletion; content unchanged. |
| `src/asr/AsrSegmenter.h`, `.cpp` | `src/asr/AsrSegmenter.{h,cpp}` | `31b29583` | 2026-08-2x | Voice-activity segmentation feeding the ASR backend. Namespace change only. |
| `src/asr/IAsrBackend.h` | `src/asr/IAsrBackend.h` | `31b29583` | 2026-08-2x | Backend interface. Namespace change only. |
| `src/asr/IVad.h` | `src/asr/IVad.h` | `31b29583` | 2026-08-2x | VAD interface. Namespace change only. |
| `src/asr/RemoteAsrBackend.h`, `.cpp` | `src/asr/RemoteAsrBackend.{h,cpp}` | `31b29583` | 2026-08-2x | Remote (network) ASR backend implementation. Namespace change only. |
| `src/gui/AsrTapPolicy.h` | `src/gui/AsrTapPolicy.h` | `31b29583` | 2026-08-2x | Namespace change only. |

The three rows below are structural derivatives, not line-by-line
ports — each file's own header already says so; this closes the same
2026-09-08 registration gap for files the full-tree sweep also flags
via inline AetherSDR citations.

| `src/core/strip/StripChain.h` | `src/core/AudioEngine.cpp` (`defaultChain()`) | `31b29583` | 2026-08-08 | Longpath-original chain runner (transmit audio here doesn't pass through an AudioEngine the way AetherSDR's does); the stage ORDER and stage SET (gate → EQ → de-esser → compressor → tube → PUDU → reverb → limiter) follow AetherSDR's `defaultChain()`. |
| `src/gui/applets/StripWindow.h` | `AetherialAudioStrip` + nine `Strip*Panel` classes | `31b29583` | 2026-08-09 | Longpath-original window (same AudioEngine-independence reason as StripChain above); control sets and their ranges come from the already-ported stage headers, not from this file directly. |
| `src/gui/applets/StripGraphics.h` | (none — draws over already-ported DSP) | `31b29583` | 2026-08-09 | Longpath-original visualisation; the curve/bar values it draws come from the already-ported `ClientEq`/stage classes' own analytic functions, not a separate model. |
| `src/gui/applets/eq/ClientEqApplet.h` | `src/gui/ClientEqApplet.h` | `31b29583` | 2026-08-09 | Longpath-original stand-in for exactly one two-value enum (`Path`) that the ported `StripEqPanel` names in its public signals/members — porting the whole (receive-side, unused-here) applet just to obtain the enum would import a widget nobody would ever show. Both enum values kept (including the unused one) so a persisted setting never silently renumbers. |
| `src/gui/applets/eq/EqPalette.h` | (none — colour substitution table for the ported EQ widgets) | `31b29583` | 2026-08-09, extended 2026-08-11 | Longpath-original. The equaliser widgets in this directory are AetherSDR's, ported verbatim; this is the one reviewable colour-substitution table they all reference, so a future re-sync stays a copy instead of a hand-merge across five files. |
| `src/gui/WindowChrome.h` | `src/gui/FloatingContainerWindow.{h,cpp}` | `31b2958` | 2026-08-20 | Longpath-original frameless-window titlebar + resize-handle chrome; structurally modeled on AetherSDR's file, which pairs the same two parts (a draggable bar, a resize handle) atop a frameless window. |
