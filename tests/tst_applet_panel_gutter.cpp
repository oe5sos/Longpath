// Verify A1: AppletPanelWidget reserves an 8 px scrollbar gutter on the right.
// Per docs/architecture/ui-audit-polish-plan.md §A1.
//
// 2026-08-18: zwei Faelle zum ☰-Knopf standen hier
// (bannerButtonHiddenUntilMenuSet, bannerButtonShownAndOpensMenuOnceSet).
// Sie sind GELOESCHT, nicht angepasst: der Knopf sass in der Titelleiste
// des festen S-Meter-Kopfes, und beide sind mit ihm weggefallen. Ein
// Test, der ein abgeschafftes Bedienelement prueft, hat seinen Sinn
// verloren — ihn auf „ist nicht da" umzuschreiben pruefte die Abwesenheit
// von etwas, das niemand vermisst.

#include <QApplication>
#include <QtTest/QtTest>
#include "gui/applets/AppletPanelWidget.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QMenu>
#include <QPushButton>

using Longpath::AppletPanelWidget;

class TstAppletPanelGutter : public QObject {
    Q_OBJECT
private slots:
    void stackLayoutReservesEightPxRightMargin();
};

void TstAppletPanelGutter::stackLayoutReservesEightPxRightMargin()
{
    // Die Rinne wird am WIDGET im Rollbereich gemessen, nicht an einem
    // bestimmten Layouttyp. Sie hing frueher an einem QVBoxLayout, seit
    // 2026-08-18 an einem AppletGrid (Schritt 1 des freien Rasters) —
    // die Zusicherung ist dieselbe geblieben: acht Pixel rechts fuer
    // die Rollleiste, sonst nichts.
    //
    // Auf den Layouttyp zu pruefen hiesse pruefen, WIE es gebaut ist,
    // statt WAS es zusichert. Der Test waere beim Rasterumbau rot
    // geworden, ohne dass sich am Bild etwas geaendert haette.
    AppletPanelWidget panel;
    auto* scroll = panel.findChild<QScrollArea*>();
    QVERIFY(scroll != nullptr);
    auto* stackWidget = scroll->widget();
    QVERIFY(stackWidget != nullptr);

    QMargins m = stackWidget->contentsMargins();
    QCOMPARE(m.right(), 8);  // 8 px reserved for the scrollbar
    QCOMPARE(m.left(), 0);
    QCOMPARE(m.top(), 0);
    QCOMPARE(m.bottom(), 0);
}



QTEST_MAIN(TstAppletPanelGutter)
#include "tst_applet_panel_gutter.moc"
