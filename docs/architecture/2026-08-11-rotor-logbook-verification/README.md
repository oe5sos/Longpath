# Rotor + Logbook — Bench Verification Matrix (2026-08-11)

Scope: the rotor-comfort features landed in `cab3faa0` plus the core
rotor/logbook flows they touch. One row per behaviour; fill the Result
column on the bench (PASS / FAIL / DONE_WITH_CONCERNS + a note).

Bench setup: ANAN-G2 connected; rotctld reachable (or deliberately
absent for the simulation rows); QRZ credentials configured for the
logbook rows that need them.

## §1 Taught presets

| # | Step | Expected | Result |
|---|------|----------|--------|
| 1.1 | Fresh start, look at the new row under the cardinal presets | Four "·" slots + Park + LP; tooltip on a "·" says right-click teaches it | |
| 1.2 | Left-click an untaught slot | Status line: "Empty preset — right-click…"; dial aim unchanged; mast does NOT move | |
| 1.3 | Aim 75° on the rose, right-click slot 1 → "Store current aim here (75°)" → name it "EU" | Button reads "EU"; tooltip shows 75° | |
| 1.4 | Turn the dial elsewhere, left-click "EU" | Dial AIMS 75°; mast does not move until Rotate | |
| 1.5 | Rotate after 1.4 | Mast turns to 75° (or simulated-needle if no rotator, clearly labelled) | |
| 1.6 | Right-click "EU" → Rename → "Europa" | Button text updates immediately | |
| 1.7 | Right-click "EU" → Clear | Slot back to "·" | |
| 1.8 | Teach two slots, restart the app | Both slots come back with names and bearings | |
| 1.9 | With NO aim set and NO rotator connected, right-click a slot | "Store current aim" entry disabled (greyed) — teaching from nothing is refused | |
| 1.10 | With no aim but a CONNECTED rotator reporting fresh position, right-click a slot | Store entry offers the rotator's current azimuth | |

## §2 Park

| # | Step | Expected | Result |
|---|------|----------|--------|
| 2.1 | Left-click Park before teaching it | Status: "No park position yet — right-click…" | |
| 2.2 | Aim 10°, right-click Park → store | Tooltip shows 10° | |
| 2.3 | Aim elsewhere, click Park, then Rotate | Mast returns to 10° | |
| 2.4 | Park survives a restart | Tooltip still 10° after relaunch | |

## §3 Long path

| # | Step | Expected | Result |
|---|------|----------|--------|
| 3.1 | No aim set, click LP | Status hint; nothing moves | |
| 3.2 | Aim 57°, click LP | Aim becomes 237°; mast unmoved until Rotate | |
| 3.3 | Click LP twice | Back to 57° (no drift from the double flip) | |

## §4 Spot → rotor

| # | Step | Expected | Result |
|---|------|----------|--------|
| 4.1 | Spot List right-click → "Turn rotor to X" | Rotor dock raises; call fills in; dial aims; turn begins (pre-existing flow — regression check) | |
| 4.2 | Panadapter spot label right-click | Menu shows "Turn rotor to X" between Tune and Copy | |
| 4.3 | Choose it | Same behaviour as 4.1, from every pan (not only pan 0) | |
| 4.4 | With no rotator connected | Needle goes dashed/dim "simulated", status says so — no silent pretending | |

## §5 Shed order (narrow dock)

| # | Step | Expected | Result |
|---|------|----------|--------|
| 5.1 | Drag the dock progressively smaller | Cardinal row disappears BEFORE the taught-preset row; compass survives longest | |
| 5.2 | Drag it back tall | All rows return, taught slots intact | |

## §6 Logbook regression sweep (touched indirectly)

| # | Step | Expected | Result |
|---|------|----------|--------|
| 6.1 | Log a QSO from the panel | Row appears; ADIF file grows | |
| 6.2 | "Point the beam from the log" on a logged contact | Aim from grid bearing still works (unchanged path) | |
| 6.3 | QRZ upload of a fresh QSO | Uploads; marked as uploaded | |
| 6.4 | Logbook window: Import merge + Export ADIF/CSV | All three still work | |

## Open ends going into this bench

- "QLayout: Cannot add a null widget" — if it fires, the log now
  prints `[bt]` frames; capture them.
- `TxWorkerThread` "PC-mic pulls ran short" — should stay absent; if it
  appears, note what mic source was selected.
