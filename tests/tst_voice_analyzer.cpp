// Voice analysis, checked against signals whose answer is known.
//
// The point of doing this with measurement rather than a model is that
// the answer can be checked. So it is: a tone lands in its own band, a
// clipped recording says so instead of reporting a confident spectrum
// of its own harmonics, hum is found at the right mains frequency, and
// the EQ suggestion never boosts.
//
// That last one is the whole design. An automatic EQ that reaches for a
// target by boosting will happily add 20 dB to a band where the
// microphone produces nothing but hiss, and the operator hears the
// result and concludes the feature is broken.
// no-port-check: NereusSDR-original.

#include <QtTest/QtTest>

#include "core/VoiceAnalyzer.h"
#include "models/TransmitModel.h"

#include <cmath>
#include <random>
#include <vector>

using namespace Longpath;

namespace {

constexpr int kRate = 48000;

// Speech-like: a few seconds of shaped noise, gated into words and
// pauses so the pause detector has something to find.
std::vector<float> speechLike(double seconds, double tilt = 0.0,
                              double hz = 0.0, double humAmp = 0.0,
                              double humHz = 50.0)
{
    const int n = int(seconds * kRate);
    std::vector<float> out(static_cast<size_t>(n), 0.0f);
    std::mt19937 rng(12345);
    std::normal_distribution<double> gauss(0.0, 0.15);

    double lp = 0.0;
    for (int i = 0; i < n; ++i) {
        // Words of 400 ms with 150 ms gaps.
        const double t = double(i) / kRate;
        const bool speaking = std::fmod(t, 0.55) < 0.40;

        double v = 0.0;
        if (speaking) {
            if (hz > 0.0) {
                v = 0.3 * std::sin(2.0 * M_PI * hz * t);
            } else {
                const double white = gauss(rng);
                // One-pole low pass gives a falling spectrum; `tilt`
                // moves the corner so a "bass-heavy" voice can be made.
                const double a = 0.85 + tilt;
                lp = a * lp + (1.0 - a) * white;
                v = lp * 4.0;
            }
        }
        if (humAmp > 0.0) { v += humAmp * std::sin(2.0 * M_PI * humHz * t); }
        out[static_cast<size_t>(i)] = float(std::clamp(v, -1.0, 1.0));
    }
    return out;
}

} // namespace

class TstVoiceAnalyzer : public QObject {
    Q_OBJECT
private slots:
    void the_target_curve_is_anchored_at_one_kilohertz();
    void the_target_curve_falls_away_outside_the_passband();
    void the_suggestion_never_boosts_a_band();
    void the_suggestion_stays_inside_what_the_eq_can_do();
    void an_empty_band_does_not_decide_the_whole_curve();
    void a_flat_input_needs_the_target_shape();
    void a_tone_lands_in_its_own_band();
    void a_bass_heavy_voice_is_told_so();
    void mains_hum_is_found_at_the_right_frequency();
    void clipping_is_reported_before_anything_else();
    void too_little_speech_refuses_rather_than_guesses();
    void nothing_at_all_is_handled();
};

void TstVoiceAnalyzer::the_target_curve_is_anchored_at_one_kilohertz()
{
    QCOMPARE(VoiceAnalyzer::targetDb(1000.0), 0.0);
    // Intelligibility lives above 1 kHz, so the curve rises there.
    QVERIFY(VoiceAnalyzer::targetDb(2000.0) > 0.0);
}

void TstVoiceAnalyzer::the_target_curve_falls_away_outside_the_passband()
{
    // Below 200 Hz and above 3 kHz the transmit filter removes it, but
    // only after the compressor has already worked on it.
    QVERIFY(VoiceAnalyzer::targetDb(63.0)  < -12.0);
    QVERIFY(VoiceAnalyzer::targetDb(125.0) < -6.0);
    QVERIFY(VoiceAnalyzer::targetDb(8000.0) < -6.0);

    // Monotonic through the low end: no dip that would leave a band
    // sticking up between two cuts.
    double prev = -1e9;
    for (double hz : {20.0, 63.0, 125.0, 250.0, 500.0, 1000.0}) {
        const double db = VoiceAnalyzer::targetDb(hz);
        QVERIFY2(db >= prev, qPrintable(QStringLiteral("%1 Hz").arg(hz)));
        prev = db;
    }
}

