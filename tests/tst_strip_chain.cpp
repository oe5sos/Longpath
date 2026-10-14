// The channel strip as a whole, and the one promise that matters most.
//
// tst_strip_dsp checks each stage on its own. This checks the runner:
// the order, the master switch, and the guarantee that an operator who
// has not turned the strip on hears exactly what they heard before it
// existed.
//
// That last one gets the strictest test in the file, because it is the
// one people rely on without knowing they are relying on it. A strip
// that is "off" but still costs a hundredth of a decibel turns every
// A/B comparison into a lie, and turns "I'll just switch it off to
// check" into an experiment that no longer controls anything.
// no-port-check: Longpath-original.

#include <QtTest/QtTest>

#include <QFile>
#include <QXmlStreamReader>

#include "core/strip/StripChain.h"
#include "core/AppSettings.h"
#include "core/strip/StripSettings.h"

#include <cmath>
#include <limits>
#include <vector>

using namespace Longpath;

namespace {

constexpr double kRate   = 48000.0;
constexpr int    kFrames = 64;      // the transmit pump's block size

std::vector<float> voiceish(int frames, float amp = 0.3f)
{
    // Two tones and a little third harmonic — enough structure that a
    // stage doing anything at all shows up as a difference.
    std::vector<float> v(static_cast<size_t>(frames), 0.0f);
    for (int i = 0; i < frames; ++i) {
        const double t = double(i) / kRate;
        v[static_cast<size_t>(i)] = amp * float(
            0.7 * std::sin(2.0 * M_PI * 220.0 * t)
          + 0.3 * std::sin(2.0 * M_PI * 1400.0 * t)
          + 0.1 * std::sin(2.0 * M_PI * 4200.0 * t));
    }
    return v;
}

bool identical(const std::vector<float>& a, const std::vector<float>& b)
{
    if (a.size() != b.size()) { return false; }
    for (size_t i = 0; i < a.size(); ++i) { if (a[i] != b[i]) { return false; } }
    return true;
}

bool allFinite(const std::vector<float>& v)
{
    for (float s : v) { if (!std::isfinite(s)) { return false; } }
    return true;
}

} // namespace

class TstStripChain : public QObject {
    Q_OBJECT
private slots:
    void a_fresh_chain_is_off_and_every_stage_with_it();
    void master_off_is_bit_exact_over_a_long_run();
    void master_off_costs_nothing_even_with_every_stage_on();
    void a_single_stage_changes_the_signal();
    void the_stage_flag_and_the_stages_own_flag_agree();
    void the_whole_chain_stays_finite();
    void every_stage_has_a_name();
    void reset_empties_every_stage();
    void the_limiter_is_last();
    void a_nan_at_the_input_becomes_silence_and_is_counted();
    void a_stage_that_returns_nan_is_bypassed_for_that_block();
    void a_poisoned_stage_recovers_once_its_setting_is_sane_again();
    void a_block_too_big_for_the_snapshot_still_comes_out_finite();
    void a_clean_chain_counts_nothing();
    void every_preset_leaves_the_master_alone();
    void every_preset_names_itself_and_applies();
    void a_user_preset_round_trips();
    void a_bad_preset_name_is_refused();
    void two_user_presets_both_survive_the_file();
    void a_preset_name_with_a_dash_keeps_the_file_readable();
    void the_default_order_is_the_one_the_header_promises();
    void an_order_that_is_not_a_permutation_is_refused();
    void the_tube_in_front_is_a_different_device();
    void a_pause_between_words_never_becomes_the_recommendation();
};

void TstStripChain::a_fresh_chain_is_off_and_every_stage_with_it()
{
    // A fresh install must sound exactly as it did before this code
    // existed. Anything else is a change the operator did not ask for,
    // arriving on the air.
    StripChain c;
    QVERIFY2(!c.isEnabled(), "the strip switched itself on");
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        const auto s = static_cast<StripChain::Stage>(i);
        QVERIFY2(!c.stageEnabled(s), StripChain::stageName(s));
    }
}

void TstStripChain::master_off_is_bit_exact_over_a_long_run()
{
    StripChain c;
    c.prepare(kRate);
    c.setEnabled(false);

    // A second of audio. A bypass that leaks would show as drift rather
    // than as a single wrong sample, so one block would not find it.
    for (int b = 0; b < int(kRate / kFrames); ++b) {
        const std::vector<float> in = voiceish(kFrames);
        std::vector<float> out = in;
        c.processMono(out.data(), kFrames);
        QVERIFY2(identical(in, out),
                 qPrintable(QStringLiteral("block %1 was altered while the "
                                           "strip was off").arg(b)));
    }
}

