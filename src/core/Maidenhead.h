// SPDX-License-Identifier: GPL-2.0-or-later
//
// NereusSDR - Maidenhead locator geometry: grid square to lat/lon,
// great-circle distance, initial bearing.
//
// These three helpers already existed as file-scope functions inside an
// anonymous namespace in src/models/FreeDVStationModel.cpp, so nothing
// outside that file could reach them. The rotator dial needs the same
// maths for its target bearing, and a second copy of a haversine is a
// bug waiting to diverge — so the declarations are hoisted here and the
// anonymous namespace in FreeDVStationModel.cpp now closes before them.
// The definitions did not move and the FreeDV station path is unchanged.
//
// Ported from freedv-gui src/gui/dialogs/freedv_reporter.cpp:2312-2410
// (calculateDistance_ / calculateLatLonFromGridSquare_ /
// calculateBearingInDegrees_ / DegreesToRadians_ / RadiansToDegrees_)
// [@77e793a]. Haversine great-circle distance + initial bearing.
//
// License (upstream): freedv-gui carries an LGPLv2.1+ root license
// (`freedv-gui/COPYING`); the specific `freedv_reporter.cpp` file has no
// per-file Copyright header, so the project root header applies. LGPL is
// upgrade-compatible to GPLv2-or-later when linked into a GPL work
// (LGPL section 3 conversion clause), which is the model NereusSDR uses.
//
// Copyright (C) 2026 NereusSDR contributors.
// Distance / heading math: derived from freedv-gui source (LGPLv2.1+,
// copyright the freedv-gui contributors / FreeDV project).
//
// Modification history (NereusSDR)
//   2026-08-07  Martin Fischer  Declarations hoisted out of
//                               FreeDVStationModel.cpp so the rotator
//                               dial can share one implementation.
//                               No maths changed. AI tooling: Anthropic
//                               Claude (Cowork).

#pragma once

#include <QString>

namespace NereusSDR {

// Centre of the given Maidenhead square. Accepts 4- or 6-character
// locators; anything shorter leaves the outputs untouched. Takes the
// locator by value because the implementation upper-cases it in place
// (freedv-gui's `gridSquare.MakeUpper()`).
void calculateLatLonFromGridSquare(QString gridSquare,
                                   double& lat, double& lon);

// Great-circle distance between two Maidenhead squares, in kilometres.
double calculateDistanceKm(const QString& gridSquare1,
                           const QString& gridSquare2);

// Initial bearing from square 1 to square 2, in degrees true (0-360).
double calculateBearingInDegrees(const QString& gridSquare1,
                                 const QString& gridSquare2);

// A locator is usable for distance/bearing from four characters on:
// two letters, two digits, optionally two more letters.
bool isValidGridSquare(const QString& gridSquare);

} // namespace NereusSDR
