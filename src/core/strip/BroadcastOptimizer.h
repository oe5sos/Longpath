#pragma once

// =================================================================
// src/core/strip/BroadcastOptimizer.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// One recording, four transmit bandwidths, four finished profiles.
//
// ── The job, in the operator's words ─────────────────────────────────
//
// "Speak for fifteen seconds with no settings as a baseline, have all
// of it analysed automatically and as carefully as possible to get the
// best possible profile. That can then be adjusted afterwards if
// wanted. The other profiles should fall out of it automatically."
//                                           — OE5SOS, 2026-10-09
//
// So: 2.3, 2.7, 3.5 and 4.0 kHz, each a complete chain, all four
// derived from the same measurement, none of them a preset somebody
// guessed in advance.
//
// ── Why the bandwidth changes the answer at all ──────────────────────
//
// It is tempting to think one good sound can be sent through any
// filter, and that a narrow channel simply gets less of it. That is
// wrong in both directions:
//
//   Too wide for the channel. Energy above the transmit filter does
//   not vanish politely. It reaches the compressor and the ALC first,
//   where it takes gain reduction with it — so a 4 kHz-shaped voice
//   sent through a 2.3 kHz filter is quieter AND duller than the same
//   voice shaped for 2.3 kHz. The operator pays twice for audio
//   nobody hears.
//
//   Too narrow for the channel. A 2.3 kHz shape in a 4 kHz channel
//   leaves the top one and a half octaves empty and the low end cut
//   for no reason. It sounds like a telephone on a band that had room
//   for a voice.
//
// The high-pass moves for the same reason, and it moves against the
// operator's own pitch rather than against a fixed number: the first
// harmonics of a 95 Hz voice and a 180 Hz voice sit an octave apart,
// so one high-pass cannot be right for both. That is what the new
// pitch measurement in VoiceAnalyzer is for.
//
// ── What this will not do ────────────────────────────────────────────
//
//   Boost. Same rule as VoiceAnalyzer::suggestEq and StripTuner: every
//   EQ move here is a cut, and the level comes back as make-up gain.
//   An optimiser that reaches a target by boosting is an optimiser
//   that amplifies whatever the microphone does worst.
//
//   Touch the live chain. plan() is a pure function of the
//   measurement; saveAsPresets() writes named presets and nothing
//   else. The chain only changes when the operator picks one and
//   apply() is called — which is the whole reason this can sit beside
//   the existing voice-check path instead of replacing it (operator,
//   2026-10-09: "build it parallel to our current audio setup and we
//   decide later, or we keep using the current one").
//
//   Switch the strip on. Nothing in core/strip/ ever does. The
//   operator decides when their transmit audio changes.
//
//   Promise. Every plan carries what it costs as well as what it
//   buys, and a plan that is not worth taking says so in `caveat`
//   rather than being quietly offered as an equal. A 4 kHz profile
//   built from a microphone that stops at 3 kHz is an empty promise,
//   and the measurement can see that.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/VoiceAnalyzer.h"

#include <QString>
#include <QStringList>
#include <QMetaType>
#include <QVector>

