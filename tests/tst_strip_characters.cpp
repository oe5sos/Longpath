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

using namespace NereusSDR;
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
                        Stage::Pudu, Stage::Limiter}) {
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
                        Stage::Pudu, Stage::Limiter}) {
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
        // The equaliser has its own targets, the tube has real models,
        // the reverb has nothing worth presetting. An empty list means
        // the window draws no picker; a one-entry list would be a
        // control that does nothing.
        QVERIFY(StripCharacters::forStage(Stage::Eq).isEmpty());
        QVERIFY(StripCharacters::forStage(Stage::Tube).isEmpty());
        QVERIFY(StripCharacters::forStage(Stage::Reverb).isEmpty());
    }

    // ── Applying ─────────────────────────────────────────────────────

    void every_listed_character_can_be_applied()
    {
        // The list and the apply() switch are two places that have to
        // agree, and nothing but this test makes them.
        auto c = makeChain();
        for (Stage s : {Stage::Gate, Stage::Comp, Stage::DeEss,
                        Stage::Pudu, Stage::Limiter}) {
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
                        Stage::Pudu, Stage::Limiter}) {
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
};

QTEST_APPLESS_MAIN(TstStripCharacters)
#include "tst_strip_characters.moc"
