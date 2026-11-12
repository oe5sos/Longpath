// The globe's two pieces of real maths, tested without a window: where
// the sun is overhead (which draws the terminator) and where the great
// circle runs (which draws the path). Both are static so the geometry is
// checkable without constructing a widget.
// no-port-check: Longpath-original.

#include <QtTest/QtTest>
#include <QDateTime>
#include <QTimeZone>

#include "gui/widgets/GlobeWidget.h"

using namespace Longpath;

namespace {

// Great-circle distance in degrees of arc, used to assert that an
// interpolated point actually lies on the path rather than near it.
double arcDeg(double lat1, double lon1, double lat2, double lon2)
{
    const double d = M_PI / 180.0;
    const double a = std::sin((lat2 - lat1) * d / 2) * std::sin((lat2 - lat1) * d / 2)
        + std::cos(lat1 * d) * std::cos(lat2 * d)
              * std::sin((lon2 - lon1) * d / 2) * std::sin((lon2 - lon1) * d / 2);
    return 2.0 * std::asin(std::min(1.0, std::sqrt(a))) / d;
}

QDateTime utc(int y, int m, int d, int h, int mi)
{
    return QDateTime(QDate(y, m, d), QTime(h, mi), QTimeZone::UTC);
}

} // namespace

class TstGlobeGeometry : public QObject {
    Q_OBJECT
private slots:
    void subsolar_latitude_tracks_the_seasons();
    void subsolar_longitude_tracks_the_clock();
    void subsolar_longitude_stays_in_range();
    void subsolar_point_matches_noaa_over_the_year_data();
    void subsolar_point_matches_noaa_over_the_year();
    void interpolation_hits_both_endpoints();
    void interpolated_points_lie_on_the_great_circle();
    void path_over_the_pole_is_not_a_straight_lat_lon_line();
    void degenerate_path_does_not_divide_by_zero();
    void destination_and_bearing_are_inverses();
    void beam_offsets_separate_with_distance();
    void bearing_matches_the_maidenhead_helper();
};

void TstGlobeGeometry::subsolar_latitude_tracks_the_seasons()
{
    double lat = 0.0, lon = 0.0;

    // Northern solstice: the sun is overhead near the Tropic of Cancer.
    GlobeWidget::subsolarPoint(utc(2026, 6, 21, 12, 0), lat, lon);
    QVERIFY2(lat > 22.0 && lat < 24.0,
             qPrintable(QStringLiteral("June: %1").arg(lat)));

    // Southern solstice: mirrored.
    GlobeWidget::subsolarPoint(utc(2026, 12, 21, 12, 0), lat, lon);
    QVERIFY2(lat < -22.0 && lat > -24.0,
             qPrintable(QStringLiteral("December: %1").arg(lat)));

    // Equinoxes: on the equator, within the tolerance of a low-precision
    // model (a couple of degrees).
    GlobeWidget::subsolarPoint(utc(2026, 3, 20, 12, 0), lat, lon);
    QVERIFY2(std::abs(lat) < 2.5,
             qPrintable(QStringLiteral("March: %1").arg(lat)));
    GlobeWidget::subsolarPoint(utc(2026, 9, 23, 12, 0), lat, lon);
    QVERIFY2(std::abs(lat) < 2.5,
             qPrintable(QStringLiteral("September: %1").arg(lat)));
}

void TstGlobeGeometry::subsolar_longitude_tracks_the_clock()
{
    double lat = 0.0, lon = 0.0;

    // At 12:00 UTC the sun is near the Greenwich meridian; the equation
    // of time moves it by up to about four degrees over the year.
    GlobeWidget::subsolarPoint(utc(2026, 3, 20, 12, 0), lat, lon);
    QVERIFY2(std::abs(lon) < 5.0,
             qPrintable(QStringLiteral("noon: %1").arg(lon)));

    // Six hours later it has moved 90 degrees west.
    double lon6 = 0.0;
    GlobeWidget::subsolarPoint(utc(2026, 3, 20, 18, 0), lat, lon6);
    double moved = lon - lon6;
    while (moved < 0.0)   { moved += 360.0; }
    while (moved > 360.0) { moved -= 360.0; }
    QVERIFY2(std::abs(moved - 90.0) < 2.0,
             qPrintable(QStringLiteral("moved %1 deg").arg(moved)));
}

