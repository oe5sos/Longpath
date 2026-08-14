#pragma once

// =================================================================
// src/core/safety/RegionSetting.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Which band plan the operator is under, resolved from the setting the
// user interface actually writes.
//
// ── Why this file exists ─────────────────────────────────────────────
//
// 2026-08-14, found after a sweep on 80 m transmitted from 3.500 to
// 4.000 MHz for an operator in Austria, where the band ends at 3.800.
//
// The region had three separate lives and none of them met:
//
//   * Setup → General → Region wrote the DISPLAY STRING ("Europe") to
//     the settings key "Region".
//   * BandPlanGuard's two consumers — the sweep planner and the MOX
//     check that is supposed to refuse out-of-band transmission — read
//     an INTEGER from the key "BandPlanRegion".
//   * The antenna window's own "Region 1 / 2 / 3" box wrote a third key
//     and only decided which band edges got drawn.
//
// Nothing in the program ever wrote "BandPlanRegion". Two readers, no
// writer, so both consumers always took the default — which was
// UnitedStates. The combo box worked, persisted, and reloaded
// correctly; it simply had nothing on the other end of it. And the
// region the operator could SEE on the chart was the one with no
// authority, while the one with authority was invisible and wrong.
//
// So: one resolver, reading the key the interface writes, and both
// consumers go through it.
//
// ── When it is not configured ────────────────────────────────────────
//
// A default has to be something, and "United States" is the widest
// major plan — the failure direction of that choice is transmitting
// outside your allocation, which is the one failure that is not ours to
// risk on the operator's behalf.
//
// So an unset region does not silently pick a country. It reports
// itself as unconfigured, and callers fall back to
// isValidTxFreqEverywhere() — a frequency is allowed only if EVERY
// region in the table allows it. That is the narrowest defensible
// answer, it is computed from the tables rather than chosen by me, and
// its failure direction is a band edge that is too tight until the
// operator says where he is.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-14 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/safety/BandPlanGuard.h"

#include <QString>

namespace NereusSDR::safety {

/// Number of entries in Region. The display table is checked against
/// this at compile time so a new region cannot be added to one and
/// forgotten in the other.
inline constexpr int kRegionCount = 24;

/// The exact strings Setup → General → Region stores, in enum order.
/// Ported from Thetis setup.designer.cs:8084-8108 [v2.10.3.13] via
/// GeneralOptionsPage, which is where they are shown to the operator.
const QString* regionDisplayNames() noexcept;

/// Display string for a region, as written to settings.
QString regionDisplayName(Region r) noexcept;

/// Parse the stored display string. Sets *ok to false and returns
/// Region::Europe for anything unrecognised — but callers must check
/// ok rather than use that value, because an unparsed region means the
/// operator has not told us where he is, not that he is in Europe.
Region regionFromDisplayName(const QString& name, bool* ok = nullptr) noexcept;

/// What the operator chose, if anything.
struct RegionChoice {
    bool   configured{false};
    Region region{Region::Europe};   // meaningless unless configured
};

/// Read the region from AppSettings, from the key the interface writes.
RegionChoice configuredRegion();

/// True only if every region in the table permits this frequency. The
/// fallback for an unconfigured station: too tight rather than too
/// wide, and derived from the tables instead of guessed.
bool isValidTxFreqEverywhere(const BandPlanGuard& guard,
                             std::int64_t freqHz, DSPMode mode,
                             bool extended) noexcept;

} // namespace NereusSDR::safety