void TstStripChain::master_off_costs_nothing_even_with_every_stage_on()
{
    // The failure this exists for: an operator sets up all eight
    // stages, switches the strip off to compare, and hears a difference
    // anyway because "off" was implemented as "ask each stage to
    // bypass itself". The master switch returns before any stage is
    // consulted, so the number of enabled stages cannot matter.
    StripChain c;
    c.prepare(kRate);
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        c.setStageEnabled(static_cast<StripChain::Stage>(i), true);
    }
    c.setEnabled(false);

    const std::vector<float> in = voiceish(kFrames, 0.6f);
    std::vector<float> out = in;
    for (int b = 0; b < 50; ++b) { c.processMono(out.data(), kFrames); }
    QVERIFY2(identical(in, out),
             "the strip altered audio with the master switch off");
}

void TstStripChain::a_single_stage_changes_the_signal()
{
    // The mirror image of the bypass test: on must actually mean on. A
    // chain that is bit-exact in both states would pass every test
    // above and do nothing at all.
    StripChain c;
    c.prepare(kRate);
    c.setEnabled(true);
    c.setStageEnabled(StripChain::Stage::Tube, true);
    c.tube().setDriveDb(12.0f);

    const std::vector<float> in = voiceish(kFrames, 0.5f);
    std::vector<float> out = in;
    for (int b = 0; b < 10; ++b) { c.processMono(out.data(), kFrames); }

    QVERIFY2(!identical(in, out), "an enabled stage did nothing");
    QVERIFY(allFinite(out));
}

void TstStripChain::the_stage_flag_and_the_stages_own_flag_agree()
{
    // Two switches for one stage is two answers to one question, and
    // the panel will read one while the audio thread reads the other.
    StripChain c;
    c.prepare(kRate);

    c.setStageEnabled(StripChain::Stage::Comp, true);
    QVERIFY(c.stageEnabled(StripChain::Stage::Comp));
    QVERIFY2(c.comp().isEnabled(),
             "the chain says the compressor is on and the compressor "
             "disagrees");

    c.setStageEnabled(StripChain::Stage::Comp, false);
    QVERIFY(!c.stageEnabled(StripChain::Stage::Comp));
    QVERIFY(!c.comp().isEnabled());
}

void TstStripChain::the_whole_chain_stays_finite()
{
    // Everything on, driven hard, for two seconds. Eight stages in
    // series each with their own feedback and make-up gain is exactly
    // where a chain runs away, and a runaway in the transmit path is
    // heard by everyone except the operator.
    StripChain c;
    c.prepare(kRate);
    c.setEnabled(true);
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        c.setStageEnabled(static_cast<StripChain::Stage>(i), true);
    }

    float worst = 0.0f;
    for (int b = 0; b < int(2.0 * kRate / kFrames); ++b) {
        std::vector<float> buf = voiceish(kFrames, 0.9f);
        c.processMono(buf.data(), kFrames);
        QVERIFY2(allFinite(buf), qPrintable(QStringLiteral("block %1").arg(b)));
        for (float s : buf) { worst = std::max(worst, std::fabs(s)); }
    }
    QVERIFY2(worst < 8.0f,
             qPrintable(QStringLiteral("the chain reached %1")
                            .arg(double(worst))));
}

// ── The non-finite guard ─────────────────────────────────────────────
//
// Four tests for one promise: whatever goes into the strip, what comes
// out is a number. The first two are the two places a NaN can enter —
// the microphone and a stage — and the third is the difference between
// a guard and a fuse: after the cause is gone the stage works again
// without anyone restarting anything. The last one is the control.
//
// The stage that misbehaves on purpose is the EQ with a master gain of
// NaN. That is not a contrived value: std::clamp(NaN, lo, hi) returns
// NaN, so a NaN that arrives from a settings file or a slider reaches
// the audio thread untouched, and the EQ multiplies every sample by it.

void TstStripChain::a_nan_at_the_input_becomes_silence_and_is_counted()
{
    StripChain c;
    c.prepare(kRate);
    c.setEnabled(true);
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        c.setStageEnabled(static_cast<StripChain::Stage>(i), true);
    }

    // A few blocks of good voice first so every stage has state to
    // poison, then one block with a NaN and both infinities in it.
    for (int b = 0; b < 20; ++b) {
        std::vector<float> buf = voiceish(kFrames, 0.5f);
        c.processMono(buf.data(), kFrames);
    }
    std::vector<float> bad = voiceish(kFrames, 0.5f);
    bad[3]  = std::numeric_limits<float>::quiet_NaN();
    bad[17] = std::numeric_limits<float>::infinity();
    bad[40] = -std::numeric_limits<float>::infinity();
    c.processMono(bad.data(), kFrames);
    QVERIFY2(allFinite(bad), "a non-finite input sample got through");

    QCOMPARE(c.nonFiniteInputBlocks(), 1u);
    QCOMPARE(c.nonFiniteBlocksTotal(), 1u);
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        const auto st = static_cast<StripChain::Stage>(i);
        QVERIFY2(c.nonFiniteBlocks(st) == 0,
                 qPrintable(QStringLiteral("%1 was blamed for the input's NaN")
                                .arg(QLatin1String(StripChain::stageName(st)))));
    }

    // And the chain is not poisoned: a second of clean voice comes out
    // finite and audible. Without the guard the limiter's envelope
    // would have gone to infinity on the +inf sample and every block
    // after it would be NaN.
    float worst = 0.0f;
    for (int b = 0; b < int(kRate / kFrames); ++b) {
        std::vector<float> buf = voiceish(kFrames, 0.5f);
        c.processMono(buf.data(), kFrames);
        QVERIFY2(allFinite(buf), qPrintable(QStringLiteral("block %1 after the NaN").arg(b)));
        for (float x : buf) { worst = std::max(worst, std::fabs(x)); }
    }
    QVERIFY2(worst > 0.01f, "the chain went silent after one bad block");
    QCOMPARE(c.nonFiniteBlocksTotal(), 1u);
}

