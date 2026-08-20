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

namespace Longpath::StripCharacters {

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

// ── Which one is actually in effect ──────────────────────────────────
//
// Returns the name of the character whose settings the stage currently
// has, or an empty string if it has none of them.
//
// This exists because the window used to remember the last name chosen
// and show it forever. Move one knob afterwards and the label went on
// naming a character the stage no longer had — which is the same class
// of fault as a picture that disagrees with the DSP, and the label was
// the one lying.
//
// ── Only what the character controls is compared ─────────────────────
//
// Several characters deliberately leave parameters alone: the gate's
// "Ragchew" sets its mode, timings and hysteresis but not the
// threshold, because the threshold depends on the room and is the
// operator's to keep. Comparing against a freshly built stage would
// therefore report "not this one" for anybody who had ever touched
// their threshold — technically defensible and useless.
//
// So the comparison starts from the stage AS IT IS, applies the
// character on top, and asks whether anything moved. Parameters the
// character does not touch are equal by construction and drop out of
// the answer, which is exactly the question being asked: is this stage
// still doing what that character says.
QString inEffect(const StripChain& chain, StripChain::Stage stage);

// The stage's parameters as a flat list, in a fixed order, for
// comparing two of them. Exposed because the tests need it and because
// a comparison function nobody can inspect is one nobody can trust.
//
// Only parameters a character may write. The enabled flag is not one —
// see the note on apply().
QVector<float> captureStage(const StripChain& chain,
                            StripChain::Stage stage);
void restoreStage(StripChain& chain, StripChain::Stage stage,
                  const QVector<float>& values);

} // namespace Longpath::StripCharacters