void TstGlobeGeometry::subsolar_longitude_stays_in_range()
{
    // Every hour of a day must land inside [-180, 180]. A wrap bug here
    // would put the terminator on the wrong side of the globe for a few
    // hours a day — intermittent, and easy to blame on something else.
    for (int h = 0; h < 24; ++h) {
        double lat = 0.0, lon = 0.0;
        GlobeWidget::subsolarPoint(utc(2026, 8, 7, h, 30), lat, lon);
        QVERIFY2(lon >= -180.0 && lon <= 180.0,
                 qPrintable(QStringLiteral("hour %1: %2").arg(h).arg(lon)));
        QVERIFY(lat >= -23.5 && lat <= 23.5);
    }
}

// ── Gegen NOAA, ueber das ganze Jahr (2026-11-10) ───────────────────
//
// Die drei Pruefungen darueber lassen „ein paar Grad" zu und haben
// deshalb nie gesehen, dass die alte Zeitgleichung bis 17 Minuten
// danebenlag (Schattengrenze bis 4,4° in der Laenge falsch). Hier die
// Werte nach den Formeln des NOAA-Solar-Calculator-Blatts (Meeus), in
// einem eigenen Skript gerechnet: Deklination und der Meridian, auf dem
// es wahrer Mittag ist. Je Monat der 1. zu wandernder Stunde und der
// 15. um 12 UTC, dazu die zwei schlimmsten Tage der alten Formel.
void TstGlobeGeometry::subsolar_point_matches_noaa_over_the_year_data()
{
    QTest::addColumn<QDateTime>("t");
    QTest::addColumn<double>("noaaLat");
    QTest::addColumn<double>("noaaLon");

    QTest::newRow("2026-01-01 01:00") << utc(2026, 1, 1, 1, 0) << -23.01 << 165.84;   // EoT -3.4 min
    QTest::newRow("2026-01-15 12:00") << utc(2026, 1, 15, 12, 0) << -21.07 << 2.35;   // EoT -9.4 min
    QTest::newRow("2026-02-01 03:00") << utc(2026, 2, 1, 3, 0) << -17.12 << 138.38;   // EoT -13.5 min
    QTest::newRow("2026-02-15 12:00") << utc(2026, 2, 15, 12, 0) << -12.58 << 3.53;   // EoT -14.1 min
    QTest::newRow("2026-03-01 05:00") << utc(2026, 3, 1, 5, 0) << -7.58 << 108.09;   // EoT -12.4 min
    QTest::newRow("2026-03-15 12:00") << utc(2026, 3, 15, 12, 0) << -2.02 << 2.22;   // EoT -8.9 min
    QTest::newRow("2026-04-01 07:00") << utc(2026, 4, 1, 7, 0) << 4.58 << 75.97;   // EoT -3.9 min
    QTest::newRow("2026-04-15 12:00") << utc(2026, 4, 15, 12, 0) << 9.87 << 0.01;   // EoT -0.0 min
    QTest::newRow("2026-05-01 09:00") << utc(2026, 5, 1, 9, 0) << 15.13 << 44.28;   // EoT +2.9 min
    QTest::newRow("2026-05-15 12:00") << utc(2026, 5, 15, 12, 0) << 18.94 << -0.91;   // EoT +3.6 min
    QTest::newRow("2026-06-01 11:00") << utc(2026, 6, 1, 11, 0) << 22.09 << 14.47;   // EoT +2.1 min
    QTest::newRow("2026-06-15 12:00") << utc(2026, 6, 15, 12, 0) << 23.32 << 0.13;   // EoT -0.5 min
    QTest::newRow("2026-07-01 13:00") << utc(2026, 7, 1, 13, 0) << 23.08 << -14.02;   // EoT -3.9 min
    QTest::newRow("2026-07-15 12:00") << utc(2026, 7, 15, 12, 0) << 21.47 << 1.50;   // EoT -6.0 min
    QTest::newRow("2026-08-01 15:00") << utc(2026, 8, 1, 15, 0) << 17.90 << -43.41;   // EoT -6.4 min
    QTest::newRow("2026-08-15 12:00") << utc(2026, 8, 15, 12, 0) << 13.95 << 1.13;   // EoT -4.5 min
    QTest::newRow("2026-09-01 17:00") << utc(2026, 9, 1, 17, 0) << 8.09 << -75.01;   // EoT +0.0 min
    QTest::newRow("2026-09-15 12:00") << utc(2026, 9, 15, 12, 0) << 2.91 << -1.19;   // EoT +4.8 min
    QTest::newRow("2026-10-01 19:00") << utc(2026, 10, 1, 19, 0) << -3.42 << -107.61;   // EoT +10.4 min
    QTest::newRow("2026-10-15 12:00") << utc(2026, 10, 15, 12, 0) << -8.62 << -3.56;   // EoT +14.2 min
    QTest::newRow("2026-11-01 21:00") << utc(2026, 11, 1, 21, 0) << -14.64 << -139.12;   // EoT +16.5 min
    QTest::newRow("2026-11-15 12:00") << utc(2026, 11, 15, 12, 0) << -18.55 << -3.86;   // EoT +15.4 min
    QTest::newRow("2026-12-01 23:00") << utc(2026, 12, 1, 23, 0) << -21.91 << -167.70;   // EoT +10.8 min
    QTest::newRow("2026-12-15 12:00") << utc(2026, 12, 15, 12, 0) << -23.28 << -1.22;   // EoT +4.9 min
    QTest::newRow("2026-03-30 12:00") << utc(2026, 3, 30, 12, 0) << 3.88 << 1.11;   // EoT -4.4 min
    QTest::newRow("2026-11-03 12:00") << utc(2026, 11, 3, 12, 0) << -15.15 << -4.12;   // EoT +16.5 min
    QTest::newRow("2027-02-11 06:15") << utc(2027, 2, 11, 6, 15) << -14.08 << 89.81;   // EoT -14.2 min
}