void TstStripChain::a_stage_that_returns_nan_is_bypassed_for_that_block()
{
    StripChain c;
    c.prepare(kRate);
    c.setEnabled(true);
    c.setStageEnabled(StripChain::Stage::Eq, true);
    // One band at 0 dB so the EQ actually runs (it returns early with
    // no active band); the damage is done by the master gain alone.
    c.eq().setBand(0, ClientEq::BandParams{});
    c.eq().setActiveBandCount(1);
    c.eq().setMasterGain(std::numeric_limits<float>::quiet_NaN());

    const std::vector<float> in = voiceish(kFrames, 0.4f);
    std::vector<float> out = in;
    c.processMono(out.data(), kFrames);

    // Bypassed means bypassed: not "finite", not "quiet" — the block
    // the stage was handed, bit for bit, exactly as the operator's own
    // bypass switch would have delivered it.
    QVERIFY2(identical(in, out),
             "the stage's block was not restored after it returned NaN");
    QCOMPARE(c.nonFiniteBlocks(StripChain::Stage::Eq), 1u);
    QCOMPARE(c.nonFiniteInputBlocks(), 0u);
    QCOMPARE(c.nonFiniteBlocksTotal(), 1u);

    // Every block, while the cause persists — and still bit-exact, so
    // the operator hears their voice minus the EQ rather than silence.
    for (int b = 0; b < 9; ++b) {
        std::vector<float> again = in;
        c.processMono(again.data(), kFrames);
        QVERIFY(identical(in, again));
    }
    QCOMPARE(c.nonFiniteBlocks(StripChain::Stage::Eq), 10u);
    QCOMPARE(c.nonFiniteBlocksTotal(), 10u);
}

void TstStripChain::a_poisoned_stage_recovers_once_its_setting_is_sane_again()
{
    StripChain c;
    c.prepare(kRate);
    c.setEnabled(true);
    c.setStageEnabled(StripChain::Stage::Eq, true);
    ClientEq::BandParams band;
    band.freqHz = 1400.0f;
    band.gainDb = 9.0f;
    c.eq().setBand(0, band);
    c.eq().setActiveBandCount(1);

    c.eq().setMasterGain(std::numeric_limits<float>::quiet_NaN());
    for (int b = 0; b < 5; ++b) {
        std::vector<float> buf = voiceish(kFrames, 0.4f);
        c.processMono(buf.data(), kFrames);
        QVERIFY(allFinite(buf));
    }
    QCOMPARE(c.nonFiniteBlocks(StripChain::Stage::Eq), 5u);

    // The cause goes away. Nothing is restarted, no stage is toggled.
    c.eq().setMasterGain(1.0f);
    const std::vector<float> in = voiceish(kFrames, 0.4f);
    std::vector<float> out = in;
    // A few blocks for the EQ's parameter smoothing to arrive at the
    // +9 dB the band asks for; the last block is the one we read.
    for (int b = 0; b < 40; ++b) {
        out = in;
        c.processMono(out.data(), kFrames);
    }
    QVERIFY2(allFinite(out), "still NaN after the setting was repaired");
    QVERIFY2(!identical(in, out),
             "the EQ is still being bypassed after its setting was repaired");
    QCOMPARE(c.nonFiniteBlocks(StripChain::Stage::Eq), 5u);
}

void TstStripChain::a_block_too_big_for_the_snapshot_still_comes_out_finite()
{
    // Nobody calls the strip with more than 64 frames today. If someone
    // does with more than the snapshot holds, the promise that matters
    // — finite out — still holds; only the bit-exact bypass is traded
    // for silence in the bad samples.
    StripChain c;
    c.prepare(kRate);
    c.setEnabled(true);
    c.setStageEnabled(StripChain::Stage::Eq, true);
    c.eq().setBand(0, ClientEq::BandParams{});
    c.eq().setActiveBandCount(1);
    c.eq().setMasterGain(std::numeric_limits<float>::quiet_NaN());

    constexpr int kBig = 4096;
    std::vector<float> buf = voiceish(kBig, 0.4f);
    c.processMono(buf.data(), kBig);
    QVERIFY(allFinite(buf));
    QCOMPARE(c.nonFiniteBlocks(StripChain::Stage::Eq), 1u);
}

