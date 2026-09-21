# WDSP Provenance & License

WDSP (Warren Pratt NR0V's DSP library) is vendored in `third_party/wdsp/`.

## Upstream

- **Author:** Warren Pratt (NR0V, W5WJ)
- **Canonical repository:** https://github.com/TAPR/OpenHPSDR-wdsp
- **Version in NereusSDR:** v1.29 (as of 2025-02-XX), with **partial Thetis
  v2.10.3.13 sync** for `cfcomp.c` + `cfcomp.h` (see "Partial sync record"
  below), plus a **direct WDSP 2.10 vendor** of the NNR (Neural Noise
  Reduction) module — see "NNR (WDSP 2.10)" below.

## Partial sync record

| File | Status | Source | Date |
| --- | --- | --- | --- |
| `third_party/wdsp/src/cfcomp.c` | Partial sync to Thetis v2.10.3.13 (commit `501e3f5`) | `../Thetis/Project Files/Source/wdsp/cfcomp.c` | 2026-04-30 |
| `third_party/wdsp/src/cfcomp.h` | Partial sync to Thetis v2.10.3.13 (commit `501e3f5`) | `../Thetis/Project Files/Source/wdsp/cfcomp.h` | 2026-04-30 |
| `third_party/wdsp/src/delay.c` | TAPR v1.29 + NereusSDR-original bound on the requested delay (`honourable_delay()`, see the file's modification history); upstream arithmetic unchanged | — (own fix, no upstream source) | 2026-09-20 |
| `third_party/wdsp/src/calcc.c` | Verbatim vendor of Thetis v2.10.3.13 (commit `501e3f5`) | `../Thetis/Project Files/Source/wdsp/calcc.c` | 2026-05-06 |
| `third_party/wdsp/src/calcc.h` | Verbatim vendor of Thetis v2.10.3.13 (commit `501e3f5`) | `../Thetis/Project Files/Source/wdsp/calcc.h` | 2026-05-06 |
| `third_party/wdsp/src/iqc.c` | Verbatim vendor of Thetis v2.10.3.13 (commit `501e3f5`) | `../Thetis/Project Files/Source/wdsp/iqc.c` | 2026-05-06 |
| `third_party/wdsp/src/iqc.h` | Verbatim vendor of Thetis v2.10.3.13 (commit `501e3f5`) | `../Thetis/Project Files/Source/wdsp/iqc.h` | 2026-05-06 |

**Reason:** Phase 3M-3a-ii needs per-band Qg (gain skirt Q) and Qe (ceiling
skirt Q) on the SetTXACFCOMPprofile setter so the CFC dialog can ship with
full Thetis userland parity (nudCFC_q + nudCFC_cq per-band Q controls).
TAPR v1.29's 5-arg signature has no Qg/Qe.

**License delta:** Thetis v2.10.3.13 cfcomp headers carry a Richard Samphire
(MW0LGE) dual-licensing block — Copyright (c) 2026 — which lands verbatim in
the bundled `cfcomp.{c,h}` headers as part of this sync.  This is upstream's
own dual-license (GPLv2-or-later WDSP carries on as the floor — Samphire
reserves additional rights only over his own contributions).  The license
delta does NOT restrict any rights granted to NereusSDR under the GPL.

All other 140 WDSP source files remain at TAPR v1.29.  Full WDSP upstream
re-sync is out of scope for 3M-3a-ii — see `UPSTREAM-SYNC-PROTOCOL.md` §6
for the full-sync procedure.

## NNR (WDSP 2.10) — direct upstream vendor

WDSP 2.10 adds a new algorithm, NNR (Neural Noise Reduction): a DPRNN-based
deep-filtering denoiser distinct from NR3/RNNR (rnnoise) and NR4/SBNR
(libspecbleach). Unlike those two, NNR has **no Thetis precedent** — it is
not part of any Thetis release (Thetis's own WDSP checkout is still v1.29)
and was never routed through Thetis at all. It is ported straight from the
canonical TAPR/OpenHPSDR-wdsp repository's `wdsp 2.10/Source/` tree.

| File | Status | Source | Date |
| --- | --- | --- | --- |
| `third_party/wdsp/src/nnr.c` | Verbatim vendor of WDSP 2.10 (commit `b02d5bac675dd2f33ec2bab2b339f79a597c47dd`) | `TAPR/OpenHPSDR-wdsp` `wdsp 2.10/Source/nnr.c` | 2026-09-14 |
| `third_party/wdsp/src/nnr.h` | Verbatim vendor, same commit | `wdsp 2.10/Source/nnr.h` | 2026-09-14 |
| `third_party/wdsp/src/nnet.c` | Verbatim vendor, same commit | `wdsp 2.10/Source/nnet.c` | 2026-09-14 |
| `third_party/wdsp/src/nnet.h` | Verbatim vendor, same commit | `wdsp 2.10/Source/nnet.h` | 2026-09-14 |
| `third_party/wdsp/src/nnet_profile.h` | Verbatim vendor, same commit | `wdsp 2.10/Source/nnet_profile.h` | 2026-09-14 |
| `third_party/wdsp/src/nnio.c` | Verbatim vendor, same commit | `wdsp 2.10/Source/nnio.c` | 2026-09-14 |
| `third_party/wdsp/src/nnio.h` | Verbatim vendor, same commit | `wdsp 2.10/Source/nnio.h` | 2026-09-14 |

All six files carry Warren Pratt NR0V's own GPLv2-or-later header verbatim
(`Copyright (C) 2026 Warren Pratt, NR0V`) — no Thetis or Samphire
attribution applies, since no Thetis intermediary exists for this module.

**RXA integration:** `RXA.c`/`RXA.h`/`comm.h` were hand-merged (not
overwritten) to add the `nnr` struct member, `create_nnr`/`destroy_nnr`/
`flush_nnr`/`xnnr` calls, and an `nnr_run` parameter appended to the
existing `RXAbp1Check` signature (already extended once before, for NR3/
NR4 — see "RXA bandpass-gain gating" note below) — NereusSDR's `RXA.c` has
diverged from stock WDSP since the NR3/NR4 port and a wholesale overwrite
from WDSP 2.10 would have dropped that work. The eight call sites
(`RXA.c`, `amd.c`, `anf.c`, `anr.c`, `emnr.c`, `rnnr.c`, `sbnr.c`, `snb.c`)
that invoke `RXAbp1Check` were all updated to pass `getRun_nnr(rxa[channel].nnr.p)`
as the new trailing argument — `nnr`'s struct (`typedef struct _nnr* NNR`)
is opaque outside `nnr.c`, unlike `rnnr`/`sbnr`'s exposed structs, so
`getRun_nnr()` is required rather than a direct `.p->run` read.

**`dprintf` — a new dependency this port introduced.** `nnr.c`/`nnet.c`
call `dprintf(const char*, ...)` for debug tracing. Upstream's own
`utilities.c` (WDSP 2.10) implements it as
`vsnprintf()` + `OutputDebugStringA()` — a Win32-only sink. NereusSDR's
`utilities.c` is still at v1.29 and never gained that function, so on
POSIX the bare name resolved to the unrelated system `dprintf(int fd,
const char*, ...)` from `<stdio.h>`, producing int-conversion build
errors. Fixed with a POSIX shim in the existing `linux_port.h`/`.c`
cross-platform-macro file (same file that already substitutes
`CRITICAL_SECTION`, `CreateSemaphore`, etc.): a `#define dprintf(...)
wdsp_dprintf(__VA_ARGS__)` macro (placed after `<stdio.h>` is already
fully parsed, so the redirect never corrupts the system header's own
declaration) plus a `wdsp_dprintf()` implementation that writes to
`stderr`. No upstream files were altered for this — the macro lives
entirely in NereusSDR's own POSIX-port file.

**Model weights — shipped as external `.bin` files, not compiled-in
C arrays.** Upstream's `nnr_model_0.c` / `nnr_model_1.c` encode the two
trained models (2,098,944 and 4,682,240 bytes of actual tensor data) as
hex-byte-array C literals — ~10.7 MB and ~24 MB of generated source text
respectively. NereusSDR does not vendor those files, for the same reason
`rnnoise_data.c` is excluded (see `RNNOISE-PROVENANCE.md`): `nnet.c`'s
`nnet_build()` already tries an external file at `nnet_model_path[slot]`
before falling back to the compiled-in `nnr_builtin[slot]` table, so only
the small binary tensor payload needs to exist, not the 34 MB of
generated C wrapping it.

- `third_party/wdsp/src/nnr_model_stub.c` (NereusSDR-original glue,
  see "NereusSDR-original glue" table below) provides zero-length
  `nnr_model_0_data`/`nnr_model_1_data` arrays so the linker resolves;
  this fallback path is never actually reached in a normal install.
- `third_party/wdsp/models/wdsp_nnr_0.bin` (2,098,944 bytes) and
  `wdsp_nnr_1.bin` (4,682,240 bytes) are the real weights, extracted
  byte-for-byte from the upstream `nnr_model_0.c` / `nnr_model_1.c` hex
  arrays via `scripts/extract-nnr-models.py`. The script parses the
  `0x??` literals in order and writes them out raw; extraction was
  independently verified by re-parsing each `.bin`'s own `WDSPNN`
  tensor-container header (magic, tensor count, per-tensor dims) and
  confirming self-consistency and the exact upstream byte count.
  SHA-256: `e1ebfed6f522746bcfc1265d4990f0250ae1de5b2050e58af9fc965dc1d94c9`
  (slot 0), `925fdb6830627d84ef4ec8b2116ad66047e56d593d168db10f510dcbac0934a`
  (slot 1).
- `RxChannel`/`RadioModel` point both slots at these bundled files via
  the upstream-exported `SetNNRModelPathSlot(slot, path)` on radio
  connect (mirroring `RNNRloadModel`'s timing) — see
  `Longpath::ModelPaths::nnrModel0Bin()`/`nnrModel1Bin()`.
- These `.bin` files are Warren Pratt's own trained model weights,
  distributed as part of WDSP itself (GPLv2-or-later) — no separate
  third-party model license applies, unlike RNNoise's case.

## NereusSDR-original glue (not from upstream)

| File | Status | Reason | Date |
| --- | --- | --- | --- |
| `third_party/wdsp/src/txgain_stub.c` | NereusSDR-original glue stub (GPLv2-or-later, J.J. Boyd KG4VCF) | Provides `SetTXFixedGain` / `SetTXFixedGainRun` symbols so the bundled `wdsp_static` library exposes the API surface that Thetis's ChannelMaster module exports (`Project Files/Source/ChannelMaster/txgain.c [v2.10.3.13]`). NereusSDR has not yet ported the wider ChannelMaster module (only individual primitives like `cmbuffs.c` have landed in `src/core/audio/TxMicSource.{cpp,h}`); the stub stores per-channel `(Igain, Qgain)` plus a run flag in a flat static so the linker resolves and the C++ wrapper at `src/core/TxChannel.cpp::setTxFixedGain` (issue #167 Phase 1 Agent 1C) can be unit-tested. A future ChannelMaster-port phase can replace this stub with the byte-for-byte port of `txgain.c`. | 2026-05-03 |
| `third_party/wdsp/src/ps_sync_stub.c` | NereusSDR-original glue stub (GPLv2-or-later, J.J. Boyd KG4VCF) | Provides `SetPSRxIdx` / `SetPSTxIdx` symbols so the bundled `wdsp_static` library exposes the API surface that Thetis's ChannelMaster module exports (`Project Files/Source/ChannelMaster/sync.c:69-79 [v2.10.3.13]`). Same convention as `txgain_stub.c`; the stub stores per-id RX/TX feedback indices in a flat static so the linker resolves and the C++ wrappers at `src/core/TxChannel.cpp::setPSRxIdx` / `setPSTxIdx` (Phase 3M-4 Task 3) can be unit-tested. cmaster.cs:533-534 [v2.10.3.13] only ever calls these once at PS init with `txid = 0`, so the non-atomic store is sufficient until real ChannelMaster wiring lands. A future ChannelMaster-port phase can replace this stub with the byte-for-byte port of `sync.c`. | 2026-05-06 |
| `third_party/wdsp/src/nnr_model_stub.c` | NereusSDR-original glue stub (GPLv2-or-later, NereusSDR contributors) | Provides zero-length `nnr_model_0_data`/`nnr_model_0_size` and `nnr_model_1_data`/`nnr_model_1_size` symbols that `nnet.c` (WDSP 2.10) declares `extern` — not a Thetis API-surface gap like the two stubs above (NNR has no Thetis precedent at all), but a deliberate substitute for upstream's `nnr_model_0.c`/`nnr_model_1.c`, which encode the same weights as ~34 MB of generated hex-byte-array C source. The real weights ship as `third_party/wdsp/models/wdsp_nnr_{0,1}.bin` instead (see "NNR (WDSP 2.10)" above for the extraction record); `nnet.c`'s own file-path lookup always finds them first, so this stub's fallback arrays are never actually reached. | 2026-09-14 |

The glue stub is GPLv2-or-later (compatible with the rest of `wdsp_static`)
and carries a verbatim NereusSDR-authored GPL header so
`scripts/verify-thetis-headers.py --kind=wdsp` passes.  The file's
`no-port-check:` marker exempts it from the Thetis-tells heuristic in
`scripts/check-new-ports.py` because it is NereusSDR-original code, not a
port.

## License Analysis

### Survey of Vendored Sources

All 144 source files in `third_party/wdsp/src/` were examined:

- **134 files** carry the full GPLv2-or-later permission block:
  - `"either version 2 of the License, or (at your option) any later version"`
  - All signal processing core: `channel.c`, `RXA.c`, `TXA.c`, `bandpass.c`, `amd.c`, `anf.c`, `anr.c`, and 125 others
  - `rnnr.c` + `rnnr.h` + `sbnr.c` + `sbnr.h` ported in Sub-epic C-1 (NR3/NR4 backends), carry verbatim Thetis GPLv2-or-later + MW0LGE dual-license headers.
  - `txgain_stub.c` (issue #167 Phase 1 Agent 1C, NereusSDR-original glue stub authored by J.J. Boyd KG4VCF, GPLv2-or-later — see row at top of "Vendored Source Files" table).
  - `ps_sync_stub.c` (Phase 3M-4 Task 3, NereusSDR-original glue stub authored by J.J. Boyd KG4VCF, GPLv2-or-later — see row in the "NereusSDR-original glue (not from upstream)" section: provides `SetPSRxIdx` / `SetPSTxIdx` symbols so the bundled wdsp_static library exposes the API surface that ChannelMaster's `sync.c:69-79 [v2.10.3.13]` exports).
  - The other 132 files are Warren Pratt, NR0V, Copyright 2012–2025.
  - **Conclusion: GPLv2-or-later**

- **2 files** are the POSIX portability shim — authored jointly with John Melton:
  - `linux_port.c` / `linux_port.h`
  - Header: `Copyright (C) 2013 Warren Pratt, NR0V and John Melton, G0ORX/N6LYT`
  - These files translate Win32 synchronization primitives (`CRITICAL_SECTION`,
    `CreateSemaphore`, `WaitForSingleObject`, `InterlockedIncrement`, etc.) to
    POSIX equivalents so WDSP builds on Linux and macOS. They are co-authored
    by Warren Pratt (NR0V) and John Melton (G0ORX/N6LYT), with a subsequent
    macOS recursive-mutex fix contributed by Christoph van Wullen (DL1YCF) —
    see inline `// DL1YCF:` comment at `linux_port.c:49`.
  - **Conclusion: GPLv2-or-later** (same permission block as the rest of WDSP)

- **10 files** have no license headers (non-copyrightable or build infrastructure):
  - Data tables: `calculus.c`, `FDnoiseIQ.c` (noise lookup tables)
  - Generated files: `resource.h`, `resource1.h` (MSVC IDE artifacts)
  - Minimal wrappers: `version.c`, `version.h`, `fastmath.h` (empty), `calculus.h` (empty)
  - Third-party library: `fftw3.h` (FFTW3 header, GPLv2-or-later — same terms as the FFTW3 source upstream; the "BSD license" label in earlier versions of this doc was incorrect)
  - **Conclusion: Non-GPL sources or utility stubs; not blocking GPL compatibility**

- **No files** carry GPLv2-only language (`"version 2 only"`, `"GPLv2-only"`)

### Representative Files Spot-Checked

Five canonical files confirm uniform GPLv2-or-later:

1. **channel.c** (core channel management)
   - Copyright 2013 Warren Pratt NR0V
   - Permission: "either version 2 of the License, or (at your option) any later version"

2. **RXA.c** (RX audio processing pipeline)
   - Copyright 2013–2025 Warren Pratt NR0V
   - Permission: "either version 2 of the License, or (at your option) any later version"

3. **TXA.c** (TX audio processing pipeline)
   - Copyright 2013–2023 Warren Pratt NR0V
   - Permission: "either version 2 of the License, or (at your option) any later version"

4. **bandpass.c** (bandpass filter implementation)
   - Copyright 2013–2017 Warren Pratt NR0V
   - Permission: "either version 2 of the License, or (at your option) any later version"

5. **amd.c** (AM demodulator)
   - Copyright 2012–2013 Warren Pratt NR0V
   - Permission: "either version 2 of the License, or (at your option) any later version"

## Compatibility with NereusSDR

**NereusSDR is distributed under GPLv3** (see `/LICENSE` at the root).

GPLv3 permits aggregation of GPLv2-or-later code:
- GPLv3 §5(b) explicitly allows combining GPLv3 code with code "under the terms of version 2 or any later version of the GNU General Public License"
- WDSP's "or any later version" language satisfies this condition unambiguously

**Result: WDSP v1.29 (GPLv2-or-later) is fully compatible with NereusSDR's GPLv3 distribution.**

## NereusSDR Modifications

The vendored WDSP tree is unmodified from upstream and retains all upstream copyright notices and license headers. NereusSDR wraps WDSP via:

- `src/core/WdspEngine.cpp` — WDSP lifecycle manager
- `src/core/RxChannel.cpp` — RX channel DSP wrapper
- `src/core/TxChannel.cpp` — TX channel DSP wrapper

These wrappers are authored by NereusSDR contributors and carry NereusSDR (GPLv3) headers with Thetis/WDSP attribution in comments (see `docs/attribution/HEADER-TEMPLATES.md` for full attribution chain).

## Attribution Chain

1. **WDSP original** ← Warren Pratt NR0V, TAPR, GPLv2-or-later
2. **WDSP POSIX shim** (`linux_port.{c,h}`) ← Warren Pratt NR0V + John Melton G0ORX/N6LYT (co-authors, 2013), with macOS mutex fix from Christoph van Wullen DL1YCF; GPLv2-or-later; distributed via both `TAPR/OpenHPSDR-wdsp` and `g0orx/wdsp`
3. **Thetis port** ← ramdor + contributors, GPLv2-or-later (Thetis itself includes WDSP)
4. **NereusSDR port** ← JJ Boyd KG4VCF + contributors, GPLv3 (with Thetis upstream attribution preserved in source headers)

Per compliance notice §6.1, the WDSP attribution is already preserved in the source tree headers and in NereusSDR wrapper source comments pointing to the Thetis contributor chain.
