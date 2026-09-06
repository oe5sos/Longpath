# Dev Automation Bridge — Design & Phased Plan

**Status:** Phase 0 shipped 2026-09-06 (`src/core/DevAutomationServer.{h,cpp}`),
extended the same day with `get radio`/`get slice` (originally filed under
Phase 1 below, reclassified into Phase 0 since a pure model *read* carries
none of the risk the click/setValue/tune verbs do — see the Phase 0 section).
Phases 1+ below (interact verbs, TX-safety gating) are scoped but not started.

**Origin:** an AetherSDR deep-dive (2026-09-06) found their in-process agent
automation bridge (`docs/automation-bridge.md`,
`src/core/AutomationServer.{h,cpp}`, issue #3646) — a tool built for AI
coding agents to introspect and drive a live Qt6 Widgets app without
computer-use screenshots or pixel-hunting. Martin's call when the true size
came up (AetherSDR's version is ~12,000 lines, 25 verbs, an MCP wrapper):
**go for the full shape eventually, but build it in phases — this is
realistically several sessions, not one.**

## Why this exists

Longpath is a native Qt6 Widgets app: no DOM, no browser tooling. Before
this bridge, verifying a GUI change meant either asking the operator to look
at it, or driving `computer-use` tools against screen coordinates — slow,
fragile against layout shifts, and it puts a screenshot review in every
loop. The bridge is the in-process substitute: a small opt-in command
channel over a local socket that reports real widget state as JSON and
captures real pixels (including the GPU-rendered panadapter) as PNG.

It is a dev/introspection tool for whoever is driving Claude Code against
this repository. It ships in every build (off by default) but has no
production surface: the server only starts when `LONGPATH_AUTOMATION` is
set, so a normal launch is unaffected.

## Phase 0 (shipped 2026-09-06): read-only introspection + capture

**Verbs:** `ping`, `dumpTree`, `grab <target>`, `get <model> [selector]`.

**Protocol:** `QLocalServer` on a name from `LONGPATH_AUTOMATION_SOCKET`
(default `longpath-automation`). One newline-terminated command per line in,
one newline-terminated compact-JSON reply per line out, connection stays
open for further commands.

**`dumpTree`** walks every `QApplication::topLevelWidgets()` recursively and
reports, per widget: `class` (full, namespaced), `objectName` (if set),
`accessibleName`, `toolTip`, `enabled`, `visible`, `geometry` (global
screen coordinates), and a best-effort `value` for the widget kinds that
come up in practice (`QLineEdit`, `QPlainTextEdit`, `QLabel`, `QComboBox`
current text + full `items` array, `QAbstractSlider`/`QSpinBox` value +
`range`, non-checkable `QAbstractButton` text). A checkable button reports
both its `text` and `checked` state — the identity and the state, since a
row of identically-shaped toggle buttons (the NR-method row is the
AetherSDR precedent) is unreadable from `checked` alone.

**`grab <target>`** resolves `target` against every top-level widget's
subtree, first by exact `objectName`, then — because the single widget an
agent most wants (the panadapter) carries no `objectName` of its own,
confirmed by dogfooding the same day this shipped — by a namespace-stripped
exact class name (`"SpectrumWidget"`, not `"Longpath::SpectrumWidget"`).
Capture goes through `QWidget::grab()` for ordinary widgets, or
`QRhiWidget::grabFramebuffer()` (Qt 6.7+, real API, not custom readback)
for anything GPU-rendered under `NEREUS_GPU_SPECTRUM` — confirmed live
against `SpectrumWidget` itself: a full-resolution, pixel-correct PNG of
the actual rendered panadapter, grid lines and band-edge marker included.
The PNG is written to a temp file; the reply carries the path plus
width/height/byte-count, not inline base64 — matching AetherSDR's own
choice, and for the same reason (a panadapter frame inline would make for
an enormous JSON line for no benefit an agent needs).

**`get <model> [selector]`** reads live model state directly, no widget or
screenshot involved. `get radio` reports `connectionState` (as the same
string `connectionStateName()` uses elsewhere) and the radio's `model`
name. `get slice [active|<id>]` reports `frequencyHz`, `mode`, `filterLowHz`,
`filterHighHz`, `band` (via `Band::bandFromFrequency()` + `bandKeyName()` —
the same pair the Band-flyout highlight feature uses, shipped the same
day) and `rxAntenna` for the given slice. Added the same day as the rest of
Phase 0 (not at launch) after confirming it carries none of Phase 1's risk:
it never writes to a model or touches a widget, so it doesn't need to wait
on `invoke`'s target resolution or TX-safety gating below. `RadioModel` is
found via `findChild<RadioModel*>()` from the top-level widgets, the same
way it's owned (a `QObject` child under `MainWindow`) and the same way
`tst_dev_automation_server.cpp`'s own test constructs one to verify against.

**Deliberately not in Phase 0:** anything that clicks, types, moves a
slider, tunes, connects, or could conceivably key a transmitter. AetherSDR
did not add interact verbs and safety gating until later phases either, and
their commit history shows real incidents (`#3918`, name-heuristics that
almost blocked a legitimate "Tune Now" button) worth learning from rather
than re-discovering. Nothing here should be mistaken for that boundary
being unimportant — it's why Phase 1 gets its own section below instead of
being folded in now.

### A real gotcha worth recording (found writing Phase 0's own test)

`QLocalSocket::waitForConnected()` / `waitForBytesWritten()` /
`waitForReadyRead()` only pump *that socket's own* engine — not the general
Qt event loop. A same-process test that drives a `QLocalSocket` client
against a `DevAutomationServer` living in the same `QCoreApplication` will
hang: the server's `QLocalServer::newConnection` signal never gets a turn
to fire, because the client's blocking waits never yield to it. The fix
(see `tests/tst_dev_automation_server.cpp`'s `pumpUntil` helper) is to poll
via `QCoreApplication::processEvents()` instead of the socket's own
`waitForX()` when client and server share a thread. A real, separate-process
client (the actual use case) doesn't hit this at all — only the in-process
test does — but it cost real time to track down, so it's written down here
rather than left to be re-discovered.

## Phase 1 (scoped, not started): drive + assert

Mirrors AetherSDR's own Phase 1 boundary:

- `invoke <target> <action> [value]` — click / toggle / setChecked /
  setValue / setText / setCurrentText / setCurrentIndex, resolved through
  the same target resolution Phase 0 already has.
- Widen `get` past `radio`/`slice` (already shipped in Phase 0) to `pan
  <panId|active>` and `dsp`, once there's a real per-pan model worth
  reading (see the `PanadapterModel` dead-code finding in
  `longpath-panadapter-model-tote-klasse-2026-09-06.md` — `pan` here should
  read whatever turns out to be the *live* per-pan state, not that class).
- `assert_state` / `wait_for` helpers, so a caller reads pass/fail instead
  of diffing JSON by hand.

**Safety, decided in advance, not deferred:** any control that keys a
transmitter (MOX/PTT, TUNE, ATU, CWX send) must be refused by `invoke`
unless a *separate* environment variable is set specifically for that
(`LONGPATH_AUTOMATION_ALLOW_TX`, matching AetherSDR's own naming and
reasoning) — the introspection opt-in must never imply the transmit opt-in.
Longpath's own `BandPlanGuard` and "kein kräftiges Rot als Zustandsfarbe"
posture (CLAUDE.local.md) already treat TX safety as non-negotiable; this
bridge inherits that, it does not get to renegotiate it for convenience.

## Phase 2+ (not scoped in detail yet)

AetherSDR's later phases add: multi-instance identity for parallel
worktrees, connect/disconnect verbs, waveform/record verbs, an MCP server
wrapper (`tools/aether_mcp.py`) so any MCP-capable assistant can drive the
bridge without hand-rolled socket scripting, and a `bridge_command` raw
escape hatch for low-level widget primitives that don't earn their own
typed tool. Each is real, separable future work — deliberately not
pre-designed here past naming them, since Phase 1 will surface real
requirements Phase 0 can't predict.

## What Longpath did not copy

AetherSDR's KiwiSDR diagnostic env knobs, its per-worktree `GUIClientID`
identity scheme (Longpath has no equivalent multi-client radio takeover
behaviour to protect against), and its sandboxed-launch `QT_QPA_PLATFORM`
guidance are AetherSDR-specific concerns without a Longpath equivalent —
noted here so a future phase doesn't assume they're missing by oversight.