void TstVoiceAnalyzer::the_suggestion_never_boosts_a_band()
{
    // The failure this rule exists for: a microphone that produces
    // almost nothing at 16 kHz, and an analyser that "corrects" it by
    // adding 20 dB of hiss.
    std::array<double, kVoiceBandCount> measured{};
    measured.fill(-60.0);
    measured[5] = 0.0;          // only the 1 kHz band has anything

    std::array<double, kVoiceBandCount> eq{};
    double preamp = 0.0;
    VoiceAnalyzer::suggestEq(measured, eq, preamp);

    for (int i = 0; i < kVoiceBandCount; ++i) {
        QVERIFY2(eq[static_cast<size_t>(i)] <= 0.0,
                 qPrintable(QStringLiteral("band %1 boosted by %2 dB")
                                .arg(kVoiceBandHz[static_cast<size_t>(i)])
                                .arg(eq[static_cast<size_t>(i)])));
    }
    // The level the cuts removed comes back as make-up gain, not as
    // boost per band.
    QVERIFY(preamp >= 0.0);
}

void TstVoiceAnalyzer::the_suggestion_stays_inside_what_the_eq_can_do()
{
    // A wild measurement must produce a settable answer, not one the
    // control silently clamps somewhere else.
    std::array<double, kVoiceBandCount> measured{};
    for (int i = 0; i < kVoiceBandCount; ++i) {
        measured[static_cast<size_t>(i)] = (i % 2) ? 40.0 : -40.0;
    }
    std::array<double, kVoiceBandCount> eq{};
    double preamp = 0.0;
    VoiceAnalyzer::suggestEq(measured, eq, preamp);

    for (double g : eq) {
        QVERIFY(g >= double(TransmitModel::kTxEqBandDbMin));
        QVERIFY(g <= double(TransmitModel::kTxEqBandDbMax));
    }
    QVERIFY(preamp >= double(TransmitModel::kTxEqPreampDbMin));
    QVERIFY(preamp <= double(TransmitModel::kTxEqPreampDbMax));
}

void TstVoiceAnalyzer::an_empty_band_does_not_decide_the_whole_curve()
{
    // The bug the obvious version of suggestEq has, and the reason this
    // test exists at all. A realistic voice measured against 1 kHz sits
    // 45 dB down at 16 kHz. Normalising by the largest shortfall makes
    // that empty band the reference, and every real band is then cut to
    // the floor — the advice for an ordinary voice was "every slider at
    // minimum".
    const std::array<double, kVoiceBandCount> voice = {
        -45.0, -35.0, -5.0, 3.0, 2.0, 0.0, -4.0, -15.0, -30.0, -45.0
    };
    std::array<double, kVoiceBandCount> eq{};
    double preamp = 0.0;
    VoiceAnalyzer::suggestEq(voice, eq, preamp);

    int atFloor = 0;
    for (int i = 2; i <= 6; ++i) {          // the speech bands
        if (eq[static_cast<size_t>(i)] <= TransmitModel::kTxEqBandDbMin + 0.1) {
            ++atFloor;
        }
    }
    QVERIFY2(atFloor < 5, "every speech band was cut to the floor");

    // The shape is what was asked for: bass down, presence left alone.
    QVERIFY2(eq[3] < eq[6],
             qPrintable(QStringLiteral("250 Hz %1, 2 kHz %2")
                            .arg(eq[3]).arg(eq[6])));

    // And bands with nothing in them are left where they are rather
    // than being cut for the sake of it.
    QCOMPARE(eq[9], 0.0);   // 16 kHz, 45 dB down
    QCOMPARE(eq[0], 0.0);   // 32 Hz
}

void TstVoiceAnalyzer::a_flat_input_needs_the_target_shape()
{
    // Measure flat, and the suggestion should be the target curve
    // shifted down to its own maximum — the shape, expressed as cuts.
    std::array<double, kVoiceBandCount> flat{};
    flat.fill(0.0);
    std::array<double, kVoiceBandCount> eq{};
    double preamp = 0.0;
    VoiceAnalyzer::suggestEq(flat, eq, preamp);

    // 2 kHz is the highest point of the target, so it should be the
    // band that is cut least.
    const int at2k = 6;
    for (int i = 0; i < kVoiceBandCount; ++i) {
        QVERIFY(eq[static_cast<size_t>(i)] <= eq[static_cast<size_t>(at2k)]);
    }
    // And the low end should be cut hard.
    QVERIFY(eq[1] < -12.0 + 1.0);   // 63 Hz, at or near the floor
}

