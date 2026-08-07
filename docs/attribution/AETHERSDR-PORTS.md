# AetherSDR whole-file ports

AetherSDR (https://github.com/aethersdr/AetherSDR) is GPLv3, as is
NereusSDR, so ported code carries forward under the same licence with
attribution per GPLv3 §5. Every file below has the project-level
AetherSDR copyright block in its own header; this table is the index.

Upstream reference: `../AetherSDR` at `3a1f59e`.

The wider structural debt to AetherSDR (applet panel shape, style
palette, meter widgets, CtyDatParser, AdifParser) is catalogued in
`aethersdr-reconciliation.md` and `aethersdr-contributor-index.md`.
This file tracks *whole-file ports* added after that reconciliation.

| NereusSDR file | AetherSDR source | Rev | Ported | Notes |
|---|---|---|---|---|
| `src/core/CallsignInfo.h` | `src/core/CallsignInfo.h`, `src/core/CallsignUtils.h` | `3a1f59e` | 2026-08-07 | Namespace change. JSON cache round-trip and cty.dat prefix-fallback fields omitted — NereusSDR reads cty.dat through its own `CtyDatParser` and has no lookup cache. |
| `src/core/QrzClient.h` | `src/core/QrzClient.h` | `3a1f59e` | 2026-08-07 | Namespace change; log category localised. |
| `src/core/QrzClient.cpp` | `src/core/QrzClient.cpp` | `3a1f59e` | 2026-08-07 | Namespace change; `LogManager` category → local `Q_LOGGING_CATEGORY(lcQrz)`; user agent renamed; `nickname` field dropped with the struct field. |

## Why the upstream issue numbers stay in the comments

`#3990` (session-retry flag leak; latitude accepted without longitude)
and `#4043` (`<QRZDatabase>` root element swallowing `<Session>`) refer
to AetherSDR's tracker, not ours. They are kept because they document
failures that are silent until they bite: a half-coordinate pair puts
every European station in the Gulf of Guinea, and the root-element bug
broke login end-to-end with no visible error. `tests/tst_qrz_client.cpp`
pins both.

## Not ported

AetherSDR keeps QRZ passwords in the OS keychain via QtKeychain.
NereusSDR has no such dependency, so `src/core/CredentialStore.{h,cpp}`
is NereusSDR-original: macOS login keychain through the `security` CLI,
session-memory everywhere else, with `isPersistent()` so the UI can say
which of the two it got.
