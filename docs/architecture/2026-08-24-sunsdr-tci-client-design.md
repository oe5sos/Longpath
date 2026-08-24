# SunSDR2 QRP via TCI — Design & the Safety-Gate Hunt

Status: **shipped** (bench-verified live against a real SunSDR2 QRP /
ExpertSDR2, OE5SOS, 2026-08-24).
Companion measurement notes: `docs/TCI-SunSDR-gemessen.md` (wire
protocol, measured against the real device with `tools/tci_probe.cpp`).
Companion tests: `tests/tst_tci_client_ddc_center.cpp`,
`tests/tst_tci_client_vfo_and_modulation.cpp`,
`tests/tst_sunsdr_connect_wiring.cpp`,
`tests/tst_sunsdr_spectrum_wiring.cpp`,
`tests/tst_sunsdr_control_wiring.cpp`.

## What this documents

Longpath acting as a **TCI 1.4 client** against a foreign TCI server —
ExpertSDR2, driving the operator's own SunSDR2 QRP. The SunSDR2 QRP
speaks no OpenHPSDR protocol at all (Expert Electronics does not
publish it), so the only path in is TCI, the same wire protocol
Longpath already speaks as a *server* (`TciServer`, Phase 3J-1) for
clients like WSJT-X. This is the mirror image: here Longpath is the
client, ExpertSDR2 is the server.

Built in four steps over one session (2026-08-24): Verbindung
(connect) → Ton (audio) → Bild (panadapter) → Steuerung (bidirectional
frequency/mode). Each step reused existing Longpath infrastructure
rather than building a parallel path — the design decision that
mattered most, and the one that shaped everything else below.

## Why this is a client, not another board driver

Every other radio Longpath talks to (ANAN, HL2, Anvelina, …) is a
`RadioConnection` implementation: Longpath owns the DDC/WDSP pipeline,
the radio is an ADC/DAC at the end of a protocol. SunSDR2 QRP is
different — ExpertSDR2 already runs its own DSP chain and decides what
audio and I/Q leave the box. Longpath cannot demodulate CW on
ExpertSDR2's behalf; it can only ask ExpertSDR2 to demodulate for it,
receive the result, and display/play it. That is why "Steuerung"
exists at all: without it, Longpath's own VFO flag would show a
frequency it does not control, disagreeing with what the operator
actually hears.

## The path

```
ExpertSDR2 (TCI 1.4, ws://host:40001)
    │  text channel: dds:/vfo:/modulation:/iq_samplerate:/
    │  audio_samplerate:/start/stop  (broadcast to every client,
    │  including receivers 0 AND 1 — see "Two receivers" below)
    │  binary channel: 64-byte header + I/Q or RX-audio frames
    ▼
TciClient (src/core/TciClient.h/.cpp)
    │  true rates + dds:/vfo:/modulation: come from the TEXT channel
    │  ONLY — the binary header's rate (offset 4) and channel-count
    │  (offset 28) fields both lie on this device (measured,
    │  docs/TCI-SunSDR-gemessen.md)
    ▼
MainWindow_SunSdr.cpp  ── wireSunSdr() / wireSunSdrOutboundControl()
    │
    ├─ Ton:      audioFrameReady → AudioEngine::feedSunSdrAudioData
    │             (opportunistic MasterMixer slot, same infra a real
    │             receiver's RX audio uses)
    │
    ├─ Bild:      iqFrameReady → a real FFTEngine, parked at a
    │             reserved pseudo stream index (100000, chosen far
    │             outside any real DDC's range) → FFTRouter →
    │             whichever pan the target slice is shown on. Same
    │             pipeline a real receiver's panadapter uses — not a
    │             second, parallel spectrum path.
    │
    └─ Steuerung: vfo:/modulation: ↔ SliceModel::frequency()/dspMode(),
                  both directions, guarded (see below).
```

## The one invariant everything else serves

**A `SliceModel` with a real DDC binding (`streamIndex() >= 0`) —
an actual, possibly TX-capable radio — must never be fed or
controlled by this path, in either direction.**

This is not a design preference. It is the safety line from
`CLAUDE.local.md`: *"Wo Zurückhaltung und Sicherheit sich
widersprechen, gewinnt die Sicherheit."* SunSDR is a second,
independent radio; if its state could leak into a slice that a real
transmitter owns, ExpertSDR2 (or whatever the operator is doing in
it) could silently retune or reconfigure hardware the operator
believes only Longpath's own connection controls.

The invariant is enforced by three cooperating pieces, not one:

1. **`connectSunSdr()`'s slice fallback never picks a bound slice.**
   Active slice, then the first *unbound* slice in the list, then a
   freshly created one — a bound slice is skipped outright, never
   reused. (Root-cause fix; see defect 1 below.)
2. **`sunSdrControllableSlice()`** is the single function every
   consumer — audio, panadapter, inbound control, outbound control —
   must call to resolve "the current SunSDR slice, if it is still
   safe to touch." It returns `nullptr` the instant the target
   becomes real-bound. No consumer resolves the slice any other way.