void TstVoiceAnalyzer::a_tone_lands_in_its_own_band()
{
    const std::vector<float> s = speechLike(6.0, 0.0, 1000.0);
    const VoiceAnalysis a =
        VoiceAnalyzer::analyse(s.data(), int(s.size()), kRate);
    QVERIFY2(a.valid, qPrintable(a.problem));

    // Everything is measured against the 1 kHz band, so a 1 kHz tone
    // makes every other band far below it.
    for (int i = 0; i < kVoiceBandCount; ++i) {
        if (i == 5) { continue; }
        QVERIFY2(a.bandDb[static_cast<size_t>(i)] < -20.0,
                 qPrintable(QStringLiteral("band %1 at %2 dB")
                                .arg(kVoiceBandHz[static_cast<size_t>(i)])
                                .arg(a.bandDb[static_cast<size_t>(i)])));
    }
}

void TstVoiceAnalyzer::a_bass_heavy_voice_is_told_so()
{
    const std::vector<float> s = speechLike(8.0, 0.14);   // heavy low pass
    const VoiceAnalysis a =
        VoiceAnalyzer::analyse(s.data(), int(s.size()), kRate);
    QVERIFY2(a.valid, qPrintable(a.problem));

    // The low bands should sit above the 1 kHz reference…
    QVERIFY2(a.bandDb[3] > 0.0,
             qPrintable(QStringLiteral("250 Hz at %1 dB").arg(a.bandDb[3])));
    // …and the advice should say so in words, not only in numbers.
    QVERIFY(a.findings.join(QLatin1Char(' '))
                .contains(QStringLiteral("Bass"), Qt::CaseInsensitive));
    // The suggested EQ should cut the low end harder than the mid.
    QVERIFY(a.suggestedEqDb[3] < a.suggestedEqDb[6]);
}

void TstVoiceAnalyzer::mains_hum_is_found_at_the_right_frequency()
{
    for (double humHz : {50.0, 60.0}) {
        const std::vector<float> s = speechLike(8.0, 0.0, 0.0, 0.05, humHz);
        const VoiceAnalysis a =
            VoiceAnalyzer::analyse(s.data(), int(s.size()), kRate);
        QVERIFY2(a.valid, qPrintable(a.problem));
        QCOMPARE(a.humBaseHz, int(humHz));
        QVERIFY2(a.findings.join(QLatin1Char(' '))
                     .contains(QStringLiteral("hum"), Qt::CaseInsensitive),
                 qPrintable(a.findings.join(QLatin1Char('|'))));
    }
}

void TstVoiceAnalyzer::clipping_is_reported_before_anything_else()
{
    // A clipped recording produces harmonics that read as a bright
    // voice. Reporting a confident EQ curve for it would send the
    // operator to fix the wrong thing.
    std::vector<float> s = speechLike(6.0);
    for (size_t i = 0; i < s.size(); i += 50) { s[i] = 1.0f; }

    const VoiceAnalysis a =
        VoiceAnalyzer::analyse(s.data(), int(s.size()), kRate);
    QVERIFY(a.clippedSamples > 0);
    QVERIFY(!a.findings.isEmpty());
    QVERIFY2(a.findings.first().contains(QStringLiteral("Clipping")),
             qPrintable(a.findings.first()));
}

void TstVoiceAnalyzer::too_little_speech_refuses_rather_than_guesses()
{
    const std::vector<float> s = speechLike(1.0);
    const VoiceAnalysis a =
        VoiceAnalyzer::analyse(s.data(), int(s.size()), kRate);
    QVERIFY(!a.valid);
    QVERIFY(!a.problem.isEmpty());
}

void TstVoiceAnalyzer::nothing_at_all_is_handled()
{
    const VoiceAnalysis a = VoiceAnalyzer::analyse(nullptr, 0, kRate);
    QVERIFY(!a.valid);
    QVERIFY(!a.problem.isEmpty());

    const std::vector<float> silence(kRate * 6, 0.0f);
    const VoiceAnalysis b =
        VoiceAnalyzer::analyse(silence.data(), int(silence.size()), kRate);
    QVERIFY(!b.valid);
}

QTEST_APPLESS_MAIN(TstVoiceAnalyzer)
#include "tst_voice_analyzer.moc"
