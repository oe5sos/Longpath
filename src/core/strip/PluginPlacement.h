#pragma once

// =================================================================
// src/core/strip/PluginPlacement.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Wo gehoert dieses Plugin hin?
//
// Der Wunsch, der das ausgeloest hat: „wichtig ist, dass man weiss,
// wo man diese einbauen soll. das muss deutlich markiert sein,
// vielleicht in gruppen oder so."
//
// Eine Audio Unit sagt nicht, was sie ist. Sie hat einen Namen, einen
// Hersteller und eine Liste von Reglern -- keine Gattung, keine
// Kategorie, nichts, woraus sich „das ist ein Kompressor" ableiten
// liesse. Wer also Gruppen will, muss raten, und die ehrliche Frage
// ist nur: raten mit oder ohne Netz.
//
// Hier wird mit Netz geraten. Eine kleine Tabelle bekannter Namen und
// Wortstaemme ordnet zu, was sich zuordnen laesst; alles andere landet
// sichtbar unter „noch nicht eingeordnet" und nicht etwa stillschweigend
// in der erstbesten Gruppe. Ein falsch einsortiertes Plugin waere
// schlimmer als ein nicht einsortiertes: es behauptet etwas.
//
// Die Vorschlagstelle ist eine EMPFEHLUNG, kein Zwang. Hingezogen wird
// von Hand, und wer einen Kompressor vor den Entzerrer haengen will,
// soll das tun -- das ist genau der Grund, aus dem die Stelle frei
// waehlbar ist.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20/Qt6, AI-assisted via Anthropic
//                 Claude (Claude Code), operator Martin Fischer.
// =================================================================

#include "core/strip/StripChain.h"

#include <QString>
#include <QStringList>

namespace Longpath::PluginPlacement {

enum class Group {
    Clean = 0,      // Rauschen, Raum, Tore
    Eq,             // Entzerren
    DeEss,          // Zischlaute
    Compress,       // Kompressoren, Pegelsteller
    Colour,         // Saettigung, Roehre, Erreger
    Final,          // Kapper, Begrenzer
    Measure,        // Anzeigen, die nichts tun
    Unknown
};

inline constexpr int kGroupCount = 8;

// Die Ueberschrift, unter der die Gruppe steht.
QString groupName(Group g);

// Ein Satz dazu, warum sie dort steht.
QString groupHint(Group g);

// Die empfohlene Stelle in der Kette (0 = vor dem Gate,
// StripChain::kStageCount = hinter dem Begrenzer). Fuer Measure und
// Unknown gibt es keine; dann kommt -1.
int suggestedPosition(Group g);

// Raten, mit Netz. Name und Hersteller, wie der Wirt sie meldet.
Group guess(const QString& name, const QString& vendor);

// ── Was das Ding eigentlich tut ──────────────────────────────────────
//
// „ggf auch beim drüber scrollen eine kurze beschreibung, um was es
//  bei dem plugin geht und was es verbessert."
//
// Eine Audio Unit sagt ihren Zweck nicht; sie nennt Hersteller, Namen
// und ihre Parameter, mehr nicht. Also zwei Quellen, in dieser
// Reihenfolge:
//
//   1. Eine Tabelle der Stücke, die wir WIRKLICH kennen -- Apples
//      eigene (die auf jedem Mac liegen) und die, die hier
//      ausgesucht und geprüft wurden. Da steht ein echter Satz.
//   2. Sonst die Gruppe und was sie im Sendeweg tut.
//
// Was nicht in der Tabelle steht, bekommt KEINE erfundene
// Beschreibung. „Vermutlich ein Entzerrer -- die Zuordnung ist
// geraten" ist eine ehrliche Auskunft; ein ausgedachter Werbetext
// wäre keine.
QString beschreibung(const QString& name, const QString& vendor);

// Ob zu diesem Namen ein echter Satz vorliegt (und nicht nur die
// Gruppenauskunft). Für die Oberfläche, die das unterscheiden will.
bool istBekannt(const QString& name);

} // namespace Longpath::PluginPlacement
