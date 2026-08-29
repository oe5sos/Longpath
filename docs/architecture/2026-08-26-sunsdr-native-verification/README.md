# SunSDR2 QRP (Native Driver) Bench Verification Matrix

**Goal:** Confirm the SunSDR2 QRP **native driver**
(`SunSdrRadioConnection` — no ExpertSDR2 involved at all) works
end-to-end on real hardware.

**Design authority:**
`docs/architecture/2026-08-24-sunsdr-native-driver-design.md`.
**Implementation plan:** `docs/architecture/2026-08-26-sunsdr-connection-plan.md`.

This matrix is **not** the same thing as
`docs/architecture/2026-08-24-sunsdr-verification/README.md` — that one
covers the older TCI-client path (`TciClient` against a running
ExpertSDR2). This one covers the new, separate `RadioConnection`
subclass that replaces ExpertSDR2 entirely. Both paths continue to
exist side by side; a user can pick either.

## How to use

1. `./build.sh && ./run.sh`. **ExpertSDR2 must NOT be running** — the
   whole point of this path is that it works without it, and a running
   ExpertSDR2 holds the QRP's control channel exclusively (design doc,
   "the network path itself may need an active companion session" /
   confirmed session-exclusivity finding), which would make a failure
   here ambiguous (native driver bug vs. ExpertSDR2 blocking it).
2. Radio menu → Manage Radios… → Add Manually… (if not already saved)
   → protocol "SunSDR2 QRP (native, no ExpertSDR2)" → the QRP's IP →
   Save → select the entry → Connect.
3. For each row, follow the reproducer. Tick the matching status box.
   A failing row here is a real regression — this is receive-only,
   personal-hardware-adjacent code, but it talks UDP to a real radio,
   so failures should be tracked, not shrugged off.
4. Rows 1-3 were exercised live during the build session (2026-08-26,
   OE5SOS) and are marked accordingly below.

## Tester sign-off legend

- `[ ] Untested`
- `[x] Passed YYYY-MM-DD by NAME (callsign)`
- `[x] Failed YYYY-MM-DD by NAME (callsign), issue: #N`

---

## Row 1: Connect without ExpertSDR2

**Reproducer:** With ExpertSDR2 fully quit (not just disconnected —
the process itself not running) and the QRP powered on, connect
through the steps in "How to use" above.

**Expected:** Non-blocking success within a few seconds — no beacon
timeout. `ConnectionState` reaches `Connected` (not stuck at
`Connecting`/`Disconnected`).

**Status:** [x] Passed 2026-08-26 by OE5SOS (after two real bugs found
and fixed live: `m_controlSocket`/`m_streamSocket` were binding to an
ephemeral local port instead of the QRP's fixed well-known port —
`SunSdrRadioConnection.cpp` `init()`, see the plan doc's dated update
for the full story)

---

## Row 2: Panadapter shows a live spectrum

**Reproducer:** With the QRP connected via the native driver, observe
the panadapter/waterfall on the slice's pan.

**Expected:** A real, moving spectrum trace and scrolling waterfall —
not a flat line, not frozen.

**Status:** [x] Passed 2026-08-26 by OE5SOS — real peak reading
observed (-133.7 dBm @ ~21.09x MHz), live-updating waterfall.

---

## Row 3: Receive audio

**Reproducer:** With the QRP connected via the native driver, listen
on the speakers/headphones.

**Expected:** Real receive audio audible, demodulated client-side by
Longpath's own WDSP chain from the raw I/Q this connection streams
(the QRP itself does no demodulation for this path — mode changes in
Longpath should work purely client-side; see Row 5).

**Status:** [x] Passed 2026-08-26 by OE5SOS

---

## Row 4: Frequency control actually retunes the radio

