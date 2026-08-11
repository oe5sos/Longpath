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
| 1.1 | Fresh start, look at the new row under the cardinal presets | Four "·" slots + Park + LP; tooltip on a "·" says right-click teaches it | PASS (remote bench 2026-08-11, Claude-driven) |
| 1.2 | Left-click an untaught slot | Status line: "Empty preset — right-click…"; dial aim unchanged; mast does NOT move | PASS — status hint shown, nothing moved |
| 1.3 | Aim 75° on the rose, right-click slot 1 → "Store current aim here (75°)" → name it "EU" | Button reads "EU"; tooltip shows 75° | PASS — taught 74°, named EU |
| 1.4 | Turn the dial elsewhere, left-click "EU" | Dial AIMS 75°; mast does not move until Rotate | PASS — re-aims 074° exactly, mast unmoved |
| 1.5 | Rotate after 1.4 | Mast turns to 75° (or simulated-needle if no rotator, clearly labelled) | PASS — simulated needle, dashed ring, 'NO ROTATOR' + status line |
| 1.6 | Right-click "EU" → Rename → "Europa" | Button text updates immediately | PASS — renamed to Europa live |
| 1.7 | Right-click "EU" → Clear | Slot back to "·" | PASS — back to '·' |
| 1.8 | Teach two slots, restart the app | Both slots come back with names and bearings | PASS — EU + Park survived the rebuild/relaunch |
| 1.9 | With NO aim set and NO rotator connected, right-click a slot | "Store current aim" entry disabled (greyed) — teaching from nothing is refused | |
| 1.10 | With no aim but a CONNECTED rotator reporting fresh position, right-click a slot | Store entry offers the rotator's current azimuth | |

## §2 Park

| # | Step | Expected | Result |
|---|------|----------|--------|
| 2.1 | Left-click Park before teaching it | Status: "No park position yet — right-click…" | PASS — hint shown |
| 2.2 | Aim 10°, right-click Park → store | Tooltip shows 10° | PASS — taught 74° |
| 2.3 | Aim elsewhere, click Park, then Rotate | Mast returns to 10° | PASS — re-aims from 202° |
| 2.4 | Park survives a restart | Tooltip still 10° after relaunch | PASS |

## §3 Long path

| # | Step | Expected | Result |
|---|------|----------|--------|
| 3.1 | No aim set, click LP | Status hint; nothing moves | PASS — hint, nothing moves |
| 3.2 | Aim 57°, click LP | Aim becomes 237°; mast unmoved until Rotate | PASS — 074 → 254 |
| 3.3 | Click LP twice | Back to 57° (no drift from the double flip) | PASS — 254 → 074, no drift |

## §4 Spot → rotor

| # | Step | Expected | Result |
|---|------|----------|--------|
| 4.1 | Spot List right-click → "Turn rotor to X" | Rotor dock raises; call fills in; dial aims; turn begins (pre-existing flow — regression check) | |
| 4.2 | Panadapter spot label right-click | Menu shows "Turn rotor to X" between Tune and Copy | PASS — entry present between Tune and Copy |
| 4.3 | Choose it | Same behaviour as 4.1, from every pan (not only pan 0) | FAIL → FIXED same session: connect missing for pan-0 (wireSpectrumForPan excludes it); re-wired at the pan-0 one-shot site. RETESTED after rebuild: PASS — dock opens, call fills, QRZ lookup resolves JO64BB, globe swings, turn begins. |
| 4.4 | With no rotator connected | Needle goes dashed/dim "simulated", status says so — no silent pretending | PASS — dashed simulated needle + honest status |

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

## Findings (remote bench, 2026-08-11 afternoon)

1. **Waterfall solid magenta** — the entire waterfall area renders as a
   single magenta block on this bench (macOS, Metal, ClarityBlue). The
   Aug-7 palette fixes removed the magenta STOP, so this is not the
   palette cap: suspect the GPU texture upload / AGC window path.
   Reproducible from app start. NOT related to today's rotor work.
   **RESOLVED same evening**: root cause was the dynamic-overlay
   texture being partial-uploaded after a recreate, drawing undefined
   Metal memory (magenta) OVER a healthy waterfall. Found via the
   NEREUS_WF_DEBUG=2 green-fill discriminator; fixed with a one-shot
   full upload after every texture (re)create
   (`m_ovDynNeedsFullUpload`). Bench-confirmed on the live app.
2. **Spot menu → "Turn rotor" did nothing on pan-0** — root-caused and
   fixed same session (see §4.3). Secondary observation while broken:
   after the dead click, the PAN overlay menu opened in its place —
   worth one eye during the retest; likely the un-consumed click
   falling through to the native QRhi surface.
3. **Spot-menu click falls through to the pan** — CONFIRMED as its own
   bug, independent of the rotor wiring: after ANY spot-menu action
   (Tune to, Turn rotor) the pan's overlay context menu opens at the
   same position. The action itself fires; the un-consumed release
   reaches the native QRhi surface afterwards. Needs an event->accept
   / popup-boundary fix in SpectrumWidget's right-click path.
   **FIX LANDED, retest pending**: two layers — the spot and notch
   menus now show async via `QMenu::popup()` (no nested `exec()` loop
   inside mousePressEvent on the native QRhi surface), and a
   250 ms replay guard swallows any right-press that arrives right
   after a context menu closed, before it can reach the overlay-menu
   fallback. Retest: right-click spot label → choose "Tune to" →
   overlay menu must NOT appear.
4. **"Long path" and "LP" duplicated** — the Rotate row's existing
   "Long path" button and the new preset-row "LP" do the same thing.
   Dedupe (keep LP next to the presets; drop the wide button) or make
   "Long path" a latched mode. Decision pending.
   **RESOLVED**: the wide "Long path" button is retired; "LP" next to
   the presets is the one control (it also has the better empty-aim
   hint). §3 rows keep testing the surviving LP button unchanged.
