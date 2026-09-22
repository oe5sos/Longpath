# Phase 3M-1a — Bench Debug Handoff

**Date:** 2026-04-27 (carried through midnight from 2026-04-26 session).
**Branch:** `feature/phase3m-1a-tune-only-first-rf`.
**Worktree:** `/Users/j.j.boyd/NereusSDR/.worktrees/phase3m-1a-tune-only-first-rf`.
**Status:** Architectural work + 6 bench-debug fix rounds landed.
First-RF carrier still not on the air.

This document is the handoff for a fresh session to pick up the remaining
WDSP-integration mystery. Read this in full before doing anything else.

---

## What works (confirmed by bench-diag log)

The chain UI → model → connection → wire is **fully wired**:

```
TUNE button click  →  TxApplet toggled signal
                   →  RadioModel::setTune(true)
                   →  reads tunePower for current band, tx model state
                   →  m_swrProt.setTunePowerSliderValue(...) + setAlexFwdLimit(...)
                   →  m_txChannel->setTuneTone(true, ±cw_pitch, kMaxToneMag)
                        SetTXAMode(channel, dspMode)             ← per-mode pipeline config
                        SetTXABandpassFreqs(channel, low, high)  ← per-mode IQ-space cutoffs
                        SetTXAPostGenToneFreq + Mode + Mag + Run = 1
                   →  m_activeSlice->setDspMode(swappedMode)  if CW→LSB/USB swap
                   →  invokeMethod(conn, ...) → setTxDrive(50)
                   →  m_moxController->setTune(true)
                        → state machine, hardwareFlipped(true) signal
                        → onMoxHardwareFlipped → applyAlexAntennaForBand
                        → invokeMethod(conn, ...) → setMox(true) + setTrxRelay(true)
                        → 30 ms rfDelay timer
                        → txReady signal
                        → m_txChannel->setRunning(true)
                            SetTXACFIRRun(channel, 1)
                            SetChannelState(channel, 1, 0)
                            m_txProductionTimer->start()        ← drives fexchange2 every 5 ms
```

Confirmed running on G2 / Saturn (the live bench radio):
- TUNE button toggles state correctly (red, "TUNING…").
- MOX status badge engages (red).
- `RadioModel::setTune(true)` fires; bench-diag shows correct band, power, signedFreq.
- `P2::setTxDrive` fires with the right level; `byte 345` of high-pri packet
  carries the drive level on next 100 ms cadence.
- `TxChannel::driveOneTxBlock` fires every ~5 ms during TUN.
- `fexchange2` returns no error (`OK`).
- `outN = 952` (correct for 192 kHz output × 5 ms tick at 48 kHz input rate).
- SPSC ring fills (`ring count = 2928–3728` between consumer drains).
- `P2 m_txIqTimer` consumer drains the ring every 5 ms (4 frames × 240 samples).
- UDP packets ARE flowing to port 1029 (192 kHz × 5 ms = 960 samples ≈ 4 frames/tick).

## The remaining bug

`peakI` and `peakQ` (max abs of fexchange2 output samples per call) are
**~0.003–0.005 instead of expected ~0.7–1.0**. That's about **52 dB
below expected** — effectively silence on the air.

```
TxChannel::driveOneTxBlock # 601 fexchange2 OK  outN= 952
  peakI= 0.00263536 peakQ= 0.00259757
```

Both peakI and peakQ have similar magnitudes — that IS consistent with a
sine wave (peak |cos| ≈ peak |sin|) — just at vastly reduced amplitude.

**Important diagnostic finding:** disabling `cfir` (the only meaningful
filter between gen1 and the output buffer) did **NOT** change the
amplitude. peakI/Q stayed near zero. So the attenuation is NOT in cfir.

Pipeline order between gen1 (where the tone is injected) and output:

```
gen1 (writes midbuff with tone at mag=0.99999)
 → uslew    (slewup ramp; passes through after ~5 ms)
 → alcmeter (observation only)
 → siphon   (observation only)
 → iqc      (run=0, pass-through)
 → cfir     (TESTED: amplitude unchanged when disabled)
 → rsmpout  (96k → 192k, gain=0.98)
 → r2 ring  (read by fexchange2)
```

