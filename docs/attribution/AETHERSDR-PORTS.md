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
| `src/gui/applets/eq/ClientEqCurveWidget.h`, `.cpp` | `src/gui/ClientEqCurveWidget.{h,cpp}` | `31b29583` | 2026-08-09 | Namespace change; includes rebased. Seven colour literals bound to NereusSDR style tokens — see below. Behaviour unchanged. |
| `src/gui/applets/eq/ClientEqFftAnalyzer.h`, `.cpp` | `src/gui/ClientEqFftAnalyzer.{h,cpp}` | `31b29583` | 2026-08-09 | Namespace change only. Takes `update(samples, count)`, so NereusSDR's `MicSpectrum` can feed it unchanged. |
| `src/gui/applets/eq/ClientEqEditorCanvas.h`, `.cpp` | `src/gui/ClientEqEditorCanvas.{h,cpp}` | `31b29583` | 2026-08-09 | Namespace change; includes rebased. |
| `src/gui/applets/eq/ClientEqParamRow.h`, `.cpp` | `src/gui/ClientEqParamRow.{h,cpp}` | `31b29583` | 2026-08-09 | As above. |
| `src/gui/applets/eq/ClientEqIconRow.h`, `.cpp` | `src/gui/ClientEqIconRow.{h,cpp}` | `31b29583` | 2026-08-09 | As above. |

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

Five have no NereusSDR equivalent — #506070, #08121d, #7f93a5, #243a4e,
#1a2e42, #0e1b28 — and are left alone rather than snapped to a near
neighbour. A colour chosen to sit between two others stops working when
moved to one of them.

**One new colour set, and it is functional.** `kPalette` in
ClientEqCurveWidget is eight hues that tell one band from another when
several overlap. NereusSDR has no equivalent because it never had
per-band colouring. It is one table and trivially removable if unwanted.
