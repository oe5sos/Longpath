// =================================================================
// tests/tst_applet_detach_reachable.cpp  (Longpath)
// =================================================================
//
// Der Weg zum eigenen Fenster muss ANKLICKBAR sein.
//
// ── Warum es diese Pruefung gibt ────────────────────────────────────
//
// Am 2026-08-20 habe ich dem Betreiber gemeldet, abgeloeste Fenster
// liessen sich jetzt ueberall hinschieben und in der Ecke ziehen. Die
// Fensterpruefungen waren gruen. Seine Antwort: „es funktioniert
// wieder nicht, was machst du die ganze zeit".
//
// Er hatte recht. Der Ablöseknopf ⤢ stand in
// AppletPanelWidget::wrapWithTitleBar — und die wird seit der
// Umstellung auf das Raster nur noch fuer die S-Meter-Kopfzeile
// gerufen. Jedes echte Applet laeuft ueber GridCellWidget, und deren
// Kopfleiste hatte nur Griff und Titel. Es gab schlicht nichts
// anzuklicken.
//
// Das war der ACHTE Fall „gebaut, aber nirgends erreichbar" in diesem
// Vorhaben. Alle acht haben dieselbe Ursache: geprueft wurde der
// Zustand, nicht der Weg. Ein Test, der ein Signal von Hand aussendet
// und dann prueft, dass ein Fenster entsteht, ist gruen, waehrend
// niemand das Signal auslösen kann.
//
// Diese Pruefung faengt bei dem an, was eine Hand findet: ein
// sichtbarer, eingeschalteter Knopf in der Kopfleiste. Sie drueckt ihn
// und schaut, ob am anderen Ende etwas ankommt.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-20 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QPushButton>
#include <QSignalSpy>
#include <QLabel>

#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/GridCellWidget.h"
#include "gui/applets/RxApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

// Sucht den Knopf so, wie ein Mensch ihn sucht: ueber seine
// Beschriftung in der Kopfleiste — nicht ueber einen Objektnamen, den
// nur der Quelltext kennt.
QPushButton* buttonWithText(QWidget* root, const QString& text)
{
    const QList<QPushButton*> all = root->findChildren<QPushButton*>();
    for (QPushButton* b : all) {
        if (b->text() == text) { return b; }
    }
    return nullptr;
}

} // namespace

class TestAppletDetachReachable : public QObject
{
    Q_OBJECT

private slots:

    // DIE Pruefung. Ohne sie war der ganze Rest Theorie.
    void everyAppletHasAClickableDetachButton()
    {
        RadioModel model;
        model.addSlice();
        auto* rx = new RxApplet(model.slices().value(0), &model);

        AppletPanelWidget panel;
        panel.addApplet(rx);
        panel.resize(280, 600);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));

        auto* cell = panel.findChild<GridCellWidget*>();
        QVERIFY2(cell, "das Applet muss in einem Rasterfeld liegen");

        QPushButton* detach = buttonWithText(cell->titleBar(),
                                             QStringLiteral("↗"));
        QVERIFY2(detach,
                 "in der Kopfleiste MUSS ein Ablöseknopf stehen — bis "
                 "2026-08-20 stand er in wrapWithTitleBar, die kein "
                 "Applet mehr benutzt, und war damit unerreichbar");
        QVERIFY2(detach->isVisible(),
                 "und er muss zu sehen sein, nicht nur zu existieren");
        QVERIFY2(detach->isEnabled(), "und anklickbar");
    }

    // ── Und bei BREITER Spalte? ──────────────────────────────────────
    //
    // Der Betreiber hat am 2026-08-20 die Spalte auf gut 700 px
    // gezogen und gemeldet, nur am Panadapter gehe es. Beim
    // Panadapter steht das ↗ dicht beim Titel; in der Applet-Spalte
    // sitzt es am rechten Rand — bei 700 px sind das 700 px Abstand
    // vom Titel, auf den man schaut.
    void theDetachButtonIsNearTheTitleEvenInAWideColumn()
    {
        RadioModel model;
        model.addSlice();
        auto* rx = new RxApplet(model.slices().value(0), &model);

        AppletPanelWidget panel;
        panel.addApplet(rx);
        panel.resize(710, 600);          // die Breite des Betreibers
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));

        auto* cell = panel.findChild<GridCellWidget*>();
        QVERIFY(cell);
        QPushButton* detach = buttonWithText(cell->titleBar(),
                                             QStringLiteral("↗"));
        QVERIFY(detach);

        // Wo steht der Titel, wo der Knopf?
        int titleRight = 0;
        for (QLabel* l : cell->titleBar()->findChildren<QLabel*>()) {
            if (!l->text().isEmpty() && l->text() != QStringLiteral("⋮⋮")) {
                titleRight = l->geometry().right();
            }
        }
        const int gap = detach->geometry().left() - titleRight;
        qDebug() << "Spaltenbreite" << cell->titleBar()->width()
                 << "Titel endet bei" << titleRight
                 << "Knopf beginnt bei" << detach->geometry().left()
                 << "Abstand" << gap;

        QVERIFY2(gap <= 120,
                 qPrintable(QStringLiteral(
                     "der Ablöseknopf steht %1 px vom Titel entfernt — bei "
                     "breiter Spalte sucht ihn dort niemand")
                     .arg(gap)));
    }

    // Und der Druck muss am anderen Ende ankommen. Ein Knopf, der
    // nichts ausloest, ist genauso nutzlos wie keiner.
    void clickingItAsksForARealWindow()
    {
        RadioModel model;
        model.addSlice();
        auto* rx = new RxApplet(model.slices().value(0), &model);

        AppletPanelWidget panel;
        panel.addApplet(rx);
        panel.resize(280, 600);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));

        QSignalSpy spy(&panel, &AppletPanelWidget::appletDetachRequested);

        auto* cell = panel.findChild<GridCellWidget*>();
        QVERIFY(cell);
        QPushButton* detach = buttonWithText(cell->titleBar(),
                                             QStringLiteral("↗"));
        QVERIFY(detach);

        // Ein echter Klick auf den Knopf, nicht ein von Hand
        // ausgesendetes Signal.
        QTest::mouseClick(detach, Qt::LeftButton);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<AppletWidget*>(),
                 static_cast<AppletWidget*>(rx));
    }

    // Dasselbe fuer das Ausblenden.
    void theHideButtonIsReachableToo()
    {
        RadioModel model;
        model.addSlice();
        auto* rx = new RxApplet(model.slices().value(0), &model);

        AppletPanelWidget panel;
        panel.addApplet(rx);
        panel.resize(280, 600);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));

        QSignalSpy spy(&panel, &AppletPanelWidget::appletHideRequested);

        auto* cell = panel.findChild<GridCellWidget*>();
        QVERIFY(cell);
        QPushButton* close = buttonWithText(cell->titleBar(),
                                            QStringLiteral("✕"));
        QVERIFY2(close, "und ein Ausblendeknopf");
        QVERIFY(close->isVisible());

        QTest::mouseClick(close, Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestAppletDetachReachable)
#include "tst_applet_detach_reachable.moc"