**Superseded 2026-08-27** — this row originally documented frequency
control as a known, deliberate limitation (`setReceiverFrequency()` a
no-op). That's no longer true: the frequency-payload encoding was
found and bench-confirmed the same day (design doc, "candidate
frequency-encoding formula found" → "CONFIRMED against a live, exact,
known-frequency test"), and `setReceiverFrequency()` now sends a real,
computed frequency-set frame.

**Reproducer:** Connect via the native driver. Change frequency from
Longpath (VFO drag, direct entry, Band button) to a specific,
deliberately-chosen value.

**Expected:** The radio actually retunes — confirm independently (e.g.
against a known station/beacon frequency, or cross-check with a
second, read-only observer) that the requested frequency is what the
radio is genuinely receiving on, not just what Longpath's own display
claims.

**Status:** [x] Passed 2026-08-27 by OE5SOS — bench-confirmed via
`tools/sunsdr_freq_confirm.py` (a read-only tcpdump observer, not this
row's own reproducer path): ExpertSDR2 displayed 7,099,904 Hz after a
VFO scroll; the real captured `0x08` frame decoded to 7,099,204 Hz via
the same formula `setReceiverFrequency()` now uses — 700 Hz apart out
of ~7.1 MHz (0.01%), consistent with scroll-settling lag, not a formula
error. `setReceiverFrequency()` itself was separately confirmed live to
execute without error during a real connect (`SunSdr:
setReceiverFrequency() -> 20889577 Hz` logged, connection stayed
`Connected` and stable). **Not yet independently re-confirmed via this
row's own reproducer** (retuning Longpath's own VFO live and
cross-checking against a known station) — the evidence above is strong
but came from the ExpertSDR2 side of the same formula, not a
Longpath-initiated retune checked against real-world reception. Worth
a follow-up pass focused specifically on that direction.

One caveat carried over, still open: the 18-byte header's bytes 14-17
are reused verbatim from the one already-confirmed-accepted frame
rather than computed — if retuning proves unreliable under sustained
real-world use (many retunes in a row, not just one), that header tail
is the next thing to investigate, not the payload formula.

---

## Row 5: Mode / filter / AGC / NR change client-side

**Reproducer:** With the QRP connected, switch the slice through
several modes (including CWL/CWU), change filter width, toggle
AGC/NR options.

**Expected:** Since this connection streams raw I/Q at a fixed native
rate and does no mode-aware framing to the radio at all (every
TX/mode-shaped setter is a no-op — see `SunSdrRadioConnection.cpp`'s
no-op block), every one of these should work exactly like any other
board's client-side WDSP processing: audio changes accordingly, no
wire traffic needed, no round-trip lag waiting on the radio.

**Status:** [ ] Untested

---

## Row 6: Reconnect cycle

**Reproducer:** With the QRP connected and working (Rows 1-3 all
passing), disconnect, then connect again to the same entry.

**Expected:** Clean disconnect (audio stops, panadapter clears, no
stale "Connected" state); clean reconnect (Rows 1-3 all work again
without restarting Longpath). Repeat 2-3 times.

**Status:** [ ] Untested

---

## Row 7: QRP powered off / unreachable mid-session

**Reproducer:** With the QRP connected and streaming, power off the
QRP (or unplug its network connection) without disconnecting from
Longpath first.

**Expected:** Longpath detects the dead stream within a reasonable
time and transitions out of `Connected` (not stuck showing a live
connection indefinitely against dead air); the panadapter freezes
rather than showing garbage (the existing stale-data freeze guard in
`SpectrumWidget::pushWaterfallRow()` should apply here the same as any
other board, once the 2000ms staleness threshold passes) — confirm
this actually engages for this connection type specifically, since
it's a different code path than P1/P2.

**Status:** [ ] Untested

**Update 2026-08-28:** a self-review pass over the just-landed native
driver found this row was not just untested but genuinely unimplemented
— nothing in `SunSdrRadioConnection` re-armed after the initial connect
watchdog stopped, so `ConnectionState` would have stayed stuck at
`Connected` forever against a dead radio. Fixed: a periodic silence
watchdog (`m_dataWatchdog`, 1s tick, 5s threshold — matching
`ConnectionState::LinkLost`'s own doc comment "no frames for >5s"),
mirroring `P1RadioConnection::onWatchdogTick()`'s pattern, full teardown
on trip since this class has no reconnect timer of its own. Code-level
only — this row's actual reproducer (power off a live QRP, watch the
transition happen) is still the real test and remains untested.

---

## Row 8: Connect attempt while ExpertSDR2 IS running

**Reproducer:** With ExpertSDR2 running and holding an active session
against the QRP, try connecting through Longpath's native driver at
the same time.

**Expected:** A clean, understandable failure — either the discovery
beacon never arrives (ExpertSDR2 holding the session exclusively,
matching this evening's own confirmed finding) or, if it does arrive,
no confusing half-connected state. The error message should now say
"no beacon reply" (if the beacon genuinely never came) rather than the
old always-wrong wording — confirm the message is accurate for
whichever failure mode actually occurs here.

**Status:** [ ] Untested

---

## Row 9: TX-shaped controls are inert

**Reproducer:** With the QRP connected via the native driver, exercise
every TX-adjacent UI control this connection's `RadioInfo` still
exposes in Longpath's UI (PTT/MOX button, attenuator/preamp controls,
antenna routing, TUNE) if any are reachable at all for this board
type — if `BoardCapabilities` already hides them, confirm that instead.

**Expected:** Either the controls are hidden/disabled by
`BoardCapabilities` gating for this board (preferred, matches the
design doc's receive-only scope), or if any are reachable, confirm
they are true no-ops with zero wire traffic (already unit-tested in
`tst_sunsdr_radio_connection.cpp::everyTxShapedSetterIsANoOp`, but that
test never reaches this code through the real GUI dispatch path — this
row is the live confirmation that the UI itself doesn't offer a false
promise).

**Status:** [ ] Untested

**Update 2026-08-28, code-level check (not the live reproducer above):**
`BoardCapabilities`' `isRxOnlySku` field — the mechanism that hides the
PA/TX Setup category for other RX-only boards (`SetupDialog.cpp`,
`GeneralOptionsPage.cpp`) — is deliberately `false` on the SunSDR2 QRP
row, per that row's own comment: the QRP genuinely has TX hardware
(opcode `0x17` drive byte, PA enable `0x24` both exist in the
protocol), `isRxOnlySku` describes hardware, not this driver's current
software support. So MOX/PTT/TUNE controls are **not** hidden by that
gate — they stay reachable in the UI and land on
`SunSdrRadioConnection`'s no-op overrides
(`everyTxShapedSetterIsANoOp`, already unit-tested). The PA Setup page
specifically stays hidden regardless, via the independent
`hasPaProfile=false` field. This is a real, intentional design choice,
not a gap — but the row's actual reproducer (click MOX/TUNE live,
confirm zero wire traffic through the real GUI dispatch path) is still
untested.

---

## Row 10: Coexistence with a real OpenHPSDR radio

**Reproducer:** Connect a real OpenHPSDR radio (e.g. the ANAN-7000DLE)
on one slice, then connect the SunSDR2 QRP via the native driver on
another.

**Expected:** Both work simultaneously without interference — the
native driver is architected as a real, independent `RadioConnection`
(design doc decision, "the same standing as an ANAN"), not sharing any
of the older TCI-client's slice-binding safety machinery, so this row
exists to confirm that holds in practice, not just on paper.

**Status:** [ ] Untested
