// =================================================================
// src/core/FrequenzEichung.h  (Longpath)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (Longpath):
//   2026-11-08 — Reimplemented in C++20/Qt6 for Longpath by Martin
//                 Fischer (OE5SOS), with AI-assisted transformation via
//                 Anthropic Claude Code. Thetis v2.10.3.15-5 [@852bf0e].
// =================================================================

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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#pragma once

#include <QVector>

namespace Longpath::FrequenzEichung {

// ── Einmessen an einem Traeger bekannter Frequenz ───────────────────
//
// Thetis' Setup -> Calibration -> "Freq Cal" stimmt den VFO auf die
// Eichfrequenz, mittelt zehn Spektren, sucht +/-2500 Hz um die Mitte
// nach dem staerksten Bin und setzt daraus den Korrekturfaktor:
//   From Thetis console.cs:9745 [@852bf0e]:
//     for (int i = 0; i < 10; i++)  // average 10 spectra to reduce noise
//   From Thetis console.cs:9755 [@852bf0e]:
//     const double cal_range = 2500.0;  // look +/- this much from current freq to find the calibration signal
//   From Thetis console.cs:9770-9771 [@852bf0e]:
//     double diff = bin_width * (fft_size / 2 - max_index);
//     double correct_factor = 1.0d - ((diff / (double)(freq * 1e6)));
//
// Longpath misst an der angeklickten Stelle statt den VFO hinzustellen,
// und mit dem Faktor, der gerade gilt (Thetis setzt ihn vorher auf 1.0).
// Darum wird der alte Faktor mit dem gemessenen Rest multipliziert.
// Zwei Zusaetze, Longpath-eigen: eine Parabel durch die drei Bins um
// die Spitze (Thetis rechnet auf ganze Bins -- bei 49 Hz je Bin sind das
// auf 10 MHz fast 5 ppm), und eine Schwelle von 10 dB ueber dem Median
// des Suchfensters, damit nicht auf Rauschen geeicht wird.

inline constexpr int    kMittelBilder = 10;      // console.cs:9745
inline constexpr double kSuchHalbHz   = 2500.0;  // console.cs:9755
inline constexpr float  kMindestSnrDb = 10.0f;   // Longpath-eigen

struct Treffer {
    bool   gefunden{false};
    double hz{0.0};        ///< Frequenz am Geraet (Mitte des Traegers)
    float  snrDb{0.0f};    ///< Spitze ueber dem Median des Suchfensters
};

/// Sucht den staerksten Traeger in [mitteHz - halbHz, mitteHz + halbHz].
/// `leistung` sind gemittelte lineare Leistungen je Bin; Bin k reicht
/// von fftUntenHz + k*binHz bis + binHz (Mitte bei k + 0,5).
Treffer sucheTraeger(const QVector<double>& leistung, double binHz,
                     double fftUntenHz, double mitteHz,
                     double halbHz = kSuchHalbHz);

/// Der neue Korrekturfaktor, wenn ein Traeger der wahren Frequenz
/// `wahrHz` bei `gemessenHz` erscheint (beide am Geraet). Thetis'
/// Formel auf den Rest angewandt: alt * (1 - (wahr - gemessen) / wahr).
double neuerFaktor(double alterFaktor, double gemessenHz, double wahrHz);

/// Abweichung in ppm (gemessen gegen wahr), zur Anzeige.
double ppm(double gemessenHz, double wahrHz);

} // namespace Longpath::FrequenzEichung
