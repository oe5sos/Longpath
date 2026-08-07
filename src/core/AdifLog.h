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

// Parse ADIF text into records. Unknown fields are ignored rather than
// rejected: every logger writes its own extras, and a log that refuses
// to open because it met an APP_LOG4OM_ tag would be useless.
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

// Comma-separated export for spreadsheets. Values are quoted and inner
// quotes doubled, per RFC 4180, because a comment containing a comma
// is entirely normal.
QString toCsv(const QVector<LogEntry>& entries);

} // namespace AdifLog

} // namespace NereusSDR
