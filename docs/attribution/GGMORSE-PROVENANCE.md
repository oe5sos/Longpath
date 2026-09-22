# ggmorse Provenance — Longpath vendored-library inventory

This document catalogs the vendored ggmorse Morse-code decoder shipped
under `third_party/ggmorse/`. The library is consumed exclusively through
the C++ wrapper at `src/core/CwDecoder.{h,cpp}` (itself a port of
AetherSDR's `src/core/CwDecoder.{h,cpp}`; see
`docs/attribution/AETHERSDR-PORTS.md` and `aethersdr-reconciliation.md`,
Nachtrag 2026-09-21).

Longpath is distributed under GPLv3 (root `LICENSE`). ggmorse is MIT,
fully GPL-compatible.

## Upstream

- **Project:** ggmorse
- **Repository:** https://github.com/ggerganov/ggmorse
- **Primary author:** Georgi Gerganov (`ggerganov`)
- **Pinned SHA:** `8fb433d6cd6a71940f51b5724663ec0c75bf0b62`
  ("ggmorse : get threshold level + build fix (#13)")
- **Pinned date:** `2024-05-31`
- **Vendored:** `2026-09-21`
- **Languages:** C++ (one library translation unit, one resampler, five
  headers)

## License

ggmorse is distributed under the **MIT License**. The verbatim upstream
`LICENSE` is preserved at `third_party/ggmorse/LICENSE`. MIT is
GPL-compatible by its own terms; no special obligations apply when
Longpath statically links ggmorse into `LongpathObjs`.

## Files vendored from ggmorse

All files are copied **byte for byte** from upstream at the pinned SHA.
They live under `third_party/ggmorse/`:

| Upstream file | Longpath path |
|---|---|
| `LICENSE` | `third_party/ggmorse/LICENSE` |
| `include/ggmorse/ggmorse.h` | `third_party/ggmorse/include/ggmorse/ggmorse.h` |
| `src/ggmorse.cpp` | `third_party/ggmorse/src/ggmorse.cpp` |
| `src/resampler.cpp` | `third_party/ggmorse/src/resampler.cpp` |
| `src/resampler.h` | `third_party/ggmorse/src/resampler.h` |
| `src/fft.h` | `third_party/ggmorse/src/fft.h` |
| `src/filter.h` | `third_party/ggmorse/src/filter.h` |
| `src/goertzel.h` | `third_party/ggmorse/src/goertzel.h` |
| `src/stfft.h` | `third_party/ggmorse/src/stfft.h` |

Not vendored: upstream's `CMakeLists.txt`, `cmake/`, `examples/`,
`tests/`, `media/`, `README*`, `CHANGELOG.md`.

## Longpath-added files (not upstream)

| Path | Purpose |
|---|---|
| `third_party/ggmorse/CMakeLists.txt` | Static library target `ggmorse` with upstream warnings silenced |
| `third_party/ggmorse/COMMIT` | The pinned SHA |
| `third_party/ggmorse/README-LONGPATH.md` | Pointer to this record |

## Local modifications

**None.** AetherSDR carries two local patches in its own copy (Nordic
letters Æ/Ø/Å in the code table; a speed search widened to 115 WPM in
2-WPM steps). Longpath deliberately vendors the pristine upstream instead,
so this record can claim byte identity; the decoder's speed range is
therefore ggmorse's own 5–55 WPM.

## Known behaviour

ggmorse echoes every decoded character to `stdout` (`src/ggmorse.cpp`,
the `printf` calls in the frame analysis). Harmless — Longpath's decoded
text reaches the applet through `GGMorse::takeRxData()`, not through
that echo — and left as is, for the byte-identity above.

## Re-syncing

1. Fetch the new upstream commit into a scratch clone.
2. Copy the nine files above; nothing else.
3. Update `COMMIT`, this record, and `third_party/ggmorse/CMakeLists.txt`'s
   header comment.
4. Run `tests/tst_cw_decoder` (synthetic keyed CW must still decode).
