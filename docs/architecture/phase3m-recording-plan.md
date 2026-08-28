# Phase 3M — Recording / Playback: Implementation Plan

**Status:** Phases A-D implemented (code-level; A/B/D previously build-verified at 807/807 tests, C added 2026-08-26 and syntax-clean but not yet build-verified — only the operator builds, per `CLAUDE.local.md`). Phase E.1 (unit-test sweep) pending the operator's next build; E.2/E.3 (live-bench and no-hardware checks) pending a real radio session.
**Author:** Martin Fischer (OE5SOS), AI-assisted (Claude Code).
**Date:** 2026-08-25.
**Design doc:** `docs/architecture/phase3m-recording-design.md` (source-of-truth for every Thetis citation and architecture rationale below — this plan does not re-derive them).
**Master-plan slot:** `docs/MASTER-PLAN.md` § "### Phase 3M: Recording/Playback".
**Risk profile:** LOW. No RF/PA/safety surface — this is receive-side audio/I-Q file I/O. The one genuinely new piece of runtime plumbing (`PlaybackRadioConnection`) can misroute samples but cannot key a transmitter or damage hardware.

---

## 0. Decisions carried from the design doc

1. **Playback = Option A**, `PlaybackRadioConnection : RadioConnection` (design doc §7.3, operator decision 2026-08-25). Rejected: bypass-injection mid-pipeline (Option B), because it would require an already-connected live radio and Longpath has no Thetis-shaped reason to accept that constraint.
2. **`WavRecorder` reuses the existing `AudioTapRing` tap idiom** (`setQsoTap`/`setAsrTap` pattern, design doc §6) rather than inventing a new audio-access mechanism.
3. **`IqRecorder` is new plumbing** — no existing Longpath tap sits pre-WDSP (design doc §6, §7.2).
4. **UI surface (button placement, icons, dialog layout) is explicitly out of this plan.** Per `CLAUDE.local.md` ("Technik Nereus, Design ich"), visual/UX placement is the operator's call, made when it's actually being built — this plan only wires the backend classes and their signals/slots up to a point where a UI can bind to them.

---

## 1. Files

**New:**
- `src/core/audio/WavRecorder.{h,cpp}` — post-WDSP audio tap, mirrors `QsoRecorder` structurally.
- `src/core/audio/IqRecorder.{h,cpp}` — pre-WDSP raw I/Q tap, new subscriber on the existing `iqDataForReceiver`/FFT-fork point.
- `src/core/PlaybackRadioConnection.{h,cpp}` — file-backed `RadioConnection` subclass.
- `src/core/RecordingScheduler.{h,cpp}` — timer-based start/stop-at-time-or-duration (design doc §8, no existing Longpath equivalent to port).
- Tests: one `tst_*` file per class below, plus wire-format/round-trip tests for the file readers/writers.

**Modified:**
- `src/core/AudioEngine.{h,cpp}` — add `setWavRecordTap(AudioTapRing*, int sliceId)`, mirroring `setQsoTap`/`setAsrTap` (`AudioEngine.h:402,414`).
- `src/core/RadioConnection.{h,cpp}` — `create()` needs either a third branch or a parallel non-discovery construction path for `PlaybackRadioConnection` (design doc §6, `RadioConnection.cpp:21-33`).
- `src/core/audio/WavFile.{h,cpp}` — extend if the write-side doesn't already cover the bit-depth/sample-rate range this phase needs (design doc §8, unresolved — first task below settles it).
- `src/models/` — a small `RecordingModel` (Q_PROPERTY surface: recording/playing state, current file, elapsed time) for whatever UI eventually binds to it. Kept deliberately thin — no layout decisions live here.
- `AppSettings` — new keys for scheduled-recording config (§4) and default record-folder/format preferences.

---

## 2. Task list

Tasks are grouped by phase; each phase is independently testable before the next starts. No hardware bench gate is required for A–C (pure file I/O against synthetic/recorded data); D needs a short real-clock wait to prove the timer actually fires; E is the one item needing a live radio, to confirm the recorded-vs-live signal chains agree.

### Phase A — `WavRecorder` (post-WDSP audio tap)