void TstGlobeGeometry::subsolar_point_matches_noaa_over_the_year()
{
    QFETCH(QDateTime, t);
    QFETCH(double, noaaLat);
    QFETCH(double, noaaLon);
    double lat = 0.0, lon = 0.0;
    GlobeWidget::subsolarPoint(t, lat, lon);
    double dLon = std::fmod(lon - noaaLon + 540.0, 360.0) - 180.0;
    // 0,3° Breite, 0,5° Laenge: das Modell aus core/SolarTimes liegt
    // ueber das Jahr bei hoechstens 0,18° bzw. 0,4°.
    QVERIFY2(std::abs(lat - noaaLat) < 0.3,
             qPrintable(QStringLiteral("Breite %1 statt %2").arg(lat).arg(noaaLat)));
    QVERIFY2(std::abs(dLon) < 0.5,
             qPrintable(QStringLiteral("Laenge %1 statt %2").arg(lon).arg(noaaLon)));
}

void TstGlobeGeometry::interpolation_hits_both_endpoints()
{
    const double aLat = 48.3, aLon = 14.3;    // Linz
    const double bLat = 40.7, bLon = -74.0;   // New York
    double lat = 0.0, lon = 0.0;

    GlobeWidget::interpolateGreatCircle(aLat, aLon, bLat, bLon, 0.0, lat, lon);
    QVERIFY(arcDeg(lat, lon, aLat, aLon) < 0.01);

    GlobeWidget::interpolateGreatCircle(aLat, aLon, bLat, bLon, 1.0, lat, lon);
    QVERIFY(arcDeg(lat, lon, bLat, bLon) < 0.01);
}

void TstGlobeGeometry::interpolated_points_lie_on_the_great_circle()
{
    const double aLat = 48.3, aLon = 14.3;    // Linz
    const double bLat = -33.9, bLon = 151.2;  // Sydney
    const double total = arcDeg(aLat, aLon, bLat, bLon);

    // On a great circle, arc(A→P) + arc(P→B) equals arc(A→B) exactly.
    // Any point off the path makes the sum larger.
    for (double f = 0.1; f < 0.99; f += 0.1) {
        double lat = 0.0, lon = 0.0;
        GlobeWidget::interpolateGreatCircle(aLat, aLon, bLat, bLon, f, lat, lon);
        const double sum = arcDeg(aLat, aLon, lat, lon)
                         + arcDeg(lat, lon, bLat, bLon);
        QVERIFY2(std::abs(sum - total) < 0.05,
                 qPrintable(QStringLiteral("f=%1 sum %2 vs %3")
                                .arg(f).arg(sum).arg(total)));
    }
}

void TstGlobeGeometry::path_over_the_pole_is_not_a_straight_lat_lon_line()
{
    // Linz to Anchorage runs far north of the latitudes of either end.
    // Linear interpolation in lat/lon would stay between 48 and 61
    // degrees; the great circle goes above 70. This is the whole reason
    // the path is slerped rather than drawn as a screen line.
    double lat = 0.0, lon = 0.0;
    GlobeWidget::interpolateGreatCircle(48.3, 14.3, 61.2, -149.9, 0.5, lat, lon);
    QVERIFY2(lat > 70.0, qPrintable(QStringLiteral("midpoint lat %1").arg(lat)));
}

