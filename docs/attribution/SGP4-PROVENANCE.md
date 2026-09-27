# SGP4 Provenance — Longpath vendored-library inventory

This document catalogs the SGP4/SDP4 orbit propagator shipped under
`third_party/sgp4/`. Longpath uses it for one thing: which amateur
satellites are above the operator's horizon — at the moment a QSO is
logged (stamped into the log entry) and live on the QSO map. The library
is consumed exclusively through `src/core/sat/SatelliteTracker.{h,cpp}`
(Longpath-original: TLE bookkeeping, observer geometry, sub-satellite
points).

Longpath is distributed under GPLv3 (root `LICENSE`). The vendored code is
MIT-licensed as part of the python-sgp4 project; the underlying reference
implementation is published by its author for free use. Both are
GPL-compatible.

## Upstream

- **Algorithm / reference code:** David A. Vallado, "SGP4 Version
  2020-07-13", companion code to *Fundamentals of Astrodynamics and
  Applications* (2013), following Vallado, Crawford, Hujsak, Kelso,
  "Revisiting Spacetrack Report #3", AIAA 2006-6753
  (https://celestrak.org/publications/AIAA/2006-6753/). Vallado publishes
  the code at https://celestrak.org/software/vallado-sw.php for free use
  without warranty.
- **Vendored from:** python-sgp4 by Brandon Rhodes,
  https://github.com/brandon-rhodes/python-sgp4, files
  `extension/SGP4.cpp` and `extension/SGP4.h`, which carry Vallado's code
  verbatim (with the `_SGP4` suffixes on the helper functions).
- **Pinned commit:** `bf25b00ccf8cf0770a8e5ba458134156f1c70812`
- **Vendored:** 2026-09-21
- **SHA-256 of the upstream files at the pinned commit (CRLF preserved):**
  - `SGP4.cpp` `2ee7ad0e8f201e8251894083fe21e33a7aace2f43c871bf04357eb44a891b06e`
  - `SGP4.h`   `2a5ec44e059a52b3173d78d9a28bda8142b4f6497c1eb16febc2cea4b5006b0c`
- **SHA-256 of `SGP4.cpp` as shipped** (upstream plus the one local patch
  under "Local modifications"):
  `3bf14d453585c016285726c0ba66aa953da1ff9bd8ff500d440293cef5c9fbbe`.
  `SGP4.h` ships unchanged. Upstream `master` was still byte-identical to
  the pinned `SGP4.cpp` on 2026-09-27 (last change there: `fac882aed0`,
  2023-07-02), so the patch is not superseded upstream.

## License

python-sgp4 is distributed under the **MIT License** (Copyright ©
2012–2016 Brandon Rhodes); the verbatim upstream `LICENSE` is preserved
at `third_party/sgp4/LICENSE.txt`. The C++ files themselves carry
Vallado's header and no separate licence text; his published terms are
"provided freely, without warranty" and the code is used unchanged in
GPL and commercial software alike. No obligations beyond keeping the
notices apply when Longpath statically links the object library.

## Files vendored

| Upstream file | Longpath path | Changes |
|---|---|---|
| `extension/SGP4.cpp` | `third_party/sgp4/SGP4.cpp` | one local patch, 2026-09-27 (see "Local modifications") |
| `extension/SGP4.h` | `third_party/sgp4/SGP4.h` | none (verbatim) |
| `LICENSE` | `third_party/sgp4/LICENSE.txt` | none |

`third_party/sgp4/CMakeLists.txt` is Longpath-added (STATIC library so it
reaches every executable that links the object library; warning
suppression for the vendored code).

## Local modifications

**2026-09-27 — skip the self-copy of the satellite number in
`SGP4Funcs::sgp4init`.** `twoline2rv` fills `satrec.satnum` from the TLE
and then calls `sgp4init(whichconst, opsmode, satrec.satnum, …)`;
`sgp4init` copies `satn` into `satrec.satnum` with `strcpy` (`strcpy_s`
under MSVC). Source and destination are the same buffer, which is
undefined behaviour for both functions. It is harmless with the libc
implementations Longpath is built against, but AddressSanitizer reports
it as `strcpy-param-overlap` and aborts: a full Debug/ASAN test run lost
23 of 906 tests to it — `tst_satellite_tracker` and every test that
builds a `MainWindow`, because that starts the satellite service.

The copy now runs only when `satn` is a different buffer
(`if (satn != satrec.satnum) { … }` around the upstream `#ifdef` block);
a caller that passes its own number still gets it copied. The patch is
marked in the file with a `Longpath local patch (2026-09-27)` comment,
keeps the file's tabs and CRLF line endings, and changes nothing else.
Martin Fischer, AI-assisted via Anthropic Claude.

When re-vendoring from python-sgp4: check whether upstream has fixed the
self-copy; if not, re-apply this patch and update the shipped hash above.

## What Longpath does NOT take from elsewhere

The satellites-in-view idea appears in other logging software. Longpath's
implementation around the propagator — TLE parsing into a catalogue,
CelesTrak fetch and cache (`TleStore`), observer geometry (geodetic to
TEME, SEZ transform, azimuth/elevation/range), sub-satellite points, the
ADIF stamp `APP_LONGPATH_SATS` and the map layer — is written for
Longpath; no other program's code was consulted for it.

## API used

`SGP4Funcs::twoline2rv` (typerun `'c'`, opsmode `'i'`, `wgs72`),
`SGP4Funcs::sgp4`, `SGP4Funcs::gstime_SGP4`, `SGP4Funcs::jday_SGP4`.
Everything else in the file is unused but shipped as upstream has it.
