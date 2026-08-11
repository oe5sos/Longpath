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

### The measuring instrument for that look (added same evening)

The dispatch-tick diagnostic sits two layers downstream and cannot
tell wire loss from in-app loss. `P2RadioConnection` now audits the
port-1026 mic frames' own 32-bit sequence numbers at the socket
(`auditMicSeq`), and the DDC I/Q direction aggregates the same way in
`processIqPacket`. Both report 5 s windows at `qCInfo` whenever a
window saw loss or reordering, plus a clean heartbeat at most once a
minute, so a healthy log still proves the audit ran:

    P2 mic seq audit: 3712 frames / 5.0 s, LOST 43 (1.14%), out-of-order 0
    P2 IQ seq audit: clean (16520 pkts / 5.0 s, no gaps)

Reading the pair:

| mic audit | IQ audit | verdict |
|---|---|---|
| gaps | gaps | network path (WLAN/switch/cable) — both directions suffer |
| gaps | clean | radio→PC path or socket-buffer stalls under load; mic-only loss on a clean IQ stream points at scheduling around the small 132-byte frames |
| clean | clean | the 3-15% dispatch-tick deficit is IN-APP (TxMicSource ring or worker pacing) — the network is exonerated |

Bench protocol (no MOX needed — mic frames flow whenever the radio
runs):

1. WLAN: run the app 2-3 minutes connected, collect the audit lines.
2. Wired: same station, same duration, Ethernet instead.
3. Compare loss percentages; anything ≥0.1% sustained on the wire is
   worth chasing with a switch-port/AP change before touching code.

Launch capturing only the audit lines:

    ~/Desktop/neureus/NereusSDR/build/NereusSDR.app/Contents/MacOS/NereusSDR 2>&1 | grep --line-buffered "seq audit"

### First measurement (2026-08-11, remote link) and the fix it bought

The remote bench (radio at another site, same routed network) ran the
audit for ~6 minutes. Verdict: the dominant defect is REORDERING, not
loss — in almost every window `LOST n == out-of-order n` cancelled out
(a late frame books a gap, then arrives), true loss sat at 0.1-0.3%
with occasional correlated bursts up to ~1.5% hitting mic and IQ in
the same window (a genuine network event). The socket received nearly
the full 750 frames/s throughout, so the 3-15% dispatch deficit is
NOT wire loss — it is in-app or TX-load-dependent (measurement under
MOX still pending).

The actionable finding: 0.2-0.5% late frames ≈ 2-4 splice glitches
per second in the TX pump, which consumed frames in ARRIVAL order.
Fixed the same evening by `MicReorderBuffer`
(src/core/audio/MicReorderBuffer.h — speculative zero-latency
reordering: in-order frames pass straight through, late frames are
slotted back into position, true losses concealed by repeating the
last 1.33 ms block, resync on stream restart). Sits between the
port-1026 decoder and TxMicSource; stats appear next to the audit as
"P2 mic reorder: rescued N late, concealed M lost, …". Contract
pinned by tests/tst_mic_reorder_buffer.cpp; validated pre-merge in an
offline ASan/UBSan harness including a 20k-frame soak (3% swaps +
0.1% loss: 588 rescued, 10 concealed, zero out-of-order emissions).

### Second measurement (same evening, under MOX): the 3-15% solved

Under MOX the audit showed 3-9% loss on mic AND IQ simultaneously —
and the IQ packet rate quadrupled from ~1000 to ~3900 pkts / 5 s the
moment MOX engaged, with PureSignal OFF and `DDCAssign` reporting the
same single enabled DDC throughout. 201 pkt/s is exactly 48 kHz;
807 pkt/s is exactly 192 kHz: the radio was being snapped from the
session's live 48 kHz to 192 kHz on every TX.

Root cause — two rate stores drifting apart. `ReceiverManager` keeps
a per-receiver rate (updated by the P2 per-stream path,
`commitStreamSampleRateChange` → `setReceiverSampleRate`) AND a
PS-orchestration rate (`m_rx1Rate`, seeded once at connect, read by
`updateDdcAssignment()` on every MOX/PS/diversity toggle). The
per-stream 48 kHz apply updated only the former; the first MOX fire
emitted a PsDdcConfig carrying the stale connect-time 192 kHz, and
CmdRx obediently reprogrammed the radio. Four times the downstream
bandwidth met a remote link that could not carry it (on top of the
protocol-fixed ~9.2 Mbit/s upstream TX I/Q that P2 always sends
during MOX), and both directions drowned. The dispatch-tick "3-15%
mic loss" from the original crackle hunt was this exact mechanism,
measured two layers downstream.

Fix: `setReceiverSampleRate` now mirrors indices 0/1 into
`m_rx1Rate`/`m_rx2Rate` silently (no assignment fire — every caller
pushes the full assignment itself; the mirror only has to be correct
by the next event-driven fire), and `setSampleRateLive` step 12 syncs
explicitly on the radio-wide path. Regression pinned in
tests/tst_receiver_manager_ps_ddc.cpp
(`perReceiverRateChangeMirrorsIntoPsRate`).

Residual for remote TX: the P2 upstream TX I/Q at 192 kHz
(~9.2 Mbit/s while transmitting) is protocol-fixed and remains the
remote link's biggest single load. If TX loss persists after this
fix, the lever is the link (wired bridge / QoS), not the client.

## Open items

- "QLayout: Cannot add a null widget" fires once when some window
  opens mid-session; resisted static hunting. The message handler now
  appends a native backtrace when it fires — the next occurrence
  identifies its caller in the log.
- PC-mic clock drift has no compensation (only detection). If the
  short-pull warning ever shows up in the field, the fix is an
  adaptive micro-resampler at the splice, not more buffering.
