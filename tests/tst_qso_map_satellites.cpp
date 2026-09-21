// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_qso_map_satellites.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Satelliten ueber dem Horizont auf der Karte. Der Dienst bekommt einen
// TLE-Text (QO-100, geostationaer bei 25,9° O — von Oesterreich aus
// immer in Sicht, egal wann der Pruefstand laeuft), das Kartenfenster
// zeichnet Dreiecke; der Haken „Satellites" schaltet sie ab. Beim
// Loggen im Panel bekommt der Eintrag den Stempel APP_LONGPATH_SATS;
// das Detailpaneel nennt das Feld „Satellites in view".
//
// Mit LONGPATH_GRAB_DIR wird das Kartenfenster als PNG abgelegt.

#include <QtTest>

#include "core/sat/SatelliteService.h"
#include "core/sat/SatelliteTracker.h"
#include "gui/QsoMapWindow.h"
#include "gui/widgets/FlatMapWidget.h"
#include "gui/widgets/GibsTileLayer.h"
#include "gui/widgets/GlobeWidget.h"
#include "gui/widgets/QsoDetailPane.h"
#include "gui/widgets/RotorLogbookPanel.h"
#include "models/LogEntry.h"

#include <QCheckBox>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include <cmath>

using namespace Longpath;

namespace {

// Ein geostationaerer Satellit, JETZT ueber `lonDeg` — als TLE mit Epoche
// von jetzt gebaut, damit der Pruefstand in Jahren noch dasselbe zeigt
// (ein echter QO-100-Satz driftet im Modell mit der Zeit davon).
// Kreisbahn in der Aequatorebene: Laenge zur Epoche = RAAN + ω + M − GMST.
QString checksummed(QString line)
{
    int sum = 0;
    for (const QChar c : line) {
        if (c.isDigit()) { sum += c.digitValue(); }
        else if (c == QLatin1Char('-')) { sum += 1; }
    }
    return line + QString::number(sum % 10);
}

QString syntheticGeoTle(double lonDeg, const QDateTime& now)
{
    const QDateTime utc = now.toUTC();
    const int yy = utc.date().year() % 100;
    const double doy = utc.date().dayOfYear()
                     + utc.time().msecsSinceStartOfDay() / 86400000.0;
    const double gmstDeg = SatelliteTracker::gmstRad(SatelliteTracker::julianDate(utc)) * 180.0 / M_PI;
    double m = std::fmod(lonDeg + gmstDeg, 360.0);
    if (m < 0.0) { m += 360.0; }
    const QString l1 = checksummed(
        QStringLiteral("1 99999U 26001A   %1%2  .00000000  00000-0  00000-0 0  999")
            .arg(yy, 2, 10, QLatin1Char('0'))
            .arg(doy, 12, 'f', 8, QLatin1Char('0')));
    const QString l2 = checksummed(
        QStringLiteral("2 99999   0.0000   0.0000 0000000   0.0000 %1  1.00273791    1")
            .arg(m, 8, 'f', 4, QLatin1Char(' ')));
    return QStringLiteral("GEO-TEST\n") + l1 + QLatin1Char('\n') + l2 + QLatin1Char('\n');
}

} // namespace

