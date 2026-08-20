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
//   2026-08-10 — Two fixes, AI-assisted via Anthropic Claude (Cowork),
//                 operator Martin Fischer:
//                 (1) parse() walks BYTES of the UTF-8 input, not
//                     QChars. ADIF lengths count what is on disk, and a
//                     char-counted walk mis-sliced every record whose
//                     NAME/QTH/COMMENT carried an umlaut. Pairs with
//                     LogEntry::field() now writing byte counts.
//                 (2) The final record of a file without a trailing
//                     <EOR> now gets distance and bearing computed like
//                     every other record, instead of silently lacking
//                     them.
//                 (3) write() keeps three rotating backups of the file
//                     it is about to replace, and value decoding falls
//                     back to Latin-1 when the bytes are not valid
//                     UTF-8 — old Windows loggers wrote Latin-1, and
//                     their umlauts arrived as replacement characters.
// =================================================================

#include "AdifLog.h"

#include "core/Maidenhead.h"

#include <QDate>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringConverter>
#include <QTextStream>
#include <QTimeZone>

#include <cmath>

namespace Longpath {
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

// A field value, as text. Strict UTF-8 first; when the bytes are not
// valid UTF-8 the file is almost certainly Latin-1 — the encoding old
// Windows loggers wrote — and decoding it as UTF-8 anyway would turn
// every umlaut into a replacement character. Latin-1 maps every byte,
// so the fallback cannot fail, only be wrong for encodings nobody's
// logger has used.
QString decodeValue(const QByteArray& raw)
{
    QStringDecoder utf8(QStringConverter::Utf8,
                        QStringConverter::Flag::Stateless);
    const QString s = utf8.decode(raw);
    if (!utf8.hasError()) { return s; }
    return QString::fromLatin1(raw);
}

// Keep the last three generations of the file about to be replaced.
// write() rewrites the whole log, so one bad merge or import replaces
// everything at once — atomicity protects against a crash mid-write,
// but not against successfully writing the wrong thing. Three
// generations rather than one: the mistake is often noticed after the
// next save has already happened.
void rotateBackups(const QString& path)
{
    if (!QFile::exists(path)) { return; }
    const QString b1 = path + QStringLiteral(".bak1");
    const QString b2 = path + QStringLiteral(".bak2");
    const QString b3 = path + QStringLiteral(".bak3");
    QFile::remove(b3);
    QFile::rename(b2, b3);
    QFile::rename(b1, b2);
    QFile::copy(path, b1);
}

} // namespace