void TstGlobeGeometry::degenerate_path_does_not_divide_by_zero()
{
    // Working a station in your own grid: sin(d) is zero and the slerp
    // would produce NaN if the short-circuit were missing.
    double lat = 1.0, lon = 1.0;
    GlobeWidget::interpolateGreatCircle(48.3, 14.3, 48.3, 14.3, 0.5, lat, lon);
    QVERIFY(!std::isnan(lat));
    QVERIFY(!std::isnan(lon));
    QCOMPARE(lat, 48.3);
    QCOMPARE(lon, 14.3);
}

void TstGlobeGeometry::destination_and_bearing_are_inverses()
{
    // Set off from Linz on a heading for a given arc, and the bearing
    // back to where you arrived must be the heading you set off on.
    // The flanking beam arcs are built entirely on this, so if it drifts
    // the main lobe would be drawn pointing somewhere it is not.
    const double home[2] = {48.3, 14.3};
    for (double bearing : {0.0, 45.0, 118.0, 180.0, 298.0, 355.0}) {
        for (double dist : {5.0, 40.0, 120.0}) {
            double lat = 0, lon = 0;
            GlobeWidget::destinationPoint(home[0], home[1], bearing, dist,
                                          lat, lon);
            const double back =
                GlobeWidget::initialBearing(home[0], home[1], lat, lon);
            double diff = std::fmod(std::abs(back - bearing) + 360.0, 360.0);
            if (diff > 180.0) { diff = 360.0 - diff; }
            QVERIFY2(diff < 0.01,
                     qPrintable(QStringLiteral("b=%1 d=%2 got %3")
                                    .arg(bearing).arg(dist).arg(back)));

            const double arc =
                GlobeWidget::angularDistance(home[0], home[1], lat, lon);
            QVERIFY2(std::abs(arc - dist) < 0.01,
                     qPrintable(QStringLiteral("arc %1 vs %2").arg(arc).arg(dist)));
        }
    }
}

void TstGlobeGeometry::beam_offsets_separate_with_distance()
{
    // Five degrees of beam width is a few hundred kilometres at short
    // range and thousands across an ocean. If the flanking arcs did not
    // widen with distance they would be decoration rather than an
    // answer to "is the rotator close enough".
    const double home[2] = {48.3, 14.3};
    const double bearing = 298.0;

    auto spreadAt = [&](double dist) {
        double aLat = 0, aLon = 0, bLat = 0, bLon = 0;
        GlobeWidget::destinationPoint(home[0], home[1], bearing - 5.0, dist,
                                      aLat, aLon);
        GlobeWidget::destinationPoint(home[0], home[1], bearing + 5.0, dist,
                                      bLat, bLon);
        return GlobeWidget::angularDistance(aLat, aLon, bLat, bLon);
    };

    const double near = spreadAt(5.0);
    const double far  = spreadAt(60.0);
    QVERIFY2(far > near * 5.0,
             qPrintable(QStringLiteral("near %1 far %2").arg(near).arg(far)));

    // At the target the two flanks straddle the centre symmetrically.
    double cLat = 0, cLon = 0;
    GlobeWidget::destinationPoint(home[0], home[1], bearing, 60.0, cLat, cLon);
    double aLat = 0, aLon = 0, bLat = 0, bLon = 0;
    GlobeWidget::destinationPoint(home[0], home[1], bearing - 5.0, 60.0,
                                  aLat, aLon);
    GlobeWidget::destinationPoint(home[0], home[1], bearing + 5.0, 60.0,
                                  bLat, bLon);
    const double toA = GlobeWidget::angularDistance(cLat, cLon, aLat, aLon);
    const double toB = GlobeWidget::angularDistance(cLat, cLon, bLat, bLon);
    QVERIFY2(std::abs(toA - toB) < 0.05,
             qPrintable(QStringLiteral("%1 vs %2").arg(toA).arg(toB)));
}

void TstGlobeGeometry::bearing_matches_the_maidenhead_helper()
{
    // Two implementations of the same thing now exist: this one, and the
    // ported Maidenhead helper the dial uses. They must agree, or the
    // needle and the globe would point different ways and neither would
    // look wrong on its own.
    //
    // JN67VV to FN30IV — Linz to Long Island, the pair from the screen
    // where this was first checked by hand: 298.5 degrees.
    const double b = GlobeWidget::initialBearing(47.8958, 13.7917,
                                                 40.8958, -73.2917);
    QVERIFY2(std::abs(b - 298.5) < 0.5,
             qPrintable(QStringLiteral("got %1").arg(b)));
}

QTEST_MAIN(TstGlobeGeometry)
#include "tst_globe_geometry.moc"
