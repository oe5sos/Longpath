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
#include <QString>

namespace NereusSDR {

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
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::LogEntry)
