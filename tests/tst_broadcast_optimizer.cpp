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

        // The profile is for a channel, so it carries the channel.
        for (const BroadcastOptimizer::Plan& one : p) {
            QCOMPARE(double(one.highCutHz), one.widthHz);
            QVERIFY(one.lowCutHz > 0);
            QVERIFY(double(one.lowCutHz) < one.widthHz);
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

        // Der Hochpass ist KEIN Breitenwerkzeug mehr. Er sitzt unter
        // dem Grundton und raeumt Rumpeln weg, mehr nicht -- das
        // Ausduennen fuer den schmalen Kanal macht das Tiefenfilter.
        // Beides zu benutzen war genau das Stapeln, das der Stimme
        // den Koerper nahm (Betreiber, 2026-10-09). Er darf beim
        // schmalen Profil hoeher liegen, muss aber nicht.
        QVERIFY(narrow.highPassHz >= wide.highPassHz);

        // Schlanker unten ist jetzt Sache des Tiefenfilters.
        QVERIFY(narrow.lowShelfDb < wide.lowShelfDb);

        // Harder top cut, because the top is outside the channel.
        QVERIFY(narrow.highShelfDb < wide.highShelfDb);

        // More density, because punching through is the job.
        QVERIFY(narrow.density > wide.density);
        QVERIFY(narrow.compRatio > wide.compRatio);

        // And it forgets the stages that only cost it something.
        QVERIFY(!narrow.characterStages);
        QVERIFY(wide.characterStages);
        QVERIFY(!narrow.forgotten.isEmpty());
        QVERIFY(wide.forgotten.isEmpty());

        // Every plan explains itself.
        for (const BroadcastOptimizer::Plan& one : p) {
            QVERIFY(!one.notes.isEmpty());
        }
    }

    // The high-pass has to follow the voice, not a constant. Two
    // measurements identical but for the fundamental must not produce
    // the same filter — that was the entire reason for measuring it.
    // Der Grundton ist die OBERGRENZE des Hochpasses, nicht sein
    // Antrieb. Die alte Fassung multiplizierte ihn mit 1,15 bis 1,80
    // und nahm den hoeheren der beiden Werte -- bei einer 110-Hz-
    // Stimme landete ein Filter mit 24 dB/Oktave bei 198 Hz, also
    // mitten in der Stimme. Jetzt gewinnt der niedrigere.
    void thePitchCapsTheHighPass()
    {
        VoiceAnalysis low = ordinary();
        low.pitchHz = 95.0;
        const QVector<BroadcastOptimizer::Plan> a =
            BroadcastOptimizer::plan(low);
        for (const BroadcastOptimizer::Plan& p : a) {
            QVERIFY2(p.highPassHz < low.pitchHz,
                     qPrintable(QStringLiteral(
                         "%1: Hochpass %2 Hz liegt auf oder ueber dem "
                         "Grundton %3 Hz")
                         .arg(p.name).arg(p.highPassHz, 0, 'f', 0)
                         .arg(low.pitchHz, 0, 'f', 0)));
        }

        // Eine hoehere Stimme darf einen hoeheren Hochpass bekommen,
        // aber nie einen niedrigeren -- und nie ueber dem Deckel der
        // Breite.
        VoiceAnalysis high = ordinary();
        high.pitchHz = 185.0;
        const QVector<BroadcastOptimizer::Plan> b =
            BroadcastOptimizer::plan(high);
        for (int i = 0; i < a.size(); ++i) {
            QVERIFY(b[i].highPassHz >= a[i].highPassHz);
        }

        VoiceAnalysis veryHigh = ordinary();
        veryHigh.pitchHz = 320.0;
        const QVector<BroadcastOptimizer::Plan> c =
            BroadcastOptimizer::plan(veryHigh);
        QVERIFY(c[0].highPassHz <= 180.0);
        QVERIFY(c[3].highPassHz <= 100.0);
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

        QVERIFY(chain.stageEnabled(StripChain::Stage::Eq));
        QVERIFY(chain.stageEnabled(StripChain::Stage::Comp));
        QVERIFY(chain.stageEnabled(StripChain::Stage::Limiter));
        QVERIFY(!chain.stageEnabled(StripChain::Stage::Tube));
        QVERIFY(!chain.stageEnabled(StripChain::Stage::Reverb));
        QVERIFY(!chain.stageEnabled(StripChain::Stage::Pudu));

        // The high-pass really is where the plan said.
        const ClientEq::BandParams hp = chain.eq().band(0);
        QVERIFY(hp.enabled);
        QVERIFY(std::abs(double(hp.freqHz) - p[0].highPassHz) < 1.0);

        // And the master switch is NOT touched — the operator decides
        // when their transmit audio changes.
        QVERIFY(!chain.isEnabled());
    }

    // Applying a second plan must not leave the first one's notches
    // behind. This is the failure that would make an A/B between two
    // profiles a comparison of the wrong two things.
    void applyingTwiceLeavesNothingBehind()
    {
        VoiceAnalysis hummy = ordinary();
        hummy.humDb = -10.0;          // far too close — notches wanted
        hummy.humBaseHz = 50;

        StripChain chain;
        chain.prepare(48000.0);
        const QVector<BroadcastOptimizer::Plan> withHum =
            BroadcastOptimizer::plan(hummy);
        QVERIFY(BroadcastOptimizer::apply(withHum[1], chain));
        QVERIFY(chain.eq().band(1).enabled);

        const QVector<BroadcastOptimizer::Plan> clean =
            BroadcastOptimizer::plan(ordinary());
        QVERIFY(BroadcastOptimizer::apply(clean[1], chain));
        QVERIFY(!chain.eq().band(1).enabled);
        QVERIFY(!chain.eq().band(2).enabled);
        QVERIFY(!chain.eq().band(3).enabled);
    }

    // ── Klingen die vier wirklich verschieden? ───────────────────────
    //
    // Der Fall, der gefehlt hat. narrowIsLeanerThanWide() prueft die
    // ZAHLEN im Plan, und die waren immer richtig -- der Ton war es
    // nicht. Eine frische ClientEq hat activeBandCount 0, process()
    // steigt darauf sofort aus, also wurden alle vier Profile ohne
    // jeden Entzerrer gerechnet: kein Hochpass, kein Klang. Uebrig
    // blieben Kompressor und Begrenzer, und die gleicht der
    // Pegelabgleich beim Abhoeren fast weg.
    //
    // Betreiber am Geraet, 2026-10-09, in fuenf Worten: „alle profile
    // hören sich gleich an". Sie taten es.
    //
    // Deshalb misst dieser Fall am Ergebnis: derselbe Dreiklang durch
    // zwei Profile, und was unten und oben herauskommt, muss sich
    // unterscheiden -- und zwar in der behaupteten Richtung.
    void theProfilesActuallySoundDifferent()
    {
        constexpr int kRate = 48000;
        constexpr int kFrames = kRate;            // eine Sekunde

        // Leise (-40 dBFS je Ton), damit der Kompressor kaum eingreift
        // und der Entzerrer die Antwort bestimmt. Ein lauter Eingang
        // wuerde die Frage mit Pegelregelung beantworten.
        std::vector<float> input(kFrames, 0.0f);
        for (int i = 0; i < kFrames; ++i) {
            const double t = double(i) / kRate;
            input[static_cast<size_t>(i)] = float(0.01 * (
                  std::sin(2.0 * M_PI *   80.0 * t)
                + std::sin(2.0 * M_PI * 1000.0 * t)
                + std::sin(2.0 * M_PI * 4000.0 * t)));
        }

        const QVector<BroadcastOptimizer::Plan> plans =
            BroadcastOptimizer::plan(ordinary());
        QCOMPARE(plans.size(), 4);

        // Goertzel: Leistung bei einer Frequenz, ohne eine FFT dafuer
        // zu bauen.
        const auto toneLevel = [](const std::vector<float>& x,
                                  double hz) {
            const double w = 2.0 * M_PI * hz / kRate;
            const double c = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (float v : x) {
                const double s0 = double(v) + c * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            return std::sqrt(std::max(0.0,
                s1 * s1 + s2 * s2 - c * s1 * s2)) / x.size();
        };

        const auto render = [&](const BroadcastOptimizer::Plan& p) {
            StripChain chain;
            chain.prepare(double(kRate));
            BroadcastOptimizer::apply(p, chain);
            chain.setEnabled(true);
            std::vector<float> out = input;
            chain.processMono(out.data(), int(out.size()));
            return out;
        };

        const std::vector<float> narrow = render(plans[0]);   // 2.3k
        const std::vector<float> wide   = render(plans[3]);   // 4.0k

        // Gegen den eigenen 1-kHz-Ton gemessen, damit die
        // Aufholverstaerkung -- die sich je Profil unterscheidet und
        // beim Abhoeren ohnehin weggeglichen wird -- die Frage nicht
        // beantwortet.
        const double narrowHigh = toneLevel(narrow, 4000.0)
                                / std::max(1e-12, toneLevel(narrow, 1000.0));
        const double wideHigh   = toneLevel(wide,   4000.0)
                                / std::max(1e-12, toneLevel(wide,   1000.0));
        const double narrowLow  = toneLevel(narrow,   80.0)
                                / std::max(1e-12, toneLevel(narrow, 1000.0));
        const double wideLow    = toneLevel(wide,     80.0)
                                / std::max(1e-12, toneLevel(wide,   1000.0));

        // Das schmale Profil wirft oben weg, was ausserhalb seines
        // Kanals liegt. Mindestens 3 dB Unterschied, sonst ist es
        // keiner, den ein Ohr findet.
        QVERIFY2(narrowHigh < wideHigh * 0.7,
                 qPrintable(QStringLiteral(
                     "4 kHz gegen 1 kHz: schmal %1, breit %2 -- die "
                     "Profile klingen gleich")
                     .arg(narrowHigh, 0, 'f', 4).arg(wideHigh, 0, 'f', 4)));

        // Und unten nimmt es mehr weg, weil der Hochpass hoeher sitzt.
        QVERIFY2(narrowLow < wideLow * 0.7,
                 qPrintable(QStringLiteral(
                     "80 Hz gegen 1 kHz: schmal %1, breit %2 -- der "
                     "Hochpass wirkt nicht")
                     .arg(narrowLow, 0, 'f', 4).arg(wideLow, 0, 'f', 4)));
    }

    // ── Nimmt ein Profil der Stimme den Koerper? ─────────────────────
    //
    // Der Befund, der alles andere ausgeloest hat. Betreiber, 2026-10-09:
    // „Das Original-Audio ist gut, nur saemtliche Bearbeitungen sind
    // eine Katastrophe" und „hier ist kein Bass, hier fehlt es einfach
    // an allem."
    //
    // Entscheidend war ein zweiter Satz: „play mic hoert sich anders
    // an, als play after strip, hier passt alles sehr gut." Die
    // gespeicherten Strip-Einstellungen des Betreibers klingen durch
    // dieselbe Rechenkette gut -- schlecht waren ausschliesslich die
    // Zahlen, die dieser Code erzeugt.
    //
    // Gemessen wird an einer ECHT analysierten Stimme, nicht an
    // ordinary(): die Handwerte dort koennen die Kurve treffen, an der
    // der Entwurf gerade scheitert, und dann prueft der Fall nichts.
    void theWideProfileKeepsTheBody()
    {
        constexpr int kRate = 48000;
        constexpr int kSeconds = 6;
        constexpr int kFrames = kRate * kSeconds;

        // Eine stimmaehnliche Anregung: Grundton 110 Hz mit
        // Oberwellen, zwei Formanten, und Pausen dazwischen, damit
        // VoiceAnalyzer einen Rauschteppich messen kann.
        std::vector<float> input(kFrames, 0.0f);
        for (int i = 0; i < kFrames; ++i) {
            const double t = double(i) / kRate;
            const bool speaking = std::fmod(t, 1.2) < 0.9;
            if (!speaking) {
                // Sehr leise Pause: weit genug unter der Stimme, dass
                // Anheben erlaubt ist.
                input[static_cast<size_t>(i)] =
                    float(0.0002 * std::sin(2.0 * M_PI * 3000.0 * t));
                continue;
            }
            double v = 0.0;
            for (int h = 1; h <= 24; ++h) {
                const double f = 110.0 * h;
                if (f > 7000.0) { break; }
                // Fallende Oberwellen plus zwei Formantbuckel.
                double amp = 1.0 / h;
                if (f > 400.0 && f < 900.0)   { amp *= 2.0; }
                if (f > 1600.0 && f < 2600.0) { amp *= 1.6; }
                v += amp * std::sin(2.0 * M_PI * f * t + 0.7 * h);
            }
            input[static_cast<size_t>(i)] = float(0.08 * v);
        }

        const VoiceAnalysis a =
            VoiceAnalyzer::analyse(input.data(), kFrames, kRate);
        QVERIFY2(a.valid, qPrintable(a.problem));

        const QVector<BroadcastOptimizer::Plan> plans =
            BroadcastOptimizer::plan(a);
        QCOMPARE(plans.size(), 4);

        const auto toneLevel = [&](const std::vector<float>& x, double hz) {
            const double w = 2.0 * M_PI * hz / kRate;
            const double c = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (float v : x) {
                const double s0 = double(v) + c * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            return std::sqrt(std::max(0.0,
                s1 * s1 + s2 * s2 - c * s1 * s2)) / x.size();
        };
        const auto tiltDb = [&](const std::vector<float>& x, double hz) {
            return 20.0 * std::log10(
                std::max(1e-12, toneLevel(x, hz))
                / std::max(1e-12, toneLevel(x, 1100.0)));
        };

        const auto render = [&](const BroadcastOptimizer::Plan& p) {
            StripChain chain;
            chain.prepare(double(kRate));
            BroadcastOptimizer::apply(p, chain);
            chain.setEnabled(true);
            std::vector<float> out = input;
            chain.processMono(out.data(), int(out.size()));
            return out;
        };

        // 220 Hz: die zweite Oberwelle, der Koerper der Stimme. Sie
        // darf das breite Profil nicht verlassen.
        const double inBody = tiltDb(input, 220.0);
        for (const BroadcastOptimizer::Plan& p : plans) {
            const std::vector<float> out = render(p);
            const double outBody = tiltDb(out, 220.0);
            const double lost = inBody - outBody;
            qInfo("%s: Koerper bei 220 Hz %+.1f dB gegenueber dem Original",
                  qPrintable(p.name), -lost);

            // Das breite Profil ist das, bei dem „kein Bass" keine
            // Entwurfsentscheidung sein darf. Vier Dezibel sind
            // reichlich bemessen; es waren zehn und mehr.
            if (p.widthHz >= 3200.0) {
                QVERIFY2(lost < 4.0,
                         qPrintable(QStringLiteral(
                             "%1 nimmt der Stimme %2 dB bei 220 Hz")
                             .arg(p.name).arg(lost, 0, 'f', 1)));
            }
            // Und keines der vier darf den Koerper ganz wegnehmen.
            QVERIFY2(lost < 12.0,
                     qPrintable(QStringLiteral(
                         "%1 nimmt der Stimme %2 dB bei 220 Hz")
                         .arg(p.name).arg(lost, 0, 'f', 1)));
        }
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
