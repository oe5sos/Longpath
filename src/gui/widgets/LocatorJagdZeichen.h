// no-port-check: Longpath-original, no Thetis logic.
//
// =================================================================
// src/gui/widgets/LocatorJagdZeichen.h  (Longpath)
// =================================================================
//
// Wie Mosaik und Live-Punkte aussehen -- an EINER Stelle fuer die flache
// Karte und die Azimutalkarte. Zwei Karten mit je eigener Vorstellung
// davon, wie ein „neues Feld" aussieht, waeren eine Legende, die auf
// einer der beiden luegt.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11 — Created in C++20/Qt6 for Longpath by Martin Fischer
//                 (OE5SOS), AI-assisted via Anthropic Claude (Claude Code).
// =================================================================
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/LocatorJagd.h"

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QVector>

class QPainter;

namespace Longpath::LocatorJagdZeichen {

/// Fuellung eines Feldes im Mosaik (mit Deckung). `Nie` ist durchsichtig.
// `kraeftig` ist der Jagdmodus: dann ist das Mosaik der Gegenstand der
// Karte und nicht ihr Hintergrund, also deckt es auch. Betreiber am
// 2026-11-11, vor der Weltkarte mit 9313 Pfaden darueber: „da kann man
// nichts erkennen".
QColor feldFuellung(FeldStand s, bool kraeftig = false);

/// Die Farbe einer Art -- Punkt, Schrift, Rahmen.
QColor farbe(LiveArt a);

/// Den Punkt einer gehoerten Station zeichnen. Neues Land: Raute mit
/// warmweissem Rand und Schein; neues Feld: Bernsteinpunkt mit Schein;
/// schon gearbeitet: Ring; sonst ein kleiner grauer Punkt.
void punkt(QPainter& p, const QPointF& c, LiveArt a);

/// Wie weit ein Klick vom Punkt entfernt sein darf (Bildpunkte).
constexpr double kTrefferPx = 10.0;

/// FT8 sendet nur vier Stellen: alle Stationen eines Feldes saessen auf
/// demselben Bildpunkt, und nur die oberste waere anklickbar. Darum
/// liegen sie auf einem kleinen Ring um die Feldmitte -- die wahre Lage
/// im Feld (rund 150 x 110 km) kennt ohnehin niemand. Je Punkt die Stelle
/// auf dem Ring (k von n im selben Feld) und der Versatz in Bildpunkten.
QVector<QPointF> versaetze(const QVector<LivePunkt>& punkte);

/// Ob das Rufzeichen neben dem Punkt steht: immer bei NEU, sonst nur,
/// wenn wenige Punkte da sind.
bool beschriften(LiveArt a, int anzahl);

/// Eine kleine Legende links unten in `rahmen`: bestaetigt, gearbeitet,
/// neu jetzt. `mitMosaik` false: nur die Live-Arten. Gibt die Hoehe.
double legende(QPainter& p, const QRectF& rahmen, bool mitMosaik, bool mitLive);

} // namespace Longpath::LocatorJagdZeichen
