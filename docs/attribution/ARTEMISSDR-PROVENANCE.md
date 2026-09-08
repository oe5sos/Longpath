# ArtemisSDR Provenance — Longpath derived-file inventory

This document catalogs every Longpath source file derived from,
translated from, or materially based on ArtemisSDR (kk68/ArtemisSDR).
Per-file license headers live in the source files themselves; this
index is the grep-able summary.

Longpath is distributed under GPLv3 (root `LICENSE`). ArtemisSDR is
GPLv2-or-later — compatible via the "or-later" escape hatch, same
standing as the Anvelina gateware citation in `CLAUDE.md`. See
§License below.

## Why this source exists in the corpus at all

SunSDR2's native wire protocol has no vendor spec — Expert Electronics
documents only TCI. ArtemisSDR is a citable, GPL-licensed, black-box
reverse-engineered implementation (its own header states plainly: "No
ExpertSDR code, binaries, firmware, or artwork was referenced or
used"), cross-checked against a second independent implementation
(solsdr, Jeff Francis N0GQ). Full rationale:
`docs/architecture/2026-08-24-sunsdr-native-driver-design.md`
§"The source-first problem, and how it's resolved".

**Standing caution, not a formality:** ArtemisSDR documents DX and PRO
only. The SunSDR2 QRP is a different, lower-power sibling in the same
product family — plausible to share the protocol, not confirmed to,
except where a real QRP bench capture has independently confirmed it
(see the design doc's "Confirmed from real QRP capture" section). A
port from this corpus that touches QRP-specific behavior must cite
that confirmation separately, not just this file's ArtemisSDR lineage.

## When entries get added

A row is added to the table below — in the **same commit** that
introduces the ported logic — whenever a Longpath file:

1. Ports, translates, or materially re-expresses logic from any
   ArtemisSDR `.c` or `.h` file, AND
2. That logic is not already covered elsewhere (i.e. ArtemisSDR is the
   *primary* source for that logic).

The procedure is identical to `THETIS-PROVENANCE.md`:

- Add the verbatim ArtemisSDR file header to the Longpath file (per
  `HOW-TO-PORT.md` §"Byte-for-byte headers and multi-file attribution").
- Add a `[@<shortsha>]` inline cite (ArtemisSDR has no tagged releases
  to stamp against — see §Citation grammar below) at every ported
  function/constant.
- Add a PROVENANCE row here with the Longpath file, ArtemisSDR source,
  line ranges, derivation type, and notes.

## Upstream

- **Project:** ArtemisSDR
- **Repository:** https://github.com/kk68/ArtemisSDR
- **Lineage:** a fork of Thetis (the same upstream Longpath itself
  descends from) extended with native SunSDR2 DX/PRO protocol support.
- **Author:** Kosta Kanchev, K0KOZ (2026)
- **Initial corpus reference commit:** `@f8b01d25c5` (HEAD of `main` for
  `Project Files/Source/ChannelMaster/sunsdr.c`, 2026-07-08, at the time
  of first ArtemisSDR port, 2026-08-25)
- **Language:** C (`sunsdr.c` / `sunsdr.h` under
  `Project Files/Source/ChannelMaster/`)
- **Cross-checked against:** solsdr (Jeff Francis, N0GQ),
  https://github.com/jfrancis42/solsdr — independent SunSDR2 PRO
  client from separate wire captures; the two agree where compared.

## Citation grammar

ArtemisSDR is reverse-engineered, not a versioned vendor release —
there is no tagged release to stamp inline cites against, only a
specific file at a specific commit in a live repo. Cites in Longpath
source use `[ArtemisSDR sunsdr.c:N @f8b01d25c5]` rather than the
Thetis-style `[v2.10.3.13]` release stamp. A future re-sync should
re-pull ArtemisSDR and note the new commit reached, both here and in
the design doc.

## License

ArtemisSDR is distributed under the **GNU General Public License v2.0
or later**. `sunsdr.c` carries the header:

```
Copyright (C) 2026 Kosta Kanchev (K0KOZ)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
```

Longpath is GPLv3. GPLv2-or-later + GPLv3 → **compatible** via the
"or-later" clause (combined work distributable under GPLv3), same
standing as the `n1gp-Anvelina_PROIII` gateware citation documented in
`CLAUDE.md`. No dual-licensing (Samphire-style or otherwise) exists in
the ArtemisSDR source tree.

## Legend

Derivation type:
- `port`       — direct reimplementation in C++/Qt6 of ArtemisSDR logic
- `reference`  — consulted for behavior during independent implementation
- `structural` — architectural template with substantive behavioral echo

## Files derived from kk68/ArtemisSDR

| Longpath file | ArtemisSDR source | Line ranges | Type | Notes |
| --- | --- | --- | --- | --- |
| `src/core/sunsdr/SunSdrProtocol.h` + `.cpp` | `sunsdr.c:1678-1690` (`sunsdr_build_iq_header`), `sunsdr.c:2242-2255` (`sunsdr_build_header`), `sunsdr.c:4653-4668` (RX sample unpack loop), `sunsdr.c:1306` (`NORM` constant), `sunsdr.c:2728-2741` (DX/PRO profile tables), `sunsdr.h:23-89,136` (port/size/opcode constants); added 2026-09-02: `sunsdr.h:48` (`SUNSDR_OP_MOX_PTT`), `sunsdr.h:54` (`SUNSDR_OP_RX_ANT`), `sunsdr.h:60-61` (`SUNSDR_OP_DRIVE`/deprecated `SUNSDR_OP_MODE` alias), `sunsdr.h:68` (`SUNSDR_OP_PA_ENABLE`), `sunsdr.c:2391-2403` (`sunsdr_send_u32_cmd` — the sub=0/4-byte-LE-payload frame shape all four opcodes share), `sunsdr.c:2579-2581` (`sunsdr_current_pa_wire_state`), `sunsdr.c:3680-3685` (`sunsdr_drive_raw_to_wire_byte`) | see above | `port` | First ArtemisSDR port. SunSDR native driver, design-doc Phase 2 (wire framing — control-channel header, IQ-stream header, IQ sample decode). QRP profile row (`kProfileQrp`, magic0=0x03, ports 50001/50002) is Longpath-original, confirmed against a real QRP bench capture per the design doc, not from ArtemisSDR (ArtemisSDR has no QRP row at all). Date: 2026-08-25. **2026-09-02 addition:** four TX control-channel PURE ENCODERS (`buildMoxFrame` 0x06, `buildAntennaSelectFrame` 0x15, `buildDriveFrame` 0x17, `buildPaEnableFrame` 0x24) — Step 1 of a 6-step operator-approved TX chain plan, zero wire reachability, no socket call site anywhere yet. `buildAntennaSelectFrame`'s selector-byte table encodes the A3 RX/0x03-vs-TX/0x02 trap from the design doc (line 983); A1/A2 (RX=TX=0x01, no split) were not in the design doc's own text and were added same-day directly from ArtemisSDR (`HPSDR/SunSdrAntenna.cs:10-30,81-95` + `sunsdr.c:2278-2293` `sunsdr_map_ant_selector`/`sunsdr_map_tx_ant_selector`, both agreeing independently), per the design doc's own instruction to re-derive from ArtemisSDR rather than guess. All three ports carry the same confirmation tier — "traced to a citable source", not yet bench-verified against real QRP hardware in a live TX session. |

## Files referencing ArtemisSDR (facts cited, no code ported)

| Longpath file | ArtemisSDR source | Notes |
| --- | --- | --- |
| `docs/architecture/2026-08-24-sunsdr-native-driver-design.md` | `sunsdr.c` (5024 lines), `sunsdr.h` (302 lines), full read | The design doc and protocol reference this provenance file supports. Every fact cited to exact file:line. |
