// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_resync_does_not_click.cpp  (Longpath)
// =================================================================
// Zweimal je Sekunde alle Einstellungen neu schreiben -- hoert man
// das?
//
// Der Mithoerer hat einen eigenen Kanalzug und holt sich die
// Einstellung des Betreibers im halbsekuendigen Takt
// (StripLiveMonitor::doResync -> StripSettings::restore). Am Geraet
// kam zurueck: „es ist wieder ein komische modulation im
// lautsprecher [...] wie ueberschlagen."
//
// Der Verdacht lag auf diesem Takt, und Verdacht ist kein Befund.
// Dieser Prueftand schickt einen Dauerton durch die Kette und
// schreibt dabei im selben Takt die Einstellungen neu. Ein Knacken
// ist ein Sprung von einem Abtastwert zum naechsten, der groesser
// ist, als der Ton selbst je macht -- das laesst sich zaehlen.
//
// Modification history (Longpath):
//   2026-10-09 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include "core/strip/StripChain.h"
#include "core/strip/StripSettings.h"
#include "core/strip/EqBandLayout.h"

#include <cmath>
#include <vector>

using namespace Longpath;

namespace {

constexpr int kRate  = 48000;
constexpr int kBlock = 64;

struct Run {
    double worstStep{0.0};   // groesster Sprung zwischen zwei Werten
    int    jumps{0};         // wie oft deutlich ueber dem Erwarteten
};

// Die Kette so bestuecken, wie sie beim Betreiber stand.
void arm(StripChain& c)
{
    c.prepare(double(kRate));
    c.setEnabled(true);
    c.setStageEnabled(StripChain::Stage::Gate, true);
    c.setStageEnabled(StripChain::Stage::Eq, true);
    c.setStageEnabled(StripChain::Stage::Comp, true);
    c.setStageEnabled(StripChain::Stage::Limiter, true);
}

Run runSine(StripChain& c, int resyncEveryBlocks)
{
    // Zwei Sekunden Dauerton. Der natuerliche Schritt eines 700-Hz-
    // Sinus bei 48 kHz ist hoechstens 2*pi*700/48000 = 0,0916 der
    // Amplitude; alles deutlich darueber kann nur eine Naht sein.
    const int blocks = 2 * kRate / kBlock;
    const double amp = 0.3;
    double phase = 0.0;
    const double step = 2.0 * M_PI * 700.0 / kRate;

    Run out;
    float previous = 0.0f;
    bool  havePrevious = false;
    std::vector<float> buf(kBlock);

    for (int b = 0; b < blocks; ++b) {
        for (int i = 0; i < kBlock; ++i) {
            buf[static_cast<size_t>(i)] = float(amp * std::sin(phase));
            phase += step;
        }
        c.processMono(buf.data(), kBlock);

        // Die ersten 200 ms sind Einschwingen (Gate oeffnet,
        // Kompressor faengt sich) und zaehlen nicht.
        const bool settled = b > (kRate / kBlock) / 5;
        for (int i = 0; i < kBlock; ++i) {
            const float v = buf[static_cast<size_t>(i)];
            if (havePrevious && settled) {
                const double d = std::abs(double(v) - double(previous));
                out.worstStep = std::max(out.worstStep, d);
                if (d > 0.02) { ++out.jumps; }
            }
            previous = v;
            havePrevious = true;
        }

        if (resyncEveryBlocks > 0 && b % resyncEveryBlocks == 0) {
            StripSettings::restore(c);
            c.setEnabled(true);
        }
    }
    return out;
}

} // namespace

class TstResyncDoesNotClick : public QObject
{
    Q_OBJECT
private slots:

    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    // Erst der Massstab: derselbe Ton durch dieselbe Kette, ohne dass
    // jemand dazwischenschreibt. Was hier herauskommt, ist das, was
    // die Kette selbst tut.
    void measureBoth()
    {
        StripChain quiet;
        arm(quiet);
        StripSettings::save(quiet);          // eine Einstellung ablegen
        const Run without = runSine(quiet, 0);

        StripChain busy;
        arm(busy);
        const Run with = runSine(busy, 375);  // 375 Bloecke = 500 ms

        // Und derselbe Lauf mit dem SPIEGELN, zehnmal in der Sekunde
        // -- so laeuft es seit dem 2026-10-09 abends wirklich.
        StripChain src;
        arm(src);
        EqBandLayout::ensureSeeded(src.eq());
        StripChain mirror;
        arm(mirror);
        {
            const int blocks = 2 * kRate / kBlock;
            double phase = 0.0;
            const double stepP = 2.0 * M_PI * 700.0 / kRate;
            std::vector<float> buf(kBlock);
            float previous = 0.0f;
            bool havePrevious = false;
            int jumps = 0;
            double worst = 0.0;
            for (int b = 0; b < blocks; ++b) {
                for (int i = 0; i < kBlock; ++i) {
                    buf[static_cast<size_t>(i)] = float(0.3 * std::sin(phase));
                    phase += stepP;
                }
                mirror.processMono(buf.data(), kBlock);
                const bool settled = b > (kRate / kBlock) / 5;
                for (int i = 0; i < kBlock; ++i) {
                    const float v = buf[static_cast<size_t>(i)];
                    if (havePrevious && settled) {
                        const double d = std::abs(double(v) - double(previous));
                        worst = std::max(worst, d);
                        if (d > 0.02) { ++jumps; }
                    }
                    previous = v;
                    havePrevious = true;
                }
                if (b % 75 == 0) {   // 75 Bloecke = 100 ms
                    StripSettings::copyParameters(src, mirror);
                    mirror.setEnabled(true);
                }
            }
            qInfo() << "mit  Spiegeln (10 Hz): groesster Schritt" << worst
                    << "Spruenge" << jumps;
            QVERIFY2(jumps == 0,
                     qPrintable(QStringLiteral("das Spiegeln erzeugt %1 "
                                               "Nahtstellen").arg(jumps)));
        }

        qInfo() << "ohne Abgleich: groesster Schritt" << without.worstStep
                << "Spruenge" << without.jumps;
        qInfo() << "mit  Abgleich: groesster Schritt" << with.worstStep
                << "Spruenge" << with.jumps;

        // Der Befund vom 2026-10-09: der Abgleich ist UNSCHULDIG.
        // Beide Laeufe liefern denselben groessten Schritt und null
        // Nahtstellen. Die Modulation im Lautsprecher kam von zwei
        // anderen Stellen -- der Hauptschalter war nie mit dem
        // Mithoeren verbunden (es lief also durch Gate und Kompressor,
        // obwohl die Kette AUS stand), und der Uhrenausgleich warf
        // ganze Bloecke weg. Beides behoben; dieser Fall bleibt als
        // Wache stehen, damit der Abgleich nicht eines Tages doch
        // anfaengt zu knacken.
        QVERIFY2(with.jumps <= without.jumps,
                 qPrintable(QStringLiteral(
                     "der halbsekuendige Abgleich erzeugt %1 Nahtstellen, "
                     "ohne ihn sind es %2 -- das ist das Knacken, das im "
                     "Lautsprecher zu hoeren war")
                     .arg(with.jumps).arg(without.jumps)));
    }
};

QTEST_MAIN(TstResyncDoesNotClick)
#include "tst_resync_does_not_click.moc"