namespace Longpath {

class StripChain;

namespace BroadcastOptimizer {

// ── The four widths ──────────────────────────────────────────────────
//
// Asked for by name. They are not arbitrary: 2.3 kHz is what a crowded
// receiver is set to, 2.7 kHz is the everyday default most of the band
// is listening on, 3.5 kHz is as wide as is polite when the band is
// quiet, and 4.0 kHz is the one that only pays off when the other
// operator has opened their filter to match.
inline constexpr int kWidthCount = 4;
inline constexpr double kWidthHz[kWidthCount] = {
    2300.0, 2700.0, 3200.0, 4000.0,
};

// The name a profile is saved and listed under. Short, sorts in width
// order, and says where it came from.
QString presetName(double widthHz);

// ── Density, the second axis ─────────────────────────────────────────
//
// Taken from Zeus, which is the benchmark here (operator, 2026-10-09:
// "Zeus with all its tools is surely the measure"). Zeus carries a
// single `TargetSpectralDensity` on every TX audio profile — an
// integer 0..100, default 55 (`Station.Engine.Hosting/
// TxFidelityPolicyStore.cs:DefaultTargetn`) — rather than exposing a
// ratio and a threshold. That is the better control and it is adopted
// wholesale: one number the operator can move, with the compressor
// derived from it instead of the other way round.
//
// The four named stops are Longpath's own placement on Zeus's scale.
// Zeus Link shows the same four words over its density row; the
// numbers behind them live in its client, which is not in the engine
// source here, so these are chosen to put Broadcast on Zeus's
// documented default and spread the rest evenly around it.
inline constexpr int kDensityNatural   = 30;
inline constexpr int kDensityBroadcast = 55;   // Zeus's own default
inline constexpr int kDensityDense     = 75;
inline constexpr int kDensityMaximum   = 92;

// The name for a density value — the nearest stop, so a hand-edited
// 58 still reads as "Broadcast" rather than as a bare number.
QString densityName(int density);

// What the compressor is aiming at: the crest factor the voice should
// still have AFTER processing, in dB. This is the whole density axis
// in one function, and it is exposed so it can be tested on numbers
// and drawn on a scale.
//
// At 0 the voice keeps its own dynamics. At 100 it is squeezed to
// about 6 dB, which is as far as speech goes before it stops sounding
// like a person. Nothing here clips.
double targetCrestDb(int density);

// Everything the optimiser decided for one width, and why.
//
// Kept as plain numbers rather than as a half-applied chain so the
// whole thing can be tested without a StripChain, drawn before it is
// applied, and shown side by side with its three siblings.
struct Plan {
    double  widthHz{2700.0};
    QString name;

    // ── The transmit filter the profile is FOR ───────────────────────
    //
    // Carried on the plan because Zeus carries it on the profile:
    // `TxAudioProfileDto` holds `LowCutHz` / `HighCutHz` alongside the
    // chain, so a named profile in Zeus is the whole answer — shaping
    // and channel together — not a shape that hopes the filter happens
    // to match. A "2.3 k profile" whose transmit filter is still at
    // 2.7 kHz is not a 2.3 k profile.
    //
    // These are reported, not applied: the transmit bandpass is WDSP's
    // and belongs to TransmitModel, which is deliberately out of reach
    // from core/strip/. apply() sets the chain; whoever offers the
    // plan to the operator is the one that may also move the filter,
    // visibly and on purpose.
    int     lowCutHz{100};
    int     highCutHz{2700};

    // Zeus's axis, 0..100. See kDensityBroadcast.
    int     density{kDensityBroadcast};

    // ── The chain ────────────────────────────────────────────────────
    double  highPassHz{120.0};
    // ── The two shelves follow the channel, not a constant ───────────
    //
    // Their CORNERS are per-width, and that is the whole point: a
    // profile that differs from its sibling only in how much it cuts
    // at a fixed 3 kHz is barely a different profile. The top shelf
    // sits below the channel edge so the roll-off starts inside the
    // channel rather than after it, and the bottom one just above the
    // high-pass so the two do not fight.
    //
    // This was found the hard way. The first version wrote only the
    // gains and left the default layout's corners alone — where band
    // 6 is a PEAK at 3 kHz, not a shelf at all. Four profiles whose
    // top-end difference was two decibels at 4 kHz, which is what
    // "alle profile hören sich gleich an" sounded like.
    double  lowShelfDb{0.0};        // never above 0 — cuts only
    double  lowShelfHz{180.0};
    double  presenceDb{0.0};
    double  highShelfDb{0.0};
    double  highShelfHz{2000.0};

    bool    humNotches{false};
    int     humBaseHz{0};

    bool    gateOn{false};
    double  gateThresholdDbFs{-60.0};

    bool    deEssOn{false};
    double  deEssHz{6000.0};
    double  deEssAmountDb{0.0};     // negative = reduction

