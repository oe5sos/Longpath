// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_blue_marble_fetcher.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Die Kugel setzt sich ihr Blue Marble selbst zusammen: 50 Kacheln der
// Stufe 3 in eine 5120 × 2560 grosse Plattkarte, Datei + Sidecar in den
// Kartenordner, als Weltbild eingetragen — ausser der Betreiber hat
// schon eines. Mit LONGPATH_NET=1 holt der letzte Pruefstand die echten
// Kacheln und legt (mit LONGPATH_GRAB_DIR) die Kugel als Bild ab.

#include <QtTest>

#include "core/AppSettings.h"
#include "gui/widgets/BlueMarbleFetcher.h"
#include "gui/widgets/GibsTileLayer.h"
#include "gui/widgets/GlobeWidget.h"
#include "gui/widgets/WorldTexture.h"

#include <QDir>
#include <QFile>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>

using namespace Longpath;

namespace {
QImage flatTile(const QColor& c)
{
    QImage img(GibsTileLayer::kTilePx, GibsTileLayer::kTilePx, QImage::Format_RGB32);
    img.fill(c);
    return img;
}
void fillAll(GibsTileLayer& tiles, int level)
{
    for (const GibsTileLayer::TileId& id : GibsTileLayer::tilesFor(level, -180, 180, -90, 90)) {
        // Eine Farbe je Kachel: Rot = Spalte, Gruen = Zeile.
        tiles.putForTest(id, flatTile(QColor(20 * id.col, 60 * id.row, 90)));
    }
}
} // namespace

class TstBlueMarbleFetcher : public QObject { Q_OBJECT
private slots:
    void init() { WorldTexture::clearPath(); }

    void composePlacesEveryTile()
    {
        GibsTileLayer tiles;
        tiles.setNetworkEnabled(false);
        const int level = BlueMarbleFetcher::kLevel;
        const auto ids = GibsTileLayer::tilesFor(level, -180, 180, -90, 90);
        QCOMPARE(ids.size(), 50);   // 10 × 5 bei 36°
        fillAll(tiles, level);

        bool complete = false;
        const QImage img = BlueMarbleFetcher::compose(tiles, level, &complete);
        QVERIFY(complete);
        QCOMPARE(img.size(), QSize(5120, 2560));
        // 0° O / 10° S liegt in Spalte 5 (0…36° O), Zeile 2 (18° N … 18° S).
        const QColor at = img.pixelColor(2560 + 10, 1280 + 10);
        QCOMPARE(at.red(), 20 * 5);
        QCOMPARE(at.green(), 60 * 2);
        // 170° W / 80° N: Spalte 0, Zeile 0.
        const QColor nw = img.pixelColor(100, 100);
        QCOMPARE(nw.red(), 0);
        QCOMPARE(nw.green(), 0);

        // Eine Kachel fehlt: unvollstaendig, aber ein Bild.
        GibsTileLayer partial;
        partial.setNetworkEnabled(false);
        for (int i = 1; i < ids.size(); ++i) {
            partial.putForTest(ids.at(i), flatTile(Qt::gray));
        }
        const QImage part = BlueMarbleFetcher::compose(partial, level, &complete);
        QVERIFY(!complete);
        QVERIFY(!part.isNull());
    }

    void startWritesFileSidecarAndRegisters()
    {
        QTemporaryDir dir;
        GibsTileLayer tiles;
        tiles.setNetworkEnabled(false);
        fillAll(tiles, BlueMarbleFetcher::kLevel);

        BlueMarbleFetcher f;
        f.setTileLayerForTest(&tiles);
        const QString path = dir.path() + QStringLiteral("/maps/") + BlueMarbleFetcher::fileName();
        f.setTargetPathForTest(path);
        QSignalSpy done(&f, &BlueMarbleFetcher::finished);
        f.start();
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 10000);
        QVERIFY(done.first().at(0).toBool());
        QCOMPARE(done.first().at(1).toString(), path);
        QVERIFY(QFile::exists(path));
        QVERIFY(QFile::exists(dir.path() + QStringLiteral("/maps/NASA Blue Marble (GIBS).json")));
        QCOMPARE(WorldTexture::currentPath(), path);
        QCOMPARE(WorldTexture::image().size(), QSize(5120, 2560));

        // Liegt die Datei schon, wird nur eingetragen — keine Kacheln noetig.
        WorldTexture::clearPath();
        BlueMarbleFetcher again;
        GibsTileLayer empty;
        empty.setNetworkEnabled(false);
        again.setTileLayerForTest(&empty);
        again.setTargetPathForTest(path);
        QSignalSpy done2(&again, &BlueMarbleFetcher::finished);
        again.start();
        QCOMPARE(done2.count(), 1);
        QVERIFY(done2.first().at(0).toBool());
        QCOMPARE(WorldTexture::currentPath(), path);
    }

    void theOperatorsOwnImageIsLeftAlone()
    {
        QTemporaryDir dir;
        const QString mine = dir.path() + QStringLiteral("/mine.png");
        QImage own(2048, 1024, QImage::Format_RGB32);
        own.fill(Qt::darkGreen);
        QVERIFY(own.save(mine));
        QVERIFY(WorldTexture::setPath(mine));

        GibsTileLayer empty;
        empty.setNetworkEnabled(false);
        BlueMarbleFetcher f;
        f.setTileLayerForTest(&empty);
        f.setTargetPathForTest(dir.path() + QStringLiteral("/maps/x.jpg"));
        QSignalSpy done(&f, &BlueMarbleFetcher::finished);
        f.start();
        QCOMPARE(done.count(), 1);
        QCOMPARE(done.first().at(1).toString(), mine);
        QVERIFY(!f.isRunning());
        QCOMPARE(WorldTexture::currentPath(), mine);
    }

    // Werkbank: echte Kacheln (Netz), dann die Kugel als Bild.
    void realTilesMakeAGlobe()
    {
        if (!qEnvironmentVariableIsSet("LONGPATH_NET")) { QSKIP("LONGPATH_NET nicht gesetzt"); }
        QTemporaryDir dir;
        BlueMarbleFetcher f;
        const QString path = dir.path() + QStringLiteral("/maps/") + BlueMarbleFetcher::fileName();
        f.setTargetPathForTest(path);
        QSignalSpy done(&f, &BlueMarbleFetcher::finished);
        f.start();
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 120000);
        QVERIFY2(done.first().at(0).toBool(), "Kacheln kamen nicht vollstaendig");
        qInfo() << "Weltbild" << path << QFileInfo(path).size() / 1024 << "kB";

        GlobeWidget g;
        g.resize(700, 700);
        g.setHome(47.8, 13.7);
        g.setTarget(40.7, -74.0);
        g.setShowAtmosphere(true);
        g.show();
        QVERIFY(QTest::qWaitForWindowExposed(&g));
        QTest::qWait(400);
        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!grabDir.isEmpty()) {
            QDir().mkpath(grabDir);
            g.grab().save(grabDir + QStringLiteral("/globe-blue-marble.png"));
            g.zoomBy(2.5);
            QTest::qWait(300);
            g.grab().save(grabDir + QStringLiteral("/globe-blue-marble-zoom.png"));
        }
    }
};

QTEST_MAIN(TstBlueMarbleFetcher)
#include "tst_blue_marble_fetcher.moc"
