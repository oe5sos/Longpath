// Sunrise, sunset and sun elevation.
//
// The reference times below come from the NOAA solar calculator's own
// formulas, computed independently, not from a remembered almanac — an
// almanac time misremembered by ten minutes would have quietly set the
// tolerance ten minutes too wide.
// no-port-check: Longpath-original.

#include <QtTest/QtTest>
#include <QTimeZone>

#include "core/SolarTimes.h"

#include <algorithm>
#include <cmath>

using namespace Longpath;

namespace {

QDateTime utc(int y, int m, int d, int h = 12, int mi = 0)
{
    return QDateTime(QDate(y, m, d), QTime(h, mi), QTimeZone::UTC);
}

// Minutes between an actual result and the NOAA reference.
double driftMin(const QDateTime& got, int h, int m)
{
    const QDateTime want(got.date(), QTime(h, m), QTimeZone::UTC);
    return std::abs(got.secsTo(want)) / 60.0;
}

} // namespace

class TstSolarTimes : public QObject {
    Q_OBJECT
private slots:
    void midlatitude_summer_matches_noaa();
    void midlatitude_winter_matches_noaa();
    void equator_days_are_near_twelve_hours();
    void southern_hemisphere_is_not_mirrored_wrongly();
    void polar_day_and_night_report_no_events();
    void elevation_at_sunrise_is_the_refraction_offset();
    void elevation_is_negative_at_local_midnight();
    void greyline_brackets_the_horizon();
    void invalid_input_is_survivable();
    void subsolar_point_agrees_with_elevation();
};

void TstSolarTimes::midlatitude_summer_matches_noaa()
{
    // Vienna, 7 Aug 2026. NOAA: 03:37:33 / 18:23:11 UTC.
    const SolarInfo s = solarInfo(utc(2026, 8, 7), 48.21, 16.37);
    QVERIFY(!s.alwaysUp && !s.alwaysDown);
    QVERIFY2(driftMin(s.riseUtc, 3, 38) < 3.0,
             qPrintable(s.riseUtc.toString(Qt::ISODate)));
    QVERIFY2(driftMin(s.setUtc, 18, 23) < 3.0,
             qPrintable(s.setUtc.toString(Qt::ISODate)));
}

void TstSolarTimes::midlatitude_winter_matches_noaa()
{
    // Vienna, 21 Dec 2026. NOAA: 06:42:12 / 15:02:29 UTC. The winter
    // solstice is where an error in the declination shows up largest.
    const SolarInfo s = solarInfo(utc(2026, 12, 21), 48.21, 16.37);
    QVERIFY2(driftMin(s.riseUtc, 6, 42) < 3.0,
             qPrintable(s.riseUtc.toString(Qt::ISODate)));
    QVERIFY2(driftMin(s.setUtc, 15, 2) < 3.0,
             qPrintable(s.setUtc.toString(Qt::ISODate)));

    // And the day really is short.
    const double hours = s.riseUtc.secsTo(s.setUtc) / 3600.0;
    QVERIFY2(hours > 8.0 && hours < 8.7,
             qPrintable(QStringLiteral("day length %1 h").arg(hours)));
}

void TstSolarTimes::equator_days_are_near_twelve_hours()
{
    // 0N 0E at the March equinox. NOAA: 06:04:14 / 18:10:54 UTC. Slightly
    // over twelve hours because the disc counts as risen while its centre
    // is still below the horizon.
    const SolarInfo s = solarInfo(utc(2026, 3, 20), 0.0, 0.0);
    QVERIFY(driftMin(s.riseUtc, 6, 4) < 3.0);
    QVERIFY(driftMin(s.setUtc, 18, 11) < 3.0);
    const double hours = s.riseUtc.secsTo(s.setUtc) / 3600.0;
    QVERIFY2(hours > 12.0 && hours < 12.2,
             qPrintable(QStringLiteral("%1 h").arg(hours)));
}

void TstSolarTimes::southern_hemisphere_is_not_mirrored_wrongly()
{
    // Sydney in August: short winter day, and both events land on the
    // UTC day either side of local noon. A latitude sign error would
    // give a long day here while still passing every Vienna test.
    const SolarInfo s = solarInfo(utc(2026, 8, 7, 3, 0), -33.87, 151.21);
    QVERIFY(!s.alwaysUp && !s.alwaysDown);
    const double hours = s.riseUtc.secsTo(s.setUtc) / 3600.0;
    QVERIFY2(hours > 10.3 && hours < 11.0,
             qPrintable(QStringLiteral("day length %1 h").arg(hours)));
}

void TstSolarTimes::polar_day_and_night_report_no_events()
{
    // Longyearbyen. There is no sunrise to report in either case, and a
    // fabricated time would be worse than a blank — so the times must
    // stay invalid, not merely be ignored by the caller.
    const SolarInfo june = solarInfo(utc(2026, 6, 21), 78.22, 15.65);
    QVERIFY(june.alwaysUp);
    QVERIFY(!june.alwaysDown);
    QVERIFY(!june.riseUtc.isValid());
    QVERIFY(!june.setUtc.isValid());
    QVERIFY(june.daylight());

    const SolarInfo dec = solarInfo(utc(2026, 12, 21), 78.22, 15.65);
    QVERIFY(dec.alwaysDown);
    QVERIFY(!dec.alwaysUp);
    QVERIFY(!dec.riseUtc.isValid());
    QVERIFY(!dec.daylight());
}

