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
    double pitchFactor;     // high-pass as a multiple of the fundamental
    double highPassCapHz;   // and never above this, whatever the pitch
    double lowExtraDb;      // extra low-shelf cut, on top of the measured one
    double highExtraDb;     // extra high-shelf cut, likewise
    int    density;         // where this width sits on Zeus's 0..100 axis
    bool   character;       // may this width run tube / exciter / reverb?
};

// Where the character stages start being allowed. Below this the
// channel is too narrow for them to be anything but cost — see
// Plan::characterStages for the operator's instruction and the
// published practice behind it.
constexpr double kCharacterFromHz = 2700.0;

constexpr WidthRule kRules[kWidthCount] = {
    // 2.3 kHz — a crowded receiver's filter. The fundamental is pure
    // cost here: it carries no intelligibility the harmonics do not,
    // and it is the loudest thing the compressor has to swallow. Cut
    // high, lean the voice forward, and accept that it will not sound
    // like the operator's living room.
    {2300.0, 1.80, 300.0, -3.0, -9.0, kDensityDense,     false},

    // 2.7 kHz — what most of the band is listening on, so this is the
    // one that should sound right by default rather than clever.
    {2700.0, 1.60, 250.0, -2.0, -7.0, kDensityBroadcast, true},

    // 3.5 kHz — room for the voice's own body again. Less density,
    // because the reason to be this wide is fidelity and compressing
    // it hard would spend the extra kilohertz on nothing.
    {3500.0, 1.30, 180.0, -0.5, -3.0, 42,                true},

    // 4.0 kHz — only pays off when the other operator has opened their
    // filter to match. Lightest hand of the four.
    {4000.0, 1.15, 150.0,  0.0, -1.5, kDensityNatural,   true},
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

        // ── High-pass, against the operator's own pitch ──────────────
        //
        // max() of the two: the measurement-driven value already
        // knows about rumble and proximity bass, and the pitch-driven
        // one knows where this particular voice begins. Taking the
        // higher of the two means neither can be the reason the
        // filter lands somewhere wrong — then the width's own cap
        // pulls it back, which is what makes the wide profiles keep
        // the bottom of the voice.
        const double measured = StripTuner::highPassHz(a);
        const double fromPitch =
            a.pitchHz > 0.0 ? a.pitchHz * rule.pitchFactor : measured;
        p.highPassHz = std::clamp(std::max(measured, fromPitch),
                                  60.0, rule.highPassCapHz);

        if (a.pitchHz > 0.0) {
            p.notes << QStringLiteral(
                "High-pass at %1 Hz — %2 times your own fundamental of "
                "%3 Hz. Below that is rumble and proximity bass: it "
                "reaches the compressor before the transmit filter "
                "throws it away, so it costs headroom and buys nothing.")
                .arg(p.highPassHz, 0, 'f', 0)
                .arg(rule.pitchFactor, 0, 'f', 2)
                .arg(a.pitchHz, 0, 'f', 0);
        } else {
            p.notes << QStringLiteral(
                "High-pass at %1 Hz, set from the low-end energy — no "
                "stable pitch was found in the take, so the fundamental "
                "could not be used.").arg(p.highPassHz, 0, 'f', 0);
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
        // The measured ten-band suggestion folded onto the three
        // controls the panel has — pairs averaged, exactly as
        // StripTuner does it, so a hand-tuned chain and an optimised
        // one are shaped by the same arithmetic — and then the
        // width's own extra cut on top.
        //
        // Presence is NOT lifted for the narrow profiles. It does not
        // need to be: cutting the low shelf and the high shelf around
        // it is what "forward" means, and it costs no gain on a band
        // the microphone may be noisy in.
        {
            const double low  = 0.5 * (a.suggestedEqDb[2] + a.suggestedEqDb[3]);
            const double pres = a.suggestedEqDb[6];
            const double high = 0.5 * (a.suggestedEqDb[7] + a.suggestedEqDb[8]);

            p.lowShelfDb  = std::clamp(std::min(0.0, low) + rule.lowExtraDb,
                                       -18.0, 0.0);
            p.presenceDb  = std::min(0.0, pres);
            p.highShelfDb = std::clamp(std::min(0.0, high) + rule.highExtraDb,
                                       -18.0, 0.0);

            p.notes << QStringLiteral(
                "Tone for %1: low %2 dB, presence %3 dB, high %4 dB. All "
                "cuts — the level comes back as make-up gain, because "
                "reaching a shape by boosting raises whatever the "
                "microphone does worst in that band along with the voice.")
                .arg(widthLabel(rule.widthHz))
                .arg(p.lowShelfDb,  0, 'f', 1)
                .arg(p.presenceDb,  0, 'f', 1)
                .arg(p.highShelfDb, 0, 'f', 1);
        }

        // ── Gate ─────────────────────────────────────────────────────
        {
            const double sep = a.speechDbFs - a.noiseFloorDbFs;
            if (sep >= StripTuner::kMinGateSeparationDb) {
                p.gateOn = true;
                p.gateThresholdDbFs =
                    std::clamp(StripTuner::gateThresholdDbFs(a), -80.0, 0.0);
            } else {
                p.gateOn = false;
                p.notes << QStringLiteral(
                    "No gate. Your voice is only %1 dB above the room, "
                    "and a gate set that close chews the ends off quiet "
                    "words. A quieter room is the fix.")
                    .arg(sep, 0, 'f', 0);
            }
        }

        // ── De-esser ─────────────────────────────────────────────────
        //
        // On for every width when there is sibilance to take, narrow
        // channels included — and that is worth saying, because the
        // obvious objection is that a 2.3 kHz filter removes 6 kHz
        // anyway. It does, but the strip runs BEFORE the transmit
        // filter: the sibilance reaches this compressor first and
        // takes gain reduction with it. Removing it here is how the
        // narrow profiles get their headroom back.
        if (a.sibilanceDb > -8.0) {
            // Wide channels get the heavier hand because there the
            // sibilance is also audible at the far end, not merely
            // expensive.
            const double audible = rule.widthHz >= 3500.0 ? 1.0 : 0.8;
            p.deEssOn       = true;
            p.deEssHz       = 6000.0;
            p.deEssAmountDb = -std::clamp(a.sibilanceDb, 3.0, 12.0) * audible;
            p.notes << QStringLiteral(
                "De-esser at 6 kHz, %1 dB. Your sibilance measures %2 dB "
                "against the midrange. The strip runs before the "
                "transmit filter, so this energy costs compressor "
                "headroom even at %3.")
                .arg(p.deEssAmountDb, 0, 'f', 1)
                .arg(a.sibilanceDb, 0, 'f', 1)
                .arg(widthLabel(rule.widthHz));
        }

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
            const double fromEq = std::clamp(a.suggestedPreampDb, 0.0, 12.0);
            p.compMakeupDb   = std::clamp(fromComp + fromEq, 0.0, 18.0);
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

        p.limiterCeilingDb = -1.0;

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

    setBandGain(chain, kBandLow,      p.lowShelfDb,  true);
    setBandGain(chain, kBandPresence, p.presenceDb,  true);
    setBandGain(chain, kBandHigh,     p.highShelfDb, true);
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
