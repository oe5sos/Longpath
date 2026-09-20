// =================================================================
// src/core/LogbookStats.cpp  (Longpath)
// =================================================================
//
// Longpath-original; see LogbookStats.h.
// no-port-check: Longpath-original, nothing ported (the reference
// client is the benchmark for what to show; it is closed source).
//
// =================================================================
// Modification history (Longpath):
//   2026-09-18 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "core/LogbookStats.h"

#include "core/AdifLog.h"
#include "core/CtyDatParser.h"
#include "core/QsoConfirmation.h"

#include <QHash>
#include <QSet>

#include <algorithm>

namespace Longpath {

namespace {

QString extra(const LogEntry& e, const QString& upperName)
{
    for (const auto& kv : e.extras) {
        if (kv.first.compare(upperName, Qt::CaseInsensitive) == 0) {
            return kv.second.trimmed();
        }
    }
    return {};
}

// cty.dat's non-DXCC entries and the DXCC entity each counts for
// (the file marks them `*`; ARRL's DXCC list has no separate entry).
QString foldNonDxcc(const QString& primaryPrefix)
{
    if (!primaryPrefix.startsWith(QLatin1Char('*'))) { return primaryPrefix; }
    static const QHash<QString, QString> kFold = {
        {QStringLiteral("*IT9"),  QStringLiteral("I")},    // Sicily -> Italy
        {QStringLiteral("*IG9"),  QStringLiteral("I")},    // African Italy -> Italy
        {QStringLiteral("*GM/S"), QStringLiteral("GM")},   // Shetland -> Scotland
        {QStringLiteral("*JW/B"), QStringLiteral("JW")},   // Bear Island -> Svalbard
        {QStringLiteral("*TA1"),  QStringLiteral("TA")},   // European Turkey -> Turkey
        {QStringLiteral("*4U1V"), QStringLiteral("OE")},   // Vienna Intl Ctr -> Austria
    };
    const auto it = kFold.constFind(primaryPrefix.toUpper());
    return it == kFold.constEnd() ? primaryPrefix.mid(1) : it.value();
}

// ARRL Worked All States: the fifty states.
const QSet<QString>& wasStates()
{
    static const QSet<QString> kStates = {
        QStringLiteral("AL"), QStringLiteral("AK"), QStringLiteral("AZ"), QStringLiteral("AR"),
        QStringLiteral("CA"), QStringLiteral("CO"), QStringLiteral("CT"), QStringLiteral("DE"),
        QStringLiteral("FL"), QStringLiteral("GA"), QStringLiteral("HI"), QStringLiteral("ID"),
        QStringLiteral("IL"), QStringLiteral("IN"), QStringLiteral("IA"), QStringLiteral("KS"),
        QStringLiteral("KY"), QStringLiteral("LA"), QStringLiteral("ME"), QStringLiteral("MD"),
        QStringLiteral("MA"), QStringLiteral("MI"), QStringLiteral("MN"), QStringLiteral("MS"),
        QStringLiteral("MO"), QStringLiteral("MT"), QStringLiteral("NE"), QStringLiteral("NV"),
        QStringLiteral("NH"), QStringLiteral("NJ"), QStringLiteral("NM"), QStringLiteral("NY"),
        QStringLiteral("NC"), QStringLiteral("ND"), QStringLiteral("OH"), QStringLiteral("OK"),
        QStringLiteral("OR"), QStringLiteral("PA"), QStringLiteral("RI"), QStringLiteral("SC"),
        QStringLiteral("SD"), QStringLiteral("TN"), QStringLiteral("TX"), QStringLiteral("UT"),
        QStringLiteral("VT"), QStringLiteral("VA"), QStringLiteral("WA"), QStringLiteral("WV"),
        QStringLiteral("WI"), QStringLiteral("WY"),
    };
    return kStates;
}

QString normBand(const LogEntry& e)
{
    const QString b = e.band.trimmed().toLower();
    return b.isEmpty() ? QStringLiteral("?") : b;
}

QString normMode(const LogEntry& e)
{
    const QString m = e.mode.trimmed().toUpper();
    return m.isEmpty() ? QStringLiteral("?") : m;
}

} // namespace

QString LogbookStats::entityKey(const LogEntry& e, const CtyDatParser* cty)
{
    if (cty && cty->isLoaded()) {
        const QString prefix = cty->resolvePrimaryPrefix(e.call.trimmed().toUpper());
        if (!prefix.isEmpty()) { return foldNonDxcc(prefix); }
    }
    const QString dxcc = extra(e, QStringLiteral("DXCC"));
    bool ok = false;
    const int n = dxcc.toInt(&ok);
    if (ok && n > 0) { return QStringLiteral("#%1").arg(n); }
    return {};
}

QString LogbookStats::wasState(const LogEntry& e, const QString& key)
{
    // The three US entities in cty.dat: the mainland, Alaska, Hawaii. With
    // the ADIF DXCC number as key: 291, 6, 110.
    const bool mainland = key == QLatin1String("K")   || key == QLatin1String("#291");
    const bool alaska   = key == QLatin1String("KL")  || key == QLatin1String("#6");
    const bool hawaii   = key == QLatin1String("KH6") || key == QLatin1String("#110");
    if (!mainland && !alaska && !hawaii) { return {}; }

    QString state = extra(e, QStringLiteral("STATE")).toUpper();
    if (state == QLatin1String("DC")) { state = QStringLiteral("MD"); }
    if (state.isEmpty()) {
        if (alaska) { return QStringLiteral("AK"); }
        if (hawaii) { return QStringLiteral("HI"); }
        return {};
    }
    return wasStates().contains(state) ? state : QString();
}

LogbookStats LogbookStats::compute(const QVector<LogEntry>& entries,
                                   const CtyDatParser* cty,
                                   const QDateTime& nowUtc)
{
    LogbookStats s;
    const QDateTime now = nowUtc.isValid() ? nowUtc.toUTC() : QDateTime::currentDateTimeUtc();
    s.weekly = QVector<int>(kWeeks, 0);

    if (cty && cty->isLoaded()) {
        // cty.dat lists the DXCC entities plus its `*` extras.
        int stars = 0;
        // The parser has no iteration API; count the folds we know about
        // as the only non-DXCC rows (six in the current file).
        static const char* const kNonDxcc[] = {"*IT9", "*IG9", "*GM/S", "*JW/B", "*TA1", "*4U1V"};
        for (const char* p : kNonDxcc) {
            if (cty->entityByPrefix(QString::fromLatin1(p))
                || cty->entityByPrefix(QString::fromLatin1(p).toLower())) {
                ++stars;
            }
        }
        s.dxccTotal = std::max(0, cty->entityCount() - stars);
    }

    QHash<QString, Bucket> band, mode, year;
    QSet<QString> calls, gridsW, gridsC, statesW, statesC, contW, zonesW;
    QHash<QString, Country> countries;
    // entity -> confirmed?
    QHash<QString, bool> entities;
    // band -> (entity -> confirmed?)
    QHash<QString, QHash<QString, bool>> entitiesPerBand;

    for (const LogEntry& e : entries) {
        if (!e.isValid()) { continue; }
        ++s.total;
        const bool conf = QsoConfirmation::isConfirmed(e);
        if (conf) { ++s.confirmed; }

        const QString call = e.call.trimmed().toUpper();
        calls.insert(call);

        const QDateTime t = e.timeOn.toUTC();
        if (t.isValid()) {
            if (!s.first.isValid() || t < s.first) { s.first = t; }
            if (!s.last.isValid()  || t > s.last)  { s.last = t; }
            const qint64 secs = t.secsTo(now);
            if (secs >= 0) {
                const qint64 days = secs / 86400;
                if (days < 7)   { ++s.last7Days; }
                if (days < 30)  { ++s.last30Days; }
                if (days < 365) { ++s.last365Days; }
                if (days < kWeeks * 7) {
                    ++s.weekly[kWeeks - 1 - static_cast<int>(days / 7)];
                }
            }
            Bucket& y = year[t.toString(QStringLiteral("yyyy"))];
            y.key = t.toString(QStringLiteral("yyyy"));
            ++y.count;
            if (conf) { ++y.confirmed; }
        }

        const QString b = normBand(e);
        Bucket& bb = band[b];
        bb.key = b;
        ++bb.count;
        if (conf) { ++bb.confirmed; }

        const QString m = normMode(e);
        Bucket& mb = mode[m];
        mb.key = m;
        ++mb.count;
        if (conf) { ++mb.confirmed; }

        if (e.distanceKm > s.longestKm) {
            s.longestKm = e.distanceKm;
            s.longestCall = e.call.trimmed();
        }

        const QString grid = e.gridSquare.trimmed().toUpper();
        if (grid.size() >= 4) {
            gridsW.insert(grid.left(4));
            if (conf) { gridsC.insert(grid.left(4)); }
        }

        const QString key = entityKey(e, cty);
        if (!key.isEmpty()) {
            bool& ec = entities[key];
            ec = ec || conf;
            bool& ebc = entitiesPerBand[b][key];
            ebc = ebc || conf;

            Country& c = countries[key];
            if (c.prefix.isEmpty()) {
                c.prefix = key;
                if (cty && cty->isLoaded() && !key.startsWith(QLatin1Char('#'))) {
                    if (const DxccEntity* ent = cty->entityByPrefix(key)) {
                        c.name = ent->name;
                        c.continent = ent->continent;
                    }
                }
                if (c.name.isEmpty()) {
                    c.name = e.country.trimmed().isEmpty() ? key : e.country.trimmed();
                }
            }
            ++c.count;
            if (conf) { ++c.confirmed; }

            const QString st = wasState(e, key);
            if (!st.isEmpty()) {
                statesW.insert(st);
                if (conf) { statesC.insert(st); }
            }

            QString cont = extra(e, QStringLiteral("CONT")).toUpper();
            if (cont.isEmpty() && !c.continent.isEmpty()) { cont = c.continent; }
            if (!cont.isEmpty()) { contW.insert(cont); }

            QString zone = extra(e, QStringLiteral("CQZ"));
            if (zone.isEmpty() && cty && cty->isLoaded() && !key.startsWith(QLatin1Char('#'))) {
                if (const DxccEntity* ent = cty->entityByPrefix(key)) {
                    if (ent->cqZone > 0) { zone = QString::number(ent->cqZone); }
                }
            }
            bool zok = false;
            const int z = zone.toInt(&zok);
            if (zok && z >= 1 && z <= kCqZones) { zonesW.insert(QString::number(z)); }
        }
    }

    s.uniqueCalls = calls.size();
    s.gridsWorked = gridsW.size();
    s.gridsConfirmed = gridsC.size();
    s.wasWorked = statesW.size();
    s.wasConfirmed = statesC.size();
    s.continentsWorked = contW.size();
    s.cqZonesWorked = zonesW.size();

    s.dxccWorked = entities.size();
    for (auto it = entities.cbegin(); it != entities.cend(); ++it) {
        if (it.value()) { ++s.dxccConfirmed; }
    }

    auto sortedBuckets = [](const QHash<QString, Bucket>& h, auto less) {
        QVector<Bucket> v;
        v.reserve(h.size());
        for (const Bucket& b : h) { v.push_back(b); }
        std::sort(v.begin(), v.end(), less);
        return v;
    };
    s.byBand = sortedBuckets(band, [](const Bucket& a, const Bucket& b) {
        return AdifLog::bandSortKeyMHz(a.key) < AdifLog::bandSortKeyMHz(b.key);
    });
    s.byMode = sortedBuckets(mode, [](const Bucket& a, const Bucket& b) {
        return a.count != b.count ? a.count > b.count : a.key < b.key;
    });
    s.byYear = sortedBuckets(year, [](const Bucket& a, const Bucket& b) {
        return a.key < b.key;
    });

    s.countries.reserve(countries.size());
    for (const Country& c : countries) { s.countries.push_back(c); }
    std::sort(s.countries.begin(), s.countries.end(),
              [](const Country& a, const Country& b) {
        return a.count != b.count ? a.count > b.count : a.name < b.name;
    });

    for (const Bucket& bb : s.byBand) {
        const QHash<QString, bool>& perBand = entitiesPerBand[bb.key];
        BandAward a;
        a.band = bb.key;
        a.worked = perBand.size();
        for (auto it = perBand.cbegin(); it != perBand.cend(); ++it) {
            if (it.value()) { ++a.confirmed; }
        }
        s.dxccByBand.push_back(a);
    }

    return s;
}

} // namespace Longpath
