// =================================================================
// tests/tst_pan_floating_window.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic D Task 8: PanFloatingWindow construct + dock signal.
// =================================================================
#include <QtTest/QtTest>
#include <QPointer>
#include <QSignalSpy>
#include "gui/PanFloatingWindow.h"
#include "gui/WindowChrome.h"
#include "gui/PanadapterApplet.h"

using namespace Longpath;

class TestPanFloatingWindow : public QObject {
    Q_OBJECT
private slots:
    void construct_with_applet_keeps_applet_reachable()
    {
        auto* applet = new PanadapterApplet(QStringLiteral("pan-floated"));
        PanFloatingWindow* w = new PanFloatingWindow(applet, nullptr);
        QCOMPARE(w->applet(), applet);
        QCOMPARE(w->panId(), QStringLiteral("pan-floated"));
        // QVBoxLayout::addWidget reparents the applet to the window, so
        // deleting the window also deletes the applet (single owner). No
        // separate `delete applet;` here; that would be a double-free.
        delete w;
    }

    void dock_requested_signal_emitted_on_request_dock()
    {
        auto* applet = new PanadapterApplet(QStringLiteral("pan-floated"));
        PanFloatingWindow* w = new PanFloatingWindow(applet, nullptr);
        QSignalSpy spy(w, &PanFloatingWindow::dockRequested);
        w->requestDock();
        QCOMPARE(spy.count(), 1);
        delete w;
    }

    void externally_destroyed_applet_is_guarded()
    {
        auto* applet = new PanadapterApplet(QStringLiteral("pan-floated"));
        auto* w = new PanFloatingWindow(applet, nullptr);
        QPointer<PanadapterApplet> guarded(applet);

        delete applet;

        QVERIFY(guarded.isNull());
        QCOMPARE(w->applet(), nullptr);
        QCOMPARE(w->panId(), QString());
        w->requestDock();
        delete w;
    }
    // ── Auch der Panadapter wird geschoben und gezogen ──────────────
    //
    // Der Betreiber hat ihn ausdruecklich mitgemeint: „wie zum Beispiel
    // den Pen Adapter kleiner nach rechts und nach links vergroessern
    // lassen". Er ist der schwierige Fall, weil er ein natives Fenster
    // ist und alles Nicht-Native verdeckt.
    void itHasItsOwnTitleBarAndGrip()
    {
        auto* applet = new PanadapterApplet(QStringLiteral("A"));
        PanFloatingWindow win(applet, nullptr);

        auto* bar  = win.findChild<WindowTitleBar*>();
        auto* grip = win.findChild<ResizeGrip*>();
        QVERIFY2(bar,  "der Panadapter braucht eine eigene Leiste zum Ziehen");
        QVERIFY2(grip, "und einen Anfasser unten rechts");
        QVERIFY2(win.windowFlags() & Qt::FramelessWindowHint,
                 "rahmenlos, sonst stehen zwei Leisten uebereinander");

        // DIE Eigenschaft, an der die Kachelversuche gescheitert sind:
        // ein nicht-natives Kind liegt auf macOS hinter dem nativen
        // QRhiWidget des Panadapters und ist damit unerreichbar.
        QVERIFY2(grip->testAttribute(Qt::WA_NativeWindow),
                 "der Anfasser MUSS nativ sein — sonst liegt er hinter "
                 "der Wasserfallflaeche (siehe SpectrumWidget.cpp:551)");
    }

};

QTEST_MAIN(TestPanFloatingWindow)
#include "tst_pan_floating_window.moc"
