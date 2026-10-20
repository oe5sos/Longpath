#pragma once

// =================================================================
// src/core/Sprache.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Deutsch ist die Sprache des Hauses, Englisch bleibt möglich.
//
// „generell sollte für mich primär alles in deutsch sein, eine
//  englische version sollte es natürlich auch geben."
//
// ── Warum kein Qt-Übersetzungssystem (noch nicht) ────────────────────
//
// Qts Weg ist tr() an jeder Stelle plus .ts/.qm-Dateien. Das ist der
// richtige Weg für ein Programm, das in zehn Sprachen erscheint --
// aber er verlangt, dass jede der rund zweihundert Zeichenketten im
// Kanalzugfenster am Aufrufort angefasst wird, und das in einer
// Datei, in der dieselbe Schreibweise auch für Einstellungsschlüssel,
// Objektnamen und Stilvorlagen benutzt wird. Ein Suchen-und-Ersetzen
// darüber übersetzt irgendwann einen Schlüssel und bricht die
// gespeicherten Einstellungen.
//
// Hier steht deshalb das Gegenteil: NICHT die Aufrufe anfassen,
// sondern den fertigen Widget-Baum einmal durchgehen und jeden
// sichtbaren Text austauschen, der in der Tabelle steht. Ein Aufruf
// je Fenster statt zweihundert Änderungen.
//
// Was das kostet, ehrlich:
//
//   * Texte, die erst zur Laufzeit zusammengesetzt werden (die
//     Messzeilen mit %1), erwischt der Durchgang nicht -- die
//     bekommen ein T() um ihre Vorlage.
//   * lupdate findet nichts. Unsere Tabelle IST der Katalog.
//   * Der Umstieg auf tr() später ist mechanisch: aus T("x") wird
//     tr("x"), und die Tabelle wird zur .ts-Datei.
//
// ── Warum das Englische die Quelle bleibt ────────────────────────────
//
// Weil der Quelltext und die Tabelle sonst auseinanderlaufen: steht
// im Code Deutsch, braucht die englische Fassung eine zweite
// Tabelle, und zwei Tabellen stimmen nie lange überein. So gibt es
// genau eine Richtung -- Englisch im Code, Deutsch in der Tabelle --
// und wenn ein Text fehlt, erscheint der englische statt gar keiner.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-10 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Claude Code).
// =================================================================

#include <QString>

class QWidget;

namespace Longpath::Sprache {

enum class Lang { Deutsch, English };

// Gelesen aus AppSettings ("Sprache"), Vorgabe Deutsch.
Lang sprache();
void setzeSprache(Lang l);

// Ein einzelner Text. Gibt bei Englisch -- oder wenn nichts in der
// Tabelle steht -- den übergebenen zurück.
QString T(const QString& englisch);

// Den fertigen Baum durchgehen: Beschriftungen, Knöpfe, Haken,
// Gruppen, Auswahlfelder, Reiter, Kurzhilfen und der Fenstertitel.
// Mehrfaches Aufrufen schadet nicht -- ein bereits deutscher Text
// steht nicht in der Tabelle und bleibt, wie er ist.
void uebersetzeBaum(QWidget* wurzel);

// Für Prüfstände: wie viele Einträge die Tabelle hat, und ob zu
// einem englischen Text einer da ist.
int  eintraege();
bool hatUebersetzung(const QString& englisch);

} // namespace Longpath::Sprache
