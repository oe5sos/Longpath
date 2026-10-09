// Vier Sendekanaele und was die Messung dazu sagt.
//
// Dieser Prueftand hatte einmal siebzehn Faelle und pruefte eine
// Automatik, die aus der Messung ganze Ketten herleitete. Die ist am
// 2026-10-09 entfallen -- nach einem Tag Gegenproben gegen die Stimme
// des Betreibers, die jedes Mal ergaben, dass seine eigene
// Einstellung besser klang. Was geblieben ist, ist klein, und der
// Prueftand ist es auch.
//
// Was hier noch zaehlt:
//   - der Kanal oeffnet sich an BEIDEN Enden, Breite fuer Breite;
//   - das Kanalfilter ist ein echter Butterworth, flach bis zur Ecke;
//   - eine Messung, die nicht trägt, erzeugt gar nichts.
// no-port-check: Longpath-original.

#include <QtTest/QtTest>

#include "core/Loudness.h"
#include "core/VoiceAnalyzer.h"
#include "core/strip/BroadcastOptimizer.h"
#include "core/strip/StripChain.h"

#include <cmath>
#include <vector>

using namespace Longpath;

namespace {

VoiceAnalysis ordinary()
{
    VoiceAnalysis a;
    a.valid = true;
    a.analysedSeconds = 9.0;
    a.speechFraction  = 0.6;
    a.bandDb = {-30.0, -20.0, -6.0, -2.0, -1.0, 0.0, 1.0, -6.0, -18.0, -30.0};
    a.suggestedEqDb = {0.0, 0.0, -4.0, -6.0, -3.0, 0.0, -1.0, 0.0, 0.0, 0.0};
    a.suggestedPreampDb = 4.0;
    a.noiseFloorDbFs = -62.0;
    a.speechDbFs     = -20.0;
    a.crestFactorDb  = 14.0;
    a.humDb          = -45.0;
    a.humBaseHz      = 50;
    a.sibilanceDb    = 1.0;
    a.sibilanceHz    = 6300.0;
    a.pitchHz        = 110.0;
    a.rolloffHz      = 6000.0;
    a.samplePeakDbFs = -6.0;
    a.loudnessLufs   = -23.0;
    a.loudnessRangeLu = 5.0;
    a.loudnessRangeValid = true;
    return a;
}

double toneLevel(const std::vector<float>& x, double hz, int rate)
{
    const double w = 2.0 * M_PI * hz / rate;
    const double c = 2.0 * std::cos(w);
    double s1 = 0.0, s2 = 0.0;
    for (float v : x) {
        const double s0 = double(v) + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - c * s1 * s2))
         / x.size();
}

} // namespace

class TstBroadcastOptimizer : public QObject {
    Q_OBJECT

private slots:

    // ── Die K-Gewichtung, gegen die veroeffentlichte Tabelle ─────────
    //
    // Loudness.cpp rechnet die Koeffizienten aus den
    // zeitkontinuierlichen Entwurfsgroessen, damit sie bei jeder
    // Abtastrate gelten. Also muss die Herleitung selbst belegt
    // werden -- und das geht nur gegen die 48-kHz-Tabelle von
    // BS.1770. Der erste Versuch nahm die RBJ-Kochbuchform und
    // verfehlte sie in der dritten Stelle.
    void kWeightingMatchesPublished48k()
    {
        const Loudness::Biquad shelf = Loudness::shelfAt(48000.0);
        QVERIFY(std::abs(shelf.b0 - 1.53512485958697)  < 1e-9);
        QVERIFY(std::abs(shelf.b1 - -2.69169618940638) < 1e-9);
        QVERIFY(std::abs(shelf.b2 - 1.19839281085285)  < 1e-9);
        QVERIFY(std::abs(shelf.a1 - -1.69065929318241) < 1e-9);
        QVERIFY(std::abs(shelf.a2 - 0.73248077421585)  < 1e-9);

        const Loudness::Biquad rlb = Loudness::rlbAt(48000.0);
        QVERIFY(std::abs(rlb.b0 - 1.0)  < 1e-6);
        QVERIFY(std::abs(rlb.b1 - -2.0) < 1e-6);
        QVERIFY(std::abs(rlb.b2 - 1.0)  < 1e-6);
        QVERIFY(std::abs(rlb.a1 - -1.99004745483398) < 1e-6);
        QVERIFY(std::abs(rlb.a2 - 0.99007225036621)  < 1e-6);
    }

    void loudnessTracksGain()
    {
        const auto sine = [](double amp) {
            std::vector<float> v(48000 * 4);
            for (size_t i = 0; i < v.size(); ++i) {
                v[i] = float(amp * std::sin(2.0 * M_PI * 1000.0
                                            * double(i) / 48000.0));
            }
            return v;
        };
        const std::vector<float> loud  = sine(0.5);
        const std::vector<float> quiet = sine(0.25);
        const Loudness::Result a =
            Loudness::measure(loud.data(), int(loud.size()), 48000);
        const Loudness::Result b =
            Loudness::measure(quiet.data(), int(quiet.size()), 48000);
        QVERIFY(a.valid && b.valid);
        QVERIFY(std::abs((a.integratedLufs - b.integratedLufs) - 6.0206) < 0.01);
    }

    void silenceIsNotAMeasurement()
    {
        const std::vector<float> quiet(48000 * 4, 0.0f);
        const Loudness::Result r =
            Loudness::measure(quiet.data(), int(quiet.size()), 48000);
        QVERIFY(r.valid);
        QVERIFY(!r.rangeValid);
        QVERIFY(r.integratedLufs <= Loudness::kAbsoluteGateLufs);
    }

