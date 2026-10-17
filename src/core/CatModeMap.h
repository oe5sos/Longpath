// =================================================================
// src/core/CatModeMap.h  (Longpath)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/CAT/CATCommands.cs [v2.10.3.15]
//     Mode2KString (9996-10039) und KString2Mode (9953-9992)
// Original licence from the Thetis source is included below.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-11 — Reimplemented in C++20 for Longpath, AI-assisted via
//                 Anthropic Claude Code. Eigene Datei, weil CatProtocol
//                 DSPMode absichtlich NICHT kennt: die Protokollschicht
//                 rechnet mit Kenwood-Zahlen, damit sie am Schreibtisch
//                 ohne Longpaths Modusbegriff pruefbar bleibt. Hier
//                 sitzen die zwei Richtungen zusammen, damit niemand
//                 eine aendert und die andere vergisst.
// =================================================================
//
//=================================================================
// CATCommands.cs
//=================================================================
// Copyright (C) 2005  Bob Tracy
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
// You may contact the author via email at: k5kdn@arrl.net
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================

#pragma once

#include "WdspTypes.h"

namespace Longpath {

/// DSPMode -> Kenwood-Zahl.
///
/// From Thetis CAT/CATCommands.cs:9996-10039 [v2.10.3.15] --
/// Mode2KString:
///   LSB=1 USB=2 CWU=3 FM=4 AM=5 DIGL=6 CWL=7 DIGU=9
///
/// Thetis gibt bei gesetztem `DigUIsUSB` fuer DIGL/DIGU die 1/2 zurueck
/// (also LSB/USB) -- eine Einstellung fuer Betreiber, die ihre
/// Digitalmodi wie normale Seitenbaender behandeln wollen. Longpath hat
/// diese Einstellung nicht; wird sie je gebraucht, gehoert sie als
/// Parameter hierher und nicht in die Aufrufer.
///
/// Alles, was Kenwood nicht kennt (SAM, DRM, SPEC, RADE_U/L), gibt die
/// 2 (USB). Thetis macht dasselbe auf dem Umweg ueber seinen Fehlerwert:
/// `if(tempmode == "?;") rtn += "2";` in IF(). Eine 0 oder ein leeres
/// Feld wuerde den 35-Zeichen-Statussatz verkuerzen und damit jedes
/// Feld dahinter verschieben.
constexpr int catKenwoodZahl(DSPMode m) noexcept
{
    switch (m) {
    case DSPMode::LSB:  return 1;
    case DSPMode::USB:  return 2;
    case DSPMode::CWU:  return 3;
    case DSPMode::FM:   return 4;
    case DSPMode::AM:   return 5;
    case DSPMode::DIGL: return 6;
    case DSPMode::CWL:  return 7;
    case DSPMode::DIGU: return 9;
    default:            return 2;   // wie Thetis: unbekannt -> USB
    }
}

/// Kenwood-Zahl -> DSPMode.
///
/// From Thetis CAT/CATCommands.cs:9953-9992 [v2.10.3.15] --
/// KString2Mode. Die 8 ist in Kenwoods Satz FSK-R und bei Thetis
/// unbesetzt; sie faellt wie jede andere unbekannte Zahl auf USB, genau
/// wie Thetis' `default`.
constexpr DSPMode catModusAusKenwoodZahl(int k) noexcept
{
    switch (k) {
    case 1: return DSPMode::LSB;
    case 2: return DSPMode::USB;
    case 3: return DSPMode::CWU;
    case 4: return DSPMode::FM;
    case 5: return DSPMode::AM;
    case 6: return DSPMode::DIGL;
    case 7: return DSPMode::CWL;
    case 9: return DSPMode::DIGU;
    default: return DSPMode::USB;   // wie Thetis' default
    }
}

} // namespace Longpath
