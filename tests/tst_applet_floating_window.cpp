// =================================================================
// tests/tst_applet_floating_window.cpp  (Longpath)
// =================================================================
//
// Ein abgeloestes Applet muss ein ECHTES Fenster sein.
//
// ── Warum das die entscheidende Eigenschaft ist ─────────────────────
//
// Am 2026-08-20 habe ich einen halben Tag lang Kacheln gebaut:
// ContainerWidget im Zustand OverlayDocked, absolute Lage, Ziehen an
// der Titelleiste. Die Tests waren gruen und auf dem Schirm aenderte
// sich NICHTS. Der Betreiber hat es viermal gemeldet.
//
// Der Grund steht in unserem eigenen Quelltext, SpectrumWidget.cpp:551:
//
//     „QRhiWidget with WA_NativeWindow on macOS does not support
//      child widget overlays"
//
// Der Panadapter ist ein natives Fenster. Auf macOS zeichnet ein
// natives NSView IMMER ueber allen nicht-nativen Geschwistern — kein
// raise() hilft. Kacheln als Kindwidgets liegen dahinter.
//
// Deshalb pruefen wir hier die Eigenschaft, die WIRKLICH traegt:
// isWindow(). Ein echtes Fenster liegt nicht im Widgetbaum des
// Hauptfensters, sondern daneben — das Betriebssystem schiebt es
// ueberall hin, auch auf einen zweiten Schirm, und nichts kann davor
// zeichnen.
//
// AetherSDR hat dieselbe Wand gefunden und dieselbe Antwort gegeben
// (FloatingContainerWindow).
//
// =================================================================
// Modification history (Longpath):
//   2026-08-20 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QScreen>
#include <QPushButton>