void TstStripChain::a_clean_chain_counts_nothing()
{
    // The control: everything on, two seconds of hot voice, and not a
    // single block blamed on anyone. A guard that fires on healthy
    // audio would be bypassing stages the operator switched on.
    StripChain c;
    c.prepare(kRate);
    c.setEnabled(true);
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        c.setStageEnabled(static_cast<StripChain::Stage>(i), true);
    }
    c.tube().setDriveDb(18.0f);
    for (int b = 0; b < int(2.0 * kRate / kFrames); ++b) {
        std::vector<float> buf = voiceish(kFrames, 0.9f);
        c.processMono(buf.data(), kFrames);
    }
    QCOMPARE(c.nonFiniteBlocksTotal(), 0u);
    QCOMPARE(c.nonFiniteInputBlocks(), 0u);
}

void TstStripChain::every_stage_has_a_name()
{
    // The panels and the chain row read these. An empty one is a blank
    // tile the operator cannot identify.
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        const char* n = StripChain::stageName(
            static_cast<StripChain::Stage>(i));
        QVERIFY2(n != nullptr && *n != '\0',
                 qPrintable(QStringLiteral("stage %1 has no name").arg(i)));
    }
}

// ── reset() muss ALLE Stufen treffen ────────────────────────────────
//
// Hier fehlten das Entrauschen und der Clipper -- die beiden, die nach
// der urspruenglichen Acht dazugekommen sind. Beide halten Zustand:
// der Entrauscher eine Verzoegerungsleitung ueber 1440 Abtastwerte,
// der Clipper die Vorgeschichte seines Halbbandfilters. Folge: die
// naechste Durchsage beginnt mit einem Rest der vorigen.
//
// Hier der Clipper. Das Entrauschen braucht sein Modell und damit ein
// QApplication, das diese Datei nicht hat (QTEST_APPLESS_MAIN) -- es
// wird in tests/tst_bandbreiten_genau geprueft, und zwar mit den
// vollen 1440 Abtastwerten.
//
// Geprueft wird die WIRKUNG, nicht die Anwesenheit eines Aufrufs.
void TstStripChain::reset_empties_every_stage()
{
    StripChain c;
    c.prepare(48000.0);
    c.setEnabled(true);
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
    }
    c.clipper().setDriveDb(12.0f);
    c.setStageEnabled(StripChain::Stage::Clip, true);

    std::vector<float> laut(2400, 0.0f);
    for (size_t i = 0; i < laut.size(); ++i) {
        laut[i] = float(0.5 * std::sin(2.0 * M_PI * 1000.0
                                       * double(i) / 48000.0));
    }
    for (size_t at = 0; at + 240 <= laut.size(); at += 240) {
        c.processMono(laut.data() + at, 240);
    }

    c.reset();

    std::vector<float> stille(960, 0.0f);
    for (size_t at = 0; at + 240 <= stille.size(); at += 240) {
        c.processMono(stille.data() + at, 240);
    }
    float p = 0.0f;
    for (float v : stille) { p = std::max(p, std::fabs(v)); }
    QVERIFY2(p < 1e-6f,
             qPrintable(QStringLiteral("nach dem Zuruecksetzen kommt noch "
                                       "%1 heraus -- der Clipper haelt "
                                       "seinen Zustand").arg(double(p))));
}

void TstStripChain::the_limiter_is_last()
{
    // Not a behavioural test — a statement of the invariant, checked
    // where it can be checked. A brickwall that anything runs after is
    // not a brickwall, and every stage above it can add gain.
    QCOMPARE(static_cast<int>(StripChain::Stage::Limiter),
             StripChain::kStageCount - 1);
    // Die Reihenfolge, Stand 2026-10-10. Sie war einmal die
    // defaultChain() von AetherSDR; seither sind zwei Stufen
    // dazugekommen, und BEIDE an einer Stelle, die eine Entscheidung
    // ist und kein Zufall:
    //
    //   Entrauschen ganz nach vorne, damit alles danach eine saubere
    //   Stimme bekommt -- ein Kompressor mit Anhebung macht aus
    //   einem leisen Luefter ein Rauschen, das mitsendet.
    //
    //   Clipper direkt VOR den Begrenzer: er holt die
    //   Sprechleistung, der Begrenzer faengt, was er stehen laesst.
    //
    // Diese Prüfung stand bis heute auf der alten Reihenfolge und
    // war seit dem Einbau des Entrauschens rot, ohne dass es jemand
    // gesehen hat.
    QCOMPARE(static_cast<int>(StripChain::Stage::Denoise), 0);
    QCOMPARE(static_cast<int>(StripChain::Stage::Gate),    1);
    QCOMPARE(static_cast<int>(StripChain::Stage::Eq),      2);
    QCOMPARE(static_cast<int>(StripChain::Stage::DeEss),   3);
    QCOMPARE(static_cast<int>(StripChain::Stage::Comp),    4);
    QCOMPARE(static_cast<int>(StripChain::Stage::Tube),    5);
    QCOMPARE(static_cast<int>(StripChain::Stage::Pudu),    6);
    QCOMPARE(static_cast<int>(StripChain::Stage::Reverb),  7);
    QCOMPARE(static_cast<int>(StripChain::Stage::Clip),    8);
}

