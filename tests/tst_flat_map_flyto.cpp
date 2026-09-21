// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_flat_map_flyto.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Der Hinflug der flachen Karte: am Ziel liegt der Ort in der Mitte und
// der Zoom stimmt; unterwegs geht der Zoom erst hinaus und dann hinein;
// ohne Luftbild endet der Zoom bei 12x, mit Luftbild erst bei der
// Kachelaufloesung; die Zielstation wird gezeichnet, ohne dass sie im
// Log stehen muss. Alles ohne Netz: die Kachelschicht bekommt nur
// hinterlegte Bilder.

#include <QtTest>

#include "gui/widgets/FlatMapWidget.h"
#include "gui/widgets/GibsTileLayer.h"

#include <QSignalSpy>

using namespace Longpath;

class TstFlatMapFlyTo : public QObject { Q_OBJECT
private slots:
    void a_jump_lands_on_the_target()
    {
        FlatMapWidget w;
        w.resize(900, 450);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        QSignalSpy done(&w, &FlatMapWidget::flightFinished);
        w.flyTo(48.3, 14.3, 8.0, /*durationMs*/ 0);
        QCOMPARE(done.count(), 1);
        QCOMPARE(w.zoom(), 8.0);
        double lat = 0.0, lon = 0.0;
        QVERIFY(w.viewCentre(lat, lon));
        QVERIFY2(std::abs(lat - 48.3) < 0.05, qPrintable(QString::number(lat)));
        QVERIFY2(std::abs(lon - 14.3) < 0.05, qPrintable(QString::number(lon)));
    }

    void the_flight_bows_outwards_before_it_lands()
    {
        // Von 8x nach 8x an einen anderen Ort: die Mitte des Flugs liegt
        // deutlich unter 8x, die Enden genau darauf.
        QCOMPARE(FlatMapWidget::flightZoomAt(0.0, 8.0, 8.0), 8.0);
        QVERIFY(std::abs(FlatMapWidget::flightZoomAt(1.0, 8.0, 8.0) - 8.0) < 1e-9);
        const double mid = FlatMapWidget::flightZoomAt(0.5, 8.0, 8.0);
        QVERIFY2(mid < 4.0, qPrintable(QString::number(mid)));
        // Nie unter 1x — die Karte kennt keinen Zoom darunter.
        QVERIFY(FlatMapWidget::flightZoomAt(0.5, 1.0, 1.0) >= 1.0);
        // Von 1x nach 500x: monoton ab der Mitte, am Ende 500.
        const double a = FlatMapWidget::flightZoomAt(0.6, 1.0, 500.0);
        const double b = FlatMapWidget::flightZoomAt(0.9, 1.0, 500.0);
        QVERIFY(b > a);
        QVERIFY(std::abs(FlatMapWidget::flightZoomAt(1.0, 1.0, 500.0) - 500.0) < 1e-6);
    }

    void an_animated_flight_finishes_where_a_jump_does()
    {
        FlatMapWidget w;
        w.resize(900, 450);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        QSignalSpy done(&w, &FlatMapWidget::flightFinished);
        w.flyTo(-33.9, 151.2, 6.0, 120);
        QVERIFY(w.isFlying());
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 5000);
        QVERIFY(!w.isFlying());
        QCOMPARE(w.zoom(), 6.0);
        double lat = 0.0, lon = 0.0;
        QVERIFY(w.viewCentre(lat, lon));
        QVERIFY(std::abs(lat + 33.9) < 0.05);
        QVERIFY(std::abs(lon - 151.2) < 0.05);
    }

    void the_zoom_ceiling_follows_the_imagery()
    {
        FlatMapWidget w;
        w.resize(900, 450);
        QCOMPARE(w.maxZoom(), 12.0);

        GibsTileLayer tiles;
        tiles.setNetworkEnabled(false);
        w.setImagery(&tiles);
        QCOMPARE(w.maxZoom(), 12.0);          // noch nicht eingeschaltet
        w.setShowImagery(true);
        // 900 px fuer 360 Grad sind 0,4 Grad je Punkt; Landsat hat
        // 0,5625/2048 — also ueber 1400-fach.
        QVERIFY2(w.maxZoom() > 1000.0, qPrintable(QString::number(w.maxZoom())));
        w.flyTo(48.3, 14.3, 5000.0, 0);
        QVERIFY(w.zoom() <= w.maxZoom());
        QVERIFY(w.zoom() > 1000.0);
        // Luftbild aus: zurueck auf den alten Anschlag, der Ort bleibt.
        w.setShowImagery(false);
        QCOMPARE(w.zoom(), 12.0);
        double lat = 0.0, lon = 0.0;
        QVERIFY(w.viewCentre(lat, lon));
        QVERIFY(std::abs(lat - 48.3) < 0.2);
        QVERIFY(std::abs(lon - 14.3) < 0.2);
    }

    void the_focus_station_is_drawn_with_a_path_from_home()
    {
        FlatMapWidget w;
        w.resize(900, 450);
        w.setHome(48.3, 14.3);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        // Ohne Zielstation: wo spaeter der Ring sitzt, ist Hintergrund.
        w.flyTo(40.0, -74.0, 1.0, 0);   // Weltansicht
        const QImage before = w.grab().toImage();

        w.setFocusStation(QStringLiteral("w2fm"), 40.0, -74.0);
        QCOMPARE(w.focusStation(), QStringLiteral("W2FM"));
        const QImage after = w.grab().toImage();
        QCOMPARE(after.size(), before.size());

        // Irgendwo hat sich etwas veraendert — Ring, Rufzeichen, Grosskreis.
        int changed = 0;
        for (int y = 0; y < after.height(); y += 2) {
            for (int x = 0; x < after.width(); x += 2) {
                if (after.pixel(x, y) != before.pixel(x, y)) { ++changed; }
            }
        }
        QVERIFY2(changed > 40, qPrintable(QString::number(changed)));

        w.clearFocusStation();
        QVERIFY(w.focusStation().isEmpty());
    }

    void imagery_from_the_memory_is_painted_once_zoomed_in()
    {
        FlatMapWidget w;
        w.resize(900, 450);
        GibsTileLayer tiles;
        tiles.setNetworkEnabled(false);
        w.setImagery(&tiles);
        w.setShowImagery(true);
        w.setShowTerminator(false);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        // Bei 64x auf Linz braucht die Karte Stufe 8; eine knallrote
        // Kachel dort muss im Bild auftauchen.
        w.flyTo(48.3, 14.3, 64.0, 0);
        const int level = GibsTileLayer::levelForDegPerPixel(w.degPerPixel());
        QVERIFY2(level >= 6 && level <= 9, qPrintable(QString::number(level)));
        const auto ids = GibsTileLayer::tilesFor(level, 14.3, 14.3 + 1e-6, 48.3 - 1e-6, 48.3);
        QCOMPARE(ids.size(), 1);
        QImage red(GibsTileLayer::kTilePx, GibsTileLayer::kTilePx, QImage::Format_RGB32);
        red.fill(QColor(255, 0, 0));
        tiles.putForTest(ids.first(), red);

        w.repaint();
        const QImage img = w.grab().toImage();
        const QColor c = img.pixelColor(img.width() / 2, img.height() / 2);
        QVERIFY2(c.red() > 200 && c.green() < 60 && c.blue() < 60,
                 qPrintable(c.name()));
        QVERIFY(w.imageryPainted());
    }
};

QTEST_MAIN(TstFlatMapFlyTo)
#include "tst_flat_map_flyto.moc"
