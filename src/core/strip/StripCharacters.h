#pragma once

// =================================================================
// src/core/strip/StripCharacters.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// A named starting point for each stage, and a sentence saying what it
// is for.
//
// ── What these are, and what they are not ────────────────────────────
//
// They are parameter sets. Choosing one moves several knobs at once to
// values that go together, and then the operator adjusts from there.
//
// They are NOT different algorithms. That distinction matters and is
// worth stating plainly, because the tube stage does have real models —
// ClientTube::Model A, B and C are three different waveshapers,
// genuinely different arithmetic.
//
// The tube was left without characters for exactly that reason at first,
// and that was the wrong call. A model on its own is nearly inaudible:
// A, B and C only separate once drive, bias and mix move with them, and
// moving four controls in step is the definition of a character. So the
// tube has both now — a character that picks a model and sets the four
// numbers around it, and the model picker underneath still showing which
// waveshaper is running and still free to be changed. The two are
// labelled differently in the window so nobody has to guess which is
// which.
//
// The gate already worked this way before any of this: ClientGate::Mode
// is documented upstream as "snap ratio + range to canonical presets",
// with two entries. This is that idea with more entries and a reason
// attached to each, and it touches no ported DSP.
//
// ── Why each one has a description ───────────────────────────────────
//
// A list of names is a quiz. "Voodoo" and "Contest" mean nothing to
// somebody who has not already decided what they want, and the operator
// who most needs a preset is exactly the one who cannot tell them apart.
// Every character therefore says what it is for and what it costs,
// because a preset with a cost hidden inside it is a trap.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/StripChain.h"

#include <QString>
#include <QVector>

namespace NereusSDR::StripCharacters {

struct Character {
    QString name;
    // One or two sentences: what it is for, and what it costs. Shown
    // next to the picture, not buried in a tooltip.
    QString description;
};

// The characters offered for a stage, in the order they should be
// listed — gentlest first, so the list itself reads as a scale.
// Empty for stages that have none (the equaliser has its own targets,
// the reverb has nothing worth presetting).
QVector<Character> forStage(StripChain::Stage stage);

// Apply one by name. Returns false for an unknown name rather than
// silently doing nothing, so a typo in a settings file is visible.
//
// Deliberately does NOT touch the stage's enabled flag. Choosing a
// character is a statement about how the stage should sound, not about
// whether it should run, and a preset that switches things on is a
// preset that surprises people.
bool apply(StripChain& chain, StripChain::Stage stage, const QString& name);

} // namespace NereusSDR::StripCharacters
