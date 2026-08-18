#pragma once

// =================================================================
// src/gui/widgets/DspQuickPopups.h  (NereusSDR)
// =================================================================
//
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis
//   source is included below — Bereiche und Vorgaben der NR-Regler
//   (udDSPNR1Taps / udDSPNR1Delay / udDSPNR1Gain / udDSPNR1Leak,
//   grpDSPGainMethod / grpDSPNR2NPEMethod / chkDSPNR2AE,
//   chkNR2PostProc_enable_rx1 und die Entsprechungen fuer NR3/NR4).
//
// Aufbau und Anordnung folgen AetherSDR MainWindow.cpp:7980-8324
// [@0cd4559] (ten9876/AetherSDR, GPLv3; NereusSDR ist ebenfalls GPLv3).
//
//
// ── Der Schnellregler-Rechtsklick ────────────────────────────────────
//
// Rechtsklick auf einen Rauschminderungsknopf oeffnet die drei bis
// fuenf Regler, die man an DIESER Rauschminderung tatsaechlich
// nachstellt — Taps, Verzoegerung, Verstaerkung, Leckrate, Lage bei
// NR1; Verstaerkungs- und Rauschschaetzverfahren bei NR2; und so
// weiter. Unten im Fenster steht ein Verweis auf die volle
// Einstellungsseite.
//
// ── Warum sie hier stehen und nicht mehr in der Flagge ───────────────
//
// Sie standen bis 2026-08-18 als sieben private Methoden in
// VfoWidget.cpp (Zeilen 3252-3533). Die VFO-Flagge faellt ersatzlos
// weg; mit ihr waeren diese 281 Zeilen gegangen, und mit ihnen 28 der
// 30 SliceModel-Setzer, die die Flagge ueberhaupt bediente.
//
// Beim zeilenweisen Abgleich Flagge gegen RxApplet am 2026-08-18 war
// das der EINE echte Fund: der erste Umzug hatte den Rechtsklick auf
// „oeffne die Einstellungsseite" abgebildet — also auf das, was im
// Schnellregler nur der Verweis GANZ UNTEN ist. Die Regler selbst
// fehlten.
//
// Freie Funktionen ueber einem SliceModel*, nicht Methoden einer
// Oberflaeche: dann kann jede Flaeche sie zeigen, und die naechste
// muss sie nicht ein zweites Mal schreiben.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-18 — Aus VfoWidget.cpp herausgeloest, damit sie deren
//                 Loeschung ueberleben. Inhalt unveraendert
//                 uebernommen. Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork).
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


#include "core/WdspTypes.h"

#include <QPoint>

#include <functional>

class QWidget;

namespace NereusSDR {

class SliceModel;

namespace DspQuickPopup {

/// Den Schnellregler zu einer Rauschminderung zeigen.
///
/// `onMore` wird vom Verweis „More Settings…" gerufen; ein leerer
/// Aufruf laesst den Verweis weg. Tut nichts ohne Scheibe.
void showFor(QWidget* parent, SliceModel* slice, NrSlot slot,
             const QPoint& globalPos,
             const std::function<void()>& onMore = {});

} // namespace DspQuickPopup
} // namespace NereusSDR
