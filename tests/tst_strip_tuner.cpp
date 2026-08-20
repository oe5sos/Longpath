// One measurement into a whole chain, and the four ways that can be
// wrong without looking wrong.
//
// This is the piece that sets eight stages from numbers the operator
// never sees, so it is the piece most able to produce a confident bad
// answer. The decisions are pure functions for exactly that reason:
// each can be checked against a measurement whose right answer is
// known, rather than by listening to the result and hoping.
// no-port-check: NereusSDR-original.

#include <QtTest/QtTest>

#include "core/strip/StripChain.h"
#include "core/strip/StripTuner.h"

using namespace Longpath;

namespace {

// A measurement of an ordinary voice with no particular faults. Tests
// start from this and spoil one thing at a time, which is the only way
// to know which input drove which decision.
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
    a.humDb          = -45.0;      // 45 dB below speech — nothing to do
    a.humBaseHz      = 50;
    a.sibilanceDb    = 1.0;
    a.clippedSamples = 0;
    return a;
}

} // namespace

class TstStripTuner : public QObject {
    Q_OBJECT
private slots:
    void a_clipped_recording_changes_nothing();
    void an_invalid_measurement_changes_nothing();
    void it_never_switches_the_strip_on();
    void it_never_boosts_a_band();

    void the_high_pass_follows_the_measured_low_end();
    void the_gate_sits_between_the_floor_and_the_voice();
    void a_background_too_close_to_the_voice_leaves_the_gate_off();
    void the_ratio_follows_the_crest_factor();
    void quiet_hum_is_left_alone_and_loud_hum_is_notched();
    void no_sibilance_means_no_de_esser();
    void every_decision_is_explained();
};

// ── Refusals ─────────────────────────────────────────────────────────

void TstStripTuner::a_clipped_recording_changes_nothing()
{
    // The important refusal. A clipped recording's spectrum is the
    // spectrum of its own flat tops, so a chain set from it would be
    // confidently wrong in every stage at once — and every stage would
    // agree with every other, which is what makes it convincing.
    VoiceAnalysis a = ordinary();
    a.clippedSamples = 12;

    StripChain c;
    c.prepare(48000.0);
    const float thrBefore = c.gate().thresholdDb();

    const auto r = StripTuner::applyAnalysis(a, c);
    QVERIFY(!r.changed);
    QVERIFY(!r.notes.isEmpty());
    QVERIFY2(r.notes.first().contains(QStringLiteral("clipped")),
             "the refusal must say why");
    QCOMPARE(c.gate().thresholdDb(), thrBefore);
    QVERIFY(!c.stageEnabled(StripChain::Stage::Eq));
}

void TstStripTuner::an_invalid_measurement_changes_nothing()
{
    VoiceAnalysis a;
    a.valid = false;
    StripChain c;
    c.prepare(48000.0);
    const auto r = StripTuner::applyAnalysis(a, c);
    QVERIFY(!r.changed);
    QVERIFY(!c.stageEnabled(StripChain::Stage::Comp));
}

void TstStripTuner::it_never_switches_the_strip_on()
{
    // Same rule as the presets. The operator decides when their
    // transmit audio changes; a measurement does not get to.
    StripChain c;
    c.prepare(48000.0);
    QVERIFY(StripTuner::applyAnalysis(ordinary(), c).changed);
    QVERIFY2(!c.isEnabled(), "the tuner put the strip on the air");
}

void TstStripTuner::it_never_boosts_a_band()
{
    // Even when handed a suggestion that asks for boost. A chain that
    // reaches a target by boosting raises whatever noise was in that
    // band along with the voice.
    VoiceAnalysis a = ordinary();
    a.suggestedEqDb = {6.0, 6.0, 9.0, 9.0, 6.0, 0.0, 12.0, 6.0, 6.0, 6.0};

    StripChain c;
    c.prepare(48000.0);
    QVERIFY(StripTuner::applyAnalysis(a, c).changed);

    for (int i = 4; i <= 6; ++i) {          // the three tone bands
        QVERIFY2(c.eq().band(i).gainDb <= 0.0f,
                 qPrintable(QStringLiteral("band %1 was boosted to %2 dB")
                                .arg(i).arg(c.eq().band(i).gainDb)));
    }
}

// ── The decisions ────────────────────────────────────────────────────

void TstStripTuner::the_high_pass_follows_the_measured_low_end()
{
    VoiceAnalysis thin = ordinary();
    thin.bandDb[0] = -40.0; thin.bandDb[1] = -35.0;   // very little bottom

    VoiceAnalysis boomy = ordinary();
    boomy.bandDb[0] = 4.0; boomy.bandDb[1] = 8.0;     // more than 1 kHz

    const double thinHz  = StripTuner::highPassHz(thin);
    const double boomyHz = StripTuner::highPassHz(boomy);

    QVERIFY2(boomyHz > thinHz,
             "a bass-heavy voice must get a higher corner than a thin one");
    // Bounded at both ends: under 60 Hz the filter is doing nothing an
    // SSB transmitter would not do anyway, over 200 Hz it has started
    // removing the voice rather than what is under it.
    for (double hz : {thinHz, boomyHz}) {
        QVERIFY(hz >= 60.0);
        QVERIFY(hz <= 200.0);
    }
}

void TstStripTuner::the_gate_sits_between_the_floor_and_the_voice()
{
    const VoiceAnalysis a = ordinary();
    const double thr = StripTuner::gateThresholdDbFs(a);

    QVERIFY2(thr > a.noiseFloorDbFs,
             "a threshold at or below the floor never closes");
    QVERIFY2(thr < a.speechDbFs,
             "a threshold at or above the speech chops every word");

    // Nearer the floor than the speech, on purpose: the end of a word
    // is much quieter than its middle, and a threshold at the midpoint
    // eats the tail.
    const double mid = 0.5 * (a.noiseFloorDbFs + a.speechDbFs);
    QVERIFY2(thr < mid, "the threshold must sit low in the gap");
}