So either:
1. **gen1 is not actually producing the expected tone** — `SetTXAPostGen*`
   calls aren't taking effect, or `tone.mag` / `tone.freq` /
   `tone.delta` is wrong on the live struct.
2. **rsmpout has a config issue** (gain=0.98 should give ~0.97 amp, not
   ~0.005 — but possible the resampler is doing something unexpected
   with our specific 96 → 192 ratio).
3. **uslew is interacting badly** with our timer cadence (e.g.,
   re-triggering slewup more than expected).

## Diagnostic instrumentation already in place

The branch has `BENCH-DIAG` log lines wired (kept in for the next session):

| Where | What |
|---|---|
| `RadioModel::setTune(true)` | band, tunePower, savedDspMode, signedFreq, txChannel + connection non-null |
| `P2::setTxDrive` | level, stored, m_running |
| `TxChannel::driveOneTxBlock` (every 50th call) | inN, outN, peakI, peakQ + early-return reason |
| `TxChannel::driveOneTxBlock` (every 50th call) | **gen1 runtime state**: `run`, `mode`, `tone.mag`, `tone.freq`, `tone.delta`, `size`, `rate` ← **NEW; not yet captured because last bench session ran before this was added** |
| `P2 m_txIqTimer` (every 200th tick) | ring count |
| `TxApplet TUNE button toggled` | direct emit log before any guards |

These all log to qInfo() with `BENCH-DIAG` prefix. Filter the log with
`grep BENCH-DIAG` for clean output.

## Next investigation steps

The first thing to do in the new session is **read the gen1 runtime state**
line that the new diagnostic emits. That tells you whether gen1 is
actually configured with the expected tone params at the moment fexchange2
runs.

Possible outcomes:

| gen1 state | Conclusion |
|---|---|
| `run=1, mag=0.99999, freq=±600, delta=±0.0392` | gen1 is configured correctly; bug is downstream (uslew / iqc / cfir / rsmpout) |
| `run=0` | gen1 isn't actually running — `SetTXAPostGenRun(1)` didn't take effect |
| `mag` near 0 | tone magnitude is wrong — either SetTXAPostGenToneMag failed or the struct field was overwritten |
| `delta=0` | tone phase isn't advancing — calc_tone wasn't called, or freq=0 was set |

If gen1 state looks correct, next isolation tests:
1. **Bypass uslew**: comment out `xuslew(txa[ch].uslew.p)` in the WDSP
   `xtxa()` pipeline (third_party/wdsp/src/TXA.c:584) — confirm whether
   uslew is the attenuator.
2. **Bypass rsmpout** by setting `outRate = dspRate` (96000) at
   `createTxChannel` call — output samples will be at 96 kHz instead
   of 192 kHz, but if amplitude jumps to ~1.0 the issue is rsmpout-related.
3. **Direct read of midbuff** right before rsmpout via a tiny WDSP patch
   that exposes a debug accessor.

## Bench-test machinery

### Build

```sh
cd /Users/j.j.boyd/NereusSDR/.worktrees/phase3m-1a-tune-only-first-rf
export LONGPATH_THETIS_DIR=/Users/j.j.boyd/Thetis
cmake --build build -j$(sysctl -n hw.ncpu)
```

### Kill + relaunch with stderr capture

```sh
pkill -x NereusSDR 2>/dev/null; sleep 0.5
LOGFILE=/tmp/nereussdr-bench-diag.log
rm -f "$LOGFILE"
QT_LOGGING_RULES="dsp=true;connection=true;*.debug=false" \
  build/NereusSDR.app/Contents/MacOS/NereusSDR > "$LOGFILE" 2>&1 &
sleep 1
pgrep -lf "NereusSDR.app/Contents/MacOS/NereusSDR"
```

### Bench retry sequence

1. Connect to G2 in the discovery list.
2. Press **TUNE** button on TxApplet for ~3 seconds, release.
3. Read log: `grep BENCH-DIAG /tmp/nereussdr-bench-diag.log | tail -60`.

### Reading the log efficiently

