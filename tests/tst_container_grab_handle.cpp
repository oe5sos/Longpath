// =================================================================
// tests/tst_container_grab_handle.cpp  (NereusSDR)
// =================================================================
//
// Der Griff, an dem ein geloester Container haengt.
//
// ENTSTANDEN AUS EINEM SELBSTTEST AM LAUFENDEN PROGRAMM (2026-08-19).
// Der Betreiber verlangte „jeder Container muss sich ueberall
// hinbewegen koennen". Das Umschalten auf „Move freely" funktionierte
// sichtbar — der Container legte sich ueber den Panadapter. Bewegen
// liess er sich trotzdem nicht: es war nichts zum Anfassen da.
//
// Grund, und das ist die Falle, auf die ich hereingefallen bin:
//
//   m_titleBarVisible       — DARF die Leiste erscheinen (Vorgabe true)
//   m_titleBar->isVisible() — ZEIGT sie gerade (beim Bauen false, im
//                             Splitter nur beim Ueberfahren)
//
// Zwei Dinge, ein aehnlicher Name. Mein erster Versuch fragte die
// Erlaubnis ab, die ohnehin true ist, und aenderte nichts.
//
// Dieser Test prueft das SICHTBARE, nicht die Erlaubnis. Er ist der
// Grund, warum ich die naechste Runde nicht wieder durch Klicken
// herausfinden muss.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QLabel>
#include <QWidget>

#include "gui/containers/ContainerWidget.h"

using namespace Longpath;

namespace {

// Die Titelleiste ist das erste Kind mit fester Hoehe 22 — dieselbe
// Zahl wie ContainerWidget::kTitleBarHeight.
QWidget* titleBarOf(ContainerWidget& c)
{
    const auto kids = c.findChildren<QWidget*>(QString{},
                                               Qt::FindDirectChildrenOnly);
    for (QWidget* w : kids) {
        if (w->height() == ContainerWidget::kTitleBarHeight
                || w->minimumHeight() == ContainerWidget::kTitleBarHeight
                || w->maximumHeight() == ContainerWidget::kTitleBarHeight) {
            return w;
        }
    }
    return nullptr;
}

} // namespace

class TestContainerGrabHandle : public QObject
{
    Q_OBJECT

private slots:

    void theTitleBarExists()
    {
        ContainerWidget c;
        QVERIFY2(titleBarOf(c) != nullptr,
                 "ohne Titelleiste gibt es nie einen Griff");
    }

    // Im Splitter darf sie verborgen bleiben: dort bewegt der
    // Splitter-Griff, nicht der Container.
    void inThePanelItMayStayHidden()
    {
        ContainerWidget c;
        c.setDockMode(DockMode::PanelDocked);
        QWidget* bar = titleBarOf(c);
        QVERIFY(bar);
        QVERIFY2(!bar->isVisible(),
                 "im Splitter bleibt sie verborgen, bis jemand hinfaehrt");
    }

    // DER FALL, DER IM LAUFENDEN PROGRAMM GESCHEITERT IST: geloest muss
    // sie DAUERHAFT stehen. Die Applets darunter schlucken jede
    // Mausbewegung, also kommt das Ueberfahren nie an — ohne dauerhafte
    // Leiste ist „frei bewegen" ein leeres Versprechen.
    void freeingItGivesItAHandle()
    {
        ContainerWidget c;
        c.resize(400, 300);
        c.show();
        c.setDockMode(DockMode::PanelDocked);
        c.setDockMode(DockMode::OverlayDocked);

        QWidget* bar = titleBarOf(c);
        QVERIFY(bar);
        QVERIFY2(bar->isVisible(),
                 "ueberlagernd MUSS die Titelleiste stehen — sonst ist "
                 "nichts zum Anfassen da");
    }

    void ownWindowGetsAHandleToo()
    {
        ContainerWidget c;
        c.resize(400, 300);
        c.show();
        c.setDockMode(DockMode::PanelDocked);
        c.setDockMode(DockMode::Floating);

        QWidget* bar = titleBarOf(c);
        QVERIFY(bar);
        QVERIFY(bar->isVisible());
    }

    // Und zurueck in den Splitter darf sie wieder verschwinden, sonst
    // frisst sie dort dauerhaft 22 Bildpunkte Hoehe.
    void backInThePanelItMayHideAgain()
    {
        ContainerWidget c;
        c.resize(400, 300);
        c.show();
        c.setDockMode(DockMode::OverlayDocked);
        c.setDockMode(DockMode::PanelDocked);

        QWidget* bar = titleBarOf(c);
        QVERIFY(bar);
        QVERIFY2(!bar->isVisible(),
                 "im Splitter wieder verborgen");
    }

    // DER FALL AUS DEM LAUFENDEN PROGRAMM. Die Leiste erschien beim
    // Umschalten und verschwand sofort wieder: drei Stellen blendeten sie
    // aus, sobald der Zeiger nicht mehr in den obersten 22 Bildpunkten
    // stand — in JEDEM Zustand. Geloest muss sie bleiben.
    void theHandleSurvivesTheMouseLeaving()
    {
        ContainerWidget c;
        c.resize(400, 300);
        c.show();
        c.setDockMode(DockMode::OverlayDocked);

        QWidget* bar = titleBarOf(c);
        QVERIFY(bar);
        QVERIFY(bar->isVisible());

        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(&c, &leave);

        QVERIFY2(bar->isVisible(),
                 "der Griff darf nicht verschwinden, wenn die Maus geht");
    }

    // Im Splitter bleibt die Automatik: dort waere eine dauerhafte
    // Leiste 22 Bildpunkte Hoehe, die niemand braucht.
    void inThePanelTheHandleStillHidesOnLeave()
    {
        ContainerWidget c;
        c.resize(400, 300);
        c.show();
        c.setDockMode(DockMode::OverlayDocked);
        c.setDockMode(DockMode::PanelDocked);

        QWidget* bar = titleBarOf(c);
        QVERIFY(bar);

        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(&c, &leave);
        QVERIFY(!bar->isVisible());
    }
};

QTEST_MAIN(TestContainerGrabHandle)
#include "tst_container_grab_handle.moc"
