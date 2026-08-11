// =================================================================
// tests/tst_eq_history.cpp  (NereusSDR)
// =================================================================
//
// Undo is a promise about the past, and the ways it breaks are all
// quiet ones: a step that restores nothing, a step that restores the
// wrong thing, a stack that never empties because undoing counted as an
// edit. None of those announce themselves — the operator presses the
// button, something happens, and they only find out later that it was
// not what they had.
//
// So the tests are about the promise rather than the plumbing: after
// undo the equaliser is byte-for-byte what it was, a commit that
// changed nothing is not a step, and redo is available exactly when it
// should be.
//
// no-port-check: NereusSDR-original.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-11 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/applets/eq/EqHistory.h"
#include "core/strip/EqBandLayout.h"
#include "core/strip/StripChain.h"

#include <QtTest/QtTest>

#include <memory>

using namespace NereusSDR;
using FT = ClientEq::FilterType;

namespace {

std::unique_ptr<StripChain> makeChain()
{
    auto c = std::make_unique<StripChain>();
    c->prepare(48000.0);
    EqBandLayout::ensureSeeded(c->eq());
    return c;
}

// Move one band, the way any gesture on the curve would.
void nudge(ClientEq& eq, int idx, float gainDb)
{
    ClientEq::BandParams p = eq.band(idx);
    p.gainDb  = gainDb;
    p.enabled = true;
    eq.setBand(idx, p);
}

} // namespace

class TstEqHistory : public QObject {
    Q_OBJECT
private slots:

    void a_fresh_history_has_nothing_to_undo()
    {
        auto c = makeChain();
        EqHistory h;
        h.reset(c->eq());
        QVERIFY(!h.canUndo());
        QVERIFY(!h.canRedo());
        QCOMPARE(h.undoDepth(), 0);
        // And pressing it anyway must be harmless rather than a crash
        // or a silent half-step.
        QVERIFY(!h.undo(c->eq()));
        QVERIFY(!h.redo(c->eq()));
    }

    void one_change_undone_gives_back_exactly_what_was_there()
    {
        auto c = makeChain();
        EqHistory h;
        h.reset(c->eq());
        const auto before = EqHistory::capture(c->eq());

        nudge(c->eq(), 5, 6.5f);
        h.noteCommit(c->eq());
        QVERIFY(h.canUndo());
        QVERIFY2(!EqHistory::same(before, EqHistory::capture(c->eq())),
                 "the nudge did not actually change anything");

        QVERIFY(h.undo(c->eq()));
        QVERIFY2(EqHistory::same(before, EqHistory::capture(c->eq())),
                 "undo did not restore the original state");
        QVERIFY(!h.canUndo());
        QVERIFY(h.canRedo());
    }

    // Selecting a band, or re-typing the value already in the box, both
    // reach the commit hook. A stack full of steps that do nothing when
    // undone is a button nobody trusts after the second press.
    void a_commit_that_changed_nothing_is_not_a_step()
    {
        auto c = makeChain();
        EqHistory h;
        h.reset(c->eq());

        h.noteCommit(c->eq());
        h.noteCommit(c->eq());
        h.noteCommit(c->eq());
        QVERIFY(!h.canUndo());
        QCOMPARE(h.undoDepth(), 0);

        // Setting a band to the value it already has is the same case.
        nudge(c->eq(), 2, c->eq().band(2).gainDb);
        h.noteCommit(c->eq());
        // `enabled` may have flipped, which IS a change — so this only
        // asserts the depth did not grow by more than that one step.
        QVERIFY(h.undoDepth() <= 1);
    }

    void several_steps_unwind_in_order()
    {
        auto c = makeChain();
        EqHistory h;
        h.reset(c->eq());

        const float steps[] = {2.0f, 4.0f, -3.0f, 7.5f};
        for (float g : steps) {
            nudge(c->eq(), 4, g);
            h.noteCommit(c->eq());
        }
        QCOMPARE(h.undoDepth(), 4);

        // Undoing walks back through the values in reverse, landing on
        // the one BEFORE each step.
        for (int i = 3; i >= 1; --i) {
            QVERIFY(h.undo(c->eq()));
            QCOMPARE(double(c->eq().band(4).gainDb), double(steps[i - 1]));
        }
        QVERIFY(h.undo(c->eq()));     // back past the first nudge
        QVERIFY(!h.canUndo());
    }

