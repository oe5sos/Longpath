# SunSDR2 QRP — Native Driver (No

### Update, 2026-08-27 (same day) — CONFIRMED against a live, exact, known-frequency test

The operator ran the confirmation script (`tools/sunsdr_freq_confirm.py`, a read-only tcpdump-based observer built for exactly this test) against a real ExpertSDR2 session, deliberately tuning to one exact, watched frequency. Two rounds were needed to fix two real bugs in the script itself (an interface-selection guess that captured zero packets at all — fixed by capturing on every interface at once, `-i any`, matching this project's own earlier proven capture method — and a payload-offset-by-2 error that read into the header's own already-flagged non-zero tail instead of the true payload). Once fixed:

- ExpertSDR2 displayed **7,099,904 Hz** at the end of a VFO scroll to approximately 7.1 MHz.
- The script decoded the real captured `0x08` frame's payload (`a8403b0400000000`) via the candidate formula to **7,099,204 Hz**.
- Difference: 700 Hz out of ~7.1 MHz (0.01%) — consistent with the captured packet reflecting an intermediate position during a continuous scroll, one or two ticks before the display's final resting value, not a formula error. A second, independent capture earlier the same session (7,104,614 Hz decoded, uncompared against an exact display value at the time) landed in the same ~7.1 MHz neighborhood the operator was actually tuned to.

**Status upgraded from hypothesis to confirmed.** `SunSdrRadioConnection::setReceiverFrequency()` now sends this formula for real (design doc's own earlier caution against wiring an unconfirmed formula onto the wire no longer applies — this one is confirmed). One caveat remains open, unrelated to the payload formula: the 18-byte header's bytes 14-17 still carry a varying, not-fully-understood value in every real captured frame; the implementation reuses the exact prefix bytes from the one already-confirmed-accepted frame rather than guessing new ones. If retuning proves unreliable under sustained real-world use, that header tail — not the payload encoding — is the next thing to investigate. ExpertSDR2), Design + Protocol Reference

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

### New finding, 2026-08-25 (later bench session): the network path itself may need an active companion session

A different, more basic failure mode showed up this time, upstream of
the protocol question entirely: plain ICMP `ping` to the QRP alternated
between "No route to host" and silent timeout across the session, with
no code change or reachability fix on either end explaining the
flips. OE5SOS noticed the pattern directly: *"das passiert dann, wenn
die Software SunSDR nicht läuft"* (this happens specifically when the
[ExpertSDR2] software isn't running). Tested it directly, twice:

1. QRP powered off, `sunsdr_probe` run — no reply on any profile (the
   pre-existing checklist item; not surprising on its own).
2. QRP powered on, ExpertSDR2 confirmed **not** running (process check
   empty, not just window-closed) — `ping` still "No route to host" or
   timeout, `sunsdr_probe` still no reply on any profile, including the
   corrected QRP magic byte and the `0x12` query that got a real reply
   in the passive capture.

Both times, powering the QRP on by itself was **not** sufficient for
the Mac to reach it at the network layer at all — not a protocol-level
non-reply, a routing-level non-reachability, the same failure ICMP
itself hits. The one config not yet tested tonight is QRP-on
+ ExpertSDR2 actively connected + `sunsdr_probe` run alongside it,
which OE5SOS declined for tonight since running ExpertSDR2 at all, even
briefly as a diagnostic, cuts against the point of this whole document
(*"das will ich ja umgehen, dass ich nur eine Software benötige"* —
avoiding needing more than one piece of software is the goal).

**Working hypothesis, clearly labelled as one:** the QRP's network
interface may only fully come up, or only answer non-broadcast/unicast
traffic, once an active ExpertSDR2 session has spoken to it —
matching the *"beacon"* first packet documented above (the QRP
announcing its own IP on connection) and the `0x18` keepalive opcode
already in the reference table. If true, a native `SunSdrRadioConnection`
would need to replicate whatever wakes this path — not just send the
right bytes once reachable, but establish reachability in the first
place. **Not confirmed** — the alternative explanation (WLAN/AP-side
instability unrelated to ExpertSDR2, the same class of problem hit
independently with the Anvelina on the same WLAN tonight) hasn't been
ruled out, and both QRP and Anvelina share the same access point.
Confirming this needs exactly the one untested config above, on a
future bench session when running ExpertSDR2 briefly as a pure
diagnostic is acceptable.

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

### New finding, 2026-08-26: the one untested config finally ran — exclusive-hold confirmed, reachability question still open

The config flagged above as untested (QRP-on + ExpertSDR2 actively
connected + `sunsdr_probe` run alongside it) happened to become
available mid-session: ExpertSDR2 was already running as a live
diagnostic OE5SOS was using for an unrelated Longpath bug (waterfall/
line-width rendering), so `sunsdr_probe 192.168.16.200 5` was run
against it opportunistically rather than as a dedicated bench step —
no extra ExpertSDR2 launch was needed, so it did not cut against the
"avoid needing a second piece of software" goal the way OE5SOS
declined it for on 2026-08-25.

Result: `ping 192.168.16.200` replied normally (sub-ms RTT, 0% loss) —
the network path **was** reachable this time, unlike the two prior
tests with ExpertSDR2 not running. But `sunsdr_probe` got **no reply
on any of the three profiles** (DX, PRO, QRP), including the corrected
QRP magic byte and the `0x12` query. `sunsdr_probe` sends read-only
self-identification queries only (no power-on, no frequency, no PTT,
no antenna — see its own header comment), so this was safe to run
without operator supervision.

This confirms the exclusive-hold hypothesis for the **protocol** layer:
with a live ExpertSDR2 session active and the network path reachable,
the QRP still refuses to answer a second client's query. It does
**not** by itself confirm or rule out the separate **reachability**
hypothesis (2026-08-25 finding, above) — this test ran with a session
already active, so it says nothing about whether the network path
would have come up on its own. The two prior "ExpertSDR2 not running"
tests (QRP powered on, `ping` still "No route to host"/timeout) remain
the only evidence on that question, and they still stand: **without
any active companion session, the QRP was not reachable at the network
layer at all, twice.**

Taken together, this now looks like two separate gates, not one:
1. **Reachability** — the QRP's network interface may not come up
   without something (ExpertSDR2, so far the only thing tested) having
   spoken to it first. Still labelled a working hypothesis, not
   confirmed — the WLAN/AP-instability alternative from 2026-08-25 is
   still on the table.
2. **Protocol exclusivity** — even once reachable, the QRP does not
   answer a second client while one session already holds it. Now
   confirmed by today's test.

