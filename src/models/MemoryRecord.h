#pragma once
// =================================================================
// src/models/MemoryRecord.h  (Longpath)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/Memory/MemoryRecord.cs, original licence
//   from Thetis source is included below
//   Project Files/Source/Console/console.cs (the tune-step name table),
//   original licence from Thetis source is included below
//   Project Files/Source/Console/enums.cs (AGCMode / FMTXMode names),
//   original licence from Thetis source is included below
//
// One memory slot: what the operator wants recalled with one click --
// frequency, mode, filter, AGC, the FM repeater set, power, a name and
// a group. Thetis keeps these in memory.xml (MemoryList.cs) and edits
// them in a grid (MemoryForm.cs); Longpath keeps the same fields under
// the same XML element names so a Thetis memory.xml can be dropped in
// and read back out unchanged.
//
// Longpath deviations, each marked at the site:
//   * Thetis's `Filter` enum (F1..F10, VAR1, VAR2) has no Longpath
//     counterpart -- Longpath filters are (low, high) pairs. The enum
//     name is kept as text for the file; recall applies the stored
//     bounds.
//   * The ke9ns scheduled-recording fields (StartDate, Duration,
//     Recording, Repeating, Repeatingm, ScheduleOn, Extra) are not
//     modelled -- Longpath has its own RecordingScheduler -- but every
//     element this program does not model is carried in `extras` and
//     written back, so nothing in a Thetis file is lost.
//   * Split / TXFreq are stored and shown but not applied on recall:
//     Longpath has no split VFO (design 2026-05-26 §3, "split is
//     replaced with XIT").
//
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Ported for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude, from Thetis
//                 v2.10.3.15-5-g852bf0e. C# properties became a plain
//                 struct; INotifyPropertyChanged is the table model's
//                 job (MemoryList); CompareTo became operator<.
// =================================================================
//=================================================================
// MemoryRecord.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2003-2013  FlexRadio Systems
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
//=================================================================
// --- From console.cs ---
//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
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
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
// ApacheLabs G2E support added throughout Thetis in various files, all changes marked  //N1GP G2E added
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
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Final modifictions by MW0LGE Richard Samphire - 19th April 2026
// Nothing further added by him after this date, and his repo is now in archive https://github.com/ramdor/Thetis
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// --- From enums.cs ---
/*  enums.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2000-2025 Original authors
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

#include "core/WdspTypes.h"

#include <QPair>
#include <QString>
#include <QVector>

namespace Longpath {

// From Thetis Memory/MemoryRecord.cs:37-167 [@852bf0e]
//   public class MemoryRecord : IComparable, INotifyPropertyChanged
struct MemoryRecord {
    // From Thetis Memory/MemoryRecord.cs:181-232 [@852bf0e] (the field
    // defaults, in upstream order)
    QString  group;                              // group = ""
    double   rxFreqMHz{10.0};                    // rx_freq = 10.0
    QString  name;                               // name = ""
    DSPMode  dspMode{DSPMode::LSB};              // dsp_mode = DSPMode.LSB
    bool     scan{true};                         // scan = true           (:310)
    QString  tuneStep{QStringLiteral("10Hz")};   // tune_step = "10Hz"    (:321)
    FmTxMode rptr{FmTxMode::High};               // repeater_mode = 0     (:332) -- FMTXMode.High
    double   rptrOffsetMHz{0.1};                 // rptr_offset = 0.1     (:343)
    bool     ctcssOn{false};                     // ctcss_on = false      (:357)
    double   ctcssFreq{0.0};                     // ctcss_freq = 0.0      (:368)
    int      deviation{5000};                    // deviation = 5000      (:379)
    int      power{0};                           // power = 0             (:390)
    bool     split{false};                       // split = false         (:401)
    double   txFreqMHz{10.0};                    // tx_freq = 10.0        (:412)
    // Longpath deviation: Thetis `Filter rx_filter = 0` (F1) is an enum;
    // kept as its upstream name so the file round-trips.
    QString  rxFilter{QStringLiteral("F1")};     // rx_filter = 0         (:423)
    int      rxFilterLow{0};                     // rx_filter_low = 0     (:434)
    int      rxFilterHigh{0};                    // rx_filter_high = 0    (:445)
    QString  comments;                           // comments = ""         (:456)
    AGCMode  agcMode{AGCMode::Med};              // agc_mode = AGCMode.MED (:467)
    int      agcT{80};                           // agct = 80             (:478)

    // Longpath addition: every XML element this struct does not model
    // (the ke9ns schedule set and anything a newer Thetis adds), kept in
    // file order and written back untouched.
    QVector<QPair<QString, QString>> extras;

    // From Thetis Memory/MemoryRecord.cs:522-534 [@852bf0e]
    //   public int CompareTo(object obj): Group, then RXFreq, then Name.
    bool operator<(const MemoryRecord& rec) const;

    // ── Thetis's enum spellings, for memory.xml ──────────────────────
    // From Thetis enums.cs:152-162 [@852bf0e] (AGCMode: FIXD LONG SLOW MED FAST CUSTOM)
    static QString  agcModeName(AGCMode mode);
    static AGCMode  agcModeFromName(const QString& name);
    // From Thetis enums.cs:381-387 [@852bf0e] (FMTXMode: High Simplex Low)
    static QString  fmTxModeName(FmTxMode mode);
    static FmTxMode fmTxModeFromName(const QString& name);

    // ── The tune-step table ──────────────────────────────────────────
    // From Thetis console.cs:1885-1910 [@852bf0e] -- tune_step_list, the
    // 26 named steps from "1Hz" to "10MHz". Memories store the name
    // (TuneStep = "10Hz"); Longpath's SliceModel keeps hertz.
    static int     tuneStepHzFromName(const QString& name);   // 0 when unknown
    static QString tuneStepNameFromHz(int hz);                // "" when not in the table
    static QStringList tuneStepNames();
};

} // namespace Longpath
