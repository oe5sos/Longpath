// =================================================================
// src/models/MemoryRecord.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/Memory/MemoryRecord.cs, original licence
//   from Thetis source is included in MemoryRecord.h
//   Project Files/Source/Console/console.cs (the tune-step name table),
//   original licence from Thetis source is included in MemoryRecord.h
//   Project Files/Source/Console/enums.cs (AGCMode / FMTXMode names)
//
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Ported for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude, from Thetis
//                 v2.10.3.15-5-g852bf0e.
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

#include "models/MemoryRecord.h"

#include <QStringList>

namespace Longpath {

// From Thetis Memory/MemoryRecord.cs:522-534 [@852bf0e]
//   public int CompareTo(object obj) // to implement the IComparable interface
//   {
//       MemoryRecord rec = (MemoryRecord)obj;
//       if (this.Group != rec.Group) return this.Group.CompareTo(rec.Group);
//       if (this.RXFreq != rec.RXFreq) return this.RXFreq.CompareTo(rec.RXFreq);
//       return this.Name.CompareTo(rec.Name);
//   }
bool MemoryRecord::operator<(const MemoryRecord& rec) const
{
    if (group != rec.group) { return group < rec.group; }
    if (rxFreqMHz != rec.rxFreqMHz) { return rxFreqMHz < rec.rxFreqMHz; }
    return name < rec.name;
}

// From Thetis enums.cs:152-162 [@852bf0e]
//   public enum AGCMode { FIRST = -1, FIXD, LONG, SLOW, MED, FAST, CUSTOM, LAST }
// Longpath's AGCMode has the same order (Off = FIXD).
QString MemoryRecord::agcModeName(AGCMode mode)
{
    switch (mode) {
    case AGCMode::Off:    return QStringLiteral("FIXD");
    case AGCMode::Long:   return QStringLiteral("LONG");
    case AGCMode::Slow:   return QStringLiteral("SLOW");
    case AGCMode::Med:    return QStringLiteral("MED");
    case AGCMode::Fast:   return QStringLiteral("FAST");
    case AGCMode::Custom: return QStringLiteral("CUSTOM");
    }
    return QStringLiteral("MED");
}

AGCMode MemoryRecord::agcModeFromName(const QString& name)
{
    const QString n = name.trimmed().toUpper();
    if (n == QLatin1String("FIXD"))   { return AGCMode::Off; }
    if (n == QLatin1String("LONG"))   { return AGCMode::Long; }
    if (n == QLatin1String("SLOW"))   { return AGCMode::Slow; }
    if (n == QLatin1String("FAST"))   { return AGCMode::Fast; }
    if (n == QLatin1String("CUSTOM")) { return AGCMode::Custom; }
    return AGCMode::Med;
}

// From Thetis enums.cs:381-387 [@852bf0e]
//   public enum FMTXMode { High = 0, Simplex, Low }
QString MemoryRecord::fmTxModeName(FmTxMode mode)
{
    switch (mode) {
    case FmTxMode::High:    return QStringLiteral("High");
    case FmTxMode::Simplex: return QStringLiteral("Simplex");
    case FmTxMode::Low:     return QStringLiteral("Low");
    }
    return QStringLiteral("High");
}

FmTxMode MemoryRecord::fmTxModeFromName(const QString& name)
{
    const QString n = name.trimmed().toLower();
    if (n == QLatin1String("simplex")) { return FmTxMode::Simplex; }
    if (n == QLatin1String("low"))     { return FmTxMode::Low; }
    return FmTxMode::High;
}

namespace {

struct TuneStepEntry { int hz; const char* name; };

// From Thetis console.cs:1885-1910 [@852bf0e]
//   tune_step_list = new List<TuneStep> { new TuneStep(1, "1Hz"), ... }
constexpr TuneStepEntry kTuneSteps[] = {
    {1, "1Hz"},           //0
    {2, "2Hz"},           //1
    {10, "10Hz"},         //2
    {25, "25Hz"},         //3
    {50, "50Hz"},         //4
    {100, "100Hz"},       //5
    {250, "250Hz"},       //6
    {500, "500Hz"},       //7
    {1000, "1kHz"},       //8
    {2000, "2kHz"},       //9
    {2500, "2.5kHz"},     //10
    {5000, "5kHz"},       //11
    {6250, "6.25kHz"},    //12
    {9000, "9kHz"},       //13
    {10000, "10kHz"},     //14
    {12500, "12.5kHz"},   //15
    {15000, "15kHz"},     //16
    {20000, "20kHz"},     //17
    {25000, "25kHz"},     //18
    {30000, "30kHz"},     //19
    {50000, "50kHz"},     //20
    {100000, "100kHz"},   //21
    {250000, "250kHz"},   //22
    {500000, "500kHz"},   //23
    {1000000, "1MHz"},    //24
    {10000000, "10MHz"},  //25
};

} // namespace

int MemoryRecord::tuneStepHzFromName(const QString& name)
{
    const QString n = name.trimmed();
    for (const TuneStepEntry& e : kTuneSteps) {
        if (n.compare(QLatin1String(e.name), Qt::CaseInsensitive) == 0) { return e.hz; }
    }
    return 0;
}

QString MemoryRecord::tuneStepNameFromHz(int hz)
{
    for (const TuneStepEntry& e : kTuneSteps) {
        if (e.hz == hz) { return QLatin1String(e.name); }
    }
    return {};
}

QStringList MemoryRecord::tuneStepNames()
{
    QStringList names;
    for (const TuneStepEntry& e : kTuneSteps) { names << QLatin1String(e.name); }
    return names;
}

} // namespace Longpath
