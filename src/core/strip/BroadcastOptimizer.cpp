// =================================================================
// src/core/strip/BroadcastOptimizer.cpp  (Longpath)
// =================================================================
//
// Longpath-original. See BroadcastOptimizer.h for why the transmit
// bandwidth changes the answer, and why every move here is a cut.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/BroadcastOptimizer.h"

#include "core/strip/ClientEq.h"
#include "core/strip/EqBandLayout.h"
#include "core/strip/ClientGate.h"
#include "core/strip/StripChain.h"
#include "core/strip/StripSettings.h"
#include "core/strip/StripTuner.h"

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
    double widthHz;
    double pitchFactor;     // high-pass as a FRACTION of the fundamental
    double highPassCapHz;   // and never above this, whatever the pitch
    // How far this width MOVES the voice from where it already is,
    // in dB. Not a target to drag it onto — see the note below.
    double lowShiftDb;      // around 125-250 Hz, the body of the voice
    double presenceShiftDb; // around 2 kHz
    double topShiftDb;      // at three quarters of the channel
    int    density;         // where this width sits on Zeus's 0..100 axis
    bool   character;       // may this width run tube / exciter / reverb?
};

// ── Shaping is a SHIFT, not a target ─────────────────────────────────
//
// Two wrong turns were taken here before this, and the second was
// subtler than the first.
//
// The first: one curve for all four widths — VoiceAnalyzer's, which
// asks for 125 Hz at -12 dB relative to 1 kHz. That is a contest
// curve and it is the right answer for exactly one of these four.
//
// The second, and the one that actually gutted the sound: expressing
// the answer as an absolute target at all. A voice's long-term
// spectrum FALLS with frequency — the 250 Hz band of any natural
// voice sits well above its 1 kHz band, and that is not a defect, it
// is what a person sounds like. Aiming at "250 Hz shall be 1.5 dB
// under 1 kHz" therefore does not correct anything; it flattens the
// voice's own tilt, and the measured cost on a synthetic voice was
// 8.3 dB of body on the WIDE profile, which is supposed to be the
// gentle one.
//
// Bench, 2026-10-09: "Das Original-Audio ist gut, nur saemtliche
// Bearbeitungen sind eine Katastrophe", "hier ist kein Bass, hier
// fehlt es einfach an allem."
//
// So the table below says how far each width MOVES the voice from
// where it already is. 4.0 kHz moves the bottom by nothing at all:
// the operator's own balance is the starting point, and the widest
// channel has no reason to argue with it. The narrow ones lean it
// forward, by an amount an ear would call a change rather than a
// different voice.
//
// A genuinely boomy microphone still gets corrected, but under a
// bound — see the shaping block in plan(). Unbounded correction is
// what this comment exists to prevent happening a third time.
constexpr WidthRule kRules[kWidthCount] = {
    // 2.3 kHz — a crowded receiver's filter. This is the one the old
    // contest curve was actually written for: body costs compressor
    // headroom the channel cannot repay, presence is what gets you
    // answered. It will not sound like the operator's living room,
    // and that is the trade being made on purpose.
    {2300.0, 0.85, 180.0, -4.0, +3.0, -4.0, kDensityDense,     false},

    // 2.7 kHz — what most of the band is listening on, so this is the
    // one that should sound right by default rather than clever.
    {2700.0, 0.85, 150.0, -2.0, +2.0, -3.0, kDensityBroadcast, true},

    // 3.2 kHz — room for the voice's own body again. Nearly flat at
    // the bottom; the reason to be this wide is fidelity, and cutting
    // the bass would spend the extra kilohertz on nothing.
    {3200.0, 0.85, 120.0, -1.0, +1.5, -1.5, 42,                true},

    // 4.0 kHz — lightest hand of the four. Flat bottom, a touch of
    // presence, nothing else. Only pays off when the other operator
    // has opened their filter to match.
    {4000.0, 0.85, 100.0,  0.0, +1.0,  0.0, kDensityNatural,   true},
};

