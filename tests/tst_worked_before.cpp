// "Have I had this one, and does it count for anything?"
//
// The award rules are the substance here, and getting them wrong is
// costly in a specific way: telling an operator a contact is new when
// it is not sends them chasing something worth nothing, and telling
// them it is not new when it is loses a contact they wanted.
//
// The DXCC prefix resolver is injected, so these run without cty.dat
// and the award logic is tested rather than the prefix table.
// no-port-check: NereusSDR-original.

#include <QtTest/QtTest>
#include <QTimeZone>

#include "core/WorkedBefore.h"

using namespace Longpath;

namespace {

LogEntry qso(const QString& call, const QString& band, const QString& mode,
             int day = 1, const QString& submode = {})
{
    LogEntry e;
    e.call = call;
    e.band = band;
    e.mode = mode;
    e.submode = submode;
    e.timeOn = QDateTime(QDate(2026, 8, day), QTime(12, 0), QTimeZone::UTC);
    return e;
}

// A stand-in cty.dat: first two characters, which is close enough for
// the callsigns used here and keeps the test about the award logic.
QString fakePrefix(const QString& call)
{
    if (call.startsWith(QStringLiteral("OE"))) { return QStringLiteral("OE"); }
    if (call.startsWith(QStringLiteral("DL"))) { return QStringLiteral("DL"); }
    if (call.startsWith(QStringLiteral("JA"))) { return QStringLiteral("JA"); }
    if (call.startsWith(QStringLiteral("VK"))) { return QStringLiteral("VK"); }
    return {};
}

const WorkedBefore::PrefixResolver kResolver = fakePrefix;

} // namespace

class TstWorkedBefore : public QObject {
    Q_OBJECT
private slots:
    void phone_modes_share_one_award_slot();
    void data_modes_share_one_award_slot();
    void an_empty_log_makes_everything_new();
    void a_worked_entity_is_not_new_on_a_different_callsign();
    void a_new_band_for_a_worked_entity();
    void a_new_mode_group_for_a_worked_entity();
    void ft8_after_rtty_is_not_a_new_mode();
    void counts_and_last_date_are_per_callsign();
    void an_unresolved_prefix_says_so_rather_than_guessing();
    void an_unknown_band_is_not_claimed_as_new();
    void duplicates_use_the_same_rule_as_the_importer();
};

void TstWorkedBefore::phone_modes_share_one_award_slot()
{
    QCOMPARE(WorkedBefore::modeGroup(QStringLiteral("SSB")),
             QStringLiteral("PHONE"));
    QCOMPARE(WorkedBefore::modeGroup(QStringLiteral("AM")),
             QStringLiteral("PHONE"));
    QCOMPARE(WorkedBefore::modeGroup(QStringLiteral("FM")),
             QStringLiteral("PHONE"));
    // Some logs record only the submode.
    QCOMPARE(WorkedBefore::modeGroup(QString{}, QStringLiteral("LSB")),
             QStringLiteral("UNKNOWN"));   // no mode at all is unknown
    QCOMPARE(WorkedBefore::modeGroup(QStringLiteral("SSB"),
                                     QStringLiteral("LSB")),
             QStringLiteral("PHONE"));
    QCOMPARE(WorkedBefore::modeGroup(QStringLiteral("CW")),
             QStringLiteral("CW"));
}

void TstWorkedBefore::data_modes_share_one_award_slot()
{
    for (const QString& m : {QStringLiteral("FT8"), QStringLiteral("FT4"),
                             QStringLiteral("RTTY"), QStringLiteral("PSK31"),
                             QStringLiteral("JT65"), QStringLiteral("MFSK")}) {
        QCOMPARE(WorkedBefore::modeGroup(m), QStringLiteral("DATA"));
    }
}

void TstWorkedBefore::an_empty_log_makes_everything_new()
{
    WorkedBefore w;
    w.rebuild({}, kResolver);
    const WorkedSummary s = w.lookup(QStringLiteral("OE1W"),
                                     QStringLiteral("40m"),
                                     QStringLiteral("SSB"), kResolver);
    QVERIFY(s.knownEntity);
    QVERIFY(s.newEntity);
    QVERIFY(s.newBand);
    QVERIFY(s.newMode);
    QVERIFY(s.isNew());
    QCOMPARE(s.timesWorked, 0);
    QVERIFY(!s.lastWorked.isValid());
}

void TstWorkedBefore::a_worked_entity_is_not_new_on_a_different_callsign()
{
    // The award is on the entity, not the station. Having had OE1W, a
    // fresh OE3ABC on the same band and mode is a new contact but not a
    // new anything — and saying otherwise is the mistake that matters.
    WorkedBefore w;
    w.rebuild({qso(QStringLiteral("OE1W"), QStringLiteral("40m"),
                   QStringLiteral("SSB"))}, kResolver);

    const WorkedSummary s = w.lookup(QStringLiteral("OE3ABC"),
                                     QStringLiteral("40m"),
                                     QStringLiteral("SSB"), kResolver);
    QVERIFY(!s.newEntity);
    QVERIFY(!s.newBand);
    QVERIFY(!s.newMode);
    QVERIFY(!s.isNew());
    QCOMPARE(s.timesWorked, 0);          // this callsign, never
}