#include "gui/applets/AppletFloatingWindow.h"
#include "gui/WindowChrome.h"
#include "gui/applets/RxApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TestAppletFloatingWindow : public QObject
{
    Q_OBJECT

private slots:

    // DIE Eigenschaft. Ohne sie liegt das Fenster im Baum des
    // Hauptfensters und damit hinter dem nativen Panadapter.
    void aDetachedAppletIsARealTopLevelWindow()
    {
        RadioModel model;
        model.addSlice();
        auto* rx = new RxApplet(model.slices().value(0), &model);

        AppletFloatingWindow win(rx, QStringLiteral("Rx"), 0, nullptr);

        QVERIFY2(win.isWindow(),
                 "ein abgeloestes Applet MUSS ein echtes Fenster sein — "
                 "als Kindwidget laege es hinter dem nativen "
                 "Panadapter, und genau das war der Fehler");
        QVERIFY2(win.parentWidget() == nullptr,
                 "und es darf kein Elternwidget haben");
    }

    // Ueberall hin: ein echtes Fenster nimmt jede Lage an, auch eine
    // ausserhalb des Hauptfensters.
    void itGoesWhereverItIsPut()
    {
        RadioModel model;
        model.addSlice();
        auto* rx = new RxApplet(model.slices().value(0), &model);
        AppletFloatingWindow win(rx, QStringLiteral("Rx"), 0, nullptr);

        win.resize(420, 300);
        win.move(137, 251);
        QCOMPARE(win.pos(), QPoint(137, 251));

        // Auf den Millimeter: ein Bildpunkt Unterschied muss ankommen.
        win.move(138, 251);
        QCOMPARE(win.pos(), QPoint(138, 251));
    }

    // Und die Groesse laesst sich aendern — keine feste Hoehe, kein
    // Raster, das rundet.
    void itCanBeResizedFreely()
    {
        RadioModel model;
        model.addSlice();
        auto* rx = new RxApplet(model.slices().value(0), &model);
        AppletFloatingWindow win(rx, QStringLiteral("Rx"), 0, nullptr);

        win.resize(500, 400);
        QCOMPARE(win.size(), QSize(500, 400));
        win.resize(501, 401);
        QCOMPARE(win.size(), QSize(501, 401));
    }

    // Das Applet muss LEBEND darin haengen — nicht kopiert, nicht tot.
    void theAppletIsAliveInside()
    {
        RadioModel model;
        model.addSlice();
        auto* rx = new RxApplet(model.slices().value(0), &model);
        QPointer<RxApplet> alive(rx);

        {
            AppletFloatingWindow win(rx, QStringLiteral("Rx"), 0, nullptr);
            QVERIFY2(!alive.isNull(), "das Applet muss leben");
            QVERIFY2(rx->window() == &win,
                     "und im neuen Fenster haengen, nicht mehr im alten");
        }
    }

    // ── Erreichbarkeit, nicht nur Zustand ───────────────────────────
    //
    // Die vier Pruefungen darueber sind gruen, seit es die Klasse gibt,
    // und der Betreiber konnte trotzdem nichts schieben. Sie pruefen,
    // dass sich das Fenster bewegen LAESST, wenn man move() ruft — nicht,
    // dass es einen Griff gibt, an dem ein Mensch es anfassen kann.
    //
    // Das ist der Fehler, der uns Tage gekostet hat: geprueft wurde der
    // Zustand, nicht die Erreichbarkeit. Ab hier pruefen wir den Weg,
    // den eine Hand nimmt.

    void itHasAGrabbableTitleBar()
    {
        RadioModel model;
        model.addSlice();
        auto* rx = new RxApplet(model.slices().value(0), &model);
        AppletFloatingWindow win(rx, QStringLiteral("Rx"), 0, nullptr);

        auto* bar = win.findChild<WindowTitleBar*>();
        QVERIFY2(bar, "ohne eigene Titelleiste gibt es nichts anzufassen — "
                      "der Rahmen des Betriebssystems ist abgeschaltet");
        QVERIFY2(win.windowFlags() & Qt::FramelessWindowHint,
                 "und das Fenster muss rahmenlos sein, sonst stehen zwei "
                 "Leisten uebereinander");
    }

    // Ein Zug an der Leiste MUSS das Fenster bewegen — in alle vier
    // Richtungen, nicht nur nach rechts unten.
    void draggingTheTitleBarMovesTheWindow()
    {
        RadioModel model;
        model.addSlice();
        auto* rx = new RxApplet(model.slices().value(0), &model);
        AppletFloatingWindow win(rx, QStringLiteral("Rx"), 0, nullptr);
        win.resize(420, 300);
        win.move(300, 300);
        win.show();
        QVERIFY(QTest::qWaitForWindowExposed(&win));

        auto* bar = win.findChild<WindowTitleBar*>();
        QVERIFY(bar);

        const QPoint before = win.pos();
        const QPoint press(600, 320);

        auto sendTo = [bar](QEvent::Type t, QPoint global, Qt::MouseButtons btns) {
            QMouseEvent ev(t, bar->mapFromGlobal(global), global,
                           Qt::LeftButton, btns, Qt::NoModifier);
            QApplication::sendEvent(bar, &ev);
        };

        sendTo(QEvent::MouseButtonPress, press, Qt::LeftButton);
        // Nach LINKS und nach OBEN — die Richtungen, die der Betreiber
        // ausdruecklich genannt hat und die ein reines
        // „unten-rechts-Ziehen" nicht abdeckt.
        sendTo(QEvent::MouseMove, press + QPoint(-80, -55), Qt::LeftButton);
        const QPoint afterUpLeft = win.pos();
        sendTo(QEvent::MouseMove, press + QPoint(120, 90), Qt::LeftButton);
        const QPoint afterDownRight = win.pos();
        sendTo(QEvent::MouseButtonRelease, press + QPoint(120, 90), Qt::NoButton);

        QCOMPARE(afterUpLeft, before + QPoint(-80, -55));
        QCOMPARE(afterDownRight, before + QPoint(120, 90));
    }

    // Und unten rechts der Anfasser: derselbe Zug, aber er aendert die
    // Groesse statt der Lage — in x UND y, groesser wie kleiner.
    void theBottomRightGripResizesInBothAxes()
    {
        RadioModel model;
        model.addSlice();
        auto* rx = new RxApplet(model.slices().value(0), &model);
        AppletFloatingWindow win(rx, QStringLiteral("Rx"), 0, nullptr);
        win.setMinimumSize(200, 120);
        if (rx) { rx->setMinimumSize(0, 0); }
        win.resize(600, 400);
        win.show();
        QVERIFY(QTest::qWaitForWindowExposed(&win));

        auto* grip = win.findChild<ResizeGrip*>();
        QVERIFY2(grip, "ohne sichtbaren Anfasser unten rechts muss man die "
                       "Ecke erraten — genau das war die Klage");

        // Er sitzt WIRKLICH unten rechts, nicht irgendwo.
        QVERIFY2(grip->x() + grip->width()  >= win.width()  - 8,
                 "der Anfasser gehoert an den rechten Rand");
        QVERIFY2(grip->y() + grip->height() >= win.height() - 8,
                 "und an den unteren");

        const QSize before = win.size();
        const QPoint press = grip->mapToGlobal(grip->rect().center());

        auto sendTo = [grip](QEvent::Type t, QPoint global, Qt::MouseButtons btns) {
            QMouseEvent ev(t, grip->mapFromGlobal(global), global,
                           Qt::LeftButton, btns, Qt::NoModifier);
            QApplication::sendEvent(grip, &ev);
        };

        sendTo(QEvent::MouseButtonPress, press, Qt::LeftButton);
        sendTo(QEvent::MouseMove, press + QPoint(70, 40), Qt::LeftButton);
        const QSize bigger = win.size();
        // Und wieder kleiner: nach links/oben zurueck unter die
        // Ausgangsgroesse.
        sendTo(QEvent::MouseMove, press + QPoint(-90, -60), Qt::LeftButton);
        const QSize smaller = win.size();
        sendTo(QEvent::MouseButtonRelease, press + QPoint(-90, -60), Qt::NoButton);

        QCOMPARE(bigger, QSize(before.width() + 70, before.height() + 40));
        QVERIFY2(smaller.width()  < before.width(),
                 "nach links ziehen muss schmaler machen");
        QVERIFY2(smaller.height() < before.height(),
                 "nach oben ziehen muss niedriger machen");
    }

    // Der Betreiber am 2026-09-17: "man sollte auch immer das fenster
    // auf die volle größe anpassen können, geht aber so nicht." Ein
    // Klick auf ⤢ fuellt den nutzbaren Schirm, der zweite stellt Lage
    // und Groesse wieder her; festgestellt tut der Knopf nichts.
    void theZoomButtonFillsTheScreenAndComesBack()
    {
        RadioModel model;
        model.addSlice();
        auto* rx = new RxApplet(model.slices().value(0), &model);
        AppletFloatingWindow win(rx, QStringLiteral("Rx"), 0, nullptr);
        win.setMinimumSize(200, 120);
        if (rx) { rx->setMinimumSize(0, 0); }
        win.move(120, 90);
        win.resize(600, 400);
        win.show();
        QVERIFY(QTest::qWaitForWindowExposed(&win));

        auto* bar = win.findChild<WindowTitleBar*>();
        QVERIFY(bar);
        QPushButton* zoom = nullptr;
        for (QPushButton* b : bar->findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("⤢")) { zoom = b; break; }
        }
        QVERIFY2(zoom && zoom->isVisible(), "ein ⤢ muss in der Leiste stehen");

        const QRect before = win.geometry();
        QTest::mouseClick(zoom, Qt::LeftButton);
        QVERIFY(bar->isZoomed());
        const QRect avail = win.screen()->availableGeometry();
        QVERIFY2(win.width() >= avail.width() - 2 && win.height() >= avail.height() - 2,
                 qPrintable(QStringLiteral("nach ⤢ nur %1x%2 statt %3x%4")
                     .arg(win.width()).arg(win.height())
                     .arg(avail.width()).arg(avail.height())));

        QTest::mouseClick(zoom, Qt::LeftButton);
        QVERIFY(!bar->isZoomed());
        QCOMPARE(win.geometry().size(), before.size());

        // Festgestellt: der Knopf ist aus.
        bar->setLocked(true);
        QVERIFY(!zoom->isEnabled());
        bar->toggleZoom();
        QVERIFY2(!bar->isZoomed(), "festgestellt heisst festgestellt");
    }
};

QTEST_MAIN(TestAppletFloatingWindow)
#include "tst_applet_floating_window.moc"
