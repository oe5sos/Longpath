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
// no-port-check: NereusSDR-original.

#include <QtTest/QtTest>

#include "core/strip/StripChain.h"
#include "core/strip/StripSettings.h"

#include <cmath>
#include <vector>

using namespace NereusSDR;

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
    void the_limiter_is_last();
    void every_preset_leaves_the_master_alone();
    void every_preset_names_itself_and_applies();
    void a_user_preset_round_trips();
    void a_bad_preset_name_is_refused();
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

void TstStripChain::the_limiter_is_last()
{
    // Not a behavioural test — a statement of the invariant, checked
    // where it can be checked. A brickwall that anything runs after is
    // not a brickwall, and every stage above it can add gain.
    QCOMPARE(static_cast<int>(StripChain::Stage::Limiter),
             StripChain::kStageCount - 1);
    // And the order above it is AetherSDR's defaultChain().
    QCOMPARE(static_cast<int>(StripChain::Stage::Gate),  0);
    QCOMPARE(static_cast<int>(StripChain::Stage::Eq),    1);
    QCOMPARE(static_cast<int>(StripChain::Stage::DeEss), 2);
    QCOMPARE(static_cast<int>(StripChain::Stage::Comp),  3);
    QCOMPARE(static_cast<int>(StripChain::Stage::Tube),  4);
    QCOMPARE(static_cast<int>(StripChain::Stage::Pudu),  5);
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

QTEST_APPLESS_MAIN(TstStripChain)
#include "tst_strip_chain.moc"
