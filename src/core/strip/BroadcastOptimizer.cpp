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
#include "core/strip/StripCharacters.h"
#include "core/strip/StripSettings.h"
#include "core/strip/StripTuner.h"

#include <algorithm>
#include <cmath>

namespace Longpath::BroadcastOptimizer {

static_assert(Plan::kStageCount == StripChain::kStageCount,
              "Plan::stageNotes ist nach StripChain::Stage indiziert; "
              "eine neue Stufe muss hier mitgezaehlt werden");

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
    int    lowCutHz;        // der Kanal, unten
    int    highCutHz;       // und oben
    const char* compChar;
    const char* tubeChar;   // "" = Roehre bleibt aus
    const char* puduChar;   // "" = Exciter bleibt aus
    const char* limiterChar;
    const char* headline;   // what this profile is, in one line
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
    {2300.0, 0.85, 180.0, -4.0, +3.0, -4.0, kDensityDense,     false, 200, 2500,
     "Voodoo",   "",          "",         "Loud",
     "Lean and dense for a crowded band. Cuts through; will not sound like your living room."},

    // 2.7 kHz — what most of the band is listening on, so this is the
    // one that should sound right by default rather than clever.
    {2700.0, 0.85, 150.0, -2.0, +2.0, -3.0, kDensityBroadcast, true,  150, 2700,
     "Contest",  "Broadcast", "Presence", "Broadcast",
     "The everyday setting — what most of the band is listening on."},

    // 3.2 kHz — room for the voice's own body again. Nearly flat at
    // the bottom; the reason to be this wide is fidelity, and cutting
    // the bass would spend the extra kilohertz on nothing.
    {3200.0, 0.85, 120.0, -1.0, +1.5, -1.5, 42,                true,  100, 3200,
     "Balanced", "Warm",      "Both",     "Broadcast",
     "Room for the body of your voice. Polite when the band is quiet."},

    // 4.0 kHz — lightest hand of the four. Flat bottom, a touch of
    // presence, nothing else. Only pays off when the other operator
    // has opened their filter to match.
    {4000.0, 0.85, 100.0,  0.0, +1.0,  0.0, kDensityNatural,   true,   50, 4050,
     "Gentle",   "Warm",      "Warmth",   "Transparent",
     "Lightest hand of the four. Closest to how you actually sound."},
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
        // ── Die Notizen stehen in der Reihenfolge der Kette ─────────
        //
        // Betreiber, 2026-10-09: "nach der reihen. gate, dann eq,
        // dann de-esser, dann kompressor....." Gerechnet wird in
        // einer anderen Reihenfolge -- der Trimm muss vor dem
        // Kompressor feststehen, der Kanal vor dem Klang -- also
        // werden die Zeilen eingesammelt und am Ende in der
        // Reihenfolge abgelegt, in der die Stufen im Kanalzug
        // stehen. Was man liest, ist dann der Weg, den das Signal
        // nimmt.
        QString nGate, nEqCurve, nEq, nDeEss, nComp, nLimiter, nColour, nRange;

        Plan p;
        p.widthHz = rule.widthHz;
        p.name    = presetName(rule.widthHz);
        p.density  = rule.density;
        p.headline = QString::fromUtf8(rule.headline);
        p.compCharacter    = QString::fromUtf8(rule.compChar);
        p.tubeCharacter    = QString::fromUtf8(rule.tubeChar);
        p.puduCharacter    = QString::fromUtf8(rule.puduChar);
        p.limiterCharacter = QString::fromUtf8(rule.limiterChar);

        // ── The channel this profile is for ─────────────────────────
        //
        // The width IS the characteristic move, exactly as Zeus's
        // BROADCAST style states it: "wide to 2.9 kHz". The low cut
        // is the transmit filter's, not an equaliser's — it belongs
        // to the channel, and the chain below never touches tone.
        // ── Der Kanal, unten UND oben ───────────────────────────────
        //
        // Der Tiefschnitt lag vorher bei allen vier fest auf 150 Hz.
        // Damit verlor das 4-k-Profil unten genau dasselbe wie das
        // schmale, und der Betreiber hoerte, was davon zu erwarten
        // war: "das 4k profil hoert sich an als waeren es 2,7"
        // (2026-10-09).
        //
        // Die Zahlen hier sind seine, aus derselben Nachricht: "ein
        // dx oder contest profil ist im bereich 200-2500(2700), ein
        // 4 k profil ist von ca 50-4050." Dazwischen gestuft. Unten
        // liegt der groessere Teil des Unterschieds -- 50 gegen 200
        // Hz sind zwei Oktaven Koerper.
        p.lowCutHz  = rule.lowCutHz;
        p.highCutHz = rule.highCutHz;

