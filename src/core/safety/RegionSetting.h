#pragma once

// =================================================================
// src/core/safety/RegionSetting.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.designer.cs (upstream has no top-of-file header — project-level LICENSE applies)
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// Umfang der Uebernahme: die 24 Regions-Anzeigenamen und ihre
// REIHENFOLGE, woertlich aus comboFRSRegion.Items.AddRange
// (setup.designer.cs:8132-8156 [@852bf0e]). Die Reihenfolge ist der
// eigentliche Gegenstand — sie ist der Zahlenwert, unter dem die
// Einstellung auf der Platte steht, und ein Vertauschen zweier Namen
// verschoebe stillschweigend das Band eines Bedieners.
//
// Der Rest dieser Datei ist NereusSDR-original: Thetis hat keinen
// BandPlanGuard und keine aufgeloeste Region als eigenen Typ.
//
// Zur fehlenden Kopfzeile: setup.designer.cs ist eine erzeugte
// Designer-Datei und traegt keinen Lizenzkopf — Regel 5 in
// docs/attribution/HOW-TO-PORT.md, kein Kopf wird erfunden.
//
// Deshalb steht setup.cs als zweite Quelle dabei, und ihr Kopf unten.
// Das ist keine Notloesung: die beiden Dateien sind DASSELBE
// `partial class Setup`, und die Lizenz dieser Klasse steht in
// setup.cs. Genau dieses Paar bildet HardwareProfile.h schon ab
// (clsHardwareSpecific.cs mit Kopf + NetworkIO.cs ohne).
//
// Ohne die zweite Quelle liefen Regel 5 und
// scripts/verify-thetis-headers.py gegeneinander: die Regel verbietet
// einen erfundenen Kopf, der Pruefer verlangt die Marker
// „Copyright (C)" und „General Public License" von jeder gelisteten
// Datei. Die Partnerdatei loest beides, ohne etwas zu erfinden.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-14 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-08-17 — Attribution nachgetragen (Ported-from-Block +
//                 PROVENANCE-Zeile). Die Datei nannte sich
//                 „NereusSDR-original", waehrend sie eine woertlich
//                 uebernommene Liste fuehrte; scripts/check-new-ports.py
//                 ist darauf seit dem Anlegen fehlgeschlagen.
//                 Martin Fischer, AI-assisted via Anthropic Claude.
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


// =================================================================
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

namespace Longpath::safety {

/// Number of entries in Region. The display table is checked against
/// this at compile time so a new region cannot be added to one and
/// forgotten in the other.
inline constexpr int kRegionCount = 24;

/// The exact strings Setup → General → Region stores, in enum order.
/// Ported from Thetis setup.designer.cs:8132-8156 [@852bf0e]
/// (comboFRSRegion.Items.AddRange) via GeneralOptionsPage, which is
/// where they are shown to the operator.
///
/// Die Angabe stand als 8084-8108 [v2.10.3.13] hier und zeigte in der
/// vorliegenden Arbeitskopie auf den Rahmen der Gruppe statt auf die
/// Liste. Die 24 Namen selbst stimmen unveraendert.
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

} // namespace Longpath::safety
