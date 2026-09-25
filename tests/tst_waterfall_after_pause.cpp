// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original test, no Thetis logic.
//
// Wasserfall nach einer Pause -- gemessen am GPU-Framebuffer.
//
// Befund 2026-09-26 (Rendering-Nacht): Die Zeilen entstehen im Takt,
// auch wenn das Fenster nicht gezeichnet wird (versteckt, verdeckt).
// Beim naechsten Bild laedt der Teil-Upload die Zeilen zwischen
// m_wfLastUploadedRow und m_wfWriteRow hoch. Ist der Ring in der Pause
// umgelaufen, nennt dieser Abstand nur den Rest: im Pruefstand 400 Zeilen
// geschrieben, 72 hochgeladen. Darunter stand der Wasserfall von VOR der
// Pause -- alte Zeilen in falscher Zeitfolge.
//
// Probe: Traeger A fuellt den Wasserfall, Fenster verstecken, dann nur
// noch Traeger B ueber mehr als eine Texturhoehe, wieder zeigen. Von A
// darf nichts mehr zu sehen sein, B muss von oben bis unten stehen.

#include <QtTest>
#include <QImage>

#include <cmath>

#include "gui/SpectrumWidget.h"

using namespace Longpath;

namespace {

constexpr int    kBins     = 4096;
constexpr double kCenterHz = 14'100'000.0;
constexpr double kSpanHz   = 48'000.0;

QVector<float> frameWithCarrierAt(double hz)
{
    QVector<float> bins(kBins, static_cast<float>(std::pow(10.0, -13.5)));
    const int c = static_cast<int>(std::lround((hz - kCenterHz) / (kSpanHz / kBins))) + kBins / 2;
    for (int k = -1; k <= 1; ++k) {
        bins[c + k] = static_cast<float>(std::pow(10.0, -6.0));
    }
    return bins;
}

// Anteil heller Punkte in einem Spaltenband des Wasserfallbereichs.
double brightShare(const QImage& img, int x0, int x1, int y0, int y1)
{
    int bright = 0;
    int rows = 0;
    for (int y = y0; y < y1; ++y) {
        bool rowBright = false;
        for (int x = x0; x <= x1; ++x) {
            const QRgb c = img.pixel(x, y);
            if (std::max({qRed(c), qGreen(c), qBlue(c)}) > 150) { rowBright = true; break; }
        }
        bright += rowBright ? 1 : 0;
        ++rows;
    }
    return rows > 0 ? static_cast<double>(bright) / rows : 0.0;
}

void feedFor(SpectrumWidget& w, const QVector<float>& f, int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        w.updateSpectrumLinear(0, f, 1.0, 0.0);
        QTest::qWait(15);
    }
}

} // namespace

class TstWaterfallAfterPause : public QObject { Q_OBJECT
private slots:
    void rowsWrittenWhileHiddenAllReachTheScreen()
    {
#ifndef LONGPATH_GPU_SPECTRUM
        QSKIP("CPU-Renderpfad: dort malt paintEvent direkt aus dem Bild.");
#else
        SpectrumWidget w;
        w.resize(800, 400);
        w.setDdcCenterFrequency(kCenterHz);
        w.setSampleRate(kSpanHz);
        w.setFrequencyRange(kCenterHz, kSpanHz);
        w.setVfoFrequency(kCenterHz + 20'000.0);   // Filter weit rechts
        w.setWfUpdatePeriodMs(10);                 // schnell viele Zeilen
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        const double fA = kCenterHz - 12'000.0;
        const double fB = kCenterHz + 4'000.0;

        // Vorher: A fuellt den ganzen Wasserfall (mehr als eine Hoehe).
        feedFor(w, frameWithCarrierAt(fA), 3000);

        // Pause: versteckt, nur noch B -- wieder mehr als eine Hoehe.
        w.hide();
        feedFor(w, frameWithCarrierAt(fB), 3000);

        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        feedFor(w, frameWithCarrierAt(fB), 150);

        const QImage img = w.grabFramebuffer();
        if (img.isNull()) {
            QSKIP("Kein QRhi/GPU-Backend unter der Offscreen-Plattform.");
        }
        // Frequenz -> x: die Spanne liegt auf der Breite ohne den
        // dBm-Streifen rechts; +-3 % Breite Spielraum statt dessen
        // genauer Breite.
        const double dpr = img.width() / static_cast<double>(w.width());
        const int specW = static_cast<int>(img.width() * 0.95);
        auto xOf = [&](double hz) {
            return static_cast<int>((hz - (kCenterHz - kSpanHz / 2)) / kSpanHz * specW);
        };
        const int slack = static_cast<int>(0.03 * specW);
        // Wasserfall: unteres Drittel sicher (oben Spektrum und Leisten)
        const int y0 = static_cast<int>(img.height() * 0.67);
        const int y1 = img.height() - static_cast<int>(4 * dpr);
        const double shareA = brightShare(img, xOf(fA) - slack, xOf(fA) + slack, y0, y1);
        const double shareB = brightShare(img, xOf(fB) - slack, xOf(fB) + slack, y0, y1);
        qInfo() << "Traeger A (vor der Pause):" << shareA << " Traeger B (in der Pause):" << shareB;
        if (const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR"); !dir.isEmpty()) {
            img.save(dir + QStringLiteral("/waterfall_after_pause.png"));
        }
        QVERIFY2(shareB > 0.9, "Die Zeilen aus der Pause fehlen im Wasserfall");
        QVERIFY2(shareA < 0.05,
                 qPrintable(QStringLiteral("Unter den neuen Zeilen steht noch der Wasserfall "
                                           "von vor der Pause (%1 der Zeilen)").arg(shareA)));
#endif
    }
};

QTEST_MAIN(TstWaterfallAfterPause)
#include "tst_waterfall_after_pause.moc"
