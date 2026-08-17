#pragma once

// =================================================================
// src/gui/instruments/InstrumentPainter.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// ── Die drei Mittel, an einer Stelle ─────────────────────────────────
//
// OE5SOS, 2026-08-17, für alle drei neuen Widgets:
//
//   „Mulde mit Innenschatten an der oberen Kante (Grund kInsetBg,
//    darüber eine dunkle Kante mit etwa 75 % Deckung).
//    Auslaufender Verlauf bis zum aktuellen Wert: innen null, aussen
//    etwa ein Viertel Deckkraft in Messwertfarbe.
//    Glut bei 16 %, die über gut die halbe Höhe reicht und dort
//    verschwindet.
//    Eine schmale hellere Kante am aktuellen Wert, damit das Auge ihn
//    findet, bevor es den Zeiger sucht."
//
// Diese Datei ist die eine Stelle. Jedes der vier Mittel steht hier
// genau einmal und arbeitet über Spine, also für Bogen und Gerade
// gleichermassen. Wer ein fünftes Instrument baut, ruft hierher und
// schreibt nichts nach.
//
// Die Deckungen sind KEINE freien Zahlen. Sie stammen aus den
// Entwürfen des Betreibers und stehen als benannte Konstanten in der
// .cpp, mit der Zeile, aus der sie kommen.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QColor>
#include <QList>

class QPainter;
class QPainterPath;
class QRectF;

namespace NereusSDR {

class Spine;
struct ReadingDescriptor;

namespace Instrument {

/// Die Mulde: Grund in inset-bg, darüber die dunkle Kante.
///
/// `thresholdFraction` färbt den Abschnitt ab dort bis zum Ende in
/// danger. Ein Wert ausserhalb 0..1 lässt ihn weg — auf der
/// Empfangsskala gibt es keine Schwelle und darum auch keinen roten
/// Abschnitt. Die Grenze bleibt eine harte Kante; ein Übergang wäre
/// hier gefällig und falsch.
void paintTrough(QPainter& p, const Spine& s,
                 double thresholdFraction = -1.0);

/// Der auslaufende Verlauf bis zum Anteil f.
void paintFade(QPainter& p, const Spine& s, double f, const QColor& c);

/// Die Glut. `intensity` ist die Deckung in der Mitte; die Vorgabe ist
/// die 16 % aus dem Entwurf.
void paintGlow(QPainter& p, const Spine& s, const QColor& c,
               double intensity = 0.16);

/// Dieselbe Glut ohne Spine — für die VFO-Variante C, in der nur die
/// Zahl steht und darunter die Glut. `clip` darf leer sein.
void paintGlow(QPainter& p, const QRectF& bounds, const QPainterPath& clip,
               const QColor& c, double intensity = 0.16);

/// Die schmale hellere Kante am aktuellen Wert.
void paintValueEdge(QPainter& p, const Spine& s, double f, const QColor& c);

/// Beschriftete und feine Teilstriche einer Messgrösse.
///
/// Nimmt den Deskriptor statt zweier Listen, damit die Umrechnung
/// Wert → Anteil an der einen Stelle bleibt, an der die Kennlinie
/// steht (die Empfangsskala ist über S9 gestaucht).
void paintTicks(QPainter& p, const Spine& s, const ReadingDescriptor& d);

/// Welche Farbe gilt — Messwert oder Gefahr. Eine Stelle, damit Zeiger,
/// Sektor und Glut nicht auseinanderlaufen können: sie „wechseln
/// gemeinsam".
QColor valueColour(const ReadingDescriptor& d, double value);

// ── Die drei Töne, an einer Stelle ───────────────────────────────────
//
// Damit die Instrumente sie nicht je Datei aus role() zusammensuchen
// und dabei einer den Rückfallwert falsch tippt. „measured-dim" hat
// heute keine Konstante in StyleConstants.h — HGauge.cpp:13 hält den
// Wert dateilokal. Hier steht er ein zweites Mal, und das ist eine
// Stelle zu viel; zusammengeführt gehört er nach StyleConstants.h,
// was aber jede Gauge im Baum berührt und darum dem Betreiber gehört.
QColor measured();
QColor measuredDim();
QColor danger();

} // namespace Instrument
} // namespace NereusSDR
