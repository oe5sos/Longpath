#pragma once

// =================================================================
// src/core/strip/StripTargets.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// What a voice should look like, for the job it is doing.
//
// There is no single right answer, and pretending otherwise is the
// usual failure of automatic audio setup. A contest signal and a
// ragchew signal want opposite things: one wants every decibel inside
// the two kilohertz a crowded receiver will pass, the other wants to be
// pleasant for an hour. The transmit bandwidth changes the answer
// again — a 3.3 kHz channel has room for a top end that a 2.7 kHz
// channel throws away, so aiming for the same shape in both wastes the
// wider one and overdrives the narrower.
//
// So the target is a named choice, made before shaping rather than
// discovered afterwards, and each one says what it is for.
//
// The curves are relative to 1 kHz and are opinions, clearly labelled.
// They are not measurements and nothing here should draw them as if
// they were.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QString>
#include <QVector>

namespace NereusSDR::StripTargets {

struct Profile {
    QString name;
    QString description;
    // Where the transmit filter closes. Above this the target falls
    // away hard, because energy there costs ALC and never arrives.
    double  topHz;
};

QVector<Profile> profiles();

// The target in dB relative to 1 kHz, for a named profile. An unknown
// name falls back to the general SSB curve rather than to silence — a
// missing profile should not produce a flat line that looks deliberate.
double targetDb(const QString& profile, double hz);

} // namespace NereusSDR::StripTargets
