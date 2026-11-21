# RtMidi Provenance — Longpath vendored-library inventory

This document catalogs the vendored RtMidi library shipped under
`third_party/rtmidi/`. It is consumed exclusively through
`src/core/midi/MidiKeyInput.{h,cpp}` (HaliKey MIDI, 2026-11-17).

Longpath is distributed under GPLv3 (root `LICENSE`). RtMidi is under an
MIT-style licence, fully GPL-compatible.

## Upstream

- **Project:** RtMidi — realtime MIDI i/o C++ classes
- **Repository:** https://github.com/thestk/rtmidi
- **Primary author:** Gary P. Scavone
- **Version:** 6.0.0
- **Taken from:** AetherSDR (`../AetherSDR/third_party/rtmidi`) at
  `d58e2b8a` — upstream 6.0.0 plus AetherSDR commit `5769f480`
  ("Fix CoreMIDI init crash during MIDI auto-connect", #1949)
- **Vendored:** 2026-11-17
- **Languages:** C++ (one translation unit, one header)

## License

RtMidi is distributed under an MIT-style licence ("Permission is hereby
granted, free of charge, …" with the request to send modifications to the
author). The verbatim `LICENSE` is preserved at
`third_party/rtmidi/LICENSE`, and the same text stands at the top of
`RtMidi.h` and `RtMidi.cpp`. No special obligations apply when Longpath
statically links RtMidi into `LongpathObjs`.

## Deviation from upstream

One change, made by AetherSDR and carried over unchanged (GPLv3, as
Longpath): `getCoreMidiClientSingleton` loses its `throw()` specifier in
`MidiInCore` and `MidiOutCore` (declaration and definition), and one
error message names the right class. Reason given upstream of us: with
`throw()`, an exception during CoreMIDI client creation terminated the
process during MIDI auto-connect.

## Files

| File | Origin |
| --- | --- |
| `third_party/rtmidi/RtMidi.h` | RtMidi 6.0.0, via AetherSDR `d58e2b8a` |
| `third_party/rtmidi/RtMidi.cpp` | RtMidi 6.0.0 + AetherSDR `5769f480`, via AetherSDR `d58e2b8a` |
| `third_party/rtmidi/LICENSE` | verbatim |
| `third_party/rtmidi/CMakeLists.txt` | Longpath-original build glue |
| `third_party/rtmidi/README-LONGPATH.md` | Longpath-original |
