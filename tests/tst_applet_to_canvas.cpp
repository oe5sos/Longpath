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
#include <QTabBar>

#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/RxApplet.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "core/AppSettings.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

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

    // ── Die Lage muss den Neustart ueberleben ────────────────────────
    //
    // Gefunden beim Durchsehen der offenen Punkte am 2026-08-20: die
    // Kacheln bekamen eine Lage, aber niemand schrieb sie auf. Nach
    // einem Neustart kam der Container leer zurueck und das Applet lag
    // wieder im Stapel — doppeltes Mobiliar.
    //
    // Geprueft wird die Zeichenkette, die in die Einstellungen geht:
    // Panelkennung und Rechteck, wiederauffindbar.
    void aTileLayoutIsWrittenInAReadableForm()
    {
        // Format: „Rx:40,40,360,260;Tx:66,66,360,260"
        const QString blob = QStringLiteral("Rx:40,40,360,260;Tx:66,66,360,260");

        const QStringList entries = blob.split(QLatin1Char(';'),
                                               Qt::SkipEmptyParts);
        QCOMPARE(entries.size(), 2);

        const QString first = entries.first();
        const int colon = first.indexOf(QLatin1Char(':'));
        QVERIFY2(colon > 0, "vor dem Doppelpunkt steht die Panelkennung");
        QCOMPARE(first.left(colon), QStringLiteral("Rx"));

        const QStringList n = first.mid(colon + 1).split(QLatin1Char(','));
        QCOMPARE(n.size(), 4);
        QCOMPARE(n[0].toInt(), 40);
        QCOMPARE(n[3].toInt(), 260);
    }

    // Eine Kachel muss sich von der Speicherung des ContainerManagers
    // ABMELDEN koennen. Sonst schreibt er sie mit, und beim naechsten
    // Start steht ein leerer Rahmen neben dem Applet.
    void aTileCanOptOutOfContainerPersistence()
    {
        QWidget host;
        QSplitter splitter;
        ContainerManager mgr(&host, &splitter);

        ContainerWidget* tile =
            mgr.createContainer(1, DockMode::OverlayDocked);
        QVERIFY(tile);

        mgr.setPersisted(tile->id(), false);
        mgr.saveState();

        const QString ids = AppSettings::instance()
            .value(QStringLiteral("ContainerIdList"), QString{}).toString();
        QVERIFY2(!ids.split(QLatin1Char(',')).contains(tile->id()),
                 "eine abgemeldete Kachel darf nicht in der Container-"
                 "Liste stehen");
    }

    // ── Reiter: mehrere Fenster in einer Kachel ──────────────────────
    //
    // Vorbild ist Zeus' „Multi Panel" (Bildschirmvideo 2026-08-20:
    // FREQUENCY·VFO und S-METER teilen einen Rahmen).
    //
    // Der Punkt, der still falsch wird: die Reiterleiste darf bei EINEM
    // Inhalt nicht erscheinen. Eine Leiste ueber einem einzigen Reiter
    // ist eine Zeile Hoehe fuer nichts — und sieht aus wie ein Fehler.
    void oneContentGetsNoTabBar()
    {
        QWidget host;
        QSplitter splitter;
        ContainerManager mgr(&host, &splitter);

        ContainerWidget* tile = mgr.createContainer(1, DockMode::OverlayDocked);
        QVERIFY(tile);
        auto* a = new QWidget;
        tile->setContent(a);

        QCOMPARE(tile->tabCount(), 1);
        QVERIFY2(tile->findChildren<QTabBar*>().isEmpty()
                     || !tile->findChildren<QTabBar*>().first()->isVisible(),
                 "bei einem Inhalt darf keine Reiterleiste stehen");
    }

    void aSecondContentBringsTheTabBar()
    {
        QWidget host;
        host.resize(800, 600);
        QSplitter splitter;
        ContainerManager mgr(&host, &splitter);

        ContainerWidget* tile = mgr.createContainer(1, DockMode::OverlayDocked);
        QVERIFY(tile);
        tile->setContent(new QWidget);
        tile->addTab(new QWidget, QStringLiteral("Zweites"));

        QCOMPARE(tile->tabCount(), 2);
        QCOMPARE(tile->tabTitle(1), QStringLiteral("Zweites"));
    }

    // Herausnehmen darf NICHT loeschen — darauf beruht das
    // Herausloesen. Ginge der Reiter dabei kaputt, bekaeme die neue
    // Kachel eine Leiche.
    void takingATabGivesBackALiveWidget()
    {
        QWidget host;
        host.resize(800, 600);
        QSplitter splitter;
        ContainerManager mgr(&host, &splitter);

        ContainerWidget* tile = mgr.createContainer(1, DockMode::OverlayDocked);
        QVERIFY(tile);
        tile->setContent(new QWidget);

        auto* second = new QWidget;
        QPointer<QWidget> alive(second);
        tile->addTab(second, QStringLiteral("Zweites"));
        QCOMPARE(tile->tabCount(), 2);

        QWidget* out = tile->takeTab(1);
        QCOMPARE(out, second);
        QVERIFY2(!alive.isNull(), "takeTab darf nicht loeschen");
        QCOMPARE(tile->tabCount(), 1);

        delete second;
    }

    // Und der Rueckweg auf eins: die Leiste muss wieder verschwinden.
    void goingBackToOneHidesTheTabBarAgain()
    {
        QWidget host;
        host.resize(800, 600);
        QSplitter splitter;
        ContainerManager mgr(&host, &splitter);

        ContainerWidget* tile = mgr.createContainer(1, DockMode::OverlayDocked);
        QVERIFY(tile);
        tile->setContent(new QWidget);
        tile->addTab(new QWidget, QStringLiteral("Zweites"));

        QWidget* out = tile->takeTab(1);
        QCOMPARE(tile->tabCount(), 1);
        const auto bars = tile->findChildren<QTabBar*>();
        QVERIFY(!bars.isEmpty());
        QVERIFY2(!bars.first()->isVisible(),
                 "bei einem Reiter muss die Leiste wieder weg sein");
        delete out;
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
