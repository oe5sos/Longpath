// =================================================================
// src/core/strip/StripCharacters.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See StripCharacters.h for why these are
// parameter sets and not algorithms, and why every one of them has to
// say what it costs.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/StripCharacters.h"

namespace NereusSDR::StripCharacters {

namespace {

// ── Gate ─────────────────────────────────────────────────────────────
//
// The two upstream modes are kept under their own names so an operator
// who knows AetherSDR finds what they expect. The rest differ in the
// three things that actually distinguish one gate from another: how far
// it closes, how long it waits before closing, and how wide the sticky
// zone is.

const Character kGate[] = {
    {QStringLiteral("Off"),
     QStringLiteral("Ratio 1:1 — the stage runs but does nothing. Use this "
                    "to hear what the gate is costing you before deciding "
                    "you need one.")},
    {QStringLiteral("Expander — soft"),
     QStringLiteral("2:1, floor 15 dB. Ducks the background rather than "
                    "cutting it, so nothing ever audibly slams shut. The "
                    "right choice in a quiet room; it will not rescue you "
                    "from a noisy one.")},
    {QStringLiteral("Ragchew"),
     QStringLiteral("Soft, with a long hold and a slow release. Breathes "
                    "with you and never chatters, at the cost of letting "
                    "some room through between sentences — which on a long "
                    "contact sounds natural rather than wrong.")},
    {QStringLiteral("SSB"),
     QStringLiteral("3:1, floor 18 dB, a long hold and a slow release. The "
                    "one to start from if you do not know which you want: "
                    "deep enough that the room goes away between overs, "
                    "slow enough that it is never heard doing it.")},
    {QStringLiteral("Shack fan"),
     QStringLiteral("Medium ratio, low threshold, long lookahead. Aimed at "
                    "steady broadband noise — a computer fan, a power "
                    "supply — which sits below speech and never stops. The "
                    "lookahead is what stops it clipping the front of "
                    "words while it decides.")},
    {QStringLiteral("Clear"),
     QStringLiteral("6:1, floor 32 dB, with 3 ms of lookahead and a wide "
                    "hysteresis. Nothing but your voice goes out, and the "
                    "wide hysteresis is what keeps it from chattering on "
                    "the quiet words at the end of a sentence. The cost is "
                    "that a soft last syllable can be swallowed.")},
    {QStringLiteral("Contest"),
     QStringLiteral("Hard, with a short hold and a fast release, for rapid "
                    "exchanges where the gate must be shut again before "
                    "the other station comes back. Tiring over an hour and "
                    "exactly right over a weekend.")},
    {QStringLiteral("Gate — hard"),
     QStringLiteral("10:1, floor 40 dB. Full silence between words. "
                    "Effective and unforgiving: any noise loud enough to "
                    "open it arrives at full level, so it is the setting "
                    "most likely to be heard working.")},
};

// ── Compressor ───────────────────────────────────────────────────────

const Character kComp[] = {
    {QStringLiteral("Gentle"),
     QStringLiteral("2:1 over a wide knee, a few decibels at most. Evens "
                    "out the difference between your loud and quiet words "
                    "without the result sounding processed.")},
    {QStringLiteral("Balanced"),
     QStringLiteral("3:1, moderate knee. The everyday setting: audibly "
                    "steadier than nothing, still recognisably a voice.")},
    {QStringLiteral("Contest"),
     QStringLiteral("6:1, hard knee, fast attack. Buys talk power by "
                    "flattening everything, which is what gets you picked "
                    "out of a pile-up and what makes you tiring to listen "
                    "to for an hour.")},
    {QStringLiteral("Voodoo"),
     QStringLiteral("10:1 with drive. Very loud and very obviously "
                    "compressed. Worth trying so you know what the far end "
                    "of the range sounds like; worth leaving unless the "
                    "band is genuinely awful.")},
};

// ── De-esser ─────────────────────────────────────────────────────────
//
// The frequency is the setting people get wrong, and it is the one that
// depends on the speaker rather than on taste — so two of these differ
// only in where they listen.

const Character kDeEss[] = {
    {QStringLiteral("Wide"),
     QStringLiteral("A broad band around 6 kHz. Catches sibilance wherever "
                    "it happens to sit, at the cost of taking some ordinary "
                    "brightness with it.")},
    {QStringLiteral("Narrow"),
     QStringLiteral("A tight band, same centre. Removes only the hiss and "
                    "leaves the rest alone — but if your sibilance is not "
                    "where it is pointed, it does nothing at all.")},
    {QStringLiteral("Lower voice"),
     QStringLiteral("Centred at 5 kHz. Sibilance sits lower for deeper "
                    "voices; aiming a de-esser above it is the commonest "
                    "way to end up with one that does nothing.")},
    {QStringLiteral("Brighter voice"),
     QStringLiteral("Centred at 7.5 kHz, for voices whose ess is higher "
                    "and sharper.")},
};

// ── Tube ─────────────────────────────────────────────────────────────
//
// The tube already had a picker, labelled "Character", holding
// ClientTube::Model A, B and C. That was wrong twice over: it is the one
// place in the window where "character" meant a different ALGORITHM
// rather than a set of numbers, and on its own the model is close to
// unusable — A, B and C sound nearly identical until drive and mix are
// moved with them, and moving four controls in step is exactly the job a
// character is for.
//
// So the model picker keeps the model's own name, and these sit above
// it. Each sets model, drive, tone, bias and mix together; the model
// combo then shows which waveshaper the character chose, and the
// operator can still change it.
//
// Ordered by how much saturated signal actually reaches the output —
// drive × mix — because that, and not the drive number, is what is
// heard. A test asserts the ordering.

const Character kTube[] = {
    {QStringLiteral("Clean"),
     QStringLiteral("Mix at zero: the stage runs and nothing of it reaches "
                    "the output. The honest comparison point — switch "
                    "between this and any other character to hear what the "
                    "tube is really adding, at matched loudness.")},
    {QStringLiteral("Warm"),
     QStringLiteral("Soft model, 5 dB of drive, a third of it mixed in and "
                    "the tone tilted down. Thickens a thin voice without "
                    "being audible as an effect. If someone asks whether "
                    "you changed anything, this is the setting that gets "
                    "no comment.")},
    {QStringLiteral("Full"),
     QStringLiteral("Asymmetric model, 9 dB, half mixed in. The asymmetry "
                    "generates even harmonics, which the ear reads as body "
                    "rather than as distortion. Noticeable on a good "
                    "receiver and flattering on a poor one.")},
    {QStringLiteral("Broadcast"),
     QStringLiteral("Hard model, 13 dB, most of it wet, tone up. Dense and "
                    "forward, the sound people mean by “broadcast "
                    "audio”. It is also where the harmonics stop being "
                    "subtle: they land inside your transmitted bandwidth "
                    "and the far end hears them.")},
    {QStringLiteral("Voodoo"),
     QStringLiteral("Hard model, 20 dB, fully wet. Worth hearing once so "
                    "that the rest of the range has a top end to be "
                    "measured against. Not worth transmitting — this is "
                    "the setting that gets you told you sound rough.")},
};

// ── Exciter ──────────────────────────────────────────────────────────

const Character kPudu[] = {
    {QStringLiteral("Warmth"),
     QStringLiteral("The low generator only. Adds body below the "
                    "fundamental, which survives a narrow receive filter "
                    "better than actual bass does.")},
    {QStringLiteral("Presence"),
     QStringLiteral("The high generator only. Adds harmonics where words "
                    "are separated from each other, which is the part a "
                    "weak signal loses first.")},
    {QStringLiteral("Both"),
     QStringLiteral("Both, modestly. More effect than either alone and the "
                    "easiest of the three to overdo — the exciter is "
                    "generating content that was not there, and past a "
                    "point it stops sounding like you.")},
    {QStringLiteral("Deep voice"),
     QStringLiteral("The low generator tuned down to 70 Hz. A deep voice "
                    "has its fundamental below what a communications "
                    "receiver passes; this puts a harmonic of it where the "
                    "receiver can hear it, which is the only way that "
                    "weight survives the trip.")},
    {QStringLiteral("Cutting"),
     QStringLiteral("The high generator hard, at 3 kHz. For a weak signal "
                    "into a narrow filter, where intelligibility is the "
                    "only thing that matters and pleasantness is not on "
                    "the list. Switch it off again afterwards.")},
};

// ── Final limiter ────────────────────────────────────────────────────
//
// The limiter has one parameter worth presetting — the ceiling — so
// these are four numbers with four reasons rather than four sets of
// settings. That is thinner than the other stages and is left honest
// rather than padded out: adding a knob to the character so the picker
// looks busier would be inventing a difference that is not there.

const Character kLimiter[] = {
    {QStringLiteral("Transparent"),
     QStringLiteral("Ceiling 6 dB below full scale. The limiter is there "
                    "and almost never engages. Costs average power and "
                    "buys the certainty that nothing downstream is being "
                    "asked to handle a peak it cannot.")},
    {QStringLiteral("Safety"),
     QStringLiteral("Ceiling 3 dB below full scale. Catches the occasional "
                    "peak and otherwise does nothing, which is what a "
                    "brickwall is for. The everyday setting.")},
    {QStringLiteral("Broadcast"),
     QStringLiteral("Ceiling 1.5 dB down. Working on most syllables now. "
                    "Louder on air, and the point past which the gain "
                    "reduction meter is worth watching — if it is pinned, "
                    "the compressor above is set too hot.")},
    {QStringLiteral("Loud"),
     QStringLiteral("Ceiling just under full scale. Every last decibel, "
                    "and the limiter working most of the time — at which "
                    "point it is doing the compressor's job and doing it "
                    "worse.")},
};

template <size_t N>
QVector<Character> toVector(const Character (&arr)[N])
{
    QVector<Character> out;
    out.reserve(int(N));
    for (const Character& c : arr) { out.append(c); }
    return out;
}

} // namespace

QVector<Character> forStage(StripChain::Stage stage)
{
    switch (stage) {
    case StripChain::Stage::Gate:    return toVector(kGate);
    case StripChain::Stage::Comp:    return toVector(kComp);
    case StripChain::Stage::DeEss:   return toVector(kDeEss);
    case StripChain::Stage::Tube:    return toVector(kTube);
    case StripChain::Stage::Pudu:    return toVector(kPudu);
    case StripChain::Stage::Limiter: return toVector(kLimiter);
    default: break;
    }
    // The equaliser has its own targets and the reverb has nothing worth
    // presetting. An empty list means the window shows no picker rather
    // than an empty one.
    return {};
}

bool apply(StripChain& chain, StripChain::Stage stage, const QString& name)
{
    switch (stage) {
    case StripChain::Stage::Gate: {
        ClientGate& g = chain.gate();
        if (name == QLatin1String("Off")) {
            g.setRatio(1.0f); g.setFloorDb(0.0f);
            return true;
        }
        if (name == QLatin1String("Expander — soft")) {
            g.setMode(ClientGate::Mode::Expander);
            g.setAttackMs(3.0f); g.setHoldMs(120.0f); g.setReleaseMs(220.0f);
            g.setReturnDb(4.0f); g.setLookaheadMs(1.0f);
            return true;
        }
        if (name == QLatin1String("Ragchew")) {
            g.setMode(ClientGate::Mode::Expander);
            g.setAttackMs(5.0f); g.setHoldMs(300.0f); g.setReleaseMs(450.0f);
            g.setReturnDb(6.0f); g.setLookaheadMs(1.0f);
            return true;
        }
        if (name == QLatin1String("Shack fan")) {
            // Between the two upstream modes, and the lookahead is the
            // point: a steady noise needs a deep enough cut to matter,
            // and a deep cut applied late eats the front of every word.
            g.setRatio(4.0f); g.setFloorDb(-24.0f);
            g.setThresholdDb(-42.0f);
            g.setAttackMs(1.0f); g.setHoldMs(160.0f); g.setReleaseMs(260.0f);
            g.setReturnDb(5.0f); g.setLookaheadMs(4.0f);
            return true;
        }
        if (name == QLatin1String("SSB")) {
            g.setRatio(3.0f); g.setFloorDb(-18.0f);
            g.setThresholdDb(-38.0f);
            g.setAttackMs(2.0f); g.setHoldMs(220.0f); g.setReleaseMs(320.0f);
            g.setReturnDb(5.0f); g.setLookaheadMs(2.0f);
            return true;
        }
        if (name == QLatin1String("Clear")) {
            // The wide return is the whole character: a deep cut with a
            // narrow hysteresis chatters on the trailing syllable of
            // every sentence, which is worse than the noise it removes.
            g.setRatio(6.0f); g.setFloorDb(-32.0f);
            g.setThresholdDb(-45.0f);
            g.setAttackMs(1.0f); g.setHoldMs(140.0f); g.setReleaseMs(200.0f);
            g.setReturnDb(8.0f); g.setLookaheadMs(3.0f);
            return true;
        }
        if (name == QLatin1String("Contest")) {
            g.setMode(ClientGate::Mode::Gate);
            g.setAttackMs(0.5f); g.setHoldMs(60.0f); g.setReleaseMs(90.0f);
            g.setReturnDb(3.0f); g.setLookaheadMs(2.0f);
            return true;
        }
        if (name == QLatin1String("Gate — hard")) {
            g.setMode(ClientGate::Mode::Gate);
            g.setAttackMs(1.0f); g.setHoldMs(100.0f); g.setReleaseMs(150.0f);
            g.setReturnDb(4.0f); g.setLookaheadMs(2.0f);
            return true;
        }
        return false;
    }

    case StripChain::Stage::Comp: {
        ClientComp& c = chain.comp();
        if (name == QLatin1String("Gentle")) {
            c.setRatio(2.0f);  c.setKneeDb(10.0f); c.setThresholdDb(-14.0f);
            c.setAttackMs(15.0f); c.setReleaseMs(180.0f); c.setDriveDb(0.0f);
            return true;
        }
        if (name == QLatin1String("Balanced")) {
            c.setRatio(3.0f);  c.setKneeDb(6.0f);  c.setThresholdDb(-18.0f);
            c.setAttackMs(8.0f); c.setReleaseMs(140.0f); c.setDriveDb(0.0f);
            return true;
        }
        if (name == QLatin1String("Contest")) {
            c.setRatio(6.0f);  c.setKneeDb(2.0f);  c.setThresholdDb(-24.0f);
            c.setAttackMs(2.0f); c.setReleaseMs(90.0f); c.setDriveDb(2.0f);
            return true;
        }
        if (name == QLatin1String("Voodoo")) {
            c.setRatio(10.0f); c.setKneeDb(1.0f);  c.setThresholdDb(-28.0f);
            c.setAttackMs(1.0f); c.setReleaseMs(70.0f); c.setDriveDb(5.0f);
            return true;
        }
        return false;
    }

    case StripChain::Stage::DeEss: {
        ClientDeEss& d = chain.deEss();
        if (name == QLatin1String("Wide")) {
            d.setFrequencyHz(6000.0f); d.setQ(1.0f);
            d.setThresholdDb(-26.0f);  d.setAmountDb(6.0f);
            return true;
        }
        if (name == QLatin1String("Narrow")) {
            d.setFrequencyHz(6000.0f); d.setQ(3.5f);
            d.setThresholdDb(-26.0f);  d.setAmountDb(8.0f);
            return true;
        }
        if (name == QLatin1String("Lower voice")) {
            d.setFrequencyHz(5000.0f); d.setQ(2.0f);
            d.setThresholdDb(-26.0f);  d.setAmountDb(7.0f);
            return true;
        }
        if (name == QLatin1String("Brighter voice")) {
            d.setFrequencyHz(7500.0f); d.setQ(2.0f);
            d.setThresholdDb(-26.0f);  d.setAmountDb(7.0f);
            return true;
        }
        return false;
    }

    case StripChain::Stage::Tube: {
        ClientTube& t = chain.tube();
        // Bias only does anything on the asymmetric model; the others
        // are symmetric waveshapers and it is set to zero for them so a
        // character never leaves a stale value behind for the next one.
        if (name == QLatin1String("Clean")) {
            t.setModel(ClientTube::Model::A);
            t.setDriveDb(0.0f); t.setTone(0.0f);
            t.setBiasAmount(0.0f); t.setDryWet(0.0f);
            t.setOutputGainDb(0.0f);
            return true;
        }
        if (name == QLatin1String("Warm")) {
            t.setModel(ClientTube::Model::A);
            t.setDriveDb(5.0f); t.setTone(-0.15f);
            t.setBiasAmount(0.0f); t.setDryWet(0.35f);
            t.setOutputGainDb(-1.0f);
            return true;
        }
        if (name == QLatin1String("Full")) {
            t.setModel(ClientTube::Model::C);
            t.setDriveDb(9.0f); t.setTone(0.0f);
            t.setBiasAmount(0.35f); t.setDryWet(0.50f);
            t.setOutputGainDb(-2.0f);
            return true;
        }
        if (name == QLatin1String("Broadcast")) {
            t.setModel(ClientTube::Model::B);
            t.setDriveDb(13.0f); t.setTone(0.10f);
            t.setBiasAmount(0.0f); t.setDryWet(0.70f);
            t.setOutputGainDb(-4.0f);
            return true;
        }
        if (name == QLatin1String("Voodoo")) {
            t.setModel(ClientTube::Model::B);
            t.setDriveDb(20.0f); t.setTone(0.20f);
            t.setBiasAmount(0.0f); t.setDryWet(1.0f);
            t.setOutputGainDb(-7.0f);
            return true;
        }
        return false;
    }

    case StripChain::Stage::Pudu: {
        ClientPudu& p = chain.pudu();
        if (name == QLatin1String("Warmth")) {
            p.setPooTuneHz(90.0f);
            p.setPooMix(0.35f); p.setDooMix(0.0f);
            return true;
        }
        if (name == QLatin1String("Presence")) {
            p.setDooTuneHz(5000.0f);
            p.setPooMix(0.0f);  p.setDooMix(0.30f);
            return true;
        }
        if (name == QLatin1String("Both")) {
            p.setPooTuneHz(90.0f); p.setDooTuneHz(5000.0f);
            p.setPooMix(0.22f); p.setDooMix(0.20f);
            return true;
        }
        if (name == QLatin1String("Deep voice")) {
            p.setPooTuneHz(70.0f); p.setPooDriveDb(6.0f);
            p.setPooMix(0.40f); p.setDooMix(0.0f);
            return true;
        }
        if (name == QLatin1String("Cutting")) {
            p.setDooTuneHz(3000.0f); p.setDooHarmonicsDb(12.0f);
            p.setPooMix(0.0f);  p.setDooMix(0.45f);
            return true;
        }
        return false;
    }

    case StripChain::Stage::Limiter: {
        ClientFinalLimiter& l = chain.limiter();
        if (name == QLatin1String("Transparent")) { l.setCeilingDb(-6.0f); return true; }
        if (name == QLatin1String("Safety"))      { l.setCeilingDb(-3.0f); return true; }
        if (name == QLatin1String("Broadcast"))   { l.setCeilingDb(-1.5f); return true; }
        if (name == QLatin1String("Loud"))        { l.setCeilingDb(-0.5f); return true; }
        return false;
    }

    default:
        return false;
    }
}

} // namespace NereusSDR::StripCharacters
