// =================================================================
// src/core/AdifLog.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — see AdifLog.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "AdifLog.h"

#include "core/Maidenhead.h"

#include <QDate>
#include <QFile>
#include <QSaveFile>
#include <QTextStream>
#include <QTimeZone>

namespace NereusSDR {
namespace AdifLog {

namespace {

QDateTime combine(const QString& date, const QString& time)
{
    if (date.size() != 8) { return {}; }
    const QDate d = QDate::fromString(date, QStringLiteral("yyyyMMdd"));
    if (!d.isValid()) { return {}; }

    // TIME_ON is hhmm or hhmmss — both are legal ADIF, and loggers
    // disagree about which they emit.
    QTime t(0, 0);
    if (time.size() >= 6) {
        t = QTime::fromString(time.left(6), QStringLiteral("hhmmss"));
    } else if (time.size() >= 4) {
        t = QTime::fromString(time.left(4), QStringLiteral("hhmm"));
    }
    if (!t.isValid()) { t = QTime(0, 0); }

    return QDateTime(d, t, QTimeZone::UTC);
}

} // namespace

QVector<LogEntry> parse(const QString& text)
{
    QVector<LogEntry> out;
    LogEntry cur;
    QString date, time;
    bool sawField = false;

    // ADIF is length-prefixed, so a value may legally contain '<'.
    // Walking lengths is the only correct way to read it; splitting on
    // brackets would corrupt any comment containing one.
    int pos = 0;
    const int n = text.size();
    while (pos < n) {
        const int lt = text.indexOf(QLatin1Char('<'), pos);
        if (lt < 0) { break; }
        const int gt = text.indexOf(QLatin1Char('>'), lt);
        if (gt < 0) { break; }

        const QString spec = text.mid(lt + 1, gt - lt - 1);
        pos = gt + 1;

        if (spec.compare(QStringLiteral("EOR"), Qt::CaseInsensitive) == 0) {
            if (cur.isValid()) {
                cur.timeOn = combine(date, time);
                if (isValidGridSquare(cur.myGridSquare)
                    && isValidGridSquare(cur.gridSquare)) {
                    cur.distanceKm =
                        calculateDistanceKm(cur.myGridSquare, cur.gridSquare);
                    cur.bearingDeg =
                        calculateBearingInDegrees(cur.myGridSquare,
                                                  cur.gridSquare);
                }
                out.append(cur);
            }
            cur = LogEntry{};
            date.clear();
            time.clear();
            sawField = false;
            continue;
        }
        if (spec.compare(QStringLiteral("EOH"), Qt::CaseInsensitive) == 0) {
            // Everything before <EOH> was header. Anything the header
            // loop accumulated is not a contact.
            cur = LogEntry{};
            date.clear();
            time.clear();
            sawField = false;
            continue;
        }

        // <NAME:LENGTH> or <NAME:LENGTH:TYPE>
        const QStringList parts = spec.split(QLatin1Char(':'));
        if (parts.size() < 2) { continue; }
        bool ok = false;
        const int len = parts.at(1).toInt(&ok);
        if (!ok || len < 0 || pos + len > n) { continue; }

        const QString value = text.mid(pos, len);
        pos += len;
        sawField = true;

        const QString key = parts.at(0).toUpper();
        if      (key == QLatin1String("CALL"))          { cur.call = value; }
        else if (key == QLatin1String("QSO_DATE"))      { date = value; }
        else if (key == QLatin1String("TIME_ON"))       { time = value; }
        else if (key == QLatin1String("BAND"))          { cur.band = value; }
        else if (key == QLatin1String("MODE"))          { cur.mode = value; }
        else if (key == QLatin1String("SUBMODE"))       { cur.submode = value; }
        else if (key == QLatin1String("RST_SENT"))      { cur.rstSent = value; }
        else if (key == QLatin1String("RST_RCVD"))      { cur.rstRcvd = value; }
        else if (key == QLatin1String("GRIDSQUARE"))    { cur.gridSquare = value; }
        else if (key == QLatin1String("MY_GRIDSQUARE")) { cur.myGridSquare = value; }
        else if (key == QLatin1String("NAME"))          { cur.name = value; }
        else if (key == QLatin1String("QTH"))           { cur.qth = value; }
        else if (key == QLatin1String("COUNTRY"))       { cur.country = value; }
        else if (key == QLatin1String("COMMENT"))       { cur.comment = value; }
        else if (key == QLatin1String("FREQ"))          { cur.freqMHz = value.toDouble(); }
        else if (key == QLatin1String("TX_PWR"))        { cur.txPowerW = value.toDouble(); }
        // Anything else is somebody's private extension. Ignored, not
        // rejected — a log that refuses to open because it met an
        // APP_LOG4OM_ tag would be useless.
    }

    // A final record with no <EOR>: some writers omit it on the last
    // one. Losing a contact to a missing terminator would be a poor
    // trade for strictness.
    if (sawField && cur.isValid()) {
        cur.timeOn = combine(date, time);
        out.append(cur);
    }
    return out;
}

QVector<LogEntry> read(const QString& path, QString* error)
{
    QFile f(path);
    if (!f.exists()) {
        // Not an error. A new installation has an empty log.
        return {};
    }
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) { *error = f.errorString(); }
        return {};
    }
    return parse(QString::fromUtf8(f.readAll()));
}

