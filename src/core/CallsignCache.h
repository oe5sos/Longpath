#pragma once

// =================================================================
// src/core/CallsignCache.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// What QRZ said about a callsign, kept between runs.
//
// ── Why this exists ──────────────────────────────────────────────────
//
// The lookup cache in RotorLogbookPanel is a QHash in the panel. It
// works, and it dies with the process. Open the program tomorrow, click
// the same contact, and it is a request to QRZ again — for an answer
// that has not changed since 1987 in the case of a name, and will not
// change at all in the case of somebody who is now a silent key.
//
// That was tolerable while lookups happened one at a time, at the
// operator's request, next to a callsign they were typing. The logbook
// detail pane changes the arithmetic: a log with two thousand contacts
// is two thousand potential lookups, and a rate-limited API accessed
// that way stops answering.
//
// AetherSDR had a JSON round-trip here and we deliberately left it out
// of the CallsignInfo port (see the note in CallsignInfo.h). This is
// that gap, closed — written independently rather than ported, because
// the shape of the file is ours and the expiry rule below is not
// upstream's.
//
// ── Expiry, and why a stale entry is still returned ──────────────────
//
// People move house, upgrade licence class, and change locator. A cache
// with no expiry makes the first answer the only answer, forever.
//
// But an entry past its date is not worthless — it is what QRZ said
// last time, and last time is a great deal better than nothing when the
// network is down or the subscription has lapsed. So `get()` hands the
// entry over regardless and `isStale()` reports separately. The caller
// decides whether to show it while a fresh lookup runs, which is what
// the detail pane does: the old name appears immediately and is
// replaced when the answer arrives.
//
// Deleting stale entries on read would be the tidier-looking choice and
// the wrong one: it throws away the only copy of something at exactly
// the moment the network cannot replace it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/CallsignInfo.h"

#include <QHash>
#include <QJsonObject>
#include <QString>

namespace Longpath {

class CallsignCache {
public:
    // An entry older than this is stale — still returned, but worth
    // refreshing when there is a network and a subscription. Ninety
    // days is roughly how often a locator changes for somebody who
    // moves, and far more often than most entries change at all.
    static constexpr qint64 kDefaultMaxAgeSeconds = 90LL * 24 * 60 * 60;

    // `path` is the JSON file. Empty means the default beside the
    // portrait cache; tests pass their own.
    explicit CallsignCache(const QString& path = QString{});

    // Reads the file. Safe to call on a file that does not exist, is
    // empty, or is not JSON — a corrupt cache is an empty cache, never
    // a refusal to start.
    void load();

    // Writes it. Atomic: to a temporary beside the target, then rename.
    // A half-written cache killed by a crash would be a corrupt file on
    // the next run, and the whole point of the thing is that it is not
    // worth losing sleep over.
    bool save() const;

    // True when `call` is present at all, stale or not.
    bool contains(const QString& call) const;

    // The entry, or an invalid CallsignInfo. `call` is normalised here,
    // so callers need not remember to.
    CallsignInfo get(const QString& call) const;

    // Whether `info` is old enough to be worth refreshing. An entry
    // with no timestamp counts as stale: it came from somewhere that
    // did not record when, so nothing can be claimed about its age.
    bool isStale(const CallsignInfo& info) const;
    bool isStale(const QString& call) const { return isStale(get(call)); }

    // Store. Stamps `fetchedUtc` when the caller left it at zero, since
    // an entry whose age is unknown can never go stale and would sit
    // there being wrong.
    //
    // Keyed on the QUERIED call, which is not always info.call: QRZ
    // answers a query for "VP2E/K1ABC" with a record whose call is
    // "K1ABC" (upstream #3990). Keying on the answer would mean the
    // portable query is never a hit and always costs a request.
    void put(const QString& call, const CallsignInfo& info);

    int  count() const { return int(m_entries.size()); }
    void clear();

    // Round-trip helpers. Public so a test can pin the shape of the
    // file rather than only the behaviour of the class.
    static QJsonObject   toJson(const CallsignInfo& info);
    static CallsignInfo  fromJson(const QJsonObject& o);

    static QString defaultPath();

    void setMaxAgeSeconds(qint64 s) { m_maxAge = s; }

private:
    QString                      m_path;
    QHash<QString, CallsignInfo> m_entries;
    qint64                       m_maxAge{kDefaultMaxAgeSeconds};
};

} // namespace Longpath
