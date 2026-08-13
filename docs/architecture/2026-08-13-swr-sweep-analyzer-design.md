# SWR Sweep Analyzer — the radio as its own antenna analyzer

Status: design + first implementation 2026-08-13 (overnight build).
RF bench pending. Operator request (OE5SOS): "alle Antennen per
Knopfdruck analysieren, Kurve wie ein SWR-Messgerät, nur genauer —
reine Analyse."

## What it is

One button per antenna/band: the radio TUNEs at low power, steps the
TX frequency across the band inside the band edges, reads its own
directional-coupler telemetry at every step, and plots SWR over
frequency. Pure analysis — nothing is adjusted, nothing is matched.
Traces overlay so antennas (or before/after trims) can be compared.

Path B of the 2026-08-13 discussion: no external VNA involved. The
existing `.s1p` file path in the Antenna window stays untouched for
operators who do own one.

## Why the radio can do this

Everything RF-critical already exists and is bench-proven:

| primitive | provider |
|---|---|
| keyed carrier at low power | `MoxController::setTune` (BandPlanGuard-gated, CW-mode gate, PA safety hooks) |
| per-step retune incl. TX low-pass | `RadioConnection::setTxFrequency` (Alex LPF follows inside the connection) |
| forward / reverse watts per sample | `RadioModel::handlePaTelemetry` (`scaleFwdPowerWatts` / `scaleRevPowerWatts`, ~10 Hz on P2) |
| band edges + region rules | `BandPlanGuard::isValidTxFreq` (60 m channelization honored: those channels are excluded from sweepable bands entirely in v1) |
| runaway protection | `SwrProtectionController` stays armed throughout (raw fwd/rev feed is upstream of this feature) |

## Components

### `SwrSweepController` (src/core/)

QObject state machine on the main thread, TwoToneController's
dependency pattern (non-owning injected pointers + injected
`std::function` seams so tests run without a radio):

    setMoxController(MoxController*)
    setTransmitModel(TransmitModel*)
    setTxFrequencyFn(std::function<void(quint64)>)   // RadioModel -> connection
    setTxFreqRestoreFn(std::function<void()>)        // RadioModel::pushTxFrequencyFromTxSlice
    setBandPlanGuard(const BandPlanGuard*), setRegion/setMode providers
    ingestTelemetry(double fwdW, double revW)        // called from handlePaTelemetry
    startSweep(const SweepPlan&) / abortSweep(reason)

`SweepPlan { Band band; quint64 startHz, stopHz; int points;
int settleMs = 120; int dwellMs = 220; }` — planned by
`SweepPlan::forBand(Band)` from the Band enum edges, clipped by
BandPlanGuard per point at start (any invalid point fails the whole
plan — no silent hole-punching in v1; 60m therefore not offered).

Sequence per sweep:

1. Gates: power on, not already in MOX/TUNE/two-tone, plan valid,
   points 11..401, connected. Tune power is whatever the operator's
   TUN slider says; the UI shows it and warns above 10 W.
2. Snapshot nothing power-related (TUNE path owns drive); remember
   only that the TX frequency must be restored.
3. `setTune(true)`; wait `kTuneSettleMs` (300 ms, mirrors
   TwoToneController::kTuneReleaseSettleMs).
4. Per point: `txFrequencyFn(f)` → wait settleMs → collect all
   telemetry samples for dwellMs → average → SWR:
   `gamma = sqrt(rev/fwd)`, `swr = (1+gamma)/(1-gamma)`, capped 99,
   sample invalid when `fwdW < kMinFwdW` (0.5 W — below the bridge's
   honest floor). Emits `pointReady(index, freqHz, swr, fwdW)`.
5. Ends: `sweepFinished(SweepResult)` — or abort on operator stop,
   disconnect, telemetry silence > 2 s, or 3 consecutive points with
   swr >= kAbortSwr (25.0: an open feedline, not a bad antenna).
6. `finishSweep()` runs on EVERY exit path: `setTune(false)`,
   300 ms settle, `txFreqRestoreFn()`.

The controller re-asserts the point frequency at every step, but a
concurrent operator VFO spin mid-dwell is not detectable — the UI
says so and the progress dialog discourages it. Not worth a lock in
v1.

### `SwrChartWidget` (src/gui/widgets/)

Plain QPainter (no QRhi — 400 points at 1 Hz repaint needs nothing):
X frequency, Y SWR clamped to [1..6] display range with zone bands
(green < 1.5, yellow < 2.0, red above — StyleConstants palette),
resonance marker at min-SWR, hover crosshair with f/SWR readout,
band-edge ticks, multi-trace overlay with legend. Traces carry
`{name, color, points, antenna, band, timestamp}`.

### Antenna window integration

Tools → Antenna gains a second tab: **"Sweep (Radio)"** — band combo
(amateur bands the connected SKU + guard allow; GEN/WWV/XVTR and 60m
excluded), points spinner, live tune-power label, Start / Stop,
progress bar, the chart, a trace list (rename / hide / delete /
export CSV). The window receives its backend via
`setSweepBackend(SwrSweepController*, RadioModel*)` from MainWindow —
the file-based analyzer half keeps working with no radio at all.

"Alle Antennen": v1 ships the loop in the window — for each antenna
port the SKU exposes (AlexController labels, skipping ports with
blockTx set), select antenna → sweep → next, one named trace each.
Gated behind a confirmation naming the ports it will key into.

## Safety posture

- Every point pre-validated against BandPlanGuard (region + mode).
- TUNE goes through MoxController — every existing gate (CW block,
  PA checks, TX inhibit) applies unchanged.
- SwrProtectionController stays armed; its foldback does not falsify
  the analysis (SWR is a ratio; drive reduction cancels out) — but a
  stage-2 windback aborts the sweep via the fwd-power floor.
- Default duty: 51 points × (120 + 220) ms ≈ 17 s keyed per band at
  tune power. PA thermals are a non-issue at ≤ 10 W on this family.

## Testability without RF

The controller's physics enter through `ingestTelemetry` and leave
through `txFrequencyFn` — a test provides a synthetic dipole
(fwd/rev computed from a known impedance curve) and asserts the
sweep finds the known resonance, the state machine restores TUNE=off
+ TX freq on every exit path, and invalid/low-power samples are
dropped. `tst_swr_sweep_controller` covers: happy path, operator
abort mid-sweep, telemetry silence abort, high-SWR abort, guard
rejection, busy rejection.

## Deferred

- Persisted trace library across sessions (v1: session + CSV export).
- Complex impedance (needs a real VNA — path A, if a device shows up).
- Sweeping THROUGH an antenna tuner intentionally (works, but the
  trace then shows the tuner, not the antenna; doc note only).
- 60 m (channelized; sweep semantics unclear — revisit on request).
