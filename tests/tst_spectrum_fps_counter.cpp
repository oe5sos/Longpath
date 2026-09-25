// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original test, no Thetis logic.
//
// Der FPS-Zaehler im GPU-Pfad muss gezeigte Bilder zaehlen.
//
// Bis 2026-09-25 zaehlte er beim Neubau der statischen Ueberlagerung --
// die entsteht nur bei Aenderungen (Abstimmen, Einstellungen), also
// zeigte er bei laufendem Spektrum fast 0 und beim Kurbeln "viel".

#include <QtTest>

#include <cmath>

#include "gui/SpectrumWidget.h"

#ifdef LONGPATH_GPU_SPECTRUM
#include <QRhiWidget>
#endif

using namespace Longpath;

class TstSpectrumFpsCounter : public QObject { Q_OBJECT
private slots:
    void countsRenderedFramesNotOverlayRebuilds()
    {
#ifndef LONGPATH_GPU_SPECTRUM
        QSKIP("CPU-Renderpfad: dort zaehlt paintEvent, das war nie falsch.");
#else
        SpectrumWidget w;
        w.resize(900, 500);
        w.setDdcCenterFrequency(14'100'000.0);
        w.setSampleRate(48'000.0);
        w.setFrequencyRange(14'100'000.0, 48'000.0);
        w.setShowFps(true);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        QVector<float> bins(4096, static_cast<float>(std::pow(10.0, -13.0)));
        // Wie viele Bilder wirklich abgeschickt werden, haengt vom System
        // ab (macOS drosselt verdeckte Fenster auf wenige je Sekunde).
        // Verglichen wird deshalb mit frameSubmitted, nicht mit 30.
        int submitted = 0;
        connect(&w, &QRhiWidget::frameSubmitted, this, [&submitted] { ++submitted; });
        // Einlaufen lassen, dann eine volle Zaehlperiode messen.
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 1200) {
            w.updateSpectrumLinear(0, bins, 1.0, 0.0);
            QTest::qWait(33);
        }
        submitted = 0;
        t.restart();
        while (t.elapsed() < 3100) {
            w.updateSpectrumLinear(0, bins, 1.0, 0.0);
            QTest::qWait(33);
        }
        if (w.grabFramebuffer().isNull()) {
            QSKIP("Kein QRhi/GPU-Backend unter der Offscreen-Plattform.");
        }
        const double real = submitted / (t.elapsed() / 1000.0);
        const float fps = w.fpsDisplayValueForTest();
        qInfo() << "angezeigt:" << fps << "fps, abgeschickt:" << real << "Bilder/s";
        QVERIFY2(real > 1.0, "es wurden kaum Bilder abgeschickt");
        // Die Rate schwankt zwischen den Messfenstern (gedrosselt 2..4 je
        // Sekunde), die Anzeige zeigt das letzte volle Fenster. Geprueft
        // wird deshalb nur, was der alte Zaehler verfehlt hat: bei
        // laufendem Spektrum nicht 0, und nicht mehr als wirklich kam.
        QVERIFY2(fps >= 0.5f && fps <= 1.5 * real + 1.0,
                 qPrintable(QStringLiteral("FPS-Anzeige %1, abgeschickt %2 Bilder/s")
                                .arg(fps).arg(real, 0, 'f', 1)));
#endif
    }
};

QTEST_MAIN(TstSpectrumFpsCounter)
#include "tst_spectrum_fps_counter.moc"
