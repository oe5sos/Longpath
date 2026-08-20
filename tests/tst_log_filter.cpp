// Logbook filtering.
//
// This is the part of a logbook an operator trusts without checking:
// "no results" reads as "I never worked that", not as "the filter is
// wrong". So each rule gets pinned separately, including the ones that
// look too obvious to test — those are the ones a later tidy-up breaks.
// no-port-check: NereusSDR-original.

#include <QtTest/QtTest>
#include <QTimeZone>

#include "core/LogFilter.h"

using namespace Longpath;

namespace {

LogEntry entry()
{
    LogEntry e;
    e.call        = QStringLiteral("OE1WYC");
    e.band        = QStringLiteral("40m");
    e.mode        = QStringLiteral("SSB");
    e.submode     = QStringLiteral("LSB");
    e.gridSquare  = QStringLiteral("JN67VV");
    e.country     = QStringLiteral("Austria");
    e.name        = QStringLiteral("Hans");
    e.qth         = QStringLiteral("Linz");
    e.comment     = QStringLiteral("first contact on the new antenna");
    e.timeOn = QDateTime(QDate(2026, 5, 20), QTime(14, 0), QTimeZone::UTC);
    return e;
}

} // namespace

class TstLogFilter : public QObject {
    Q_OBJECT
private slots:
    void an_empty_filter_matches_everything();
    void band_is_exact_and_case_insensitive();
    void mode_matches_the_submode_too();
    void grid_matches_from_the_start();
    void grid_does_not_match_in_the_middle();
    void country_matches_part_of_the_name();
    void dates_are_inclusive_at_both_ends();
    void dates_are_ignored_until_switched_on();
    void a_contact_with_no_date_is_excluded_by_a_date_range();
    void free_text_reaches_every_field();
    void fields_combine_with_and();
    void whitespace_only_is_not_a_filter();
};

void TstLogFilter::an_empty_filter_matches_everything()
{
    // Clearing the boxes must show the whole log, not nothing.
    LogFilter f;
    QVERIFY(!f.isActive());
    QVERIFY(f.matches(entry()));
    QVERIFY(f.matches(LogEntry{}));    // even a blank record
}

void TstLogFilter::band_is_exact_and_case_insensitive()
{
    LogFilter f;
    f.band = QStringLiteral("40M");
    QVERIFY(f.matches(entry()));
    f.band = QStringLiteral("  40m ");
    QVERIFY(f.matches(entry()));

    // Exact, not prefix: asking for 40m must not return 40mm or 4m,
    // and asking for 4 must not return everything on 40.
    f.band = QStringLiteral("4");
    QVERIFY(!f.matches(entry()));
    f.band = QStringLiteral("20m");
    QVERIFY(!f.matches(entry()));
}

void TstLogFilter::mode_matches_the_submode_too()
{
    // This program writes phone contacts as MODE=SSB with SUBMODE=LSB,
    // because ADIF has no LSB mode. An operator filtering for LSB means
    // this contact, and finding nothing would look like a missing log.
    LogFilter f;
    f.mode = QStringLiteral("LSB");
    QVERIFY(f.matches(entry()));
    f.mode = QStringLiteral("SSB");
    QVERIFY(f.matches(entry()));
    f.mode = QStringLiteral("CW");
    QVERIFY(!f.matches(entry()));
}

void TstLogFilter::grid_matches_from_the_start()
{
    LogFilter f;
    for (const QString& g : {QStringLiteral("J"), QStringLiteral("JN"),
                             QStringLiteral("jn67"),
                             QStringLiteral("JN67VV")}) {
        f.grid = g;
        QVERIFY2(f.matches(entry()), qPrintable(g));
    }
    f.grid = QStringLiteral("IO");
    QVERIFY(!f.matches(entry()));
}

void TstLogFilter::grid_does_not_match_in_the_middle()
{
    // A locator is hierarchical: the first characters are the field,
    // then the square. "67" is not a place, and matching it anywhere
    // would return an arbitrary scatter across the world.
    LogFilter f;
    f.grid = QStringLiteral("67");
    QVERIFY(!f.matches(entry()));
    f.grid = QStringLiteral("VV");
    QVERIFY(!f.matches(entry()));
}

