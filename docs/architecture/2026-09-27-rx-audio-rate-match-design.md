# RX audio rate match (radio clock vs. sound-card clock) — design

**Status:** design draft, 2026-09-27. No code yet. Written for review before
anything touches the audio path (CLAUDE.md: architecture changes need the
maintainer).

**Kurzfassung (deutsch):** Die Uhr des Funkgeräts und die der Soundkarte
laufen am QRP etwa 4 ppm auseinander. Longpath gleicht das am Ausgang
nicht aus. Der Ring läuft daher langsam voll (rund 0,2 ms pro Minute) und
würde nach etwa 5 Stunden überlaufen; dann springt oder knackt der Ton.
Thetis löst das mit `rmatch` (adaptiver Resampler, ChannelMaster
`ivac.c`). Der Entwurf übernimmt dessen Regelgesetz unverändert, führt
es aber nur auf der Erzeugerseite aus. Grund: Thetis hält im
Audio-Rückruf eine Sperre, und das verbietet Longpaths Regel. Dazu kommt
eine neue, optionale Füllstandsabfrage an der Bus-Schnittstelle.

## 1. The problem, measured

- Radio I/Q arrives at the radio's sample clock; the demodulated 48 kHz audio
  is pushed into the output bus ring by the DSP thread; the audio device pulls
  at the sound card's clock. Two crystals — they never agree exactly.
- Bench, SunSDR2 QRP + MacBook Air, 2026-09-27
  (`werkzeug/berichte/2026-09-27-drift-qrp.txt`): output ring fill rises
  ~0.23 ms per minute ≈ **4 ppm**, plus one step (+10.9 ms at 08:22, a burst,
  not drift). With the 100 ms ring the drift alone overflows after roughly
  5 h; the other direction (radio slower) underruns after the same time.
- Today nothing corrects this. `PortAudioBus::push()` counts overruns
  (drop-oldest) and `paCallback` counts underruns; both are symptoms.
  The only resampler with a ratio is the TX mic path, and its ratio is fixed.

## 2. What Thetis does

Source: ChannelMaster `ivac.c` + WDSP/ChannelMaster `rmatch.c`
(`third_party/wdsp/src/rmatch.c` in our tree, vendored, currently unused;
`varsamp.c` likewise).

- `ivac.c:36-44`: every VAC direction gets an `rmatchV`; the ring holds
  `2 · rate · latency` samples (`OUTringsize = 2 * vac_rate * out_latency`),
  so the loop regulates the fill to **half the ring = the configured latency**.
- RX direction: the producer calls `xrmatchIN` (`ivac.c:168`, audio from the
  mixer; `:161` for I/Q), the PortAudio callback `CallbackIVAC` (`ivac.c:196`)
  calls `xrmatchOUT` (`ivac.c:254`). (`ivac.c:136/142` are the TX direction,
  VAC mic → TX, with the roles swapped.)
- `create_rmatchV` parameters (`rmatch.c:500-526`): startup delay 3.0 s,
  R = 1024, feed-forward moving average 4096…262144 samples with exponential
  smoothing α = 0.01, proportional moving average 4096…16384 samples,
  proportional gain 4.0e-6 (scaled by 48000 / nominal out-rate,
  `rmatch.c:147`), linear interpolation of the ratio per sample, 3 ms slew.
- Control law (`rmatch.c:256-272`, called from both IN and OUT with ± the
  number of samples moved):
  ```
  feed_forward = α · (moving-average in/out ratio · nom_out/nom_in)
               + (1 − α) · feed_forward
  av_deviation = moving average of (n_ring − rsize/2)
  var          = clamp(feed_forward − pr_gain · av_deviation, 0.96, 1.04)
  ```
  `var` is the resampling ratio `varsamp` applies to the input side.
- Locking: `cs_ring` around every ring access and `cs_var` around `var` —
  **both are taken inside the device callback** (`xrmatchOUT`).
