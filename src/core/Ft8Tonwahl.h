#pragma once

// =================================================================
// src/core/Ft8Tonwahl.h  (Longpath)
// =================================================================
//
// Eine freie Sendestelle im Durchlassbereich suchen.
//
// Longpath-original. Weder WSJT-X noch MSHV tun das: dort ruft man
// auf der Tonlage, die eingestellt ist, und ob dort schon jemand
// liegt, sieht man selbst im Wasserfall -- oder eben nicht.
//
// ── Warum das ueberhaupt geht ───────────────────────────────────────
//
// Longpath weiss aus den eigenen Funden, WO Stationen senden: jeder
// Fund traegt seine Tonlage. Das ist eine bessere Auskunft als die
// Energie im Wasserfall, denn sie sagt „hier sendet jemand" und nicht
// „hier ist Rauschen".
//
// Die Grenze dazu, und sie gehoert dazu: Stationen, die zu schwach
// zum Dekodieren sind, stehen nicht in der Liste. Sie stoeren aber
// auch weniger -- wer unter der Dekodierschwelle liegt, deckt keinen
// zu. Eine freie Stelle nach dieser Rechnung ist also „frei von
// allem, was hier gehoert wird", nicht „frei von allem".
//
// ── Was eine Stelle belegt ──────────────────────────────────────────
//
// Ein FT8-Signal ist 50 Hz breit (8 Toene zu 6,25 Hz). Zwei Signale
// nebeneinander brauchen mehr: MSHV setzt seine gleichzeitigen
// Aussendungen 60 Hz auseinander
// (`gen_ft8.cpp:262`, uebernommen als `Ft8TxSource::kPlatzAbstandHz`).
// Dieselbe Zahl gilt hier.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QVector>

namespace Longpath {

/// Eine freie Sendestelle suchen.
///
/// `belegt`    Tonlagen, auf denen gehoert wurde (Hz, ungeordnet).
/// `von`/`bis` der erlaubte Bereich.
/// `abstand`   wie weit es zur naechsten belegten Stelle sein muss.
/// `wunsch`    wo man am liebsten bliebe.
///
/// Gibt `wunsch` zurueck, wenn dort frei ist -- eine Sendestelle
/// ohne Not zu wechseln ist schlecht: wer einen ruft, sucht einen
/// dort, wo er zuletzt war.
///
/// Sonst die FREIE Stelle, die dem Wunsch am naechsten liegt. Gibt es
/// keine, kommt `wunsch` unveraendert zurueck -- ein Ergebnis
/// ausserhalb des Bereichs waere schlimmer als keine Aenderung, und
/// eine stille Null waere am schlimmsten.
int freieTonlage(const QVector<int>& belegt, int von, int bis,
                 int abstand, int wunsch);

/// Liegt auf dieser Stelle jemand?
bool stelleBelegt(const QVector<int>& belegt, int hz, int abstand);

} // namespace Longpath
