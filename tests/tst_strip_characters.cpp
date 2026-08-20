// =================================================================
// tests/tst_strip_characters.cpp  (NereusSDR)
// =================================================================
//
// A character is a claim: "this one is gentler than that one". Nothing
// enforces such a claim, so it rots — somebody nudges a number, the
// ordering quietly inverts, and the list still reads gentlest-first
// while behaving the other way round.
//
// These tests check the claims rather than the numbers. Asserting that
// Contest has ratio 6.0 would just be the table written twice; asserting
// that Contest compresses harder than Balanced is the thing the operator
// is actually promised.
//
// no-port-check: NereusSDR-original.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/StripCharacters.h"
#include "core/strip/StripChain.h"

#include <QtTest/QtTest>

using namespace Longpath;
using Stage = StripChain::Stage;

namespace {

// A chain that is prepared but never processes audio; the characters
// only move parameters, so nothing here needs a sample.
std::unique_ptr<StripChain> makeChain()
{
    auto c = std::make_unique<StripChain>();
    c->prepare(48000.0);
    return c;
}

} // namespace

class TstStripCharacters : public QObject {
    Q_OBJECT
private slots:

    // ── The list itself ──────────────────────────────────────────────

    void every_character_has_a_name_and_a_reason()
    {
        // A preset with no description is a quiz question. The operator
        // who most needs presets is exactly the one who cannot tell
        // "Voodoo" from "Contest" without being told.
        for (Stage s : {Stage::Gate, Stage::Comp, Stage::DeEss,
                        Stage::Tube, Stage::Pudu, Stage::Limiter}) {
            const auto list = StripCharacters::forStage(s);
            QVERIFY2(!list.isEmpty(), "stage offers no characters");
            for (const auto& ch : list) {
                QVERIFY(!ch.name.trimmed().isEmpty());
                QVERIFY2(ch.description.length() > 40,
                         qPrintable(QStringLiteral("'%1' has no real "
                                                   "description").arg(ch.name)));
            }
        }
    }

    void names_are_unique_within_a_stage()
    {
        // Two characters with one name means apply() picks whichever is
        // written first and the other is unreachable.
        for (Stage s : {Stage::Gate, Stage::Comp, Stage::DeEss,
                        Stage::Tube, Stage::Pudu, Stage::Limiter}) {
            QSet<QString> seen;
            for (const auto& ch : StripCharacters::forStage(s)) {
                QVERIFY2(!seen.contains(ch.name),
                         qPrintable(QStringLiteral("duplicate: %1")
                                        .arg(ch.name)));
                seen.insert(ch.name);
            }
        }
    }

    void stages_without_characters_offer_none()
    {
        // The equaliser has its own targets and the reverb has nothing
        // worth presetting. An empty list means the window draws no
        // picker; a one-entry list would be a control that does nothing.
        //
        // The tube was on this list and is not any more. The reasoning
        // for excluding it — that it has real models, so a second picker
        // would blur the difference between a model and a parameter set
        // — was right about the risk and wrong about the remedy. A model
        // alone is nearly inaudible; the character sets the four numbers
        // that make it audible, and the window labels the other one
        // "Waveshaper" so the two cannot be confused.
        QVERIFY(StripCharacters::forStage(Stage::Eq).isEmpty());
        QVERIFY(StripCharacters::forStage(Stage::Reverb).isEmpty());
        QVERIFY(!StripCharacters::forStage(Stage::Tube).isEmpty());
    }

    // ── Applying ─────────────────────────────────────────────────────

    void every_listed_character_can_be_applied()
    {
        // The list and the apply() switch are two places that have to
        // agree, and nothing but this test makes them.
        auto c = makeChain();
        for (Stage s : {Stage::Gate, Stage::Comp, Stage::DeEss,
                        Stage::Tube, Stage::Pudu, Stage::Limiter}) {
            for (const auto& ch : StripCharacters::forStage(s)) {
                QVERIFY2(StripCharacters::apply(*c, s, ch.name),
                         qPrintable(QStringLiteral("listed but not "
                                                   "applicable: %1")
                                        .arg(ch.name)));
            }
        }
    }

    void an_unknown_name_is_refused_not_ignored()
    {
        auto c = makeChain();
        QVERIFY(!StripCharacters::apply(*c, Stage::Gate,
                                        QStringLiteral("Nonexistent")));
        QVERIFY(!StripCharacters::apply(*c, Stage::Eq,
                                        QStringLiteral("Gentle")));
    }

