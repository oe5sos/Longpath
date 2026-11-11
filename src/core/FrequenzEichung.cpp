// =================================================================
// src/core/FrequenzEichung.cpp  (Longpath)
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

// Siehe FrequenzEichung.h fuer die Herkunft jeder Zahl.

#include "core/FrequenzEichung.h"

#include <algorithm>
#include <cmath>

namespace Longpath::FrequenzEichung {

Treffer sucheTraeger(const QVector<double>& leistung, double binHz,
                     double fftUntenHz, double mitteHz, double halbHz)
{
    Treffer t;
    const int n = leistung.size();
    if (n < 3 || binHz <= 0.0 || halbHz <= 0.0) { return t; }
    const int erster = std::max(1, static_cast<int>(
        std::floor((mitteHz - halbHz - fftUntenHz) / binHz)));
    const int letzter = std::min(n - 2, static_cast<int>(
        std::ceil((mitteHz + halbHz - fftUntenHz) / binHz)));
    if (letzter - erster < 2) { return t; }

    int best = erster;
    QVector<double> fenster;
    fenster.reserve(letzter - erster + 1);
    for (int k = erster; k <= letzter; ++k) {
        fenster.push_back(leistung[k]);
        if (leistung[k] > leistung[best]) { best = k; }
    }
    std::nth_element(fenster.begin(), fenster.begin() + fenster.size() / 2, fenster.end());
    const double median = fenster[fenster.size() / 2];
    auto db = [](double p) { return 10.0 * std::log10(std::max(p, 1.0e-30)); };
    t.snrDb = static_cast<float>(db(leistung[best]) - db(median));
    if (t.snrDb < kMindestSnrDb) { return t; }

    // Parabel durch die drei dB-Werte um die Spitze.
    const double a = db(leistung[best - 1]);
    const double b = db(leistung[best]);
    const double c = db(leistung[best + 1]);
    const double nenner = a - 2.0 * b + c;
    double delta = (std::abs(nenner) > 1.0e-12) ? 0.5 * (a - c) / nenner : 0.0;
    delta = std::clamp(delta, -0.5, 0.5);
    t.hz = fftUntenHz + (best + 0.5 + delta) * binHz;
    t.gefunden = true;
    return t;
}

double neuerFaktor(double alterFaktor, double gemessenHz, double wahrHz)
{
    if (wahrHz <= 0.0) { return alterFaktor; }
    const double diff = wahrHz - gemessenHz;
    return alterFaktor * (1.0 - diff / wahrHz);
}

double ppm(double gemessenHz, double wahrHz)
{
    if (wahrHz <= 0.0) { return 0.0; }
    return (gemessenHz - wahrHz) / wahrHz * 1.0e6;
}

} // namespace Longpath::FrequenzEichung
