#pragma once

// =================================================================
// src/core/QsoConfirmation.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Is this contact confirmed, and by whom?
//
// The data has been in the logbook since the ADIF round-trip was fixed
// — QSL_RCVD, LOTW_QSL_RCVD, EQSL_QSL_RCVD and their sent counterparts
// all survive an import now and are written back out. None of it is
// visible anywhere. A confirmation you cannot see is not much better
// than one that was deleted, which is what used to happen to it.
//
// ── Why this is not a one-line lookup ────────────────────────────────
//
// ADIF's confirmation fields are a small mess, and every logger writes
// them slightly differently:
//
//   Y / N / R / I / V     the standard's own values, and only Y and V
//                         mean confirmed. R is "requested", I is
//                         "ignore", and a great deal of software writes
//                         them in lower case.
//   empty                 not the same as N. N means "explicitly not
//                         confirmed"; empty means nobody has said.
//   whitespace            more common than it should be.
//
// Treating anything non-empty as confirmed is the obvious
// implementation and it is wrong in the direction that matters: it
// would report a QSL you REQUESTED as one you received, and an award
// count built on that is a claim you cannot back up.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "models/LogEntry.h"

#include <QString>

namespace NereusSDR::QsoConfirmation {

// What one ADIF confirmation field says.
enum class State {
    Unknown = 0,   // absent or blank — nobody has said either way
    No,            // explicitly N
    Requested,     // R — asked for, not received. NOT a confirmation.
    Ignored,       // I — the operator has marked it as not applicable
    Confirmed,     // Y or V (V = verified)
};

// Parse one field value. Case- and whitespace-insensitive; anything
// unrecognised is Unknown rather than being guessed at.
State parse(const QString& adifValue);

// Read a named field out of an entry's preserved ADIF extras.
State field(const LogEntry& e, const QString& adifName);

// The three that matter, by the name an operator uses.
inline State qslCard(const LogEntry& e)
{ return field(e, QStringLiteral("QSL_RCVD")); }
inline State lotw(const LogEntry& e)
{ return field(e, QStringLiteral("LOTW_QSL_RCVD")); }
inline State eqsl(const LogEntry& e)
{ return field(e, QStringLiteral("EQSL_QSL_RCVD")); }

// Confirmed by ANY of the three. This is the figure most awards accept
// and the one worth showing in a list.
bool isConfirmed(const LogEntry& e);

// A short badge for the table: "L" for LoTW, "C" for a card, "e" for
// eQSL, combined. Empty when nothing is confirmed, so an unconfirmed
// contact reads as blank rather than as a row of dashes.
QString badge(const LogEntry& e);

// The same thing spelled out, for a tooltip or a detail line.
QString describe(const LogEntry& e);

} // namespace NereusSDR::QsoConfirmation
