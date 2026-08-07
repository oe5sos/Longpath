// SPDX-License-Identifier: GPL-2.0-or-later
//
// NereusSDR - LogEntry ADIF serialisation. See LogEntry.h.
//
// Copyright (C) 2026 NereusSDR contributors.
//
// Modification history (NereusSDR)
//   2026-08-07  Martin Fischer  Initial create. AI tooling: Anthropic
//                               Claude (Cowork).

#include "LogEntry.h"

namespace NereusSDR {

namespace {

// ADIF field: <NAME:LENGTH>VALUE. Length counts characters, which is
// why a value may legally contain '<' — readers walk lengths, they do
// not split on the bracket.
void field(QString& out, const QString& name, const QString& value)
{
    const QString v = value.trimmed();
    if (v.isEmpty()) { return; }
    out += QStringLiteral("<%1:%2>%3 ").arg(name).arg(v.length()).arg(v);
}

} // namespace

QString LogEntry::toAdifRecord() const
{
    // ADIF timestamps are UTC by definition. toUTC() rather than
    // assuming the caller already converted — a local-time record is
    // silently wrong by hours and only shows up when someone tries to
    // match the QSO at the other end.
    const QDateTime utc = timeOn.toUTC();

    QString r;
    field(r, QStringLiteral("CALL"), call);
    if (utc.isValid()) {
        field(r, QStringLiteral("QSO_DATE"),
              utc.toString(QStringLiteral("yyyyMMdd")));
        field(r, QStringLiteral("TIME_ON"),
              utc.toString(QStringLiteral("hhmmss")));
    }
    if (freqMHz > 0.0) {
        field(r, QStringLiteral("FREQ"), QString::number(freqMHz, 'f', 6));
    }
    field(r, QStringLiteral("BAND"), band);
    field(r, QStringLiteral("MODE"), mode);
    field(r, QStringLiteral("SUBMODE"), submode);
    field(r, QStringLiteral("RST_SENT"), rstSent);
    field(r, QStringLiteral("RST_RCVD"), rstRcvd);
    field(r, QStringLiteral("GRIDSQUARE"), gridSquare);
    field(r, QStringLiteral("MY_GRIDSQUARE"), myGridSquare);
    field(r, QStringLiteral("NAME"), name);
    field(r, QStringLiteral("QTH"), qth);
    field(r, QStringLiteral("COUNTRY"), country);
    field(r, QStringLiteral("COMMENT"), comment);
    if (uploadedToQrz) {
        // APP_ is the ADIF-sanctioned prefix for a field the standard
        // does not define. Other programs must pass it through
        // untouched, so a log that goes out to Log4OM and comes back
        // still knows what has been uploaded.
        field(r, QStringLiteral("APP_NEREUS_QRZUP"), QStringLiteral("Y"));
    }
    if (txPowerW > 0.0) {
        field(r, QStringLiteral("TX_PWR"), QString::number(txPowerW, 'f', 0));
    }
    r += QStringLiteral("<EOR>");
    return r;
}

} // namespace NereusSDR
