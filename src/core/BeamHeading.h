#pragma once

// =================================================================
// src/core/BeamHeading.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Which way to point, and how far the rotor has to travel to get there.
//
// ── Short path and long path ─────────────────────────────────────────
//
// Every bearing in the logbook is the short path — the great circle the
// signal takes if nothing is in the way. On the low bands, at grey line,
// or across a pole in winter, the long path is often the stronger
// signal, and it is the short path plus 180°.
//
// Adding 180 in your head is not hard. Doing it at three in the morning
// while a rare station is calling, and getting 275 instead of 95, is.
// This exists so nobody has to.
//
// ── End stops, which are the part people get wrong ───────────────────
//
// A rotor is not a compass. Most have a mechanical stop somewhere —
// commonly at north or at south — and cannot pass through it. Asking a
// north-stop rotor to go from 350° to 10° is a twenty-degree move if it
// can wrap and a three-hundred-and-forty-degree move if it cannot.
//
// Software that ignores this sends the antenna the long way round while
// the operator watches, and on a windy day with a big beam that is not
// merely slow. So the travel is computed against the stop, and the
// answer says how far it will actually turn.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QString>

namespace NereusSDR::BeamHeading {

// Normalise any angle to 0..360.
double wrap360(double deg);

// The long path for a given short-path bearing.
double longPath(double shortPathDeg);

// Where a rotor cannot turn through.
enum class Stop {
    None  = 0,   // continuous rotation, 0 and 360 are the same place
    North = 1,   // cannot pass 0° — the common case
    South = 2,   // cannot pass 180°
};

struct Move {
    bool    reachable{false};
    double  targetDeg{0.0};
    // Degrees the rotor will actually turn. Signed: negative is
    // counter-clockwise. This is the number that says whether a move is
    // twenty degrees or three hundred and forty.
    double  travelDeg{0.0};
    QString note;      // why it is unreachable, or what is unusual
};

// Plan a move from `fromDeg` to `toDeg` for a rotor with `stop`.
//
// With Stop::None the shorter of the two directions wins. With a stop,
// the rotor is confined to one continuous span and there is only one
// route — which may be the long way round, and the returned travel says
// so rather than hiding it.
Move plan(double fromDeg, double toDeg, Stop stop);

// A sentence for the operator, naming the travel and warning when a
// move is a long one. Empty when there is nothing worth saying.
QString advice(const Move& m);

} // namespace NereusSDR::BeamHeading