        // ── Expander, from the measured signal-to-noise ─────────────
        //
        // Zeus runs one in every style, gentlest included, and names
        // the threshold with the SNR it came from: "Expander -38 dB
        // (SNR 28 dB)". On its own numbers the threshold sat about
        // two thirds of the way up from the noise floor to the
        // voice, which is where an expander leans on the room
        // without reaching for the ends of words.
        {
            const double snr = a.speechDbFs - a.noiseFloorDbFs;
            if (snr >= StripTuner::kMinGateSeparationDb) {
                p.expanderOn = true;
                p.expanderThresholdDbFs =
                    std::clamp(a.noiseFloorDbFs + 0.65 * snr, -80.0, -10.0);
                p.expanderFloorDb = -std::clamp(snr * 0.5, 8.0, 20.0);
                nGate = QStringLiteral(
                    "Expander at %1 dBFS with a %2 dB floor, both from "
                    "your measured %3 dB of signal-to-noise. The "
                    "pauses go quiet. What it cannot do is remove the "
                    "room WHILE you speak — the reverb tail sits under "
                    "your voice then, and no level-driven stage can "
                    "tell the two apart.")
                    .arg(p.expanderThresholdDbFs, 0, 'f', 0)
                    .arg(p.expanderFloorDb, 0, 'f', 0)
                    .arg(snr, 0, 'f', 0);
            } else {
                nGate = QStringLiteral(
                    "No expander. Your voice is only %1 dB above the "
                    "room, and anything set that close takes the ends "
                    "off quiet words. A quieter room is the fix.")
                    .arg(snr, 0, 'f', 0);
            }
        }

        // ── The trim, onto one target ───────────────────────────────
        p.trimDb = std::clamp(kTargetLevelDbFs - a.speechDbFs, -12.0, 12.0);
        const QString nTrim = QStringLiteral(
            "Trim %1%2 dB to %3 dBFS. Pure arithmetic: your take "
            "measured %4 dBFS and every profile starts from the same "
            "level, so what you hear between them is the processing "
            "and not the volume.")
            .arg(p.trimDb >= 0.0 ? QStringLiteral("+") : QString())
            .arg(p.trimDb, 0, 'f', 1)
            .arg(kTargetLevelDbFs, 0, 'f', 0)
            .arg(a.speechDbFs, 0, 'f', 1);

        // ── De-esser, at the frequency that was measured ────────────
        //
        // Zeus shows the sibilance frequency it found ("Sib 4.0k")
        // and puts the de-esser there. The previous version here was
        // wired to 6 kHz, which is somebody else's voice.
        constexpr double kSibilantAboveDb = -3.0;
        if (a.sibilanceDb > kSibilantAboveDb) {
            const double over = a.sibilanceDb - kSibilantAboveDb;
            p.deEssOn       = true;
            p.deEssHz       = std::clamp(a.sibilanceHz, 3500.0, 9000.0);
            p.deEssAmountDb = -std::clamp(over * 1.2, 2.0, 8.0);
            nDeEss = QStringLiteral(
                "De-esser at %1 Hz, %2 dB — that is where your "
                "sibilance actually sits, not a fixed frequency. It "
                "measures %3 dB against the midrange.")
                .arg(p.deEssHz, 0, 'f', 0)
                .arg(p.deEssAmountDb, 0, 'f', 1)
                .arg(a.sibilanceDb, 0, 'f', 1);
        }

        p.limiterCeilingDb = -1.0;

