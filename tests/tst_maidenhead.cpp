// Verify the shared Maidenhead helpers now that they are reachable
// outside FreeDVStationModel.cpp: locator validation, distance and
// bearing against known real-world pairs.
// no-port-check: maths ported from freedv-gui, attributed in
// core/Maidenhead.h; only the validator is NereusSDR-original.

#include <QtTest/QtTest>
#include "core/Maidenhead.h"

using namespace NereusSDR;

class TstMaidenhead : public QObject {
    Q_OBJECT
private slots:
    void validator_accepts_four_and_six_characters();
    void validator_rejects_malformed_locators();
    void distance_matches_known_paths();
    void bearing_matches_known_paths();
    void distance_to_self_is_zero();
    void bearing_is_not_symmetric();
};

void TstMaidenhead::validator_accepts_four_and_six_characters()
{
    QVERIFY(isValidGridSquare(QStringLiteral("JN67")));
    QVERIFY(isValidGridSquare(QStringLiteral("JN67VV")));
    QVERIFY(isValidGridSquare(QStringLiteral("jn67vv")));   // case-insensitive
    QVERIFY(isValidGridSquare(QStringLiteral("  JN67VV ")));// trimmed
    QVERIFY(isValidGridSquare(QStringLiteral("AA00")));     // corner of the world
    QVERIFY(isValidGridSquare(QStringLiteral("RR99XX")));   // other corner
}

void TstMaidenhead::validator_rejects_malformed_locators()
{
    QVERIFY(!isValidGridSquare(QString{}));
    QVERIFY(!isValidGridSquare(QStringLiteral("JN")));      // too short
    QVERIFY(!isValidGridSquare(QStringLiteral("JN6")));     // odd length
    QVERIFY(!isValidGridSquare(QStringLiteral("JN67V")));   // odd length
    QVERIFY(!isValidGridSquare(QStringLiteral("JN67VVX"))); // too long
    QVERIFY(!isValidGridSquare(QStringLiteral("6767")));    // digits in field
    QVERIFY(!isValidGridSquare(QStringLiteral("JNAB")));    // letters in square
    // Fields only run A..R; S would be off the globe.
    QVERIFY(!isValidGridSquare(QStringLiteral("SS67")));
    // Sub-squares only run A..X.
    QVERIFY(!isValidGridSquare(QStringLiteral("JN67ZZ")));
}

void TstMaidenhead::distance_matches_known_paths()
{
    // Linz (JN78) to London (IO91): roughly 1200 km.
    const double toLondon = calculateDistanceKm(QStringLiteral("JN78"),
                                                QStringLiteral("IO91"));
    QVERIFY2(toLondon > 1050 && toLondon < 1350,
             qPrintable(QStringLiteral("got %1 km").arg(toLondon)));

    // Linz to Sydney (QF56): roughly 16000 km — the antipodal end of
    // the scale, which catches a sign or radius error the short path
    // would hide.
    const double toSydney = calculateDistanceKm(QStringLiteral("JN78"),
                                                QStringLiteral("QF56"));
    QVERIFY2(toSydney > 15000 && toSydney < 17000,
             qPrintable(QStringLiteral("got %1 km").arg(toSydney)));
}

void TstMaidenhead::bearing_matches_known_paths()
{
    // Linz to London is west-north-west.
    const double toLondon = calculateBearingInDegrees(QStringLiteral("JN78"),
                                                      QStringLiteral("IO91"));
    QVERIFY2(toLondon > 280 && toLondon < 320,
             qPrintable(QStringLiteral("got %1 deg").arg(toLondon)));

    // Linz to Cape Town (JF96) is roughly south, slightly east.
    const double toCapeTown = calculateBearingInDegrees(QStringLiteral("JN78"),
                                                        QStringLiteral("JF96"));
    QVERIFY2(toCapeTown > 150 && toCapeTown < 200,
             qPrintable(QStringLiteral("got %1 deg").arg(toCapeTown)));
}

void TstMaidenhead::distance_to_self_is_zero()
{
    QVERIFY(calculateDistanceKm(QStringLiteral("JN67VV"),
                                QStringLiteral("JN67VV")) < 1.0);
}

void TstMaidenhead::bearing_is_not_symmetric()
{
    // Great-circle bearings are not reciprocal by 180 degrees except on
    // a meridian; a naive implementation that just negates would pass
    // the forward test and fail here.
    const double there = calculateBearingInDegrees(QStringLiteral("JN78"),
                                                   QStringLiteral("FN31"));
    const double back  = calculateBearingInDegrees(QStringLiteral("FN31"),
                                                   QStringLiteral("JN78"));
    const double naive = std::fmod(there + 180.0, 360.0);
    QVERIFY2(std::abs(back - naive) > 5.0,
             qPrintable(QStringLiteral("there %1, back %2").arg(there).arg(back)));
}

QTEST_APPLESS_MAIN(TstMaidenhead)
#include "tst_maidenhead.moc"
