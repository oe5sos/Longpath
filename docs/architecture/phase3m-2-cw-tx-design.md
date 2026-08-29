# Phase 3M-2 Design: CW TX — Sidetone, Keyer, and QSK/Break-In

**Status:** Source research complete; design sketch pending a hardware-capability check (§9) before an implementation plan can be written.
**Author:** Martin Fischer (OE5SOS), AI-assisted (Claude Code).
**Date:** 2026-08-25.
**TX-epic slot:** `docs/architecture/phase3m-tx-epic-master-design.md` §6 (Phase 3M-2 — CW TX), sub-phases §6.1 (Sidetone + Keyer + QSK) and §6.2 (CWX + Paddle USB Driver).
**Upstream stamps captured this session:** Thetis `v2.10.3.15-5-g852bf0e` (`@852bf0e`); mi0bot-Thetis `v2.10.3.15-beta1` (`@0cef1c9`).

---

## Relationship to the master design

The TX-epic master doc already scopes 3M-2 at the component/UI-surface level
(§6.1–6.2): the CW MOX special case, the SW/HW sidetone split, the 10 CW
`NetworkIO` setters, the CWX form port, the CWInput USB driver. This document
does not replace that scope — it grounds it in the actual Thetis source
(exact algorithms, exact timing constants, exact wire bytes) and adds three
things the summary-level scope didn't surface:

1. **A citation-drift finding.** The master doc's HL2 CWX bit-3 note pins
   `networkproto1.c:1247-1252 [@c26a8a4]`; a fresh mi0bot-Thetis checkout at
   `@0cef1c9` has the identical logic at lines 1252-1261. The pinned commit
   has drifted out from under the citation — see §6 and §9.
2. **A real architectural tension**, not just a risk note: Thetis's full QSK
   bypasses its software PTT poll entirely and lets FPGA firmware own
   TX/RX timing; Longpath's `MoxController` is built as a single central
   state machine with no such bypass. See §7–§9.
3. **Byte-level wire detail** for both protocols, needed before any
   `RadioConnection` wire-write code can be written — see §6.

Read this alongside master-doc §6, not instead of it.

## 1. Goal

Phase 3M-2 makes CW transmission usable from Longpath's console, scoped
strictly to what this round of research covered: the keyer, the sidetone,
QSK/break-in timing, the wire protocol that carries key state to the radio,
and the HL2-specific CWX quirk. It does **not** cover CW message macros,
contest keyer memories, or UI/applet layout beyond the stubs that already
exist (§7) — those are separate concerns even where a stub happens to sit
nearby in the tree. CWX (canned-text sending) and the external-paddle USB
driver are master-doc §6.2 (Phase 3M-2b) and are only touched here where the
source research crossed into them (the CWX wire-keying mechanism itself, §3
and §6).

Concrete, operator-visible success criteria, each grounded in a Thetis
reference behavior:

- **Audible, click-free sidetone.** Keying a paddle or straight key produces
  a sinewave sidetone at a configurable pitch (Thetis default 400 Hz,
  `ChannelMaster/cmaster.c:235-251 [@852bf0e]`) with raised-cosine rise/fall
  edges (5 ms default, `ChannelMaster/sidetone.c:41-82 [@852bf0e]`) — no key
  clicks, no thumps.
- **A working keyer.** The operator can key CW at a chosen speed and iambic
  mode (A/B) or in straight/bug mode, matching the parameter surface Thetis
  pushes to firmware (`EnableCWKeyer`, `SetCWKeyerSpeed`, `SetCWKeyerMode`,
  `SetCWKeyerWeight`, `SetCWIambic` — `HPSDR/NetworkIOImports.cs:273-321
  [@852bf0e]`), or types text through a CWX-style software keyer if no
  firmware iambic engine is available on the operator's board (§9).
