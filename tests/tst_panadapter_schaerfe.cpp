// no-port-check: Longpath-original test, no Thetis logic.
//
// ── Panadapter schaerfer (2026-11-08) ────────────────────────────────────
//
// Betreiber an den Entwurfsblaettern der Rendering-Werkbank: „Alles
// zusammen". Drei Schalter sind seitdem AN ab Werk:
//
//   * Wasserfall in Geraete-Pixeln (setWaterfallDevicePixels)
//   * Raster als Haarlinie          (setGridHairline)
//   * kurze Kurvenkante, 0,5 Punkte (setTraceEdgePx)
//
// Dieser Pruefstand nagelt drei Dinge fest:
//
//   1. die Vorgabe ab Werk (leere Einstellungen -> alle drei an),
//   2. dass eine Abwahl gespeichert wird und den Neustart ueberlebt,
//   3. dass der Wasserfall bei doppelter Pixeldichte wirklich eine Spalte
//      je Geraete-Pixel hat -- und nicht nur einen Schalter, der nichts tut.
//
// Fall 3 laeuft mit QT_SCALE_FACTOR=2 (vor dem QApplication-Konstruktor
// gesetzt, nur dort liest Qt den Massstab), also auch offscreen in der CI.
// Gegenprobe gefahren: mit der Skalierung in applyResizeSettled
// abgeschaltet faellt Fall 3 (gleiche Breite an und aus).

#include <QtTest/QtTest>
#include <QApplication>

#include <cmath>

#include "gui/SpectrumWidget.h"

using namespace Longpath;

class TestPanadapterSchaerfe : public QObject {
    Q_OBJECT
private slots:
    void abWerkSindAlleDreiAn()
    {
        SpectrumWidget w;
        w.loadSettings();
        QVERIFY(w.waterfallDevicePixels());
        QVERIFY(w.gridHairline());
        QCOMPARE(w.traceEdgePx(), 0.5f);
    }

    void eineAbwahlUeberlebtDenNeustart()
    {
        {
            SpectrumWidget w;
            w.loadSettings();
            w.setWaterfallDevicePixels(false);
            w.setGridHairline(false);
            w.setTraceEdgePx(1.0f);
            w.saveSettings();
        }
        SpectrumWidget w2;
        w2.loadSettings();
        QVERIFY(!w2.waterfallDevicePixels());
        QVERIFY(!w2.gridHairline());
        QCOMPARE(w2.traceEdgePx(), 1.0f);

        // Zurueck auf die Vorgabe, damit die Reihenfolge der Faelle egal ist.
        w2.setWaterfallDevicePixels(true);
        w2.setGridHairline(true);
        w2.setTraceEdgePx(0.5f);
        w2.saveSettings();
    }

    void derWasserfallHatEineSpalteJeGeraetepixel()
    {
        SpectrumWidget w;
        w.resize(900, 600);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        const qreal dpr = w.devicePixelRatioF();
        if (dpr < 1.5) {
            QSKIP("QT_SCALE_FACTOR=2 hat nicht gegriffen -- ohne doppelte Dichte "
                  "gibt es nichts zu unterscheiden.");
        }

        w.setWaterfallDevicePixels(false);
        const QSize logisch = w.waterfallImageSizeForTest();
        w.setWaterfallDevicePixels(true);
        const QSize geraet = w.waterfallImageSizeForTest();

        QVERIFY2(logisch.width() > 0, "kein Wasserfallbild angelegt");
        // Breite: so viele Spalten, wie der Schirm Punkte hat.
        QCOMPARE(geraet.width(),
                 static_cast<int>(std::lround(logisch.width() * dpr)));
        // Hoehe bleibt logisch: eine Zeile je Takt, dieselbe Zeit je
        // Bildschirmhoehe wie vorher.
        QCOMPARE(geraet.height(), logisch.height());
    }
};

int main(int argc, char** argv)
{
    qputenv("QT_SCALE_FACTOR", "2");
    QApplication app(argc, argv);
    TestPanadapterSchaerfe t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_panadapter_schaerfe.moc"
