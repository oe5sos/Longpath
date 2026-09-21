// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_gibs_tile_geometry.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Das GIBS-Raster in EPSG:4326, ohne Netz: welche Stufe zu welcher
// Bildschirmaufloesung gehoert, welche Kacheln einen Ausschnitt decken,
// wo eine Kachel in Grad liegt, und wie ihre Adresse lautet. Die Zahlen
// kommen aus dem WMTS-Capabilities-Dokument (TileMatrixSets „500m" und
// „31.25m": 512 px, Ursprung -180/90, Stufe 0 = 2 x 1 Kacheln zu 288°,
// Stufe 7 = 160 x 80 zu 2,25°, Stufe 11 = 2560 x 1280 zu 0,140625°).
// Eine Kachel, die daneben liegt, zeigt die Nachbarstadt — und das
// faellt auf einem Luftbild nicht sofort auf.

#include <QtTest>

#include "gui/widgets/GibsTileLayer.h"

using namespace Longpath;
using TileId = GibsTileLayer::TileId;

class TstGibsTileGeometry : public QObject { Q_OBJECT
private slots:
    void the_span_halves_per_level()
    {
        QCOMPARE(GibsTileLayer::tileSpanDeg(0), 288.0);
        QCOMPARE(GibsTileLayer::tileSpanDeg(1), 144.0);
        QCOMPARE(GibsTileLayer::tileSpanDeg(7), 2.25);
        QCOMPARE(GibsTileLayer::tileSpanDeg(11), 0.140625);
        QCOMPARE(GibsTileLayer::degPerPixel(0), 0.5625);
        QVERIFY(qFuzzyCompare(GibsTileLayer::degPerPixel(11), 0.5625 / 2048.0));
    }

    void the_level_never_blows_a_tile_up()
    {
        // Bei 0,4 Grad je Bildpunkt (Weltkarte in 900 px) reicht Stufe 1
        // (0,28125): feiner als der Bildschirm, aber nicht mehr als noetig.
        QCOMPARE(GibsTileLayer::levelForDegPerPixel(0.4), 1);
        // Genau die Aufloesung einer Stufe -> diese Stufe.
        QCOMPARE(GibsTileLayer::levelForDegPerPixel(GibsTileLayer::degPerPixel(5)), 5);
        // Groeber als Stufe 0 -> 0, feiner als Landsat -> 11.
        QCOMPARE(GibsTileLayer::levelForDegPerPixel(2.0), 0);
        QCOMPARE(GibsTileLayer::levelForDegPerPixel(1e-5), GibsTileLayer::kLandsatMax);
        QCOMPARE(GibsTileLayer::levelForDegPerPixel(0.0), 0);
        // Die Stufen darueber tragen Landsat.
        QVERIFY(!GibsTileLayer::usesLandsat(7));
        QVERIFY(GibsTileLayer::usesLandsat(8));
    }

    void austria_on_level_7_is_eight_tiles()
    {
        // 9..17 Ost, 46..49,5 Nord: Spalten 84..87, Zeilen 18..19.
        const auto tiles = GibsTileLayer::tilesFor(7, 9.0, 17.0, 46.0, 49.5);
        QCOMPARE(tiles.size(), 8);
        QCOMPARE(tiles.first().col, 84);
        QCOMPARE(tiles.first().row, 18);
        QCOMPARE(tiles.last().col, 87);
        QCOMPARE(tiles.last().row, 19);
        for (const TileId& t : tiles) {
            QCOMPARE(t.level, 7);
            QVERIFY(!t.landsat);
        }
    }

    void a_tile_knows_where_it_lies()
    {
        // Spalte 84, Zeile 18 auf Stufe 7: 9..11,25 Ost, 49,5..47,25 Nord.
        const QRectF b = GibsTileLayer::tileBoundsDeg(TileId{7, 84, 18, false});
        QCOMPARE(b.left(), 9.0);
        QCOMPARE(b.top(), 49.5);
        QCOMPARE(b.width(), 2.25);
        QCOMPARE(b.height(), 2.25);
        // Und die Kachel, die Linz (48,3 N / 14,3 O) enthaelt, ist die
        // mit Spalte floor((14,3+180)/2,25) = 86, Zeile floor((90-48,3)/2,25) = 18.
        const auto one = GibsTileLayer::tilesFor(7, 14.3, 14.3 + 1e-6, 48.3 - 1e-6, 48.3);
        QCOMPARE(one.size(), 1);
        QCOMPARE(one.first().col, 86);
        QCOMPARE(one.first().row, 18);
    }

    void an_edge_on_a_tile_border_does_not_fetch_the_neighbour()
    {
        // Rechte Kante genau auf 11,25 (= Kante von Spalte 84|85): nur 84.
        const auto tiles = GibsTileLayer::tilesFor(7, 9.0, 11.25, 47.25, 49.5);
        QCOMPARE(tiles.size(), 1);
        QCOMPARE(tiles.first().col, 84);
        QCOMPARE(tiles.first().row, 18);
    }

    void the_world_on_level_0_is_two_tiles_and_never_more()
    {
        const auto tiles = GibsTileLayer::tilesFor(0, -180.0, 180.0, -90.0, 90.0);
        QCOMPARE(tiles.size(), 2);
        // Ausserhalb der Karte wird begrenzt, nicht erfunden.
        const auto clamped = GibsTileLayer::tilesFor(0, -400.0, 400.0, -100.0, 100.0);
        QCOMPARE(clamped.size(), 2);
        // Ein leerer Ausschnitt liefert nichts.
        QVERIFY(GibsTileLayer::tilesFor(3, 10.0, 10.0, 40.0, 50.0).isEmpty());
    }

    void the_url_follows_the_wmts_template()
    {
        const QUrl bm = GibsTileLayer::urlFor(TileId{7, 86, 18, false});
        QCOMPARE(bm.toString(), QStringLiteral(
            "https://gibs.earthdata.nasa.gov/wmts/epsg4326/best/"
            "BlueMarble_ShadedRelief_Bathymetry/default/500m/7/18/86.jpeg"));
        const QUrl ls = GibsTileLayer::urlFor(TileId{11, 1381, 296, true});
        QCOMPARE(ls.toString(), QStringLiteral(
            "https://gibs.earthdata.nasa.gov/wmts/epsg4326/best/"
            "Landsat_WELD_CorrectedReflectance_TrueColor_Global_Annual/default/"
            "2000-12-01/31.25m/11/296/1381.jpeg"));
        // Zeile vor Spalte — vertauscht zeigt die Kachel eine andere Gegend.
        QVERIFY(bm.toString().endsWith(QStringLiteral("/18/86.jpeg")));
    }

    void the_memory_answers_without_the_network()
    {
        GibsTileLayer layer;
        layer.setNetworkEnabled(false);
        const TileId id{3, 4, 2, false};
        QVERIFY(layer.tile(id).isNull());
        QCOMPARE(layer.pendingForTest(), 0);   // Netz aus: nichts angestossen

        QImage img(GibsTileLayer::kTilePx, GibsTileLayer::kTilePx, QImage::Format_RGB32);
        img.fill(Qt::darkGreen);
        layer.putForTest(id, img);
        QCOMPARE(layer.tile(id).size(), img.size());
        QCOMPARE(layer.cached(id).pixelColor(1, 1), QColor(Qt::darkGreen));
        QVERIFY(layer.cached(TileId{3, 5, 2, false}).isNull());
        QVERIFY(!GibsTileLayer::attribution().isEmpty());
    }
};

QTEST_MAIN(TstGibsTileGeometry)
#include "tst_gibs_tile_geometry.moc"
