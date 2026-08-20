#pragma once

// =================================================================
// src/gui/applets/eq/ClientEqApplet.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original stand-in for one enum from AetherSDR
// (https://github.com/aethersdr/AetherSDR), GPLv3, primary author
// Jeremy [KK7GWY]: src/gui/ClientEqApplet.h at 31b29583.
//
// The ported StripEqPanel names `ClientEqApplet::Path` in its public
// signals and members. AetherSDR's ClientEqApplet is a docked applet
// NereusSDR has no use for — it is the receive-side equaliser panel,
// and NereusSDR's receive path goes through WDSP with its own applet.
//
// Porting the whole class to obtain one two-value enum would import a
// widget nobody would ever show. Declaring the enum here keeps the
// ported panel byte-comparable against upstream without dragging in
// its neighbour.
//
// Rx is kept even though NereusSDR never selects it, because removing
// a value from a ported enum silently renumbers the other one, and a
// setting written as 1 would come back as 0.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

namespace Longpath {

class ClientEqApplet {
public:
    enum class Path { Rx = 0, Tx = 1 };
};

} // namespace Longpath
