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
- **SHA-256 of the vendored files (verbatim, CRLF preserved):**
  - `SGP4.cpp` `2ee7ad0e8f201e8251894083fe21e33a7aace2f43c871bf04357eb44a891b06e`
  - `SGP4.h`   `2a5ec44e059a52b3173d78d9a28bda8142b4f6497c1eb16febc2cea4b5006b0c`

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
| `extension/SGP4.cpp` | `third_party/sgp4/SGP4.cpp` | none (verbatim) |
| `extension/SGP4.h` | `third_party/sgp4/SGP4.h` | none (verbatim) |
| `LICENSE` | `third_party/sgp4/LICENSE.txt` | none |

`third_party/sgp4/CMakeLists.txt` is Longpath-added (STATIC library so it
reaches every executable that links the object library; warning
suppression for the vendored code).

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
Everything else in the file is unused but shipped verbatim.
