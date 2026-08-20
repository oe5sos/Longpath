#pragma once

// =================================================================
// src/gui/styles/ThemeQss.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// ── Why a substitution and not a refactor ────────────────────────────
//
// 2026-08-15. The operator asked to move the whole interface toward the
// Zeus house style, starting with the palette. The obvious first step
// was "pull every hex literal into StyleConstants.h, then change one
// file". I estimated thirty files. Counted, it is:
//
//     1733 hex literals · 276 distinct colours · 130 files
//     45 of them have a name today
//     1115 literals match a named constant exactly
//     618 are colours that are in no palette at all
//
// and — the part that decides the approach — **1141 of them sit inside
// Qt stylesheet strings**:
//
//     w->setStyleSheet("QLabel { color: #c8d8e8; font-size: 13px; }");
//
// A string like that cannot take a constant. It has to be rebuilt as an
// .arg() chain, one call site at a time, across a hundred and thirty
// files, with no way to compile between edits — I cannot build this
// project; only the operator's Mac can. That is a recipe for handing
// back a broken tree.
//
// ── What this does instead ───────────────────────────────────────────
//
// One function that rewrites the legacy palette values in a stylesheet
// to whatever the active theme says they are:
//
//     w->setStyleSheet(Style::themed("QLabel { color: #c8d8e8; }"));
//
// Two properties make it worth the indirection:
//
//   · **Today it changes nothing.** The map is the identity until a
//     theme is switched on, so wrapping a call site is invisible and
//     cannot break anything. The risky work (touching every call site)
//     is separated from the visible work (changing the colours).
//
//   · **It goes file by file.** A wrapped file follows the new theme; an
//     unwrapped one keeps the old colours; nothing is broken in
//     between. There is no flag day.
//
// When every call site is wrapped, the palette change is one table.
//
// This is a scaffold, not an ideal. The ideal is named tokens in every
// stylesheet, and this does not stop anyone reaching it — a call site
// rebuilt as an .arg() chain simply stops needing themed(). It exists
// so the palette can move before that work is finished, rather than
// after.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QString>
#include <QVector>

namespace Longpath::Style {

/// One row of the theme table.
///
/// `role` ist der Name, unter dem eine Theme-Datei die Farbe ansprechen
/// kann — kurz, kleingeschrieben, stabil. Er darf sich nicht ändern,
/// sobald jemand eine Datei damit geschrieben hat; ein umbenannter
/// Schlüssel ist eine stille Farbänderung beim nächsten Start.
struct ThemeEntry {
    const char* role;     ///< z. B. "border", "accent", "measured"
    const char* legacy;   ///< was der Nereus-Quelltext hinschreibt
    const char* current;  ///< was Nereus heute daraus malt
};

/// The whole map, in the order it is applied. Public so a test can walk
/// it — the assertion that matters is that every constant in
/// StyleConstants.h appears here, because a constant with no row is a
/// colour that will silently stay behind when the theme moves.
const QVector<ThemeEntry>& themeTable();

/// Rewrite the legacy palette values in a Qt stylesheet.
///
/// Case-insensitive on the hex digits, since the code writes both
/// `#00B4D8` and `#00b4d8`. Anything not in the table is left exactly
/// as it was: this must never guess.
///
/// Idempotent — themed(themed(s)) == themed(s) — so a call site that
/// gets wrapped twice by mistake is harmless.
QString themed(QString qss);

/// True when the string still holds a legacy value the table knows
/// about. For tests and for the audit script; not used at runtime.
bool hasLegacyColour(const QString& qss);

/// Was an dieser Stelle gemalt werden soll: die Theme-Datei, wenn sie
/// eine Meinung hat, sonst der Nereus-Wert. Für Malcode, der kein
/// Stylesheet hat — QPainter, Charts, Instrumente.
///
///     p.setPen(QColor(Style::role("measured", Style::kAmberText)));
QString role(const char* roleName, const char* fallback);

} // namespace Longpath::Style