        // ── Compressor, derived from the density target ─────────────
        //
        // The ratio is solved for, not chosen: with the threshold at
        // the speech average the peaks sit the measured crest factor
        // above it and leave crest/ratio above it, so the ratio that
        // lands on the target crest is measured divided by target.
        {
            const double target = targetCrestDb(p.density);
            const double measuredCrest = std::max(a.crestFactorDb, 1.0);
            p.compRatio = std::clamp(measuredCrest / std::max(target, 1.0),
                                     1.0, 6.0);
            p.compThresholdDbFs = std::clamp(a.speechDbFs + p.trimDb,
                                             -60.0, -6.0);

            const double fromComp =
                measuredCrest - measuredCrest / std::max(p.compRatio, 1.0);
            const double peak = (a.samplePeakDbFs > -120.0
                                 ? a.samplePeakDbFs
                                 : a.speechDbFs + measuredCrest) + p.trimDb;
            const double afterComp = p.compThresholdDbFs
                + (peak - p.compThresholdDbFs) / std::max(p.compRatio, 1.0);
            const double headroom = (p.limiterCeilingDb - 1.0) - afterComp;

            p.compMakeupDb  = std::clamp(std::min(fromComp, headroom),
                                         0.0, 12.0);
            p.densityGainDb = p.trimDb + p.compMakeupDb;

            nComp = QStringLiteral(
                "Compressor %1:1 at %2 dBFS, make-up %3 dB. Density "
                "%4 (%5) aims your peaks at %6 dB over the average; "
                "they measure %7 dB now.")
                .arg(p.compRatio, 0, 'f', 1)
                .arg(p.compThresholdDbFs, 0, 'f', 0)
                .arg(p.compMakeupDb, 0, 'f', 1)
                .arg(p.density)
                .arg(densityName(p.density))
                .arg(target, 0, 'f', 0)
                .arg(measuredCrest, 0, 'f', 0);
        }

        nColour = p.tubeCharacter.isEmpty()
            ? QStringLiteral(
                "Valve, exciter and reverb stay off. At %1 there is no "
                "room for colour: it costs compressor headroom and the "
                "far end hears none of it.").arg(widthLabel(rule.widthHz))
            : QStringLiteral(
                "Valve \"%1\", exciter \"%2\". Named characters, not "
                "numbers invented here — each is a set of values that "
                "belong together, with a sentence saying what it is "
                "for. Reverb stays off; there is no measurement that "
                "asks for it.")
                .arg(p.tubeCharacter, p.puduCharacter);

        nLimiter = QStringLiteral(
            "Limiter \"%1\" at %2 dB. Phase rotator, %3 stages. Your voice's waveform is "
            "lopsided — speech always is — so its loudest peak "
            "reaches the limiter well before the average level has "
            "earned it. An all-pass cascade leaves every frequency at "
            "the same level and only moves phase, which pulls the two "
            "peaks together. That is headroom turned into loudness "
            "without a single tone control being touched.")
            .arg(p.limiterCharacter)
            .arg(p.limiterCeilingDb, 0, 'f', 0)
            .arg(p.phaseRotatorStages);

