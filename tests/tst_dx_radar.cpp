// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_dx_radar.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Der DX-Radar: Wurzelskala der Ringe, Norden oben / Osten rechts,
// Punkte an der richtigen Stelle, Klick meldet den Kontakt; im
// Kartenfenster als dritte Ansicht hinter dem Knopf „Radar", und der
// Hinflug fuehrt wieder auf die flache Karte.

#include <QtTest>

#include "gui/QsoMapWindow.h"
#include "gui/widgets/DxRadarWidget.h"
#include "gui/widgets/GibsTileLayer.h"
#include "gui/widgets/MapPoint.h"

#include <QDir>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>

#include <cmath>

using namespace Longpath;

class TstDxRadar : public QObject { Q_OBJECT
private slots:
    void theScaleIsASquareRoot()
    {
        QCOMPARE(DxRadarWidget::radiusFor(0.0, 20000.0), 0.0);
        QCOMPARE(DxRadarWidget::radiusFor(20000.0, 20000.0), 1.0);
        QVERIFY(std::abs(DxRadarWidget::radiusFor(5000.0, 20000.0) - 0.5) < 1e-9);
        QVERIFY(std::abs(DxRadarWidget::radiusFor(500.0, 20000.0) - std::sqrt(0.025)) < 1e-9);
        QCOMPARE(DxRadarWidget::radiusFor(40000.0, 20000.0), 1.0);   // gedeckelt
    }

    void northIsUpAndEastIsRight()
    {
        DxRadarWidget r;
        r.resize(400, 400);
        r.show();
        QVERIFY(QTest::qWaitForWindowExposed(&r));
        const QPointF c(200.0, 200.0);
        const QPointF n = r.pointAt(0.0, 20000.0);
        const QPointF e = r.pointAt(90.0, 20000.0);
        const QPointF s = r.pointAt(180.0, 5000.0);
        QVERIFY(std::abs(n.x() - c.x()) < 0.01 && n.y() < c.y());
        QVERIFY(std::abs(e.y() - c.y()) < 0.01 && e.x() > c.x());
        QVERIFY(std::abs(s.x() - c.x()) < 0.01 && s.y() > c.y());
        // 5 000 km liegen bei der Haelfte des Radius.
        QVERIFY(std::abs((s.y() - c.y()) / (c.y() - n.y()) - 0.5) < 0.01);
    }

    void pointsLandWhereTheyBelongAndClickReports()
    {
        DxRadarWidget r;
        r.resize(500, 500);
        r.setHome(47.8, 13.7);     // JN67
        QVector<MapPoint> pts;
        pts << MapPoint{40.7, -74.0, QStringLiteral("W2XYZ"), false, false}   // New York: ~300°, ~6 500 km
            << MapPoint{35.7, 139.7, QStringLiteral("JA1ABC"), true, false}   // Tokio: ~40°, ~9 200 km
            << MapPoint{48.2, 16.4, QStringLiteral("OE1AA"), false, true};    // Wien: ~78°, ~200 km
        r.setPoints(pts);
        r.show();
        QVERIFY(QTest::qWaitForWindowExposed(&r));
        r.grab();
        QCOMPARE(r.pointsPainted(), 3);

        // New York liegt links oben (Nordwesten), Tokio rechts oben.
        const QPointF ny = r.pointAt(302.0, 6500.0);
        const QPointF c(250.0, 250.0);
        QVERIFY(ny.x() < c.x() && ny.y() < c.y());
        const QPointF tk = r.pointAt(40.0, 9200.0);
        QVERIFY(tk.x() > c.x() && tk.y() < c.y());

        QSignalSpy clicked(&r, &DxRadarWidget::pointClicked);
        QTest::mouseClick(&r, Qt::LeftButton, Qt::NoModifier, ny.toPoint());
        QCOMPARE(clicked.count(), 1);
        QCOMPARE(clicked.first().at(0).toString(), QStringLiteral("W2XYZ"));
    }

    void theMapWindowHasTheRadarAsThirdView()
    {
        QsoMapWindow w;
        w.resize(1100, 620);
        w.setHomeGrid(QStringLiteral("JN67UT"));
        w.imagery()->setNetworkEnabled(false);
        QVector<LogEntry> log;
        LogEntry e; e.call = QStringLiteral("W2XYZ"); e.gridSquare = QStringLiteral("FN30");
        e.timeOn = QDateTime::currentDateTimeUtc(); e.band = QStringLiteral("20m"); e.mode = QStringLiteral("SSB");
        log << e;
        w.setEntries(log);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        auto* stack = w.findChild<QStackedWidget*>();
        QVERIFY(stack);
        QCOMPARE(stack->currentIndex(), 0);
        QPushButton* radar = w.radarButtonForTest();
        QVERIFY(radar);
        radar->setChecked(true);
        QCOMPARE(stack->currentIndex(), 2);
        w.radarForTest()->grab();
        QCOMPARE(w.radarForTest()->pointsPainted(), 1);

        // Zurueck: der Haken fuehrt dorthin, wo man herkam.
        radar->setChecked(false);
        QCOMPARE(stack->currentIndex(), 0);

        // Der Hinflug verlaesst den Radar Richtung flache Karte.
        radar->setChecked(true);
        w.flyToStation(QStringLiteral("W2XYZ"), 40.7, -74.0);
        QCOMPARE(stack->currentIndex(), 1);
        QVERIFY(!radar->isChecked());

        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!grabDir.isEmpty()) {
            radar->setChecked(true);
            QTest::qWait(200);
            QDir().mkpath(grabDir);
            w.grab().save(grabDir + QStringLiteral("/dx-radar.png"));
        }
    }
};

QTEST_MAIN(TstDxRadar)
#include "tst_dx_radar.moc"