| # | Task | Tests | Cite |
|---|---|---|---|
| A.1 | Audit `WavFile.{h,cpp}`'s current write-side against Thetis's `BitDepthMode` range (IeeeFloat32/Pcm32/Pcm24/Pcm16/Pcm8, 2ch, default 48000 Hz, floor 6000 Hz) and extend if needed. Settles the design doc's first open question (§8). | `tst_wav_file_format_matrix.cpp` (round-trip write→read at each supported bit depth) | Design doc §3, §8 |
| A.2 | `AudioEngine::setWavRecordTap(AudioTapRing*, int sliceId)` — new tap alongside `setQsoTap`/`setAsrTap`, drained at the identical pre-mixer/pre-volume point in `rxBlockReady` (`AudioEngine.cpp:1295-1305`). | `tst_audio_engine_wav_tap.cpp` (tap receives the same samples QSO tap does, given the same slice) | Design doc §6, §7.1 |
| A.3 | `WavRecorder` class: owns an `AudioTapRing`, drains on a `QTimer` into a `WavFile` writer; `start(path, format)` / `stop()`; no pause (matches Thetis — design doc §3, "Start/Stop (no Pause)"). | `tst_wav_recorder_start_stop.cpp` (recorded file byte-matches the fed samples at the chosen format) | Design doc §3 |
| A.4 | Quick Record: fixed-path overwrite variant (`start()`/`stop()` convenience wrapping A.3 with a well-known scratch filename) — the backend half of Thetis's `ckQuickRec` behavior (design doc §3); no UI here (§0.4). | `tst_wav_recorder_quick.cpp` (repeated start overwrites cleanly, no orphaned handles) | Design doc §3 |

### Phase B — `IqRecorder` (pre-WDSP raw I/Q tap)