    double  compRatio{2.5};
    double  compThresholdDbFs{-24.0};
    double  compMakeupDb{0.0};

    double  limiterCeilingDb{-1.0};

    // ── The stages this profile deliberately forgets ─────────────────
    //
    // Operator, 2026-10-09: "for contest various filters have to be
    // forgotten; you'd use those from the 2.7 k profile upward."
    //
    // That is not only a preference, it is the published practice.
    // NU9N's transmit-chain guide marks the noise gate, the soft
    // compressor, the spectral enhancer, the peak limiter and the
    // effects as OPTIONAL, and leaves exactly two stages unmarked —
    // the equaliser and the compressor. The same page is blunt about
    // the other end: broadcast processors "are not specific enough
    // unless you are transmitting at 6 kHz of audio bandwidth or
    // more. Their flexibility is very limited below 6 kHz."
    // (https://www.nu9n.com/tx.html)
    //
    // So the narrow profiles run the two stages that earn their
    // place and switch the rest OFF rather than leaving them as they
    // were. Left alone, a valve stage from a previous profile would
    // keep colouring a contest signal that wanted none of it, and
    // the operator would have no way to tell which profile they were
    // actually hearing.
    bool    characterStages{true};   // tube, exciter, reverb

    // What the operator is told about that decision, so a stage
    // going dark is never a surprise.
    QString forgotten;

    // ── What it is worth ─────────────────────────────────────────────
    //
    // An estimate, clearly labelled as one wherever it is shown. It is
    // the average-level gain the chain is expected to produce against
    // the unprocessed take: make-up plus the part of the crest factor
    // the compressor actually takes. It is NOT a measurement of the
    // result, and the only honest measurement of the result is the
    // next recording.
    double  densityGainDb{0.0};

    // One line each, in the order they matter. Written to be read.
    QStringList notes;

    // Empty when the plan is worth taking as it stands. Set when the
    // measurement says this width cannot deliver what its name claims
    // — a microphone that rolls off below the channel, a room too
    // noisy to open the top end. The plan is still produced, because
    // refusing to produce it would hide the reason.
    QString caveat;
};

// ── The whole job ────────────────────────────────────────────────────
//
// Pure. Takes the measurement, returns four plans in kWidthHz order.
// An invalid or clipped measurement returns an empty vector rather
// than four confident plans built on a spectrum that is not real —
// the same refusal StripTuner makes, for the same reason.
QVector<Plan> plan(const VoiceAnalysis& a);

// The one to start from, out of the four: the plan for the width
// closest to what the transmitter is actually set to. Returns an index
// into plan()'s result, or -1 when there are no plans.
int bestIndex(const QVector<Plan>& plans, double currentTxWidthHz);

// ── Putting one into the chain ───────────────────────────────────────
//
// Sets every stage the plan names and switches those stages on. Does
// NOT touch the master switch, and does not save anything. After this
// the operator can adjust from there, which is the point — the plan is
// a starting point that happens to be derived from their own voice
// rather than from somebody else's taste.
bool apply(const Plan& p, StripChain& chain);

// ── Writing all four out ─────────────────────────────────────────────
//
// Applies each plan to a scratch chain and saves it through
// StripSettings::saveUserPreset, so an optimiser profile and a
// hand-made one are the same kind of thing and appear in the same
// list. Returns how many were written.
//
// The caller's own chain is never touched: the four profiles exist to
// be chosen from, not to be imposed.
int saveAsPresets(const QVector<Plan>& plans);

} // namespace BroadcastOptimizer
} // namespace Longpath

// Carried through a Qt signal, and a QSignalSpy stores what it sees in
// a QVariant — which needs the type declared. Without this the spy
// records the emission but hands back an empty Plan, and a test that
// checks "did it ask with the SELECTED profile" would pass on a panel
// that always asked with the first one.
Q_DECLARE_METATYPE(Longpath::BroadcastOptimizer::Plan)
