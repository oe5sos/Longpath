// =================================================================
// src/core/SMeterSkala.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/common.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (Longpath):
//   2026-11-08 — Reimplemented in C++20/Qt6 for Longpath by Martin
//                 Fischer (OE5SOS), with AI-assisted transformation via
//                 Anthropic Claude Code. Thetis v2.10.3.15-5 [@852bf0e].
// =================================================================

// --- From common.cs ---
//=================================================================
// common.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2012  FlexRadio Systems
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
// You may contact us via email at: gpl@flexradio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    4616 W. Howard Lane  Suite 1-150
//    Austin, TX 78728
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

// --- From MeterManager.cs ---
/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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

#include "SMeterSkala.h"

namespace Longpath::SMeter {

// From Thetis common.cs:881-887 [@852bf0e]:
//   public static double GetSMeterUnits(double dbm, bool bAboveS9Frequency)
//   {
//       if (bAboveS9Frequency)
//           return 9 + ((dbm + 93) / 6f); //MW0LGE_[2.9.0.7] fixed to 93
//       else
//           return 9 + ((dbm + 73) / 6f);
//   }
double einheiten(double dbm, bool ueberS9Frequenz)
{
    if (ueberS9Frequenz) {
        return 9.0 + ((dbm + 93.0) / 6.0); //MW0LGE_[2.9.0.7] fixed to 93
    }
    return 9.0 + ((dbm + 73.0) / 6.0);
}

// Longpath-eigen: die Umkehrung von einheiten(), fuer die Lage der
// Striche an einer Skala.
double dbmFuer(double s, bool ueberS9Frequenz)
{
    return (s - 9.0) * 6.0 - (ueberS9Frequenz ? 93.0 : 73.0);
}

// From Thetis common.cs:834-880 [@852bf0e] SMeterFromDBM_Spaceless:
//   // same as above, but without spaces. Used by MultiMeter display
// Die Grenzen der Tabelle wortgetreu; je Zeile "> unten & <= oben".
QString text(double dbm, bool ueberS9Frequenz)
{
    struct Zeile { double oben; const char* t; };
    static constexpr Zeile kUeber[] = {
        {-144.0, "S0"}, {-138.0, "S1"}, {-132.0, "S2"}, {-126.0, "S3"},
        {-120.0, "S4"}, {-114.0, "S5"}, {-108.0, "S6"}, {-102.0, "S7"},
        {-96.0, "S8"},  {-90.0, "S9"},  {-86.0, "S9+5"}, {-80.0, "S9+10"},
        {-76.0, "S9+15"}, {-66.0, "S9+20"}, {-56.0, "S9+30"},
        {-46.0, "S9+40"}, {-36.0, "S9+50"},
    };
    static constexpr Zeile kUnter[] = {
        {-124.0, "S0"}, {-118.0, "S1"}, {-112.0, "S2"}, {-106.0, "S3"},
        {-100.0, "S4"}, {-94.0, "S5"},  {-88.0, "S6"},  {-82.0, "S7"},
        {-76.0, "S8"},  {-70.0, "S9"},  {-66.0, "S9+5"}, {-60.0, "S9+10"},
        {-56.0, "S9+15"}, {-46.0, "S9+20"}, {-36.0, "S9+30"},
        {-26.0, "S9+40"}, {-16.0, "S9+50"},
    };
    const auto& tab = ueberS9Frequenz ? kUeber : kUnter;
    for (const Zeile& z : tab) {
        if (dbm <= z.oben) { return QString::fromLatin1(z.t); }
    }
    return QStringLiteral("S9+60");
}

} // namespace Longpath::SMeter