    // Choosing how a stage should sound is not the same as choosing
    // whether it should run. A preset that switches things on is a
    // preset that surprises people mid-contact.
    void applying_a_character_never_changes_the_enabled_flag()
    {
        auto c = makeChain();
        for (Stage s : {Stage::Gate, Stage::Comp, Stage::DeEss,
                        Stage::Tube, Stage::Pudu, Stage::Limiter}) {
            for (bool on : {false, true}) {
                c->setStageEnabled(s, on);
                for (const auto& ch : StripCharacters::forStage(s)) {
                    StripCharacters::apply(*c, s, ch.name);
                    QVERIFY2(c->stageEnabled(s) == on,
                             qPrintable(QStringLiteral("%1 changed the "
                                                       "enabled flag")
                                            .arg(ch.name)));
                }
            }
        }
    }

    // ── The claims the list makes ────────────────────────────────────

    void the_gate_list_really_does_run_gentlest_to_hardest()
    {
        // The order is a promise. "Off" does nothing, "Gate — hard"
        // does the most, and everything between is monotonic in how
        // deeply it can cut.
        auto c = makeChain();
        double previous = -1.0;
        for (const auto& ch : StripCharacters::forStage(Stage::Gate)) {
            QVERIFY(StripCharacters::apply(*c, Stage::Gate, ch.name));
            // How much attenuation this character can reach at all.
            const double depth = -double(c->gate().floorDb());
            QVERIFY2(depth >= previous - 0.001,
                     qPrintable(QStringLiteral("'%1' cuts less deeply than "
                                               "the one before it (%2 vs %3)")
                                    .arg(ch.name).arg(depth).arg(previous)));
            previous = depth;
        }
    }

    void off_really_is_off()
    {
        auto c = makeChain();
        QVERIFY(StripCharacters::apply(*c, Stage::Gate,
                                       QStringLiteral("Off")));
        QCOMPARE(double(c->gate().ratio()), 1.0);
        QCOMPARE(double(c->gate().floorDb()), 0.0);
    }

    void the_compressor_list_runs_gentlest_to_hardest()
    {
        auto c = makeChain();
        double prevRatio = 0.0;
        double prevKnee  = 1e9;
        for (const auto& ch : StripCharacters::forStage(Stage::Comp)) {
            QVERIFY(StripCharacters::apply(*c, Stage::Comp, ch.name));
            const double ratio = double(c->comp().ratio());
            const double knee  = double(c->comp().kneeDb());
            QVERIFY2(ratio >= prevRatio,
                     qPrintable(QStringLiteral("'%1' has a lower ratio than "
                                               "the one before").arg(ch.name)));
            // Harder characters also have harder knees; a high ratio
            // over a wide knee is not the "harder" the name promises.
            QVERIFY2(knee <= prevKnee + 0.001,
                     qPrintable(QStringLiteral("'%1' has a softer knee than "
                                               "the one before").arg(ch.name)));
            prevRatio = ratio;
            prevKnee  = knee;
        }
    }

    // The frequency is the setting people get wrong, and the two
    // voice-shaped characters exist entirely to move it. If they do not
    // differ there they are two names for one thing.
    void the_voice_de_essers_actually_listen_in_different_places()
    {
        auto c = makeChain();
        QVERIFY(StripCharacters::apply(*c, Stage::DeEss,
                                       QStringLiteral("Lower voice")));
        const double low = double(c->deEss().frequencyHz());
        QVERIFY(StripCharacters::apply(*c, Stage::DeEss,
                                       QStringLiteral("Brighter voice")));
        const double high = double(c->deEss().frequencyHz());
        QVERIFY2(high > low + 1000.0,
                 qPrintable(QStringLiteral("%1 Hz vs %2 Hz — not far enough "
                                           "apart to be two settings")
                                .arg(low).arg(high)));
    }

    void wide_and_narrow_differ_in_width_and_not_in_place()
    {
        auto c = makeChain();
        QVERIFY(StripCharacters::apply(*c, Stage::DeEss,
                                       QStringLiteral("Wide")));
        const double wideQ = double(c->deEss().q());
        const double wideF = double(c->deEss().frequencyHz());
        QVERIFY(StripCharacters::apply(*c, Stage::DeEss,
                                       QStringLiteral("Narrow")));
        QVERIFY2(double(c->deEss().q()) > wideQ * 2.0,
                 "Narrow is not appreciably narrower than Wide");
        QCOMPARE(double(c->deEss().frequencyHz()), wideF);
    }

