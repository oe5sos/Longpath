# Phase 3M Design: Recording / Playback

**Status:** Reviewed and implemented — see `docs/architecture/phase3m-recording-plan.md` for the implementation plan and current build/test status.
**Author:** Martin Fischer (OE5SOS), AI-assisted (Claude Code).
**Date:** 2026-08-25.
**Master-plan slot:** `docs/MASTER-PLAN.md` § "### Phase 3M: Recording/Playback" (`docs/MASTER-PLAN.md:973-982`).
**Upstream stamps captured this session:**
- Thetis `v2.10.3.15-5-g852bf0e` (`@852bf0e`)

---

## 1. Goal

Give Longpath operator-visible parity, scoped to what this session's four research passes actually covered, for:

- **WAV audio record/playback** — record the demodulated (post-WDSP) receive audio to a `.wav` file, and play a `.wav` file back through the same audio path an operator can hear.
- **I/Q recording** — record the raw, pre-demodulation receiver I/Q stream to a file for later analysis or re-play.
- **Quick record** — a one-button, no-dialog scratch-pad recording an operator can start/stop without naming a file or picking parameters.
- **Scheduled recording** — a start/stop that fires at an operator-set time and/or duration without the operator sitting at the console.

Everything else — I/Q *playback* substituting for a live radio (an operator tunes into a previously captured file the way Thetis's own file-playback tap does, or the "dev/demo without hardware" bullet in `docs/MASTER-PLAN.md:978` implies) — is explicitly in scope for §7's architecture sketch, but no operator-facing acceptance criteria are asserted for it yet: today's research established *how* Thetis does file-driven playback and *whether* Longpath has an analogous seam, not a finished UX for it. That gap is called out again in §8.

Out of scope for this document (not researched today): QSO audio logging (`QsoRecorder`), TX voice-check recording (`TxAudioRecorder`, `ClientPuduMonitor`), and keyer memory (`DvkApplet`) — these are real, shipped Longpath features but are not RX "off the air" WAV/I-Q recording, per §5.

---

## 2. Source provenance and licensing

All Thetis facts in §3 and §4 were read against Thetis `v2.10.3.15-5-g852bf0e` (`@852bf0e`), captured once for this research session. Every Thetis citation below carries that stamp per this repo's inline-cite grammar.

This is a design document, not a port — no code is written here, so no license headers are copied yet. When implementation begins, the license-preservation rule and the inline-comment/author-tag preservation rule in `CLAUDE.md` govern verbatim (see `CLAUDE.md` §"License-preservation rule (non-negotiable)" and §"Inline comment preservation — SHIP-BLOCKING"); this document does not restate them.

---

## 3. Thetis WAV audio recording and playback

**Core class:** `Console/clsAudioRecordPlayback.cs` (`clsAudioRecordPlayback`, 4042 lines) [v2.10.3.15-5-g852bf0e], backed by the unmanaged-callback plumbing in `Console/cmaster.cs` (`WaveThing`, `RecordWave`, `PlayWave` — calls into `ChannelMaster.dll`) [v2.10.3.15-5-g852bf0e]. NAudio 2.3.0 is referenced in `Console/Thetis.csproj:273-292` [v2.10.3.15-5-g852bf0e] (Core/Wasapi/WinMM/WinForms/Asio/Midi) and used only for the *PC-audio* path.

### Two independent record/playback engines

1. **WDSP path** (`RecordToFileFromWDSP`, `clsAudioRecordPlayback.cs:1154` [v2.10.3.15-5-g852bf0e]; `PlayFileViaWDSP:1736` [v2.10.3.15-5-g852bf0e]) — taps/injects audio inside the DSP chain via a hand-rolled native `WaveFileWriter`/`WaveFileReader1` (not NAudio), driven by callbacks `RecordWave.wrecord`/`PlayWave.wplay` registered with `ChannelMaster.dll` (`cmaster.cs:2022-2038` [v2.10.3.15-5-g852bf0e]).
2. **PC-audio path** (`RecordToFileFromPCAudio:1334` [v2.10.3.15-5-g852bf0e]; `PlayFileViaPCAudio:1862` [v2.10.3.15-5-g852bf0e]) — genuine NAudio: `IWaveIn`→`NAudio.Wave.WaveFileWriter` for capture (`:1473` [v2.10.3.15-5-g852bf0e]), `AudioFileReader`+`WaveOutEvent`/`WasapiOut` for playback to the PC's chosen output device (`:1911-1936` [v2.10.3.15-5-g852bf0e]).

### Tap point (WDSP path)

`RxSource` (default `AudioRecordRxSource.ReceiverOutputAudio`, `:258` [v2.10.3.15-5-g852bf0e]) selects `rxpre` in `RecordWave.wrecord` (`cmaster.cs:2164-2239` [v2.10.3.15-5-g852bf0e]):

- `ReceiverOutputAudio` (default) → **pos==1 "post"**, `cmaster.GetChannelOutputRate` — tap is the **receiver channel output, i.e. after WDSP processing/AGC**, gain-matched via `RecordGain = RXOutputGain` (`cmaster.cs:1020` [v2.10.3.15-5-g852bf0e]).
- `ReceiverInputIQ` → **pos==0 "pre"**, `cmaster.GetInputRate` — tap is **before WDSP** (raw channel input).

### WAV format parameters (WDSP record, `:1202-1282` [v2.10.3.15-5-g852bf0e])

`BitDepthMode` selects: `IeeeFloat32`→32-bit/formatTag 3, `Pcm32`→32/1, `Pcm24`→24/1, `Pcm16`→16/1, `Pcm8`→8/1. Always **2 channels** (`d.Channels = 2`, `:1237` [v2.10.3.15-5-g852bf0e]; writer constructed `new WaveFileWriter(id, 2, SampleRate, …)`, `:1273-1282` [v2.10.3.15-5-g852bf0e]). `SampleRate` defaults to **48000** (`:260` [v2.10.3.15-5-g852bf0e]), user-settable, validated ≥6000 (`:1173` [v2.10.3.15-5-g852bf0e]). PC-audio path builds an explicit `WaveFormat` per mode (Ieee float or PCM at chosen bits/channels, `:1378-1404` [v2.10.3.15-5-g852bf0e]).

### Start/Stop (no Pause)

`StopRecord` (`:1579` [v2.10.3.15-5-g852bf0e]) and `StopPlayback` (`:2052` [v2.10.3.15-5-g852bf0e]) are the only terminal controls; a whole-Console-tree grep found no record/playback pause — the only "Pause" in the codebase is `Display.PausedDisplay` (waterfall), unrelated to audio (`display.cs:3805` [v2.10.3.15-5-g852bf0e], `ucInfoBar.cs:69` [v2.10.3.15-5-g852bf0e]).

### Quick Record (one-button scratch-pad)

`ckQuickRec_CheckedChanged` (`console.cs:36805-36850` [v2.10.3.15-5-g852bf0e]) toggles `ARP.RecordToFileFromWDSP("quick", …, wfw_id 0, remove_if_file_exists:true, …)` to a fixed overwrite file `AudioFolder\quickrecord\SDRQuickAudio.wav`; `ckQuickPlay_CheckedChanged` (`:36761-36795` [v2.10.3.15-5-g852bf0e]) plays it back via `ARP.PlayFileViaWDSP("quick", file, 0, …)`. The two checkboxes disable each other while active.

### Scheduled/timed recording

`Memory/MemoryForm.cs` implements a per-memory-slot scheduler thread `SCHEDULER()` (`:1010-1077` [v2.10.3.15-5-g852bf0e]) that polls once a minute (UTC), and a `DurationCount` countdown (minutes, clamped 0-120 at `:961-962` [v2.10.3.15-5-g852bf0e]) that auto-stops. Start is triggered from the schedule-hit branch (`:1315-1328` [v2.10.3.15-5-g852bf0e]) via `console.RECPOST = true`; the `RECPOST` setter (`console.cs:12111-12183` [v2.10.3.15-5-g852bf0e]) builds a `RecordingDetails`, names the file `{mode}_{freqMHz}_[bits_Hz]_{isoTimestamp}.wav`, and calls `ARP.RecordToFileFromWDSP("scheduled", …AudioFolder\scheduled\…, wfw_id 0, …)`. When `DurationCount` reaches 1, `RECPOST = false` calls `ARP.StopRecord` and spawns an MP3-conversion thread (`:1042-1064` [v2.10.3.15-5-g852bf0e]).

---

## 4. Thetis I/Q recording and file-based playback

**Location:** `Project Files/Source/Console/clsAudioRecordPlayback.cs` (recorder/reader classes) tied into `Project Files/Source/Console/cmaster.cs` (DSP-tap glue to `ChannelMaster.dll`) [v2.10.3.15-5-g852bf0e]. There is no `IQRecord`/`PlaybackFile`/`SimulatedRadio`/`PlaybackRadio` class name in this codebase — the feature is called "Audio Record/Playback" (`clsAudioRecordPlayback.cs`, exposed as `console.ARP`), with an RX source selector:

```csharp
// clsAudioRecordPlayback.cs:54-58 [v2.10.3.15-5-g852bf0e]
public enum AudioRecordRxSource
{
    ReceiverInputIQ = 0,
    ReceiverOutputAudio = 1
}
```

Set from Setup > Misc/Recording tab: `setup.cs:36552-36556` [v2.10.3.15-5-g852bf0e].

**Where raw I/Q is tapped (pre-demod):** `cmaster.cs:2164-2223` [v2.10.3.15-5-g852bf0e], `RecordWave.wrecord()`. `pos==0` is the "pre" tap (before demodulation, still complex I/Q), `pos==1` is "post" (demodulated audio). `ReceiverInputIQ` maps to `RxPre=true`, which selects the pre-demod branch:

```csharp
// cmaster.cs:2176-2189 [v2.10.3.15-5-g852bf0e]
if ((state == 0) && rxpre)
{
    int rcvr_inrate = cmaster.GetInputRate(0, id);
    int rcvr_insize = cmaster.GetBuffSize(rcvr_inrate);
    deswizzle(rcvr_insize, data, pleft, pright);
    ...
    WaveThing.wave_file_writer[id].AddWriteBuffer(pleft, pright, rcvr_insize);
}
```

`deswizzle` splits the DSP's interleaved complex buffer into I/Q float arrays (`cmaster.cs:2252-2259` [v2.10.3.15-5-g852bf0e]), so the capture rate is the DSP channel's own input rate (`GetInputRate`, per-RX-channel post-DDC rate), *not* audio rate — confirming pre-demod IQ at the receiver's processing sample rate.

**File format:** plain RIFF/WAVE, no IQ-specific chunk or metadata tag (`clsAudioRecordPlayback.cs:3553-3569` [v2.10.3.15-5-g852bf0e], `WriteWaveHeader`): standard `RIFF`/`WAVE`/`fmt `(16-byte)/`data` chunks, 2 channels (L=I, R=Q), format tag PCM(1) or IEEE_FLOAT(3), configurable bit depth (8/16/24/32) and sample rate (6 kHz–1.536 MHz, `setup.cs:36504-36531` [v2.10.3.15-5-g852bf0e]). Frequency/mode/band/sample-rate metadata is written to a **separate sidecar JSON** (`writeRecordingJson`, `clsAudioRecordPlayback.cs:2584+` [v2.10.3.15-5-g852bf0e]), not embedded in the WAV.

**Playback substitution mechanism:** it is a **separate native DSP-tap code path, not an emulated NetworkIO/hardware source**. `WaveThing` (`cmaster.cs:1952-2047` [v2.10.3.15-5-g852bf0e]) registers C# callback delegates (`WPlay`/`WRecord`) directly with `ChannelMaster.dll` via `SendCBWavePlayer`/`SendCBWaveRecorder`. During playback, `PlayWave.wplay()` (`cmaster.cs:2079-2103` [v2.10.3.15-5-g852bf0e]) is invoked by the native DSP engine mid-pipeline and overwrites the channel's working buffer in place:

```csharp
// cmaster.cs:2087-2092 [v2.10.3.15-5-g852bf0e]
int size = 2048;
WaveThing.wave_file_reader[id].GetPlayBuffer(pleft, pright);
swizzle(size, pleft, pright, data);   // writes directly into ChannelMaster's IQ buffer
```

So playback injects recorded I/Q straight into an already-running DSP channel inside `ChannelMaster.dll` — it never touches `NetworkIO.cs` (`Project Files/Source/Console/HPSDR/NetworkIO.cs`), never impersonates a UDP/hardware discovery-and-stream session, and requires the DSP pipeline (i.e., an initialized radio session) to already exist. Thetis has no `SimulatedRadio`/hardware-less "no radio attached" file-source mode — grep for `Simulat*` across Console turns up only UI/TCI simulation, not a radio-hardware stand-in.

---

## 5. What Longpath already has: existing UI stubs

**Nothing.** The stubs the master plan describes no longer exist, and no replacement stub exists either.

`docs/MASTER-PLAN.md:982` claims:
```
- Recording controls wired to VfoWidget (existing button stubs)
```
This references a `VfoWidget` control described in an old "Post-3E" note (`docs/MASTER-PLAN.md:124`, `"VfoWidget improvements: ...floating control buttons (close/lock/record/play)..."`) — a state of the codebase that predates `VfoWidget`'s deletion on 2026-08-18 (commit `75cc2c35`, "Die VFO-Flagge ist ausgebaut"). That plan line is stale.

What those stubs actually were, recovered via `git show 75cc2c35^:src/gui/widgets/VfoWidget.cpp` (verified against this repo's actual history):
```cpp
// Record button — checkable, NYI-badged (no consumer in Stage 1)
m_recBtn = makeBtn(QStringLiteral("⏺"), kFloatingBtn);
// From Thetis console.resx:2028 — ckQuickRec.ToolTip
m_recBtn->setToolTip(QStringLiteral("Quick Record of \"off the air\" signals"));
m_recBtn->setCheckable(true);
connect(m_recBtn, &QPushButton::toggled, this, [this](bool on) {
    if (!m_updatingFromModel) { emit recordToggled(on); }
});
NyiOverlay::markNyi(m_recBtn, QStringLiteral("phase3g10-stage2"));
// ...(m_playBtn, identical pattern, tooltip "Quick Playback of signals recorded \"off the air\"",
//     From Thetis console.resx:1941 — ckQuickPlay.ToolTip)
```
with signal declarations at `VfoWidget.h:571-573`:
```cpp
// --- Record/play signals (S1.10 — NYI in Stage 1, wired to future recording subsystem) ---
void recordToggled(bool recording);
void playToggled(bool playing);
```
A `git grep recordToggled|playToggled` across that entire commit found **no connection anywhere else in the tree** — these were always dead-end signals, never wired to a slot.

The deletion was deliberate, not an accidental drop. `src/gui/MainWindow.cpp`, the operator's own closeout comment on the VfoWidget removal:
```
//   * recordToggled/playToggled — waren an nichts verdrahtet; laut
//     Zielbild bekommen Aufnahme und Wiedergabe ein eigenes Feld
```
("were wired to nothing; per the target picture, recording and playback get their own surface.") CHANGELOG.md's deletion entry enumerates the control groups that *did* migrate to `RxApplet`; record/play is conspicuously absent, matching the MainWindow comment.

The only live forward-reference is a comment, not code: `src/gui/SpectrumWidget.h:337`:
```cpp
// Disk-spool tier deferred to Phase 3M (Recording).
static constexpr int kMaxWaterfallHistoryRows = 16384;
```

There is **zero** recording-related UI in `src/gui`, no `RecordingModel`-shaped stub in `src/models/`, and no reserved `AppSettings` keys. A broad grep did surface adjacent-but-unrelated recording systems — `src/core/audio/QsoRecorder.{h,cpp}` + `QsoRecorderApplet` (QSO audio logging), `src/core/TxAudioRecorder.*` and `ClientPuduMonitor` (TX voice-check take/playback), `DvkApplet` (keyer memory slots) — all real, wired, working features, but none are RX WAV/I-Q "off the air" recording per Phase 3M's scope, and none reserve `AppSettings` keys for it.

**Conclusion: Phase 3M has no existing UI hooks to wire against.** A new surface must be designed from scratch. (But see §6 — while the *UI* is gone, a substantial amount of reusable *backend* infrastructure for exactly this feature already exists and ships today.)

---

## 6. Longpath's current audio/IQ pipeline

### Radio → Audio chain

```
RadioConnection::iqDataReceived(int hwReceiverIndex, QVector<float> samples)   // RadioConnection.h:559
  → RadioModel.cpp:8583-8586  connect(..., DirectConnection) lambda
        → m_receiverManager->feedIqData(ddcIndex, samples)
  → ReceiverManager::feedIqData()  (ReceiverManager.cpp:359-396)
        → emit iqDataForReceiver(logicalIndex, samples)   // :396
  → RadioModel.cpp:8733-8735  connect(iqDataForReceiver → RxDspWorker::processIqBatch, Qt::QueuedConnection)
  → RxDspWorker::processIqBatch()  (RxDspWorker.cpp:472)
        → rxCh->processIq(acc.i.data(), acc.q.data(), ...)   // :647, fexchange2 inside RxChannel
        → m_audioEngine->rxBlockReady(sliceIdx, interleaved, outSize);  // :777
  → AudioEngine::rxBlockReady(int sliceId, const float* samples, int frames)  (AudioEngine.cpp:1097)
        → m_masterMix.accumulate(sliceId, samples, frames, muted...)   // :1177 — demodulated audio, pre-volume, pre-mixer
        → drains m_qsoTap / m_asrTap here (AudioEngine.cpp:1295-1305)
```

`CLAUDE.md`'s `feedAudio()`/`m_rxBuffer`/`QAudioSink` description is stale for the current tree — Phase 3O replaced it with the `IAudioBus`-per-endpoint model; the live entry point is `rxBlockReady`, and output is one of `m_speakersBus`/`m_headphonesBus`/`m_vaxBus[0..3]` (all `IAudioBus`).

### Radio → Spectrum/FFT chain

The same `iqDataForReceiver` emit forks a second way, **DirectConnection** on the connection thread (`RadioModel.cpp:8689-8692`) into `RadioModel::forkIqToTaps()` (`RadioModel.cpp:15710-15730`):

```cpp
if (receiverIndex == 0) { emit rawIqData(samples); }      // untagged, single-stream subscribers (TciServer.cpp:568)
emit rawIqDataForStream(receiverIndex, samples);            // tagged, per-stream — MainWindow.cpp:2038
```

`MainWindow.cpp:2038` connects `rawIqDataForStream` (filtered by `streamIndex`) to `engine->feedIQ(samples)` on the per-stream `FFTEngine`.

### RadioConnection as a pluggable interface — confirmed

`RadioConnection` (`RadioConnection.h:98`) is an abstract `QObject` base: `init()`, `connectToRadio()`, `setReceiverFrequency()`, `sendTxIq()` etc. are pure virtual; concrete work is done by `P1RadioConnection`/`P2RadioConnection`. Selection is a factory switch, verified at `RadioConnection.cpp:21-33`:

```cpp
switch (info.protocol) {
case ProtocolVersion::Protocol2:
    return std::make_unique<P2RadioConnection>();
case ProtocolVersion::Protocol1:
    return std::make_unique<P1RadioConnection>();
}
```

A `PlaybackRadioConnection` subclassing `RadioConnection` and emitting `iqDataReceived(hwReceiverIndex, samples)` from a WAV/IQ-file reader on a timer would enter the *exact same* `feedIqData`→audio/FFT fan-out real hardware uses — no downstream code needs to know. `create()` would need a third protocol/branch (or a separate construction path bypassing `create()`, since it isn't driven by discovery).

**No precedent exists for this pattern.** A grep found no `SimulatedRadio`/`FileSource`/`PlaybackConnection`/`MockRadio` outside `tests/TestMockRadioModel.*` (a Qt-signal test double, not IQ-file-backed). `TciClient` (`TciClient.h:50`) is `QObject`-only — **not** a `RadioConnection` subclass — it bypasses `RadioConnection` entirely and wires directly to `SliceModel`/`AudioEngine`/`FFTEngine` (design doc `docs/architecture/2026-08-24-sunsdr-tci-client-design.md`). It's useful evidence that "a non-hardware source feeding existing infra" is a pattern Longpath already tolerates, but it's not a `RadioConnection`-interface template.

### IAudioBus-style abstraction — a ready recording-tap template already exists

`IAudioBus` (`IAudioBus.h:29`) is the IO abstraction (`open/close/push/pull/flush`, backends `CoreAudioHalBus`/`LinuxPipeBus`/`PortAudioBus`). More directly relevant: **`AudioTapRing`** (`src/core/audio/AudioTapRing.h`, confirmed present) is a lock-free, allocation-free SPSC ring already used for exactly this purpose — QSO recording and ASR, confirmed at `AudioEngine.h:402,414`:

```cpp
void setQsoTap(AudioTapRing* ring, int sliceId);
void setAsrTap(AudioTapRing* ring, int sliceId);
```

drained inside `rxBlockReady` at `AudioEngine.cpp:1295-1305`, explicitly documented as tapping "VOR MasterMixer und Lautstärkeregler" (pre-mixer, pre-volume) — the same point WDSP-demodulated audio is available. A WAV recorder tap should follow this exact pattern (own `AudioTapRing` slot, main-thread drain via `QTimer` into `WavFile`).

Existing WAV infra to reuse directly: `src/core/audio/WavFile.{h,cpp}` (confirmed present, Thetis-ported writer/reader, carries a Richard Samphire MW0LGE copyright/dual-license header), `QsoRecorder.{h,cpp}`, `QsoRecorderController.{h,cpp}`, `QsoRecorderApplet` — a working post-WDSP-audio-to-WAV pipeline already ships. `WavPlayer.{h,cpp}` also exists but plays straight to a raw `QAudioSink` (`WavPlayer.cpp:130`), not through any bus or into the RX chain — not reusable for I/Q injection without rework.

**Pre-WDSP raw I/Q recording has no existing tap** — it needs a new subscriber on `RadioConnection::iqDataReceived` (or `ReceiverManager::iqDataForReceiver`) alongside the FFT fork, writing interleaved float32 to a `.wav`/raw-IQ container on its own thread.

**Bottom line for the question this section was asked:** yes — `RadioConnection`'s virtual interface could host a file-backed `PlaybackRadioConnection`, and the research found no architectural obstruction to it; the only missing piece is that nothing like it has been built yet, so there's no precedent to copy, only the interface contract to satisfy.

---

## 7. Proposed Longpath architecture (sketch)

Two new recorder classes, mirrored on Thetis's two taps, plus a playback decision that has to be made deliberately because it doesn't have a clean Thetis analogue in Longpath's shape.

### 7.1 `WavRecorder` — post-WDSP audio tap

Follows the `AudioTapRing` pattern §6 already documents (`setQsoTap`/`setAsrTap`, `AudioEngine.h:402,414`, drained at `AudioEngine.cpp:1295-1305`): a new `setWavRecordTap(AudioTapRing*, int sliceId)` on `AudioEngine`, drained at the same pre-mixer/pre-volume point, feeding the existing `WavFile.{h,cpp}` writer. This is the direct structural equivalent of Thetis's `ReceiverOutputAudio` (pos==1, post-WDSP) tap in `RecordWave.wrecord` (`cmaster.cs:2164-2239` [v2.10.3.15-5-g852bf0e]) — Longpath already has the ring-buffer-tap idiom and an existing WAV writer; wiring a third tap alongside QSO/ASR is additive, not new infrastructure.

### 7.2 `IqRecorder` — raw I/Q tap

New: a subscriber alongside the FFT fork on `RadioConnection::iqDataReceived` / `ReceiverManager::iqDataForReceiver`, writing interleaved float32 (I=left, Q=right) RIFF/WAVE on its own thread — the structural equivalent of Thetis's `ReceiverInputIQ` (pos==0, pre-demod) tap (`cmaster.cs:2176-2189` [v2.10.3.15-5-g852bf0e]), captured at the DSP channel's own input rate exactly as Thetis's `deswizzle`/`GetInputRate` path does. Unlike `WavRecorder`, there is no existing Longpath tap infrastructure at this point in the chain (§6) — this is genuinely new plumbing, not a third consumer of something that exists.

### 7.3 Playback — the architecturally interesting choice

Two candidate shapes, not equally well supported by the research:

**Option A — `PlaybackRadioConnection : RadioConnection`.** A file-backed subclass emitting `iqDataReceived()` from a timer-paced WAV/IQ-file reader. Enters the existing `feedIqData → iqDataForReceiver → RxDspWorker → AudioEngine` fan-out and the FFT fork (`rawIqData`/`rawIqDataForStream`) unmodified — every downstream consumer (audio buses, spectrum, QSO tap, TCI server) works with zero changes, because none of them know or care which `RadioConnection` subclass produced the samples. Needs a non-discovery construction path (`create()` today only branches on a discovered `ProtocolVersion`, `RadioConnection.cpp:21-33`), and, notably, needs **no live radio session** — it *is* the session.

**Option B — bypass injection**, mirroring Thetis's actual mechanism: inject recorded I/Q directly mid-pipeline (into `RxDspWorker`/`RxChannel`, analogous to Thetis's `PlayWave.wplay()` overwriting the channel's working buffer inside `ChannelMaster.dll`, `cmaster.cs:2079-2103` [v2.10.3.15-5-g852bf0e]). This requires an already-connected, already-running radio session — Thetis's mechanism has the same requirement, because it was built as a DSP-internal callback, not a source abstraction.

**The research suggests Option A is the better fit for Longpath's existing architecture, and this is a real divergence from Thetis, not a coincidence.** Thetis has no `RadioConnection`-shaped abstraction to subclass — its playback mechanism is a native-DSP-callback hack precisely *because* nothing like `RadioConnection`'s pluggable-source interface exists in Thetis. Longpath already has that interface, built for a different reason (P1/P2 protocol selection), and §6 found no obstruction to a third implementation of it. Option A also naturally satisfies the "dev/demo without hardware" framing MASTER-PLAN.md's Phase 3M bullet gestures at (`docs/MASTER-PLAN.md:978`), since it needs no live radio at all — the opposite of what Option B requires. (This would also have been useful more than once this session, when live-hardware network issues blocked verifying UI/waterfall changes.)

**Decision (2026-08-25, operator):** Option A — `PlaybackRadioConnection : RadioConnection`.

**Where the research did not fully resolve this:** nobody has built a `RadioConnection` subclass that isn't discovery-driven, so the exact shape of a non-`create()` construction path is unverified against real code; whether TX-side interlocks (PTT, MOX) need special-casing for a fileless "radio" wasn't examined; and whether Thetis's own file format choices (§4 — RIFF/WAVE + sidecar JSON) should be adopted as Longpath's read format for `PlaybackRadioConnection`, versus reusing whatever `IqRecorder` (§7.2) decides to write, is an open design decision, not a research finding.

---

## 8. Open questions / risks

- **WAV parameter choices (audio path).** Thetis exposes 5 `BitDepthMode` options (IeeeFloat32/Pcm32/Pcm24/Pcm16/Pcm8) always at 2 channels, default 48000 Hz, floor 6000 Hz (`clsAudioRecordPlayback.cs:1173,1202-1282,260` [v2.10.3.15-5-g852bf0e]). Today's research did not inspect `WavFile.{h,cpp}`'s current write-side capabilities in enough depth to say whether it already supports this range or needs extension — that's a gap, not a decision made here.
- **I/Q file format.** Thetis writes plain RIFF/WAVE (2ch, I=L/Q=R, PCM or IEEE-float, 8-32 bit, 6 kHz–1.536 MHz) plus a **separate sidecar JSON** for frequency/mode/band metadata (`clsAudioRecordPlayback.cs:3553-3569, setup.cs:36504-36531, clsAudioRecordPlayback.cs:2584+` [v2.10.3.15-5-g852bf0e]). Whether Longpath's `IqRecorder` follows this exact two-file convention, or embeds metadata differently, is undecided.
- **Scheduled recording's settings surface.** Thetis's scheduler is a per-memory-slot thread (`Memory/MemoryForm.cs SCHEDULER()`, polling once/minute UTC, `DurationCount` clamped 0-120 min) tied to Thetis's Memory system (`:1010-1077, 961-962, 1315-1328` [v2.10.3.15-5-g852bf0e]). Longpath has no `MemoryForm`-equivalent identified by today's research, and no existing timer/settings infrastructure for this was found — scheduled recording likely needs a new persistent settings surface and timer built from scratch, not a port of an existing Longpath scheduler.
- **Disk-space / duration guards.** None of the four research passes examined disk-space checks, maximum-duration caps, or low-disk warnings in either Thetis or Longpath. This is flagged as entirely unresearched, not merely undecided.
- **The Thetis-vs-Longpath architectural mismatch on playback (§7.3).** Thetis's playback mechanism structurally requires a live DSP session and injects mid-pipeline; Longpath's best-fit mechanism (a `RadioConnection` subclass) requires no live session and injects upstream of the whole pipeline. This divergence should be confirmed as intentional before implementation — it changes operator-visible behavior (whether playback works with "no radio connected"), and the research surfaced it but did not resolve which behavior is actually wanted.
- **`WavPlayer` reuse.** `WavPlayer.{h,cpp}` already exists and plays WAV audio straight to a raw `QAudioSink` (`WavPlayer.cpp:130`), not through any bus or into the RX chain. It is not reusable for I/Q-injection playback without rework; whether it's the right base for plain WAV *audio* playback (distinct from I/Q playback) is open.
- **Quick Record's UI home.** Thetis's Quick Record is a fixed-overwrite scratch file toggled by two mutually-disabling checkboxes (`console.cs:36761-36850` [v2.10.3.15-5-g852bf0e]). Longpath's prior UI surface for this concept (`VfoWidget`'s record/play buttons) is gone and was explicitly not migrated (§5) — where quick record's controls now live is unresolved; this is a visual/UX decision reserved for the operator (`CLAUDE.local.md`: "Technik Nereus, Design ich"), not something to pre-decide here.
- **Pause.** Thetis has no record/playback pause at all, Start/Stop only (`clsAudioRecordPlayback.cs:1579,2052` [v2.10.3.15-5-g852bf0e]). If Longpath wants pause, there is no Thetis behavior to port — it would be a Longpath-original addition, which needs explicit sign-off per `CLAUDE.md`'s "Feature scope" boundary.

---

## 9. Suggested next step

~~Resolve the Option A vs. Option B playback question (§7.3) with the operator before writing an implementation plan~~ — **resolved 2026-08-25: Option A.** Next step is an implementation plan covering, in rough dependency order: (1) `WavRecorder` post-WDSP audio tap (§7.1, additive on existing `AudioTapRing`/`WavFile` infra), (2) `IqRecorder` pre-WDSP tap (§7.2, new plumbing), (3) `PlaybackRadioConnection`'s non-discovery construction path (§7.3/§6), (4) the still-open items in §8 (WAV/IQ file-format decisions, scheduled-recording settings surface, disk-space guards, and — separately, reserved for the operator per `CLAUDE.local.md` — where the record/playback controls live in the UI).
