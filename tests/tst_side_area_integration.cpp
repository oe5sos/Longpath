// no-port-check: Longpath-original test, no Thetis logic.
//
// Der Seitenbereich am ECHTEN Hauptfenster (Betreiber 2026-09-25,
// Variante 2 „Symbolleiste am Rand"): Rotor/Log und ein Applet hinein,
// umschalten, zuklappen, Profil hin und zurück, ausschalten, Beenden.
// Die Klasse allein prüft tst_side_area_window; hier geht es um die
// Heimaten drumherum — dass nichts verloren geht und nichts doppelt
// wohnt.

#include <QtTest>
#include <QScopeGuard>
#include <QGuiApplication>
#include <QScreen>

#include "core/AppSettings.h"
#include "gui/StyleConstants.h"
#include "gui/LayoutProfiles.h"
#include "gui/MainWindow.h"
#include "gui/SideAreaWindow.h"
#include "gui/ToolWindow.h"
#include "gui/PanFloatingWindow.h"
#include "gui/PanadapterStack.h"
#include "gui/PanadapterApplet.h"
#include "gui/WindowPlacement.h"   // kSnapGridPx — der Fall rastet
#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/AppletVisibilityController.h"
#include "gui/applets/AppletWidget.h"
#include "FensterFrist.h"

// Spiegelt kNeighbourSlackPx aus src/gui/MainWindow_SideArea.cpp:58
// („grenzt an" — Luecke bis hierhin). Dort dateilokal; hier noch einmal,
// damit der ferne Fall nachrechnen kann, dass er WIRKLICH fern ist.
namespace { constexpr int kNeighbourSlackPxFuerTest = 16; }

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

