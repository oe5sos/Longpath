// =================================================================
// tests/tst_applet_to_canvas.cpp  (NereusSDR)
// =================================================================
//
// Ein Applet verlaesst den Stapel und wird eine frei bewegliche
// Kachel.
//
// DAS IST DIE SACHE, DIE DER BETREIBER ZEHN MAL VERLANGT HAT:
// „man kann alles x beliebig verschieben! das geht bei uns nicht!"
// und „es muss alles auf den mm verschoben werden koennen. jedes
// window! jeder panel, ueberall" (2026-08-20).
//
// Sie hat so lange gefehlt, weil der Grund unter der Oberflaeche lag:
// die Applets sitzen in einem STAPEL, und ein Stapel hat keine
// Positionen, nur eine Reihenfolge. Kein Knopf aendert das.
//
// Geprueft wird deshalb genau das, was fehlte — nicht ob es huebsch
// aussieht, sondern ob das Applet HINTERHER EINE EIGENE LAGE HAT und
// ob es sie behaelt.
//
// Ohne Bildschirm und ohne Funkgeraet.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-20 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QSplitter>

#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/RxApplet.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestAppletToCanvas : public QObject
{
    Q_OBJECT

private slots:

    // Die Bausteine: ein Container im Zustand OverlayDocked ist das,
    // was eine freie Kachel ausmacht. Ohne ihn ist der Rest sinnlos.
    void anOverlayContainerHasItsOwnPlace()
    {
        QWidget host;
        host.resize(1200, 800);
        QSplitter splitter;
        ContainerManager mgr(&host, &splitter);

        ContainerWidget* tile =
            mgr.createContainer(1, DockMode::OverlayDocked);
        QVERIFY(tile);
        QVERIFY2(tile->isOverlayDocked(),
                 "eine Kachel muss ueberlagernd liegen — im Splitter "
                 "haette sie keine eigene Lage");

        tile->setDockedLocation(QPoint(120, 90));
        tile->setDockedSize(QSize(400, 300));
        tile->restoreLocation();

        QCOMPARE(tile->pos(), QPoint(120, 90));
        QVERIFY2(tile->parentWidget() == &host,
                 "sie muss ueber der Arbeitsflaeche haengen, nicht "
                 "irgendwo");
    }

    // AUF DEN MILLIMETER: zwei Lagen, die sich um einen Bildpunkt
    // unterscheiden, muessen beide ankommen. Ein Raster, das rundet,
    // waere genau das, was der Betreiber nicht will.
    void itGoesExactlyWhereItIsPut()
    {
        QWidget host;
        host.resize(1200, 800);
        QSplitter splitter;
        ContainerManager mgr(&host, &splitter);

        ContainerWidget* tile =
            mgr.createContainer(1, DockMode::OverlayDocked);
        QVERIFY(tile);

        tile->setDockedLocation(QPoint(317, 204));
        tile->restoreLocation();
        QCOMPARE(tile->pos(), QPoint(317, 204));

        tile->setDockedLocation(QPoint(318, 204));
        tile->restoreLocation();
        QCOMPARE(tile->pos(), QPoint(318, 204));
    }

    // Ein Applet, das den Stapel verlaesst, muss LEBEND herauskommen.
    // removeApplet haengt es aus, ohne es zu loeschen — darauf beruht
    // der ganze Weg. Ginge es dabei kaputt, waere die Kachel leer.
    void theAppletSurvivesLeavingTheStack()
    {
        RadioModel model;
        model.addSlice();
        AppletPanelWidget panel;
        auto* rx = new RxApplet(model.slices().value(0), &model);
        panel.addApplet(rx);

        QCOMPARE(panel.applets().size(), 1);
        QPointer<RxApplet> alive(rx);

        panel.removeApplet(rx);

        QVERIFY2(!alive.isNull(),
                 "removeApplet darf das Applet NICHT loeschen — sonst "
                 "bekommt die Kachel eine Leiche");
        QCOMPARE(panel.applets().size(), 0);

        delete rx;
    }

    // Und der ganze Weg: aus dem Stapel in eine Kachel mit Lage.
    void anAppletEndsUpInATileWithAPlace()
    {
        RadioModel model;
        model.addSlice();
        QWidget host;
        host.resize(1200, 800);
        QSplitter splitter;
        ContainerManager mgr(&host, &splitter);

        AppletPanelWidget panel;
        auto* rx = new RxApplet(model.slices().value(0), &model);
        panel.addApplet(rx);
        panel.removeApplet(rx);

        ContainerWidget* tile =
            mgr.createContainer(1, DockMode::OverlayDocked);
        QVERIFY(tile);
        tile->setContent(rx);
        tile->setDockedLocation(QPoint(66, 44));
        tile->restoreLocation();

        QCOMPARE(tile->content(), static_cast<QWidget*>(rx));
        QCOMPARE(tile->pos(), QPoint(66, 44));
        QVERIFY2(rx->parentWidget() != nullptr,
                 "das Applet muss in der Kachel haengen");
    }

    // Das Schloss ist der Gegenpol: wer eine Anordnung hat, will sie
    // festhalten koennen.
    void aLockedTileSaysSo()
    {
        QWidget host;
        QSplitter splitter;
        ContainerManager mgr(&host, &splitter);

        ContainerWidget* tile =
            mgr.createContainer(1, DockMode::OverlayDocked);
        QVERIFY(tile);
        QVERIFY(!tile->isLocked());

        QSignalSpy spy(tile, &ContainerWidget::lockedChanged);
        tile->setLocked(true);
        QCOMPARE(spy.count(), 1);
        QVERIFY(tile->isLocked());
    }
};

QTEST_MAIN(TestAppletToCanvas)
#include "tst_applet_to_canvas.moc"