void TstWorkedBefore::a_new_band_for_a_worked_entity()
{
    WorkedBefore w;
    w.rebuild({qso(QStringLiteral("OE1W"), QStringLiteral("40m"),
                   QStringLiteral("SSB"))}, kResolver);

    const WorkedSummary s = w.lookup(QStringLiteral("OE1W"),
                                     QStringLiteral("20m"),
                                     QStringLiteral("SSB"), kResolver);
    QVERIFY(!s.newEntity);
    QVERIFY(s.newBand);
    QVERIFY(!s.newMode);
    QVERIFY(s.isNew());
    QCOMPARE(s.timesWorked, 1);
}

void TstWorkedBefore::a_new_mode_group_for_a_worked_entity()
{
    WorkedBefore w;
    w.rebuild({qso(QStringLiteral("JA1XYZ"), QStringLiteral("20m"),
                   QStringLiteral("SSB"))}, kResolver);

    const WorkedSummary s = w.lookup(QStringLiteral("JA1XYZ"),
                                     QStringLiteral("20m"),
                                     QStringLiteral("CW"), kResolver);
    QVERIFY(!s.newEntity);
    QVERIFY(!s.newBand);
    QVERIFY(s.newMode);
    QVERIFY(s.isNew());
}

void TstWorkedBefore::ft8_after_rtty_is_not_a_new_mode()
{
    // The rule that costs an operator a wasted call if it is wrong.
    WorkedBefore w;
    w.rebuild({qso(QStringLiteral("VK3ABC"), QStringLiteral("20m"),
                   QStringLiteral("RTTY"))}, kResolver);

    const WorkedSummary s = w.lookup(QStringLiteral("VK3ABC"),
                                     QStringLiteral("20m"),
                                     QStringLiteral("FT8"), kResolver);
    QVERIFY(!s.newMode);
    QVERIFY(!s.isNew());
}

void TstWorkedBefore::counts_and_last_date_are_per_callsign()
{
    WorkedBefore w;
    w.rebuild({
        qso(QStringLiteral("OE1W"), QStringLiteral("40m"),
            QStringLiteral("SSB"), 1),
        qso(QStringLiteral("oe1w"), QStringLiteral("20m"),
            QStringLiteral("CW"), 5),      // same station, lower case
        qso(QStringLiteral("DL1AB"), QStringLiteral("40m"),
            QStringLiteral("SSB"), 9),
    }, kResolver);

    const WorkedSummary s = w.lookup(QStringLiteral("OE1W"),
                                     QStringLiteral("15m"),
                                     QStringLiteral("SSB"), kResolver);
    QCOMPARE(s.timesWorked, 2);
    QCOMPARE(s.lastWorked.toUTC().date(), QDate(2026, 8, 5));
}

void TstWorkedBefore::an_unresolved_prefix_says_so_rather_than_guessing()
{
    // A callsign cty.dat cannot place must not be reported as a new
    // entity — that is the single most exciting thing this can say, and
    // saying it on a typo would make the whole line untrustworthy.
    WorkedBefore w;
    w.rebuild({qso(QStringLiteral("OE1W"), QStringLiteral("40m"),
                   QStringLiteral("SSB"))}, kResolver);

    const WorkedSummary s = w.lookup(QStringLiteral("ZZ9QQ"),
                                     QStringLiteral("40m"),
                                     QStringLiteral("SSB"), kResolver);
    QVERIFY(!s.knownEntity);
    QVERIFY(!s.isNew());
    QVERIFY(s.entity.isEmpty());
    QCOMPARE(s.timesWorked, 0);
}

void TstWorkedBefore::an_unknown_band_is_not_claimed_as_new()
{
    // The radio may not have reported a band yet. "New band" then means
    // "no band", which is not an award.
    WorkedBefore w;
    w.rebuild({qso(QStringLiteral("OE1W"), QStringLiteral("40m"),
                   QStringLiteral("SSB"))}, kResolver);

    const WorkedSummary s = w.lookup(QStringLiteral("OE1W"), QString{},
                                     QString{}, kResolver);
    QVERIFY(!s.newBand);
    QVERIFY(!s.newMode);
    QVERIFY(!s.isNew());
}

void TstWorkedBefore::duplicates_use_the_same_rule_as_the_importer()
{
    WorkedBefore w;
    w.rebuild({qso(QStringLiteral("OE1W"), QStringLiteral("40m"),
                   QStringLiteral("SSB"), 7)}, kResolver);

    LogEntry again = qso(QStringLiteral("OE1W"), QStringLiteral("40m"),
                         QStringLiteral("SSB"), 7);
    QVERIFY(w.wouldDuplicate(again));

    // Half an hour later on the same band is a second contact, not a
    // duplicate — the importer's two-minute window, shared so the panel
    // and the importer cannot disagree about what "the same QSO" means.
    again.timeOn = again.timeOn.addSecs(1800);
    QVERIFY(!w.wouldDuplicate(again));

    // Different band, same minute: also not a duplicate.
    LogEntry otherBand = qso(QStringLiteral("OE1W"), QStringLiteral("20m"),
                             QStringLiteral("SSB"), 7);
    QVERIFY(!w.wouldDuplicate(otherBand));

    QVERIFY(!w.wouldDuplicate(qso(QStringLiteral("DL1AB"),
                                  QStringLiteral("40m"),
                                  QStringLiteral("SSB"), 7)));
}

QTEST_APPLESS_MAIN(TstWorkedBefore)
#include "tst_worked_before.moc"
