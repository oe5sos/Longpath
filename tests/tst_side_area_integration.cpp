// no-port-check: Longpath-original test, no Thetis logic.
//
// Der Seitenbereich am ECHTEN Hauptfenster (Betreiber 2026-09-25,
// Variante 2 „Symbolleiste am Rand"): Rotor/Log und ein Applet hinein,
// umschalten, zuklappen, Profil hin und zurück, ausschalten, Beenden.
// Die Klasse allein prüft tst_side_area_window; hier geht es um die
// Heimaten drumherum — dass nichts verloren geht und nichts doppelt
// wohnt.

#include <QtTest>

#include "core/AppSettings.h"
#include "gui/LayoutProfiles.h"
#include "gui/MainWindow.h"
#include "gui/SideAreaWindow.h"
#include "gui/ToolWindow.h"
#include "gui/PanFloatingWindow.h"
#include "gui/PanadapterStack.h"
#include "gui/PanadapterApplet.h"
#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/AppletVisibilityController.h"
#include "gui/applets/AppletWidget.h"

using namespace Longpath;

namespace {

QVariantMap reloadedProfileState()
{
    QVariantMap restored;
    LayoutProfiles fresh;
    fresh.setHooks([]() -> QVariantMap { return {}; },
                   [&restored](const QVariantMap& s) { restored = s; });
    fresh.load();
    fresh.applyCurrent();
    return restored;
}

bool hasAncestor(const QWidget* w, const QWidget* ancestor)
{
    for (const QWidget* p = w; p; p = p->parentWidget()) {
        if (p == ancestor) { return true; }
    }
    return false;
}

} // namespace

class TestSideAreaIntegration : public QObject
{
    Q_OBJECT

private:
    MainWindow* m_mw{nullptr};

