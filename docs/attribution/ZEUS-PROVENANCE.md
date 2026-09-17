# Zeus station-engine Provenance — NereusSDR derived-file inventory

This document catalogs every NereusSDR source file derived from, translated
from, or materially based on the Zeus station engine (Zeus-SDR/station-engine).
Per-file license headers and modification-history blocks live in the source
files themselves; this index is the grep-able summary.

NereusSDR is distributed under GPLv3 (root `LICENSE`). The Zeus station engine
is GPL-2.0-or-later (most first-party files) with GPL-3.0 components — fully
compatible. See §License below.

## When entries get added

A row is added to the table below — in the **same commit** that introduces
the ported logic — whenever a NereusSDR file:

1. Ports, translates, or materially re-expresses logic from any file in the
   Zeus station-engine repository, AND
2. That logic is not already covered by the Thetis / WDSP lineage (i.e. Zeus
   is the *primary* source for that logic, not a cross-reference).

The procedure is identical to `THETIS-PROVENANCE.md` / `DESKHPSDR-PROVENANCE.md`:

- Keep the upstream file header (for `native/wdsp/*` that is Warren Pratt's
  WDSP header, which the vendored file already carries) and add a
  `Modification history (NereusSDR)` block naming Zeus as the source.
- Add a `// From Zeus station-engine <path>:<line> [@<sha>]` inline cite at
  every ported function (per `HOW-TO-PORT.md` §"Inline cite versioning").
- Add a PROVENANCE row here with the NereusSDR file, Zeus source, line
  ranges, derivation type, and notes.

## Upstream

- **Project:** Zeus station engine (`station-engine`) — the headless
  OpenHPSDR Protocol-1/2 station process underneath Zeus SDR. **Only the
  engine is open**; the Zeus SDR client is a separate, proprietary program
  and is never a source for NereusSDR.
- **Repository:** https://github.com/Zeus-SDR/station-engine
- **Maintainers / authors:** Douglas J. Cerrato (KB2UKA) and Christian
  Suarez (N9WAR), and contributors.
- **Reference SHA:** `@324e865` (Release v2.0.19, HEAD at the time of the
  first Zeus port, 2026-09-17). Local clone: `../zeus-station-engine`.
- **Language:** C# (.NET 10) for the engine; C for `native/wdsp` (Zeus's fork
  of WDSP 2.1.0 with the patches listed in `native/wdsp/ZEUS-PATCHES.md`).

## License

The repository's `LICENSE` states: most first-party engine source is
**GPL-2.0-or-later**; `Station.Engine.Hosting/SpeTaurus/` is GPL-3.0-or-later;
a Windows distribution containing the Steinberg ASIO SDK-derived bridge is
GPL-3.0-only; `Station.AudioRing` is additionally MIT. `native/wdsp/*` files
keep Warren Pratt's GPL-2.0-or-later WDSP header; Zeus's modifications to them
are GPL-2.0-or-later.

NereusSDR is GPLv3. GPL-2.0-or-later → GPLv3: **compatible** (the "or later"
option is exercised). GPL-3.0-only components would also be compatible with
NereusSDR's GPLv3 but none have been ported.

**Trademarks:** "Zeus" and "ZeusSDR" are trademarks of the maintainers
(`TRADEMARK.md`). The GPL grants no rights to those names; nominative use
("ported from the Zeus station engine") is permitted and is all this document
does. Nothing in NereusSDR may be presented as Zeus or as affiliated with it.

## Legend

Derivation type:
- `port`       — logic re-expressed in NereusSDR from a Zeus source file
- `verbatim`   — byte-for-byte copy of a Zeus function or block
- `reference`  — Zeus consulted for behaviour or design only; no code taken

## Inventory

| NereusSDR file | Zeus source | Lines | Type | Notes |
| --- | --- | --- | --- | --- |
| `src/core/CwDecoderCore.h` | `Station.Engine.Hosting/CwDecoder/GoertzelDetector.cs`, `AdaptiveThreshold.cs`, `MorseTimingEstimator.cs`, `MorseFsm.cs`, `CwDecoderCore.cs` | full | port | Receive-side CW decoder: eleven-bin Goertzel bank with confirmed retune, adaptive key-on/key-off threshold with noise-floor reacquire, lower-cluster dit estimator, Morse FSM with per-character confidence. Ported 2026-09-17 @8970f2d (v2.0.26). Two documented deviations in `CwMorseTimingEstimator` (letter/word gap thresholds 2.0/5.0 dits instead of 3.0/5.5; displayed WPM from the mean of tone and element-gap clusters) — marked "Longpath deviation"/"Longpath addition" at the site, found and pinned by `tests/tst_cw_decoder.cpp`. |
| `src/core/CwDecoderCore.cpp` | same five files | full | port | Implementation; every function cites its upstream file and lines. |
| `src/core/CwDecoder.{h,cpp}` | — | — | reference | Longpath-original Qt wrapper (stereo tap → mono → core, signals at the Zeus 10 Hz status cadence). No ported code. |
| `src/gui/applets/CwDecoderApplet.{h,cpp}` | — | — | reference | Longpath-original applet modelled on Longpath's own `RttyDecoderApplet`; shows what Zeus's CW Console shows (tone lock, WPM, SNR) but shares no code with it. |
| `third_party/wdsp/src/delay.c` | `native/wdsp/delay.c` | 29-38, 55, 122-128 | port | `set_delay_value_unlocked()`: clamps the requested delay to `(WSDEL-1)*L + (L-1)` phases so `snum` can never exceed the ring (`rsize = cpp + WSDEL - 1`) — upstream WDSP computed it unbounded and `xdelay()` wraps its read index once, so a delay past `(WSDEL-1)/rate` (5.3 ms at 192 kHz; the PureSignal amp-delay field allows 25 ms) read beyond the ring's allocation. `tdelay` now holds the realised value and `SetDelayValue()` returns it. Ported 2026-09-17 @324e865; regression test `tests/tst_wdsp_delay_clamp.cpp`. Brace style adapted to the vendored file (Allman, tabs); arithmetic unchanged. |

## Design references (no code taken)

- The Zeus CW Console (2.0.26 changelog #2181/#2183: keyer + decoder +
  macro banks with decoded speed and SNR) is the feature the CW decoder
  applet answers; only the engine-side decoder was open and is ported
  above, the keyer we already had (`CwxApplet`), the macro banks are not
  built.

- `docs/design/2026-09-17-zeus-plugin-system-inventar.md` — the plugin
  system, read for architecture only; the non-finite guard in
  `src/core/strip/StripChain.cpp` (PR #14) is NereusSDR-original and takes
  a different approach (restore + reset per stage) than Zeus's
  `AudioChain.RepairNonFiniteSamples` (zero per slot).
- `docs/design/2026-09-17-zeus-wdsp-fork-inventar.md` — Zeus's WDSP fork
  against upstream 2.1.0, Thetis and NereusSDR; the delay.c row above is
  the first port to come out of it.