void TstStripChain::every_preset_leaves_the_master_alone()
{
    // A preset is a starting point, not a decision to go on the air
    // with it. Whether the strip is in circuit stays the operator's
    // call — and a preset that switched it on would change the transmit
    // audio of someone who was only browsing the list.
    StripChain c;
    c.prepare(kRate);
    QVERIFY(!c.isEnabled());
    for (const auto& p : StripSettings::builtInPresets()) {
        QVERIFY2(StripSettings::applyBuiltIn(p.name, c), qPrintable(p.name));
        QVERIFY2(!c.isEnabled(),
                 qPrintable(QStringLiteral("%1 switched the strip on")
                                .arg(p.name)));
    }
}

void TstStripChain::every_preset_names_itself_and_applies()
{
    const auto presets = StripSettings::builtInPresets();
    QVERIFY(!presets.isEmpty());

    StripChain c;
    c.prepare(kRate);
    for (const auto& p : presets) {
        QVERIFY(!p.name.trimmed().isEmpty());
        // The description is the whole point of a preset list. A name
        // like "DX" tells an operator nothing about what it will do to
        // their voice or what it costs.
        QVERIFY2(p.description.length() > 40, qPrintable(p.name));
        QVERIFY(StripSettings::applyBuiltIn(p.name, c));

        // Each one must actually put something in circuit, or choosing
        // it does nothing and the operator concludes presets are broken.
        int on = 0;
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            if (c.stageEnabled(static_cast<StripChain::Stage>(i))) { ++on; }
        }
        QVERIFY2(on >= 2, qPrintable(QStringLiteral("%1 enabled %2 stages")
                                         .arg(p.name).arg(on)));
    }

    // And an unknown name is refused rather than silently doing nothing
    // that looks like success.
    QVERIFY(!StripSettings::applyBuiltIn(QStringLiteral("nonesuch"), c));
}

void TstStripChain::a_user_preset_round_trips()
{
    // The whole point of saving one: what comes back has to be what
    // went in. A preset system that stores a subset of the settings is
    // worse than none, because the operator recalls their sound and
    // gets most of it — and cannot tell which part is missing.
    StripChain a;
    a.prepare(kRate);
    a.gate().setThresholdDb(-33.0f);
    a.comp().setRatio(4.5f);
    a.comp().setMakeupDb(5.5f);
    a.deEss().setFrequencyHz(7100.0f);
    a.tube().setDriveDb(9.0f);
    a.limiter().setCeilingDb(-2.5f);
    a.setStageEnabled(StripChain::Stage::Gate, true);
    a.setStageEnabled(StripChain::Stage::Tube, true);

    const QString name = QStringLiteral("tst round trip");
    QVERIFY(StripSettings::saveUserPreset(name, a));
    QVERIFY(StripSettings::userPresetNames().contains(name));

    StripChain b;
    b.prepare(kRate);
    QVERIFY(StripSettings::applyUserPreset(name, b));

    QCOMPARE(b.gate().thresholdDb(), -33.0f);
    QCOMPARE(b.comp().ratio(), 4.5f);
    QCOMPARE(b.comp().makeupDb(), 5.5f);
    QCOMPARE(b.deEss().frequencyHz(), 7100.0f);
    QCOMPARE(b.tube().driveDb(), 9.0f);
    QCOMPARE(b.limiter().ceilingDb(), -2.5f);
    QVERIFY(b.stageEnabled(StripChain::Stage::Gate));
    QVERIFY(b.stageEnabled(StripChain::Stage::Tube));
    QVERIFY(!b.stageEnabled(StripChain::Stage::Reverb));

    // And like the built-ins, it does not put the strip on the air.
    QVERIFY(!b.isEnabled());

    QVERIFY(StripSettings::removeUserPreset(name));
    QVERIFY(!StripSettings::userPresetNames().contains(name));
    QVERIFY(!StripSettings::applyUserPreset(name, b));
}

void TstStripChain::a_bad_preset_name_is_refused()
{
    // A slash would open a second level of settings keys and the preset
    // would reappear somewhere unexpected, or not at all. Refused
    // rather than quietly mangled into something that nearly works.
    StripChain c;
    c.prepare(kRate);
    QVERIFY(!StripSettings::saveUserPreset(QString(), c));
    QVERIFY(!StripSettings::saveUserPreset(QStringLiteral("   "), c));
    QVERIFY(!StripSettings::saveUserPreset(QStringLiteral("a/b"), c));
    QVERIFY(!StripSettings::removeUserPreset(QStringLiteral("never existed")));
}

