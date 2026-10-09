// One recording into four transmit bandwidths, and the ways that can
// be confidently wrong.
//
// Two things are checked here that cannot be checked by listening.
//
// The first is the K-weighting derivation. Loudness.cpp computes the
// filter from continuous-time design parameters rather than copying
// BS.1770's table, so that it works at any sample rate — which means
// the derivation itself has to be proved. It is proved against the
// published 48 kHz table: if the bilinear transform or any of the
// four design constants were wrong, it could not land on those five
// numbers by accident.
//
// The second is that the four profiles actually DIFFER in the ways
// they claim to. A width table is the easiest thing in the world to
// wire up so that every row produces the same chain, and the result
// would look entirely plausible in the window.
// no-port-check: Longpath-original.

#include <QtTest/QtTest>

#include "core/Loudness.h"
#include "core/VoiceAnalyzer.h"
#include "core/strip/BroadcastOptimizer.h"
#include "core/strip/ClientEq.h"
#include "core/strip/StripChain.h"

#include <cmath>
#include <vector>

using namespace Longpath;

namespace {

// An ordinary voice with no particular faults, same shape as the one
// tst_strip_tuner starts from. Tests spoil one thing at a time.
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
    a.clippedSamples = 0;
    a.pitchHz        = 110.0;     // an ordinary male speaking voice
    a.rolloffHz      = 6000.0;    // a microphone with room to spare
    a.loudnessLufs   = -23.0;
    a.loudnessRangeLu = 5.0;
    a.loudnessRangeValid = true;
    return a;
}

std::vector<float> sine(double hz, double seconds, int rate, double amp)
{
    std::vector<float> v(static_cast<size_t>(seconds * rate));
    for (size_t i = 0; i < v.size(); ++i) {
        v[i] = float(amp * std::sin(2.0 * M_PI * hz * double(i) / rate));
    }
    return v;
}

} // namespace

class TstBroadcastOptimizer : public QObject {
    Q_OBJECT

private slots:

    // ── The derivation, against the published table ──────────────────
    //
    // BS.1770's 48 kHz coefficients, as reproduced in the standard's
    // own table and in every implementation that copies it. Five
    // numbers each; the derivation has to reach them from four design
    // constants and a bilinear transform.
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

    // Halving the amplitude must move the answer by 6.02 LU and
    // nothing else. This is the cheapest possible proof that the
    // gating and the averaging are not quietly scaling anything.
    void loudnessTracksGain()
    {
        const std::vector<float> loud  = sine(1000.0, 4.0, 48000, 0.5);
        const std::vector<float> quiet = sine(1000.0, 4.0, 48000, 0.25);

        const Loudness::Result a =
            Loudness::measure(loud.data(), int(loud.size()), 48000);
        const Loudness::Result b =
            Loudness::measure(quiet.data(), int(quiet.size()), 48000);
        QVERIFY(a.valid);
        QVERIFY(b.valid);
        QVERIFY(std::abs((a.integratedLufs - b.integratedLufs) - 6.0206) < 0.01);
    }

