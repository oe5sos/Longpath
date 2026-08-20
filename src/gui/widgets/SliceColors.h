#pragma once

// =================================================================
// src/gui/widgets/SliceColors.h  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR (GPLv3):
//   src/gui/SliceColors.h [@0cd4559] — die vier Scheibenfarben.
//   NereusSDR ist ebenfalls GPLv3; Attribution nach GPLv3 §5.
//
// ── Die Farbe einer Scheibe, an einer Stelle ─────────────────────────
//
// A cyan, B magenta, C gruen, D gelb. Sie faerbt das Abzeichen der
// Scheibe, ihre Marke auf dem Panadapter und ihren Reiter in der
// RxApplet — drei Flaechen, EINE Tabelle. Liefen sie auseinander,
// waere dieselbe Scheibe an drei Stellen verschieden eingefaerbt, und
// genau davon lebt die Zuordnung.
//
// Sie stand bis 2026-08-18 als statische Methode auf VfoWidget, mit
// dem Vermerk „Public static so the RX applet's per-slice tab row
// (Phase 3F Bug 3) shares the exact flag palette". Mit der Loeschung
// der VFO-Flagge waere sie gegangen und haette die RxApplet
// mitgenommen — also steht sie jetzt fuer sich.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-18 — Aus VfoWidget herausgeloest, damit sie deren
//                 Loeschung ueberlebt. Inhalt unveraendert.
//                 Martin Fischer, AI-assisted via Anthropic Claude
//                 (Cowork).
// =================================================================

#include <QColor>

namespace Longpath {

/// Die Farbe der Scheibe mit diesem Index. Unbekannte Indizes
/// bekommen die Farbe von A — eine fuenfte Scheibe faellt damit auf,
/// ohne dass etwas ohne Farbe dasteht.
inline QColor sliceColor(int index)
{
    // From AetherSDR SliceColors.h
    switch (index) {
    case 0: return QColor(Style::kBlueText);  // cyan
    case 1: return QColor(Style::kTxRed);  // magenta
    case 2: return QColor(Style::kGreenText);  // green
    case 3: return QColor(0xff, 0xff, 0x00);  // yellow
    default: return QColor(Style::kBlueText);
    }
}

} // namespace Longpath
