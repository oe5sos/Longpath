# SunSDR2 QRP (TCI Client) Bench Verification Matrix

**Goal:** Confirm the SunSDR2 QRP / ExpertSDR2 TCI-client integration
(Verbindung, Ton, Bild, Steuerung, and the safety invariant that keeps
it away from a real radio) works end-to-end on real hardware.

**Design authority:**
`docs/architecture/2026-08-24-sunsdr-tci-client-design.md`.
**Measured wire protocol:** `docs/TCI-SunSDR-gemessen.md`.

## How to use

1. `./build.sh && ./run.sh`, ExpertSDR2 running against the SunSDR2
   QRP, TCI enabled on the default port (40001).
2. For each row, follow the reproducer. Tick the matching status box.
   If the observed behaviour differs, note it and open a follow-up —
   this matrix does not gate a release by itself the way the larger
   phase matrices do (SunSDR is a personal-hardware accessory, not a
   shipped-SKU driver), but a failing row here is a real regression.
3. Rows 1-7 were exercised live during the build session
   (2026-08-24, OE5SOS) and are marked accordingly below; re-run them
   after any change that touches `TciClient` or `MainWindow_SunSdr.cpp`.

## Tester sign-off legend

- `[ ] Untested`
- `[x] Passed YYYY-MM-DD by NAME (callsign)`
- `[x] Failed YYYY-MM-DD by NAME (callsign), issue: #N`

---

## Row 1: Connect

**Reproducer:** Radio menu → SunSDR (TCI) → Verbinden… → enter
`127.0.0.1:40001` (or the host ExpertSDR2 runs on).

**Expected:** Non-blocking "SunSDR verbunden" notice naming the
device; `Radio → SunSDR (TCI) → Trennen` becomes meaningful (a live
connection exists).

**Status:** [x] Passed 2026-08-24 by OE5SOS

---

## Row 2: Audio

**Reproducer:** With SunSDR connected and the receiver running in
ExpertSDR2, listen on the speakers and on a headset plugged in mid
session.

**Expected:** RX audio audible on both output devices; master-mute
toggle silences it like any other source.

**Status:** [x] Passed 2026-08-24 by OE5SOS

---

## Row 3: Panadapter shows a live spectrum

**Reproducer:** With SunSDR connected, observe the panadapter/
waterfall on the slice's pan.

**Expected:** A real, moving spectrum trace and scrolling waterfall,
centred on the SunSDR's actual DDC centre frequency (`dds:`), not the
14.225 MHz slice construction default.

**Status:** [x] Passed 2026-08-24 by OE5SOS (after fixing a missing
`startIqStream()` call — see design doc, defect 1)

---

## Row 4: Panadapter follows a live retune

**Reproducer:** With SunSDR connected and the panadapter showing a
spectrum, change band/frequency inside ExpertSDR2 itself (not via
Longpath).

**Expected:** Longpath's panadapter re-centres on the new frequency
within about a second, without reconnecting.

**Status:** [x] Passed 2026-08-24 by OE5SOS (after fixing the
two-receiver `dds:` cross-talk — see design doc, defect 2)

---

## Row 5: Frequency sync, outbound

**Reproducer:** With SunSDR connected, change the slice's frequency in
Longpath (VFO drag, direct entry, or Mode/Band controls acting on the
SunSDR-bound slice).

**Expected:** ExpertSDR2's VFO follows within about a second.

**Status:** [x] Passed 2026-08-24 by OE5SOS

---

## Row 6: Mode sync, outbound (including CW)

**Reproducer:** With SunSDR connected, switch the slice through
several modes in Longpath, including CWL and CWU.

**Expected:** ExpertSDR2's mode follows for every mode Longpath
supports over TCI (LSB/USB/DSB/CWL/CWU/FM/AM/DIGU/DIGL/SAM/DRM); CWL
and CWU both land ExpertSDR2 in CW (the device does not distinguish
the sideband over TCI — see design doc, defect 3).

**Status:** [x] Passed 2026-08-24 by OE5SOS (after fixing the
`cwl`/`cwu` → `cw` outbound mapping)

---

## Row 7: Frequency + mode sync, inbound

**Reproducer:** With SunSDR connected, change frequency and mode
inside ExpertSDR2 itself.

**Expected:** Longpath's slice (VFO flag, mode indicator) follows
within about a second, and does **not** echo the change straight back
out to ExpertSDR2 (watch for the VFO/mode settling once, not
oscillating).

**Status:** [x] Passed 2026-08-24 by OE5SOS

---

## Row 8: Safety invariant — SunSDR never touches a real radio

**Reproducer:** Connect a real OpenHPSDR radio first (so its slice is
active and DDC-bound), *then* connect SunSDR. Observe which slice
SunSDR's audio/panadapter/control end up on. Retune the real radio
afterward and confirm nothing goes out to ExpertSDR2; retune via
ExpertSDR2 and confirm the real radio's slice is untouched.

**Expected:** `connectSunSdr()` skips the real radio's slice entirely
and either reuses another unbound slice or creates a new one for
SunSDR — the real radio's audio, panadapter, and frequency/mode are
never touched by the SunSDR connection in either direction.

**Status:** [ ] Untested live (covered by
`tst_sunsdr_control_wiring::verbindenUeberspringtEineEchteScheibeUndLegtEineNeueAn`
and `echteScheibeMitDdcBindungWirdInKeinerRichtungGesteuert`-class
tests with a simulated binding; not yet exercised with two real,
simultaneously connected radios)

---

## Row 9: Reconnect cycle

**Reproducer:** With SunSDR connected and working (audio + panadapter
+ control all confirmed), choose Trennen, then Verbinden… again to the
same endpoint.

**Expected:** Clean disconnect (audio stops, panadapter mapping
clears), clean reconnect (everything from rows 1-7 works again without
restarting Longpath).

**Status:** [ ] Untested

---

## Row 10: ExpertSDR2-side disconnect / crash recovery

**Reproducer:** With SunSDR connected, quit or kill ExpertSDR2 (or
disable its TCI server) without using Longpath's Trennen.

**Expected:** Longpath detects the dropped connection (State::Error or
State::Disconnected), shows a non-blocking notice, and cleans up audio
+ panadapter mapping on its own — no stuck "opportunistic" audio
source, no stale panadapter feed, no crash.

**Status:** [ ] Untested
