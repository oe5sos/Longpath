// =================================================================
// tests/tst_logbook_stats.cpp  (Longpath)
// =================================================================
//
// LogbookStats on a synthetic log against a seven-entity cty.dat:
//
//   * totals, unique calls, first/last, last-N-days, weekly buckets
//   * band order is dial order, modes by use, years ascending
//   * countries from cty.dat with the Sicily fold into Italy
//   * DXCC worked vs. confirmed (card Y, LoTW V count; R does not),
//     per band
//   * WAS from STATE on US entities, DC -> MD, Alaska/Hawaii implicit
//   * grids, continents, CQ zones (ADIF field first, cty.dat default)
//   * without cty.dat the ADIF DXCC number keys the entity
//
// Longpath-original test. no-port-check: Longpath-original code under test.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-18 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QTemporaryFile>

#include "core/CtyDatParser.h"
#include "core/LogbookStats.h"
#include "models/LogEntry.h"

using namespace Longpath;

namespace {

const char* kCty =
    "Austria:                  15:  28:  EU:   47.33:   -13.33:    -1.0:  OE:\n"
    "    OE;\n"
    "Italy:                    15:  28:  EU:   42.82:   -12.58:    -1.0:  I:\n"
    "    I,IZ;\n"
    "Sicily:                   15:  28:  EU:   37.50:   -14.00:    -1.0:  *IT9:\n"
    "    IT9;\n"
    "United States:             5:  08:  NA:   37.53:    91.67:     5.0:  K:\n"
    "    K,W,N,AA;\n"
    "Alaska:                    1:  01:  NA:   61.40:   148.87:     8.0:  KL:\n"
    "    KL,AL7;\n"
    "Japan:                    25:  45:  AS:   36.40:  -138.38:    -9.0:  JA:\n"
    "    JA,JH;\n"
    "Germany:                  14:  28:  EU:   51.00:   -10.00:    -1.0:  DL:\n"
    "    DL,DJ;\n";

QString writeCty()
{
    auto* f = new QTemporaryFile;
    f->setAutoRemove(false);
    f->open();
    f->write(kCty);
    f->close();
    return f->fileName();
}

const QDateTime kNow = QDateTime(QDate(2026, 9, 18), QTime(12, 0), Qt::UTC);

LogEntry qso(const QString& call, int daysAgo, const QString& band,
             const QString& mode, const QString& grid = {},
             std::initializer_list<QPair<QString, QString>> extras = {})
{
    LogEntry e;
    e.call = call;
    e.timeOn = kNow.addDays(-daysAgo);
    e.band = band;
    e.mode = mode;
    e.gridSquare = grid;
    for (const auto& kv : extras) { e.extras.push_back(kv); }
    return e;
}

QPair<QString, QString> f(const char* k, const char* v)
{
    return {QString::fromLatin1(k), QString::fromLatin1(v)};
}

} // namespace

class TestLogbookStats : public QObject {
    Q_OBJECT

private:
    CtyDatParser m_cty;

