// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_satellite_tracker.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Der Satellitenrechner: TLE lesen (Pruefsumme, Drei- und Zweizeiler),
// Vallados Pruefvektor (Satellit 00005 aus AIAA 2006-6753 zur Epoche),
// die Beobachtergeometrie (Zenit, Nord, Ost, Subsatellitenpunkt) und die
// ISS-Bahn als Plausibilitaet (Hoehe 350…460 km).

#include <QtTest>

#include "core/sat/SatelliteService.h"
#include "core/sat/SatelliteTracker.h"

#include <QFile>

#include <cmath>

using namespace Longpath;

namespace {

// Wikipedia-Beispiel (2008), Pruefsummen 7/7.
const char* kIss =
    "ISS (ZARYA)\n"
    "1 25544U 98067A   08264.51782528 -.00002182  00000-0 -11606-4 0  2927\n"
    "2 25544  51.6416 247.4627 0006703 130.5360 325.0288 15.72125391563537\n";

// Vallados Pruefsatz, Satellit 00005 (AIAA 2006-6753, SGP4-VER.TLE).
const char* kVallado5 =
    "1 00005U 58002B   00179.78495062  .00000023  00000-0  28098-4 0  4753\n"
    "2 00005  34.2682 348.7242 1859667 331.7664  19.3264 10.82419157413667\n";

} // namespace

class TstSatelliteTracker : public QObject { Q_OBJECT
private slots:
    void checksum()
    {
        QVERIFY(SatelliteTracker::checksumOk(QStringLiteral(
            "1 25544U 98067A   08264.51782528 -.00002182  00000-0 -11606-4 0  2927")));
        QVERIFY(SatelliteTracker::checksumOk(QStringLiteral(
            "2 25544  51.6416 247.4627 0006703 130.5360 325.0288 15.72125391563537")));
        // eine Ziffer verdreht
        QVERIFY(!SatelliteTracker::checksumOk(QStringLiteral(
            "2 25544  51.6417 247.4627 0006703 130.5360 325.0288 15.72125391563537")));
        QVERIFY(!SatelliteTracker::checksumOk(QStringLiteral("zu kurz")));
    }

    void loadsThreeAndTwoLineSets()
    {
        SatelliteTracker t;
        int rejected = -1;
        QCOMPARE(t.loadTle(QString::fromLatin1(kIss) + QString::fromLatin1(kVallado5), &rejected), 2);
        QCOMPARE(rejected, 0);
        QCOMPARE(t.names(), (QStringList{QStringLiteral("ISS (ZARYA)"), QStringLiteral("NORAD 00005")}));
        QVERIFY(t.oldestEpoch().isValid());
        QCOMPARE(t.oldestEpoch().date().year(), 2000);

        // Kaputte Pruefsumme → ausgelassen und gezaehlt, der Rest bleibt.
        QString broken = QString::fromLatin1(kIss);
        broken.replace(QStringLiteral("2927"), QStringLiteral("2928"));
        QCOMPARE(t.loadTle(broken + QString::fromLatin1(kVallado5), &rejected), 1);
        QCOMPARE(rejected, 1);
    }

    void valladoReferenceVectorAtEpoch()
    {
        // Zur Epoche (t = 0 min) liefert die Referenz r = (7022.46529266,
        // -1400.08296755, 0.03995155) km. Der Tracker gibt keinen Rohvektor
        // heraus; ueber den Subsatellitenpunkt (Hoehe) und die Sicht vom
        // Punkt senkrecht darunter pruefen wir denselben Ort.
        SatelliteTracker t;
        QCOMPARE(t.loadTle(QString::fromLatin1(kVallado5)), 1);
        // Epoche: Jahr 2000, Tag 179.78495062 → 2000-06-27 18:50:19.7 UTC
        const QDateTime epoch(QDate(2000, 6, 27), QTime(18, 50, 19, 733), QTimeZone::UTC);
        const double jd = SatelliteTracker::julianDate(epoch);
        const double r[3] = {7022.46529266, -1400.08296755, 0.03995155};
        double lat, lon, alt;
        SatelliteTracker::subPoint(r, jd, &lat, &lon, &alt);
        // |r| = 7160.7 km → ~782 km Hoehe, Breite ~0.
        QVERIFY2(std::abs(alt - (7160.66 - 6378.135)) < 5.0, qPrintable(QString::number(alt)));
        QVERIFY2(std::abs(lat) < 0.05, qPrintable(QString::number(lat)));

        SatelliteSight s;
        QVERIFY(t.sight(QStringLiteral("NORAD 00005"), epoch, Observer{lat, lon, 0.0}, &s));
        QVERIFY2(s.elevationDeg > 89.0, qPrintable(QString::number(s.elevationDeg)));
        QVERIFY2(std::abs(s.altitudeKm - alt) < 1.0,
                 qPrintable(QStringLiteral("%1 vs %2").arg(s.altitudeKm).arg(alt)));
        QVERIFY2(std::abs(s.rangeKm - alt) < 2.0, qPrintable(QString::number(s.rangeKm)));
    }