    void the_exciter_characters_use_the_generators_they_are_named_for()
    {
        auto c = makeChain();
        QVERIFY(StripCharacters::apply(*c, Stage::Pudu,
                                       QStringLiteral("Warmth")));
        QVERIFY(double(c->pudu().pooMix()) > 0.0);
        QCOMPARE(double(c->pudu().dooMix()), 0.0);

        QVERIFY(StripCharacters::apply(*c, Stage::Pudu,
                                       QStringLiteral("Presence")));
        QCOMPARE(double(c->pudu().pooMix()), 0.0);
        QVERIFY(double(c->pudu().dooMix()) > 0.0);

        QVERIFY(StripCharacters::apply(*c, Stage::Pudu,
                                       QStringLiteral("Both")));
        QVERIFY(double(c->pudu().pooMix()) > 0.0);
        QVERIFY(double(c->pudu().dooMix()) > 0.0);
    }

    // ── Recognising which one is in effect ───────────────────────────
    //
    // The window used to remember the last name clicked and show it
    // forever, so moving one knob left the label naming a character the
    // stage no longer had. inEffect() replaces that by asking the chain.
    // Everything the label says now rests on these.

    void a_freshly_applied_character_is_recognised()
    {
        auto c = makeChain();
        for (Stage s : {Stage::Gate, Stage::Comp, Stage::DeEss,
                        Stage::Tube, Stage::Pudu, Stage::Limiter}) {
            for (const auto& ch : StripCharacters::forStage(s)) {
                QVERIFY(StripCharacters::apply(*c, s, ch.name));
                const QString found = StripCharacters::inEffect(*c, s);
                QVERIFY2(found == ch.name,
                         qPrintable(QStringLiteral("applied '%1', recognised "
                                                   "'%2'").arg(ch.name, found)));
            }
        }
    }

    void moving_one_knob_afterwards_means_no_character_matches()
    {
        auto c = makeChain();
        QVERIFY(StripCharacters::apply(*c, Stage::Comp,
                                       QStringLiteral("Balanced")));
        QCOMPARE(StripCharacters::inEffect(*c, Stage::Comp),
                 QStringLiteral("Balanced"));

        c->comp().setRatio(c->comp().ratio() + 1.0f);
        QVERIFY2(StripCharacters::inEffect(*c, Stage::Comp).isEmpty(),
                 "a nudged compressor still claimed to be Balanced");

        // And picking it again puts it back — the tooltip on the
        // "edited" mark promises exactly this.
        QVERIFY(StripCharacters::apply(*c, Stage::Comp,
                                       QStringLiteral("Balanced")));
        QCOMPARE(StripCharacters::inEffect(*c, Stage::Comp),
                 QStringLiteral("Balanced"));
    }

    // Several characters deliberately leave a parameter alone — the
    // gate's Ragchew sets its timings but not the threshold, which
    // depends on the room. Comparing against a freshly built stage
    // would report "not this one" for anybody who had ever set their
    // threshold, which is everybody.
    void a_parameter_the_character_does_not_touch_does_not_break_the_match()
    {
        auto c = makeChain();
        QVERIFY(StripCharacters::apply(*c, Stage::Gate,
                                       QStringLiteral("Ragchew")));
        QCOMPARE(StripCharacters::inEffect(*c, Stage::Gate),
                 QStringLiteral("Ragchew"));

        c->gate().setThresholdDb(-33.0f);   // Ragchew never writes this
        QVERIFY2(StripCharacters::inEffect(*c, Stage::Gate)
                     == QStringLiteral("Ragchew"),
                 "setting a threshold Ragchew does not control lost the "
                 "match");
    }

    // Recognition must not be able to change the stage it inspects —
    // the audio thread is reading it at the same time.
    void recognising_a_character_leaves_the_chain_alone()
    {
        auto c = makeChain();
        QVERIFY(StripCharacters::apply(*c, Stage::Tube,
                                       QStringLiteral("Full")));
        for (Stage s : {Stage::Gate, Stage::Comp, Stage::DeEss,
                        Stage::Tube, Stage::Pudu, Stage::Limiter}) {
            const auto before = StripCharacters::captureStage(*c, s);
            StripCharacters::inEffect(*c, s);
            QVERIFY2(StripCharacters::captureStage(*c, s) == before,
                     "inEffect() moved a parameter while looking at it");
        }
    }

    void capture_and_restore_are_the_same_order()
    {
        // If they ever disagree a stage's parameters get shuffled —
        // attack into release — and the only symptom is that characters
        // stop being recognised. Far too quiet for what it would do.
        auto c = makeChain();
        for (Stage s : {Stage::Gate, Stage::Comp, Stage::DeEss,
                        Stage::Tube, Stage::Pudu, Stage::Limiter}) {
            QVERIFY(!StripCharacters::captureStage(*c, s).isEmpty());
            const auto original = StripCharacters::captureStage(*c, s);

            // Move it somewhere else, then put the capture back.
            const auto list = StripCharacters::forStage(s);
            QVERIFY(StripCharacters::apply(*c, s, list.last().name));
            StripCharacters::restoreStage(*c, s, original);

            QVERIFY2(StripCharacters::captureStage(*c, s) == original,
                     qPrintable(QStringLiteral("stage %1 did not survive a "
                                               "capture/restore round trip")
                                    .arg(int(s))));
        }
    }

