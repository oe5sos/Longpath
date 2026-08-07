#pragma once

// =================================================================
// src/core/LogFilter.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Which contacts the logbook is currently showing.
//
// A plain struct with one predicate, kept out of the window so the
// rules can be tested one at a time. Filtering is the part of a logbook
// an operator trusts without checking — "no results" is read as "I
// never worked that", not as "the filter is wrong" — so the rules have
// to be right rather than merely plausible.
//
// Every field is optional and they combine with AND. An empty filter
// matches everything, which is what an operator who has cleared the
// boxes expects to see.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "models/LogEntry.h"

#include <QDate>
#include <QString>

namespace NereusSDR {

struct LogFilter {
    // Anything: callsign, name, QTH, country, grid, band, mode,
    // comment. The box for when you do not want to decide which field
    // the thing you remember lives in.
    QString text;

    // Exact, case-insensitive. Empty means any.
    QString band;

    // Matches MODE or SUBMODE, so asking for LSB finds contacts logged
    // as SSB with LSB as the submode — which is how this program writes
    // them, because ADIF has no LSB mode.
    QString mode;

    // Prefix, case-insensitive: "JN" finds every JN square, "JN67" the
    // ones in that square. Anyone typing two characters of a locator
    // means the field, not a station whose grid happens to start that
    // way — and there is nothing else a two-character locator could be.
    QString grid;

    // Substring, case-insensitive. Country names arrive spelled
    // differently from different loggers, so "united" has to find both
    // "United States" and "United Kingdom" rather than nothing.
    QString country;

    // Off by default. A date range that defaults to something would
    // hide contacts the moment the window opened, and an operator who
    // sees an empty log concludes the log is empty.
    bool  useDates{false};
    QDate from;
    QDate to;

    bool isActive() const;
    bool matches(const LogEntry& e) const;
};

} // namespace NereusSDR