// ── Die Reihenfolge der Stufen ───────────────────────────────────────
//
// Seit dem 2026-10-13 ist sie einstellbar. Anlass: Martins Aether-
// Profil (SSB_30.07.2026, unter Windows mit dem Teufel-Headset als gut
// befunden) traegt die Kette als Liste und stellt die Roehre VOR das
// Tor -- Tube, Gate, Eq, Comp, DeEss, Enh, Reverb.

void TstStripChain::the_default_order_is_the_one_the_header_promises()
{
    StripChain c;
    c.prepare(kRate);
    const StripChain::Order ist = c.stageOrder();
    const StripChain::Order soll = StripChain::defaultOrder();
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        QCOMPARE(static_cast<int>(ist[size_t(i)]),
                 static_cast<int>(soll[size_t(i)]));
        // Und sie ist dieselbe wie die Reihenfolge des
        // Aufzaehlungstyps, solange niemand sie umstellt -- sonst
        // zeigte die Oberflaeche eine andere Kette als die, die laeuft.
        QCOMPARE(static_cast<int>(soll[size_t(i)]), i);
    }
}

void TstStripChain::an_order_that_is_not_a_permutation_is_refused()
{
    StripChain c;
    c.prepare(kRate);

    StripChain::Order doppelt = StripChain::defaultOrder();
    doppelt[0] = StripChain::Stage::Comp;   // Comp steht jetzt zweimal
    QVERIFY2(!c.setStageOrder(doppelt),
             "eine Kette mit einer Stufe zweimal ist kein Kanalzug");

    // Eine abgelehnte Umstellung darf nichts halb umgestellt
    // hinterlassen.
    const StripChain::Order ist = c.stageOrder();
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        QCOMPARE(static_cast<int>(ist[size_t(i)]), i);
    }

    StripChain::Order aether = { StripChain::Stage::Tube,
                                 StripChain::Stage::Gate,
                                 StripChain::Stage::Eq,
                                 StripChain::Stage::Comp,
                                 StripChain::Stage::DeEss,
                                 StripChain::Stage::Pudu,
                                 StripChain::Stage::Reverb,
                                 StripChain::Stage::Denoise,
                                 StripChain::Stage::Clip,
                                 StripChain::Stage::Limiter };
    QVERIFY(c.setStageOrder(aether));
    QCOMPARE(static_cast<int>(c.stageOrder()[0]),
             static_cast<int>(StripChain::Stage::Tube));
}

void TstStripChain::the_tube_in_front_is_a_different_device()
{
    // Der Beweis, dass die Umstellung im AUDIOWEG ankommt und nicht
    // nur in einer Zahl: dieselben zwei Stufen, dieselben Werte,
    // dasselbe Signal -- einmal Kompressor vor Roehre, einmal
    // umgekehrt. Kaeme dasselbe heraus, waere die Reihenfolge Schmuck.
    const auto fahre = [](bool roehreVorn, std::vector<float> x) {
        StripChain c;
        c.prepare(kRate);
        c.setEnabled(true);
        c.setStageEnabled(StripChain::Stage::Comp, true);
        c.setStageEnabled(StripChain::Stage::Tube, true);

        c.comp().setThresholdDb(-24.0f);
        c.comp().setRatio(4.0f);
        c.comp().setMakeupDb(6.0f);
        c.tube().setDriveDb(16.0f);

        StripChain::Order o = StripChain::defaultOrder();
        if (roehreVorn) {
            o = { StripChain::Stage::Tube,   StripChain::Stage::Denoise,
                  StripChain::Stage::Gate,   StripChain::Stage::Eq,
                  StripChain::Stage::DeEss,  StripChain::Stage::Comp,
                  StripChain::Stage::Pudu,   StripChain::Stage::Reverb,
                  StripChain::Stage::Clip,   StripChain::Stage::Limiter };
        }
        if (!c.setStageOrder(o)) { return std::vector<float>{}; }

        // Mehrere Bloecke, damit die Huellkurven beider Stufen
        // eingeschwungen sind.
        for (int b = 0; b < 8; ++b) { c.processMono(x.data(), int(x.size())); }
        return x;
    };

    const std::vector<float> ein = voiceish(kFrames, 0.5f);
    const std::vector<float> a = fahre(false, ein);
    const std::vector<float> b = fahre(true,  ein);

    QVERIFY(!a.empty() && !b.empty());
    QVERIFY(allFinite(a) && allFinite(b));
    QVERIFY2(!identical(a, b),
             "Roehre vor dem Kompressor klingt wie Roehre dahinter -- "
             "dann kommt die Reihenfolge im Audioweg nicht an");

    double sa = 0.0;
    double sb = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        sa += double(a[i]) * double(a[i]);
        sb += double(b[i]) * double(b[i]);
    }
    const double rmsA = std::sqrt(sa / double(a.size()));
    const double rmsB = std::sqrt(sb / double(b.size()));
    qInfo("Kompressor->Roehre RMS %.4f | Roehre->Kompressor RMS %.4f "
          "(%.2f dB Unterschied)", rmsA, rmsB,
          20.0 * std::log10(std::max(rmsB, 1e-9) / std::max(rmsA, 1e-9)));
}

