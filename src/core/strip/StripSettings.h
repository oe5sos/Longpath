#pragma once

// =================================================================
// src/core/strip/StripSettings.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Save and restore the channel strip.
//
// Without this the strip is a toy: every setting is lost on restart, so
// an operator spends twenty minutes finding a sound and then never has
// it again. AetherSDR solves the same problem with a preset library
// (ChannelStripPresets); this is the layer below that — one current
// state, always saved, so the radio comes up sounding the way it did
// when it was switched off. Named presets can sit on top later.
//
// Two things deliberately do NOT persist:
//
//   The master switch. It loads off, every time. A strip that comes
//   back on by itself is a strip that changes the operator's transmit
//   audio without them present to hear it — the same reasoning that
//   makes MON load off in TransmitModel.
//
//   Nothing about the radio. These are stage parameters and nothing
//   else; no gain that could reach the modulator by another route.
//
// Everything is written under one prefix and read back with the DSP's
// own defaults as fallbacks, so a settings file from before this
// existed — or one that has been hand-edited into nonsense — produces
// the stage's own default rather than a zero.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QString>

namespace NereusSDR {

class StripChain;

namespace StripSettings {

// The prefix every key sits under, exposed so a support bundle or a
// settings-hygiene pass can find them as a group.
inline constexpr char kPrefix[] = "ChannelStrip/";

// Write the chain's current state. Cheap enough to call on every
// control change; AppSettings coalesces its own disk writes.
void save(const StripChain& chain);

// Read it back into a prepared chain. Stage parameters and per-stage
// enables are restored; the master switch is not — see the header note.
void restore(StripChain& chain);

} // namespace StripSettings
} // namespace NereusSDR
