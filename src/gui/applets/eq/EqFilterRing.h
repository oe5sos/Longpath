#pragma once

// =================================================================
// src/gui/applets/eq/EqFilterRing.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// One ring of filter types, and one function that changes a band onto
// one of them.
//
// ── Why this exists ──────────────────────────────────────────────────
//
// Three places in the equaliser change a band's filter type, and until
// this header they were three separate implementations:
//
//   ClientEqIconRow   click an icon — cycles Peak → LS → HS → LP → HP
//   ClientEqEditorCanvas  right-click menu — assigns a type directly
//   ClientEqEditorCanvas  double-click a handle — added 2026-08-11
//
// The third was written with the ring in the other order (HP before LP)
// without noticing the first existed. Two gestures that both mean
// "cycle the type" walking different rings is the kind of fault nobody
// reports as a bug — the operator just quietly decides the equaliser is
// unpredictable and stops using one of them.
//
// The rotor panel taught this lesson already in this project: the
// logbook's turn and the panel's turn button were two paths for one
// action and behaved differently. Same remedy — one function, three
// callers.
//
// ── The ring order is the shipped one ────────────────────────────────
//
// Peak → Low Shelf → High Shelf → Low Pass → High Pass → Peak, which is
// simply the enum's own order. It is not the order I would choose from
// scratch — putting the two slopes next to the two shelves would read
// better — but it is what the icon row has always done, and changing a
// gesture people have muscle memory for to make a comment read nicer is
// not a trade worth making.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-11 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/ClientEq.h"

namespace Longpath::EqFilterRing {

// How many entries the ring has. A static_assert cannot check an enum's
// size, so this is the one place the count lives and step() is written
// against it.
inline constexpr int kTypeCount = 5;

// Move `direction` places round the ring. +1 is forward, -1 back;
// anything else is taken modulo the ring, so a mouse wheel can be
// handed straight in.
inline ClientEq::FilterType step(ClientEq::FilterType t, int direction)
{
    int i = static_cast<int>(t) + direction;
    i = ((i % kTypeCount) + kTypeCount) % kTypeCount;   // never negative
    return static_cast<ClientEq::FilterType>(i);
}

inline ClientEq::FilterType next(ClientEq::FilterType t) { return step(t, 1); }
inline ClientEq::FilterType prev(ClientEq::FilterType t) { return step(t, -1); }

// A readable name, for menus and for the label under the handle.
inline const char* name(ClientEq::FilterType t)
{
    switch (t) {
    case ClientEq::FilterType::Peak:      return "Peak";
    case ClientEq::FilterType::LowShelf:  return "Low Shelf";
    case ClientEq::FilterType::HighShelf: return "High Shelf";
    case ClientEq::FilterType::LowPass:   return "Low Pass";
    case ClientEq::FilterType::HighPass:  return "High Pass";
    }
    return "Peak";
}

// The single place a band's type changes.
//
// Beyond assigning the type it does two things, and both are the reason
// this is a function rather than three assignments:
//
//   It enables the band. Reshaping a band is an explicit statement that
//   you want it working; the default slots ship disabled so the EQ is
//   transparent until touched.
//
//   It tames Q. A peak at Q 8 is a narrow notch and useful. The same
//   figure on a shelf or a slope is a resonant hump sitting on the
//   corner frequency — not what anybody means by "make this a
//   high-pass", and it arrives as a surprise because the number did not
//   change. Only ever clamped downward, so a deliberately resonant
//   slope is still one drag away.
//
// gainDb is left alone even for the slopes, which ignore it. Keeping it
// means stepping once more round the ring gives back the curve that was
// there rather than a flat band.
inline void applyTo(ClientEq& eq, int bandIdx, ClientEq::FilterType t)
{
    ClientEq::BandParams p = eq.band(bandIdx);
    p.type    = t;
    p.enabled = true;
    if (t != ClientEq::FilterType::Peak && p.q > 1.0f) {
        p.q = 0.707f;
    }
    eq.setBand(bandIdx, p);
}

} // namespace Longpath::EqFilterRing