    // A steady tone has no loudness range; a tone that steps 10 dB
    // every three seconds has about 10 LU of it. Both directions,
    // because a range measure that always returns zero passes the
    // first test on its own.
    void loudnessRangeSeesTheStep()
    {
        {
            const std::vector<float> steady = sine(440.0, 30.0, 48000, 0.3);
            const Loudness::Result r =
                Loudness::measure(steady.data(), int(steady.size()), 48000);
            QVERIFY(r.rangeValid);
            QVERIFY(r.rangeLu < 0.5);
        }
        {
            std::vector<float> stepped;
            for (int block = 0; block < 10; ++block) {
                const double amp = (block % 2 == 0) ? 0.30 : 0.0949;  // -10 dB
                const std::vector<float> part = sine(440.0, 3.0, 48000, amp);
                stepped.insert(stepped.end(), part.begin(), part.end());
            }
            const Loudness::Result r =
                Loudness::measure(stepped.data(), int(stepped.size()), 48000);
            QVERIFY(r.rangeValid);
            QVERIFY(r.rangeLu > 7.0);
            QVERIFY(r.rangeLu < 13.0);
        }
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

    // ── The four plans ───────────────────────────────────────────────

    void fourPlansInWidthOrder()
    {
        const QVector<BroadcastOptimizer::Plan> p =
            BroadcastOptimizer::plan(ordinary());
        QCOMPARE(p.size(), BroadcastOptimizer::kWidthCount);
        QCOMPARE(p[0].widthHz, 2300.0);
        QCOMPARE(p[1].widthHz, 2700.0);
        QCOMPARE(p[2].widthHz, 3200.0);
        QCOMPARE(p[3].widthHz, 4000.0);
        QCOMPARE(p[0].name, QStringLiteral("Broadcast 2.3k"));
        QCOMPARE(p[3].name, QStringLiteral("Broadcast 4.0k"));

        // Das Profil ist fuer einen Kanal, also traegt es den Kanal
        // -- oben UND unten. Die Zahlen sind die des Betreibers:
        // Contest 200-2500, das breite 50-4050.
        QCOMPARE(p[0].lowCutHz,   200);
        QCOMPARE(p[0].highCutHz, 2500);
        QCOMPARE(p[3].lowCutHz,    50);
        QCOMPARE(p[3].highCutHz, 4050);

        // Und der Kanal oeffnet sich an BEIDEN Enden, Zeile fuer
        // Zeile. Der Tiefschnitt lag einmal bei allen vier fest auf
        // 150 Hz -- dann verliert das breite Profil unten dasselbe
        // wie das schmale und klingt wie dieses.
        for (int i = 1; i < p.size(); ++i) {
            QVERIFY2(p[i].lowCutHz < p[i - 1].lowCutHz,
                     qPrintable(QStringLiteral("%1 schneidet unten nicht "
                                               "tiefer als %2")
                         .arg(p[i].name, p[i - 1].name)));
            QVERIFY(p[i].highCutHz > p[i - 1].highCutHz);
        }
    }

    // Nothing is planned from a measurement that is not real. Both
    // refusals, because they have separate causes and a single early
    // return could satisfy one while leaving the other open.
    void refusesWhatItCannotTrust()
    {
        VoiceAnalysis invalid = ordinary();
        invalid.valid = false;
        QVERIFY(BroadcastOptimizer::plan(invalid).isEmpty());

        VoiceAnalysis clipped = ordinary();
        clipped.clippedSamples = 1;
        QVERIFY(BroadcastOptimizer::plan(clipped).isEmpty());
    }

    // The point of the whole exercise: the four must differ, and
    // differ in the stated direction.
    void narrowIsLeanerThanWide()
    {
        const QVector<BroadcastOptimizer::Plan> p =
            BroadcastOptimizer::plan(ordinary());
        QCOMPARE(p.size(), 4);

        const BroadcastOptimizer::Plan& narrow = p[0];   // 2.3k
        const BroadcastOptimizer::Plan& wide   = p[3];   // 4.0k

        // Schlanker heisst jetzt: dichter und ohne Farbe, nicht
        // beschnitten. Der Klang bleibt unberuehrt -- siehe
        // applyTouchesNoEqualiser().
        QVERIFY(narrow.density > wide.density);
        QVERIFY(narrow.compRatio > wide.compRatio);
        QVERIFY(!narrow.characterStages);
        QVERIFY(wide.characterStages);
        QVERIFY(!narrow.forgotten.isEmpty());
        QVERIFY(wide.forgotten.isEmpty());

        // Die Breite selbst ist die kennzeichnende Aenderung.
        QVERIFY(narrow.highCutHz < wide.highCutHz);

        // Und der Pegel startet fuer alle vier gleich, damit ein A/B
        // zwischen ihnen die Verarbeitung vergleicht und nicht die
        // Lautstaerke.
        QCOMPARE(narrow.trimDb, wide.trimDb);

        // Every plan explains itself.
        for (const BroadcastOptimizer::Plan& one : p) {
            QVERIFY(!one.notes.isEmpty());
        }
    }

    // The high-pass has to follow the voice, not a constant. Two
    // measurements identical but for the fundamental must not produce
    // the same filter — that was the entire reason for measuring it.
    // ── Der Entzerrer wird nicht angefasst ───────────────────────────
    //
    // Der neue Vertrag, und der wichtigste. Jeder Fehler vom
    // 2026-10-09 -- Brummkerbe auf dem Grundton, Hochpass darueber,
    // zwei Schnitte uebereinander, De-Esser ohne Zischen -- war
    // dieser Code, der an einem Klangregler drehte. Zeus Link dreht
    // in keinem seiner drei Stile an einem.
    void applyTouchesNoEqualiser()
    {
        StripChain chain;
        chain.prepare(48000.0);

        // Eine Kurve, wie der Bediener sie hinterlassen haben koennte.
        ClientEq::BandParams mine = chain.eq().band(4);
        mine.type    = ClientEq::FilterType::LowShelf;
        mine.freqHz  = 180.0f;
        mine.gainDb  = 4.5f;
        mine.enabled = true;
        chain.eq().setBand(4, mine);
        chain.eq().setActiveBandCount(7);
        chain.setStageEnabled(StripChain::Stage::Eq, true);

        const int bandsBefore = chain.eq().activeBandCount();

        const QVector<BroadcastOptimizer::Plan> p =
            BroadcastOptimizer::plan(ordinary());
        QVERIFY(BroadcastOptimizer::apply(p[0], chain));

        const ClientEq::BandParams after = chain.eq().band(4);
        QCOMPARE(after.gainDb, 4.5f);
        QCOMPARE(after.freqHz, 180.0f);
        QVERIFY(after.enabled);
        QCOMPARE(chain.eq().activeBandCount(), bandsBefore);
        QVERIFY2(chain.stageEnabled(StripChain::Stage::Eq),
                 "der Entzerrer bleibt an, wenn er an war");

        // Und auch nicht eingeschaltet, wenn er aus war.
        StripChain off;
        off.prepare(48000.0);
        QVERIFY(BroadcastOptimizer::apply(p[0], off));
        QVERIFY2(!off.stageEnabled(StripChain::Stage::Eq),
                 "und aus, wenn er aus war");
    }

    // A microphone that stops before the channel does must be said
    // out loud, on the widths it affects and not on the others.
    void saysWhenTheWidthCannotBeDelivered()
    {
        VoiceAnalysis dull = ordinary();
        dull.rolloffHz = 2800.0;
        const QVector<BroadcastOptimizer::Plan> p =
            BroadcastOptimizer::plan(dull);
        QVERIFY(p[0].caveat.isEmpty());    // 2.3k — the microphone covers it
        QVERIFY(!p[2].caveat.isEmpty());   // 3.2k — it does not
        QVERIFY(!p[3].caveat.isEmpty());   // 4.0k — still less
    }

    // ── The density axis ─────────────────────────────────────────────

    void densityIsMonotonicAndNamed()
    {
        QVERIFY(BroadcastOptimizer::targetCrestDb(0)
              > BroadcastOptimizer::targetCrestDb(50));
        QVERIFY(BroadcastOptimizer::targetCrestDb(50)
              > BroadcastOptimizer::targetCrestDb(100));

        QCOMPARE(BroadcastOptimizer::densityName(
                     BroadcastOptimizer::kDensityBroadcast),
                 QStringLiteral("Broadcast"));
        // A hand-edited value still reads as the nearest stop rather
        // than as a bare number.
        QCOMPARE(BroadcastOptimizer::densityName(58),
                 QStringLiteral("Broadcast"));
        QCOMPARE(BroadcastOptimizer::densityName(0),
                 QStringLiteral("Natural"));
        QCOMPARE(BroadcastOptimizer::densityName(100),
                 QStringLiteral("Maximum"));
    }

    // ── Into a chain ─────────────────────────────────────────────────

    void applySetsTheChainAndForgetsWhatItSaidItWould()
    {
        StripChain chain;
        chain.prepare(48000.0);

        // Somebody had the character stages on before. The narrow
        // profile must take them out, not inherit them.
        chain.setStageEnabled(StripChain::Stage::Tube, true);
        chain.setStageEnabled(StripChain::Stage::Reverb, true);
        chain.setStageEnabled(StripChain::Stage::Pudu, true);

        const QVector<BroadcastOptimizer::Plan> p =
            BroadcastOptimizer::plan(ordinary());
        QVERIFY(BroadcastOptimizer::apply(p[0], chain));

        QVERIFY(chain.stageEnabled(StripChain::Stage::Comp));
        QVERIFY(chain.stageEnabled(StripChain::Stage::Limiter));
        QVERIFY(!chain.stageEnabled(StripChain::Stage::Tube));
        QVERIFY(!chain.stageEnabled(StripChain::Stage::Reverb));
        QVERIFY(!chain.stageEnabled(StripChain::Stage::Pudu));

        // Der Trimm steht als Vorverstaerkung des Kompressors da,
        // also vor dessen Detektor -- dort, wo eine Schwelle erst
        // eine Bedeutung bekommt.
        QVERIFY(std::abs(double(chain.comp().driveDb())
                         - std::clamp(p[0].trimDb, 0.0, 18.0)) < 0.1);

        // And the master switch is NOT touched — the operator decides
        // when their transmit audio changes.
        QVERIFY(!chain.isEnabled());
    }

    // Ein zweites Profil darf nichts vom ersten stehen lassen. Mit
    // dem Entzerrer aussen vor sind die Stufenschalter die Stelle,
    // an der das noch passieren kann.
    void applyingTwiceLeavesNothingBehind()
    {
        StripChain chain;
        chain.prepare(48000.0);

        const QVector<BroadcastOptimizer::Plan> p =
            BroadcastOptimizer::plan(ordinary());

        // Erst das schmale: es schaltet die Farbstufen ab.
        QVERIFY(BroadcastOptimizer::apply(p[0], chain));
        QVERIFY(!chain.stageEnabled(StripChain::Stage::Tube));

        // Dann das breite: es schaltet sie NICHT wieder ein -- dafuer
        // gibt es keine Messung.
        QVERIFY(BroadcastOptimizer::apply(p[3], chain));
        QVERIFY(!chain.stageEnabled(StripChain::Stage::Tube));

        // Aber seine eigenen Werte stehen jetzt da.
        QVERIFY(std::abs(double(chain.comp().ratio()) - p[3].compRatio) < 0.1);
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
};

QTEST_MAIN(TstBroadcastOptimizer)
#include "tst_broadcast_optimizer.moc"