    QVector<LogEntry> sampleLog() const
    {
        QVector<LogEntry> log;
        log << qso("OE5XYZ", 1,   "40m", "SSB", "JN68", {f("QSL_RCVD", "Y")});
        log << qso("OE5XYZ", 2,   "20m", "CW",  "JN68");                       // same call again
        log << qso("IT9ABC", 3,   "20m", "SSB", "JM77", {f("LOTW_QSL_RCVD", "V")}); // Sicily -> Italy
        log << qso("IZ1DEF", 40,  "20m", "FT8", "JN45", {f("QSL_RCVD", "R")}); // requested, not confirmed
        log << qso("W1AW",   10,  "20m", "CW",  "FN31", {f("STATE", "CT"), f("LOTW_QSL_RCVD", "Y")});
        log << qso("K3LR",   200, "15m", "SSB", "EN91", {f("STATE", "PA")});
        log << qso("W3DC",   400, "20m", "SSB", "FM18", {f("STATE", "DC")}); // counts as MD
        log << qso("KL7AA",  5,   "20m", "CW",  "BP51");                       // Alaska, no STATE
        log << qso("JA1ABC", 6,   "15m", "CW",  "PM95", {f("CQZ", "25")});
        log << qso("DL1ABC", 7,   "40m", "SSB", "JO62", {f("CONT", "EU")});
        log << qso("DL1ABC", 8,   "40m", "SSB", "JO62");
        log << qso("",       9,   "40m", "SSB");                                // invalid, ignored
        return log;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_cty.loadFromFile(writeCty()));
        QCOMPARE(m_cty.entityCount(), 7);
    }

    void totalsAndDates()
    {
        const LogbookStats s = LogbookStats::compute(sampleLog(), &m_cty, kNow);
        QCOMPARE(s.total, 11);
        QCOMPARE(s.uniqueCalls, 9);
        QCOMPARE(s.confirmed, 3);          // Y, V, Y -- not R
        QCOMPARE(s.first, kNow.addDays(-400));
        QCOMPARE(s.last, kNow.addDays(-1));
        QCOMPARE(s.last7Days, 5);          // days 1, 2, 3, 5, 6 (day 7 is not < 7)
        QCOMPARE(s.last30Days, 8);
        QCOMPARE(s.last365Days, 10);
        // Weekly: the newest bucket holds days 0..6, the one before days 7..13.
        QCOMPARE(s.weekly.size(), LogbookStats::kWeeks);
        QCOMPARE(s.weekly.last(), 5);
        QCOMPARE(s.weekly[LogbookStats::kWeeks - 2], 3);   // days 7, 8, 10
        int sum = 0;
        for (int n : s.weekly) { sum += n; }
        QCOMPARE(sum, 9);                  // everything younger than 26 weeks (day 200/400 are not)
    }

    void distributionsAreOrdered()
    {
        const LogbookStats s = LogbookStats::compute(sampleLog(), &m_cty, kNow);
        QCOMPARE(s.byBand.size(), 3);
        QCOMPARE(s.byBand[0].key, QStringLiteral("40m"));
        QCOMPARE(s.byBand[1].key, QStringLiteral("20m"));
        QCOMPARE(s.byBand[2].key, QStringLiteral("15m"));
        QCOMPARE(s.byBand[1].count, 6);
        QCOMPARE(s.byBand[1].confirmed, 2);
        QCOMPARE(s.byMode[0].key, QStringLiteral("SSB"));
        QCOMPARE(s.byMode[0].count, 6);
        QCOMPARE(s.byMode[1].key, QStringLiteral("CW"));
        QCOMPARE(s.byMode[2].key, QStringLiteral("FT8"));
        QCOMPARE(s.byYear.size(), 2);
        QCOMPARE(s.byYear[0].key, QStringLiteral("2025"));
        QCOMPARE(s.byYear[1].key, QStringLiteral("2026"));
    }

    void countriesComeFromCtyDatWithTheSicilyFold()
    {
        const LogbookStats s = LogbookStats::compute(sampleLog(), &m_cty, kNow);
        // OE 2, I 2 (IT9 + IZ), K 3, KL 1, JA 1, DL 2 -> six entities
        QCOMPARE(s.countries.size(), 6);
        QCOMPARE(s.countries[0].name, QStringLiteral("United States"));
        QCOMPARE(s.countries[0].count, 3);
        QCOMPARE(s.countries[0].continent, QStringLiteral("NA"));
        bool italy = false;
        for (const auto& c : s.countries) {
            if (c.prefix == QLatin1String("I")) {
                italy = true;
                QCOMPARE(c.name, QStringLiteral("Italy"));
                QCOMPARE(c.count, 2);
                QCOMPARE(c.confirmed, 1);
            }
            QVERIFY2(!c.prefix.startsWith(QLatin1Char('*')), "Sicily must fold into Italy");
        }
        QVERIFY(italy);
    }

    void dxccWorkedAndConfirmedOverallAndPerBand()
    {
        const LogbookStats s = LogbookStats::compute(sampleLog(), &m_cty, kNow);
        QCOMPARE(s.dxccTotal, 6);          // seven rows minus the `*` one
        QCOMPARE(s.dxccWorked, 6);
        QCOMPARE(s.dxccConfirmed, 3);      // OE (card), I (LoTW V), K (LoTW Y)
        QCOMPARE(s.dxccByBand.size(), 3);
        QCOMPARE(s.dxccByBand[0].band, QStringLiteral("40m"));
        QCOMPARE(s.dxccByBand[0].worked, 2);     // OE, DL
        QCOMPARE(s.dxccByBand[0].confirmed, 1);  // OE
        QCOMPARE(s.dxccByBand[1].band, QStringLiteral("20m"));
        QCOMPARE(s.dxccByBand[1].worked, 4);     // OE, I, K, KL
        QCOMPARE(s.dxccByBand[1].confirmed, 2);  // I, K
    }

    void wasCountsStatesOfTheUsEntities()
    {
        const LogbookStats s = LogbookStats::compute(sampleLog(), &m_cty, kNow);
        // CT, PA, MD (from DC), AK (implicit) -> 4 worked; CT confirmed
        QCOMPARE(s.wasWorked, 4);
        QCOMPARE(s.wasConfirmed, 1);
        QCOMPARE(LogbookStats::wasState(qso("W1AW", 0, "20m", "CW", {}, {f("STATE", "XX")}), QStringLiteral("K")), QString());
        QCOMPARE(LogbookStats::wasState(qso("DL1ABC", 0, "20m", "CW", {}, {f("STATE", "TX")}), QStringLiteral("DL")), QString());
    }

    void gridsContinentsAndZones()
    {
        const LogbookStats s = LogbookStats::compute(sampleLog(), &m_cty, kNow);
        // JN68, JM77, JN45, FN31, EN91, FM18, BP51, PM95, JO62 -> 9 squares
        QCOMPARE(s.gridsWorked, 9);
        QCOMPARE(s.gridsConfirmed, 3);     // JN68, JM77, FN31
        QCOMPARE(s.continentsWorked, 3);   // EU, NA, AS
        // Zones: OE 15, I 15, K 5, KL 1, JA 25 (ADIF), DL 14 -> {15, 5, 1, 25, 14}
        QCOMPARE(s.cqZonesWorked, 5);
    }

    void withoutCtyDatTheAdifDxccNumberKeysTheEntity()
    {
        QVector<LogEntry> log;
        log << qso("OE5XYZ", 1, "40m", "SSB", {}, {f("DXCC", "206")});
        log << qso("OE1ABC", 2, "40m", "SSB", {}, {f("DXCC", "206"), f("QSL_RCVD", "Y")});
        log << qso("W1AW",   3, "20m", "CW",  {}, {f("DXCC", "291"), f("STATE", "CT")});
        log << qso("XX9XX",  4, "20m", "CW");                          // no DXCC field: no entity
        const LogbookStats s = LogbookStats::compute(log, nullptr, kNow);
        QCOMPARE(s.dxccTotal, 0);
        QCOMPARE(s.dxccWorked, 2);
        QCOMPARE(s.dxccConfirmed, 1);
        QCOMPARE(s.wasWorked, 1);
        QCOMPARE(s.countries.size(), 2);
        QCOMPARE(s.countries[0].prefix, QStringLiteral("#206"));
        QCOMPARE(s.countries[0].name, QStringLiteral("#206"));
    }

    void emptyLogIsAllZeros()
    {
        const LogbookStats s = LogbookStats::compute({}, &m_cty, kNow);
        QCOMPARE(s.total, 0);
        QVERIFY(!s.first.isValid());
        QVERIFY(s.byBand.isEmpty());
        QCOMPARE(s.weekly.size(), LogbookStats::kWeeks);
        QCOMPARE(s.dxccWorked, 0);
        QCOMPARE(s.dxccTotal, 6);
    }
};

QTEST_APPLESS_MAIN(TestLogbookStats)
#include "tst_logbook_stats.moc"