bool write(const QString& path, const QVector<LogEntry>& entries,
           QString* error)
{
    // QSaveFile writes to a temporary and renames on commit, so a crash
    // or a full disk mid-write leaves the previous log intact rather
    // than a truncated one.
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) { *error = f.errorString(); }
        return false;
    }

    QTextStream out(&f);
    out << "NereusSDR logbook\n"
        << "<ADIF_VER:5>3.1.4 <PROGRAMID:9>NereusSDR <EOH>\n";
    for (const LogEntry& e : entries) {
        out << e.toAdifRecord() << "\n";
    }
    out.flush();

    if (!f.commit()) {
        if (error) { *error = f.errorString(); }
        return false;
    }
    return true;
}

QString toCsv(const QVector<LogEntry>& entries)
{
    auto q = [](const QString& s) {
        QString v = s;
        v.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + v + QLatin1Char('"');
    };

    QString out = QStringLiteral(
        "Date,Time,Call,Band,Mode,Submode,RST Sent,RST Rcvd,Grid,My Grid,"
        "Name,QTH,Country,Distance km,Bearing,Comment\n");

    for (const LogEntry& e : entries) {
        const QDateTime u = e.timeOn.toUTC();
        out += q(u.toString(QStringLiteral("yyyy-MM-dd"))) + QLatin1Char(',')
             + q(u.toString(QStringLiteral("hh:mm")))      + QLatin1Char(',')
             + q(e.call)         + QLatin1Char(',')
             + q(e.band)         + QLatin1Char(',')
             + q(e.mode)         + QLatin1Char(',')
             + q(e.submode)      + QLatin1Char(',')
             + q(e.rstSent)      + QLatin1Char(',')
             + q(e.rstRcvd)      + QLatin1Char(',')
             + q(e.gridSquare)   + QLatin1Char(',')
             + q(e.myGridSquare) + QLatin1Char(',')
             + q(e.name)         + QLatin1Char(',')
             + q(e.qth)          + QLatin1Char(',')
             + q(e.country)      + QLatin1Char(',')
             + q(e.distanceKm > 0 ? QString::number(e.distanceKm, 'f', 0)
                                  : QString{}) + QLatin1Char(',')
             + q(e.distanceKm > 0 ? QString::number(e.bearingDeg, 'f', 0)
                                  : QString{}) + QLatin1Char(',')
             + q(e.comment) + QLatin1Char('\n');
    }
    return out;
}

} // namespace AdifLog
} // namespace NereusSDR
