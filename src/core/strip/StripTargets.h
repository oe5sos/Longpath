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

// ── The operator's own target ────────────────────────────────────────
//
// Five built-in curves are five opinions, and the bench's answer to all
// five was "that is not what I want". That is not a reason for a sixth
// opinion — it is a reason for the target to stop being an opinion at
// all and become something the operator draws.
//
// Twelve control points, log-spaced across the voice range, in dB
// relative to 1 kHz. Twelve because it is enough to describe any shape
// a voice needs and few enough to drag; the curve between them is
// interpolated the same way the built-ins are, so a custom target and a
// built-in one behave identically everywhere downstream.
inline constexpr int kUserPointCount = 12;

// The frequencies of those points. Fixed, so the saved file and the
// drag handles agree about which is which without storing frequencies
// alongside every gain.
const double* userPointFreqs();

// Read and write the operator's curve. Persisted under the same
// settings prefix as the rest of the strip.
QVector<double> userTarget();
void setUserTarget(const QVector<double>& db);

// Fill the operator's curve from a built-in, so "not quite right" can
// start from the nearest thing rather than from flat.
void seedUserTargetFrom(const QString& profileName);

// The name the picker shows for it.
inline constexpr char kUserProfileName[] = "Mine";

// The target in dB relative to 1 kHz, for a named profile. An unknown
// name falls back to the general SSB curve rather than to silence — a
// missing profile should not produce a flat line that looks deliberate.
double targetDb(const QString& profile, double hz);

} // namespace NereusSDR::StripTargets