void TstLogFilter::country_matches_part_of_the_name()
{
    // Country names arrive spelled differently from different loggers,
    // and half of them are two words.
    LogFilter f;
    f.country = QStringLiteral("austr");
    QVERIFY(f.matches(entry()));
    f.country = QStringLiteral("AUSTRIA");
    QVERIFY(f.matches(entry()));
    f.country = QStringLiteral("Australia");
    QVERIFY(!f.matches(entry()));
}

void TstLogFilter::dates_are_inclusive_at_both_ends()
{
    // An operator asking for 1 to 31 May means the whole month. An
    // exclusive bound quietly loses the last day, which is the sort of
    // thing that is only noticed when a contact seems to be missing.
    LogFilter f;
    f.useDates = true;
    f.from = QDate(2026, 5, 20);
    f.to   = QDate(2026, 5, 20);
    QVERIFY(f.matches(entry()));

    f.from = QDate(2026, 5, 1);
    f.to   = QDate(2026, 5, 31);
    QVERIFY(f.matches(entry()));

    f.from = QDate(2026, 5, 21);
    QVERIFY(!f.matches(entry()));

    f.from = QDate(2026, 5, 1);
    f.to   = QDate(2026, 5, 19);
    QVERIFY(!f.matches(entry()));
}

void TstLogFilter::dates_are_ignored_until_switched_on()
{
    // The date boxes hold something the moment the window opens. If
    // they applied without being switched on, opening the logbook would
    // show a filtered log with no sign that it was filtered.
    LogFilter f;
    f.from = QDate(2020, 1, 1);
    f.to   = QDate(2020, 12, 31);
    QVERIFY(!f.useDates);
    QVERIFY(!f.isActive());
    QVERIFY(f.matches(entry()));
}

void TstLogFilter::a_contact_with_no_date_is_excluded_by_a_date_range()
{
    LogEntry undated = entry();
    undated.timeOn = QDateTime{};

    LogFilter f;
    f.useDates = true;
    f.from = QDate(2020, 1, 1);
    f.to   = QDate(2030, 1, 1);
    // It cannot be shown to fall inside the range, so it does not.
    // Including it would put a contact in a range the operator can see
    // it is not in.
    QVERIFY(!f.matches(undated));

    f.useDates = false;
    QVERIFY(f.matches(undated));
}

void TstLogFilter::free_text_reaches_every_field()
{
    LogFilter f;
    for (const QString& needle : {QStringLiteral("OE1"), QStringLiteral("hans"),
                                  QStringLiteral("Linz"),
                                  QStringLiteral("austria"),
                                  QStringLiteral("JN67"), QStringLiteral("40m"),
                                  QStringLiteral("ssb"), QStringLiteral("lsb"),
                                  QStringLiteral("antenna")}) {
        f.text = needle;
        QVERIFY2(f.matches(entry()), qPrintable(needle));
    }
    f.text = QStringLiteral("Melbourne");
    QVERIFY(!f.matches(entry()));
}

void TstLogFilter::fields_combine_with_and()
{
    LogFilter f;
    f.band = QStringLiteral("40m");
    f.grid = QStringLiteral("JN");
    QVERIFY(f.matches(entry()));

    // One wrong field is enough to exclude. Anything else and narrowing
    // a search would widen it.
    f.grid = QStringLiteral("IO");
    QVERIFY(!f.matches(entry()));
}

void TstLogFilter::whitespace_only_is_not_a_filter()
{
    // A space left in a box after clearing it must not hide the log.
    LogFilter f;
    f.text = QStringLiteral("   ");
    f.band = QStringLiteral(" ");
    f.grid = QStringLiteral("  ");
    QVERIFY(!f.isActive());
    QVERIFY(f.matches(entry()));
}

QTEST_APPLESS_MAIN(TstLogFilter)
#include "tst_log_filter.moc"