class TstQsoMapSatellites : public QObject { Q_OBJECT
private slots:
    void theMapDrawsWhatIsInView()
    {
        SatelliteService svc;
        svc.setTleText(syntheticGeoTle(25.9, QDateTime::currentDateTimeUtc()));
        QVERIFY2(svc.hasCatalog(), "synthetischer TLE nicht angenommen");
        // Der Kunstsatellit steht, wo QO-100 steht: von JN67UT aus ~34°
        // hoch im Suedsuedosten.
        const auto sights = svc.inView(QDateTime::currentDateTimeUtc(), QStringLiteral("JN67UT"));
        QCOMPARE(sights.size(), 1);
        qInfo() << "GEO-TEST von JN67UT: el" << sights.first().elevationDeg
                << "az" << sights.first().azimuthDeg << "lon" << sights.first().subLonDeg;
        QVERIFY2(std::abs(sights.first().subLonDeg - 25.9) < 0.5, qPrintable(QString::number(sights.first().subLonDeg)));
        QVERIFY2(sights.first().elevationDeg > 30.0 && sights.first().elevationDeg < 38.0,
                 qPrintable(QString::number(sights.first().elevationDeg)));
        QVERIFY2(sights.first().azimuthDeg > 155.0 && sights.first().azimuthDeg < 172.0,
                 qPrintable(QString::number(sights.first().azimuthDeg)));

        QsoMapWindow w;
        w.resize(1100, 620);
        w.setHomeGrid(QStringLiteral("JN67UT"));
        w.imagery()->setNetworkEnabled(false);
        w.setSatellites(&svc);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        FlatMapWidget* flat = w.flatMapForTest();
        QVERIFY(flat);
        GlobeWidget* globe = w.findChild<GlobeWidget*>();
        QVERIFY(globe);
        w.refreshSatellites();
        flat->grab();
        QTRY_COMPARE_WITH_TIMEOUT(flat->satellitesPainted(), 1, 3000);
        // Auf der Kugel ebenso — sie schaut auf HOME, QO-100 steht ueber
        // Afrika, gut im Blick.
        globe->grab();
        QTRY_COMPARE_WITH_TIMEOUT(globe->satellitesPainted(), 1, 3000);

        // Der Haken schaltet die Schicht ab …
        QCheckBox* box = nullptr;
        for (QCheckBox* c : w.findChildren<QCheckBox*>()) {
            if (c->text() == QLatin1String("Satellites")) { box = c; }
        }
        QVERIFY(box);
        box->setChecked(false);
        flat->grab();
        globe->grab();
        QCOMPARE(flat->satellitesPainted(), 0);
        QCOMPARE(globe->satellitesPainted(), 0);
        // … und wieder an.
        box->setChecked(true);
        flat->grab();
        QTRY_COMPARE_WITH_TIMEOUT(flat->satellitesPainted(), 1, 3000);

        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!grabDir.isEmpty()) {
            QDir().mkpath(grabDir);
            QTest::qWait(300);
            w.grab().save(grabDir + QStringLiteral("/qso-map-satellites-globe.png"));
            for (QPushButton* b : w.findChildren<QPushButton*>()) {
                if (b->text() == QLatin1String("Flat map")) { b->click(); break; }
            }
            QTest::qWait(300);
            w.grab().save(grabDir + QStringLiteral("/qso-map-satellites-flat.png"));
        }
    }

    // Das Logbuch-Panel stempelt beim Bauen des Eintrags: mit QO-100 im
    // Satz und JN67UT als eigenem Locator steht der Satellit im Feld.
    void thePanelStampsTheEntry()
    {
        SatelliteService svc;
        svc.setTleText(syntheticGeoTle(25.9, QDateTime::currentDateTimeUtc()));
        RotorLogbookPanel panel(nullptr, nullptr, nullptr);
        panel.setSatellites(&svc);
        QLineEdit* call = nullptr; QLineEdit* grid = nullptr;
        for (QLineEdit* e : panel.findChildren<QLineEdit*>()) {
            if (e->placeholderText() == QLatin1String("callsign"))     { call = e; }
            if (e->placeholderText() == QLatin1String("your locator")) { grid = e; }
        }
        QVERIFY(call && grid);
        call->setText(QStringLiteral("OE3AA"));
        grid->setText(QStringLiteral("JN67UT"));
        const LogEntry e = panel.buildEntryForTest();
        QString stamp;
        for (const auto& kv : e.extras) {
            if (kv.first == SatelliteService::stampField()) { stamp = kv.second; }
        }
        QVERIFY2(stamp.contains(QLatin1String("GEO-TEST")), qPrintable(stamp));

        // Ohne Dienst: kein Feld.
        panel.setSatellites(nullptr);
        const LogEntry plain = panel.buildEntryForTest();
        for (const auto& kv : plain.extras) {
            QVERIFY(kv.first != SatelliteService::stampField());
        }
    }

    void theDetailPaneNamesTheStamp()
    {
        QsoDetailPane pane;
        pane.resize(420, 600);
        pane.show();
        QVERIFY(QTest::qWaitForWindowExposed(&pane));
        LogEntry e;
        e.call = QStringLiteral("OE3AA");
        e.timeOn = QDateTime(QDate(2026, 9, 21), QTime(12, 0), QTimeZone::UTC);
        e.extras.append(qMakePair(SatelliteService::stampField(),
                                  QStringLiteral("ES'HAIL 2 el 34° az 164°")));
        pane.setEntry(e);
        QCoreApplication::processEvents();
        bool sawLabel = false, sawRaw = false;
        for (QLabel* l : pane.findChildren<QLabel*>()) {
            if (l->text() == QLatin1String("Satellites in view")) { sawLabel = true; }
            if (l->text() == QLatin1String("APP_LONGPATH_SATS")) { sawRaw = true; }
        }
        QVERIFY(sawLabel);
        QVERIFY(!sawRaw);
    }
};

QTEST_MAIN(TstQsoMapSatellites)
#include "tst_qso_map_satellites.moc"