        // ── Die Korrekturkurve: Ueberhoehungen und Senken ───────────
        //
        // Das Gemessene an jeder der sieben Griff-Frequenzen, minus
        // der eigene geglaettete Verlauf an derselben Stelle. Was
        // uebrig bleibt, ist eine Resonanz oder ein Loch -- und genau
        // das wird gegengesteuert.
        {
            const double snrForLift = a.speechDbFs - a.noiseFloorDbFs;
            const bool mayLift = snrForLift > kBoostNeedsSnrDb;

            // Das Gemessene an einer beliebigen Frequenz, log-
            // interpoliert zwischen den zehn Baendern.
            const auto measuredAt = [&](double hz) {
                if (hz <= kVoiceBandHz[0]) { return a.bandDb[0]; }
                for (int i = 1; i < kVoiceBandCount; ++i) {
                    if (hz <= kVoiceBandHz[i]) {
                        const double t =
                            (std::log(hz) - std::log(kVoiceBandHz[i - 1]))
                          / (std::log(kVoiceBandHz[i])
                             - std::log(kVoiceBandHz[i - 1]));
                        return a.bandDb[i - 1]
                             + t * (a.bandDb[i] - a.bandDb[i - 1]);
                    }
                }
                return a.bandDb[kVoiceBandCount - 1];
            };

            // Der eigene Verlauf, ueber zwei Oktaven geglaettet: breit
            // genug, dass eine einzelne Resonanz ihn nicht mitzieht,
            // schmal genug, dass er der Neigung der Stimme folgt.
            const auto trendAt = [&](double hz) {
                double acc = 0.0;
                int    n   = 0;
                for (double f = hz / 4.0; f <= hz * 4.0; f *= 1.25) {
                    if (f < 60.0 || f > 8000.0) { continue; }
                    acc += measuredAt(f);
                    ++n;
                }
                return n > 0 ? acc / n : measuredAt(hz);
            };

            QStringList moved;
            for (int i = 0; i < Plan::kCorrectionBands; ++i) {
                const double hz = double(
                    EqBandLayout::spareFrequencyHz(i));
                const double deviation = measuredAt(hz) - trendAt(hz);

                // Gegengesteuert wird zur Haelfte. Voll ausgeglichen
                // klingt eine Stimme wie durch ein Rohr -- ihre
                // Unebenheiten SIND sie zu einem guten Teil.
                double g = std::clamp(-deviation * 0.5, -5.0, 5.0);
                if (!mayLift) { g = std::min(g, 0.0); }
                // Unter einem Dezibel ist es kein Fund, sondern
                // Messrauschen.
                if (std::abs(g) < 1.0) { g = 0.0; }
                p.correctionDb[i] = g;

                if (g != 0.0) {
                    moved << QStringLiteral("%1 Hz %2%3")
                        .arg(hz, 0, 'f', 0)
                        .arg(g > 0.0 ? QStringLiteral("+") : QString())
                        .arg(g, 0, 'f', 1);
                }
            }

            nEqCurve = moved.isEmpty()
                ? QStringLiteral(
                    "Curve: nothing to correct. Measured against your "
                    "own smoothed response, no band is more than a "
                    "decibel off its own trend.")
                : QStringLiteral(
                    "Curve: %1 dB. Each one is where your voice sits "
                    "above or below its OWN smoothed response — a "
                    "resonance or a hole, not a deviation from "
                    "somebody else's target curve. Corrected halfway: "
                    "fully flattened, a voice sounds like a pipe.%2")
                    .arg(moved.join(QStringLiteral(", ")))
                    .arg(mayLift
                         ? QString()
                         : QStringLiteral(" Only cuts here — your room "
                                          "is too close to lift a band "
                                          "without lifting it too."));
        }

        // ── Klang, gedeckelt ────────────────────────────────────────
        {
            const double measuredLow  = 0.5 * (a.bandDb[2] + a.bandDb[3]);
            const double measuredTop  = a.bandDb[7];
            const double snr = a.speechDbFs - a.noiseFloorDbFs;
            const bool mayLift = snr > kBoostNeedsSnrDb;
            const double lift = mayLift ? 4.0 : 0.0;

            // Rund +9 dB bei 125-250 Hz gegen 1 kHz ist fuer Sprache
            // gewoehnlich; korrigiert wird nur der Abstand DAVON, und
            // hoechstens um drei Dezibel.
            constexpr double kOrdinaryLowTiltDb = 9.0;
            constexpr double kMaxCorrectionDb   = 3.0;
            const double lowCorrection = std::clamp(
                -(measuredLow - kOrdinaryLowTiltDb) * 0.5,
                -kMaxCorrectionDb, kMaxCorrectionDb);

            p.lowShelfDb  = std::clamp(rule.lowShiftDb + lowCorrection,
                                       -6.0, lift);
            p.presenceDb  = std::clamp(rule.presenceShiftDb, -6.0, lift);
            p.highShelfDb = std::clamp(
                rule.topShiftDb + (measuredTop > 0.0 ? -1.0 : 0.0),
                -8.0, std::min(lift, 2.0));

            // Das Tiefenfilter sitzt ueber dem Kanalschnitt, nicht
            // auf ihm: der schneidet schon, und zwei Stufen auf
            // derselben Oktave waren der Fehler von vorhin.
            p.lowShelfHz  = std::clamp(double(p.lowCutHz) * 2.5,
                                       150.0, 500.0);
            p.highShelfHz = rule.widthHz * 0.75;

            nEq = QStringLiteral(
                "Tone: low shelf %1 dB from %2 Hz, presence %3 dB, "
                "high shelf %4 dB from %5 Hz. These MOVE your voice "
                "from where it already is — your own balance is the "
                "starting point, never a curve it gets dragged onto. "
                "No high-pass here and no hum notches: the channel "
                "cut does the first, and the second takes out the "
                "fundamental of a low voice.")
                .arg(p.lowShelfDb,  0, 'f', 1)
                .arg(p.lowShelfHz,  0, 'f', 0)
                .arg(p.presenceDb,  0, 'f', 1)
                .arg(p.highShelfDb, 0, 'f', 1)
                .arg(p.highShelfHz, 0, 'f', 0);
        }

