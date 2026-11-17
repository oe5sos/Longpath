# RtMidi (vendored)

Upstream: https://github.com/thestk/rtmidi — Gary P. Scavone, MIT-style
licence (see `LICENSE`, verbatim). Version **6.0.0**.

Taken **as carried by AetherSDR** (`../AetherSDR/third_party/rtmidi` at
`d58e2b8a`), which is upstream 6.0.0 plus one AetherSDR fix, commit
`5769f480` ("Fix CoreMIDI init crash during MIDI auto-connect", #1949):
the `throw()` specifier removed from `getCoreMidiClientSingleton` (four
places) and one error text corrected (`MidiInCore` → `MidiOutCore`). No
Longpath patches on top; `RtMidi.cpp` and `RtMidi.h` are byte-identical to
AetherSDR's copy.

Only the library (`RtMidi.cpp`, `RtMidi.h`) is vendored; upstream's build
files, tests and documentation are not. Consumed through
`src/core/midi/MidiKeyInput.{h,cpp}`.

Provenance record: `docs/attribution/RTMIDI-PROVENANCE.md`.