// ── Zwei eigene Ketten, nicht nur eine ───────────────────────────────
//
// Die Liste der eigenen Ketten steht als QStringList in den
// Einstellungen, und AppSettings legt jeden Wert als val.toString() ab
// (AppSettings.cpp:871-909). QVariant::toString() liefert bei einer
// Liste mit EINEM Eintrag diesen Eintrag -- und bei zweien eine LEERE
// Zeichenkette. Die zweite abgelegte Kette nimmt also beim Speichern
// die erste mit ins Grab, und nach dem Neustart ist die Liste leer,
// obwohl alle Werte noch auf der Platte stehen.
//
// Geprueft wird deshalb die DATEI, nicht der Hauptspeicher: im
// Hauptspeicher ist die Liste in Ordnung, und genau daran ist das
// bisher nicht aufgefallen.
void TstStripChain::two_user_presets_both_survive_the_file()
{
    StripChain c;
    c.prepare(kRate);
    c.comp().setMakeupDb(4.0f);
    QVERIFY(StripSettings::saveUserPreset(QStringLiteral("Eins"), c));
    c.comp().setMakeupDb(5.0f);
    QVERIFY(StripSettings::saveUserPreset(QStringLiteral("Zwei"), c));

    QStringList namen = StripSettings::userPresetNames();
    QVERIFY2(namen.contains(QStringLiteral("Eins"))
                 && namen.contains(QStringLiteral("Zwei")),
             qPrintable(QStringLiteral("im Speicher fehlt schon eine: %1")
                            .arg(namen.join(QLatin1Char(',')))));

    AppSettings::instance().save();
    QFile f(AppSettings::instance().filePath());
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString inhalt = QString::fromUtf8(f.readAll());

    // In der Datei stehen die Namen base64-kodiert, einer je Eintrag:
    // ein Trennzeichen kann so in keinem Namen vorkommen. Geprueft wird
    // deshalb der entschluesselte Inhalt, nicht der Rohtext.
    const int at = inhalt.indexOf(QStringLiteral("UserPresets"));
    QVERIFY2(at >= 0, "der Schluessel steht gar nicht in der Datei");
    const int von = inhalt.indexOf(QLatin1Char('>'), at) + 1;
    const int bis = inhalt.indexOf(QLatin1Char('<'), von);
    const QString roh = inhalt.mid(von, bis - von);
    QVERIFY2(!roh.trimmed().isEmpty(),
             "die Liste steht leer in der Datei -- die zweite Kette hat "
             "die erste mitgenommen");

    QStringList ausDerDatei;
    if (roh.startsWith(QStringLiteral("@StringList("))) {
        const QString inner = roh.mid(12, roh.size() - 13);
        for (const QString& e : inner.split(QLatin1Char(','),
                                            Qt::SkipEmptyParts)) {
            ausDerDatei << QString::fromUtf8(
                QByteArray::fromBase64(e.toLatin1()));
        }
    } else {
        ausDerDatei << roh;
    }
    QVERIFY2(ausDerDatei.contains(QStringLiteral("Eins"))
                 && ausDerDatei.contains(QStringLiteral("Zwei")),
             qPrintable(QStringLiteral("in der Datei stehen: %1")
                            .arg(ausDerDatei.join(QLatin1Char(',')))));

    QVERIFY(StripSettings::removeUserPreset(QStringLiteral("Eins")));
    QVERIFY(StripSettings::removeUserPreset(QStringLiteral("Zwei")));
}