| # | Task | Tests | Cite |
|---|---|---|---|
| B.1 | New subscriber alongside the existing FFT fork (design doc §6: `RadioModel::forkIqToTaps()`, `RadioModel.cpp:15710-15730`) — does not touch the audio or FFT paths, purely additive. | `tst_radio_model_iq_tap_fork.cpp` (new tap receives identical samples to the FFT fork, doesn't alter existing subscriber behavior) | Design doc §6, §7.2 |
| B.2 | `IqRecorder` class: writes interleaved float32 (I=left, Q=right) RIFF/WAVE at the DDC's own input rate, on its own thread — structural mirror of Thetis's `ReceiverInputIQ` tap (`cmaster.cs:2176-2189`). | `tst_iq_recorder_capture.cpp` (captured file's sample values and rate match the fed I/Q stream) | Design doc §4, §7.2 |
| B.3 | Metadata: decide and implement — sidecar JSON (Thetis's convention, design doc §4) vs. an alternative. This is the design doc's second open question (§8); resolve it here rather than deferring further, since `PlaybackRadioConnection` (Phase C) needs to read whatever this writes. | `tst_iq_recorder_metadata.cpp` (freq/mode/band round-trip through whatever format is chosen) | Design doc §4, §8 |

### Phase C — `PlaybackRadioConnection`

| # | Task | Tests | Cite |
|---|---|---|---|
| C.1 | `PlaybackRadioConnection : RadioConnection` skeleton — implements the pure-virtual interface (`init()`, `connectToRadio()`, `setReceiverFrequency()`, etc.) with file-backed or no-op behavior as appropriate; does NOT go through the discovery-driven `create()` factory (design doc §6). | `tst_playback_radio_connection_skeleton.cpp` (interface satisfied; instantiable without a discovery result) | Design doc §6, §7.3 |
| C.2 | File reader + timer-paced `iqDataReceived(hwReceiverIndex, samples)` emission, reading whatever `IqRecorder` (B.2/B.3) wrote; cadence matched to the recorded sample rate so downstream consumers (audio, FFT) see realistic timing. | `tst_playback_radio_connection_replay.cpp` (emitted samples and timing match the source file; downstream `feedIqData` fan-out receives them unmodified) | Design doc §6, §7.3 |
| C.3 | Construction path: a non-discovery way to start a playback "connection" (e.g. a file-open entry point parallel to the radio-discovery Connect dialog, wired at the `RadioModel`/`MainWindow` level — no dialog layout decided here, just the signal/slot seam a future dialog would call). | `tst_playback_radio_connection_construction.cpp` (constructing and starting playback doesn't require any discovered `RadioInfo`) | Design doc §6 |
| C.4 | End-of-file behavior: stop cleanly (emit a disconnect-equivalent state) vs. loop — pick one default, make it a parameter either way. Not resolved in the design doc; decide here since it's a small, low-stakes implementation choice, not an architecture question. | `tst_playback_radio_connection_eof.cpp` (both stop and loop modes behave correctly) | New (not in design doc) |

### Phase D — Scheduled recording

| # | Task | Tests | Cite |
|---|---|---|---|
| D.1 | `RecordingScheduler` — AppSettings-backed list of (start time or "in N minutes", duration, target: WAV/IQ, slice) entries; no `MemoryForm`-equivalent exists in Longpath to hang this off of (design doc §8), so this is new persistent settings, not a port. | `tst_recording_scheduler_settings.cpp` (round-trip persistence) | Design doc §3, §8 |
| D.2 | Timer polling + auto-start/auto-stop, driving `WavRecorder`/`IqRecorder` from Phase A/B. Duration-based auto-stop mirrors Thetis's `DurationCount` countdown concept (design doc §3) without adopting its per-memory-slot coupling. | `tst_recording_scheduler_timer.cpp` (uses a short real or simulated duration to prove start/stop fire on schedule) | Design doc §3 |

### Phase E — Verification

| # | Task | Description |
|---|---|---|
| E.1 | Unit-test sweep + build green: `cmake --build build && ctest --output-on-failure`. All new tests pass, no regressions in the existing `QsoRecorder`/`AudioTapRing` tests A.2 sits alongside. | No hardware needed. |
| E.2 | **Live bench check** (any connected radio): record a short WAV clip and a short I/Q clip against a real signal; stop; play the WAV back through Longpath's normal audio path and confirm it's audible and undistorted; load the I/Q clip into `PlaybackRadioConnection` and confirm the panadapter/waterfall render the same signal shape as the original live capture did. This is the one point where "recorded vs. live" actually needs comparing side by side. | Needs one working radio connection at the time (any of this session's radios). |
| E.3 | **No-hardware check**: with no radio connected at all, open a previously recorded I/Q file via `PlaybackRadioConnection` and confirm the full RX chain (spectrum, waterfall, audio) works exactly as it would with live hardware — this is the concrete payoff of choosing Option A (design doc §7.3) and is worth verifying explicitly, since it's the scenario that would have helped earlier this session. | No hardware needed — the point of the test is that none is needed. |
| E.4 | Verification notes appended to this plan (or a short `phase3m-recording-verification.md` if the project's usual per-phase matrix convention is wanted) — record what E.2/E.3 actually showed. | Docs only. |

---

## 3. Ordering

- **A and B are independent of each other** — both build on already-existing infrastructure (`AudioTapRing`/`WavFile` for A, the existing I/Q fork for B) and don't share code. Can proceed in parallel.
- **C depends on B.3** (needs a settled file format to read) but not on A.
- **D depends on A and B both existing** (it drives them, doesn't replace them).
- **E depends on everything.**

Suggested sequence if done serially: A → B → C → D → E. If split across sessions, A and B are each a reasonable stopping point on their own (each ships a working, testable recorder with no dependency on the others).

---

## 4. Open items this plan does not resolve

Carried forward from the design doc, still open after this plan:

- **Disk-space / duration guards** (design doc §8) — not scheduled into any task above; needs a decision on whether it's in scope for a first pass or a later hardening pass.
- **UI surface** (§0.4) — deliberately deferred to the operator, at whatever point implementation actually starts.
- **`WavPlayer` reuse for plain WAV playback** (design doc §8) — Phase A above only covers *recording*; WAV *playback* (as opposed to I/Q playback via `PlaybackRadioConnection`) isn't its own task yet because `WavPlayer.{h,cpp}` already exists and its gap (design doc §6: "plays straight to a raw `QAudioSink`, not through any bus") is a small, well-scoped fix rather than new architecture — worth a task once Phase A's tap exists to test playback against, but not written in here as its own numbered item since it wasn't part of today's research scope.

---

## 5. References

- **Design doc:** `docs/architecture/phase3m-recording-design.md`
- **Master plan:** `docs/MASTER-PLAN.md` § "Phase 3M: Recording/Playback"
- **CLAUDE.md** — source-first protocol, attribution rules, Thetis cite versioning grammar
- **CLAUDE.local.md** — build/run boundary (operator-only), `syntax_check.sh` gate, manual `CMakeLists.txt` entries, visual-design boundary

---

**End of plan.**