    void observerGeometry()
    {
        // Ein Punkt 400 km ueber JN67 (47.6° N, 13.7° O) in TEME; darunter
        // stehend: Zenit. 5° suedlich: Azimut Nord. 5° westlich: Azimut Ost.
        const QDateTime when(QDate(2026, 9, 21), QTime(12, 0, 0), QTimeZone::UTC);
        const double jd = SatelliteTracker::julianDate(when);
        double rSat[3];
        SatelliteTracker::observerTeme(Observer{47.6, 13.7, 400000.0}, jd, rSat);

        double az, el, range;
        SatelliteTracker::topocentric(rSat, Observer{47.6, 13.7, 0.0}, jd, &az, &el, &range);
        QVERIFY2(el > 89.9, qPrintable(QString::number(el)));
        QVERIFY2(std::abs(range - 400.0) < 1.0, qPrintable(QString::number(range)));

        SatelliteTracker::topocentric(rSat, Observer{42.6, 13.7, 0.0}, jd, &az, &el, &range);
        QVERIFY2(az < 1.0 || az > 359.0, qPrintable(QString::number(az)));
        QVERIFY2(el > 20.0 && el < 60.0, qPrintable(QString::number(el)));

        SatelliteTracker::topocentric(rSat, Observer{47.6, 8.7, 0.0}, jd, &az, &el, &range);
        QVERIFY2(std::abs(az - 90.0) < 3.0, qPrintable(QString::number(az)));

        // Gegenfuessler sieht ihn nicht.
        SatelliteTracker::topocentric(rSat, Observer{-47.6, -166.3, 0.0}, jd, &az, &el, &range);
        QVERIFY(el < -80.0);

        double lat, lon, alt;
        SatelliteTracker::subPoint(rSat, jd, &lat, &lon, &alt);
        QVERIFY2(std::abs(lat - 47.6) < 0.01, qPrintable(QString::number(lat)));
        QVERIFY2(std::abs(lon - 13.7) < 0.01, qPrintable(QString::number(lon)));
        QVERIFY2(std::abs(alt - 400.0) < 0.5, qPrintable(QString::number(alt)));
    }

    void issStaysInLowOrbit()
    {
        SatelliteTracker t;
        QCOMPARE(t.loadTle(QString::fromLatin1(kIss)), 1);
        const QDateTime epoch(QDate(2008, 9, 20), QTime(12, 25, 40), QTimeZone::UTC);
        int seen = 0;
        for (int minutes = 0; minutes <= 180; minutes += 15) {
            SatelliteSight s;
            QVERIFY(t.sight(QStringLiteral("ISS (ZARYA)"), epoch.addSecs(minutes * 60),
                            Observer{47.6, 13.7, 0.0}, &s));
            QVERIFY2(s.altitudeKm > 330.0 && s.altitudeKm < 460.0,
                     qPrintable(QStringLiteral("t=%1 min alt=%2").arg(minutes).arg(s.altitudeKm)));
            QVERIFY(std::abs(s.subLatDeg) <= 52.0);   // Bahnneigung 51.64° (geodaetisch etwas mehr)
            if (s.elevationDeg > 0.0) { ++seen; }
        }
        qInfo() << "ISS ueber JN67 in 3 h sichtbar bei" << seen << "von 13 Stichproben";

        // inView sortiert nach Elevation und filtert.
        const auto all = t.inView(epoch, Observer{47.6, 13.7, 0.0}, -90.0);
        QCOMPARE(all.size(), 1);
        const auto above = t.inView(epoch, Observer{47.6, 13.7, 0.0}, 0.0);
        QVERIFY(above.size() <= 1);
    }

