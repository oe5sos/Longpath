// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original test, no Thetis logic.
//
// Die Kurve muss auf dem Gitter liegen -- gemessen am GPU-Framebuffer.
//
// Befund 2026-09-25 (Rendering-Nacht): Gitter, dBm-Leiste und alle
// Ueberlagerungen rechnen dBm -> Hoehe mit dbmToY/dbmToYf. Die lassen
// unten den Bandplan-Streifen frei: der Boden des Bereichs liegt an
// dessen Oberkante. Die GPU-Kurve dagegen rechnete ueber die VOLLE
// Hoehe -- mit Bandplan (beim Betreiber Schrift 6 -> 10 px) lag sie am
// Rauschboden um fast die Streifenhoehe zu tief, oben stimmte sie. Und
// die 1-Hz-Normierung (normalizeShiftDb) kam in der GPU-Kurve gar nicht
// vor, obwohl Gitter und Ueberlagerungen sie anwenden.
//
// Messung: ein flaches Spektrum genau auf einer Gitterlinie (-120 dBm,
// Spanne -150..-60, Linien alle 10 dB). Liegt die Kurve richtig, verdeckt
// sie ihre Linie, und sie steht genau einen Linienabstand unter der
// naechsten sichtbaren (-110). Liegt sie daneben, taucht die -120-Linie
// auf, oder der Abstand stimmt nicht.

#include <QtTest>
#include <QImage>

#include <cmath>

#include "gui/SpectrumWidget.h"
#include "models/BandPlanManager.h"

using namespace Longpath;

namespace {

struct Probe {
    double traceY = -1.0;   // Mitte der Kurve (Geraete-Pixel)
    int line1 = -1;         // naechste sichtbare Gitterzeile ueber der Kurve
    int line2 = -1;         // die darueber
};

// Kurvenfarbe: sattes Zyan. Gitter: grau, alle drei Kanaele nah beieinander.
bool isTrace(QRgb c) { return qBlue(c) > 150 && qGreen(c) > 120 && qRed(c) < 90; }
bool isGrid(QRgb c)
{
    const int r = qRed(c), g = qGreen(c), b = qBlue(c);
    return std::max({r, g, b}) - std::min({r, g, b}) < 24 && g > 45 && g < 140;
}

// Nur Linien UEBER der Kurve zaehlen: darunter liegt die Fuellung und
// toent sie. Liegt die Kurve richtig, verdeckt sie ihre Linie, und die
// naechste sichtbare darueber ist die 10-dB-Nachbarin -- die Kurve steht
// dann genau einen Linienabstand unter ihr.
Probe measure(const QImage& img, int specBottomPx)
{
    Probe pr;
    // Spalten weit weg vom Filter/VFO und den Raendern
    const int xs[] = {img.width() / 8, img.width() / 5, img.width() / 4};
    double sum = 0.0;
    int n = 0;
    for (int x : xs) {
        for (int y = 0; y < specBottomPx; ++y) {
            if (isTrace(img.pixel(x, y))) { sum += y; ++n; }
        }
    }
    if (n == 0) { return pr; }
    pr.traceY = sum / n;
    auto gridRow = [&](int y) {
        for (int x : xs) { if (!isGrid(img.pixel(x, y))) { return false; } }
        return true;
    };
    for (int y = static_cast<int>(pr.traceY) - 4; y > 0; --y) {
        if (gridRow(y)) { pr.line1 = y; break; }
    }
    for (int y = pr.line1 - 4; y > 0; --y) {
        if (gridRow(y)) { pr.line2 = y; break; }
    }
    return pr;
}

} // namespace

class TstSpectrumTraceOnGrid : public QObject { Q_OBJECT
private slots:
    void traceSitsOnItsGridLine_data()
    {
        QTest::addColumn<bool>("bandPlan");
        QTest::addColumn<bool>("normalize");
        QTest::addColumn<float>("calDb");
        QTest::addColumn<bool>("readCalibrated");
        QTest::newRow("ohne Bandplan") << false << false << 0.0f << false;
        QTest::newRow("mit Bandplan") << true << false << 0.0f << false;
        // GPU-Kurve und Gitter verschieben sich um dieselbe Normierung
        // (wie im CPU-Pfad). Vorher bekam nur das Gitter sie.
        QTest::newRow("1-Hz-Normierung") << false << true << 0.0f << false;
        // Soll: die Achse steht fest, die Kalibrierung verschiebt die
        // DATEN -- ein Rohwert -135 mit +15 dB liest sich als -120. Ist:
        // Gitter und Zahlen laufen durch dbmToY mit und verschieben sich
        // mit, die Ablesung bleibt roh (siehe QEXPECT_FAIL unten).
        // +15: kein Vielfaches von 10, sonst fiele das verschobene Gitter
        // wieder auf Linien.
        QTest::newRow("Kalibrierung +15 dB kalibriert ablesen") << false << false << 15.0f << true;
    }