3. **`releaseSunSdrSlice()`** tears down all three surfaces together
   — audio source, router mapping, outbound wiring — the moment the
   target slice is deleted (`RadioModel::sliceRemoved`) or claimed by
   a real radio later in the session (`SliceModel::streamIndexChanged`,
   e.g. via `RadioModel::bindUnboundSlices()` on the next real
   connect). It also resets the target id to `-1`, so a later slice
   that happens to reuse the same (now-free) index — `RadioModel::
   addSlice()` always hands out the lowest free id — inherits
   nothing.

An echo guard (`m_sunSdrApplyingRemoteState`, a plain bool flag around
each inbound apply — same pattern as `RadioModel::
m_rollingBackFrequency`) stops an incoming TCI line from being
immediately re-sent outbound as if the operator had typed it.

## The defects found, in order

Four sessions of "measure, don't guess" against the real device, each
building on the last.

1. **`startIqStream()` missing.** The very first cut of "Bild" called
   `startAudioStream()` but never `startIqStream()`. Audio worked, the
   panadapter stayed black. `dds:` (the DDC centre) arrives regardless
   of stream state, so the panadapter's centre frequency was already
   correct — which hid the missing call on first glance at the
   operator's screen. Fixed by adding the matching call.

2. **Two receivers, one filter missing at a time.** `tci_probe`
   (parallel to a live Longpath session) showed `trx_count:2` — the
   SunSDR2 QRP reports **two** receiver slots over TCI even though
   only one is used, and *both* get their own `dds:`/`vfo:`/
   `modulation:` lines. Receiver 1's line always arrives after
   receiver 0's in the self-description, so an unfiltered handler
   would silently prefer receiver 1's (stale, unrelated) value. Found
   and fixed once for `dds:` (the panadapter kept re-centring on
   receiver 1), then found *again* for `vfo:`/`modulation:` when
   "Steuerung" landed, then found a *third* time (by an adversarial
   review, not live testing) on the **binary** I/Q/audio channel,
   which had never had the filter applied at all. Every receiver-index
   field on every channel — text and binary — now checks
   `receiver == kSunSdrReceiverIndex` before doing anything.

3. **`cwl`/`cwu` outbound: ExpertSDR2 doesn't know them.** Longpath's
   own `DSPMode` distinguishes CW-lower and CW-upper; TCI's own
   `modulations_list` self-description only advertises the generic
   `cw`. An early cut sent `cwl`/`cwu` outbound — syntactically valid
   TCI, silently ignored by ExpertSDR2. Fixed by collapsing both to
   `cw` outbound. The fix has its own tail: since ExpertSDR2 echoes
   every state change back to all clients, a `cw` echo after
   Longpath's own outbound CWU selection must **not** be read as "go
   to CWL" — `applyRemoteSunSdrModulation()` only treats an incoming
   generic `cw` as a real mode change when the slice is not *already*
   in a CW mode.

4. **The safety invariant covered control, not audio or the
   panadapter.** An adversarial code review (a fresh agent, primed
   with the invariant and told to find where it was not actually
   enforced) found that `streamIndex() >= 0` was checked in exactly
   two places — the two control paths — and nowhere else. The audio
   and panadapter wiring trusted `m_sunSdrTargetSliceId` unconditionally.
   Concretely: if a real radio's active slice happened to be picked by
   `connectSunSdr()`'s fallback, SunSDR's audio and panadapter would
   have silently overwritten a real receiver's — the exact class of
   thing the invariant exists to prevent, just via a door nobody had
   locked. The same review found two more real gaps in the *timing* of
   the existing control gate (checked once at wiring time, not on
   every subsequent event; and no reaction at all to the target slice
   being deleted or later claimed by a real radio). All three,
   plus the binary-channel filter from defect 2, were closed together
   — see "The one invariant" above for the resulting design, not the
   original point fixes.

## Deliberately out of scope

- **TX.** SunSDR2 QRP integration here is receive-only by design
  ("Empfangs-Aufsatz ohne Senden") — no `trx:`/`tune:` commands are
  ever sent.
- **VFO channel B / split.** Longpath speaks only VFO channel A
  (`vfo:<rx>,0,…`) in both directions — the "kept simple" scope
  decision the operator made at the outset (one SunSDR, no profile
  manager, no split). Revisit only on explicit request.
- **Multi-device / profile management.** One SunSDR, fixed
  connection, no `KiwiSdrManager`-style list. Same decision as above.

## What a future reader should NOT need to re-derive

- The exact wire strings, field lies, and the `dds:`/`vfo:`/`if:`
  arithmetic identity (`dds + if = vfo`) are in
  `docs/TCI-SunSDR-gemessen.md`, not repeated here.
- `kSunSdrPseudoStreamIndex` (100000) is a deliberately out-of-range
  sentinel, not a magic number — see the comment at its declaration in
  `MainWindow_SunSdr.cpp` for why 100000 specifically is safe against
  every real DDC index this codebase can produce.
- `SliceModel::streamIndex()` is one of three separate slice-numbering
  domains (see `MainWindow_SunSdr.cpp` header comments and
  `SliceModel::sliceIndex()` vs. hardware DDC index) — do not assume
  it means "the letter shown on the VFO flag."