        // ── What this width forgets ─────────────────────────────────
        p.characterStages = rule.character;
        if (!p.characterStages) {
            p.forgotten = QStringLiteral(
                "Valve, exciter and reverb are switched OFF, not left "
                "as they were. At %1 there is no room for colour: it "
                "costs compressor headroom and the far end hears none "
                "of it. They come back from the 2.7k profile upward.")
                .arg(widthLabel(rule.widthHz));

        }

        // ── What the measurement says this width cannot deliver ─────
        if (a.rolloffHz > 0.0 && a.rolloffHz < rule.widthHz * 0.95) {
            p.caveat = QStringLiteral(
                "Your microphone is already 20 dB down by %1 Hz, below "
                "this channel's %2 Hz. The top of it will carry hiss "
                "rather than voice — a narrower profile loses you "
                "nothing here and costs the other operator less band.")
                .arg(a.rolloffHz, 0, 'f', 0)
                .arg(rule.widthHz, 0, 'f', 0);
        }

        if (a.loudnessRangeValid && a.loudnessRangeLu > 10.0) {
            nRange = QStringLiteral(
                "Your loudness range is %1 LU — that is you moving "
                "between sentences, not your peaks, and no compressor "
                "setting here answers it.")
                .arg(a.loudnessRangeLu, 0, 'f', 1);
        }

        // Gate, EQ, De-Esser, Kompressor (mit dem Trimm, der vor
        // dessen Detektor sitzt), dann Phasendreher und Begrenzer,
        // dann was abgeschaltet bleibt.
        for (const QString& line : {nGate, nEqCurve, nEq, nDeEss, nTrim, nComp,
                                    nLimiter, nColour, nRange}) {
            if (!line.isEmpty()) { p.notes << line; }
        }

        // Dieselben Zeilen, aber jede an ihrer Stufe -- damit der
        // Reiter, in dem der Bediener weiterdreht, auch sagt, was
        // dort gerade gesetzt wurde.
        const auto at = [&](StripChain::Stage st) -> QString& {
            return p.stageNotes[static_cast<int>(st)];
        };
        at(StripChain::Stage::Gate)  = nGate;
        at(StripChain::Stage::Eq)    = nEqCurve.isEmpty()
            ? nEq
            : (nEq.isEmpty() ? nEqCurve
                             : nEqCurve + QStringLiteral("\n\n") + nEq);
        at(StripChain::Stage::DeEss) = nDeEss;
        at(StripChain::Stage::Comp)  = nTrim.isEmpty()
            ? nComp
            : nTrim + QStringLiteral("\n\n") + nComp;
        at(StripChain::Stage::Limiter) = nLimiter;
        if (!p.tubeCharacter.isEmpty()) {
            at(StripChain::Stage::Tube) = QStringLiteral(
                "Character \"%1\", chosen by the %2 profile.")
                .arg(p.tubeCharacter, p.name);
        }
        if (!p.puduCharacter.isEmpty()) {
            at(StripChain::Stage::Pudu) = QStringLiteral(
                "Character \"%1\", chosen by the %2 profile.")
                .arg(p.puduCharacter, p.name);
        }
        at(StripChain::Stage::Reverb) = QStringLiteral(
            "Left off. No measurement asks for reverb, and on a "
            "transmit path it is the one stage in this chain that is "
            "understood WORSE at the far end rather than better.");

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

