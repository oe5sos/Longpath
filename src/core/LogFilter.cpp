// =================================================================
// src/core/LogFilter.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — see LogFilter.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "LogFilter.h"

#include "core/QsoConfirmation.h"

namespace Longpath {

namespace {

bool blank(const QString& s) { return s.trimmed().isEmpty(); }

bool sameIgnoringCase(const QString& a, const QString& b)
{
    return a.trimmed().compare(b.trimmed(), Qt::CaseInsensitive) == 0;
}

} // namespace

bool LogFilter::isActive() const
{
    return !blank(text) || !blank(band) || !blank(mode) || !blank(grid)
           || !blank(country) || !blank(activation) || useDates
           || unconfirmedOnly;
}

bool LogFilter::matches(const LogEntry& e) const
{
    // Cheapest and most selective first.
    if (unconfirmedOnly && QsoConfirmation::isConfirmed(e)) { return false; }

    if (!blank(band) && !sameIgnoringCase(e.band, band)) { return false; }

    if (!blank(mode)) {
        // Either field may carry it. A contact written as SSB/LSB is a
        // match for LSB and for SSB, because both are true of it.
        if (!sameIgnoringCase(e.mode, mode)
            && !sameIgnoringCase(e.submode, mode)) {
            return false;
        }
    }

    if (!blank(grid)) {
        if (!e.gridSquare.trimmed().startsWith(grid.trimmed(),
                                               Qt::CaseInsensitive)) {
            return false;
        }
    }

    if (!blank(country)) {
        if (!e.country.contains(country.trimmed(), Qt::CaseInsensitive)) {
            return false;
        }
    }

    if (!blank(activation)
        && !sameIgnoringCase(e.mySotaRef, activation)
        && !sameIgnoringCase(e.myPotaRef, activation)) {
        return false;
    }

    if (useDates) {
        const QDate d = e.timeOn.toUTC().date();
        // A contact with no date cannot be shown to fall inside the
        // range, so it does not. Including it would put contacts in a
        // range the operator can see they are not in; the count of
        // what is hidden is the caller's job to show.
        if (!d.isValid()) { return false; }
        if (from.isValid() && d < from) { return false; }
        if (to.isValid()   && d > to)   { return false; }
    }

    if (!blank(text)) {
        const QString needle = text.trimmed();
        const bool hit =
               e.call.contains(needle, Qt::CaseInsensitive)
            || e.name.contains(needle, Qt::CaseInsensitive)
            || e.qth.contains(needle, Qt::CaseInsensitive)
            || e.country.contains(needle, Qt::CaseInsensitive)
            || e.gridSquare.contains(needle, Qt::CaseInsensitive)
            || e.band.contains(needle, Qt::CaseInsensitive)
            || e.mode.contains(needle, Qt::CaseInsensitive)
            || e.submode.contains(needle, Qt::CaseInsensitive)
            || e.comment.contains(needle, Qt::CaseInsensitive);
        if (!hit) { return false; }
    }

    return true;
}

} // namespace Longpath
