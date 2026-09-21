# ggmorse (vendored)

Upstream: https://github.com/ggerganov/ggmorse — Georgi Gerganov, MIT License
(see `LICENSE`, verbatim). Pinned commit: see `COMMIT`
(`8fb433d6cd6a71940f51b5724663ec0c75bf0b62`, "ggmorse : get threshold level +
build fix (#13)", 2024-05-31).

Only the library is vendored (`include/ggmorse/ggmorse.h`, `src/*`); the
examples, tests, CMake and media of the upstream repository are not. Every
file is byte-identical to upstream at the pinned commit — no Longpath
patches. Consumed through `src/core/CwDecoder.{h,cpp}`.

Provenance record: `docs/attribution/GGMORSE-PROVENANCE.md`.