Top events of interest, in order:
- `G.1: TX channel 1 created (deferred until conn live) outRate= 192000`
  → confirms TX channel correctly created at 192 kHz output rate.
- `TX channel 1 init: ALC max-gain capped at 0 dB (per deskhpsdr)`
  → confirms deskhpsdr-equivalent default config applied.
- `BENCH-DIAG TxApplet TUNE button toggled = true updatingFromModel=false`
  → click reached the model layer.
- `BENCH-DIAG RadioModel::setTune(true) band= N tunePower= N`
  → orchestrator running with valid params.
- `BENCH-DIAG gen1 state: run= 1 mode= 0 tone.mag= 0.99999 tone.freq= -600`
  → **NEW** — confirms gen1 actually configured. Check this first.
- `BENCH-DIAG TxChannel::driveOneTxBlock # N fexchange2 OK ... peakI= ... peakQ= ...`
  → **the smoking gun** — should be ~0.7–1.0 if pipeline is healthy.

## What's been ruled out

- ❌ Wire-byte path (UDP packets to port 1029 confirmed flowing with non-zero seq + ring fill).
- ❌ MOX wire bit (high-pri byte 4 bit 1 confirmed by the radio engaging MOX state).
- ❌ Drive level (`byte 345` confirmed = 50 / 25 / etc.).
- ❌ TX channel output rate mismatch (was 48000 due to a Saturn-rate-init copy-paste bug; now 192000).
- ❌ `m_connection` null in TxChannel (was — caused by init-order issue with `conn.release()` happening AFTER WDSP-init lambda; fixed by deferring TX channel creation).
- ❌ ALC running away to infinity (was — fixed by `SetTXAALCMaxGain(0)` cap).
- ❌ Default bandpass blocking the tone (was — fixed by `SetTXAMode` + `SetTXABandpassFreqs` per-mode init).
- ❌ cfir filter attenuating (tested by disabling — amplitude unchanged).

## What's STILL not ruled out

- gen1 PostGen tone params not actually taking effect on the live struct.
- uslew triggering on every fexchange2 instead of just channel start.
- rsmpout config issue specific to our 96 kHz → 192 kHz path.
- Some other WDSP TXA stage we haven't audited.

## Side bug observed but not investigated

**RX audio still flows during MOX engagement** despite the
`onMoxHardwareFlipped` slot calling `RxChannel::setActive(false)` on
all active RX channels. JJ reports band noise still audible through
speakers when TUN engages. Investigate after first-RF carrier is
working.

## Reference reads

Source-first reading queue for the next session:

- `third_party/wdsp/src/TXA.c:557-592` — full `xtxa()` pipeline order.
- `third_party/wdsp/src/gen.c:215-240` — `xgen()` mode 0 (sine tone).
- `third_party/wdsp/src/slew.c:90-155` — `xuslew()` slewup state machine.
- `third_party/wdsp/src/resample.c` — `xresample()` rsmpout impl.
- `/Users/j.j.boyd/deskhpsdr/src/transmitter.c:1438-1495` — deskhpsdr's
  exact TX channel setup (the reference).
- `/Users/j.j.boyd/deskhpsdr/src/transmitter.c:1518-1825` — `tx_full_buffer`
  (deskhpsdr's per-block TX I/Q production).
- `/Users/j.j.boyd/deskhpsdr/src/transmitter.c:2837-2862` — `tx_set_singletone`
  (deskhpsdr's tune-tone setup).
- `pre-code review §3` (`docs/architecture/phase3m-1a-thetis-pre-code-review.md`)
  — chkTUN_CheckedChanged port spec.

## Restoration before merge

When first-RF works, two cleanup commits before the PR opens:

1. **Strip `BENCH-DIAG` log lines** from production code.
2. Remove the diagnostic instrumentation block in
   `TxChannel::driveOneTxBlock` (gen1 state dump) and the per-call counter.

The architectural fixes (P2 samplingRate, ALC cap, deskhpsdr init block,
SetTXAMode + SetTXABandpassFreqs, deferred TX channel creation, conn
re-push) all stay — they're correct ports and tested by `ctest`.

— J.J. Boyd ~ KG4VCF (handoff written by Claude, awaiting JJ's "post it" before any public action).
