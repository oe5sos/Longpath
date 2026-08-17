// =================================================================
// src/core/safety/RegionSetting.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.designer.cs (upstream has no top-of-file header — project-level LICENSE applies)
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// HIER liegen die 24 Regions-Anzeigenamen, woertlich und in der
// Reihenfolge aus comboFRSRegion.Items.AddRange
// (setup.designer.cs:8132-8156 [@852bf0e]). Der Kopf gehoert deshalb
// hierher und nicht nur an die Deklaration. Alles Uebrige ist
// NereusSDR-original — siehe RegionSetting.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-14 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//


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
