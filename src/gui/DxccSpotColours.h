// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original.
//
// DXCC-Farben der Spots am Panadapter (2026-09-27).
//
// DxccColorProvider (aus AetherSDR) war gebaut, aber nie eingeschaltet:
// isEnabled() blieb false, keine Einstellung, und bis 2026-09-27 fehlten
// auch cty.dat und das eigene Log. Betreiber: "entscheide selbst und
// setze um". Eingeschaltet, abschaltbar im Spot-Hub (Display), und in
// den Farben des Hauses statt AetherSDRs Rot/Orange/Gold/Grau -- Rot
// bleibt in Longpath der Warnung (Sendetaste 2026-09-02), und kraeftige
// Farbe steht an genau einer Stelle:
//
//   neues Land            warmes Bernstein -- die eine kraeftige Farbe
//   neues Band / Betriebsart   gedecktes Messing
//   schon gearbeitet      keine eigene Farbe: der Spot behaelt die Farbe
//                         seiner Quelle (SpectrumWidget: DXCC-Farbe nur,
//                         wenn gueltig)

#pragma once

#include "core/AppSettings.h"
#include "core/DxccColorProvider.h"
#include "gui/StyleConstants.h"

#include <QColor>

namespace Longpath {

inline constexpr const char* kDxccColoringKey = "IsDxccColoringEnabled";

inline bool dxccColoringEnabled()
{
    return AppSettings::instance()
               .value(QString::fromLatin1(kDxccColoringKey), QStringLiteral("True"))
               .toString() == QStringLiteral("True");
}

inline void applyDxccSpotColours(DxccColorProvider& dxcc)
{
    dxcc.setEnabled(dxccColoringEnabled());
    dxcc.colorNewDxcc = QColor(QString::fromLatin1(Style::kBusyAmber));
    dxcc.colorNewBand = QColor(QString::fromLatin1(Style::kAmberWarn));
    dxcc.colorNewMode = QColor(QString::fromLatin1(Style::kAmberWarn));
    dxcc.colorWorked  = QColor();   // ungueltig: Farbe der Quelle bleibt
}

} // namespace Longpath
