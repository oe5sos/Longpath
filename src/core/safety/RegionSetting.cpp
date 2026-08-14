// =================================================================
// src/core/safety/RegionSetting.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See RegionSetting.h for why this exists.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-14 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/safety/RegionSetting.h"

#include "core/AppSettings.h"

#include <array>

namespace NereusSDR::safety {
namespace {

// In Region's declaration order, and that is load-bearing: the index
// into this table IS the enum value. GeneralOptionsPage builds its
// combo from the same list in the same order.
const std::array<QString, kRegionCount>& table()
{
    static const std::array<QString, kRegionCount> kNames = {
        QStringLiteral("Australia"),
        QStringLiteral("Europe"),
        QStringLiteral("India"),
        QStringLiteral("Italy"),
        QStringLiteral("Israel"),
        QStringLiteral("Japan"),
        QStringLiteral("Spain"),
        QStringLiteral("United Kingdom"),
        QStringLiteral("United States"),
        QStringLiteral("Norway"),
        QStringLiteral("Denmark"),
        QStringLiteral("Sweden"),
        QStringLiteral("Latvia"),
        QStringLiteral("Slovakia"),
        QStringLiteral("Bulgaria"),
        QStringLiteral("Greece"),
        QStringLiteral("Hungary"),
        QStringLiteral("Netherlands"),
        QStringLiteral("France"),
        QStringLiteral("Russia"),
        QStringLiteral("Region1"),
        QStringLiteral("Region2"),
        QStringLiteral("Region3"),
        QStringLiteral("Germany"),
    };
    return kNames;
}

// If Region ever grows an entry, this stops the build rather than
// letting the new one parse as something else.
static_assert(static_cast<int>(Region::Germany) + 1 == kRegionCount,
              "Region and the display-name table have drifted apart");

} // namespace

const QString* regionDisplayNames() noexcept { return table().data(); }

QString regionDisplayName(Region r) noexcept
{
    const int i = static_cast<int>(r);
    if (i < 0 || i >= kRegionCount) { return {}; }
    return table()[static_cast<std::size_t>(i)];
}

Region regionFromDisplayName(const QString& name, bool* ok) noexcept
{
    for (int i = 0; i < kRegionCount; ++i) {
        if (table()[static_cast<std::size_t>(i)] == name) {
            if (ok) { *ok = true; }
            return static_cast<Region>(i);
        }
    }
    if (ok) { *ok = false; }
    return Region::Europe;
}

RegionChoice configuredRegion()
{
    // The key the interface writes. Deliberately NOT "BandPlanRegion" —
    // that one had two readers and never a writer, which is how an
    // Austrian station came to be clipped against the US band plan.
    const QString stored =
        AppSettings::instance().value(QStringLiteral("Region")).toString();
    if (stored.isEmpty()) { return {}; }

    bool ok = false;
    const Region r = regionFromDisplayName(stored, &ok);
    if (!ok) { return {}; }
    return {true, r};
}

bool isValidTxFreqEverywhere(const BandPlanGuard& guard,
                             std::int64_t freqHz, DSPMode mode,
                             bool extended) noexcept
{
    // Extended is the operator explicitly taking the guard off. It
    // means the same thing here as everywhere else.
    if (extended) {
        return guard.isValidTxFreq(Region::UnitedStates, freqHz, mode, true);
    }
    for (int i = 0; i < kRegionCount; ++i) {
        if (!guard.isValidTxFreq(static_cast<Region>(i), freqHz, mode,
                                 false)) {
            return false;
        }
    }
    return true;
}

} // namespace NereusSDR::safety
