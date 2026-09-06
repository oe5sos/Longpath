// src/gui/RttyDecoderSensitivity.h
// RTTY decoder sensitivity: the slider -> confidence-threshold mapping
// behind the RTTY applet's sensitivity control. Pure and header-only so
// the mapping is pinned by a test without a widget.
//
// =================================================================
// src/gui/RttyDecoderSensitivity.h  (Longpath)
// =================================================================
//
// Source attribution (AetherSDR -- GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       -- per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a near-verbatim port of AetherSDR's
//   src/gui/RttyDecoderSensitivity.h -- the slider-to-threshold formula
//   and its rationale are unchanged. No Thetis equivalent exists (see
//   RttyDecoder.h for the full sole-source rationale).
//   AetherSDR is licensed under the GNU General Public License v3.
//   Longpath is also GPLv3. Attribution follows GPLv3 SS5 requirements.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-06 -- Ported for Longpath by OE5SOS with AI-assisted
//                 transformation via Anthropic Claude Code. Namespace
//                 only; formula and constants unchanged.
// =================================================================

#pragma once

// RttyDecoder reports per-character confidence as max(mark,space)/(mark+space),
// which can never fall below 0.5 (it is the larger of two envelopes over their
// sum) and is 1.0 for a clean tone. 0..100 maps onto 0.50..0.95.
//
// The default is 0: threshold 0.50 is the confidence floor, so `confidence <
// threshold` can never fire and out-of-the-box behavior shows every decoded
// character -- filtering is strictly opt-in. A useful starting value when
// noise floods the pane is 38, which lands the threshold at ~0.67 -- the
// decoder's own 3 dB "locked" point (snrDb == 10*log10(c/(1-c))), i.e.
// filter what the stats bar calls UNLOCK.
namespace Longpath {

constexpr int kRttySensitivityDefault = 0;

constexpr float rttyConfThresholdFor(int sens)
{
    const int clamped = sens < 0 ? 0 : (sens > 100 ? 100 : sens);
    return 0.5f + (static_cast<float>(clamped) / 100.0f) * 0.45f;
}

} // namespace Longpath