- **QSK/break-in that "sounds right."** The operator can select Manual,
  Semi, or QSK break-in (`enums.cs:430-435 [@852bf0e]`), and full QSK
  behaves like Thetis's: near-zero-latency TX/RX switching per element with
  `BreakInDelay` forced to `0` while QSK is active (`console.cs:12930-12975
  [@852bf0e]`), distinct from semi break-in's ~300 ms hang (`console.cs:18453
  [@852bf0e]`).
- **Correct wire-level key state.** Real-time key-down/dot/dash state
  reaches the radio in the bit layout the operator's hardware expects — the
  standard 3-bit embedded nibble for most boards, or the HL2's 4-bit nibble
  with `cwx_ptt` in bit 3, if the operator's radio is a HermesLite-2.

Everything below is scoped to these four mechanisms only; it is not a
general CW-operating design.

## 2. Source provenance and licensing

Primary source for keyer, sidetone, and QSK behavior: **Thetis**, stamp
`v2.10.3.15-5-g852bf0e` / `@852bf0e` — files under
`Project Files/Source/Console/` (`console.cs`, `cwx.cs`, `enums.cs`,
`setup.cs`, `CW/CWInput.cs`), `Project Files/Source/ChannelMaster/`
(`sidetone.c`, `cmaster.c`, `networkproto1.c`, `network.c`), and
`Project Files/Source/HPSDR/NetworkIOImports.cs`.

Source for the HL2-specific CWX PTT bit-3 quirk and HL2 timing constants:
**mi0bot-Thetis**, stamp `v2.10.3.15-beta1` / `@0cef1c9` — a fork touching
`ChannelMaster/networkproto1.c`, `netInterface.c`, `network.h`, `cwx.cs`,
and `NetworkIOImports.cs`.

Any code ported or adapted from these sources into Longpath is subject to
the project's standard GPL header/attribution preservation rule as defined
in `CLAUDE.md`; that rule is cited by name here and not restated.

## 3. Thetis CW keyer architecture

The iambic keyer state machine — dot/dash memory, squeeze logic, weighting,
WPM timing, Iambic A/B — is **native FPGA behavior on HPSDR/Hermes-class
hardware, not C# code**. The console only pushes parameters and forwards
raw contact state.

**Legacy dead code.** `Project Files/Source/Console/cwkeyer.cs`
(`class CWKeyer2`, namespace `PowerSDR`) implements a full software iambic
engine calling `DttSP.NewKeyer/KeyValue/KeyerStartedWait`, but it is not
referenced in `Thetis.csproj` (only `CW\CWInput.cs` is) and is never
instantiated. It is inert PowerSDR/DttSP-era code left in the tree — not
part of the live path.

**Live paddle path.** `CW/CWInput.cs [@852bf0e]` only routes which port
supplies paddle contacts (`SetPrimaryInput`, line 127) and PTT/key lines for
a secondary COM port (`SecondaryPTTLine`/`SecondaryKeyLine`, lines 57–111).
Raw contact closures from a PC COM port's DSR/CTS pins are forwarded
straight to firmware with no C# iambic logic in between, e.g.
`CAT/SDRSerialPortII.cs:261-262 [@852bf0e]`:
```csharp
if (isDsrChanged) NetworkIO.SetCWDot(dsrHolding ? 1 : 0);
else if (isCtsChanged) NetworkIO.SetCWDash(ctsHolding ? 1 : 0);
```

**Keyer parameters pushed to firmware** via P/Invoke into
`ChannelMaster.dll`, `HPSDR/NetworkIOImports.cs:273-321 [@852bf0e]`:
```csharp
public static extern void EnableCWKeyer(int enable);
public static extern void SetCWKeyerSpeed(int speed);
public static extern void SetCWKeyerMode(int mode);   // 0=Iambic A, 1=Iambic B
public static extern void SetCWKeyerWeight(int weight);
public static extern void EnableCWKeyerSpacing(int bits); // "strict char spacing"
public static extern void SetCWIambic(int bit);        // iambic vs external/bug key
```
Callers: `console.cs:28802 [@852bf0e]` (`NetworkIO.SetCWKeyerSpeed(ptbCWSpeed.Value)`,
speed slider); `setup.cs:8797-8806` and `setup.cs:11925-11928 [@852bf0e]`
(Iambic-vs-bug and Mode A/B checkboxes). Tooltip at
`setup.designer.cs:38507 [@852bf0e]`: "Iambic or External/Bug Key" — there
is no separate Ultimatic or Straight mode; straight-key operation is simply
the non-iambic "external/bug" path via `SetCWIambic(0)`.

**Default is firmware keyer.** `console.cs:10646-10665 [@852bf0e]`, property
`CWFWKeyer` (default `true`, set at `console.cs:912`), calls
`NetworkIO.EnableCWKeyer(1)`. The toggle checkbox `chkCWFWKeyer` is hidden in
the UI (`console.resx:660`, `Visible=False`); comment at `console.cs:34485`:
"Although CWFWKeyer is mostly a deprecated flag, it's useful in the
QSK-enabled firmware (1.7 or later)."

**CWX (typed-text keyer) is the one genuinely client-side keyer.**
`cwx.cs [@852bf0e]` runs its own multimedia timer (`setup_timer`,
`wpmrate()`, `timeSetEvent`) that reads characters from a FIFO and, per
Morse element, calls `NetworkIO.SetCWX(Convert.ToInt32(state))`
(`cwx.cs:302`) — the PC computes exact element timing and streams raw key
up/down bits to the radio, bypassing the firmware iambic engine entirely.

**MOX/PTT wiring for CW**, `console.cs:25397-25407 [@852bf0e]`, function
`PollPTT`:
```csharp
int dotdashptt = NetworkIO.nativeGetDotDashPTT();  // bit0=ptt, bit1=dash, bit2=dot
bool cw_ptt = CWInput.KeyerPTT && _current_breakin_mode == BreakIn.Semi;
```
This is used only for semi break-in; full break-in (QSK) is handled entirely
in FPGA timing, outside this polling loop (§5).

No WDSP calls are involved anywhere in keying — the only DSP-library keyer
calls found are the dead `DttSP.*` calls in the unbuilt `cwkeyer.cs`.

## 4. Thetis sidetone generation

Sidetone generation lives entirely in native C, not WDSP:
`Project Files/Source/ChannelMaster/sidetone.c [@852bf0e]` plus a thin C#
control layer in `console.cs`. `Project Files/Source/wdsp/` has zero
sidetone references — WDSP only receives the mixed monitor audio
downstream.

**Instantiation & defaults**, `ChannelMaster/cmaster.c:235-251 [@852bf0e]`:
```c
create_sidetone(i, 1, 0, pcm->xmtr[i].ch_outrate, pcm->xmtr[i].ch_outsize,
    pcm->xmtr[i].out[0], pcm->xmtr[i].out[2], pcm->xmtr[i].out[0], 0,
    400.0,   // pitch
    1.0, 1.0,// volume_sidetone, volume_tx
    20,      // wpm
    0,       // raised-cosine edge
    0.005);  // edge length (seconds)