// Laenger als die 180 ms von snapToGridAfterSettle (WindowPlacement.h),
// mit Luft fuer einen langsamen Laeufer.
static constexpr int kRastFristMs = 400;

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
        QVERIFY(QTest::qWaitForWindowExposed(m_mw, kFensterFrist));
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

        // ── Keine feste Liste (2026-10-15) ───────────────────────────
        //
        // Hier stand die Erwartung {WinRotorLog, Rx} woertlich. Wie
        // viele Seiten beim Einschalten uebernommen werden, haengt aber
        // davon ab, welche Applets gerade als „in der Spalte verdeckt"
        // gelten (MainWindow::appletHiddenInColumn zaehlt, was zu mehr
        // als einem Viertel von anderen FENSTERN ueberdeckt ist) — und
        // das wiederum von Bildschirmgroesse und Fensterlage. Auf
        // diesem Mac kam „Tx" dazu und der Lauf fiel um, in der CI
        // (anderer Schirm) nicht.
        //
        // Dieser Lauf will ohnehin etwas anderes wissen: ueberlebt der
        // Bereich den Weg durchs Profil UNVERAENDERT? Also wird
        // verglichen, was vorher drin war — nicht, was einmal drin
        // war.
        const QStringList vorher = area->pageIds();
        QVERIFY2(vorher.contains(QStringLiteral("WinRotorLog")),
                 qPrintable(vorher.join(QLatin1Char(','))));
        QVERIFY2(vorher.contains(QStringLiteral("Rx")),
                 qPrintable(vorher.join(QLatin1Char(','))));

        profiles()->captureIntoCurrent();
        profiles()->save();

        const QVariantMap s = reloadedProfileState();
        const QVariantMap side = s.value(QStringLiteral("sideArea")).toMap();
        QCOMPARE(side.value(QStringLiteral("pages")).toStringList(), vorher);
        QCOMPARE(side.value(QStringLiteral("active")).toString(), QStringLiteral("Rx"));
        QCOMPARE(s.value(QStringLiteral("rotor")).toMap()
                     .value(QStringLiteral("form")).toString(),
                 QStringLiteral("side"));

        profiles()->applyCurrent();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        SideAreaWindow* again = m_mw->sideAreaForTest();
        QVERIFY(again);
        QCOMPARE(again->pageIds(), vorher);
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
        // ── Der Fall muss AUF DEN SCHIRM passen (2026-11-02) ───────
        //
        // Hier standen feste Zahlen: die Seitenflaeche auf x=896 mit
        // Breite 336, also bis 1232. Auf einem Mac stimmt das. Unter
        // `xvfb-run` ist der virtuelle Schirm aber nur rund 796 Punkte
        // breit (gemessen am 2026-10-20, #279) -- die Flaeche lag dort
        // KOMPLETT ausserhalb, der beim Einklappen frei werdende Platz
        // damit auch, und der Nachbar konnte nicht hineinwachsen.
        //
        // Der Lauf meldete `win->width()` 781 statt 1074 und fiel
        // danach mit SIGSEGV um. Zwei Mal an einem Vormittag gekostet:
        // einmal gesucht, einmal durch einen Neulauf widerlegt. Siehe
        // [[longpath-pruefstand-fensterbreite-xvfb]].
        //
        // Jetzt haengen alle vier Kanten am WIRKLICH verfuegbaren
        // Bereich: die Flaeche klebt an dessen rechter Kante, das
        // Fenster links daneben. Damit ist der frei werdende Platz auf
        // jedem Schirm wirklich da, und die Rechnung unten gilt
        // ueberall -- auf dem Mac wie unter xvfb.
        //
        // Rasterfest bleibt es auch: `kSnapGridPx` teilt jede Kante,
        // sonst zieht das Raster nach dem Setzen nach und die Rechnung
        // geht um ein paar Punkte daneben (so geschehen am 2026-10-20,
        // 904 rastete auf 912).
        const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
        constexpr int kGrid  = Longpath::kSnapGridPx;
        constexpr int kAreaW = 336;          // wie bisher: 21 Rasterfelder
        auto abRunden = [](int v) { return (v / kGrid) * kGrid; };

        // Die Flaeche an die rechte Kante, rasterfest nach links gerueckt.
        const int areaX = abRunden(avail.right() + 1 - kAreaW);
        const int areaY = abRunden(avail.top() + 96);
        area->setGeometry(areaX, areaY, kAreaW, 512);

        auto* stack = m_mw->findChild<PanadapterStack*>();
        auto* pan = m_mw->findChild<PanadapterApplet*>();
        QVERIFY(stack);
        QVERIFY(pan);
        stack->floatPanadapter(pan->panId());
        QTest::qWait(300);
        PanFloatingWindow* win = stack->floatingWindowForTest(pan->panId());
        QVERIFY(win);
        // Andocken auf jedem Weg hinaus -- auch wenn ein QCOMPARE unten
        // abbricht. Ein beim Abbau noch schwebender Panadapter stuerzt
        // unter "offscreen" in QOffscreenBackingStore::clearHash() ab
        // (Linux-CI 2026-11-03: erst QCOMPARE rot, dann SIGSEGV), und aus
        // einem Fehlschlag wurde ein Absturz ohne Zahl.
        auto andocken = qScopeGuard([stack, pan]() {
            stack->dockPanadapter(pan->panId());
            QTest::qWait(100);
        });

        // Das Fenster links daneben, seine rechte Kante eine Luft vor
        // der linken Kante der Flaeche -- das ist „grenzt an"
        // (kNeighbourSlackPx = 16 in MainWindow_SideArea.cpp).
        //
        // ── Erst setzen, dann ABLESEN, dann ruecken ────────────────
        //
        // Die Breite kann man sich nicht ausrechnen: das Fenster hat
        // eine Mindestbreite (gemessen 420 unter offscreen), und
        // `resize` klemmt stillschweigend darauf hoch. Ein aus der
        // Flaechenkante gerechnetes, schmaleres Mass landete deshalb
        // 52 Punkte IN der Flaeche statt daneben -- und `touches`
        // (Grenze 16) schlug nicht an, der Nachbar wuchs nicht, der
        // Lauf fiel um. Darum: Groesse setzen, die WIRKLICHE Breite
        // ablesen, und das Fenster danach so ruecken, dass seine
        // rechte Kante eine Luft vor der Flaeche sitzt.
        const int winY = abRunden(avail.top() + 144);
        // Nach jedem Setzen und Ruecken WARTEN, bis die Lage steht:
        // snapToGridAfterSettle (WindowPlacement.h) rastet den Rahmen erst
        // 180 ms nach dem Loslassen ein. Wer nach 50 ms misst, misst die
        // Breite VOR dem Rasten und vergleicht spaeter mit der danach --
        // auf dem langsamen Linux-Laeufer fiel genau das in die Messung
        // (429 statt 420, 2026-11-03). Breiten erst nach dem Rasten lesen.
        win->setGeometry(abRunden(avail.left() + 96), winY, 2 * kGrid, 304);
        QTest::qWait(kRastFristMs);
        const int w0 = win->width();
        const int winX = areaX - w0 - Style::kFensterLuftPx;
        win->move(winX, winY);
        QTest::qWait(kRastFristMs);
        QCOMPARE(win->width(), w0);

        // Die Voraussetzung ausdruecklich pruefen, statt sie zu hoffen:
        // ohne Platz links von der Flaeche misst der Rest nichts.
        // Nicht ueberspringen -- ein uebersprungener Lauf sieht wie ein
        // Erfolg aus.
        QVERIFY2(winX >= avail.left(),
                 qPrintable(QStringLiteral(
                     "kein Platz neben der Seitenflaeche: Schirm %1 breit, "
                     "Flaeche ab %2, Fenster mindestens %3 breit -- es "
                     "begaenne bei %4")
                     .arg(avail.width()).arg(areaX).arg(w0).arg(winX)));

        // ── Es waechst um GENAU den frei gewordenen Platz ──────────
        //
        // Hier stand bis zum 2026-11-02 ein `- kFensterLuftPx`, mit der
        // Begruendung „gemessen: 1077 statt 1080, also exakt eine
        // Luft". Die Luft war aber nie ein Abzug beim Wachsen -- sie
        // steckte darin, dass die Erwartung mit der GESETZTEN Breite
        // der Flaeche rechnete (336) statt mit der, die nach dem
        // Rastern wirklich dasteht (gemessen 333). Drei Punkte
        // Unterschied, und genau drei wurden abgezogen.
        //
        // Jetzt mit der wirklichen Breite -- und dann stimmt die Regel
        // ohne Korrektur: `resizeNeighbourPans` legt `delta` drauf,
        // und `delta` ist die Aenderung der Flaechengeometrie.
        // Nachgerechnet unter offscreen: Flaeche 333 breit, Leiste 40,
        // frei also 293; Fenster 420 → 713. Exakt 293.
        // Auch die Breite der Flaeche selbst wird gerastert (gemessen:
        // 333 statt der gesetzten 336). Die Erwartung rechnet deshalb
        // mit dem, was wirklich dasteht.
        const int areaWist = area->geometry().width();
        area->setCollapsed(true);
        QCOMPARE(win->width(),
                 w0 + (areaWist - SideAreaWindow::kRailW));
        area->setCollapsed(false);
        QCOMPARE(win->width(), w0);

        // Ein Panadapter, der NICHT angrenzt, bleibt, wie er ist.
        //
        // „Weit weg" heisst: mehr als kNeighbourSlackPx (16) von der
        // linken Kante der Flaeche entfernt. Erreicht wird das durch
        // RUECKEN, nicht durch Verkleinern -- die Mindestbreite des
        // Fensters laesst sich nicht unterschreiten, ein schmaleres
        // Mass wird stillschweigend hochgeklemmt und der Fall misst
        // dann wieder etwas anderes als gemeint (2026-11-02).
        // Erst ruecken, dann rasten lassen, DANN die Breite lesen, mit der
        // weiter gerechnet wird -- bei x = 0 rastet die rechte Kante neu.
        const int fernX = avail.left();
        win->move(fernX, winY);
        QTest::qWait(kRastFristMs);
        const int fernW = win->width();
        QVERIFY2(areaX - (fernX + fernW) > kNeighbourSlackPxFuerTest,
                 qPrintable(QStringLiteral(
                     "das Fenster grenzt immer noch an: rechte Kante %1, "
                     "Flaeche ab %2")
                     .arg(fernX + fernW).arg(areaX)));
        area->setCollapsed(true);
        QCOMPARE(win->width(), fernW);
        area->setCollapsed(false);

        // Wieder andocken: ein beim Abbau noch schwebender Panadapter
        // loest unter "offscreen" den Qt-eigenen Absturz in
        // QOffscreenBackingStore::clearHash() aus (docs/architecture/
        // 2026-09-08-container-move-test-offscreen-teardown-segfault.md,
        // tst_closing_takes_the_float_along ueberspringt deshalb) -- mit
        // dem Seitenbereich hat das nichts zu tun. Angedockt wird vom
        // qScopeGuard oben, auf jedem Weg hinaus.
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
