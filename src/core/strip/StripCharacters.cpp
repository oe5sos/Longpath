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
    {QStringLiteral("Shack fan"),
     QStringLiteral("Medium ratio, low threshold, long lookahead. Aimed at "
                    "steady broadband noise — a computer fan, a power "
                    "supply — which sits below speech and never stops. The "
                    "lookahead is what stops it clipping the front of "
                    "words while it decides.")},
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
};

// ── Final limiter ────────────────────────────────────────────────────

const Character kLimiter[] = {
    {QStringLiteral("Safety"),
     QStringLiteral("Ceiling 3 dB below full scale. Catches the occasional "
                    "peak and otherwise does nothing, which is what a "
                    "brickwall is for.")},
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
    case StripChain::Stage::Pudu:    return toVector(kPudu);
    case StripChain::Stage::Limiter: return toVector(kLimiter);
    default: break;
    }
    // The equaliser has its own targets, the tube has real models, and
    // the reverb has nothing worth presetting. An empty list means the
    // window shows no picker rather than an empty one.
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

    case StripChain::Stage::Pudu: {
        ClientPudu& p = chain.pudu();
        if (name == QLatin1String("Warmth")) {
            p.setPooMix(0.35f); p.setDooMix(0.0f);
            return true;
        }
        if (name == QLatin1String("Presence")) {
            p.setPooMix(0.0f);  p.setDooMix(0.30f);
            return true;
        }
        if (name == QLatin1String("Both")) {
            p.setPooMix(0.22f); p.setDooMix(0.20f);
            return true;
        }
        return false;
    }

    case StripChain::Stage::Limiter: {
        ClientFinalLimiter& l = chain.limiter();
        if (name == QLatin1String("Safety")) { l.setCeilingDb(-3.0f); return true; }
        if (name == QLatin1String("Loud"))   { l.setCeilingDb(-0.5f); return true; }
        return false;
    }

    default:
        return false;
    }
}

} // namespace NereusSDR::StripCharacters