    AppletPanelWidget* panel() const
    { return m_mw ? m_mw->findChild<AppletPanelWidget*>() : nullptr; }
    AppletVisibilityController* vis() const
    { return m_mw ? m_mw->findChild<AppletVisibilityController*>() : nullptr; }
    LayoutProfiles* profiles() const
    { return m_mw ? m_mw->findChild<LayoutProfiles*>() : nullptr; }

private slots:
    void initTestCase()
    {
        AppSettings::instance().remove(LayoutProfiles::settingsKey());
        m_mw = new MainWindow();
        m_mw->resize(1280, 800);
        m_mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_mw));
        QTest::qWait(300);
        QVERIFY(panel());
        QVERIFY(vis());
        QVERIFY(profiles());
    }

    // Einschalten legt Rotor/Log als erste Seite hinein.
    void enablingPutsRotorFirst()
    {
        QVERIFY(!m_mw->sideAreaForTest());
        m_mw->setSideAreaEnabledForTest(true);
        SideAreaWindow* area = m_mw->sideAreaForTest();
        QVERIFY(area);
        QVERIFY(area->isVisible());
        QVERIFY(!area->pageIds().isEmpty());
        QCOMPARE(area->pageIds().first(), QStringLiteral("WinRotorLog"));
        QVERIFY(vis()->isVisible(QStringLiteral("WinRotorLog")));
        // Das Rotor-Fenster gibt es dann nicht mehr — das Panel wohnt hier.
        QVERIFY(!m_mw->rotorWindowForTest());
    }

    // Ein Applet hinein: aus der Spalte heraus, sichtbar, aktiv.
    void appletMovesOutOfTheColumn()
    {
        SideAreaWindow* area = m_mw->sideAreaForTest();
        QVERIFY(area);
        AppletWidget* rx = m_mw->appletForKeyForTest(QStringLiteral("Rx"));
        QVERIFY(rx);
        QVERIFY(m_mw->addToSideAreaForTest(QStringLiteral("Rx"), true));
        QCOMPARE(area->activeId(), QStringLiteral("Rx"));
        QVERIFY(!panel()->applets().contains(rx));
        QVERIFY(hasAncestor(rx, area));
        QVERIFY(vis()->isVisible(QStringLiteral("Rx")));
        // Die Sichtbarkeitspumpe darf es NICHT in die Spalte zurücklegen.
        QCoreApplication::processEvents();
        QVERIFY(hasAncestor(rx, area));
    }

    void railSwitchesAndCollapses()
    {
        SideAreaWindow* area = m_mw->sideAreaForTest();
        QVERIFY(area);
        area->railClicked(QStringLiteral("Rx"));          // aktiv → zu
        QVERIFY(area->isCollapsed());
        QCOMPARE(area->width(), SideAreaWindow::kRailW);
        area->railClicked(QStringLiteral("WinRotorLog")); // andere → auf
        QVERIFY(!area->isCollapsed());
        QCOMPARE(area->activeId(), QStringLiteral("WinRotorLog"));
    }

    // Das Profil trägt den Bereich, und eine Anwendung baut ihn wieder
    // auf — mit denselben Seiten, derselben aktiven, und niemand wohnt
    // danach doppelt.
    void profileRoundTrip()
    {
        SideAreaWindow* area = m_mw->sideAreaForTest();
        QVERIFY(area);
        area->setActive(QStringLiteral("Rx"));
        profiles()->captureIntoCurrent();
        profiles()->save();

        const QVariantMap s = reloadedProfileState();
        const QVariantMap side = s.value(QStringLiteral("sideArea")).toMap();
        QCOMPARE(side.value(QStringLiteral("pages")).toStringList(),
                 (QStringList{QStringLiteral("WinRotorLog"), QStringLiteral("Rx")}));
        QCOMPARE(side.value(QStringLiteral("active")).toString(), QStringLiteral("Rx"));
        QCOMPARE(s.value(QStringLiteral("rotor")).toMap()
                     .value(QStringLiteral("form")).toString(),
                 QStringLiteral("side"));

        profiles()->applyCurrent();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        SideAreaWindow* again = m_mw->sideAreaForTest();
        QVERIFY(again);
        QCOMPARE(again->pageIds(),
                 (QStringList{QStringLiteral("WinRotorLog"), QStringLiteral("Rx")}));
        QCOMPARE(again->activeId(), QStringLiteral("Rx"));
        AppletWidget* rx = m_mw->appletForKeyForTest(QStringLiteral("Rx"));
        QVERIFY(rx);
        QVERIFY(hasAncestor(rx, again));
        QVERIFY(!panel()->applets().contains(rx));
        QVERIFY(!m_mw->rotorWindowForTest());
    }

    // Ausschalten über den Auswähler nimmt es heraus, zurück in die Spalte.
    void switchingOffTakesItOut()
    {
        SideAreaWindow* area = m_mw->sideAreaForTest();
        QVERIFY(area);
        vis()->setVisible(QStringLiteral("Rx"), false);
        QVERIFY(!area->hasPage(QStringLiteral("Rx")));
        AppletWidget* rx = m_mw->appletForKeyForTest(QStringLiteral("Rx"));
        QVERIFY(panel()->applets().contains(rx));
        vis()->setVisible(QStringLiteral("Rx"), true);   // wieder an (Spalte)
    }

    // „Aus dem Seitenbereich nehmen" macht ein eigenes Fenster daraus.
    void removingRotorGivesItsOwnWindow()
    {
        m_mw->removeFromSideAreaToOwnWindowForTest(QStringLiteral("WinRotorLog"));
        QVERIFY(m_mw->rotorWindowForTest());
        QVERIFY(m_mw->rotorWindowForTest()->isVisible());
        QVERIFY(!m_mw->sideAreaForTest()->hasPage(QStringLiteral("WinRotorLog")));
    }

    // Ausschalten löst den Bereich auf, ohne dass etwas verschwindet.
    void disablingDissolves()
    {
        QVERIFY(m_mw->addToSideAreaForTest(QStringLiteral("WinRotorLog"), true));
        QVERIFY(!m_mw->rotorWindowForTest());
        m_mw->setSideAreaEnabledForTest(false);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!m_mw->sideAreaForTest());
        QVERIFY(m_mw->rotorWindowForTest());   // eigenes Fenster
    }

    // Karte aus "Widget hinzufuegen" abgelegt -> Seite (Betreiber 2026-09-25:
    // "widget oeffnen und per drag and drop auf die taskleiste"). Was keine
    // Seite sein kann (Fenster ausser Rotor/Log, Leisten), wird abgewiesen.
    void droppedPickerCardBecomesAPage()
    {
        m_mw->setSideAreaEnabledForTest(true);
        SideAreaWindow* area = m_mw->sideAreaForTest();
        QVERIFY(area);
        QCOMPARE(m_mw->sideAreaKeyForForTest(QStringLiteral("Tx")), QStringLiteral("Tx"));
        QCOMPARE(m_mw->sideAreaKeyForForTest(QStringLiteral("WinRotorLog")),
                 QStringLiteral("WinRotorLog"));
        QVERIFY(m_mw->sideAreaKeyForForTest(QStringLiteral("WinLogbook")).isEmpty());
        QVERIFY(m_mw->sideAreaKeyForForTest(QStringLiteral("ChromeStatusBar")).isEmpty());

        emit area->pageDropped(QStringLiteral("Tx"));
        QVERIFY(area->hasPage(QStringLiteral("Tx")));
        QCOMPARE(area->activeId(), QStringLiteral("Tx"));
        emit area->pageDropped(QStringLiteral("WinLogbook"));
        QVERIFY(!area->hasPage(QStringLiteral("WinLogbook")));
    }

    // Ein schwebendes Applet-Fenster kennt seine Seitenkennung; wird es
    // abgelegt, verschwindet das Fenster und das Applet ist eine Seite.
    void floatingAppletWindowCanBeDroppedIn()
    {
        SideAreaWindow* area = m_mw->sideAreaForTest();
        QVERIFY(area);
        m_mw->removeFromSideAreaToOwnWindowForTest(QStringLiteral("Tx"));
        AppletFloatingWindow* win = nullptr;
        for (QWidget* w : QApplication::topLevelWidgets()) {
            auto* f = qobject_cast<AppletFloatingWindow*>(w);
            if (f && f->appletId() == QStringLiteral("Tx")) { win = f; }
        }
        QVERIFY(win);
        QCOMPARE(m_mw->sideAreaIdForWindowForTest(win), QStringLiteral("Tx"));
        QVERIFY(m_mw->addToSideAreaForTest(m_mw->sideAreaIdForWindowForTest(win), true));
        QVERIFY(area->hasPage(QStringLiteral("Tx")));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        for (QWidget* w : QApplication::topLevelWidgets()) {
            auto* f = qobject_cast<AppletFloatingWindow*>(w);
            QVERIFY(!(f && f->appletId() == QStringLiteral("Tx")));
        }
        m_mw->removeFromSideAreaToOwnWindowForTest(QStringLiteral("Tx"));
    }

    // Zuklappen gibt die Breite einem angrenzenden schwebenden Panadapter,
    // Aufklappen nimmt sie ihm wieder — genau so viel, nicht mehr.
    void collapsingWidensTheAdjacentPanadapter()
    {
        m_mw->setSideAreaEnabledForTest(true);
        SideAreaWindow* area = m_mw->sideAreaForTest();
        QVERIFY(area);
        // Rasterfeste Werte (2026-09-26): Fenster rasten jetzt an allen
        // vier Kanten auf 8 px ein, krumme Werte (900/330) zog das
        // Raster nach dem Setzen auf 904/328.
        constexpr int kAreaW = 328;
        area->setGeometry(904, 104, kAreaW, 496);

        auto* stack = m_mw->findChild<PanadapterStack*>();
        auto* pan = m_mw->findChild<PanadapterApplet*>();
        QVERIFY(stack);
        QVERIFY(pan);
        stack->floatPanadapter(pan->panId());
        QTest::qWait(300);
        PanFloatingWindow* win = stack->floatingWindowForTest(pan->panId());
        QVERIFY(win);
        win->setGeometry(96, 152, 800, 304);   // rechter Rand 896: 8 px Luft
        QTest::qWait(50);
        const int w0 = win->width();

        area->setCollapsed(true);
        QCOMPARE(win->width(), w0 + (kAreaW - SideAreaWindow::kRailW));
        area->setCollapsed(false);
        QCOMPARE(win->width(), w0);

        // Ein Panadapter, der NICHT angrenzt, bleibt, wie er ist.
        win->setGeometry(96, 152, 600, 304);
        QTest::qWait(50);
        area->setCollapsed(true);
        QCOMPARE(win->width(), 600);
        area->setCollapsed(false);

        // Wieder andocken: ein beim Abbau noch schwebender Panadapter
        // loest unter "offscreen" den Qt-eigenen Absturz in
        // QOffscreenBackingStore::clearHash() aus (docs/architecture/
        // 2026-09-08-container-move-test-offscreen-teardown-segfault.md,
        // tst_closing_takes_the_float_along ueberspringt deshalb) -- mit
        // dem Seitenbereich hat das nichts zu tun.
        stack->dockPanadapter(pan->panId());
        QTest::qWait(100);
    }

    // Beenden mit gefülltem Bereich: kein Absturz beim Abräumen.
    void closingWithPagesInside()
    {
        m_mw->setSideAreaEnabledForTest(true);
        QVERIFY(m_mw->addToSideAreaForTest(QStringLiteral("Rx"), true));
        m_mw->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        delete m_mw;
        m_mw = nullptr;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
};

QTEST_MAIN(TestSideAreaIntegration)
#include "tst_side_area_integration.moc"
