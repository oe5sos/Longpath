// =================================================================
// src/core/strip/BroadcastOptimizer.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Siehe BroadcastOptimizer.h -- insbesondere,
// warum hier keine Ketten mehr hergeleitet werden.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-10-09 — Herleitung von Ketten entfernt; es bleiben Kanal,
//                 Kanalfilter, Messbericht und das Ablegen der von
//                 Hand eingestellten Kette. Entscheidung des
//                 Betreibers nach einem Tag Gegenproben.
// =================================================================

#include "core/strip/BroadcastOptimizer.h"

#include "core/strip/StripChain.h"
#include "core/strip/StripSettings.h"

#include <algorithm>
#include <cmath>

namespace Longpath::BroadcastOptimizer {

namespace {

// The same band indices StripTuner uses, and duplicated here for the
// same reason it gives: core code must not depend on a window, and the
// layout is pinned by a test so the two cannot drift apart silently.
constexpr int kBandHighPass = 0;
constexpr int kBandHum1     = 1;
constexpr int kBandLow      = 4;
constexpr int kBandPresence = 5;
constexpr int kBandHigh     = 6;

// ── Per-width shaping ────────────────────────────────────────────────
//
// One row per width, so the four profiles differ in a table that can
// be read in one go rather than in four branches scattered through the
// function. Every column is explained where it is used.
struct WidthRule {
    double      widthHz;
    int         lowCutHz;
    int         highCutHz;
    const char* headline;
};

// Die vier Breiten, mit den Zahlen des Betreibers (2026-10-09): "ein
// dx oder contest profil ist im bereich 200-2500(2700), ein 4 k profil
// ist von ca 50-4050." Dazwischen gestuft. Der Kanal oeffnet sich an
// BEIDEN Enden -- unten liegt der groessere Teil des Unterschieds.
constexpr WidthRule kRules[kWidthCount] = {
    {2300.0, 200, 2500,
     "Contest. Lean and narrow; cuts through a crowded band."},
    {2700.0, 150, 2700,
     "The everyday setting — what most of the band is listening on."},
    {3200.0, 100, 3200,
     "Room for the body of your voice. Polite when the band is quiet."},
    {4000.0,  50, 4050,
     "Widest. Two octaves more body at the bottom than Contest."},
};

const WidthRule& ruleFor(double widthHz)
{
    const WidthRule* best = &kRules[0];
    double bestGap = std::abs(kRules[0].widthHz - widthHz);
    for (int i = 1; i < kWidthCount; ++i) {
        const double gap = std::abs(kRules[i].widthHz - widthHz);
        if (gap < bestGap) {
            bestGap = gap;
            best    = &kRules[i];
        }
    }
    return *best;
}

void setBandGain(StripChain& c, int idx, double gainDb, bool enabled)
{
    ClientEq::BandParams p = c.eq().band(idx);
    p.gainDb  = float(gainDb);
    p.enabled = enabled;
    c.eq().setBand(idx, p);
}

QString widthLabel(double widthHz)
{
    // 2300 reads as "2.3k". One decimal, because the four widths the
    // operator asked for all land on one and a trailing ".0" on 4 kHz
    // would be noise.
    return QStringLiteral("%1k").arg(widthHz / 1000.0, 0, 'f', 1);
}

} // namespace

// ── Names and the density axis ───────────────────────────────────────

QString presetName(double widthHz)
{
    return QStringLiteral("Broadcast %1").arg(widthLabel(widthHz));
}

// ── The plans ────────────────────────────────────────────────────────

QVector<Plan> plan(const VoiceAnalysis& a)
{
    QVector<Plan> out;
    if (!a.valid || a.clippedSamples > 0) { return out; }

    out.reserve(kWidthCount);
    for (const WidthRule& rule : kRules) {
        Plan p;
        p.widthHz   = rule.widthHz;
        p.name      = presetName(rule.widthHz);
        p.headline  = QString::fromUtf8(rule.headline);
        p.lowCutHz  = rule.lowCutHz;
        p.highCutHz = rule.highCutHz;

        p.notes << QStringLiteral(
            "Channel %1 to %2 Hz. That is the whole difference between "
            "these four — what is set inside the strip is yours, and "
            "this window does not touch it.")
            .arg(p.lowCutHz).arg(p.highCutHz);

        // ── Was die Messung zu DIESER Breite sagt ───────────────────
        if (a.pitchHz > 0.0) {
            p.notes << QStringLiteral(
                "Your fundamental measures %1 Hz. A low cut at %2 Hz "
                "leaves it %3.")
                .arg(a.pitchHz, 0, 'f', 0)
                .arg(p.lowCutHz)
                .arg(double(p.lowCutHz) < a.pitchHz * 0.9
                     ? QStringLiteral("untouched")
                     : QStringLiteral("inside the filter's skirt — the "
                                      "body of your voice goes with it"));
        }
        if (a.sibilanceDb > -3.0) {
            p.notes << QStringLiteral(
                "Sibilance measures %1 dB against the midrange, loudest "
                "around %2 Hz — a de-esser there is worth a try, and %3 "
                "passes %4 of it.")
                .arg(a.sibilanceDb,  0, 'f', 1)
                .arg(a.sibilanceHz,  0, 'f', 0)
                .arg(widthLabel(rule.widthHz))
                .arg(a.sibilanceHz < double(p.highCutHz)
                     ? QStringLiteral("all") : QStringLiteral("none"));
        }
        {
            const double snr = a.speechDbFs - a.noiseFloorDbFs;
            p.notes << QStringLiteral(
                "Room is %1 dB under your voice (%2 dBFS against "
                "%3 dBFS). %4")
                .arg(snr, 0, 'f', 0)
                .arg(a.speechDbFs, 0, 'f', 0)
                .arg(a.noiseFloorDbFs, 0, 'f', 0)
                .arg(snr < 20.0
                     ? QStringLiteral("Close enough that a gate will "
                                      "chew word endings; a quieter "
                                      "room beats any setting.")
                     : QStringLiteral("Quiet enough to work with."));
        }
        if (a.loudnessRangeValid) {
            p.notes << QStringLiteral(
                "Loudness %1 LUFS, range %2 LU.%3")
                .arg(a.loudnessLufs,    0, 'f', 1)
                .arg(a.loudnessRangeLu, 0, 'f', 1)
                .arg(a.loudnessRangeLu > 10.0
                     ? QStringLiteral(" That range is you moving "
                                      "between sentences — no "
                                      "compressor setting answers it.")
                     : QString());
        }

        if (a.rolloffHz > 0.0 && a.rolloffHz < rule.widthHz * 0.95) {
            p.caveat = QStringLiteral(
                "Your microphone is already 20 dB down by %1 Hz, below "
                "this channel's %2 Hz. The top of it will carry hiss "
                "rather than voice.")
                .arg(a.rolloffHz, 0, 'f', 0)
                .arg(rule.widthHz, 0, 'f', 0);
        }

        out.push_back(p);
    }
    return out;
}

int bestIndex(const QVector<Plan>& plans, double currentTxWidthHz)
{
    if (plans.isEmpty()) { return -1; }
    int    best    = 0;
    double bestGap = std::abs(plans[0].widthHz - currentTxWidthHz);
    for (int i = 1; i < plans.size(); ++i) {
        const double gap = std::abs(plans[i].widthHz - currentTxWidthHz);
        if (gap < bestGap) {
            bestGap = gap;
            best    = i;
        }
    }
    return best;
}

bool saveCurrentAsProfile(const Plan& p, const StripChain& chain)
{
    if (p.name.isEmpty()) { return false; }
    return StripSettings::saveUserPreset(p.name, chain);
}

void applyChannelFilter(float* samples, int frames, int sampleRate,
                        const Plan& p)
{
    if (samples == nullptr || frames <= 0 || sampleRate <= 0) { return; }

    struct Bq { double b0, b1, b2, a1, a2; };
    const auto run = [&](const Bq& f) {
        double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
        for (int i = 0; i < frames; ++i) {
            const double in = samples[i];
            const double out = f.b0 * in + f.b1 * x1 + f.b2 * x2
                             - f.a1 * y1 - f.a2 * y2;
            x2 = x1; x1 = in;
            y2 = y1; y1 = out;
            samples[i] = float(out);
        }
    };
    // ── Butterworth vierter Ordnung, richtig gestuft ────────────────
    //
    // Zwei zweite Ordnungen mit DEMSELBEN Q (0,7071) sind kein
    // Butterworth vierter Ordnung, sondern zwei gestapelte zweiter:
    // -6 dB schon AUF der Ecke und merklicher Abfall eine Oktave
    // darunter. Bei 4000 Hz faellt das ab etwa 2 kHz -- und genau so
    // klang es: „das 4k profil hoert sich an als waeren es 2,7"
    // (Betreiber, 2026-10-09).
    //
    // Ein echter Butterworth braucht zwei VERSCHIEDENE Guetefaktoren,
    // 0,5412 und 1,3066. Dann ist der Durchlassbereich flach bis zur
    // Ecke, dort -3 dB, und danach 24 dB je Oktave -- was ein
    // Sendefilter tatsaechlich tut.
    static constexpr double kButterQ[2] = {0.54119610, 1.30656296};

    const auto lowPassAt = [&](double hz, double q) {
        const double w  = 2.0 * M_PI * hz / sampleRate;
        const double cs = std::cos(w);
        const double al = std::sin(w) / (2.0 * q);
        const double a0 = 1.0 + al;
        return Bq{(1.0 - cs) / 2.0 / a0, (1.0 - cs) / a0,
                  (1.0 - cs) / 2.0 / a0, -2.0 * cs / a0, (1.0 - al) / a0};
    };
    const auto highPassAt = [&](double hz, double q) {
        const double w  = 2.0 * M_PI * hz / sampleRate;
        const double cs = std::cos(w);
        const double al = std::sin(w) / (2.0 * q);
        const double a0 = 1.0 + al;
        return Bq{(1.0 + cs) / 2.0 / a0, -(1.0 + cs) / a0,
                  (1.0 + cs) / 2.0 / a0, -2.0 * cs / a0, (1.0 - al) / a0};
    };

    if (p.lowCutHz > 20) {
        for (double q : kButterQ) {
            run(highPassAt(double(p.lowCutHz), q));
        }
    }
    if (p.highCutHz > 100 && double(p.highCutHz) < sampleRate * 0.45) {
        for (double q : kButterQ) {
            run(lowPassAt(double(p.highCutHz), q));
        }
    }
}

} // namespace Longpath::BroadcastOptimizer
