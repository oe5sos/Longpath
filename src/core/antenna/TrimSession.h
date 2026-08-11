#pragma once

// =================================================================
// src/core/antenna/TrimSession.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Learning how this antenna actually behaves, from the last two
// measurements.
//
// ── Why the textbook rule is not enough ──────────────────────────────
//
// AntennaTrim assumes f ∝ 1/L: lengthen the wire by one percent and the
// resonance drops by one percent. That is true of an antenna in free
// space and of almost nothing on a summit. Ground under it, the mast,
// a guy line, the counterpoise lying differently — all of them change
// how much a given cut moves the resonance.
//
// The operator finds this out the hard way: the tool said add 22 cm,
// they added 22 cm, and the resonance moved half as far as promised.
// At that point they have the two measurements needed to work out the
// real number, and nobody was doing it.
//
//     f ∝ L^-k    →    k = ln(f₁/f₂) / ln(L₂/L₁)
//
// In the case this was written against, k came out at 0.4986 — the
// antenna responded half as much as the model. Recomputing the next
// step with the measured exponent gives +66 cm where the textbook still
// says +33. Getting that wrong is another trip up the hill.
//
// ── When it refuses to learn ─────────────────────────────────────────
//
// A number derived from two measurements is only as good as the
// difference between them, so it is refused when:
//
//   * the length barely changed — under half a percent, the movement is
//     inside the noise of finding a crossing on a 101-point sweep
//   * the frequency barely moved — under 5 kHz, same reason
//   * the frequency moved the WRONG way. Lengthening a wire lowers its
//     resonance; if it rose, something other than length changed and
//     the exponent comes out negative. That is not a slow antenna, it
//     is a different antenna, and pretending otherwise would feed a
//     nonsense number into the next recommendation.
//   * the result lands outside 0.2 to 5. Beyond that it is not a
//     property of an antenna, it is a mistake somewhere.
//
// In every one of those the session says so and the recommendation
// falls back to the textbook rule, which is wrong in a way that is at
// least familiar.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/antenna/AntennaTrim.h"

#include <QDateTime>
#include <QString>
#include <QVector>

namespace NereusSDR {

class TrimSession {
public:
    struct Observation {
        double    lengthM{0.0};      // total wire, as it was
        double    resonanceHz{0.0};  // what it measured
        QDateTime when;
        QString   label;             // file name, or whatever names it
    };

    // Below these, two measurements are not far enough apart to say
    // anything. Half a percent of 20 m is 10 cm, which moves a 40 m
    // dipole about 36 kHz — comfortably above the kilohertz or so of
    // uncertainty in finding the crossing.
    static constexpr double kMinLengthChangeFraction = 0.005;
    static constexpr double kMinFrequencyChangeHz    = 5000.0;
    static constexpr double kMinExponent = 0.2;
    static constexpr double kMaxExponent = 5.0;

    // Add a measurement. A length of zero is still recorded — the
    // resonance is worth keeping — but nothing can be learned from it.
    void record(double lengthM, double resonanceHz,
                const QString& label = {});

    // Correct the length on the most recent measurement. The operator
    // cuts the wire, loads the new sweep, and only then remembers to
    // type the new length — in that order, which is the natural one.
    // Without this the pair would be compared against the old figure
    // and the exponent would be wrong in a way nothing could catch.
    void updateLastLength(double lengthM);

    const QVector<Observation>& observations() const { return m_obs; }
    void clear() { m_obs.clear(); }
    int  count() const { return int(m_obs.size()); }

    struct Learned {
        bool    valid{false};
        double  exponent{1.0};
        // What actually happened between the last two measurements, and
        // what the textbook rule said would happen. The pair is the
        // interesting output: it is how an operator sees that their
        // ground is eating half the adjustment.
        double  movedHz{0.0};
        double  predictedHz{0.0};
        double  lengthChangeM{0.0};
        QString note;     // why not, or what the numbers mean
    };

    // From the last two observations.
    Learned learned() const;

    // The next step, using the learned exponent when there is one and
    // the textbook rule when there is not.
    AntennaTrim::Trim recommend(AntennaTrim::Kind kind, double targetHz,
                                double currentLengthM) const;

private:
    QVector<Observation> m_obs;
};

} // namespace NereusSDR