// Below this much distance between the voice and the room, nothing is
// lifted — anywhere. Boosting a band is boosting whatever noise lives
// in it, and that is the whole reason the no-boost rule existed.
// Twenty-five decibels is the same separation StripTuner already
// treats as "a gate will help; a quieter room will help more".
constexpr double kBoostNeedsSnrDb = 25.0;

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

QString densityName(int density)
{
    struct Stop { int value; const char* name; };
    static constexpr Stop kStops[] = {
        {kDensityNatural,   "Natural"},
        {kDensityBroadcast, "Broadcast"},
        {kDensityDense,     "Dense"},
        {kDensityMaximum,   "Maximum"},
    };
    const Stop* best = &kStops[0];
    int bestGap = std::abs(kStops[0].value - density);
    for (const Stop& s : kStops) {
        const int gap = std::abs(s.value - density);
        if (gap < bestGap) {
            bestGap = gap;
            best    = &s;
        }
    }
    return QString::fromLatin1(best->name);
}

double targetCrestDb(int density)
{
    // 16 dB at the natural end, 6 dB at the hard end, straight line
    // between. The ends are the argument: a conversational voice
    // measures around 16 dB peak-to-average, and below about 6 dB
    // speech stops sounding like a person breathing between words.
    constexpr double kNaturalCrestDb = 16.0;
    constexpr double kHardestCrestDb = 6.0;
    const double d = std::clamp(double(density), 0.0, 100.0) / 100.0;
    return kNaturalCrestDb - d * (kNaturalCrestDb - kHardestCrestDb);
}

// ── The plans ────────────────────────────────────────────────────────

