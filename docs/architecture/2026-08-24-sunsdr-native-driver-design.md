# SunSDR2 QRP — Native Driver (No ExpertSDR2), Design + Protocol Reference

## Why this exists

The SunSDR-via-TCI feature shipped earlier today (`2026-08-24-sunsdr-tci-client-design.md`)
requires ExpertSDR2 running as a middleman, and is receive-only by design.
2026-08-24, OE5SOS: *"ich will ohne sun sdr software funken, also nur
longpath software"* — Longpath should drive the SunSDR2 QRP directly,
the same way it already drives an ANAN or HL2: no other vendor's console
running underneath it, and with transmit.

That is a different feature, not an extension of the TCI client. TCI talks
to a documented, vendor-published companion protocol built for exactly this
purpose. This document is about the *other* protocol — the one ExpertSDR2
itself speaks to the physical radio — which Expert Electronics has never
published.

## The source-first problem, and how it's resolved

Every other radio Longpath drives is ported from a real, citable source:
OpenHPSDR Protocol 1/2 (open spec, mirrored in Thetis's C# source) for the
ANAN/HL2 family. SunSDR2's native wire protocol has no open spec —
Expert Electronics documents only TCI. Guessing it would break this
project's `SOURCE-FIRST PORTING PROTOCOL` (`CLAUDE.md`) outright, and on a
transmit path, guessing wrong is not a software bug, it's an RF safety
incident.

A citable source exists anyway: **[ArtemisSDR](https://github.com/kk68/ArtemisSDR)**
(Kosta Kanchev, K0KOZ, GPLv2-or-later — same "or-later" GPL family Longpath
itself ships under, no licence conflict, same standing as the Anvelina
gateware citation in `CLAUDE.md`). It's a Thetis fork — the same upstream
Longpath descends from — that drives SunSDR2 DX and PRO natively, RX and
TX, in production use. Its own header says plainly what it is and isn't:

> "Protocol details derived from black-box reverse engineering — passive
> observation of UDP traffic between a genuine ExpertSDR instance and an
> owned SunSDR2 DX radio. No ExpertSDR code, binaries, firmware, or artwork
> was referenced or used." — `sunsdr.c:9-11` [ArtemisSDR, kk68/ArtemisSDR]

Cross-checked against a second, independent implementation:
**[solsdr](https://github.com/jfrancis42/solsdr)** (Jeff Francis, N0GQ), a
Python client for the SunSDR2 PRO built from its own separate wire
captures. The two agree.

**This satisfies "another open-source project's code" as a port source**
under this project's own rules — the same standing as citing WDSP or
AetherSDR — but it is reverse-engineered, not vendor-documented, so cites
below use `[ArtemisSDR sunsdr.c:N]` rather than a Thetis-style
`[v2.10.3.13]` release stamp: there is no upstream release to stamp
against, only a specific file at a specific commit in a live repo. Any
future re-sync should re-pull ArtemisSDR and note the commit reached.

## The one gate that comes before any code

**Neither ArtemisSDR nor solsdr has ever run against a SunSDR2 QRP.** Both
projects' own documentation covers only DX and PRO by name — the QRP is a
lower-power sibling in the same product family, running the same
ExpertSDR2/3 software, so the protocol is *plausible* to be identical, but
that is an inference, not a confirmed fact. `sunsdr_pick_profile()` falls
back to the DX profile for any unrecognized model ID
(`sunsdr.c:2756-2766`) — which tells you what ArtemisSDR does when it
doesn't know a radio, not what a real QRP actually sends.

Everything downstream of the DX/PRO profile split in §"Key protocol facts"
below — ports, magic byte, boot macro, STATE_SYNC template, drive-byte
calibration curve — could differ for the QRP in ways that matter a great
deal more on a transmit path than a receive one: a wrong drive-byte
mapping doesn't just sound bad, it can overdrive a PA stage calibrated for
a different unit.

**Longpath must not write a `SunSdrRadioConnection` that transmits before
this is confirmed against real QRP hardware.** `CLAUDE.local.md`: *"Wo
Zurückhaltung und Sicherheit sich widersprechen, gewinnt die Sicherheit."*
The concrete next step, before any production RX or TX code:

1. Build a read-only probe tool (`tools/sunsdr_probe.cpp`, mirroring
   `tools/tci_probe.cpp`'s role for TCI) that opens the two UDP sockets
   this document describes and logs traffic — either passively sniffing
   real ExpertSDR2 ↔ QRP traffic on the bench, or actively sending
   ArtemisSDR's documented commands and recording the QRP's actual
   replies.
2. Diff the QRP's real behaviour against every fact table in this
   document. Where the QRP agrees with the DX profile, it's confirmed,
   not assumed. Where it doesn't, that's a new profile entry, not a
   footnote.
3. Only after that: port the control-channel and RX IQ path (receive-only
   first, mirroring how the TCI client and the P1/P2 drivers themselves
   were both proven on the bench before the next step was trusted).
   Transmit is a separate, later gate — see "Phasing" below.

Nothing in this document should be read as "ready to port." It's the
reference to port *from*, once step 1 confirms the QRP actually matches
it.

## Confirmed from real QRP capture (2026-08-24)

Step 1 of the gate above has now been done for real, not just built.
OE5SOS captured genuine ExpertSDR2 ↔ QRP traffic on the bench —
`sudo tcpdump -i any -w /tmp/sunsdr-capture.pcap 'host 192.168.16.200'`
while power-cycling the radio and operating normally in ExpertSDR2 —
21,720 packets. (A first attempt to parse the capture with a
hand-rolled PCAP-NG walker found zero matching packets: macOS's `any`
interface prepends PKTAP framing before the IP header, which the
walker's offset-scan never located. The reliable path was parsing
`tcpdump -r ... -x`'s own text+hex dump, since tcpdump already strips
PKTAP before printing.)

This directly answers the standing question of why `sunsdr_probe`
(built against the DX profile) got no reply on the bench twice, even
once `ping` was working: **wrong magic byte, not a network or
busy-radio problem.**

### Confirmed: ports and magic byte

| Model | Control port | Stream port | Magic byte `[0]` |
|---|---|---|---|
| SunSDR2 QRP | **50001 (confirmed)** | **50002 (confirmed)** | **`0x03` (confirmed)** |

Matches the DX port assignment exactly (50001/50002, not PRO's
50002/50003) but **not** the DX magic byte (`0x32`). Confirmed on
21,716 of 21,718 decoded control- and stream-channel packets — the two
exceptions are the `0x33` anomaly documented below. Byte `[1]` confirmed
`0xFF` universally, matching the documented header. The "unconfirmed"
placeholder row two sections up is superseded by this one.

### Confirmed: both header formats, byte-for-byte, once the magic byte is corrected

Every one of the 92 control-channel packets (port 50001, the single
boot/session sequence captured) decodes cleanly against the documented
18-byte header — field positions, sizes, and byte order all match
ArtemisSDR's DX layout exactly, only `[0]` differs. Example, a typical
request/ack pair from the boot sequence:

```
H->R  03 ff 1a 00 04 00 00 00 00 00 01 00 00 00 00 00 00 00  payload=00000000
R->H  03 ff 1a 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00  payload=(none)
```

The IQ stream (port 50002) is dominated by 1210-byte packets (20,751
of 21,626 total) — 10-byte header + 1200-byte I/Q payload, confirming
the documented layout down to the magic byte:

```
03 ff fe ff b0 04 c4 e3 01 00 | <1200 bytes I/Q>
```

`[2]=0xFE` matches the documented RX-idle opcode. Two more frame
shapes share port 50002, both new observations beyond the DX write-up
below: **865 packets of 77 bytes**, radio→host, opcode `0x00`/`b3=0x1f`
— a periodic telemetry/meter-looking frame (floating-point-shaped
payload), not yet attributed — and a handful of **10-byte host→radio
frames** using the same IQ header with zero payload: a stream
keepalive distinct from the documented `0xFE` idle-data frames.

### New finding: the QRP's real opcode set is richer than ArtemisSDR's documented DX subset

Confirmed present, matching the DX table above: `0x01`, `0x05`, `0x06`,
`0x07`, `0x08`, `0x10`, `0x15`, `0x17`, `0x18`, `0x1a`.

Also present in the same 92-packet boot sequence, **not in ArtemisSDR's
DX opcode table at all**:

| Opcode | Direction | decl_len | Example payload | Notes |
|---|---|---|---|---|
| `0x03` | H→R | 4 | `01000000` | |
| `0x04` | H→R | 4 | `00000000` | |
| `0x0c` | H→R | 0 | — | |
| `0x0d` | H→R | 0 | — | |
| `0x0f` | H→R | 4 | `00000000` | |
| `0x11` | H→R | 4 | `fe000000` | |
| `0x12` | H→R then R→H | 0 then 20 | reply: `0300070000004119000041c27c0001000100` | reply reuses the `4119…7c00` byte pattern seen in the boot-announce frame below — possibly identity/capability-shaped, not attributed |
| `0x13` | H→R | 4 | `01000000` | |
| `0x16` | H→R | 36 | `010000000100000000000000010000006400…` | |
| `0x1c` | H→R | 16 | `13370c041449040114ae47013b4f5200` | |

None of these are guessed at further here — that would be exactly the
kind of inference this document's own gate exists to forbid on a
transmit-adjacent protocol. Attributing them safely needs a follow-up
capture that isolates one ExpertSDR2 action at a time (preamp toggle,
antenna change, frequency change, PTT) and diffs which opcode fires —
future work, not tonight's.

### Unresolved: the one-off "board announce" frame, and two `0x33`-magic outliers

The very first packet in the capture, radio→host, sent once
immediately after connection:

```
03 ff 01 1a 7c 00 00 00 41 19 c0 a8 10 c8 c0 a8 10 c8 51 c3 00 00 49 28
```

24 bytes total. Bytes `[10:14]` and `[14:18]` are both `c0 a8 10 c8` =
`192.168.16.200` — the QRP's own IP, appearing twice back to back. Read
against the general header, `[4:5]` decodes as a declared length of 124
that doesn't match the packet's actual 6 trailing bytes — this frame
does **not** use the general envelope at all; it only happens to share
its first three bytes (magic / `0xFF` / opcode `0x01`) with it. This is
**not** the documented 68-byte DX STATE_SYNC template — no packet of
that size or shape appears anywhere in the capture. Whether the QRP has
a STATE_SYNC-equivalent, and what it looks like, is open.

Two more packets, ~21 seconds apart, both host→radio, both identical:

```
33 ff 05 00 00 00 00 00 00 00 00 00 5f 1d 9b 9c 00 ff 00 00 00 ff 00 00 00 …
```

1218 bytes total — magic byte `0x33` (not `0x03`), opcode-position
byte `0x05` (the DX PREAMP/START_IQ slot), far larger than any `0x05`
payload the DX table documents. After a 16-byte-ish prefix, the rest is
a regular `ff 00 00 00` repeat. Both occurrences land early in the
boot sequence, before steady IQ streaming begins, so a receiver-init or
buffer-clear command is plausible — but that is a guess, stated as one,
not a finding. Flagged unresolved rather than papered over.

### What this changes for the next step

- `tools/sunsdr_probe.cpp` tried only the DX (`0x32`) and PRO (`0x01`)
  magic bytes — against this QRP it could never get a reply as written,
  which is now understood, not mysterious. A third profile (`QRP`:
  ctrl 50001 / stream 50002 / magic `0x03`) has been added to the
  tool's profile table — mechanical, additive, still read-only,
  same zero-payload `0x1A` query as before, plus a second QRP-only
  query (`0x12`, also confirmed empty-payload in the real capture,
  also got a real reply there — see the opcode table above). One
  caveat already visible in the capture: the real boot sequence's own
  `0x1a` exchange carried a 4-byte zero payload, not the empty payload
  this probe sends (which mirrors ArtemisSDR's own identity-query
  usage) — if the corrected magic byte still gets no reply, that
  mismatch is one plausible reason to check next, not a settled
  explanation.
  **Run several times on the bench tonight (2026-08-25) with the QRP
  profile in place — no reply on any profile, any time.** At least one
  of those runs happened while the QRP was confirmed genuinely
  reachable and in active use — Longpath's existing TCI client was
  connected through ExpertSDR2 at the time, with live audio and
  panadapter both confirmed working — so this specific run is a real,
  if still narrow, negative result: a live, reachable, actively-used
  QRP did not answer either the `0x1A` or `0x12` empty-payload query on
  the corrected magic byte. Doesn't settle the question (ExpertSDR2
  holding the control channel exclusively is still on the table, and
  so is the payload-shape caveat above), but it does rule out "the
  device just wasn't there."

  Checked ArtemisSDR's own source for how it handles a radio already
  claimed by another client (an `ExpertSDR2` instance, in our case):
  no such handling exists — `grep`-ing `sunsdr.c` for anything
  session/exclusivity-shaped (busy, in-use, already-connected, etc.)
  finds nothing. Silence, not a confirmed all-clear: ArtemisSDR is
  built as a *replacement* for ExpertSDR2, so its author most likely
  never ran the two side by side to hit this case either. **The
  cleanest next bench test isolates this variable directly: run
  `sunsdr_probe` with ExpertSDR2 fully quit (not just idle/backgrounded)
  while the QRP is powered and reachable.** If it replies with
  ExpertSDR2 closed but not while ExpertSDR2 holds the session, that
  confirms exclusive-hold as the explanation for tonight's null result
  and rules out a QRP-specific protocol difference in the same step.
- The confirmed header formats and magic byte are enough to build and
  test further **read-only** probes. They are **not** enough to safely
  open a live session: several of the ~20 opcodes in the real boot
  sequence have no attributed meaning yet, and replaying that sequence
  to open a session would mean sending commands of unknown effect —
  exactly what the gate above exists to prevent. **No session/RX/TX
  code should be written from tonight's findings alone.**
- Concretely still needed first: one or more targeted follow-up
  captures, each isolating a single ExpertSDR2 action, to attribute the
  unknown opcodes above one at a time.

Raw evidence (all 92 decoded control-channel packets, the IQ-header
histogram, and the parsing scripts used) lives in this session's
scratch analysis. The pcap itself is at `/tmp/sunsdr-capture.pcap` on
the operator's bench Mac — not part of the repo.

## Protocol reference (ArtemisSDR `sunsdr.c` / `sunsdr.h`, DX profile)

Everything in this section is cited to ArtemisSDR source, read in full
(`sunsdr.c`, 5024 lines; `sunsdr.h`, 302 lines) — not sampled, not
paraphrased from the project's own README. `grep -ni "qrp"` over both
files: zero matches. There is no QRP-specific code anywhere in ArtemisSDR
to read even if we wanted to — this whole section is the DX/PRO behaviour.
Ports, magic byte, and both header formats are now confirmed for the QRP
too (see "Confirmed from real QRP capture" above); the per-opcode meanings
below remain DX-sourced and only partially cross-checked — read each
opcode row against that section before trusting it on the QRP.

### Sockets

Two connectionless UDP sockets, each bound locally, addressed by explicit
`sendto()` rather than `connect()` (`sunsdr.c:2982-3031`). **Ports and the
protocol magic byte are per-model, not fixed** — this is the first thing
a native Longpath implementation must not hardcode:

| Model | Control port | Stream port | Magic byte `[0]` |
|---|---|---|---|
| SunSDR2 DX | 50001 | 50002 | `0x32` |
| SunSDR2 PRO | 50002 | 50003 | `0x01` |
| SunSDR2 QRP | 50001 **(confirmed)** | 50002 **(confirmed)** | `0x03` **(confirmed)** |

(`sunsdr.c:2728-2742`, the `sunsdr_profile_dx` / `sunsdr_profile_pro`
tables) — the QRP row is now confirmed from a real bench capture, not a
placeholder; see "Confirmed from real QRP capture" above for the
evidence and the parts of the protocol still open.

No broadcast/discovery packet exists in this protocol; the radio's IP is
supplied out-of-band (matches SunSDR's own TCI client, which likewise
needs a manual `host:port` — see today's `MainWindow_SunSdr.cpp`). Control
replies are awaited with a 200 ms `select()` timeout per send
(`sunsdr.c:2110-2141`); the stream socket's receive timeout is 500 ms
(`sunsdr.c:4211-4214`).

### Control channel — 18-byte header

```
[0]      magic0 (0x32 DX / 0x01 PRO)
[1]      0xFF
[2]      opcode
[3]      0x00
[4:5]    declared payload length, u16 LE
[6:7]    sub-index / sub-opcode, u16 LE
[8:9]    0x00
[10]     0x01
[11:17]  zero padding
[18:]    payload (length = [4:5])
```
(`sunsdr_build_header`, `sunsdr.c:2242-2256`) Total packet length = 18 +
declared payload length.

Opcodes actually used (every one of these needs QRP wire confirmation
before being trusted on a TX path):

| Opcode | Meaning | Payload | Cite |
|---|---|---|---|
| `0x01` | STATE_SYNC | 68-byte template; DX patches byte 0x36 = active RX count | `sunsdr.c:2415-2461` |
| `0x02` | POWER_OFF | u32 = 0, sent while stream still live | `sunsdr.c:3336-3344` |
| `0x05` | preamp/atten (alias START_IQ) | u32, low byte `0x80\|state` (0..3 → −20/−10/0/+10 dB) | `sunsdr.h:33-47`, `sunsdr.c:3562-3579` |
| `0x06` | MOX/PTT | u32, 1=TX / 0=RX | `sunsdr.c:3987,4075` |
| `0x07` | INFO_QUERY | 44-byte fixed hex | `sunsdr.c:2592` |
| `0x08` | freq, DDC companion | u64 LE × 10 Hz, sub 0=RX1 / 1=RX2 | `sunsdr.c:2381,2386,2656` |
| `0x09` | freq, primary/TX VFO | u64 LE × 10 Hz | `sunsdr.c:2341,2380,2651` |
| `0x0E` | bootstrap primer | zero payload | `sunsdr.c:2668` |
| `0x15` | RX/TX antenna select | u32 selector — RX A3 wire `0x03`, TX A3 wire `0x02` (differs!) | `sunsdr.c:2279-2286`, `HPSDR/SunSdrAntenna.cs:81-97` |
| `0x17` | **drive byte, not mode** | u32, low byte = pre-calibrated 0-255 passthrough | `sunsdr.c:3660-3699` (see "TX drive" below) |
| `0x18` | keepalive | u32 = 0 | dozens of call sites |
| `0x1A` | firmware/identity query | reply carries version at bytes 22/24 | `sunsdr.c:4961-5008` |
| `0x1B` | RX2 enable | u32, 0/1 | `sunsdr.c:2625` |
| `0x1E` | antenna preamble | u32 | `sunsdr.c:3737-3749` |
| `0x20` | CONFIG_BLOCK (carries **mode**) | 70-byte template, 3 variants (wideband/NFM/WFM) | `sunsdr.c:2463-2524` |
| `0x21` | mic source | u32, 0=Mic1/1=Mic2 | `sunsdr.c:3599-3616` |
| `0x22` | STREAM_XPORT (band-dependent LO param) | 30-byte, HF/VHF-2m templates only | `sunsdr.c:2481-2492` |
| `0x24` | PA enable | u32 = `(paEnabled && ptt) ? 1 : 0` | `sunsdr.c:2579-2582` |
| `0x27` | EXT_CTRL | 34-byte fixed hex, never field-decoded | `sunsdr.c:2602,2708` |
| `0x5A` | "I'm alive" | zero payload | `sunsdr.c:2635-2643` |
| `0x5F` | power wake | 24-byte hex, ×3 in boot macro — **DX only, absent from PRO** | `sunsdr.c:2617-2622` |

Note `0x17` — an earlier ArtemisSDR revision read this as a mode opcode
and got AM stuck at 2 W and SSB running at "mode-code-as-drive" power
levels before the bug was found (`sunsdr.c:3660-3679` comment). **Mode
lives inside the `0x20` payload**, byte offset inside it explicitly not
yet pinned even by ArtemisSDR (§7 of the glue-layer research below).

### IQ stream — 10-byte header, 1200-byte payload, 1210 bytes total

```
[0]   magic0
[1]   0xFF
[2]   opcode (0xFE = RX-state/idle-TX, 0xFD = TX-active)
[3]   0xFF
[4:5] payload size u16 LE (1200)
[6:7] sequence u16 LE
[8:9] state bytes
[10:] 200 × 6-byte I/Q samples, Q first, 24-bit signed LE each
```
(`sunsdr_build_iq_header`, `sunsdr.c:1678-1691`; sample unpack,
`sunsdr.c:4653-4668`, confirms Q-in-low-3-bytes / I-in-high-3-bytes per
slot — ArtemisSDR's own comment warns that swapping this mirrors the
sideband on air, `sunsdr.c:1711-1720`.) Native sample rate: **312,500 Hz**
fixed (`sunsdr.c:2436-2461`).

TX packet sequence numbers reset to 0 on every PTT-on — carrying them
monotonically across sessions makes the radio silently drop packets as
out-of-order, producing a keyed, unmodulated carrier (`sunsdr.c:3857` and
comment). TX packets are paced on a dedicated high-resolution timer thread
at **exactly 5.12 ms per packet** (195.3125 pps); an empty pacing ring
repeats the last packet rather than emitting silence, to avoid an audible
gap (`sunsdr.c:388-557`, `TX_PACE_TICK_100NS`). During RX idle, the host
must keep sending silent `0xFE` packets at the same ~195 pps or the radio
disconnects the stream after roughly 8 seconds — this is the wire-level
"keepalive" the TCI client doesn't need but this protocol does.

### Session lifecycle

Full sequences (power-on macro, band-change prelude, PTT-on/-off ordering,
power-off ordering) are documented exactly, opcode-by-opcode with
per-step delays, in the research transcript this design doc is drawn
from — see "Where the detail lives" below rather than duplicating a
~30-step macro here. The two load-bearing facts worth stating directly:

- **Power-off sends `0x02` first, while the stream is still live**, then
  tears down local threads — ArtemisSDR's own history records that
  reversing this order left the radio in a state that corrupted the next
  power-on (`sunsdr.c:3336-3343`).
- **A cold-start race exists in the reference implementation** between
  the stream starting and WDSP being ready to receive — ArtemisSDR gates
  this with an explicit `rxWdspReady` flag the read thread checks before
  dispatching any packet, real or synthetic (`sunsdr.c` + `console.cs`
  `SetChannelState` → `FlushChannelNow` → `nativeSunSDRSetRxWdspReady(1)`
  sequence). Longpath's own `RadioModel` already has an equivalent
  "don't touch a slice before it's real" pattern from tonight's SunSDR-TCI
  work (`suppressAutoStreamBinding`) — the same discipline, a different
  race.

### TX drive / power scaling

Two independent actuators, confirmed from the code, **not derived from a
formula inside the protocol layer**:

1. **Drive byte (`0x17`)** — a pure 0-255 passthrough
   (`sunsdr_drive_raw_to_wire_byte`, `sunsdr.c:3680-3699`). The
   calibrated integer is computed entirely on the C# side: target dBm →
   per-band gain table (`GetSunSDRDefaultAdjust`, see below) → target
   volts → `round(255 × f × swr_protect)`. **A Longpath port needs to
   replicate that upstream calibration chain itself** — the wire layer
   has no formula to copy.
2. **Wire IQ amplitude (`iq_gain`)** — a small fixed set of gain
   constants (~1.167 for TUNE, 1.0 for voice MOX, ~1.577 on VHF), *not* a
   function of requested watts, plus a soft-knee limiter above 0.97
   (`sunsdr.c:1609-1653`, `1905-1918`).

`GetSunSDRDefaultAdjust` (ArtemisSDR `setup.cs:23752-23835`) is a 9-entry
dB offset table indexed by UI drive value in 10 W steps (10 W..90 W),
linearly interpolated between anchors, derived from a **40 m sweep only**
and used as the fallback for every other band pending separate
measurement. Locked 40 m curve (ArtemisSDR `TECHNICAL.md`):

| UI W | Actual W | Note |
|---:|---:|---|
| 5 | 4 | hardware low-drive dead-zone |
| 10 | 9.8 | |
| 30–80 | exact | |
| 90 | 91 | |
| 100 | 97 | radio PA ceiling |

This table is SunSDR2-DX-specific, on ArtemisSDR's own hardware, on 40 m
only. **It is very likely wrong for a QRP** (lower-power PA stage,
plausibly a different ceiling and dead-zone) — this is exactly the kind
of number that must come from a real QRP bench sweep, not be copied.

### Antenna model

Three fixed physical ports (`HPSDR/SunSdrAntenna.cs`, 140 lines): `A1`
(2 m VHF only), `A2`/`A3` (HF only, mutually exclusive with A1 by band).
Materially narrower than the OpenHPSDR/Alex antenna matrix Longpath
already models for the ANAN/HL2 family — no RX-only/XVTR checkbox grid,
no step attenuator. One shared RX/TX selector opcode (`0x15`) with a byte
value that differs between RX and TX for the same physical antenna A3
(`0x03` vs `0x02`) — an easy transcription bug if not carried over
carefully.

### What ArtemisSDR itself doesn't have

Worth carrying into any Longpath capability-gating decision: PS-A
pre-distortion, the Alex step-attenuator, full duplex (RX2 while
transmitting — "MOX on SunSDR shuts down the RX LO"), and diversity RX are
all explicitly unsupported on the SunSDR family in ArtemisSDR, not merely
unimplemented — `ApplySunSDRSpecificUI()` (`console.cs:6657-6757`)
actively hides those controls rather than leaving them silently
non-functional. A Longpath `BoardCapabilities` row for SunSDR should do
the same: gate these off explicitly, the way `SkuUiProfile` already gates
HL2/Atlas antenna UI.

## Architecture mapping onto Longpath

Longpath already has the right shape for this — a `RadioConnection`
subclass per wire protocol (`P1RadioConnection`, `P2RadioConnection`).
A native SunSDR driver is a third: `SunSdrRadioConnection`, implementing
the same interface, NOT a variant of today's `TciClient` (that class stays
exactly what it is — a client of ExpertSDR2's companion protocol for
people who don't want the native path, or whose radio isn't validated for
it yet).

Rough shape, deferred until the probe step confirms QRP specifics:

- **Control**: a thin C++ port of `sunsdr.c`'s opcode table — this is
  mechanical once the QRP profile fields (port, magic byte, boot macro)
  are confirmed.
- **RX IQ**: feeds the same `FFTEngine`/`RxChannel` pipeline every other
  board uses — no parallel path, same principle as today's TCI panadapter
  wiring.
- **TX IQ pacing**: Longpath doesn't have an equivalent hard-real-time
  UDP pacing thread today (P1/P2 pace differently) — this is new
  infrastructure, not a reuse of existing TX plumbing, and needs its own
  design pass once RX is proven.
- **The safety invariant from today's SunSDR/KiwiSDR work does not apply
  here** — that invariant existed because SunSDR-via-TCI is a *foreign
  accessory* that must never touch a slice a real radio owns. A native
  SunSDR2 QRP driver isn't an accessory sharing a slice with something
  else — it *is* the real radio for that connection, the same standing
  as an ANAN. `RadioModel::connectToRadio()`'s normal path applies
  directly; no `suppressAutoStreamBinding` involved.

## Phasing

Matches the pattern already proven twice today (TCI client: Verbindung →
Ton → Bild → Steuerung; KiwiSDR: same shape) — receive fully proven before
transmit is even attempted, each step bench-verified before the next:

1. **Probe tool + QRP wire confirmation** (blocking gate, described
   above). Not receive, not transmit — just proving what's actually true.
2. **Connect + RX IQ + panadapter**, receive-only, no PTT/drive code
   written at all yet. Bench-verified against real QRP hardware, same
   verification-matrix discipline as `2026-08-24-sunsdr-verification/`.
3. **RX audio**, opportunistic into the existing mixer, same shape as
   today's TCI audio path.
4. **VFO/mode control**, RX-side only — confirms the control channel
   round-trips correctly before any TX opcode is ever sent.
5. **TX** — its own design pass, its own bench matrix, gated on steps 1-4
   being solid and on a real QRP TX power sweep replacing ArtemisSDR's
   40 m-only DX table. Not attempted in the same pass as RX.

## Where the detail lives

This document is the synthesis. The full opcode-by-opcode wire dump (all
~30 boot-macro steps with per-step delays, the complete threading/locking
table, the exact TUNE-frequency-offset state machine, the full
`ApplySunSDRSpecificUI` capability list) came from a full, uninterrupted
read of ArtemisSDR's `sunsdr.c` (5024 lines), `sunsdr.h` (302 lines),
`NetworkIO.cs`, `NetworkIOImports.cs`, and the SunSDR-touching sections of
`console.cs`/`cmaster.cs`/`setup.cs`/`clsHardwareSpecific.cs`/`enums.cs`/
`HPSDR/SunSdrAntenna.cs` — every fact cited to an exact file:line. That
transcript is long (two ~30 KB reports); re-derive it by re-reading the
same ArtemisSDR files rather than trusting a paraphrase, the same standard
this project holds Thetis ports to.

## What a future reader should NOT need to re-derive

- ArtemisSDR is GPLv2-or-later, Kosta Kanchev (K0KOZ) — porting actual
  code (not just citing facts) from it triggers the same
  License-preservation rule as a Thetis port (`CLAUDE.md`): copyright
  line, GPL permission block, and a PROVENANCE-equivalent record, in the
  same commit as the port.
- The QRP-unconfirmed status was not a formality to skip past — it is
  why this document stopped at "reference," not "implementation plan
  with a start date." Ports, magic byte, and both header formats are
  now confirmed (see "Confirmed from real QRP capture" above); the
  opcode-by-opcode meanings are still DX-sourced and only partially
  cross-checked. Do not treat the whole protocol as QRP-confirmed just
  because the header framing is — re-read that section's "richer opcode
  set" and "unresolved" tables before trusting any specific opcode.
- `0x17` is drive, not mode, and this was a real, shipped bug in the
  reference implementation before it was caught — a Longpath port
  re-deriving this from scratch would be reasonable to make the exact
  same mistake without this note.
- The real QRP capture behind the confirmed section is a live artifact
  on the operator's bench Mac (`/tmp/sunsdr-capture.pcap`, 21,720
  packets, not in the repo) plus a set of scratch parsing scripts
  (`parse_dump.py` over `tcpdump -x` text output — the PCAP-NG route was
  tried first and abandoned, see that section for why). A future
  targeted re-capture to attribute the unknown opcodes should reuse
  that parsing approach rather than re-solving the PKTAP problem.