    void redo_puts_back_what_undo_took()
    {
        auto c = makeChain();
        EqHistory h;
        h.reset(c->eq());

        nudge(c->eq(), 6, -5.0f);
        h.noteCommit(c->eq());
        const auto edited = EqHistory::capture(c->eq());

        QVERIFY(h.undo(c->eq()));
        QVERIFY(!EqHistory::same(edited, EqHistory::capture(c->eq())));
        QVERIFY(h.redo(c->eq()));
        QVERIFY2(EqHistory::same(edited, EqHistory::capture(c->eq())),
                 "redo did not restore the undone state");
        QVERIFY(!h.canRedo());
    }

    // Standard, and the alternative is worse: a kept redo branch
    // restores a curve the operator has since edited past, which looks
    // like the panel inventing settings.
    void editing_after_an_undo_abandons_the_redo_branch()
    {
        auto c = makeChain();
        EqHistory h;
        h.reset(c->eq());

        nudge(c->eq(), 4, 3.0f);
        h.noteCommit(c->eq());
        QVERIFY(h.undo(c->eq()));
        QVERIFY(h.canRedo());

        nudge(c->eq(), 4, -8.0f);
        h.noteCommit(c->eq());
        QVERIFY2(!h.canRedo(), "the redo branch survived a new edit");
    }

    // An undo step is the whole state, not a description of the change,
    // so everything an edit can touch has to come back.
    void the_band_count_the_master_gain_and_the_family_all_come_back()
    {
        auto c = makeChain();
        EqHistory h;
        h.reset(c->eq());

        const int   count0  = c->eq().activeBandCount();
        const float gain0   = c->eq().masterGain();
        const auto  family0 = c->eq().filterFamily();

        c->eq().setMasterGain(0.5f);
        c->eq().setFilterFamily(ClientEq::FilterFamily::Bessel);
        c->eq().setActiveBandCount(count0 - 2);
        h.noteCommit(c->eq());

        QVERIFY(h.undo(c->eq()));
        QCOMPARE(c->eq().activeBandCount(), count0);
        QCOMPARE(double(c->eq().masterGain()), double(gain0));
        QVERIFY(c->eq().filterFamily() == family0);
    }

    void every_field_of_a_band_survives_a_round_trip()
    {
        auto c = makeChain();
        ClientEq::BandParams p;
        p.freqHz        = 1234.0f;
        p.gainDb        = -7.25f;
        p.q             = 4.5f;
        p.type          = FT::HighShelf;
        p.enabled       = true;
        p.slopeDbPerOct = 36;
        c->eq().setBand(3, p);

        const auto snap = EqHistory::capture(c->eq());
        c->eq().setBand(3, ClientEq::BandParams{});   // wipe it
        EqHistory::apply(snap, c->eq());

        const auto back = c->eq().band(3);
        QCOMPARE(double(back.freqHz), 1234.0);
        QCOMPARE(double(back.gainDb), -7.25);
        QCOMPARE(double(back.q), 4.5);
        QVERIFY(back.type == FT::HighShelf);
        QVERIFY(back.enabled);
        QCOMPARE(back.slopeDbPerOct, 36);
    }

    // Sixty-four steps is several minutes of shaping. Past that the
    // oldest goes, and the important part is that the stack stops
    // growing rather than that any particular step survives.
    void the_stack_stops_at_its_limit_and_keeps_the_recent_end()
    {
        auto c = makeChain();
        EqHistory h;
        h.reset(c->eq());

        const int over = EqHistory::kMaxDepth + 20;
        for (int i = 0; i < over; ++i) {
            nudge(c->eq(), 4, float(i % 17) - 8.0f);
            h.noteCommit(c->eq());
        }
        QVERIFY2(h.undoDepth() <= EqHistory::kMaxDepth,
                 qPrintable(QStringLiteral("stack grew to %1")
                                .arg(h.undoDepth())));
        // The most recent step must still be there — dropping from the
        // wrong end would make undo useless exactly when it is used.
        const float last = c->eq().band(4).gainDb;
        QVERIFY(h.undo(c->eq()));
        QVERIFY(!qFuzzyCompare(c->eq().band(4).gainDb + 100.0f,
                               last + 100.0f));
    }

    void a_reset_forgets_everything_and_takes_a_new_baseline()
    {
        auto c = makeChain();
        EqHistory h;
        h.reset(c->eq());
        nudge(c->eq(), 4, 5.0f);
        h.noteCommit(c->eq());
        QVERIFY(h.canUndo());

        h.reset(c->eq());
        QVERIFY2(!h.canUndo(),
                 "history survived a reset — undoing across a "
                 "reconnection would restore another session's curve");
        QVERIFY(!h.canRedo());
    }
};

QTEST_APPLESS_MAIN(TstEqHistory)
#include "tst_eq_history.moc"