QVector<Plan> plan(const VoiceAnalysis& a)
{
    QVector<Plan> out;

    // The same two refusals StripTuner makes, for the same reasons: an
    // invalid measurement has nothing to plan from, and a clipped one
    // would have every stage confidently wrong at once because the
    // spectrum being read is the spectrum of its own flat tops.
    if (!a.valid || a.clippedSamples > 0) {
        return out;
    }

    out.reserve(kWidthCount);
    for (const WidthRule& rule : kRules) {
        Plan p;
        p.widthHz = rule.widthHz;
        p.name    = presetName(rule.widthHz);
        p.density = rule.density;

        // ── High-pass, kept BELOW the fundamental ────────────────────
        //
        // The first version multiplied the fundamental by 1.15 to
        // 1.80 and took the HIGHER of that and the rumble-driven
        // value. On a 110 Hz voice that put a 24 dB/octave filter at
        // 198 Hz — above the fundamental, which is to say through the
        // middle of the voice. Together with a shelf cut just above
        // it, that is where the bass went.
        //
        // Now it is a fraction, and the LOWER of the two wins. The
        // fundamental is the voice's own bottom note; a transmit
        // filter will deal with what is under it, and a high-pass
        // that eats it cannot be made up for anywhere downstream.
        const double measured = StripTuner::highPassHz(a);
        const double belowPitch =
            a.pitchHz > 0.0 ? a.pitchHz * rule.pitchFactor : measured;
        p.highPassHz = std::clamp(std::min(measured, belowPitch),
                                  60.0, rule.highPassCapHz);

        if (a.pitchHz > 0.0) {
            p.notes << QStringLiteral(
                "High-pass at %1 Hz — below your own fundamental of "
                "%2 Hz, deliberately. What is under it is rumble and "
                "proximity bass that reaches the compressor before the "
                "transmit filter throws it away; what is above it is "
                "your voice, and no later stage can put that back.")
                .arg(p.highPassHz, 0, 'f', 0)
                .arg(a.pitchHz, 0, 'f', 0);
        } else {
            p.notes << QStringLiteral(
                "High-pass at %1 Hz, set from the low-end energy — no "
                "stable pitch was found in the take, so the fundamental "
                "could not be used to place it.").arg(p.highPassHz, 0, 'f', 0);
        }

        // ── The transmit channel this profile is for ─────────────────
        p.lowCutHz  = int(std::lround(p.highPassHz));
        p.highCutHz = int(std::lround(rule.widthHz));

        // ── Hum ──────────────────────────────────────────────────────
        p.humNotches = StripTuner::humWorthNotching(a);
        p.humBaseHz  = a.humBaseHz;
        if (p.humNotches) {
            p.notes << QStringLiteral(
                "Notches at %1, %2 and %3 Hz. The hum is only %4 dB "
                "under your voice — these hide it, they do not fix it, "
                "and finding what is picking up the mains is worth more "
                "than any setting here.")
                .arg(a.humBaseHz).arg(a.humBaseHz * 2).arg(a.humBaseHz * 3)
                .arg(-a.humDb, 0, 'f', 1);
        }

        // ── Tone ─────────────────────────────────────────────────────
        //
        // Measured against THIS width's target, not against one
        // curve for all four, and the difference may go either way.
        //
        // The measurement is a.bandDb — the voice's own spectrum in
        // dB relative to its 1 kHz band. The target is what this
        // channel wants there. Subtract, and the shelf gain is what
        // closes the gap: negative where the voice has more than the
        // channel can use, POSITIVE where it has less.
        //
        // That positive half is new and it is the whole repair. Under
        // the old cuts-only rule a profile could only ever take away,
        // so the bottom of every voice went with it.
        {
            const double measuredLow  =
                0.5 * (a.bandDb[2] + a.bandDb[3]);   // 125 + 250 Hz
            const double measuredTop  = a.bandDb[7]; // 4 kHz

            // The one guard that made cuts-only right in the first
            // place: lifting a band lifts whatever noise is in it.
            // With the room close to the voice there is nothing there
            // worth lifting.
            const double snr = a.speechDbFs - a.noiseFloorDbFs;
            const bool mayLift = snr > kBoostNeedsSnrDb;

            // ── The bounded correction ──────────────────────────────
            //
            // A voice that is genuinely boomy, or genuinely thin,
            // still gets helped — but by at most three decibels, and
            // measured against what a voice normally does rather than
            // against a flat line. Around +9 dB at 125-250 Hz
            // relative to 1 kHz is ordinary for speech; the
            // correction only answers the distance from THAT.
            //
            // The bound is the point. Unbounded, this term is exactly
            // the absolute target that flattened the voice, wearing a
            // different name.
            constexpr double kOrdinaryLowTiltDb = 9.0;
            constexpr double kMaxCorrectionDb   = 3.0;
            const double lowCorrection = std::clamp(
                -(measuredLow - kOrdinaryLowTiltDb) * 0.5,
                -kMaxCorrectionDb, kMaxCorrectionDb);

            const double lift = mayLift ? 6.0 : 0.0;

            p.lowShelfDb  = std::clamp(rule.lowShiftDb + lowCorrection,
                                       -6.0, std::min(lift, 4.0));
            p.presenceDb  = std::clamp(rule.presenceShiftDb,
                                       -6.0, std::min(lift, 4.0));
            // The top shift is a roll-off toward the channel edge and
            // is applied as given; a microphone that is already dull
            // up there is told about it in `caveat` rather than being
            // dulled further by a correction term.
            p.highShelfDb = std::clamp(
                rule.topShiftDb + (measuredTop > 0.0 ? -1.0 : 0.0),
                -8.0, std::min(lift, 2.0));

            // Three quarters of the channel: the roll-off begins
            // inside it, which is what makes a 2.3 k profile sound
            // narrow rather than merely quieter at 4 kHz. Above the
            // edge the transmit filter would do the work anyway, and
            // a shelf placed there shapes only what is thrown away.
            p.highShelfHz = rule.widthHz * 0.75;

            // Clear of the high-pass by an octave and a half. The two
            // used to sit on top of each other — a 24 dB/octave
            // high-pass at 198 Hz with an 8 dB shelf cut at 297 Hz on
            // top of it, both working on the same part of the voice.
            // That stacking is half of why there was no bass left.
            p.lowShelfHz  = std::clamp(p.highPassHz * 2.5, 150.0, 500.0);

            p.notes << QStringLiteral(
                "Tone for %1: low shelf %2 dB from %5 Hz, presence "
                "%3 dB, high shelf %4 dB from %6 Hz. These MOVE your "
                "voice from where it already is rather than dragging "
                "it onto a curve — your own balance is the starting "
                "point, and they lift as readily as they cut%7.")
                .arg(widthLabel(rule.widthHz))
                .arg(p.lowShelfDb,  0, 'f', 1)
                .arg(p.presenceDb,  0, 'f', 1)
                .arg(p.highShelfDb, 0, 'f', 1)
                .arg(p.lowShelfHz,  0, 'f', 0)
                .arg(p.highShelfHz, 0, 'f', 0)
                .arg(mayLift
                     ? QString()
                     : QStringLiteral(" — except that nothing is lifted "
                                      "here: your voice is only %1 dB "
                                      "above the room, and lifting a "
                                      "band lifts the room with it")
                           .arg(snr, 0, 'f', 0));
        }

        // ── Gate ─────────────────────────────────────────────────────
        //
        // Only when the room is actually audible. A gate costs the
        // ends of quiet words and buys nothing in a quiet room, and
        // switching one on by default is the sort of thing that makes
        // a profile sound worse than no profile at all.
        {
            const double sep = a.speechDbFs - a.noiseFloorDbFs;
            constexpr double kRoomIsQuietDb = 30.0;
            if (sep < StripTuner::kMinGateSeparationDb) {
                p.gateOn = false;
                p.notes << QStringLiteral(
                    "No gate. Your voice is only %1 dB above the room, "
                    "and a gate set that close chews the ends off quiet "
                    "words. A quieter room is the fix.")
                    .arg(sep, 0, 'f', 0);
            } else if (sep >= kRoomIsQuietDb) {
                p.gateOn = false;
                p.notes << QStringLiteral(
                    "No gate — your room is %1 dB below your voice, "
                    "which is quiet enough that a gate would only risk "
                    "the ends of words.").arg(sep, 0, 'f', 0);
            } else {
                p.gateOn = true;
                p.gateThresholdDbFs =
                    std::clamp(StripTuner::gateThresholdDbFs(a), -80.0, 0.0);
            }
        }

        p.limiterCeilingDb = -1.0;

        // ── Compressor, derived from the density target ──────────────
        //
        // The ratio is not chosen, it is solved for. With the
        // threshold at the speech average, the peaks sit the measured
        // crest factor above it and come out crest/ratio above it. So
        // the ratio that lands on the target crest is measured
        // divided by target, and nothing here is a taste.
        {
            const double target = targetCrestDb(p.density);
            const double measuredCrest = std::max(a.crestFactorDb, 1.0);
            p.compRatio = std::clamp(measuredCrest / std::max(target, 1.0),
                                     1.0, 6.0);
            p.compThresholdDbFs = std::clamp(a.speechDbFs, -60.0, -6.0);

            // What the compression took off the peaks is what may be
            // put back, plus the make-up the EQ cuts earned. Capped
            // so the limiter is a safety net and not a stage.
            const double fromComp =
                measuredCrest - measuredCrest / std::max(p.compRatio, 1.0);
            // ── Make-up, bounded by the headroom there actually is ──
            //
            // The old line added the analyser's suggested preamp on
            // top of what the compressor took and clamped at 18 dB —
            // a number with no relation to this take. With the
            // measured peak known, the honest bound is arithmetic:
            // a peak P entering a compressor with threshold T and
            // ratio R leaves at T + (P - T) / R, and whatever is
            // added after that has to still fit under the limiter.
            //
            // Leaving a decibel of margin means the limiter catches
            // transients, which is its job, instead of riding the
            // whole take, which is what makes a voice sound crushed.
            const double peak = a.samplePeakDbFs > -120.0
                ? a.samplePeakDbFs
                : a.speechDbFs + measuredCrest;
            const double afterComp = p.compThresholdDbFs
                + (peak - p.compThresholdDbFs) / std::max(p.compRatio, 1.0);
            const double headroom =
                (p.limiterCeilingDb - 1.0) - afterComp;

            p.compMakeupDb   = std::clamp(std::min(fromComp, headroom),
                                          0.0, 12.0);
            p.densityGainDb  = p.compMakeupDb;

            p.notes << QStringLiteral(
                "Compressor %1:1 at %2 dBFS, make-up %3 dB. Density "
                "%4 (%5) aims your peaks at %6 dB over the average; "
                "they measure %7 dB now. Expect roughly %3 dB more "
                "average level — an estimate, not a measurement. The "
                "next recording is the measurement.")
                .arg(p.compRatio, 0, 'f', 1)
                .arg(p.compThresholdDbFs, 0, 'f', 0)
                .arg(p.compMakeupDb, 0, 'f', 1)
                .arg(p.density)
                .arg(densityName(p.density))
                .arg(target, 0, 'f', 0)
                .arg(measuredCrest, 0, 'f', 0);
        }

        // ── What this width forgets ──────────────────────────────────
        p.characterStages = rule.character;
        if (!p.characterStages) {
            p.forgotten = QStringLiteral(
                "Valve, exciter and reverb are switched OFF, not left "
                "as they were. At %1 there is no room for colour: it "
                "costs compressor headroom and the far end hears none "
                "of it. They come back from the 2.7k profile upward.")
                .arg(widthLabel(rule.widthHz));
            p.notes << p.forgotten;
        }

        // ── What the measurement says this width cannot deliver ──────
        if (a.rolloffHz > 0.0 && a.rolloffHz < rule.widthHz * 0.95) {
            p.caveat = QStringLiteral(
                "Your microphone is already 20 dB down by %1 Hz, below "
                "this channel's %2 Hz. The top of it will carry hiss "
                "rather than voice — a narrower profile loses you "
                "nothing here and costs the other operator less band.")
                .arg(a.rolloffHz, 0, 'f', 0)
                .arg(rule.widthHz, 0, 'f', 0);
        }

        // ── The thing a compressor cannot fix ────────────────────────
        //
        // Loudness range, not crest. A wide LRA is the operator
        // moving between sentences, and a compressor set from the
        // crest factor will chase it and pump instead of fixing it.
        // Tech 3342 calls a range above about 10 LU wide for speech.
        if (a.loudnessRangeValid && a.loudnessRangeLu > 10.0) {
            p.notes << QStringLiteral(
                "Your loudness range is %1 LU — that is you moving "
                "between sentences, not your peaks, and no compressor "
                "setting here answers it. Keep a steady distance from "
                "the microphone, or let the leveller take it.")
                .arg(a.loudnessRangeLu, 0, 'f', 1);
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

// ── Putting one into a chain ─────────────────────────────────────────

bool apply(const Plan& p, StripChain& chain)
{
    if (p.name.isEmpty()) { return false; }

    // ── Seed the band layout FIRST ───────────────────────────────────
    //
    // A freshly constructed ClientEq has activeBandCount 0, and
    // ClientEq::process returns immediately on that — so every band
    // written below would be stored, read back correctly by any test
    // that asks for it, and do absolutely nothing to audio.
    //
    // That is exactly what happened: the four profiles were rendered
    // with no equaliser at all, which left them differing only by
    // compressor and limiter — and the loudness-matched audition
    // flattens most of that. Reported from the bench in five words,
    // 2026-10-09: "alle profile hören sich gleich an". They did.
    //
    // ensureSeeded only ever appends; a chain the operator has
    // already shaped keeps every band it had.
    EqBandLayout::ensureSeeded(chain.eq());

    // High-pass.
    {
        ClientEq::BandParams b = chain.eq().band(kBandHighPass);
        b.type          = ClientEq::FilterType::HighPass;
        b.freqHz        = float(p.highPassHz);
        b.slopeDbPerOct = 24;
        b.enabled       = true;
        chain.eq().setBand(kBandHighPass, b);
    }

    // Hum notches — written when they are wanted and switched off
    // when they are not, so applying a second plan cannot leave the
    // first one's notches behind.
    for (int h = 0; h < 3; ++h) {
        ClientEq::BandParams b = chain.eq().band(kBandHum1 + h);
        if (p.humNotches && p.humBaseHz > 0) {
            b.type    = ClientEq::FilterType::Peak;
            b.freqHz  = float(p.humBaseHz * (h + 1));
            b.q       = 8.0f;
            b.gainDb  = float(-18.0 + 3.0 * h);
            b.enabled = true;
        } else {
            b.enabled = false;
        }
        chain.eq().setBand(kBandHum1 + h, b);
    }

    // The two shelves carry their corner AND their type, because the
    // default layout has a peak at 800 Hz in slot 4 and a peak at
    // 3 kHz in slot 6 — writing only a gain into those shapes the
    // wrong part of the voice and barely separates the widths.
    {
        ClientEq::BandParams lo = chain.eq().band(kBandLow);
        lo.type    = ClientEq::FilterType::LowShelf;
        lo.freqHz  = float(p.lowShelfHz);
        lo.gainDb  = float(p.lowShelfDb);
        lo.enabled = true;
        chain.eq().setBand(kBandLow, lo);

        ClientEq::BandParams hi = chain.eq().band(kBandHigh);
        hi.type    = ClientEq::FilterType::HighShelf;
        hi.freqHz  = float(p.highShelfHz);
        hi.gainDb  = float(p.highShelfDb);
        hi.enabled = true;
        chain.eq().setBand(kBandHigh, hi);
    }
    setBandGain(chain, kBandPresence, p.presenceDb,  true);
    chain.setStageEnabled(StripChain::Stage::Eq, true);

    if (p.gateOn) {
        chain.gate().setThresholdDb(float(p.gateThresholdDbFs));
        chain.gate().setMode(ClientGate::Mode::Expander);
        chain.gate().setFloorDb(-15.0f);
        chain.gate().setHoldMs(25.0f);
        chain.gate().setReleaseMs(180.0f);
    }
    chain.setStageEnabled(StripChain::Stage::Gate, p.gateOn);

    if (p.deEssOn) {
        chain.deEss().setFrequencyHz(float(p.deEssHz));
        chain.deEss().setThresholdDb(float(std::clamp(
            p.compThresholdDbFs + 6.0, -60.0, 0.0)));
        chain.deEss().setAmountDb(float(p.deEssAmountDb));
    }
    chain.setStageEnabled(StripChain::Stage::DeEss, p.deEssOn);

    chain.comp().setRatio(float(p.compRatio));
    chain.comp().setThresholdDb(float(p.compThresholdDbFs));
    chain.comp().setAttackMs(8.0f);
    chain.comp().setReleaseMs(150.0f);
    chain.comp().setKneeDb(8.0f);
    chain.comp().setMakeupDb(float(p.compMakeupDb));
    chain.setStageEnabled(StripChain::Stage::Comp, true);

    chain.limiter().setCeilingDb(float(p.limiterCeilingDb));
    chain.setStageEnabled(StripChain::Stage::Limiter, true);

    // ── The character stages ─────────────────────────────────────────
    //
    // Switched OFF when the profile forgets them, and otherwise left
    // exactly as the operator had them. Never switched ON: there is
    // no measurement that says a voice wants a valve, and an
    // optimiser that turns one on is making a decision it cannot
    // justify from the recording.
    //
    // Off, rather than left alone, is the asymmetry and it is
    // deliberate. A profile that claims to be lean while a reverb
    // from the previous profile is still running is a profile the
    // operator cannot trust, and the one thing an A/B between two
    // profiles must mean is that the difference heard is the
    // difference between them.
    if (!p.characterStages) {
        chain.setStageEnabled(StripChain::Stage::Tube,   false);
        chain.setStageEnabled(StripChain::Stage::Pudu,   false);
        chain.setStageEnabled(StripChain::Stage::Reverb, false);
    }

    // Deliberately absent: the master switch. The operator decides
    // when their transmit audio changes.
    return true;
}

int saveAsPresets(const QVector<Plan>& plans)
{
    int written = 0;
    for (const Plan& p : plans) {
        // A scratch chain per plan, so nothing the operator is
        // listening to is touched and no plan can inherit a stage
        // from the one before it.
        StripChain scratch;
        scratch.prepare(48000.0);
        if (!apply(p, scratch)) { continue; }
        if (StripSettings::saveUserPreset(p.name, scratch)) { ++written; }
    }
    return written;
}

} // namespace Longpath::BroadcastOptimizer
