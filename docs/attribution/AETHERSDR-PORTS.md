# AetherSDR whole-file ports

AetherSDR (https://github.com/aethersdr/AetherSDR) is GPLv3, as is
NereusSDR, so ported code carries forward under the same licence with
attribution per GPLv3 §5. Every file below has the project-level
AetherSDR copyright block in its own header; this table is the index.

Upstream reference: `../AetherSDR` at `3a1f59e`; the channel-strip
DSP below was taken later, at `31b29583`.

The wider structural debt to AetherSDR (applet panel shape, style
palette, meter widgets, CtyDatParser, AdifParser) is catalogued in
`aethersdr-reconciliation.md` and `aethersdr-contributor-index.md`.
This file tracks *whole-file ports* added after that reconciliation.

| NereusSDR file | AetherSDR source | Rev | Ported | Notes |
|---|---|---|---|---|
| `src/core/CallsignInfo.h` | `src/core/CallsignInfo.h`, `src/core/CallsignUtils.h` | `3a1f59e` | 2026-08-07 | Namespace change. JSON cache round-trip and cty.dat prefix-fallback fields omitted — NereusSDR reads cty.dat through its own `CtyDatParser` and has no lookup cache. |
| `src/core/QrzClient.h` | `src/core/QrzClient.h` | `3a1f59e` | 2026-08-07 | Namespace change; log category localised. |
| `src/core/QrzClient.cpp` | `src/core/QrzClient.cpp` | `3a1f59e` | 2026-08-07 | Namespace change; `LogManager` category → local `Q_LOGGING_CATEGORY(lcQrz)`; user agent renamed; `nickname` field dropped with the struct field. |
| `src/core/strip/ClientPhaseRotator.h`, `.cpp` | `src/core/ClientPhaseRotator.{h,cpp}` | `31b29583` | 2026-08-08 | Namespace change; include paths rebased onto `core/strip/`. DSP unchanged. |
| `src/core/strip/ClientGate.h`, `.cpp` | `src/core/ClientGate.{h,cpp}` | `31b29583` | 2026-08-08 | As above. Downward expander / noise gate, first stage of the chain. |
| `src/core/strip/ClientEq.h`, `.cpp` | `src/core/ClientEq.{h,cpp}` | `31b29583` | 2026-08-08 | As above. |
| `src/core/strip/ClientDeEss.h`, `.cpp` | `src/core/ClientDeEss.{h,cpp}` | `31b29583` | 2026-08-08 | As above. |
| `src/core/strip/ClientComp.h`, `.cpp` | `src/core/ClientComp.{h,cpp}` | `31b29583` | 2026-08-08 | As above. |
| `src/core/strip/ClientTube.h`, `.cpp` | `src/core/ClientTube.{h,cpp}` | `31b29583` | 2026-08-08 | As above. |
| `src/core/strip/ClientPudu.h`, `.cpp` | `src/core/ClientPudu.{h,cpp}` | `31b29583` | 2026-08-08 | As above. This is the AetherVoice exciter. |
| `src/core/strip/ClientReverb.h`, `.cpp` | `src/core/ClientReverb.{h,cpp}` | `31b29583` | 2026-08-08 | As above. No `setEnabled()` upstream; wet mix at zero is its bypass. |
| `src/core/strip/ClientFinalLimiter.h`, `.cpp` | `src/core/ClientFinalLimiter.{h,cpp}` | `31b29583` | 2026-08-08 | As above. No `setEnabled()` upstream — a brickwall that can be switched off is not one. |

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