QVector<LogEntry> parse(const QByteArray& bytes)
{
    QVector<LogEntry> out;
    LogEntry cur;
    QString date, time;
    bool sawField = false;

    // Timestamp, distance and bearing — the same completion whether the
    // record ended in <EOR> or the file just stopped.
    auto finalize = [&date, &time](LogEntry& e) {
        e.timeOn = combine(date, time);
        if (isValidGridSquare(e.myGridSquare)
            && isValidGridSquare(e.gridSquare)) {
            e.distanceKm =
                calculateDistanceKm(e.myGridSquare, e.gridSquare);
            e.bearingDeg =
                calculateBearingInDegrees(e.myGridSquare, e.gridSquare);
        }
    };

    // ADIF is length-prefixed, so a value may legally contain '<'.
    // Walking lengths is the only correct way to read it; splitting on
    // brackets would corrupt any comment containing one.
    //
    // The walk is over BYTES: <NAME:length> counts octets on disk, and
    // that is also what every other logger writes. Values become
    // QStrings only after their byte extent is known.
    int pos = 0;
    const int n = bytes.size();
    while (pos < n) {
        const int lt = bytes.indexOf('<', pos);
        if (lt < 0) { break; }
        const int gt = bytes.indexOf('>', lt);
        if (gt < 0) { break; }

        // Field names are ASCII, so bytes-as-Latin1 is exact here.
        const QString spec =
            QString::fromLatin1(bytes.mid(lt + 1, gt - lt - 1));
        pos = gt + 1;

        if (spec.compare(QStringLiteral("EOR"), Qt::CaseInsensitive) == 0) {
            if (cur.isValid()) {
                finalize(cur);
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

        const QString value = decodeValue(bytes.mid(pos, len));
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
        else if (key == QLatin1String("APP_NEREUS_QRZUP")) {
            cur.uploadedToQrz = value.trimmed().compare(QLatin1String("Y"),
                                    Qt::CaseInsensitive) == 0;
        }
        // ── Everything else is kept, not dropped ─────────────────
        //
        // It used to be ignored, and that was data loss the moment the
        // file was rewritten: the writer replaces the whole log, so a
        // QSL_RCVD or LOTW_QSL_RCVD that nothing was holding simply
        // ceased to exist. Confirmations are the one thing in a logbook
        // that cannot be reconstructed locally — they are somebody
        // else's record that a contact happened.
        //
        // So they are carried, in file order, and written back
        // untouched. See LogEntry::extras.
        else if (!LogEntry::modelsAdifField(key)) {
            cur.extras.append(qMakePair(key, value));
        }
    }

    // A final record with no <EOR>: some writers omit it on the last
    // one. Losing a contact to a missing terminator would be a poor
    // trade for strictness.
    if (sawField && cur.isValid()) {
        finalize(cur);
        out.append(cur);
    }
    return out;
}

QVector<LogEntry> parse(const QString& text)
{
    return parse(text.toUtf8());
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
    // Bytes straight into the byte-walking parser — converting to
    // QString first would reintroduce the char/byte length mismatch.
    return parse(f.readAll());
}

bool write(const QString& path, const QVector<LogEntry>& entries,
           QString* error)
{
    // Backups first: QSaveFile protects against a crash mid-write, but
    // not against successfully writing a log that a bad merge or import
    // just gutted. Three generations sit beside the file as .bak1-3.
    rotateBackups(path);

    // QSaveFile writes to a temporary and renames on commit, so a crash
    // or a full disk mid-write leaves the previous log intact rather
    // than a truncated one.
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) { *error = f.errorString(); }
        return false;
    }

    QTextStream out(&f);
    out << "Longpath logbook\n"
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

bool isSameQso(const LogEntry& a, const LogEntry& b)
{
    const QString ca = a.call.trimmed().toUpper();
    const QString cb = b.call.trimmed().toUpper();
    if (ca.isEmpty() || ca != cb) { return false; }

    // Band and mode only distinguish when both records state them. An
    // export that omitted BAND is not thereby a different contact, and
    // treating it as one would import the whole file twice.
    auto agrees = [](const QString& x, const QString& y) {
        const QString a1 = x.trimmed();
        const QString b1 = y.trimmed();
        if (a1.isEmpty() || b1.isEmpty()) { return true; }
        return a1.compare(b1, Qt::CaseInsensitive) == 0;
    };
    if (!agrees(a.band, b.band)) { return false; }
    // MODE only, not SUBMODE: one side writing SSB and the other
    // SSB/LSB is the same contact spelled with more detail.
    if (!agrees(a.mode, b.mode)) { return false; }

    const bool va = a.timeOn.isValid();
    const bool vb = b.timeOn.isValid();
    if (va != vb) { return false; }
    if (!va) {
        // Neither has a time. Call, band and mode already matched, and
        // there is nothing further to go on.
        return true;
    }
    const qint64 secs = std::abs(a.timeOn.secsTo(b.timeOn));
    return secs <= kDuplicateToleranceMinutes * 60;
}

MergeResult merge(const QVector<LogEntry>& existing,
                  const QVector<LogEntry>& incoming)
{
    MergeResult r;
    r.merged = existing;

    // Bucket by callsign so a large import does not compare every
    // incoming record against every existing one. A 20,000-contact log
    // imported into another would otherwise be 400 million comparisons.
    QHash<QString, QVector<int>> byCall;
    for (int i = 0; i < r.merged.size(); ++i) {
        byCall[r.merged.at(i).call.trimmed().toUpper()].append(i);
    }

    for (const LogEntry& in : incoming) {
        if (!in.isValid()) { continue; }
        const QString key = in.call.trimmed().toUpper();

        int dupAt = -1;
        for (int idx : byCall.value(key)) {
            if (isSameQso(r.merged.at(idx), in)) { dupAt = idx; break; }
        }
        if (dupAt >= 0) {
            ++r.skipped;
            // Take what the local copy does not have. See AdifLog.h:
            // the operator cannot have corrected a field this program
            // never showed them, and a confirmation report is nothing
            // but such fields.
            LogEntry& have = r.merged[dupAt];
            bool grew = false;
            for (const auto& kv : in.extras) {
                bool present = false;
                for (const auto& mine : have.extras) {
                    if (mine.first == kv.first) { present = true; break; }
                }
                if (present) { continue; }
                have.extras.append(kv);
                grew = true;
            }
            if (grew) { ++r.enriched; }
            continue;
        }

        byCall[key].append(r.merged.size());
        r.merged.append(in);
        ++r.added;
    }
    return r;
}

double bandSortKeyMHz(const QString& band)
{
    const QString b = band.trimmed().toLower();
    if (b.isEmpty()) { return 0.0; }

    // Leading number, then a unit. "40m", "70cm", "2190m", "1.25m".
    static const QRegularExpression re(
        QStringLiteral("^([0-9]+(?:\\.[0-9]+)?)\\s*(mm|cm|m)$"));
    const QRegularExpressionMatch m = re.match(b);
    if (!m.hasMatch()) { return 0.0; }

    bool ok = false;
    const double value = m.captured(1).toDouble(&ok);
    if (!ok || value <= 0.0) { return 0.0; }

    const QString unit = m.captured(2);
    double metres = value;
    if (unit == QLatin1String("cm")) { metres = value / 100.0; }
    else if (unit == QLatin1String("mm")) { metres = value / 1000.0; }

    // c / lambda, near enough. The exact figure does not matter; the
    // ordering does, and this is the order the bands sit in on the dial.
    return 299.792458 / metres;
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
} // namespace Longpath
