#pragma once

// =================================================================
// src/core/AdifLog.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Reading and rewriting the whole logbook file.
//
// NOT core/AdifParser.h. That one answers "which DXCC entities have I
// worked on this band" and keeps four fields per contact, because it
// exists to colour the band plan and is fed by importing somebody
// else's log. This one round-trips complete LogEntry records so the
// logbook window can show, correct and export them. Two readers of the
// same file format, with different jobs and different appetites.
//
// The writer rewrites the entire file. That is the right trade for a
// log of the size one operator makes — tens of thousands of records is
// a few megabytes — and it means a correction cannot leave the file
// half-updated the way an in-place edit could.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "models/LogEntry.h"

#include <QString>
#include <QVector>

namespace NereusSDR {

namespace AdifLog {

// Parse ADIF text into records. Fields this program does not model are
// kept in LogEntry::extras rather than dropped, and written back out
// untouched — see the note there for why dropping them was data loss
// and not merely untidy.
//
// Lengths are counted in BYTES of the UTF-8 file, not in QChars
// (2026-08-10). ADIF's <NAME:length> counts what is on disk, and every
// other logger counts bytes; counting UTF-16 code units mis-sliced any
// record whose NAME, QTH or COMMENT carried an umlaut. The byte
// overload is the real parser; the QString one converts and delegates.
QVector<LogEntry> parse(const QByteArray& bytes);
QVector<LogEntry> parse(const QString& text);

// Read the file at `path`. A missing file is not an error — it is an
// empty log, which is what a new installation has.
QVector<LogEntry> read(const QString& path, QString* error = nullptr);

// Rewrite `path` with exactly these records, header included.
//
// Writes to a temporary file and renames over the original, so an
// interrupted save leaves the previous log intact instead of a
// truncated one. Losing a logbook to a crash mid-write is the kind of
// thing an operator never forgives.
bool write(const QString& path, const QVector<LogEntry>& entries,
           QString* error = nullptr);

// Sort key for an ADIF band string, as an approximate frequency in MHz.
//
// Sorting band names as text is wrong in a way that looks right:
// "160m" sorts before "40m" because '1' precedes '4', and a logbook
// sorted by band then lists 10m, 160m, 17m, 20m — plausible enough at a
// glance that nobody checks it. Bands have an order, and it is the one
// the dial has.
//
// Handles metres and centimetres ("70cm"), returns 0 for anything
// unrecognised so blanks and junk sort together at one end rather than
// scattering through the list.
double bandSortKeyMHz(const QString& band);

// Comma-separated export for spreadsheets. Values are quoted and inner
// quotes doubled, per RFC 4180, because a comment containing a comma
// is entirely normal.
QString toCsv(const QVector<LogEntry>& entries);

// How far two records' timestamps may differ and still be the same
// contact. Clocks disagree, and the two ends of a QSO log it seconds
// or a minute apart; the same contact re-imported from a friend's log
// or from LoTW should not double up.
//
// Two minutes rather than ten: on a contest weekend the same station
// on the same band and mode ten minutes later is very often a genuine
// second contact, and merging those would destroy data that cannot be
// recovered.
constexpr int kDuplicateToleranceMinutes = 2;

// Are these the same contact?
//
// Callsign must match. Band and mode must match when both records say
// — an import that omits BAND should not be treated as a different
// contact for that reason alone. Times must be within tolerance, or
// both absent.
bool isSameQso(const LogEntry& a, const LogEntry& b);

struct MergeResult {
    QVector<LogEntry> merged;   // existing plus whatever was new
    int added{0};
    int skipped{0};             // already present
    // Duplicates that were not merely skipped: the incoming copy
    // carried fields the local one did not have, and those were taken.
    // Reported separately because "0 added, 340 skipped" after
    // importing a confirmation report reads like nothing happened, and
    // the operator needs to know that 340 confirmations arrived.
    int enriched{0};
};

// Fold `incoming` into `existing`. Never replaces a field the operator
// can see and edit: the local log is the one they have been
// correcting, and an import overwriting those corrections would be
// silent damage.
//
// Unmodelled fields are different, and are taken from the incoming
// record when the local one has nothing there. The operator cannot have
// corrected a field this program has never shown them, so there is no
// local edit to protect — while the thing an operator most often
// imports a duplicate FOR is exactly such a field. Downloading a LoTW
// or eQSL confirmation report and having it counted as 340 skipped
// duplicates, changing nothing, is the failure this rule exists to
// prevent.
//
// Still conservative in one respect: a value already present locally is
// left alone rather than overwritten. Adding what is missing cannot
// destroy anything; replacing what is there can.
MergeResult merge(const QVector<LogEntry>& existing,
                  const QVector<LogEntry>& incoming);

} // namespace AdifLog

} // namespace NereusSDR
