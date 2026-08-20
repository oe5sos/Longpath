// =================================================================
// tests/tst_qso_confirmation.cpp  (NereusSDR)
// =================================================================
//
// The tempting implementation is "if the field is not empty, it is
// confirmed". That reads a QSL you REQUESTED as one you received, and
// an award count built on it is a claim the operator cannot back up.
//
// So most of these tests are about the values that are NOT
// confirmations.
//
// no-port-check: NereusSDR-original.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/QsoConfirmation.h"

#include <QtTest/QtTest>

using namespace Longpath;
using State = QsoConfirmation::State;

namespace {

LogEntry withField(const QString& name, const QString& value)
{
    LogEntry e;
    e.call = QStringLiteral("DL2AB");
    e.extras.append(qMakePair(name, value));
    return e;
}

} // namespace

class TstQsoConfirmation : public QObject {
    Q_OBJECT
private slots:

    void y_and_v_are_the_only_confirmations()
    {
        QCOMPARE(QsoConfirmation::parse(QStringLiteral("Y")), State::Confirmed);
        QCOMPARE(QsoConfirmation::parse(QStringLiteral("V")), State::Confirmed);
        // LoTW writes V for "verified" and plenty of software writes
        // lower case.
        QCOMPARE(QsoConfirmation::parse(QStringLiteral("y")), State::Confirmed);
        QCOMPARE(QsoConfirmation::parse(QStringLiteral(" v ")), State::Confirmed);
    }

    // The whole point of the module.
    void requested_is_not_confirmed()
    {
        QCOMPARE(QsoConfirmation::parse(QStringLiteral("R")), State::Requested);
        QVERIFY(!QsoConfirmation::isConfirmed(
            withField(QStringLiteral("QSL_RCVD"), QStringLiteral("R"))));
    }

    void no_and_ignore_are_not_confirmed()
    {
        QCOMPARE(QsoConfirmation::parse(QStringLiteral("N")), State::No);
        QCOMPARE(QsoConfirmation::parse(QStringLiteral("I")), State::Ignored);
        for (const char* v : {"N", "I"}) {
            QVERIFY(!QsoConfirmation::isConfirmed(
                withField(QStringLiteral("LOTW_QSL_RCVD"),
                          QString::fromLatin1(v))));
        }
    }

    // Empty is not N. N means somebody said no; empty means nobody has
    // said. An award report that counts them the same is wrong twice.
    void empty_is_unknown_and_not_a_refusal()
    {
        QCOMPARE(QsoConfirmation::parse(QString()), State::Unknown);
        QCOMPARE(QsoConfirmation::parse(QStringLiteral("   ")), State::Unknown);
        QVERIFY(QsoConfirmation::parse(QString()) != State::No);
    }

    void an_unrecognised_value_is_unknown_rather_than_guessed()
    {
        for (const char* v : {"X", "YES", "1", "true", "??"}) {
            QCOMPARE(QsoConfirmation::parse(QString::fromLatin1(v)),
                     State::Unknown);
        }
    }

    void a_missing_field_reads_as_unknown()
    {
        LogEntry bare;
        QCOMPARE(QsoConfirmation::qslCard(bare), State::Unknown);
        QCOMPARE(QsoConfirmation::lotw(bare),    State::Unknown);
        QVERIFY(!QsoConfirmation::isConfirmed(bare));
    }

    void field_names_are_matched_without_regard_to_case()
    {
        // The parser upper-cases on the way in, but a settings file
        // written by hand might not have.
        QCOMPARE(QsoConfirmation::lotw(
                     withField(QStringLiteral("lotw_qsl_rcvd"),
                               QStringLiteral("Y"))),
                 State::Confirmed);
    }

    void any_one_of_the_three_is_enough()
    {
        QVERIFY(QsoConfirmation::isConfirmed(
            withField(QStringLiteral("QSL_RCVD"), QStringLiteral("Y"))));
        QVERIFY(QsoConfirmation::isConfirmed(
            withField(QStringLiteral("LOTW_QSL_RCVD"), QStringLiteral("Y"))));
        QVERIFY(QsoConfirmation::isConfirmed(
            withField(QStringLiteral("EQSL_QSL_RCVD"), QStringLiteral("Y"))));
    }

    void the_badge_is_blank_when_nothing_is_confirmed()
    {
        // An unconfirmed contact should read as an empty cell, not as a
        // row of dashes competing with the confirmed ones for attention.
        QVERIFY(QsoConfirmation::badge(LogEntry{}).isEmpty());
        QVERIFY(QsoConfirmation::badge(
            withField(QStringLiteral("QSL_RCVD"),
                      QStringLiteral("R"))).isEmpty());
    }

    void the_badge_names_every_source_that_confirmed()
    {
        LogEntry e;
        e.extras.append(qMakePair(QStringLiteral("LOTW_QSL_RCVD"),
                                  QStringLiteral("Y")));
        e.extras.append(qMakePair(QStringLiteral("QSL_RCVD"),
                                  QStringLiteral("Y")));
        const QString b = QsoConfirmation::badge(e);
        QVERIFY(b.contains(QLatin1Char('L')));
        QVERIFY(b.contains(QLatin1Char('C')));
        QVERIFY(!b.contains(QLatin1Char('e')));
    }

    void describe_distinguishes_silence_from_refusal()
    {
        const QString nothing = QsoConfirmation::describe(LogEntry{});
        QVERIFY(nothing.contains(QStringLiteral("either way")));

        const QString refused = QsoConfirmation::describe(
            withField(QStringLiteral("QSL_RCVD"), QStringLiteral("N")));
        QVERIFY(refused.contains(QStringLiteral("not confirmed")));
        QVERIFY(refused != nothing);
    }
};

QTEST_APPLESS_MAIN(TstQsoConfirmation)
#include "tst_qso_confirmation.moc"
