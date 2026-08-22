// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Spektrum-Kurve muss SICHTBAR sein — gemessen am echten
// GPU-Framebuffer, nicht an Zustandsvariablen.
//
// Vorgeschichte (2026-08-22): der Betreiber sah tagelang keinen Trace
// ("weiters sehe ich oben auch keinen wasserfall"), waehrend der
// Wasserfall lief. Alle Zustandsgroessen waren gesund: 964 Vertices,
// Zyan, Deckkraft 0.9, Viewport korrekt, Pipelines gebaut. Ein
// Minimal-Repro mit denselben Shadern zeichnete perfekt.
//
// Die Ursache war die ZEICHENREIHENFOLGE: die Behebung von
// "hintergrundbild funktioniert auch nicht im pandapter" (2026-08-20)
// malt paintBackgroundLayer in die statische Overlay-Textur — und
// deren Quad wurde NACH der Kurve gezeichnet. Ein deckender Grund
// ueber der Kurve; das Raster aus derselben Textur blieb sichtbar,
// die Kurve verschwand. tst_both_paint_paths hat den Zustand sogar
// erzwungen: er verlangte paintBackgroundLayer in beiden Wegen, sagte
// aber nichts ueber die Reihenfolge.
//
// Dieser Test fuettert ein kuenstliches Spektrum (Rauschflur -110 dBm,
// drei Traeger) und verlangt ZYANFARBENE Punkte im Spektrumbereich des
// Framebuffers. Gegenprobe von Hand gefahren: mit der alten
// Reihenfolge (Chrome nach der Kurve) findet er nichts und faellt.

#include <QtTest>
#include <QImage>
#include "gui/SpectrumWidget.h"
using namespace Longpath;
class TstSpectrumTraceVisible : public QObject { Q_OBJECT
private slots:
    void theTraceIsActuallyVisibleOnTheFramebuffer() {
        SpectrumWidget w;
        w.resize(1000, 600);
        // Die DDC-Abbildung MUSS zur Pan-Sicht passen, sonst greift
        // visibleBinRange daneben und der Detektor sieht nur Nullen.
        w.setDdcCenterFrequency(7'100'000.0);
        w.setSampleRate(192'000.0);
        w.setFrequencyRange(7'100'000.0, 192'000.0);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        for (int i=0;i<6;++i) QCoreApplication::processEvents();

        // Kuenstliches Spektrum: Rauschflur mit drei kraeftigen Traegern.
        QVector<float> bins(2048);
        for (int i = 0; i < bins.size(); ++i) {
            double dbm = -110.0;
            for (int c : {400, 1024, 1500}) {
                const double d = std::abs(i - c);
                if (d < 24) { dbm = std::max(dbm, -40.0 - d * 2.5); }
            }
            bins[i] = static_cast<float>(std::pow(10.0, dbm / 10.0));
        }
        for (int f = 0; f < 30; ++f) {
            w.updateSpectrumLinear(0, bins, 1.0, 0.0);
            QCoreApplication::processEvents();
            QTest::qWait(16);
        }

        const QImage img = w.grabFramebuffer();
        QVERIFY(!img.isNull());
        img.save("/tmp/trace_probe.png");
        qInfo() << "Framebuffer:" << img.size();

        // Farbige Punkte im Spektrumbereich zaehlen (oberes Drittel).
        int lit = 0;
        for (int y = 10; y < img.height() / 3; y += 2)
            for (int x = 10; x < img.width() - 10; x += 2) {
                const QColor c(img.pixel(x, y));
                if (c.lightness() > 60) { ++lit; }
            }
        // Nicht "hell" zaehlen (das findet auch Raster und Beschriftung
        // — genau daran war die erste Fassung dieser Sonde blind),
        // sondern die KURVENFARBE: sattes Zyan hat viel Blau+Gruen und
        // wenig Rot. Raster und Schrift sind grau.
        int cyan = 0;
        for (int y = 10; y < img.height() / 3; y += 2) {
            for (int x = 10; x < img.width() - 10; x += 2) {
                const QColor c(img.pixel(x, y));
                if (c.blue() > 120 && c.green() > 90 && c.red() < 80) {
                    ++cyan;
                }
            }
        }
        qInfo() << "Zyan-Punkte:" << cyan << "(hell insgesamt:" << lit << ")";
        QVERIFY2(cyan > 300,
                 qPrintable(QStringLiteral(
                     "Nur %1 zyanfarbene Punkte im Spektrumbereich — die "
                     "Kurve ist auf dem Schirm nicht zu sehen, obwohl "
                     "Daten flossen. So sah der Betreiber tagelang ein "
                     "leeres Spektrum ueber laufendem Wasserfall.")
                     .arg(cyan)));
    }
};
QTEST_MAIN(TstSpectrumTraceVisible)
#include "tst_spectrum_trace_visible.moc"
