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

#include "gui/applets/AppletFloatingWindow.h"
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
};

QTEST_MAIN(TestAppletFloatingWindow)
#include "tst_applet_floating_window.moc"