If gate 1 is real, a native `SunSdrRadioConnection` opening the
**first and only** session (ExpertSDR2 never launched at all) is a
different situation from what was tested today — gate 2 wouldn't
apply, since there'd be no competing session. Whether gate 1 itself
would block that first-session case is still the open question, and
answering it needs a capture of whatever ExpertSDR2 sends in the first
few packets after a cold launch (before it even reaches a usable UI
state) — the diagnostic C.1 already calls for, not a new one.

### New finding, 2026-08-26 (cold-launch capture, C.1): 83 clean control packets, and the "boot macro" framing may be the wrong model

OE5SOS ran the exact C.1 diagnostic tonight: QRP power-cycled, ExpertSDR2
fully quit first, then relaunched fresh with `tcpdump -i any -w
/tmp/sunsdr-capture.pcap 'host 192.168.16.200'` running throughout. The
capture (117733 packets total, mostly IQ stream) was parsed with a
small local script (`/tmp/parse_sunsdr.py` — reads the pcap-ng blocks
directly, no external deps) filtered to `udp port 50001`: **83 control
packets**, decoded to opcode + payload and written to
`/tmp/sunsdr-decoded.txt`. Both files are scratch, not part of the
repo — same standing as the earlier 92-packet capture.

**ArtemisSDR is now cloned locally** at `../ArtemisSDR` (relative to
Longpath root, commit `f8b01d2`, "release: v2.1.9 — PRO native 312.5 kHz
done properly, credit Jeff N0GQ (issue #47)") instead of cited via
WebFetch — makes future opcode cross-referencing a `grep`, not a
fetch. Confirmed consistent with this document's earlier citations
before trusting anything from it: the DX boot macro at `sunsdr.c:2664`
is exactly 3 bootstrap + 29 labeled steps (32 total, matching the
already-cited count), magic byte `0x32` for DX matches, and the IQ
frame constants (`SUNSDR_IQ_PKT_SIZE 1210` / `_HDR_SIZE 10` /
`_PAYLOAD_SIZE 1200`) match the QRP capture's own confirmed values
from the first pass. Treated as the same trustworthy reference this
document has cited all along, not independently re-vetted beyond that
consistency check.

**The ten previously-unattributed opcodes are still unattributed.**
`grep -n` for `0x03`, `0x04`, `0x0c`, `0x0d`, `0x0f`, `0x11`, `0x13`,
`0x16`, `0x1c` as `SUNSDR_OP_*` definitions across both `sunsdr.c` and
`sunsdr.h` — none exist. `0x12` gets one unrelated hit (a byte-offset
literal, not an opcode). This is a real negative result, now checked
against the actual source rather than assumed: ArtemisSDR's own
DX/PRO reference genuinely does not cover these opcodes. Reinforces
rather than resolves the 2026-08-25 finding.

**Structural read of the new capture, independent of opcode naming:**
the sequence does not look like the DX "power-on from cold" macro
shape at all. It opens with a single 24-byte QRP→Mac beacon (matching
the "beacon" pattern already documented), then a large ~1.2 KB
Mac→QRP packet sent twice, then a burst of ~15 small query packets —
`0x03, 0x04, 0x17, 0x11, 0x0f, 0x1a, 0x13, 0x15, 0x0c` and others —
each Mac→QRP query immediately answered by a QRP→Mac reply carrying
the *same* opcode, all within about a 1 ms window. That shape (many
distinct "give me your current value of X" query/reply pairs, no
`Sleep()`-paced sequencing) reads as **ExpertSDR2 syncing its own UI
to an already-running radio's current state**, not as a power-on
sequence bringing up RF/DSP hardware from scratch — a materially
different thing to replicate than "port the DX boot macro." If
correct, a native `SunSdrRadioConnection` opening its first session
against an already-powered QRP may need to replicate *this*
query-and-sync shape rather than anything resembling the DX macro.
**Not confirmed** — needs the still-outstanding C.1 sub-task (isolated
single-action captures) to distinguish "this is just how ExpertSDR2
syncs on connect" from "some of this is genuinely necessary to wake
the radio."

**Safety-relevant tangent, surfaced by the ArtemisSDR cross-reference,
not by anything in the new QRP capture:** `sunsdr.h` names opcode
`0x17` `SUNSDR_OP_DRIVE`, with its own correcting comment —
*"0x17 was MISIDENTIFIED as MODE. Actual semantics per AM drive
calibration captures 2026-04-14: 0x17 payload byte sets radio TX
drive level. byte = round(sqrt(watts/100) * 255)."* This explains,
after the fact, why this document already flagged opcode `0x17` for
caution before knowing why. It does **not** change the standing
caution — this is ArtemisSDR's own DX/PRO finding, unverified against
the QRP, and the QRP capture's own `0x17` traffic tonight was a
query/reply pair (asking the radio's current value), not a write — so
nothing in tonight's evidence contradicts or confirms the watts-to-byte
formula either way. Flagging it here specifically so it is not lost:
**opcode `0x17` must never be written from Longpath without a QRP-
specific bench confirmation of what it actually controls on this
radio**, formula or no formula.

### New finding, 2026-08-26 (isolated-action capture attempt #1, "PA" button): confounded by an unrelated accidental reconnect

First of the C.1 isolated-action captures: OE5SOS pressed ExpertSDR2's
"PA" control (`/tmp/sunsdr-action-preamp.pcap`, 110 control packets,
same parser). About 15 seconds into the capture a fresh 24-byte beacon
(opcode `0x01`, QRP→Mac) appears and the *entire* connect-sync burst
re-runs nearly identically to the cold-launch capture above (same
opcode sequence, same query/reply shape, ~90 packets in under a
second). **OE5SOS confirmed this reconnect was an unrelated accidental
click on his end, not caused by the PA control** — so this capture is
confounded and does not isolate whatever the PA control itself sends.
The genuinely PA-adjacent traffic, if any, is in the small quiet-start
window (`#000`-`#007`, first ~154 ms): opcodes `0x06`, `0x02`, `0x16`,
`0x08`, all query/reply pairs — but these same four opcodes also
appear as routine periodic polling in every other capture in this
document, so nothing here can be confidently attributed to the PA
press specifically rather than to background polling that would have
fired regardless. This capture is kept for the two findings below, not
as evidence about the PA control.

One concrete, useful difference did turn up: the recurring `0x33`-magic
1218-byte packet (opcode `0x05`, magic `0x33` — not the QRP's usual
`0x03`) carries **different payload content** than in the cold-launch
capture — this time a repeating 4-byte pattern (`2d2d2dff`, `343434ff`,
`6f6f6fff`, `6c6c6cff`, `8f8f8fff`, ...) shaped exactly like an RGBA
grayscale gradient table, not the pointer-shaped garbage seen before.
Whatever this packet is, its *content* varies run to run while its
*shape* (magic `0x33`, always sent twice) stays fixed — consistent
with a client-side UI/display resource (e.g. a waterfall palette) sent
as part of the sync burst, not a hardware parameter. Also concretely
useful: opcode `0x05` here is clearly **not** ArtemisSDR's DX-profile
`SUNSDR_OP_PREAMP_ATT` — that's a 22-byte packet in the DX macro
(`sunsdr.c`), not a 1218-byte one — a direct, capture-verified example
of the "same opcode number, different meaning per profile" risk this
document already carries as a general caution.

Next isolated-action capture should try something with less apparent
side-effect surface (frequency nudge, AGC toggle) to get a cleaner
single-opcode delta than this one produced.

### New finding, 2026-08-26 (isolated-action capture attempt #2, VFO tuning): opcode `0x08` confirmed clean — first fully unconfounded C.1 result

Second attempt, VFO knob turned in ExpertSDR2
(`/tmp/sunsdr-action-freq.pcap`, 216 control packets). Unlike attempt
#1, this one is completely clean: **every single one of the 216
packets is opcode `0x08`** — no other opcode appears anywhere in the
capture. This directly matches ArtemisSDR's `SUNSDR_OP_FREQ_COMP`
naming for `0x08` (`sunsdr.h`) with a real QRP capture, the first C.1
isolated-action result that lines up with the reference cleanly rather
than surfacing a QRP-specific divergence.

Each retune sends the same 26-byte query/18-byte-reply pair repeated
(each query itself sent twice back-to-back, matching the duplicate-
send pattern seen in the connect-sync bursts too), with an 8-byte
payload field that changes on every retune — e.g. `8ca31dd76ce07808`,
`7e52ebe884dc7808`, `f35cfab39cd87808`, stepping smoothly as the knob
turned. **Exact encoding not yet solved**: neither an 8-byte
little/big-endian double nor a straightforward 64-bit integer produced
a plausible frequency; splitting as two u32 LE words gives a
second word that steps by ~1000 per retune in the right direction
(down then up, matching the knob motion) but centered around 142
million — too high to be straight Hz for HF/VoIP work, so either a
non-Hz unit, a different byte split, or an offset/scale this session
didn't crack. Flagging as solvable with a more targeted follow-up
(tune to one exact, known frequency, capture a single packet) rather
than digging further from a free-spin capture. Not blocking — the
opcode attribution itself (the actual C.1 goal) is the clean part of
this result.

### New finding, 2026-08-26 (isolated-action capture attempt #3, band change 20m→40m): no distinct "switch filter for new band" opcode found — and 0x04/0x16 look like routine polling, not band-specific

Third attempt: 20m→40m band change in ExpertSDR2
(`/tmp/sunsdr-action-band.pcap`, 40 control packets — clean, short
capture, no confound this run). Two near-identical bursts appear 13
seconds apart, one for each band: `0x08` (frequency, several
duplicated sends) + one `0x07` (`INFO_QUERY`) + one `0x18`
(`KEEPALIVE`), all already-known opcodes from ArtemisSDR. **No opcode
appears that is unique to the band-change moment itself** — the two
bursts differ from each other only in the frequency value carried
inside the `0x08` payloads, not in which opcodes fire. If this radio
needs any RF-path reconfiguration (band-pass filter, antenna relay) on
a band crossing the way the ANAN/HL2 family's Alex board does, it is
not visible as a separate wire command here — either it rides
implicitly inside the frequency-set itself (radio-side auto-selection
by frequency), or it uses a mechanism this one capture didn't happen
to catch. **Not confirmed either way** — one before/after pair is not
enough to rule out a rare, only-sometimes-sent filter command.

Two of the ten originally-unattributed opcodes did show up —
`0x04` (once per band-burst, ~50-60 ms after the frequency/info/
keepalive group) and `0x16` (once, only in the first burst) — but
**their payloads are byte-for-byte identical across the 20m and 40m
bursts**, which argues against a band-specific meaning: a real
band-pass-filter selection would need to carry *some* band-dependent
value. Combined with `0x04` already appearing as routine background
traffic in the very first quiet-baseline window of capture attempt #1
(before any action was taken there), the working read is that `0x04`
(and plausibly `0x16`) are **periodic/keepalive-shaped, not triggered
by this specific action** — narrowing, not solving, the attribution
question at the time. Superseded by the finding immediately below.

### New finding, 2026-08-26 (isolated-action capture attempts #4-#6: mode, AGC, antenna — all silent; #7 preamp/atten — clean hit): `0x04` is very likely preamp/attenuator state

Three quick isolated-action attempts in a row produced **zero
control-channel packets each** (mode USB→LSB; AGC Slow→other; antenna
switch attempt, then confirmed moot — OE5SOS: only one antenna port
on this QRP). Not a capture failure — each ran a clean 25-38 second
window with normal IQ-stream traffic throughout, just nothing on port
50001. Read together with the earlier band-change and mode findings,
this now looks like a real pattern rather than noise: **RX-side DSP
settings (demodulation mode, AGC) appear to be entirely client-side**
in this protocol — the QRP streams raw I/Q continuously regardless,
and ExpertSDR2 does the demodulation/AGC itself, with no wire command
needed. If this holds, Longpath's existing WDSP-side mode/AGC handling
(already used for every other radio) needs no SunSDR-specific
counterpart at all — a smaller wire surface than originally assumed.
**Not proven from three data points** — plausible false negatives
exist (e.g. a control that only sends on a value *change* the radio
doesn't already have — untested here since we don't know the
pre-click state) — but consistent across three independent controls.

The 7th attempt, preamp/attenuator (`/tmp/sunsdr-action-preamp2.pcap`,
labelled `-20dB` dropdown next to `RX2` in ExpertSDR2's toolbar),
finally produced a clean 2-packet hit on **`0x04`** — and critically,
the payload *changed value* rather than repeating what earlier
captures already showed for `0x04`: request trailer `...01000000`
here vs. `...00000000` in every earlier `0x04` capture, a real bit
flip in the final byte, consistent with a small state index (matching
DX's `SUNSDR_OP_PREAMP_ATT` bit-pattern shape — a low-bit state index,
not a full dB value). This reframes the band-change finding above:
`0x04`'s payload was identical across the 20m/40m bursts not because
it's un-actionable, but because the attenuator state genuinely didn't
change between those two clicks — the opcode itself is real and
stateful. **Working attribution: `0x04` = preamp/attenuator state**,
not yet bench-confirmed against a second, independent toggle to rule
out coincidence.

Still fully unattributed after all seven attempts tonight: `0x03,
0x0c, 0x0d, 0x0f, 0x11, 0x13, 0x16, 0x1c`.

### Update, 2026-08-27 — `0x04` re-verified against the raw capture files and wired for real

The captures behind the `0x04` attribution above (`/tmp/sunsdr-action-preamp.pcap`, `/tmp/sunsdr-action-preamp2.pcap`) still existed on disk. Given the same evening's earlier byte-alignment bug in this document's own informal frequency-payload quotes (found and corrected the same day — see the frequency-encoding section below), the `0x04` payload quotes here were independently re-parsed from the raw pcaps rather than trusted a second time. They checked out: payload `00000000` (0dB) is 6/6 identical repeats in `preamp.pcap`; payload `01000000` (-20dB) is 2/2 identical repeats in `preamp2.pcap`. Both frames' full 22 bytes (18-byte header + 4-byte payload) are now known exactly:

- 0dB:   `03ff040004000000000001000000d804da1900000000`
- -20dB: `03ff040004000000000001000000bd6366a101000000`

Notably, the two frames' header tails (bytes 14-17: `d804da19` vs `bd6366a1`) differ from each other AND from the frequency frame's own tail (`8ca31dd7`) — direct confirmation this field is genuinely session-specific, not a fixed per-opcode constant, strengthening (not proving) the earlier "checksum-or-sequence-shaped" guess.

`SunSdrRadioConnection::setAttenuator(int dB)` now sends these two exact frames for `dB == 0` and `dB == -20` respectively — deliberately not interpolating to the nearest known value for any other request, since only these two states have ever actually been observed working. `setPreamp(bool)` is left untouched: the captured UI action was ExpertSDR2's own "-20dB" attenuator dropdown, not a separate preamp boost control, so there is no real evidence tying this opcode to preamp specifically. Not yet live-bench-verified through Longpath's own UI (only unit-tested for crash-safety so far) — that is the next step, same as frequency control was before its own live confirmation.

### Update, 2026-08-27 (workflow re-derivation) — all 8 remaining opcodes structurally characterized, two real breakthroughs

Autonomous re-parse of every still-existing capture file (`/tmp/sunsdr-*.pcap`, 13 files, all from 2026-08-26), using the byte-offset methodology this same day's earlier bug-hunts finally nailed down (strip 20-byte IPv4 + 8-byte UDP = 28 bytes, then the SunSDR frame's own 18-byte header, then the declared-length payload). Every extraction was cross-checked against the known-good `0x08` frame split before being trusted. Full per-opcode detail (exact bytes, file/timestamp provenance) lives in the workflow journal; this section is the synthesis.

**Breakthrough 1 — the header's "checksum-or-sequence-shaped" bytes 11-17 are very likely a checksum of (opcode, payload), not a session nonce.** Opcode `0x04`'s tail is `d804da19` every time its payload is `00000000`, and `bd6366a1` every time its payload is `01000000` — reproducibly, across four independent capture sessions hours apart. Every other opcode's tail is likewise constant across every session that captured it, and distinct from every other opcode's tail. This reframes months of "probably a sequence number, replay the exact bytes and hope" caution: if it's a checksum of (opcode, payload) rather than session state, a future session could plausibly reverse-engineer the actual checksum algorithm (a handful of same-opcode-different-payload samples would narrow it fast) and then compute valid headers for genuinely new commands, instead of being permanently limited to exact-byte replay of only what's already been captured. Not attempted this session — flagging it as the single most valuable next research thread if opcode work continues.

**Breakthrough 2 — `0x0c`/`0x0d` fully decoded, resolving an apparent contradiction in this document's own earlier notes.** Both notes were correct at once: the *request* (host→radio) is a genuine 0-byte-payload query for both opcodes; the *reply* (radio→host) is the 338-byte (18-byte header + 320-byte payload) blob described elsewhere. Byte-exact, reproducible across every session:
- `0x0c` reply: a 4-byte unexplained prefix, then 12 repeats of a 16-byte unit that is exactly two IEEE-754 doubles (`12.5`, `-2.4`), then two doubles ≈1.0, then 27 float32s ranging 0.34-1.1 with several exact 1.0s.
- `0x0d` reply: a 4-byte prefix, 192 zero bytes, two exact-1.0 doubles, then 27 float32s (25 exactly 1.0, 2 exactly 0.0).

Working read: two companion firmware-resident calibration/gain tables, read once at connect-sync, never observed to change and never triggered by any attributed user action — not live controls. `0x0d`'s near-all-1.0 shape reads as a default/unity curve (plausible with no antenna/PA load calibrated in); `0x0c`'s specific, non-trivial repeated pair (12.5, -2.4) reads as genuine per-band/per-stage firmware constants, not noise or a leaked-memory artifact (unlike the earlier `0x12` finding, `0x0c`/`0x0d` are radio-authored replies, not host-constructed queries, so there's no host process memory to leak in the first place).

### Update, 2026-08-27 (checksum reverse-engineering attempt) — negative result against the standard-algorithm space, one real lead ruled out

Follow-up to Breakthrough 1 above. Zero wire risk: pure offline analysis of already-captured bytes, no packets sent to hardware.

**Dataset.** Independently re-extracted (not trusted from prior prose) every host→radio `03 ff` control frame across all 13 remaining `/tmp/sunsdr-*.pcap` files, keyed by `(opcode, payload)`, keeping only entries whose header tail (bytes 14-17 of the 18-byte header — the exact 4-byte span the frequency-frame decomposition already pinned down, not the wider "bytes 11-17" first guessed at) was identical every time that `(opcode, payload)` pair recurred. 80 clean rows survived, spanning 18 distinct opcodes and, for opcode `0x08` (frequency), 34 same-opcode samples with only the payload varying — the best possible shape for isolating a checksum function from its input.

**Tested against the standard-algorithm space:**
- 5 general-purpose checksums (CRC-32 IEEE, CRC-32C, Adler-32, plain 32-bit sum, FNV-1a) × 5 byte-framings (payload alone; opcode+payload; the 14-byte header prefix+payload; magic+opcode+payload; length-prefixed payload) — no hits beyond the trivial opcode `0x00`/empty-payload/all-zero-tail case, which any of these algorithms satisfies by construction and carries no signal.
- The full 11-entry CRC-32 RevEng catalogue (ISO-HDLC, BZIP2, C, D, MPEG-2, POSIX, Q, JAMCRC, XFER, AUTOSAR, CD-ROM/EDC) × 7 framings, both byte orders — same negative result.
- A concrete, source-grounded lead: `../ArtemisSDR/Project Files/Source/Console/HPSDR/clsSunSDRDiscovery.cs:184-196` implements a real RFC-1071-style Internet checksum (16-bit-word sum with end-around carry, one's complement) for the SunSDR2 DX/PRO 24-byte discovery query's checksum field. Tested this exact algorithm (and its non-complemented raw-sum variant) against both halves of the control-frame tail, across 5 framings — no hits. The source's own comment flags this specific checksum as "not validated... radios accept zero checksum in practice," and it covers a structurally different 24-byte discovery packet family (DX/PRO only, never the QRP's `03 ff` control frames), so a miss here doesn't rule out the same firmware family using a different routine for this other packet type — it just isn't this one.

**Read on the negative result.** The tail values are well-diffused (adjacent frequency payloads 50 Hz apart, differing by one low-order byte, produce completely unrelated tails — see the sorted `0x08` table in the session's own working notes), which is consistent with a real checksum/hash rather than a simple additive or positional encoding, so the checksum hypothesis from Breakthrough 1 is not weakened by this result. What's ruled out is only the common, publicly-documented algorithm space; a proprietary or firmware-specific construction (custom polynomial, keyed hash, or a routine embedded in `SunSDR2dx.dll`/QRP firmware with no source access) remains fully consistent with the data and would need either a disassembly of that firmware or a leaked/documented spec neither this project nor ArtemisSDR has — out of reach for a source-first port.

**Disposition.** Not pursued further this session. The 80-row dataset is reusable if a future session gets a stronger lead (e.g. a firmware dump, or ArtemisSDR gains DX/PRO-side checksum coverage for this specific frame family upstream). Does not block anything already shipped — `setReceiverFrequency()` and `setAttenuator()` both work today via exact-byte replay of bench-confirmed frames, which never required computing this checksum in the first place.

**The other five, structurally solid but semantically still open:**

| Opcode | Payload (every occurrence, every session) | Fires | Notes |
| --- | --- | --- | --- |
| `0x03` | `01000000` (u32 LE = 1) | Once, connect/reconnect burst only | Confirmed byte-for-byte (this document's earlier informal quote was, this time, correctly aligned) |
| `0x0f` | `00000000` | Once, connect/reconnect burst only | Zero variance across 4 sessions — genuinely nothing to correlate against |
| `0x11` | `fe000000` (u32 LE = 254) | Once, connect/reconnect burst only | The `0xFE` byte numerically matches `kOpIqRxIdle`'s opcode value, but in a completely different field/port/cadence context — ruled out as coincidental, not a real cross-reference |
| `0x13` | `01000000` | **Twice**, connect/reconnect burst only | Same payload value as `0x03` but a distinct, non-overlapping header tail and different cadence (twice vs. once) — genuinely a different control that happens to share a payload value, not an alias |
| `0x16` | 36 bytes, fully decoded: nine u32 LE words `1,1,0,1,100,30,700,7,60` (this document's earlier quote was truncated; now complete) | Once per connection-open/reconnect, **not** tied to `0x18` KEEPALIVE's recurring cadence (checked directly — `0x18` recurs every ~13s within a single capture, `0x16` does not) | Supersedes the 2026-08-26 "periodic/keepalive-shaped" hypothesis — structurally a one-time capability-announce/query frame instead |
| `0x1c` | 16 bytes, `13370c041449040114ae47013b4f5200`, fully invariant including its own header tail (unlike every other opcode's tail, which varies with payload) | Once, connect/reconnect burst only | No IPv4/MAC/ASCII/float structure found in any window; no match against the ArtemisSDR source tree either — looks like a fixed boot-time template, no DX/PRO precedent |

**What this changes:** every opcode this project has ever captured on the QRP now has a fully characterized wire shape — no more "0-length" vs. "338-byte" contradictions, no more truncated quotes, no more unverified byte-alignment. What remains open is *semantic* attribution for six of the eight (`0x0c`/`0x0d`'s calibration-table hypothesis is fairly strong; `0x03`/`0x0f`/`0x11`/`0x13`/`0x16`/`0x1c` remain "known shape, unknown purpose"), and that genuinely does need new, targeted bench captures — none of these six ever fired during any of the isolated single-action captures (preamp, antenna, band, frequency, mode, noise-blanker, squelch), so no user-visible action has ever been observed to correlate with any of them changing. They may simply be one-time connection-parameters ExpertSDR2 always sends the same way, not live controls at all.

### New finding, 2026-08-26 (major): the "reachability" gate is a broadcast discovery packet, not a mystery "wake" mechanism — Gate 1 from the 2026-08-25 finding is now solved

Every capture tonight up to this point used a `tcpdump ... 'host
192.168.16.200'` capture filter. That filter is applied **at capture
time**, so it silently discarded anything not addressed directly
to/from the QRP's own IP — including any broadcast packet, which by
definition isn't addressed to a single host. This was a real
methodology gap, not just an oversight in hindsight: it meant every
capture tonight, including the original cold-launch one, could only
ever show what happens *after* the QRP is already reachable, never
what makes it reachable in the first place.

Fixed by re-running the cold-launch sequence (QRP off, ExpertSDR2 shut
down, then `tcpdump -i any -w ...` with **no host filter at all**,
then QRP on, ExpertSDR2 launched fresh) and searching the result for
broadcast/subnet-broadcast traffic instead of QRP-only traffic. Found
it immediately: the very first UDP/50001 traffic in the whole capture
is ExpertSDR2 broadcasting **seven** copies of an 24-byte packet —

```
03 ff 00 1a 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 fb e6
```

(magic `0x03`, a **new opcode `0x00`**, never seen in any previous
capture because the host filter always excluded it) — to
`127.255.255.255`, `192.168.255.255`, and `172.30.30.255`: the
broadcast address of every local interface the Mac has, one send per
interface, port 50001 both ends. **476 microseconds later** the QRP
answers with a direct **unicast** reply to the sender's real IP — the
same 24-byte "beacon" (opcode `0x01`) that opened every earlier
capture, previously assumed to be the radio spontaneously announcing
itself. It isn't spontaneous: it's a reply to this broadcast query,
and the entire rest of the already-documented connect-sync burst
follows immediately after.

This is structurally identical to the OpenHPSDR discovery pattern
Longpath already implements for every ANAN/HL2 radio
(`RadioDiscovery`, UDP broadcast on port 1024) — broadcast query,
unicast board-info reply, then unicast session traffic. It fully
resolves the 2026-08-25 "Gate 1: reachability" open question from
above: **the QRP was never refusing to respond because some special
"wake" state was missing — a plain ICMP `ping` or a directed
`sunsdr_probe` unicast packet was simply never going to work, because
the radio does not answer unsolicited unicast traffic at all. It only
speaks after receiving this specific broadcast query first.** Both
prior "unreachable without ExpertSDR2" tests (2026-08-25, ICMP-level;
tonight's `sunsdr_probe` test, protocol-level) never sent this
broadcast, so both would have failed regardless of whether a
companion session was active — the missing variable was never
ExpertSDR2 itself, it was this one packet.

**Practical implication for Phase C:** a native `SunSdrRadioConnection`
opening its **first and only** session does not need to replicate
anything ExpertSDR2-specific to "wake" the radio — it needs to send
this broadcast discovery query (magic `0x03`, opcode `0x00`, the exact
24-byte payload above or a QRP-specific variant of it) to the LAN
broadcast address on port 50001 and listen for the unicast beacon
reply, the same shape Longpath's own `RadioDiscovery` already
implements for OpenHPSDR radios. This does not by itself resolve Gate
2 (protocol exclusivity while ExpertSDR2 already holds a session,
2026-08-26 finding above) — that gate is irrelevant to this scenario
specifically because it only applies when a *second* client competes
with an already-active ExpertSDR2 session, which a native-only
connection (ExpertSDR2 never launched) would never be.

### CONFIRMED, 2026-08-26 (same evening): Longpath's own broadcast triggers the QRP's beacon reply — Gate 1 is solved, not just theorized

Built out immediately rather than left as a proposal: `tools/
sunsdr_probe.cpp` gained a `--discover` mode that sends exactly the
24-byte broadcast frame above (`03 ff 00 1a` + 17 zero bytes + `fb
e6`) to the broadcast address of every local IPv4 interface (via
`QNetworkInterface::allInterfaces()`, matching what the capture showed
ExpertSDR2 itself doing across its own interfaces), then listens for a
reply. Run against the real QRP (`./build/sunsdr_probe --discover 5`,
**with ExpertSDR2 still actively running** at the time — this was not
a from-cold-boot test):

```
> Discovery (24 Byte): 03 ff 00 1a 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 fb e6
  an 127.255.255.255 (lo0)
  an 192.168.255.255 (en9)
  an 172.30.30.255 (en0)
< Antwort von 192.168.16.200:50001 (24 Byte): 03 ff 01 1a 7c 00 00 00 41 19 c0 a8 10 c8 c0 a8 10 c8 51 c3 00 00 49 28
```

The QRP replied — the exact same 24-byte beacon (opcode `0x01`) seen
opening every capture tonight — to Longpath's **own** broadcast,
sent by a bare standalone tool with no ExpertSDR2 involvement in
constructing or sending the query. This is the strongest evidence yet
that Gate 1 (reachability) is solvable by Longpath alone: a real reply
to a real, independently-sent query, not an inference from packet
shapes.

One nuance worth flagging honestly: this test ran with ExpertSDR2
*already active*, so it does not by itself prove the QRP would have
answered a cold-boot discovery with **no** ExpertSDR2 session ever
having existed that day — it proves the beacon exchange specifically
tolerates a concurrent second sender, which is informative (Gate 2's
"exclusive hold" apparently applies to the deeper protocol
conversation, not to the discovery/beacon layer itself — the earlier
`sunsdr_probe` unicast-query test under an active ExpertSDR2 session
got zero replies on any profile, while this broadcast got one
immediately) but is a different claim from "works with ExpertSDR2
never launched at all." The clean version of that specific test (QRP
power-cycled, ExpertSDR2 never started, `sunsdr_probe --discover` run
cold) is the natural next bench step to close out Gate 1 completely.

### CONFIRMED, 2026-08-26 (same evening, clean run): Gate 1 fully closed — QRP answers Longpath with ExpertSDR2 never running

Ran immediately after the above: ExpertSDR2 fully quit (process list
confirmed empty, not just window-closed — the same rigor the
2026-08-25 test used), QRP power-cycled off then on, `ping
192.168.16.200` succeeded on the first try (contrast with 2026-08-25,
where the identical QRP-on/ExpertSDR2-off config gave "No route to
host"/timeout twice — see caveat below), then `./build/sunsdr_probe
--discover 5`:

```
< Antwort von 192.168.16.100:50001 (24 Byte): 03 ff 00 1a ... (echo of our own broadcast, expected)
< Antwort von 172.30.30.121:50001 (24 Byte): 03 ff 00 1a ... (another LAN host's echo, expected)
< Antwort von 192.168.16.200:50001 (24 Byte): 03 ff 01 1a 7c 00 00 00 41 19 c0 a8 10 c8 c0 a8 10 c8 51 c3 00 00 49 28
```

The third reply is the QRP's real beacon, byte-identical to every
other capture tonight, sent to Longpath's own tool with ExpertSDR2
never having run at all this session. **This is the clean test the
plan called for, and it passed. Gate 1 (reachability) is closed**:
`SunSdrRadioConnection::connectToRadio()` does not need to replicate
any ExpertSDR2-specific "wake" behavior — sending this discovery
broadcast and reading the beacon reply is sufficient, and Longpath
already has the code shape for exactly this (`RadioDiscovery`, the
OpenHPSDR broadcast-discovery class already used for every ANAN/HL2
radio).

**Necessary caveat, stated plainly rather than smoothed over:** this
same config (QRP powered on, ExpertSDR2 confirmed not running) failed
reachability *twice* on 2026-08-25 and succeeded immediately tonight.
Both findings are real bench results, not a contradiction to wave
away — the honest reading is that the 2026-08-25 "Gate 1" framing
likely conflated two different things: a real WLAN/AP-side
instability (the design doc's own competing hypothesis from that
session, also hit independently with the Anvelina on the same access
point) that happened to coincide with an ExpertSDR2-off state twice in
a row, versus a QRP-protocol-level requirement that turns out not to
exist. Tonight's result does not retroactively prove the 2026-08-25
failures were network flakiness rather than something QRP-specific —
it proves the *current* config works cleanly right now. Gate 1 is
closed for practical purposes (Phase C.1's blocking bench diagnostic
has now been run and passed), but Phase C.2 implementation work should
still budget for the possibility of occasional reachability flakiness
on this WLAN, independent of the protocol question this section
answers.

### New finding, 2026-08-26 (offline byte analysis, no bench needed): the large `0x12` reply looks like leaked ExpertSDR2 process memory, not radio data — a caution for reading any of the other large blobs

With the bench powered down for the night, re-examined the large
multi-hundred/thousand-byte payloads already captured tonight rather
than requesting new captures. Searching the 1042-byte `0x12` MAC→QRP
packet from the "PA button" capture (2026-08-26, attempt #1) for
printable ASCII turned up readable macOS Launch Services strings —
`"ExpertSDR2 ColibriNANO"`, `"LSBundlePath"="/Applications/ExpertSDR2
SunSDR2QRP.app"`, `"LSApplicationCoalitionIDKey"`, `"LSBundlePathINode"`
— i.e. **metadata about the ExpertSDR2 application itself**, not
anything resembling radio protocol content. The *same* opcode/size
packet in the original cold-launch capture was almost entirely zero
bytes, no readable content at all. Same shape, wildly different
content between two runs, one of them literally containing host
process bookkeeping strings: the working read is that this specific
field is **uninitialized or reused buffer memory on ExpertSDR2's own
side** (a classic C buffer-not-cleared bug, leaking whatever was
previously in that memory onto the wire) rather than meaningful
protocol content tied to the radio's state.

**Caution this implies for the still-open `0x0c`/`0x0d` blobs**: both
are also large (338 bytes), also mostly zero-padded, also from the
radio (`QRP→MAC`, unlike `0x12` which is `MAC→QRP` — an important
difference, since a *reply the radio itself constructs* is much less
likely to be leaking the Mac's own process memory than a *query the
Mac sends*). `0x0c`'s reply does show real internal structure (a
16-byte-strided repeating unit, 12 times, then a run of what look like
IEEE-754 single-precision floats near `1.0`) that a garbage buffer
would be unlikely to produce by chance — read as tentatively more
likely to be genuine device data (a 12-entry table, plausibly
per-band, followed by a calibration/gain curve near unity) than a
leaked-memory artifact, but **not confirmed**, and now flagged with
appropriately lower confidence than before this comparison. `0x0d`'s
reply is almost entirely zero except a short run of `1.0` doubles/
floats near the end — consistent with a companion "second calibration
table, currently at default/unity" reading, equally unconfirmed.
Resolving either needs a targeted capture comparing this payload
before/after an action that would plausibly change a real calibration
value (if one is even user-adjustable) — not more offline byte-staring
without a ground-truth delta to anchor against.

### BREAKTHROUGH, 2026-08-26 (same evening): a real I/Q stream, triggered by Longpath alone — the minimal RX-start sequence is found

Found by comparing a fresh live-connect capture against everything
already known, not by guessing: with QRP-on/ExpertSDR2-off/tcpdump
running from before ExpertSDR2 launched (the precise ordering matters
— two earlier attempts this same evening started tcpdump too late and
missed the transition), the exact moment the first port-50002 packet
appears lines up, to the same microsecond, with the reply to **one
specific control packet**: opcode `0x01` (`SUNSDR_OP_STATE_SYNC` in
ArtemisSDR's naming — there a 68-byte packet in the DX macro; the QRP
uses a smaller 30-byte version of the same opcode), payload
`7648ea9e010000000c08040302020202` (an 8-byte tail `0c 08 04 03 02 02
02 02` whose meaning is still unattributed, but the bytes are a real,
already-observed value from a legitimate session, not synthesized).

Extended `tools/sunsdr_probe.cpp` with `--listen-sync`: sends the
discovery broadcast (already confirmed safe), and the moment the
beacon reply arrives, replays this exact 30-byte `0x01` frame
byte-for-byte, then only listens — no other command. Run cold (QRP
power-cycled, ExpertSDR2 fully quit beforehand, confirmed via process
list):

```
< Leuchtfeuer von 192.168.16.200 -- QRP hat geantwortet.
> sende bereits bestaetigten Befehl an 192.168.16.200 (30 Byte): 03 ff 01 00 0c 00 00 00 00 00 01 00 00 00 76 48 ea 9e 01 00 00 00 0c 08 04 03 02 02 02 02
< ERSTES Paket auf Port 50002 von 192.168.16.200:50002 (77 Byte): 03 ff 00 1f 00 00 16 48 53 12 00 00 00 17 84 00 00 16 42 00 00 f4 41 00 ...

  Pakete auf Port 50002: 15336 (18374037 Byte gesamt)
```

**15,336 real I/Q packets, 18.3 MB, in an 8-second window — a genuine,
sustained receive stream, triggered by exactly two things: the
discovery broadcast and one replayed control frame, both already
independently confirmed safe, ExpertSDR2 never running.** This is the
first time any Longpath-authored code has gotten this radio to stream
real data. Matches the earlier structural read of this opcode
(`SUNSDR_OP_STATE_SYNC`) as the boundary between "connected, syncing
state" and "actively streaming."

**What this does and does not prove:**
- Proves: discovery (`0x00` broadcast) + this one `0x01` frame is
  *sufficient* to start an I/Q stream on this specific QRP, cold, with
  no other software involved. The earlier tonight's `--listen-freq`
  test (frequency alone, no `0x01`) got zero stream packets — this
  really is the specific trigger, not "any control packet will do."
- Does **not** prove this frame's 8-byte payload is safe to vary or
  even that it needs to be sent verbatim rather than derived —
  it was replayed byte-for-byte from one real capture, unmodified.
  Whether the QRP is actually listening to those 8 bytes (e.g. as a
  receiver-count/DDC-config array) or ignoring them entirely is
  untested; changing them was never attempted and should not be,
  without a dedicated bench step isolating that question specifically.
- Does **not** yet establish frequency control over the resulting
  stream — this test never sent the (already fully independently
  confirmed) `0x08` frequency frame, so the stream came in at
  whatever frequency the QRP's own internal state already had
  (likely wherever ExpertSDR2 last left it, since client-authoritative
  radio state doesn't reset on its own). Combining `0x01` then `0x08`
  is the natural next bench step, and both frames are now independently
  proven-safe pieces to combine.
- Does **not** decode the stream's actual sample content yet — packets
  arrived and were counted, not parsed. `SunSdrProtocol::decodeIqSamples`
  already exists and is tested against the confirmed 1210-byte header
  layout (Phase D.1), so decoding this exact traffic through it is
  the next natural check, not new work.

**Practical implication:** Phase C.1's blocking bench-capture gate is
now closed in every sense that matters — reachability (Gate 1), the
minimal RX-trigger sequence, and a working proof-of-concept are all in
hand. Phase C.2 (porting this into `SunSdrRadioConnection`'s real
session-opening code) is no longer blocked on missing information the
way it was at the start of this evening; it is blocked only on doing
the actual porting work, with the standing cite-and-attribute
discipline this document has held all along, and the opcode `0x17`
caution from earlier in this section applies unchanged — this
breakthrough used only opcodes already independently confirmed safe,
and that discipline should hold for whatever comes next too.

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

### New finding, 2026-08-27 (operator away, autonomous research window): candidate frequency-encoding formula found — source-grounded, band-plausible, but NOT bench-confirmed

**Status: hypothesis, not a finding.** Everything below is presented at
that confidence level deliberately — nothing here has been checked
against a bench capture at one exact, known frequency (design doc's
own earlier note flagged that as the actual missing piece: "Flagging
as solvable with a more targeted follow-up (tune to one exact, known
frequency, capture a single packet)"). This section exists so that
one future capture can confirm or kill the hypothesis in a minute,
not to declare the problem solved.

**Where this started:** re-reading ArtemisSDR's real frequency-packet
builder, `sunsdr_send_freq_pkt()`
(`sunsdr.c:2259-2277 [@f8b01d25c5]`):

```c
static void sunsdr_send_freq_pkt(int opcode, int sub, int freqHz)
{
    unsigned char pkt[26];
    unsigned char reply[64];
    unsigned long long scaled = (unsigned long long)freqHz * SUNSDR_FREQ_SCALE;

    sunsdr_build_header(pkt, opcode, sub, 0x08);
    /* Payload: 8-byte u64 LE at offset 18 */
    pkt[18] = (unsigned char)(scaled & 0xFF);
    ...
```

`SUNSDR_FREQ_SCALE` is `#define`d as `10` (`sunsdr.h:123
[@f8b01d25c5]`). So DX/PRO's own formula is: multiply the frequency in
Hz by 10, write the result as an 8-byte little-endian integer at
payload offset 0 (packet byte 18, right after the 18-byte header).

**Applying that formula turned up a real byte-alignment bug in this
document's own earlier notes.** The "isolated-action capture attempt
#2" section above quotes three 8-byte "payload" values by eye —
`8ca31dd76ce07808`, `7e52ebe884dc7808`, `f35cfab39cd87808` — and notes
neither a u64 nor a two-u32-word split produced a plausible frequency.
Re-deriving this from the one exact frame this project actually has in
code, byte-for-byte
(`SunSdrRadioConnection::replayedFrequencyFrameForTest()`,
`03ff0800080000000000010000008ca31dd76ce0780800000000`, 26 bytes total
— matching ArtemisSDR's own `pkt[26]` size exactly) shows why: indexing
that frame at the FORMAL 18-byte header boundary (the same boundary
`SunSdrProtocol.h`'s own documented header layout uses, and the same
one already flagged elsewhere in this document as carrying an
unexplained non-zero tail — "bytes 14-17... a checksum-or-sequence-
shaped value") gives payload = bytes[18:26] = `6ce0780800000000`, NOT
`8ca31dd76ce07808`. The three-quoted-value note had, by eye, folded in
4 bytes from the header's own already-flagged non-zero tail (bytes
14-17) as if they were the start of the payload, and lost the true
tail 4 bytes off the end. The genuinely-varying bytes are the SAME 4
bytes in both windows (`6ce07808`), just at different offsets in the
two accountings — the earlier note's "second u32 word steps by ~1000,
centered around 142 million" observation was directionally right and
survives the correction unchanged, it just was not looking at the
correctly-bounded payload.

**Reconstructing the other two captures' payloads the same way** (design
doc prose only records their informal 8-byte quotes, not the full
26-byte frames, so this assumes the same 4-byte tail correction and the
already-observed "last 4 payload bytes are zero" pattern holds for all
three — reasonable given all three were the same capture session, but
this specific step IS an assumption, not a re-derivation from raw
bytes):

| Capture | Corrected payload | u32-LE (first 4 bytes) | ÷10 → Hz | ÷10 → MHz |
| --- | --- | --- | --- | --- |
| #1 (the exact frame in code) | `6ce0780800000000` | 142,139,500 | 14,213,950.0 | 14.213950 |
| #2 | `84dc780800000000` | 142,138,500 | 14,213,850.0 | 14.213850 |
| #3 | `9cd8780800000000` | 142,137,500 | 14,213,750.0 | 14.213750 |

Applying ArtemisSDR's real `SUNSDR_FREQ_SCALE = 10` to the corrected
u32-LE value lands all three **inside the 20m amateur band
(14.000-14.350 MHz)**, at 14.2137-14.2140 MHz, stepping by an exact,
clean **100 Hz per tuning-knob detent** (1000 raw units ÷ 10) — a
thoroughly ordinary VFO step size. The trailing 4 bytes being zero in
every observed case is consistent with the payload actually being a
**32-bit** scaled value (max ~429 MHz at this scale — comfortable
headroom for any HF/VHF ham frequency) even though ArtemisSDR's own
code always writes a full 64-bit field; the upper 32 bits simply never
populate at realistic frequencies, so this isn't evidence they're
unused for some *other* purpose, only that this one capture window
never needed them.

**What would confirm or kill this in one step:** tune the QRP to one
exact, deliberately-chosen frequency in ExpertSDR2 (e.g. 14,074,000 Hz
— a recognizable, memorable FT8 frequency, also conveniently inside
the same 20m band this hypothesis already lands in), capture the
resulting `0x08` control frame (`sunsdr_probe`'s existing capture
tooling or a fresh `tcpdump` window both work), and check whether
`u32_LE(payload[0:4]) / 10` equals `14074000` exactly. If it does, the
formula is confirmed and `SunSdrRadioConnection::setReceiverFrequency()`
can be wired for real. If it's close-but-off by a fixed ratio, the
scale factor differs from DX/PRO's `10` for the QRP specifically — the
±100 Hz step size would still narrow that ratio down fast. If it's
wildly different, the hypothesis is wrong and this section should be
struck, not patched.

**Deliberately not done as part of this finding:** wiring this into
`setReceiverFrequency()` as a live, wire-sending implementation. This
project's own standing discipline for this connection is exact-byte
replay of already-bench-confirmed frames only (see the class's own
header comment) — a computed value from an unconfirmed formula is
exactly the kind of guess that discipline exists to keep off the wire
against real hardware, TX-capable even though Longpath's own driver is
RX-only. What this finding DOES enable, safely, with zero wire risk:
`SunSdr::encodeFrequencyPayload()` / `decodeFrequencyPayload()` pure
functions in `SunSdrProtocol.{h,cpp}`, unit-tested against the one real
captured frame this project has, ready to wire into
`setReceiverFrequency()` the moment a bench capture confirms or refines
the formula.