    // ── Der Kanal ────────────────────────────────────────────────────

    void fourChannelsOpeningAtBothEnds()
    {
        const QVector<BroadcastOptimizer::Plan> p =
            BroadcastOptimizer::plan(ordinary());
        QCOMPARE(p.size(), BroadcastOptimizer::kWidthCount);

        // Die Zahlen des Betreibers: Contest 200-2500, breit 50-4050.
        QCOMPARE(p[0].lowCutHz,   200);
        QCOMPARE(p[0].highCutHz, 2500);
        QCOMPARE(p[3].lowCutHz,    50);
        QCOMPARE(p[3].highCutHz, 4050);

        // Und der Kanal oeffnet sich an BEIDEN Enden, Zeile fuer
        // Zeile. Lag der Tiefschnitt einmal bei allen vier fest auf
        // 150 Hz, verlor das breite Profil unten dasselbe wie das
        // schmale -- und klang wie dieses.
        for (int i = 1; i < p.size(); ++i) {
            QVERIFY2(p[i].lowCutHz < p[i - 1].lowCutHz,
                     qPrintable(QStringLiteral("%1 schneidet unten nicht "
                                               "tiefer als %2")
                         .arg(p[i].name, p[i - 1].name)));
            QVERIFY(p[i].highCutHz > p[i - 1].highCutHz);
        }

        for (const BroadcastOptimizer::Plan& one : p) {
            QVERIFY(!one.headline.isEmpty());
            QVERIFY(!one.notes.isEmpty());
        }
    }

    // Nichts wird aus einer Messung gebaut, die nicht traegt.
    void refusesWhatItCannotTrust()
    {
        VoiceAnalysis invalid = ordinary();
        invalid.valid = false;
        QVERIFY(BroadcastOptimizer::plan(invalid).isEmpty());

        VoiceAnalysis clipped = ordinary();
        clipped.clippedSamples = 1;
        QVERIFY(BroadcastOptimizer::plan(clipped).isEmpty());
    }

    void bestIndexPicksTheNearestWidth()
    {
        const QVector<BroadcastOptimizer::Plan> p =
            BroadcastOptimizer::plan(ordinary());
        QCOMPARE(BroadcastOptimizer::bestIndex(p, 2400.0), 0);
        QCOMPARE(BroadcastOptimizer::bestIndex(p, 2800.0), 1);
        QCOMPARE(BroadcastOptimizer::bestIndex(p, 3300.0), 2);
        QCOMPARE(BroadcastOptimizer::bestIndex(p, 9000.0), 3);
        QCOMPARE(BroadcastOptimizer::bestIndex({}, 2700.0), -1);
    }

    // ── Das Kanalfilter ──────────────────────────────────────────────
    //
    // Der Fall, der "das 4k profil hoert sich an als waeren es 2,7"
    // gefangen haette. Zwei zweite Ordnungen mit demselben
    // Guetefaktor sind kein Butterworth vierter Ordnung: -6 dB schon
    // AUF der Ecke und Abfall eine Oktave darunter. Bei 4000 Hz faellt
    // so etwas ab 2 kHz.
    void theChannelFilterIsFlatToItsCorner()
    {
        constexpr int kRate = 48000;
        const QVector<BroadcastOptimizer::Plan> p =
            BroadcastOptimizer::plan(ordinary());

        const auto response = [&](const BroadcastOptimizer::Plan& plan,
                                  double hz) {
            std::vector<float> x(kRate);
            for (int i = 0; i < kRate; ++i) {
                x[static_cast<size_t>(i)] =
                    float(0.05 * std::sin(2.0 * M_PI * hz * i / kRate));
            }
            std::vector<float> y = x;
            BroadcastOptimizer::applyChannelFilter(y.data(), int(y.size()),
                                                   kRate, plan);
            return 20.0 * std::log10(
                std::max(1e-12, toneLevel(y, hz, kRate))
              / std::max(1e-12, toneLevel(x, hz, kRate)));
        };

        const BroadcastOptimizer::Plan& wide = p[3];   // 50 - 4050
        // Eine Oktave unter der Ecke muss der Durchlass praktisch
        // unberuehrt sein -- genau das konnte die alte Fassung nicht.
        QVERIFY2(response(wide, 2000.0) > -1.0,
                 qPrintable(QStringLiteral("2 kHz liegt %1 dB unten")
                     .arg(response(wide, 2000.0), 0, 'f', 1)));
        // Und oberhalb faellt es wirklich.
        QVERIFY(response(wide, 8000.0) < -20.0);

        // Das schmale laesst bei 3,5 kHz deutlich weniger durch als
        // das breite. Ohne diesen Unterschied klingen die vier gleich.
        const BroadcastOptimizer::Plan& narrow = p[0];  // 200 - 2500
        QVERIFY2(response(narrow, 3500.0) < response(wide, 3500.0) - 10.0,
                 "schmal und breit unterscheiden sich oben nicht");
        // Und unten ebenso: 80 Hz ist der Unterschied zwischen 200 und
        // 50 Hz Tiefschnitt, also zwei Oktaven Koerper.
        QVERIFY2(response(narrow, 80.0) < response(wide, 80.0) - 10.0,
                 "schmal und breit unterscheiden sich unten nicht");
    }
};

QTEST_MAIN(TstBroadcastOptimizer)
#include "tst_broadcast_optimizer.moc"
