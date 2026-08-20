// SPDX-License-Identifier: GPL-2.0-or-later
//
// NereusSDR - LogEntry: one logged contact, plus ADIF serialisation.
//
// NereusSDR-original. Thetis has no logbook — it hands QSOs to external
// loggers over CAT/TCI — so there is no upstream to port. Field names
// mirror their ADIF 3 tags, because that is what every logger, QRZ,
// Club Log and LoTW all speak; ADIF is therefore also the interchange
// format for the uploaders, rather than a per-service payload.
//
// NOT called QsoRecord: core/AdifParser.h already owns that name for a
// four-field record (callsign / band / modeGroup / dxccPrefix) used to
// colour the band plan by DXCC. Same words, different job — that one
// answers "which entities have I worked on this band", this one is the
// contact itself.
//
// Copyright (C) 2026 NereusSDR contributors.
//
// Modification history (NereusSDR)
//   2026-08-07  Martin Fischer  Initial create. AI tooling: Anthropic
//                               Claude (Cowork).

#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QPair>
#include <QString>
#include <QVector>

namespace Longpath {

struct LogEntry {
    QString   call;          // CALL
    QDateTime timeOn;        // QSO_DATE + TIME_ON, always UTC
    double    freqMHz{0.0};  // FREQ
    QString   band;          // BAND  ("40m")
    QString   mode;          // MODE  ("SSB", "CW", …)
    QString   submode;       // SUBMODE ("LSB"/"USB" when MODE is SSB)
    QString   rstSent;       // RST_SENT
    QString   rstRcvd;       // RST_RCVD
    QString   gridSquare;    // GRIDSQUARE — theirs
    QString   myGridSquare;  // MY_GRIDSQUARE
    QString   name;          // NAME
    QString   qth;           // QTH
    QString   country;       // COUNTRY
    QString   comment;       // COMMENT
    double    txPowerW{0.0}; // TX_PWR

    // Has this been accepted by the QRZ logbook?
    //
    // Written as APP_NEREUS_QRZUP. An APP_ prefix is the ADIF-sanctioned
    // way to record something the standard has no field for, and other
    // programs are required to leave it alone rather than choke on it.
    //
    // It exists because "upload" without "uploaded" is not a feature:
    // without somewhere to record the answer, every restart forgets
    // which contacts got through, and the only safe thing left to do is
    // send everything again.
    bool uploadedToQrz{false};

    // ── Everything this struct does not model ────────────────────────
    //
    // Kept verbatim, in file order, and written back out unchanged.
    //
    // This is not a nicety. The fields NereusSDR models are the ones it
    // shows; the ones it does not include QSL_RCVD, QSL_SENT,
    // LOTW_QSL_RCVD, LOTW_QSL_SENT, EQSL_QSL_RCVD, DXCC, CQZ, ITUZ,
    // STATE, CNTY, IOTA, CONTEST_ID, SRX, STX, SAT_NAME, PROP_MODE and
    // every APP_ tag any other logger ever wrote. Ignoring them on
    // import was defensible; ignoring them on import and then
    // REWRITING THE FILE was data loss, because the writer replaces the
    // whole log and anything not held here ceased to exist.
    //
    // What that costs in practice: an operator imports their log from
    // Log4OM, edits one comment, and every LoTW and QSL confirmation
    // they have ever earned is gone from the file. Those cannot be
    // reconstructed from anything local — they are somebody else's
    // record of a contact having been confirmed.
    //
    // A vector of pairs rather than a map: ADIF forbids a repeated
    // field name within a record, so a map would be correct, but order
    // is free to preserve here and a file that round-trips
    // byte-for-byte is far easier to trust and to diff.
    QVector<QPair<QString, QString>> extras;

    // Derived from the two locators; not written to ADIF, which has no
    // field for either — every logger recomputes them.
    double distanceKm{0.0};
    double bearingDeg{0.0};

    bool isValid() const { return !call.trimmed().isEmpty(); }

    // One <...:len>value record terminated by <EOR>. This is both what
    // goes in the local file and what the QRZ logbook API wants as its
    // ADIF parameter, so there is exactly one place that decides how a
    // contact is spelled.
    QString toAdifRecord() const;

    // Does this struct have a member for that ADIF field?
    //
    // The one place that answers it, so the reader and the writer
    // cannot disagree. They did not disagree before only because the
    // reader dropped what it did not know and the writer never saw it;
    // now that unknown fields survive, a field modelled by one and
    // treated as an extra by the other would appear twice in the file.
    //
    // `upperName` must already be upper-cased — ADIF field names are
    // case-insensitive and are canonicalised on the way in.
    static bool modelsAdifField(const QString& upperName);
};

} // namespace Longpath

Q_DECLARE_METATYPE(Longpath::LogEntry)