    void traceSitsOnItsGridLine()
    {
#ifndef LONGPATH_GPU_SPECTRUM
        QSKIP("CPU-Renderpfad: kein GPU-Framebuffer.");
#else
        QFETCH(bool, bandPlan);
        QFETCH(bool, normalize);
        QFETCH(float, calDb);
        QFETCH(bool, readCalibrated);

        BandPlanManager plans;
        plans.loadPlans();
        plans.setActivePlan(QStringLiteral("IARU Region 1"));

        SpectrumWidget w;
        w.resize(1000, 700);
        w.setDdcCenterFrequency(14'100'000.0);
        w.setSampleRate(192'000.0);
        w.setFrequencyRange(14'100'000.0, 192'000.0);
        w.setVfoFrequency(14'180'000.0);   // Filter weit rechts, weg von den Messspalten
        w.setDbmRange(-150.0f, -60.0f);
        w.setSpectrumRenderMode(SpectrumRenderMode::Mode2D);
        w.setBandPlanFontSize(bandPlan ? 10 : 0);
        w.setBandPlanManager(bandPlan ? &plans : nullptr);
        w.setDispNormalize(normalize);
        w.setDbmCalOffset(calDb);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        // Flach auf -120 dBm; im Kalibrier-Fall um die Verschiebung
        // vorgehalten, so dass die kalibrierte Ablesung -120 waere.
        // 4096 Bins: 46,875 Hz, Normierung -16,7 dB -- kein Vielfaches der
        // 10-dB-Linien (bei 2048 Bins waeren es -19,7, fast genau zwei).
        QVector<float> bins(4096);
        double shownDbm = -120.0;
        for (int f = 0; f < 40; ++f) {
            const double shift = normalize ? -10.0 * std::log10(w.binWidthHz()) : 0.0;
            const double inDbm = readCalibrated ? shownDbm - shift - calDb : shownDbm;
            bins.fill(static_cast<float>(std::pow(10.0, inDbm / 10.0)));
            w.updateSpectrumLinear(0, bins, 1.0, 0.0);
            QCoreApplication::processEvents();
            QTest::qWait(16);
        }

        const QImage img = w.grabFramebuffer();
        if (img.isNull()) {
            QSKIP("Kein QRhi/GPU-Backend unter der Offscreen-Plattform.");
        }
        if (const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR"); !dir.isEmpty()) {
            img.save(dir + QStringLiteral("/trace_on_grid_%1.png")
                               .arg(QString::fromLatin1(QTest::currentDataTag()).replace(QLatin1Char(' '), QLatin1Char('_'))));
        }
        // Spektrumbereich: oberhalb der Frequenzleiste. Etwa die obere
        // Haelfte; genauer braucht es nicht, die Suche endet an der
        // ersten Gitterzeile unter der Kurve.
        const Probe pr = measure(img, img.height() / 2);
        QVERIFY2(pr.traceY >= 0.0, "keine Kurve gefunden");
        QVERIFY2(pr.line1 >= 0 && pr.line2 >= 0, "keine zwei Gitterlinien ueber der Kurve");
        const double spacing = pr.line1 - pr.line2;
        const double expected = pr.line1 + spacing;
        qInfo().noquote() << QStringLiteral("Kurve y=%1, erwartet %2 (Linien %3/%4, Abstand %5)")
                                 .arg(pr.traceY, 0, 'f', 1).arg(expected, 0, 'f', 1)
                                 .arg(pr.line2).arg(pr.line1).arg(spacing, 0, 'f', 0);
        if (readCalibrated) {
            QEXPECT_FAIL("", "Gitter und dBm-Zahlen bekommen die Kalibrierung "
                             "mit (dbmToY), die Ablesung bleibt roh -- Befund "
                             "2026-09-25, Entscheidung beim Betreiber", Abort);
        }
        QVERIFY2(std::abs(pr.traceY - expected) <= 3.0,
                 qPrintable(QStringLiteral("Kurve liegt nicht auf ihrer -120-dBm-Linie: "
                                           "y=%1 statt %2")
                                .arg(pr.traceY, 0, 'f', 1).arg(expected, 0, 'f', 1)));
#endif
    }
};

QTEST_MAIN(TstSpectrumTraceOnGrid)
#include "tst_spectrum_trace_on_grid.moc"