```

**Waveform.** A quadrature recursive oscillator, `sidetone.c:31-39,172-191`:
`tone_delta = TWOPI*pitch/rate`, with `osc()` rotating `(cos_phs, sin_phs)`
by complex multiply each sample — a pure sinewave I/Q pair, no lookup table.

**Click suppression.** A raised-cosine envelope, precomputed into
`rise_samps`/`fall_samps` arrays, `sidetone.c:41-82`:
```c
a->nrise = (int)(a->edgelength * a->rate);
a->rise_samps[i] = 0.5 * (1.0 - cos(a->rise_theta));  // theta: 0 → PI
...
a->fall_samps[i] = 0.5 * (1.0 - cos(a->fall_theta));  // theta: PI → 0
```
Edge length default is **0.005 s (5 ms)**, edge type `0` = raised-cosine
(`cmaster.c:249-250`). A 4-state machine `LOW/RISE/HIGH/FALL`
(`sidetone.c:164-170,193-288`) drives per-sample envelope selection inside
`xsidetone()`, gated by `key`/`dot`/`dash` flags set via
`keySidetone`/`makedotSidetone`/`makedashSidetone` (`sidetone.c:318-343`),
under a critical section for thread safety.

**WPM timing**, `calc_wpm_times`, `sidetone.c:89-96`:
```c
dot_time = 1.2 / (double)a->wpm;  dash_time = 3.0 * dot_time;
a->n_dot_high  = (int)round((dot_time  - a->edgelength) * (double)a->rate);
a->n_dash_high = (int)round((dash_time - a->edgelength) * (double)a->rate);
```
Element hold time is shortened by the edge length so ramps don't inflate
dot/dash duration.

**Volume/mixing separation.** The same oscillator+key state feeds two
independently-scaled buffers each sample (`sidetone.c:265-282`): `out_st`
(monitor, `volume_st`) and `out_tx` (aliases the real TX chain buffer
`out[0]`, `volume_tx`, capped `<0.999` in `SetCWtxVolume`, line 369).
`run_st`/`run_tx` flags decide which is written, so internal-keyed CW can
make this tone *be* the transmitted signal, while QSK/hardware-keyed
operation only writes the monitor copy. Monitor mixing happens at
`cmaster.c:394`: `xMixAudio(0, 0, chid(stream,0), pcm->xmtr[tx].out[2]);
// mix monitor audio`, keeping it off the `out[0]` TX path unless `run_tx`
is set.