void TstStripTuner::a_background_too_close_to_the_voice_leaves_the_gate_off()
{
    // The case the bench actually produced: hum only a few decibels
    // under the speech. There is no threshold that separates them, and
    // a gate placed inside the overlap chops words — which sounds like
    // a failing connection and gets blamed on the radio.
    VoiceAnalysis a = ordinary();
    a.noiseFloorDbFs = -26.0;      // 6 dB below speech
    QVERIFY(a.speechDbFs - a.noiseFloorDbFs
            < StripTuner::kMinGateSeparationDb);

    StripChain c;
    c.prepare(48000.0);
    const auto r = StripTuner::applyAnalysis(a, c);
    QVERIFY(r.changed);
    QVERIFY2(!c.stageEnabled(StripChain::Stage::Gate),
             "the gate was enabled with nothing to gate against");

    bool explained = false;
    for (const QString& n : r.notes) {
        if (n.contains(QStringLiteral("Gate left off"))) { explained = true; }
    }
    QVERIFY2(explained, "leaving the gate off has to be said, not just done");
}

void TstStripTuner::the_ratio_follows_the_crest_factor()
{
    VoiceAnalysis even = ordinary();
    even.crestFactorDb = 8.0;
    VoiceAnalysis wild = ordinary();
    wild.crestFactorDb = 22.0;

    const double rEven = StripTuner::compressorRatio(even);
    const double rWild = StripTuner::compressorRatio(wild);

    QVERIFY2(rWild > rEven,
             "a more dynamic voice must get a higher ratio");
    // Bounded, so a wild measurement cannot turn the compressor into a
    // limiter behind the operator's back.
    for (double r : {rEven, rWild}) {
        QVERIFY(r >= 1.5);
        QVERIFY(r <= 6.0);
    }
}

void TstStripTuner::quiet_hum_is_left_alone_and_loud_hum_is_notched()
{
    VoiceAnalysis quiet = ordinary();          // 45 dB down
    QVERIFY(!StripTuner::humWorthNotching(quiet));

    VoiceAnalysis loud = ordinary();
    loud.humDb = -9.6;                          // the bench measurement
    loud.humBaseHz = 50;
    QVERIFY(StripTuner::humWorthNotching(loud));

    StripChain c;
    c.prepare(48000.0);
    QVERIFY(StripTuner::applyAnalysis(loud, c).changed);
    // Three notches, at the fundamental and its first two harmonics —
    // transformer hum is never only the fundamental.
    QCOMPARE(c.eq().band(1).freqHz, 50.0f);
    QCOMPARE(c.eq().band(2).freqHz, 100.0f);
    QCOMPARE(c.eq().band(3).freqHz, 150.0f);
    for (int i = 1; i <= 3; ++i) {
        QVERIFY(c.eq().band(i).enabled);
        QVERIFY2(c.eq().band(i).gainDb < 0.0f, "a notch must cut");
    }

    // And a 60 Hz station gets 60, not 50.
    VoiceAnalysis us = loud;
    us.humBaseHz = 60;
    StripChain d;
    d.prepare(48000.0);
    QVERIFY(StripTuner::applyAnalysis(us, d).changed);
    QCOMPARE(d.eq().band(1).freqHz, 60.0f);

    // Quiet hum leaves them off rather than notching for the sake of it.
    StripChain e;
    e.prepare(48000.0);
    QVERIFY(StripTuner::applyAnalysis(quiet, e).changed);
    for (int i = 1; i <= 3; ++i) { QVERIFY(!e.eq().band(i).enabled); }
}

void TstStripTuner::no_sibilance_means_no_de_esser()
{
    StripChain c;
    c.prepare(48000.0);
    QVERIFY(StripTuner::applyAnalysis(ordinary(), c).changed);
    QVERIFY2(!c.stageEnabled(StripChain::Stage::DeEss),
             "a de-esser on a voice with no sibilance only dulls it");

    VoiceAnalysis hissy = ordinary();
    hissy.sibilanceDb = 8.0;
    StripChain d;
    d.prepare(48000.0);
    QVERIFY(StripTuner::applyAnalysis(hissy, d).changed);
    QVERIFY(d.stageEnabled(StripChain::Stage::DeEss));
    QVERIFY2(d.deEss().amountDb() < 0.0f, "the amount is a cut");
    QVERIFY2(d.deEss().amountDb() >= -12.0f,
             "more than 12 dB turns an s into a th");
}

void TstStripTuner::every_decision_is_explained()
{
    // An automatic setup that cannot explain itself has to be either
    // trusted completely or discarded completely, and neither is what
    // an operator wants from something that changes how they sound.
    StripChain c;
    c.prepare(48000.0);
    const auto r = StripTuner::applyAnalysis(ordinary(), c);
    QVERIFY(r.changed);

    // One line per stage it touched, plus the note about what it left
    // alone. Short lines are not explanations.
    QVERIFY2(r.notes.size() >= 6,
             qPrintable(QStringLiteral("only %1 notes").arg(r.notes.size())));
    for (const QString& n : r.notes) {
        QVERIFY2(n.length() > 40, qPrintable(n));
    }

    // And it says what it deliberately did not do, rather than leaving
    // the operator to wonder whether it forgot.
    bool mentionsUntouched = false;
    for (const QString& n : r.notes) {
        if (n.contains(QStringLiteral("left off"))) { mentionsUntouched = true; }
    }
    QVERIFY(mentionsUntouched);
}

QTEST_APPLESS_MAIN(TstStripTuner)
#include "tst_strip_tuner.moc"