    void safety_leaves_more_headroom_than_loud()
    {
        auto c = makeChain();
        QVERIFY(StripCharacters::apply(*c, Stage::Limiter,
                                       QStringLiteral("Safety")));
        const double safe = double(c->limiter().ceilingDb());
        QVERIFY(StripCharacters::apply(*c, Stage::Limiter,
                                       QStringLiteral("Loud")));
        QVERIFY(double(c->limiter().ceilingDb()) > safe);
        QVERIFY(safe < 0.0);      // never at or above full scale
    }

    // The list order is the promise, and the limiter's whole character
    // is one number, so the promise is entirely in the ordering.
    void the_limiter_list_runs_from_most_headroom_to_least()
    {
        auto c = makeChain();
        double previous = -1e9;
        for (const auto& ch : StripCharacters::forStage(Stage::Limiter)) {
            QVERIFY(StripCharacters::apply(*c, Stage::Limiter, ch.name));
            const double ceil = double(c->limiter().ceilingDb());
            QVERIFY2(ceil > previous,
                     qPrintable(QStringLiteral("'%1' leaves no less "
                                               "headroom than the one "
                                               "before it (%2 vs %3)")
                                    .arg(ch.name).arg(ceil).arg(previous)));
            // Not one of them may sit at or above full scale, however
            // loud it is called. Everything downstream can overshoot.
            QVERIFY2(ceil < 0.0,
                     qPrintable(QStringLiteral("'%1' has no headroom at "
                                               "all").arg(ch.name)));
            previous = ceil;
        }
    }

    // ── The tube ─────────────────────────────────────────────────────
    //
    // Drive is the number on the control and NOT what is heard: 20 dB of
    // drive with the mix at zero is silence from this stage. The claim
    // the list makes is about how much saturated signal reaches the
    // output, so that is what is asserted.
    void the_tube_list_runs_from_least_saturation_to_most()
    {
        auto c = makeChain();
        double previous = -1.0;
        for (const auto& ch : StripCharacters::forStage(Stage::Tube)) {
            QVERIFY(StripCharacters::apply(*c, Stage::Tube, ch.name));
            const double heard = double(c->tube().driveDb())
                                 * double(c->tube().dryWet());
            QVERIFY2(heard >= previous,
                     qPrintable(QStringLiteral("'%1' saturates less than "
                                               "the one before it (%2 vs "
                                               "%3)")
                                    .arg(ch.name).arg(heard).arg(previous)));
            previous = heard;
        }
    }

    void clean_really_passes_the_dry_signal()
    {
        auto c = makeChain();
        QVERIFY(StripCharacters::apply(*c, Stage::Tube,
                                       QStringLiteral("Clean")));
        QCOMPARE(double(c->tube().dryWet()), 0.0);
    }

    // Bias only does anything on the asymmetric waveshaper. Left behind
    // by a previous character it would quietly change a model that is
    // documented as symmetric, which is the kind of state that makes two
    // identical-looking settings sound different.
    void bias_is_only_left_set_on_the_asymmetric_model()
    {
        auto c = makeChain();
        for (const auto& ch : StripCharacters::forStage(Stage::Tube)) {
            QVERIFY(StripCharacters::apply(*c, Stage::Tube, ch.name));
            if (c->tube().model() != ClientTube::Model::C) {
                QVERIFY2(qFuzzyIsNull(c->tube().biasAmount()),
                         qPrintable(QStringLiteral("'%1' left bias set on "
                                                   "a symmetric model")
                                        .arg(ch.name)));
            }
        }
    }

    // Louder is not better, it is only louder. A character that adds
    // drive without taking the same amount off the output turns every
    // A/B comparison into a volume test.
    void more_drive_comes_with_more_output_trim()
    {
        auto c = makeChain();
        double previousTrim = 1e9;
        for (const auto& ch : StripCharacters::forStage(Stage::Tube)) {
            QVERIFY(StripCharacters::apply(*c, Stage::Tube, ch.name));
            const double trim = double(c->tube().outputGainDb());
            QVERIFY2(trim <= previousTrim + 0.001,
                     qPrintable(QStringLiteral("'%1' is hotter AND louder "
                                               "than the one before")
                                    .arg(ch.name)));
            previousTrim = trim;
        }
    }
};

QTEST_APPLESS_MAIN(TstStripCharacters)
#include "tst_strip_characters.moc"
