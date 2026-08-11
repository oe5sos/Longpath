# TX Self-Monitor Audio Path — Design & the 2026-08-11 Crackle Hunt

Status: **shipped** (bench-verified on ANAN-G2, P2, macOS).
Companion test: `tests/tst_master_mixer_cadence.cpp`.

## What this documents

The complete signal path for "hear myself" (off-air monitor and MOX
monitor), every seam in it, the five defects found on the 2026-08-11
bench, and the design that fixed them. Written because four successive
point fixes each cured one symptom and exposed the next — the lesson is
the chain, not any single patch.

## The path

```
Radio mic stream (P2, 48 kHz, radio-clocked)
    │  64-frame blocks, semaphore-wake pacing
    ▼
TxWorkerThread ── optional PC/VAX mic splice (zero-fill on short pull;
    │             instrumented: "PC-mic pulls ran short" warning)
    ▼
WDSP TXA chain (strip → EQ → compressor → modulator), fexchange0
    │  output at DUC rate: 192 kHz on P2, 48 kHz on P1
    ▼
driveOneTxBlock siphon ── box-average DECIMATION back to 48 kHz
    │  (factor = outN / inN; the emit contract of sip1OutputReady is
    │   "mic rate, always" — subscribers must NOT resample)
    ▼
AudioEngine::txMonitorBlockReady ── clamp ±1.0 + over-range warning
    │
    ▼
MasterMixer slot kTxMonitorSlotId (-2), OPPORTUNISTIC
    │  adaptive jitter cushion + seam fades (see below)
    ▼
tryDrain, paced by RX block arrival (~1024 frames / ~21.3 ms)
    │  same drain feeds…
    ▼
Speakers bus AND headphones bus (PortAudio, drop-oldest rings)
```

## The five defects, in the order they were found

1. **Rate mismatch (pure crackle, no intelligible audio).** The siphon
   emitted the raw DUC-rate output; on P2 that fed 192 kHz into a
   48 kHz mixer — 4× oversupply, 4× playback speed. P1 (48 kHz DUC)
   never showed it. Fix: decimate in `TxChannel::driveOneTxBlock`
   before the emit; the block ratio IS the rate ratio, so no rate
   plumbing was needed.

2. **Headphones bus never fed.** `setHeadphonesConfig` opened the bus;
   no code pushed into it. Meters moved, headphone device stayed
   silent. Fix: `rxBlockReady` pushes the same mix under a new
   `m_headphonesBusMutex` (same try_lock contract as speakers), and
   master-mute flushes it (#201 echo-tail reasoning).

3. **Monitor ring too shallow (voice shredded into 5 ms shards).**
   `ensureRing` sized every slot to 4 producer blocks — 256 frames for
   the 64-frame monitor, against a ~21 ms drain period. Drop-oldest
   discarded three quarters of the waveform every cycle. Fix:
   opportunistic slots get `kOpportunisticRingBlocks` (128) blocks.

4. **Unbounded backlog (latency "too high to listen to").** The drain
   takes exactly n frames and production matches drain rate, so
   whatever backlog existed at prime time was carried forever — up to
   the full deep ring (~85 ms). Fix: backlog cap trims to the cushion
   whenever it exceeds twice the cushion.

5. **Fixed cushion smaller than real jitter (the stubborn scratch).**
   The offline cadence simulation reproduced it exactly: with a fixed
   256-frame cushion, normal UDP jitter (σ 1/3 ms) starves the slot
   ~5×/s (50 discontinuities / 30 s); σ=8 ms WLAN clumping gives 161.
   Fix, validated in the same simulation before it ever reached the
   bench:
   - **Adaptive cushion** — doubles on every starvation, bounded at
     `kOpportunisticCushionMaxFrames` (2048 ≈ 43 ms), never probed
     back down. A downward probe was tried: each probe bought 1-2 ms
     of latency at the price of an audible click. Latency is spent
     only where the link's observed jitter demanded it, and stays.
   - **Seam fades** — every discontinuity the scheme still has (prime,
     trim, starvation) re-enters through the per-sample gain ramp:
     gains reset to zero at prime and trim, and a starved slot plays
     its residue out with gain target zero instead of cutting
     mid-word. Converged result across all six simulated profiles
     (lockstep → WLAN clumping → ±100 ppm drift): **zero steady-state
     discontinuities**, 0-2 faded seams during first-minute
     adaptation.

## Latency budget (and its floor)

    monitor slot cushion        5–11 ms (LAN)  …  up to ~43 ms (bad WLAN)
    drain batching              ~21 ms (one RX block)
    device buffer               ~10–15 ms
    ─────────────────────────────────────────
    total                       ~35–45 ms typical LAN

The drain batching is architectural: the monitor rides the RX block
cadence because that is where the speakers push lives. Reducing the
floor means decoupling the monitor from the RX drain (its own sink, or
sub-block drains) — an architecture change deliberately NOT made during
a bug hunt. Thetis's MON path has a comparable budget. For judging EQ
changes precisely, the record-then-listen Voice Check (embedded in the
strip window since 2026-08-11) is the intended tool; the live monitor
is a coarse check.

## Verification

- `tests/tst_master_mixer.cpp` — prime gate contract (silent while
  buffering, full blocks once primed, fade-out + de-prime + re-buffer
  on starvation, never a barrier member).
- `tests/tst_master_mixer_cadence.cpp` — the six-profile simulation,
  fixed seeds, `lateClicks == 0` asserted for every profile plus a 2×
  WLAN extreme case.
- `TxWorkerThread` logs "PC-mic pulls ran short …" (throttled, 5 s)
  when the PC-clocked mic ring runs dry against the radio-clocked
  pump — the one seam that would put clicks in the TX audio itself.
  A clean bench log means the mic seam is healthy.

## Postscript (same day, evening): the sixth defect was upstream of all five

The scratch survived every fix above because its deepest source was
none of them: **the radio's mic-frame stream loses 3-15% of its blocks
on this bench** (pump-cadence diagnostic: 3190-3639 dispatch ticks per
5 s where 3755 belong). A sample that never arrived cannot be buffered
back into existence, so everything paced by the radio — the live
monitor, the sip1 takes, the worker-tap takes — stuttered no matter
what sat downstream.

The resolution is AetherSDR's actual architecture, ported whole at the
bench's request: `ClientPuduMonitor` (record-then-listen, auto-play,
RX muted across the cycle) fed by a **device-paced** capture — a
QAudioSource on the PC input, gapless by construction — with the
channel strip run OFFLINE over the finished take on a private
StripChain. The radio paces nothing in the loop; there is no live
self-monitoring anywhere any more. Bench-confirmed clean.

Standing consequence to investigate: the same mic-frame loss feeds the
REAL transmit path. On-air SSB voice from this bench inherits those
gaps. Needs a network-level look (WLAN vs. wired, packet captures)
before the next TX-quality pass.

## Open items

- "QLayout: Cannot add a null widget" fires once when some window
  opens mid-session; resisted static hunting. The message handler now
  appends a native backtrace when it fires — the next occurrence
  identifies its caller in the log.
- PC-mic clock drift has no compensation (only detection). If the
  short-pull warning ever shows up in the field, the fix is an
  adaptive micro-resampler at the splice, not more buffering.