    // ── Der Entzerrer wird wieder gesetzt ────────────────────────────
    //
    // Zwischenzeitlich lag er ganz draussen -- nach Zeus' Vorbild,
    // dessen Stile keinen Klangregler anfassen. Der Betreiber hat das
    // am 2026-10-09 zweimal korrigiert: erst mit einem Bild der
    // Reiterreihe ("du siehst hier alles, was die optimierung
    // durchlaufen sollte"), dann ausdruecklich: "wichtig ist vor
    // allem die eq kurve zu analysieren ... und dann dort anzuheben
    // bzw. zu senken, wo die stimme zu stark oder zu schwach ist".
    //
    // Was davon bleibt: KEIN Hochpass (Kanalschnitt), KEINE
    // Brummkerben (nehmen den Grundton), und gemessen wird gegen den
    // eigenen geglaetteten Verlauf, nie gegen eine fremde Zielkurve.
    //
    // ── Alt: der Entzerrer wurde nicht angefasst ─────────────────────
    //
    // Not seeded, not enabled, not disabled, not read. Whatever the
    // operator has on their curve stays exactly as they left it.
    //
    // This is the single biggest difference from the version before,
    // and it is the benchmark's shape: Zeus Link's three voice styles
    // state an expander, a trim and one characteristic move, and not
    // one of them is a tone control. Every fault found on 2026-10-09
    // — a hum notch on the fundamental, a high-pass above it, two
    // cuts stacked over one octave, a de-esser with nothing to
    // de-ess — was this code moving a tone control it had no business
    // moving.

    // ── Die Charaktere zuerst, die Messung darueber ──────────────────
    //
    // Reihenfolge der Kette, wie der Kanalzug sie zeigt: Gate, EQ,
    // De-Esser, Kompressor, Roehre, PUDU, Nachhall, Begrenzer
    // (Betreiber, 2026-10-09: "nach der reihen").
    //
    // Der Charakter kommt vor der Messung, nie danach: er ist die
    // Stimmung, und was gemessen werden kann -- Schwelle, Boden,
    // Verhaeltnis, Trimm -- gehoert dieser Stimme und ueberschreibt
    // ihn anschliessend.
    if (!p.compCharacter.isEmpty()) {
        StripCharacters::apply(chain, StripChain::Stage::Comp,
                               p.compCharacter);
    }
    if (!p.limiterCharacter.isEmpty()) {
        StripCharacters::apply(chain, StripChain::Stage::Limiter,
                               p.limiterCharacter);
    }

    // ── Der Entzerrer: die Korrekturkurve ───────────────────────────
    //
    // Sieben Glocken auf den freien Griffen, die EqBandLayout fuehrt
    // -- dieselben, die der Kanalzug zeichnet und die der Bediener
    // danach mit der Maus nachziehen kann.
    //
    // Band 0 (Hochpass) und 1 bis 3 (Brummkerben) werden NICHT
    // angefasst: den Tiefschnitt macht der Kanal, und die Kerben
    // nehmen bei einer tiefen Stimme den Grundton mit. Beides waren
    // Fehler dieses Tages.
    EqBandLayout::ensureSeeded(chain.eq());
    {
        bool anyEq = false;
        for (int i = 0; i < Plan::kCorrectionBands; ++i) {
            const int idx = EqBandLayout::kFirstSpare + i;
            ClientEq::BandParams b = chain.eq().band(idx);
            b.type    = ClientEq::FilterType::Peak;
            b.freqHz  = EqBandLayout::spareFrequencyHz(i);
            // Q 1,4: schmal genug, eine Resonanz zu treffen, breit
            // genug, dass sieben davon zusammen eine Kurve ergeben
            // und keine Zinken.
            b.q       = 1.4f;
            b.gainDb  = float(p.correctionDb[i]);
            b.enabled = p.correctionDb[i] != 0.0;
            chain.eq().setBand(idx, b);
            anyEq = anyEq || b.enabled;
        }

        ClientEq::BandParams lo = chain.eq().band(kBandLow);
        lo.type    = ClientEq::FilterType::LowShelf;
        lo.freqHz  = float(p.lowShelfHz);
        lo.gainDb  = float(p.lowShelfDb);
        lo.enabled = p.lowShelfDb != 0.0;
        chain.eq().setBand(kBandLow, lo);

        ClientEq::BandParams pr = chain.eq().band(kBandPresence);
        pr.gainDb  = float(p.presenceDb);
        pr.enabled = p.presenceDb != 0.0;
        chain.eq().setBand(kBandPresence, pr);

        ClientEq::BandParams hi = chain.eq().band(kBandHigh);
        hi.type    = ClientEq::FilterType::HighShelf;
        hi.freqHz  = float(p.highShelfHz);
        hi.gainDb  = float(p.highShelfDb);
        hi.enabled = p.highShelfDb != 0.0;
        chain.eq().setBand(kBandHigh, hi);

        // Die Stufe wird eingeschaltet, auch wenn die Korrektur
        // nirgends ein Dezibel erreicht hat. Eine abgeschaltete
        // Stufe sieht aus wie ein Fehler, und die Aussage "an deiner
        // Kurve war nichts zu korrigieren" ist eine Aussage -- sie
        // gehoert sichtbar gemacht, nicht durch Abwesenheit.
        (void)anyEq;
        chain.setStageEnabled(StripChain::Stage::Eq, true);
    }