- Underflow handling: on `n_ring < outsize` the callback outputs a slewed
  fade (`ntslew`, 3 ms) instead of a hard gap; overflow drops the oldest
  samples with the same slew.

## 3. Constraints in Longpath

- CLAUDE.md: **never hold a mutex in the audio callback**. `rmatch` as-is
  violates this on the OUT side. `PortAudioBus`'s ring is lock-free SPSC
  (`m_ringRead` / `m_ringWrite` atomics) and must stay that way.
- Several output backends: `PortAudioBus`, `CoreAudioHalBus`, `PipeWireBus`,
  `QAudioSinkAdapter`, `LinuxPipeBus`. A fix only in `PortAudioBus` would
  leave the others drifting.
- `IAudioBus` (`src/core/IAudioBus.h`) has no fill-level query today
  (`push/pull/flush/levels/nsSinceLastCallback`).

## 4. Proposal

**Same control law, producer side only.**

1. `IAudioBus` gains `virtual qint64 queuedFrames() const { return -1; }`
   (−1 = unknown; the matcher then passes audio through unchanged).
   `PortAudioBus` returns `m_ringWrite − m_ringRead` (both atomics, readable
   from the producer without a lock); the other backends implement it where
   their API exposes a queue depth, otherwise keep −1.
2. New `AudioRateMatcher` (src/core/audio/), owned per output bus, running
   **on the DSP thread before `push()`**:
   - feed-forward: moving average of *frames consumed* (delta of the bus's
     read position, sampled at each push) over *frames produced* — the same
     quantity `rmatch` gets from its ± calls, without the callback doing any
     bookkeeping;
   - proportional: moving average of `queuedFrames() − target`, target = half
     the ring (Thetis' `rsize/2`);
   - `var` from the exact `rmatch.c:256-272` formula and constants, clamp
     0.96…1.04, 3 s startup before regulating;
   - resampling with WDSP's `varsamp` (already vendored), linear
     interpolation of the ratio per sample as in `rmatch`.
   `var` is written and read only by the DSP thread → no lock at all. The
   callback is untouched.
3. Underflow/overflow slewing stays where it is today (bus side); with the
   loop working, both should become rare events instead of a certainty.
4. Port as a faithful derivative: verbatim `rmatch.c` header, PROVENANCE
   rows for `rmatch.c` / `ivac.c`, a deviation note ("control law executed on
   the producer side only; Thetis runs it on both sides under `cs_var`").

**Why not port `rmatch` verbatim and live with the lock?** The callback
would block whenever the DSP thread holds `cs_ring` — exactly the class of
audio glitches the lock-free ring was built to prevent (see the audio
dropout diagnosis, `longpath-audio-aussetzer-diagnose`).

**Why not only enlarge the ring?** It moves the overflow from 5 h to 10 h
and adds latency; the drift direction depends on the radio, so an
underflow is equally likely.

## 5. Verification plan

1. Unit test of the control law against hand-computed `rmatch` values.
2. Synthetic drift test (no audio device): producer at 48000·(1 + 4e-6),
   consumer at 48000 on a virtual clock, simulated 8 h: fill stays within
   ± a few ms of target, zero under/overruns; same at 1 − 4e-6 and at ±100 ppm
   (cheap USB sound cards).
3. Perf overlay on the bench (`ring_under` / `ring_over`, fill in ms): QRP
   for several hours, then the ANAN on the cable.
4. Ear test by the operator — the measurement does not beat the device
   (`feedback-messung-schlaegt-nicht-das-geraet`).

## 6. Effort

About one day: 2–3 h matcher + bus query, 2 h tests, the rest on the bench
across two radios and two backends.

## 7. Open questions for the operator

- Target latency: half of today's 100 ms ring = 50 ms, as in Thetis
  (`latency` = half the ring). Keep 100 ms ring / 50 ms target, or smaller?
- Apply to the TX monitor / headphones bus as well (it already has an
  adaptive cushion in `MasterMixer`), or RX speakers only first?