**C# layer.** `console.cs:10658 [@852bf0e]` gates `NetworkIO.SetSidetoneRun`
on `is_cw && _cw_sidetones && _cw_sw_sidetone`;
`console.cs:12976-13019 setCWSideToneVolume()` splits volume between
`SetCWSidetoneVolume` (hardware-board sidetone) and
`SetSidetoneVolume(0, vol/100f)` (this software path), forcing software
volume to 0 when QSK is active. No dedicated stereo-pan control exists for
the sidetone (I/Q pair only).

## 5. Thetis QSK / break-in timing

**Three-way enum**, `enums.cs:430-435 [@852bf0e]`:
```csharp
public enum BreakIn { Manual, Semi, QSK }
```
Set via `CurrentBreakInMode` (`console.cs:14683-14711`): `QSK` →
`QSKEnabled=true; NetworkIO.SetCWBreakIn(1)`; `Semi` → `QSKEnabled=false;
NetworkIO.SetCWBreakIn(1)`; `Manual` → `QSKEnabled=false;
NetworkIO.SetCWBreakIn(0)`.

**QSK (full break-in) is a separate path from the voice MOX machinery.**
`PollPTT()` (`console.cs:25393-25400`) — the loop driving `chkMOX.Checked`
for MIC/VOX/CAT/CW-semi PTT — is gated `!QSKEnabled`; when QSK is on,
per-dit/dah TX/RX switching happens entirely in Protocol-2 firmware
(comment `console.cs:34`: "Support QSK, possible with Protocol-2 firmware
v1.7… W2PA"), bypassing the software MOX state machine entirely. The
`QSKEnabled` setter (`console.cs:12930-12975`) forces `RX1AGCMode=CUSTOM`,
`AGCRX1HangThreshold=100` (if <70), `ATTOnTX=true` /
`SetupForm.ATTOnTX=31`, and **`BreakInDelay=0`** while QSK is active,
restoring saved non-QSK values on disable.

**Semi break-in reuses the voice MOX state machine.** `PollPTT()` line
25403: `cw_ptt = CWInput.KeyerPTT && _current_breakin_mode ==
BreakIn.Semi`; combined with `mic_ptt`, it sets `_current_ptt_mode =
PTTMode.CW` and `chkMOX.Checked = true` (line 25449-25454) — exactly the
same `chkMOX`/`HdwMOXChanged` path voice PTT uses. Release drops MOX only
when both `cw_ptt` and `mic_ptt` are false (line 25526-25532).

**Exact timer/delay constants** (preserve verbatim per project rules):
- `break_in_delay = 300` ms default, `console.cs:18453` (property
  `BreakInDelay`). Setter: `NetworkIO.SetCWHangTime((int)value +
  key_up_delay)` if break-in enabled, else `SetCWHangTime(0)`
  (`console.cs:18457-18463`) — a native firmware call
  (`HPSDR/NetworkIOImports.cs:288`).
- `key_up_delay = 10` ms, `console.cs:19638`.
- `mox_delay = 10` ms ("allows in-flight samples to clear"),
  `console.cs:19620`, used only for non-CW RX transition
  (`console.cs:29598-29599`).
- `rf_delay = 30` ms, `console.cs:19648`, used only for non-CW TX
  transition (`console.cs:29573-29574`).
- `ptt_out_delay = 20` ms ("time for HW to switch"), `console.cs:19655`,
  applied on every TX→RX (`console.cs:29608-29609`).
- `space_mox_delay = 0` ms, `console.cs:19630`, delay before TX→RX begins
  (`console.cs:29583-29584`).
- `non_qsk_breakin_delay` default `100` ms, `console.cs:12925` — fallback
  saved when entering QSK.

**RX-mute lead/lag for CW specifically.** In the TX→RX branch
(`console.cs:29583-29595`, quoted in full below), if `!cw_fw_keyer &&
key_up_delay>0`, the code sleeps `key_up_delay` before flipping DDCs back —
CW substitutes `key_up_delay` where other modes use `mox_delay`:
```csharp
if (space_mox_delay > 0)
    Thread.Sleep(space_mox_delay); // default 0 // from PSDR MW0LGE

_mox = tx;
psform.Mox = tx;
WDSP.SetChannelState(WDSP.id(1, 0), 0, 1);  // turn off the transmitter (no action if it's already off)

if (radio.GetDSPTX(0).CurrentDSPMode == DSPMode.CWL ||
    radio.GetDSPTX(0).CurrentDSPMode == DSPMode.CWU)
{
    if (!cw_fw_keyer && key_up_delay > 0)
        Thread.Sleep(key_up_delay);
}
else
{
    if (mox_delay > 0)
        Thread.Sleep(mox_delay); // default 10, allows in-flight samples to clear
}
```
In `HdwMOXChanged` (`console.cs:29052-29059`), when `cw_fw_keyer` is true
and mode is CWL/CWU and PTT mode isn't SPACE/CAT/CW, `NetworkIO.SetPttOut(0)`
is forced (firmware/QSK keyer owns RF switching), else `SetPttOut(1)`.

## 6. Wire protocol: how CW keying reaches the radio

**Thetis Protocol 1 (USB)** — `ChannelMaster/networkproto1.c`, function
`sendProtocol1Samples()`, lines 738-741 `[@852bf0e]`:
```c
if (prn->cw.cw_enable && j == 1)
    temp = (prn->tx[0].dot << 2 |
        prn->tx[0].dash << 1 |
        prn->tx[0].cwx) & 0b00000111;
```
Real-time key state (cwx/dot/dash) is **not** carried in the 5-byte C0-C4
C&C register — it's bit-stuffed into the low 3 bits of the outgoing L/R
"speaker audio" sample word (the `j==1` component) for near-zero-latency
keying, overwriting what would otherwise be audio data. Stock Thetis masks
to `0b00000111` (3 bits only) — no HL2/bit-3 branch exists in mainline.
Speed/weight/mode/sidetone **are** sent via true C&C register writes in the
same file: C0=0x16 → C3=`keyer_speed`(bits0-5)|mode bits,
C4=`keyer_weight`|strict_spacing (lines 605-628); C0=0x1e → C1=`cw_enable`,
C2=`sidetone_level`, C3=`rf_delay` (633-639); C0=0x20 →
hang_delay/sidetone_freq (641-646).

**Thetis Protocol 2 (ETH)** — `ChannelMaster/network.c`:
- `CmdHighPriority()` (port 1027), lines 927-933: byte 5 "CWX0" — bit0=CWX,
  bit1=Dot, bit2=Dash — sent immediately on every
  `SetCWX/SetCWDot/SetCWDash` call.
- `CmdTx()` (port 1026), lines 1190-1222: byte5=`mode_control` (bit1=CW,
  bit3=Iambic, bit4=Sidetone, bit5=ModeB, bit6=StrictSpacing,
  bit7=BreakIn), byte6=sidetone_level, byte7-8=sidetone_freq,
  byte9=keyer_speed, byte10=keyer_weight, byte11-12=hang_delay,
  byte13=rf_delay, byte17=edge_length.

So for P2, speed/weight/mode genuinely go via C&C; for P1 they do too (a
different register), but real-time keying itself is P1-audio-embedded /
P2-high-priority-packet, never a persistent register bit.

**Longpath's existing desk-review note** — no local copy of
`networkproto1.c` exists in this repo (`src/` was searched; none found);
the citation is to the external mi0bot-Thetis file, referenced verbatim in
`CLAUDE.md:660`, `docs/MASTER-PLAN.md:774-776`, and
`docs/architecture/phase3m-tx-epic-master-design.md:557-559`:

> "Absorbs the HL2 CWX bit-3 follow-up (`networkproto1.c:1247-1252
> [@c26a8a4]` — desk-review B3, "HL2 firmware uses bit 3 of I-low byte for
> CWX PTT, non-HL2 boards don't")."

Note also: a *different*, already-implemented HL2 workaround exists at
`src/core/P1RadioConnection.cpp:1567-1573` (clears I/Q low-byte LSB, citing
`deskhpsdr/src/old_protocol.c:2441-2453 [@120188f]`) — an unrelated fix that
must not be conflated with the CWX bit-3 PTT item above.

**What mi0bot-Thetis actually shows.** Checked out at `@0cef1c9`. The
fork's current tree has this logic at `networkproto1.c:1252-1261` — **not**
`1247-1252` as pinned in Longpath's docs (see §9, citation drift):
```c
if (prn->cw.cw_enable && j == 1)
    if (HPSDRModel == HPSDRModel_HERMESLITE)
        temp = (prn->tx[0].cwx_ptt << 3 |	// MI0BOT: Bit 3 in HL2 is used to signal PTT for CWX
                prn->tx[0].dot << 2 |
                prn->tx[0].dash << 1 |
                prn->tx[0].cwx) & 0b00001111;
    else
        temp = (prn->tx[0].dot << 2 |
                prn->tx[0].dash << 1 |
                prn->tx[0].cwx) & 0b00000111;
```
This extends the stock 3-bit mask to 4 bits only for
`HPSDRModel_HERMESLITE`, adding bit 3 = `cwx_ptt`, a separate PTT-for-CWX
flag driven by `NetworkIO.SetCWXPTT(int bit)`
(`NetworkIOImports.cs:321`), called from `Midi2CatCommands.cs:6399-6407`.
Backing pieces: `netInterface.c:1219-1226` (`SetCWXPTT`, comment "On the
HL2 the CWX protocol has been updated to pass PTT in Bit 3"); field
`cwx_ptt` in `network.h:268-269` ("CWX enhancement for PTT on HL2"). The C#
UI side gates identically — `cwx.cs:292-294` calls
`NetworkIO.SetCWXPTT()` **only** `if (HardwareSpecific.Model ==
HPSDRModel.HERMESLITE)`. It is the same low-byte-of-the-I/Q-sample scheme
as stock Thetis — not a separate C&C register.

mi0bot also carries two HL2-only timing constants, both commented
`MI0BOT: HL2`: `tx_latency = 20` and `ptt_hang = 12`
(`netInterface.c:1713-1714`, `[@0cef1c9]`), set via
`SetTxLatency`/`SetPttHang` (`NetworkIOImports.cs:384,387`) and packed into
C3/C4 in `networkproto1.c:1171`. HL2 also gets its own low-level write path
(`WriteMainLoop_HL2` vs `WriteMainLoop`, chosen at
`networkproto1.c:1264-1267` on the same model check). The keyer/QSK
algorithm itself is unchanged from mainline in this fork —
`cwkeyer.cs`, `CW/CWInput.cs`, `sidetone.c`, and `vox.c` contain zero
`HERMESLITE`/`HL2`/`MI0BOT` matches. The fork's CW divergence is confined
entirely to the wire-encoding and PTT-hang/latency layer.

## 7. What Longpath already has to build on

**MOX/PTT state machine — exists today.** `src/core/MoxController.h`/`.cpp`
(namespace `Longpath`): a 7-state QObject state machine (`MoxState`: Rx,
RxToTxRfDelay, RxToTxMoxDelay, Tx, TxToRxInFlight, TxToRxBreakIn,
TxToRxFlush) driven by 6 QTimers, ported from Thetis
`chkMOX_CheckedChanged2` (`console.cs:29311-29678 [@852bf0e]`). Central
entry point `void setMox(bool on)` (`MoxController.h:755`) runs:
BandPlanGuard check → TxInterlockPolicy check → `runMoxSafetyEffects(on)` →
idempotent guard → `moxChanging` (Pre) → commit `m_mox` → timer walk →
`hardwareFlipped(bool isTx)` / `txReady`/`rxReady` phase signals →
`moxChanged` (Post).

It already has 5 accepted PTT-source dispatch slots (`onMicPttFromRadio`,
`onCatPtt`, `onVoxActive`, `onSpacePtt`, `onX2Ptt`) plus two **rejected**
ones reserved for later phases:
```cpp
// MoxController.h:657-667
void onCwPtt(bool pressed);   // REJECTED (deferred to 3M-2)
void onTciPtt(bool pressed);  // REJECTED (deferred to 3J)
```
`onCwPtt` currently only `qCWarning`s and returns without touching state
(`MoxController.cpp:1298-1313`). `MoxState::TxToRxBreakIn` and
`kBreakInDelayMs`/`m_breakInDelayTimer` (300 ms — matching Thetis's
`break_in_delay` default, §5) are declared but never started — explicitly
reserved for 3M-2 CW QSK.

**DSPMode — CWL/CWU already exist, but only as demod modes.**
`src/core/WdspTypes.h:168-187`:
```cpp
enum class DSPMode : int { LSB=0, USB=1, DSB=2, CWL=3, CWU=4, FM=5, AM=6, DIGU=7, SPEC=8, DIGL=9, SAM=10, DRM=11, RADE_U=12, RADE_L=13 };
```
This is purely an RX/filter-sideband concept: `MoxController::isVoiceMode()`
explicitly excludes CWL/CWU from the VOX-gate voice family, and
`TxChannel::applyTxFilterForMode` maps CWL→LSB-family / CWU→USB-family IQ
sign for filtering only. There is no separate "CW keying" enum/class
anywhere yet — TX key-down gating is exactly the gap `onCwPtt`'s rejection
and the reserved `TxToRxBreakIn` state are waiting for.

**Voice TX → MOX → C&C bit today — the template to follow.**
`RadioModel::onMoxHardwareFlipped(bool isTx)` (`RadioModel.cpp:13726`)
subscribes to `MoxController::hardwareFlipped`, resolves Alex antenna
routing, then marshals the wire-bit write onto the connection thread:
```cpp
// RadioModel.cpp:13780-13784
auto* conn = m_connection;
QMetaObject::invokeMethod(conn, [conn, isTx]() {
    conn->setMox(isTx);      // Step 2 — P1 queues bank-0 flush; P2 sends immediate high-priority packet.
    conn->setTrxRelay(isTx); // Step 3 — P1 queues bank-10 flush; P2 not yet wired.
});
```
`RadioConnection::setMox` is pure-virtual (`RadioConnection.h:269-289`),
implemented per-protocol:
- **P1** (`P1RadioConnection.cpp:1344`): sets `m_forceBank0Next=true`
  (safety effect, every call), idempotent-guards on `m_mox==enabled`, else
  commits and arms the TX-I/Q ring pre-prime. The MOX bit is composed later
  in `composeCcBank0Full()`: `out[0] = m_mox ? 0x01 : 0x00` (C0 byte 3
  bit 0).
- **P2** (`P2RadioConnection.cpp:1094`): idempotent-guards, commits
  `m_mox`, arms a "MOX-off grace" retransmit window on release, then
  `sendCmdHighPriority()` immediately — bit 1 (0x02) of the high-priority
  byte 4.

**Existing TODO/stub trail, all pointing at 3M-2:**
- `P1CodecStandard.cpp:358` — `// C3 / C4 = CW keyer defaults — zero for
  Phase A; Phase 3M-2 wires CW.` (`out[3]=0; out[4]=0;`)
- `P1RadioConnection.cpp:1342` — `// CW gating (txmode == modeCWU/L branch
  from deskhpsdr:3596-3598) is 3M-2.`
- `TciProtocol.cpp:3170-3186` — `handleCwMacrosSpeedUpCommand`/
  `SpeedDownCommand`/`handleCwMsgCommand` are logged stubs: "Real handler
  lands in Phase 3M-2." (Macro handling itself remains out of this design's
  scope per §1 — noted only because the stub already exists.)
- `BandPlanGuard.cpp:522` — rejects CWL/CWU TX today with reason string
  `"CW TX coming in Phase 3M-2"`.
- `MainWindow.cpp:7096`, `PhoneCwApplet.h:228-231`, `TxApplet.h:253` — CWX
  applet / CW setup page instantiation commented out, "TODO 3M-2."
- `AudioEngine.h:865`/`.cpp:758` — `makeSidetoneOut()` PipeWire bus factory
  (`Role::Sidetone`) already exists but is generic infra, not yet wired to
  any keyer.

No duplication risk was found: the MOX state machine, its phase-signal
fan-out, and the P1/P2 wire-write pattern are all directly reusable. 3M-2
should add a CW-specific key-down path alongside them rather than
rebuilding any of this machinery.

## 8. Proposed Longpath architecture (sketch)

This is a sketch, not a plan — several choices below are flagged as needing
a live Thetis behavioral test before being locked in.

**A new `CwKeyer` class**, following the project's AetherSDR-skeleton +
Thetis-organs convention: a Longpath-native skeleton class owning keyer
state (mode Iambic-A/B/straight, speed, weight, break-in mode), with its
element-timing "organs" ported from whichever Thetis source turns out to
match Longpath's actual keying model — either the firmware-parameter push
calls (`NetworkIOImports.cs:273-321`, §3) if the target board's FPGA
implements the iambic engine, or the CWX-style software timing loop
(`cwx.cs`'s `wpmrate()`/`timeSetEvent` pattern, §3) if it must be
client-side. **Which of these two organ transplants is correct depends on
hardware capability that this research did not establish (§9) — the class
should be designed so the timing-source choice is swappable, not baked
in.**

**Plug-in to `MoxController`:**
- Semi break-in: unreject `onCwPtt`, following the existing
  `onMicPttFromRadio`-style dispatch pattern, driving the same timer-walk
  `setMox()` path voice PTT uses — this mirrors Thetis's own choice to
  reuse the MOX machinery for semi break-in (§5).
- Full QSK: Thetis's full QSK bypasses the software PTT poll entirely and
  lets firmware own TX/RX timing (§5, `PollPTT` gated `!QSKEnabled`).
  Longpath's `MoxController` is a single central state machine with no such
  bypass today. The reserved `MoxState::TxToRxBreakIn` and
  `kBreakInDelayMs=300` suggest the original design intended QSK to still
  run *through* `MoxController` rather than around it — this is a real
  design decision, not just wiring, and should be settled by checking what
  "sounds right" actually requires at the wire-protocol layer available to
  Longpath (P1 audio-embedded bits vs P2 high-priority packets, §6) before
  committing.

**Wire write**: a new virtual on `RadioConnection`, analogous to
`setMox`/`setTrxRelay` (§7) — e.g. a `setCwKey(bool)`-shaped call —
implemented per-protocol following the exact `P1RadioConnection.cpp:1344` /
`P2RadioConnection.cpp:1094` idempotent-guard-then-commit pattern, writing
into the currently-zeroed CW keyer bytes (`P1CodecStandard.cpp:358`) and,
for P1, embedding real-time dot/dash/cwx bits into the low bits of the
outgoing audio sample word per `networkproto1.c:738-741`. Whether Longpath
supports the HL2 4-bit `cwx_ptt` extension
(`networkproto1.c:1252-1261 [@0cef1c9]`, §6) is a per-board decision, not
universal.

**Sidetone mix point**: reuse the existing `AudioEngine::makeSidetoneOut()`
`Role::Sidetone` bus (`AudioEngine.h:865`/`.cpp:758`) rather than inventing
a new audio path. A sidetone generator (either inside `CwKeyer` or a
companion object) would port Thetis's NCO + raised-cosine-envelope logic
(`sidetone.c:31-96`, §4) and push into that bus — independent of WDSP,
exactly as in Thetis.

## 9. Open questions / risks

- **Firmware keyer vs client-side CWX is hardware-dependent and
  unresolved.** The research establishes that Thetis's iambic engine is
  FPGA-native on HPSDR/Hermes-class boards, and that the C# side is
  otherwise limited to parameter pushes or the CWX software path. It does
  not establish whether the operator's actual radios support the FPGA
  iambic keyer — this needs to be checked against the specific board(s)
  Longpath targets (P1/P2, HL2 or otherwise) before `CwKeyer`'s
  timing-source choice (§8) can be locked in.
- **Architectural mismatch: full QSK bypasses `MoxController` in Thetis,
  but Longpath centralizes all TX/RX switching through it.** Thetis routes
  semi break-in through the software MOX poll and full QSK entirely around
  it (firmware-timed). Longpath's `MoxController` has a reserved
  `TxToRxBreakIn` state suggesting an intent to keep QSK inside the state
  machine, but whether that can meet "sounds right" latency was not tested
  and is a real risk.
- **mi0bot-Thetis citation drift.** Longpath's existing desk-review note
  pins `networkproto1.c:1247-1252 [@c26a8a4]`; the actual `@0cef1c9`
  checkout has the same logic at lines 1252-1261. The pinned commit has
  drifted from what's citable today — the citation in
  `CLAUDE.md`/`MASTER-PLAN.md`/the phase3m epic doc should be re-verified
  against a current mi0bot-Thetis checkout before 3M-2 work starts.
- **Whether the HL2 bit-3 quirk applies at all depends on the operator's
  actual board.** The `cwx_ptt` bit-3 extension is gated strictly on
  `HPSDRModel_HERMESLITE` in both the fork's C and C# layers (§6). If
  Longpath's target hardware for this phase isn't a HermesLite-2, this
  entire quirk may be non-applicable scope, not a required feature — needs
  confirming against actual deployed/target hardware.
- **HL2 timing constants (`tx_latency=20`, `ptt_hang=12`) are HL2-only in
  mi0bot** and have no established equivalents for other boards in this
  research — if Longpath supports non-HL2 hardware for CW, those boards'
  correct timing constants are an open gap.
- **No WDSP involvement was confirmed for sidetone or keying** in either
  codebase — useful as a simplifying constraint, but worth a sanity check
  against Longpath's existing WDSP integration to make sure no assumption
  there depends on CW passing through it.
- **mi0bot-Thetis's working tree is a partial/promisor clone** (`git
  status` shows everything staged-deleted); all citations above were
  recovered via `git show`/`git grep` against present objects, not a live
  checkout — re-verify against a full clone before this becomes an
  implementation dependency.
- **No behavioral/timing test against real Thetis or real hardware has
  been done.** Every timing value and wire-format claim above is from
  static source reading. Before committing to a specific `CwKeyer` design,
  a live test (Thetis against real firmware, and/or the operator's actual
  radio) should confirm that the P1 audio-embedded bit path and/or P2
  high-priority packet path actually deliver the "sounds right" QSK
  latency this design assumes.

## 10. Suggested next step

A future implementation-plan doc should start by determining, against the
operator's actual radio hardware, whether the FPGA-native iambic keyer path
or the CWX-style client-side keying path is the one Longpath can rely on —
every other design choice in §8 depends on that answer.