    // ── Expander ─────────────────────────────────────────────────────
    //
    // The chain's gate stage, run as an expander, which is what Zeus
    // puts in every style including the gentlest.
    if (p.expanderOn) {
        chain.gate().setThresholdDb(float(p.expanderThresholdDbFs));
        chain.gate().setMode(ClientGate::Mode::Expander);
        // Der Boden kommt aus der Messung, siehe Plan::expanderFloorDb.
        chain.gate().setFloorDb(float(p.expanderFloorDb));
        chain.gate().setHoldMs(40.0f);
        chain.gate().setReleaseMs(250.0f);
    }
    chain.setStageEnabled(StripChain::Stage::Gate, p.expanderOn);

    // ── De-esser, where the sibilance was measured ───────────────────
    if (p.deEssOn) {
        chain.deEss().setFrequencyHz(float(p.deEssHz));
        chain.deEss().setThresholdDb(float(std::clamp(
            p.compThresholdDbFs + 6.0, -60.0, 0.0)));
        chain.deEss().setAmountDb(float(p.deEssAmountDb));
    }
    chain.setStageEnabled(StripChain::Stage::DeEss, p.deEssOn);

    // ── Trim and compressor ──────────────────────────────────────────
    //
    // The trim goes in as the compressor's DRIVE, which is a gain
    // ahead of the detector — the right place for it, because a
    // threshold only means something once the level it is measuring
    // has been set. A take that arrives already louder than the
    // target cannot be driven down there (drive starts at 0 dB), so a
    // negative trim comes off the make-up instead.
    chain.comp().setDriveDb(float(std::clamp(p.trimDb, 0.0, 18.0)));
    chain.comp().setRatio(float(p.compRatio));
    chain.comp().setThresholdDb(float(p.compThresholdDbFs));
    chain.comp().setAttackMs(8.0f);
    chain.comp().setReleaseMs(150.0f);
    chain.comp().setKneeDb(8.0f);
    chain.comp().setPhaseRotatorStages(p.phaseRotatorStages);
    chain.comp().setMakeupDb(float(std::clamp(
        p.compMakeupDb + std::min(0.0, p.trimDb), 0.0, 12.0)));
    chain.setStageEnabled(StripChain::Stage::Comp, true);

    chain.limiter().setCeilingDb(float(p.limiterCeilingDb));
    chain.setStageEnabled(StripChain::Stage::Limiter, true);

    // ── The character stages ─────────────────────────────────────────
    //
    // Switched OFF when the profile forgets them, otherwise left
    // exactly as the operator had them. Never switched ON: there is
    // no measurement that says a voice wants a valve.
    if (p.tubeCharacter.isEmpty()) {
        chain.setStageEnabled(StripChain::Stage::Tube, false);
    } else {
        StripCharacters::apply(chain, StripChain::Stage::Tube,
                               p.tubeCharacter);
        chain.setStageEnabled(StripChain::Stage::Tube, true);
    }
    if (p.puduCharacter.isEmpty()) {
        chain.setStageEnabled(StripChain::Stage::Pudu, false);
    } else {
        StripCharacters::apply(chain, StripChain::Stage::Pudu,
                               p.puduCharacter);
        chain.setStageEnabled(StripChain::Stage::Pudu, true);
    }
    // Nachhall bleibt aus. Es gibt keine Messung, die danach fragt,
    // und auf einem Sendeweg ist er das Einzige in dieser Kette, das
    // am anderen Ende schlechter verstanden wird statt besser.
    chain.setStageEnabled(StripChain::Stage::Reverb, false);

    // Deliberately absent: the master switch. The operator decides
    // when their transmit audio changes.
    return true;
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
