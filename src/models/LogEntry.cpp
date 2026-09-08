// SPDX-License-Identifier: GPL-2.0-or-later
//
// NereusSDR - LogEntry ADIF serialisation. See LogEntry.h.
//
// Copyright (C) 2026 NereusSDR contributors.
//
// Modification history (NereusSDR)
//   2026-08-07  Martin Fischer  Initial create. AI tooling: Anthropic
//                               Claude (Cowork).
//   2026-08-10  Martin Fischer  ADIF lengths in UTF-8 bytes, not
//                               QString::length() code units. The file
//                               is written UTF-8, and <NAME:length>
//                               counts octets on disk — "Jürgen" is 6
//                               chars but 7 bytes, and the char count
//                               mis-sliced every byte-counting reader,
//                               QRZ and Cloudlog included (they receive
//                               exactly this string). Pairs with
//                               AdifLog::parse() now walking bytes.
//                               AI tooling: Anthropic Claude (Cowork).

#include "LogEntry.h"

#include <QSet>

namespace Longpath {

namespace {

// ADIF field: <NAME:LENGTH>VALUE. Length counts BYTES of the value as
// it sits in the (UTF-8) file, which is why a value may legally contain
// '<' — readers walk lengths, they do not split on the bracket.
//
// QString::length() would count UTF-16 code units instead, and the two
// disagree the moment a name or comment carries an umlaut: "Jürgen" is
// 6 code units but 7 bytes, and a byte-counting reader — every other
// logger, plus QRZ and Cloudlog, which are sent exactly this string —
// then mis-slices the record from that field onwards.
void field(QString& out, const QString& name, const QString& value)
{
    const QString v = value.trimmed();
    if (v.isEmpty()) { return; }
    out += QStringLiteral("<%1:%2>%3 ").arg(name).arg(v.toUtf8().size())
                                        .arg(v);
}

// The same, but without the trim and without dropping an empty value.
// Used for fields NereusSDR does not model: those are somebody else's
// data and go back out exactly as they came in, including a legal
// zero-length one.
void rawField(QString& out, const QString& name, const QString& value)
{
    out += QStringLiteral("<%1:%2>%3 ").arg(name).arg(value.toUtf8().size())
                                        .arg(value);
}

} // namespace

bool LogEntry::modelsAdifField(const QString& upperName)
{
    static const QSet<QString> kMine = {
        QStringLiteral("CALL"),          QStringLiteral("QSO_DATE"),
        QStringLiteral("TIME_ON"),       QStringLiteral("FREQ"),
        QStringLiteral("BAND"),          QStringLiteral("MODE"),
        QStringLiteral("SUBMODE"),       QStringLiteral("RST_SENT"),
        QStringLiteral("RST_RCVD"),      QStringLiteral("GRIDSQUARE"),
        QStringLiteral("MY_GRIDSQUARE"), QStringLiteral("NAME"),
        QStringLiteral("QTH"),           QStringLiteral("COUNTRY"),
        QStringLiteral("COMMENT"),       QStringLiteral("TX_PWR"),
        QStringLiteral("APP_NEREUS_QRZUP"),
        QStringLiteral("MY_SOTA_REF"),   QStringLiteral("SOTA_REF"),
        // MY_SIG/MY_SIG_INFO/SIG/SIG_INFO are deliberately NOT here.
        // They are ADIF's generic Special Interest Activity pair, not a
        // POTA-only field, so whether a given record's SIG value is
        // "modelled" depends on the VALUE ("POTA" or not), which this
        // static, name-only check cannot express. AdifLog::parse()
        // intercepts them itself and hands anything that is not POTA
        // back to extras rather than losing it — see there.
    };
    return kMine.contains(upperName);
}

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
    field(r, QStringLiteral("MY_SOTA_REF"), mySotaRef);
    field(r, QStringLiteral("SOTA_REF"), sotaRef);
    // POTA: MY_SIG/SIG only appear when there is a park to name them
    // for — an empty MY_SIG=POTA with no MY_SIG_INFO would be a claim
    // with nothing behind it.
    if (!myPotaRef.trimmed().isEmpty()) {
        field(r, QStringLiteral("MY_SIG"), QStringLiteral("POTA"));
        field(r, QStringLiteral("MY_SIG_INFO"), myPotaRef);
    }
    if (!potaRef.trimmed().isEmpty()) {
        field(r, QStringLiteral("SIG"), QStringLiteral("POTA"));
        field(r, QStringLiteral("SIG_INFO"), potaRef);
    }
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

    // ── Everything else, back out untouched ──────────────────────────
    //
    // Last, so the fields this program understands stay together at the
    // front and the file remains readable by a human. The guard against
    // a modelled name is defensive: the reader will never put one here,
    // but if it ever did, the field would be written twice and the
    // second one would win in most readers — a corruption that no test
    // of either half on its own would catch.
    for (const auto& kv : extras) {
        if (modelsAdifField(kv.first)) { continue; }
        rawField(r, kv.first, kv.second);
    }

    r += QStringLiteral("<EOR>");
    return r;
}

} // namespace Longpath
