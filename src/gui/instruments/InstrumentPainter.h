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

namespace Longpath {

class ArcSpine;
class Spine;
struct ReadingDescriptor;

namespace Instrument {

/// Erster oder zweiter Zeiger — nur die Strichstärke unterscheidet sie.
enum class NeedleWeight { Primary, Secondary };

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

// ── Segmente statt Verlauf ───────────────────────────────────────────
//
// Der Betreiber, 2026-08-20, mit einem Bildschirmfoto aus Zeus: das
// S-Meter als Kette einzelner Felder statt als durchgehender Balken.
//
// Der Unterschied ist nicht nur Zierde. Segmente liest man als MENGE —
// man schaetzt sie, ohne die Skala zu lesen, so wie man Finger zaehlt
// statt eine Linie zu messen. Ein durchgehender Balken zeigt dafuer
// kleine Aenderungen genauer: bei schwachem Signal sieht man ein
// Zittern, das die Segmente verschlucken.
//
// Beides bleibt, weil beides seinen Fall hat. Die Wahl steht im
// Rechtsklickmenue des Instruments.
//
// `f` ist der Anteil wie bei paintFade. Ein Segment leuchtet, sobald
// seine MITTE unter dem Wert liegt — nicht seine Kante: sonst
// erschiene das erste Feld schon bei einem Hauch ueber null und das
// letzte nie.
void paintSegments(QPainter& p, const Spine& s, double f, const QColor& c,
                   int count = 40, double gapPx = 1.6);

// Die Spitzenmarke: ein heller Strich quer ueber die Mulde. Steht in
// derselben Datei wie der Rest, damit Balken und Zeigerwerk dieselbe
// Marke bekommen und nicht zwei verschiedene.
void paintPeakMark(QPainter& p, const Spine& s, double f);

/// Die Glut. `intensity` ist die Deckung in der Mitte; die Vorgabe ist
/// die 16 % aus dem Entwurf.
void paintGlow(QPainter& p, const Spine& s, const QColor& c,
               double intensity = 0.16);

/// Dieselbe Glut ohne Spine — für die VFO-Variante C, in der nur die
/// Zahl steht und darunter die Glut. `clip` darf leer sein.
void paintGlow(QPainter& p, const QRectF& bounds, const QPainterPath& clip,
               const QColor& c, double intensity = 0.16);

/// Die schmale hellere Kante am aktuellen Wert — auf der GERADEN.
///
/// Auf dem Bogen ist der Wert der Zeiger selbst; dort liegt die Kante
/// auf seiner Flanke und kommt aus paintNeedle(). Ein Querstrich über
/// der Rille zeigte dort auf dieselbe Stelle ein zweites Mal.
void paintValueEdge(QPainter& p, const Spine& s, double f, const QColor& c);

/// Der Zeiger, samt der hellen Kante auf seiner vorderen Flanke.
///
/// Form, Breite, Kappe und Länge stehen so im Entwurf: gleichmässiger
/// Strich, keine Verjüngung, kein Schatten. Wer das ändern will, ändert
/// den Entwurf — bis dahin ist hier nichts hinzuerfunden.
void paintNeedle(QPainter& p, const ArcSpine& s, double f, const QColor& c,
                 NeedleWeight weight = NeedleWeight::Primary,
                 bool withEdge = true);

/// Der Nachlaufzeiger: kurzer matter Strich am äusseren Rand.
void paintPeakNeedle(QPainter& p, const ArcSpine& s, double f,
                     const QColor& c);

/// Die Nabe. Sie deckt den Zeigerfuss ab und bleibt auch dann stehen,
/// wenn keine Messung vorliegt — sie ist der Drehpunkt des Instruments
/// und behauptet keine Zahl.
void paintHub(QPainter& p, const ArcSpine& s);

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
} // namespace Longpath