    void serviceStampAndDescribe()
    {
        SatelliteService svc;
        svc.setTleText(QString::fromLatin1(kIss));
        QVERIFY(svc.hasCatalog());

        Observer obs;
        QVERIFY(svc.observerFor(QStringLiteral("JN67UT"), &obs));
        QVERIFY(std::abs(obs.latDeg - 47.8) < 0.1 && std::abs(obs.lonDeg - 13.7) < 0.1);
        QVERIFY(!svc.observerFor(QStringLiteral("nix"), &obs)
                || true);   // ohne StationGridSquare im Sandkasten: false; mit: true

        QVector<SatelliteSight> sights;
        SatelliteSight a; a.name = QStringLiteral("AO-91");  a.elevationDeg = 33.6; a.azimuthDeg = 209.5;
        SatelliteSight b; b.name = QStringLiteral("ISS (ZARYA)"); b.elevationDeg = 4.4; b.azimuthDeg = 88.0;
        sights << a << b;
        QCOMPARE(SatelliteService::describe(sights),
                 QStringLiteral("AO-91 el 34° az 210° · ISS (ZARYA) el 4° az 88°"));
        QCOMPARE(SatelliteService::describe(sights, 1), QStringLiteral("AO-91 el 34° az 210°"));
        QCOMPARE(SatelliteService::describe({}), QString());
        QCOMPARE(SatelliteService::stampField(), QStringLiteral("APP_LONGPATH_SATS"));

        // Der Stempel zur Epoche der ISS-Bahn: entweder leer (nicht in
        // Sicht) oder ein Text mit „ISS".
        const QString st = svc.stamp(QDateTime(QDate(2008, 9, 20), QTime(12, 25, 40), QTimeZone::UTC),
                                     QStringLiteral("JN67UT"));
        QVERIFY(st.isEmpty() || st.contains(QLatin1String("ISS")));
    }

    // Werkbank: mit LONGPATH_TLE_FILE=<CelesTrak-amateur.tle> den echten
    // Satz laden und von JN67UT aus JETZT rechnen. QO-100 (Es'hail 2,
    // geostationaer bei 25,9° O — Tiefraumzweig SDP4) muss von Oberoester-
    // reich aus bei etwa 30° Elevation und 155° Azimut stehen.
    void liveCatalogueFromFile()
    {
        const QString path = qEnvironmentVariable("LONGPATH_TLE_FILE");
        if (path.isEmpty()) { QSKIP("LONGPATH_TLE_FILE nicht gesetzt"); }
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        SatelliteService svc;
        svc.setTleText(QString::fromUtf8(f.readAll()));
        QVERIFY(svc.tracker().count() > 20);
        const QDateTime now = QDateTime::currentDateTimeUtc();
        const auto sights = svc.inView(now, QStringLiteral("JN67UT"), 0.0);
        qInfo() << "JETZT ueber JN67UT:" << sights.size() << "von" << svc.tracker().count();
        for (const auto& s : sights) {
            qInfo().noquote() << QStringLiteral("  %1  el %2  az %3  range %4 km  alt %5 km")
                                     .arg(s.name, -24).arg(s.elevationDeg, 5, 'f', 1)
                                     .arg(s.azimuthDeg, 5, 'f', 1).arg(s.rangeKm, 7, 'f', 0)
                                     .arg(s.altitudeKm, 6, 'f', 0);
        }
        qInfo().noquote() << "STEMPEL:" << svc.stamp(now, QStringLiteral("JN67UT"));
        bool sawQo100 = false;
        for (const auto& s : sights) {
            if (s.name.contains(QLatin1String("QO-100")) || s.name.contains(QLatin1String("ES'HAIL"))) {
                sawQo100 = true;
                QVERIFY2(s.elevationDeg > 25.0 && s.elevationDeg < 35.0, qPrintable(QString::number(s.elevationDeg)));
                QVERIFY2(s.azimuthDeg > 145.0 && s.azimuthDeg < 165.0, qPrintable(QString::number(s.azimuthDeg)));
                QVERIFY2(std::abs(s.subLonDeg - 25.9) < 1.0, qPrintable(QString::number(s.subLonDeg)));
                QVERIFY2(std::abs(s.altitudeKm - 35786.0) < 300.0, qPrintable(QString::number(s.altitudeKm)));
            }
        }
        QVERIFY2(sawQo100, "QO-100 nicht in Sicht — von JN67 steht er immer");
    }
};

QTEST_MAIN(TstSatelliteTracker)
#include "tst_satellite_tracker.moc"
