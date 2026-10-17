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

// ── Die ZZ-Zahl (zwei Stellen) ────────────────────────────────────────
//
// From Thetis CAT/CATCommands.cs:9901-9945 [v2.10.3.15] -- Mode2String,
// und :3949-3969 -- ZZMD (Bereichspruefung 0..11).
//
// **Die ZZ-Zahl IST der DSPMode-Wert.** Thetis' Mode2String zaehlt
// LSB=00, USB=01, DSB=02, CWL=03, CWU=04, FM=05, AM=06, DIGU=07,
// SPEC=08, DIGL=09, SAM=10, DRM=11 -- genau die Reihenfolge von
// `enum class DSPMode` in WdspTypes.h, weil dieses enum aus Thetis
// stammt. Trotzdem steht hier eine ausgeschriebene Tabelle und kein
// `static_cast<int>`:
//
//   * Longpath hat zwei Modi MEHR (RADE_U=12, RADE_L=13, Phase 3R).
//     Thetis kennt sie nicht. Ein Cast schickte einem Logbuchprogramm
//     eine "12", die in keiner seiner Tabellen steht.
//   * Wuerde jemand DSPMode spaeter umsortieren oder einen Wert
//     dazwischen einfuegen, waere ein Cast still falsch. Die Tabelle
//     bricht dann im Prueffall.
//
// Der ZZ-Satz und der Kenwood-Satz sind NICHT voneinander ableitbar,
// und sie sind auch keine Doppelung: beide sind verlustbehaftete
// Abbildungen DESSELBEN Modus, jede auf andere Weise. Kenwood kennt
// weder DSB noch SAM noch DRM und hat fuer CW nur EINE Zahl (3), also
// verliert ZZ->Kenwood die Seitenbandwahl; Kenwood->ZZ muesste sie
// erraten. Darum fuehrt die Verdrahtung BEIDE Zahlen aus dem einen
// Longpath-Modus, statt eine aus der anderen zu rechnen.
constexpr int catZzZahl(DSPMode m) noexcept
{
    switch (m) {
    case DSPMode::LSB:  return 0;
    case DSPMode::USB:  return 1;
    case DSPMode::DSB:  return 2;
    case DSPMode::CWL:  return 3;
    case DSPMode::CWU:  return 4;
    case DSPMode::FM:   return 5;
    case DSPMode::AM:   return 6;
    case DSPMode::DIGU: return 7;
    case DSPMode::SPEC: return 8;
    case DSPMode::DIGL: return 9;
    case DSPMode::SAM:  return 10;
    case DSPMode::DRM:  return 11;
    // RADE hat in Thetis keine Entsprechung. Gemeldet wird das
    // SEITENBAND, nicht "unbekannt": ein Logbuchprogramm traegt damit
    // USB bzw. LSB ins Log, was fuer den Rapport stimmt. Thetis gibt in
    // seinem default "?;" zurueck -- das waere hier schaedlich, weil
    // dieselbe Funktion das Modusfeld MITTEN im 36-Zeichen-Statussatz
    // fuellt (ZZIF): ein "?;" darin zerschiesst jedes Feld dahinter.
    case DSPMode::RADE_U: return 1;
    case DSPMode::RADE_L: return 0;
    }
    return 1;   // wie Thetis' default-Zweig: USB
}

/// Rueckweg. Ein Wert ausserhalb 0..11 wird NICHT auf USB geschoben,
/// sondern ist ungueltig -- Thetis' ZZMD lehnt ihn mit `?;` ab
/// (CATCommands.cs:3953), und ein Logbuchprogramm, dem ein Tippfehler
/// stillschweigend USB einstellt, sendet im falschen Seitenband.
/// Der Aufrufer prueft darum zuerst `catZzZahlGueltig()`.
constexpr bool catZzZahlGueltig(int z) noexcept { return z >= 0 && z <= 11; }

constexpr DSPMode catModusAusZzZahl(int z) noexcept
{
    switch (z) {
    case 0:  return DSPMode::LSB;
    case 1:  return DSPMode::USB;
    case 2:  return DSPMode::DSB;
    case 3:  return DSPMode::CWL;
    case 4:  return DSPMode::CWU;
    case 5:  return DSPMode::FM;
    case 6:  return DSPMode::AM;
    case 7:  return DSPMode::DIGU;
    case 8:  return DSPMode::SPEC;
    case 9:  return DSPMode::DIGL;
    case 10: return DSPMode::SAM;
    case 11: return DSPMode::DRM;
    default: return DSPMode::USB;
    }
}

} // namespace Longpath