void TstSolarTimes::elevation_at_sunrise_is_the_refraction_offset()
{
    // Self-consistency: feeding the computed sunrise back in must put
    // the sun at -0.833 degrees, the threshold the rise was solved for.
    // This ties the two halves of the model together — they could
    // otherwise drift apart without any single test failing.
    for (double lat : {48.21, -33.87, 0.0, 60.0}) {
        const SolarInfo s = solarInfo(utc(2026, 8, 7), lat, 16.37);
        QVERIFY(s.riseUtc.isValid());
        const SolarInfo at = solarInfo(s.riseUtc, lat, 16.37);
        QVERIFY2(std::abs(at.elevationDeg + 0.833) < 0.15,
                 qPrintable(QStringLiteral("lat %1: %2 deg")
                                .arg(lat).arg(at.elevationDeg)));
    }
}

void TstSolarTimes::elevation_is_negative_at_local_midnight()
{
    // Vienna at 00:00 UTC in June: still deep night locally.
    const SolarInfo s = solarInfo(utc(2026, 6, 21, 0, 0), 48.21, 16.37);
    QVERIFY2(s.elevationDeg < -10.0,
             qPrintable(QStringLiteral("%1 deg").arg(s.elevationDeg)));
    QVERIFY(!s.daylight());

    // And high at local noon (about 11:00 UTC for this longitude).
    const SolarInfo noon = solarInfo(utc(2026, 6, 21, 11, 0), 48.21, 16.37);
    QVERIFY2(noon.elevationDeg > 60.0,
             qPrintable(QStringLiteral("%1 deg").arg(noon.elevationDeg)));
    QVERIFY(noon.daylight());
}

void TstSolarTimes::greyline_brackets_the_horizon()
{
    const SolarInfo s = solarInfo(utc(2026, 8, 7), 48.21, 16.37);

    // At sunrise itself: in the grey line.
    QVERIFY(solarInfo(s.riseUtc, 48.21, 16.37).greyline);

    // An hour after sunrise in August the sun is already above six
    // degrees at this latitude, so the band has closed.
    QVERIFY(!solarInfo(s.riseUtc.addSecs(3600), 48.21, 16.37).greyline);

    // And at local noon, plainly not.
    QVERIFY(!solarInfo(utc(2026, 8, 7, 11, 0), 48.21, 16.37).greyline);
}

void TstSolarTimes::invalid_input_is_survivable()
{
    // A caller with no locator yet passes a default-constructed time;
    // that must return blanks rather than a NaN that spreads.
    const SolarInfo s = solarInfo(QDateTime{}, 48.21, 16.37);
    QVERIFY(!s.riseUtc.isValid());
    QVERIFY(!std::isnan(s.elevationDeg));
}

QTEST_APPLESS_MAIN(TstSolarTimes)
// subsolarPoint() und solarInfo() sind ein Modell (2026-11-10). Der
// Winkel zum Subsolarpunkt muss dieselbe Hoehe ergeben, die solarInfo()
// meldet — sonst zeichnet der Globus die Grauzone woanders hin, als das
// Logbuch sie beurteilt (core/Grauzone.h). Uebrig bleibt nur, dass
// solarInfo() die Deklination des naechsten Mittags nimmt: hoechstens
// 0,2° am Horizont. Die alte Globus-Rechnung lag dort bis 4,3° daneben.
void TstSolarTimes::subsolar_point_agrees_with_elevation()
{
    struct Ort { double lat; double lon; };
    const Ort orte[] = {{47.5, 13.0}, {40.7, -74.0}, {-33.9, 151.2},
                        {64.0, -21.0}, {0.0, 0.0}, {70.0, 25.0}, {-60.0, -60.0}};
    constexpr double d = M_PI / 180.0;
    double schlimmste = 0.0;
    for (int tag = 0; tag < 366; tag += 5) {
        for (int stunde = 0; stunde < 24; stunde += 1) {
            const QDateTime t = utc(2026, 1, 1, 0, 17).addDays(tag).addSecs(stunde * 3600);
            double sLat = 0.0, sLon = 0.0;
            subsolarPoint(t, sLat, sLon);
            QVERIFY(sLon >= -180.0 && sLon < 180.0);
            for (const Ort& o : orte) {
                const double h = solarInfo(t, o.lat, o.lon).elevationDeg;
                if (std::abs(h) > 20.0) { continue; }    // nur nahe dem Horizont zaehlt
                const double ausPunkt = std::asin(
                    std::sin(o.lat * d) * std::sin(sLat * d)
                    + std::cos(o.lat * d) * std::cos(sLat * d) * std::cos((o.lon - sLon) * d)) / d;
                schlimmste = std::max(schlimmste, std::abs(h - ausPunkt));
            }
        }
    }
    QVERIFY2(schlimmste < 0.3, qPrintable(QStringLiteral("%1 Grad").arg(schlimmste)));

    // Ohne Zeit: Nullen, kein NaN.
    double lat = 1.0, lon = 1.0;
    subsolarPoint(QDateTime{}, lat, lon);
    QCOMPARE(lat, 0.0);
    QCOMPARE(lon, 0.0);
}

#include "tst_solar_times.moc"