// ── Ein Gedankenstrich im Namen ──────────────────────────────────────
//
// „SSB_30.07.2026 — Glanz im Band": der Gedankenstrich U+2014 ist in
// einem XML-Elementnamen NICHT erlaubt. Bis zum 2026-10-13 wanderte er
// ungeprüft in den Namen des Schlüssels, und die Einstellungsdatei war
// damit unlesbar -- beim nächsten Start lief die Wiederherstellung aus
// der Sicherung an und die Arbeit seither war weg. Am Gerät des
// Betreibers ist genau das einmal passiert (Longpath.settings.corrupt-
// 20261013-214358).
void TstStripChain::a_preset_name_with_a_dash_keeps_the_file_readable()
{
    StripChain c;
    c.prepare(kRate);
    c.tube().setDriveDb(7.0f);

    const QString name = QStringLiteral("Probe \u2014 Glanz im Band");
    QVERIFY(StripSettings::saveUserPreset(name, c));
    AppSettings::instance().save();

    // Die Datei muss ein Leser noch öffnen können.
    QFile f(AppSettings::instance().filePath());
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray roh = f.readAll();
    QXmlStreamReader xml(roh);
    while (!xml.atEnd()) { xml.readNext(); }
    QVERIFY2(!xml.hasError(),
             qPrintable(QStringLiteral("die Datei ist kein gültiges XML "
                                       "mehr: %1").arg(xml.errorString())));

    // Und der Name muss zurückkommen, wie er hineinging.
    QVERIFY2(StripSettings::userPresetNames().contains(name),
             qPrintable(QStringLiteral("Liste: %1")
                            .arg(StripSettings::userPresetNames()
                                     .join(QLatin1Char(',')))));

    StripChain z;
    z.prepare(kRate);
    QVERIFY(StripSettings::applyUserPreset(name, z));
    QVERIFY(qFuzzyCompare(z.tube().driveDb(), 7.0f));

    QVERIFY(StripSettings::removeUserPreset(name));
}


// ── Die Pause zwischen zwei Wörtern ist keine Messung ───────────────
//
// Am 2026-10-14 hat der Betreiber dreimal den Kopfhörer abreissen
// müssen. Der dritte Vorfall ging so: die Zeile über dem Schieber
// sagte „23 dB fehlen", der Knopf daneben wurde gedrückt -- und setzte
// +36 dB, den Anschlag. Beide rechneten mit rawInputPeakDb(), der
// Spitze des letzten Blocks. Die Zeile hatte eine Silbe erwischt, der
// Knopf die Atempause danach.
//
// Geprüft wird deshalb genau diese Reihenfolge: sprechen, dann
// schweigen, dann rechnen. Der Augenblickswert MUSS dabei in den
// Störteppich fallen -- sonst prüft der Fall nichts -- und die Zahl,
// mit der gerechnet wird, muss oben bleiben.
void TstStripChain::a_pause_between_words_never_becomes_the_recommendation()
{
    StripChain c;
    c.prepare(kRate);
    c.setEnabled(true);

    // Zwei Sekunden Sprache: Silben mit Pausen dazwischen. Das ist der
    // Scheitelfaktor, an dem die Kette Sprache erkennt.
    const int fenster = int(kRate * 2.0) + kFrames;
    for (int n = 0; n < fenster; n += kFrames) {
        std::vector<float> blk(static_cast<size_t>(kFrames), 0.0f);
        if ((n / kFrames) % 10 == 0) {
            blk = voiceish(kFrames, 0.45f);
        }
        c.processMono(blk.data(), kFrames);
    }
    const float gesprochen = c.speechPeakDb();
    QVERIFY2(gesprochen > -20.0f,
             qPrintable(QStringLiteral("Sprache nicht erkannt: %1 dBFS")
                            .arg(double(gesprochen))));

    // Jetzt die Atempause: ein Störteppich, wie ihn die Klinkenbuchse
    // liefert. Zwei volle Fenster lang, damit die Automatik ihn
    // mindestens einmal zu sehen bekommt.
    const float brumm = std::pow(10.0f, -48.0f / 20.0f);
    for (int n = 0; n < 2 * fenster; n += kFrames) {
        std::vector<float> blk(static_cast<size_t>(kFrames), 0.0f);
        for (int i = 0; i < kFrames; ++i) {
            const double t = double(n + i) / kRate;
            blk[static_cast<size_t>(i)] =
                brumm * float(std::sin(2.0 * M_PI * 50.0 * t));
        }
        c.processMono(blk.data(), kFrames);
    }

    // Die Gegenprobe: der Augenblickswert IST jetzt der Störteppich.
    // Ohne diese Zeile belegt der Fall nichts -- genau mit dieser Zahl
    // hat der Knopf gerechnet.
    const float augenblick = c.rawInputPeakDb();
    QVERIFY2(augenblick < -40.0f,
             qPrintable(QStringLiteral("die Pause ist nicht leise genug: "
                                       "%1 dBFS")
                            .arg(double(augenblick))));
    QVERIFY2(-12.0f - augenblick > 30.0f,
             "die alte Rechnung haette hier keine 30 dB gefordert — "
             "dann prueft dieser Fall den Vorfall nicht nach");

    // Und die Zahl, mit der gerechnet werden darf, steht noch oben.
    QCOMPARE(c.speechPeakDb(), gesprochen);
    const float empfehlung = -12.0f - c.speechPeakDb();
    QVERIFY2(empfehlung < 15.0f,
             qPrintable(QStringLiteral("die Empfehlung ist wieder "
                                       "davongelaufen: %1 dB")
                            .arg(double(empfehlung))));
}

QTEST_APPLESS_MAIN(TstStripChain)
#include "tst_strip_chain.moc"
