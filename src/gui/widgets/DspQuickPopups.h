#pragma once

// =================================================================
// src/gui/widgets/DspQuickPopups.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original (Aufbau); Bereiche und Vorgaben aus Thetis
// setup.cs [v2.10.3.13] und AetherSDR MainWindow.cpp:7980-8324
// [@0cd4559] — siehe die Zitate je Regler in der .cpp.
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
