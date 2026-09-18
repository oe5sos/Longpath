#pragma once
// =================================================================
// src/core/LogbookStats.h  (Longpath)
// =================================================================
//
// Longpath-original. The numbers behind the logbook's Kennzahlen view:
// what one log adds up to, per band, mode, year and week, which
// countries come up most, and how far along the classic awards it is
// (DXCC, WAS, grid squares, continents, CQ zones) -- worked and
// confirmed, the confirmation read the way QsoConfirmation reads it
// (card, LoTW or eQSL, Y or V; R is a request, not a confirmation).
//
// Benchmark, not source: Zeus's logbook workspace shows tiles for Log /
// Bänder / Modi / Aktivität (26 Wochen) / Top-Länder / Awards
// (DXCC · WAS · Grids). Its client is proprietary and its GPL engine
// only carries the DTOs, so nothing here is ported; the choice of what
// to count follows what the tiles show and what the awards require.
//
// DXCC entities are keyed by cty.dat's primary prefix, the same key
// DxccWorkedStatus and the spot colours use. cty.dat's six non-DXCC
// entries (marked `*`: Sicily, Shetland, Bear Island, European Turkey,
// African Italy, Vienna Intl Ctr) are folded into the DXCC entity they
// count for. Without a loaded cty.dat the ADIF DXCC number is the key
// when a record carries one; otherwise the record counts for no entity.
//
// WAS is the ARRL list of 50 states; contacts with the US entities
// (K, KL, KH6) whose ADIF STATE names one of them count, DC counts as
// Maryland (ARRL rule), and Alaska / Hawaii contacts count for AK / HI
// even when STATE is blank.
//
// Pure computation over a QVector<LogEntry>; no Qt widgets, no I/O, so
// the tests can hand it a synthetic log and a fixed "now".
//
// =================================================================
// Modification history (Longpath):
//   2026-09-18 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "models/LogEntry.h"

#include <QDateTime>
#include <QString>
#include <QVector>

namespace Longpath {

class CtyDatParser;

struct LogbookStats {
    // One row of a distribution: how many contacts, how many of them
    // confirmed.
    struct Bucket {
        QString key;
        int     count{0};
        int     confirmed{0};
    };
    struct Country {
        QString prefix;       // cty.dat primary prefix ("DL"), or "#291"
        QString name;         // entity name, or the record's COUNTRY text
        QString continent;    // "EU", may be empty
        int     count{0};
        int     confirmed{0};
    };
    struct BandAward {
        QString band;
        int     worked{0};    // distinct DXCC entities on this band
        int     confirmed{0};
    };

    // ── Log ──────────────────────────────────────────────────────────
    int       total{0};
    int       uniqueCalls{0};
    int       confirmed{0};
    QDateTime first;          // UTC; invalid when the log is empty
    QDateTime last;
    int       last7Days{0};
    int       last30Days{0};
    int       last365Days{0};
    double    longestKm{0.0};
    QString   longestCall;

    // ── Distributions ────────────────────────────────────────────────
    QVector<Bucket>  byBand;       // dial order (160m first)
    QVector<Bucket>  byMode;       // most-used first
    QVector<Bucket>  byYear;       // ascending
    QVector<int>     weekly;       // kWeeks buckets, oldest first
    QVector<Country> countries;    // most-worked first, all of them

    // ── Awards ───────────────────────────────────────────────────────
    int dxccWorked{0};
    int dxccConfirmed{0};
    int dxccTotal{0};              // entities cty.dat knows (340), 0 without it
    QVector<BandAward> dxccByBand; // dial order, bands with contacts only
    int wasWorked{0};
    int wasConfirmed{0};
    int gridsWorked{0};            // distinct 4-character squares
    int gridsConfirmed{0};
    int continentsWorked{0};
    int cqZonesWorked{0};

    static constexpr int kWeeks = 26;
    static constexpr int kWasStates = 50;
    static constexpr int kContinents = 7;
    static constexpr int kCqZones = 40;

    // `cty` may be null. `nowUtc` anchors "last N days" and the weekly
    // buckets (week kWeeks-1 ends at nowUtc).
    static LogbookStats compute(const QVector<LogEntry>& entries,
                                const CtyDatParser* cty,
                                const QDateTime& nowUtc);

    // The DXCC entity key for one record: cty.dat primary prefix with the
    // non-DXCC entries folded, or "#<DXCC>" from the ADIF field, or empty.
    static QString entityKey(const LogEntry& e, const CtyDatParser* cty);

    // The WAS state for one record ("TX"), or empty when it does not count.
    static QString wasState(const LogEntry& e, const QString& entityKey);
};

} // namespace Longpath
